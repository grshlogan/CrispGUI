# CrisperWeaver 审计报告（面向 CrispRustGUI v1）

> 撰写：A002。日期：2026-10-11。审计视角：**HTTP 客户端 GUI 的可复刻架构模式**，批处理队列优先。
> 证据标注：`源码确认`（文件:行号）。被审计树：`E:\AI\ASR\CrisperWeaver\Source`（v0.11.0），全程只读。
> 背景：CrisperWeaver = Flutter 桌面应用，通过 Dart FFI 进程内驱动 CrispASR。我们要复刻的是它的**架构模式**（任务模型、队列、参数快照、服务分层），不是它的进程内引擎接入方式。

---

## 1. 总览

`lib/` 180 个 Dart 文件，分层清晰：`engines/`（引擎抽象）→ `services/`（领域服务）→ `providers/`（Riverpod 状态）→ `screens/`+`widgets/`（UI）。对 C++ GUI 有价值的是 **services 层的任务模型与服务边界**；engines/providers 是 Flutter 特有的外壳。

## 2. 模式清单（按修正后的优先级排序）

### 模式 P1：批处理队列（最高价值，直接复刻）

**解决的问题**：多文件排队、按模型分组省去重载、崩溃后恢复、参数快照。

**核心文件**：

| 文件 | 规模 | 内容 |
|---|---|---|
| `lib/services/batch_queue_service.dart` | 517 行 | 队列状态机 |
| `lib/services/batch_persistence_service.dart` | 338 行 | 磁盘持久化 |
| `lib/screens/transcription_screen.dart:2804` | — | `_startBatchRun` 驱动循环 |

**任务模型（BatchJob，`batch_queue_service.dart:36-135`）**——这是"步骤链"数据结构 today 的样子，也是我们要扩展的起点：

```
BatchJob {
  id, filePath, createdAt,
  status: queued | running | done | error | cancelled   // :17-23
  progress, errorMessage, resultText, historyEntryId,
  backend, modelId, language,        // 入队时快照（:29-35）
  durationSec,
  resumeOffsetSec                    // 崩溃恢复起点（Q3）
}
```

- **参数快照粒度**：只有 `backend/modelId/language` 三个字段入队时快照；其余参数（AdvancedOptions）在**按下"开始批处理"那一刻**统一快照（`transcription_screen.dart:2812-2855`：`_startBatchRun` 里从 provider 读出整套 AdvancedTranscribeOptions）→ 整批共用一套参数。**没有"每任务参数"**——这正是我们 v1 要补强的点（见 §4）。
- **按模型分组**（`batch_queue_service.dart:368-394` `reorderByGrouping`）：queued 任务按 `(backend, modelId, language)` 排序，同组连续执行，避免逐任务换模型。可开关（settings.groupBatchByBackend）。
- **崩溃恢复/断点**（两遍修复，`batch_queue_service.dart:216-283`）：
  1. `running` → 降级回 `queued`（被杀任务重新入队）；
  2. 有残留 checkpoint 的任务 → `resumeOffsetSec = 最后一个已完成 segment 的 endTime`，续跑时跳过已转写前缀。
- **持久化布局**（`batch_persistence_service.dart:1-40`）：每任务一个 `job-<id>.json` + 追加式 `job-<id>.ckpt.jsonl`（每 segment 一行，O(1) 追加）+ `manifest.json` 索引。**写盘 fire-and-forget 不阻塞 UI**（`:66-73`），但提供 `whenPersisted()` 供测试同步。
- **重复检测**：音频指纹（`AudioFingerprintService`）对比历史，已转写文件入队前提示（`:141-155, 285-300`）。
- **取消语义**：任务级 cancel → `cancelled`（终态）；错误任务保留 error 信息可重试（`retryJobs`，`:425-431`）。

**C++ 复刻要点**：
- BatchJob 结构照搬，**把 `backend/modelId/language` 快照泛化为 `params` 快照 + `steps[]` 数组**（v1 job = `[asr]` 单步；未来 `[separate,asr]`、`[asr,translate,merge]` 无需改表结构）。
- 持久化用同样的"每任务一 JSON + 追加 checkpoint"布局；C++ 侧没有 fire-and-forget 必要性，但"写盘不阻塞 UI 线程"的原则要保留（用后台线程 + 完成后 requestUpdate）。
- resumeOffsetSec 的妙处在于**服务器无关**：续跑就是带 `offset_t_ms`/裁剪后的文件再发一次请求（HTTP 版甚至更简单——服务端有 `offset_t_ms` 参数，`crispasr_server.cpp:2167-2168`）。
- 分组排序逻辑约 30 行，直接移植。

### 模式 P2：参数快照与"运行中调参"语义

**解决的问题**：批处理进行中用户改参数，什么时机生效。

- CrisperWeaver 的答案：**批级快照**（按下开始时定格），批内不跟 UI 变化（`transcription_screen.dart:2812`）。
- 两种语义的框架支持都有：FFI 会话有 sticky setter（每个任务派发前重设一遍，`transcription_worker.dart:200-330`——**每次派发都全量重设**，防止上一任务的参数泄漏到下一任务），所以"下一任务生效"只需要 UI 把新值写进快照。
- **HTTP 版映射**：CrispASR 服务器本身就是"每请求全量参数"（见 crispasr-audit.md §3.3），CrisperWeaver 的"防泄漏重设"模式直接变成"每个请求都带完整参数集"——更简单，无需任何 sticky 状态。

### 模式 P3：模型服务（发现/下载/校验）

**核心文件**：`lib/services/model_service.dart`（2048 行）+ `lib/services/model_catalog.dart`（7750 行，纯数据）。

要点：
- **模型分类学**（`model_catalog.dart` ModelKind enum，`:13-60` 附近）：`asr/tts/voice/codec/punc/vad/lid/diarize/translate` ——按"引擎用途"而非"文件格式"分类，每个条目带 companions（配套文件，如 qwen3-tts 需要 tokenizer GGUF）。**我们的 profile 体系不需要这么大的目录**（模型由 profile 直接指路径），但 companions 概念值得吸收（日语 profile 的 aligner 就是一种 companion）。
- **下载 → 校验 → 重试一次**（`model_service.dart:849-930`）：校验失败删文件重下一次，两次失败才报错（"校验失败几乎总是代理/CDN 抖动"）；支持断点续传（`_downloadWithResume`）。
- **注册表自动发现**（`:563-640`）：新装 CrispASR 后扫注册表把新模型补进目录。
- **对本项目的取舍**：v1 的模型获取是"用户手动放置 + profile 指路径"（硬性规则 2），下载/校验整套**不需要**；只需要"路径存在性检查 + 缺失时显示 download URL 提示"（现有 PowerShell 启动器已实现该 UX，`ListOnly [OK]/[MISS]`）。**P3 整体标记为"了解即可，不移植"。**

### 模式 P4：Preset 系统（与 profile 的关系）

**核心文件**：`lib/services/preset_service.dart`（397 行）。

- Preset = `{id, name, createdAt, backend, modelId, language, options(AdvancedOptions), schemaVersion}`（`:34-75`），存 SharedPreferences 单 JSON。
- **设计要点**：`schemaVersion` + fromJson 容忍未知字段（向前兼容）/缺失字段（向后兼容）——这是 JSONC profile 演进时同样需要的。
- 名称唯一化（重名自动加序号 `:374-379`）。
- **对本项目**：我们的 profile（文件、可分享、带注释）已比 Preset（数据库行）更强；GUI 的参数微调层应该是"profile 基础 + 会话内覆盖"，**不必复刻 Preset 存储**，但 schemaVersion/兼容字段的做法要用在 profile loader 里。

### 模式 P5：对齐器二次通过

**核心文件**：`lib/services/aligner_service.dart`（338 行）。

- 流程：主转写得到 segment 级结果 → 若无词级时间戳，用 CTC 对齐器对**已有文本**跑第二遍 → 填充 `words[]`。
- **绝不自动下载**（文件头注释 `:18-20`）：本地找不到对齐器就原样返回，保持服务边界干净。
- 按语言选对齐器文件名（wav2vec2 XLSR 表，`:32-40`）。
- **HTTP 版映射**：CrispASR 服务器已内置该流程（启动 `-am` + 请求 `no_auto_aligner=false`，主+对齐一次请求完成），GUI 无需复刻此服务——**标记为"已被服务器吸收，不移植"**；但其"缺失即降级、不阻塞主流程"的错误处理风格值得保留。

### 模式 P6：设置存储

`lib/services/settings_service.dart`（705 行，SharedPreferences 键值）。要点：类型化 getter/setter、每键默认值集中定义。C++ 版对应"一个 JSON 配置文件 + 类型化访问器"，无新知识。

### 模式 P7：worker 隔离（**不要移植，但要理解其动机**）

`lib/services/transcription_worker_pool.dart`（592 行）+ `transcription_worker.dart`（436 行）：Dart isolate 里跑同步 FFI，防止 UI 冻结。这是 **Flutter/Dart 特有的问题**（同步 FFI 阻塞 UI 线程）。HTTP 架构下对应物是：**HTTP 客户端调用放在后台线程**，完成后回 UI 线程更新（EUI-NEO 的 `app::async` 恰好就是这个模型）。动机相同，实现完全不同。

## 3. AdvancedOptions → API 调用映射（源码确认）

`AdvancedTranscribeOptions`（`lib/services/transcription_service.dart:30-206`）共约 40 个字段，是 CrisperWeaver 参数面板的完整集合。映射到 CrispASR HTTP 参数（CrisperWeaver 走 FFI setter，我们走 form 字段——两者语义相同，因为 FFI setter 与服务器参数最终落到同一套引擎参数）：

| AdvancedOptions 字段 | FFI 调用（CrisperWeaver） | HTTP form 字段（GUI v1） |
|---|---|---|
| vadBackend/vadThreshold/vadMinSpeechMs/vadMinSilenceMs/vadSpeechPadMs | transcribeVad(...options) | `vad`,`vad_threshold`,`vad_min_speech_duration_ms`,`vad_min_silence_duration_ms`,`vad_speech_pad_ms` |
| diarizeMethod/enableSpeakerRecognition | DiarizeService | `diarize`,`diarize_method`,`diarize_max_speakers` |
| temperature/bestOf/beamSize | setTemperature/setBestOf/setBeamSize（每次派发重设，`transcription_worker.dart:237-256`） | `temperature`,`best_of`,`beam_size` |
| sensitivityPreset / entropyThold 等4项 | setSensitivity/setFallbackThresholds（二选一，preset 优先，`:262-280`） | `sensitivity` 或 4 个阈值字段 |
| hotwords/hotwordsBoost | setHotwords | `hotwords`,`hotwords_boost` |
| suppressNonSpeechTokens/suppressTokensRegex/carryInitialPrompt | setWhisperDecodeExtras | `suppress_nst`,`suppress_regex`,`carry_initial_prompt` |
| altN | setAltN | `alt_n` |
| grammarText/RootRule/Penalty/Strict | setGrammar/setGrammarStrict | `grammar`,`grammar_rule` |
| puncFamily | PuncService.preferredFamily | `punctuation`（bool；具体 punc 模型是启动参数） |
| chunkSeconds | 引擎分块 | `chunk_seconds` |
| alignerModel | AlignerService 二次通过 | 服务器启动参数 `-am`（请求级只有 no_auto_aligner 开关） |
| asrUseGpu/asrFlashAttn/asrNGpuLayers | openWithParams | **启动参数**（HTTP 版无请求级对应） |
| maxLen/splitOnWord/splitOnPunct | 引擎参数 | `max_len`,`split_on_word`,`split_on_punct` |
| offset/duration window | 引擎切片 | `offset_t_ms`,`duration_ms` |
| nThreads | openWithParams | 启动参数 `-t` |
| tdrz/tokenTimestamps | whisper 专用 FFI 路径 | HTTP 版无直接对应（tdrz 在服务器参数集之外；tokenTimestamps 由 verbose_json 的 words 承载） |

**注意**：`asrUseGpu/aligner/nThreads/tdrz` 四组在 HTTP 版是启动参数或无对应——GUI 的参数面板要区分"请求级可调"与"需重启"两类（crispasr-audit.md §5.1 的两档模型切换）。

## 4. 什么不该复制

1. **进程内引擎接入**（engines/、native/、FFI 绑定、worker isolate）——v1 是 HTTP 客户端，全部不适用。
2. **模型下载/校验/目录**（model_service 2000 行 + model_catalog 7750 行）——模型手动放置是项目硬规则，GUI 只需存在性检查 + 提示 URL。
3. **Flutter 特有层**：Riverpod providers、l10n（14 语言）、平台通道（macOS security-scoped bookmarks、iOS 文件）、web/移动端代码路径。
4. **Preset 存储**：profile 文件体系已覆盖。
5. **TTS/voice/clone/chat/live-translate 等服务**（约一半的 screens/services）：与六步工作流无关，v1 不做。

## 5. 推荐精读源码文件列表

| 文件 | 规模 | 为什么值得读 | 与需求的关联 |
|---|---|---|---|
| `lib/services/batch_queue_service.dart` | 517 行 | 任务模型、状态机、分组、恢复、去重——**本报告最高价值文件** | v1 队列模块的蓝本；BatchJob 结构直接翻译成 C++ |
| `lib/services/batch_persistence_service.dart` | 338 行 | 每任务 JSON + 追加 checkpoint + manifest 的持久化布局 | 断点续跑的磁盘设计 |
| `lib/screens/transcription_screen.dart`（仅 `_startBatchRun`，2804-2960） | ~160 行 | 批处理驱动循环：参数快照时机、模型加载一次、任务派发、进度回写 | GUI 批处理主循环的行为规范 |
| `lib/services/transcription_service.dart`（30-330） | ~300 行 | AdvancedTranscribeOptions 完整字段集 + 服务编排顺序（VAD 确认→参数应用→转写→后处理） | 参数面板字段清单 + 调用顺序 |
| `lib/services/transcription_worker.dart`（200-330） | ~130 行 | "每次派发全量重设 sticky 参数"的防泄漏协议 | HTTP 版的直接对应：每请求全量参数 |
| `lib/services/preset_service.dart` | 397 行 | schemaVersion + 双向兼容反序列化 | profile loader 的演进策略 |
| `lib/services/aligner_service.dart` | 338 行 | 二次通过模式与"绝不自动下载"边界 | 理解词级时间戳的来源；实际流程已被服务器吸收 |
| `lib/engines/crispasr_engine.dart`（520-700 加载段） | ~180 行 | companion 加载顺序、失败快照清理、加载状态机 | 模型加载失败 UX 的参照 |
