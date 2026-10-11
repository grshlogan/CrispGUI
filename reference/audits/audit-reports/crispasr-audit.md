# CrispASR 审计报告（面向 CrispRustGUI v1）

> 撰写：A002。日期：2026-10-11。审计视角：**GUI 作为 CrispASR HTTP 服务器（端口 5114）的客户端**，不是进程内 FFI。
> 证据标注：`源码确认`（给出文件:行号）/ `实测`（本项目已有实测记录，见 docs/STATUS.md）/ `待验证`（尚无本地实测）。
> 被审计树：`E:\AI\ASR\CrispASR\Source`（版本 0.8.42，见 `VERSION`），全程只读。

---

## 1. 架构概述（服务器视角）

CrispASR 是 ggml 系 C++ 引擎，同一份可执行文件 `crispasr.exe` 既是完整 CLI，也通过 `--server` 开关变成常驻 HTTP 服务器（`examples/cli/cli.cpp:2683-2686`：`params.server` 为真时调用 `crispasr_run_server`）。

服务器进程模型（`examples/cli/crispasr_server.cpp:1263` 起）：

- **单实例单模型**：默认（无 `--server-workers`）整个进程一个模型实例 + 一把 `model_mutex`，请求串行执行（`crispasr_server.cpp:1350-1358`；`docs/concurrency.md` "并发传输、串行模型"）。
- `--server-workers N` 才会加载 N 个模型实例，且只让"纯 ASR 请求"（显式 language、无 aligner、无后处理）并发（`crispasr_server.cpp:1433-1470`）。**我们的 profile 场景保持默认 1 即可**——批处理的价值在"模型常驻、无逐文件重载"，不在并发推理。
- 启动 warmup 默认开启，失败只降级不致命（`crispasr_server.cpp:1414-1431`）。
- 启动时检查 CPU ISA 与 host 匹配，不匹配则拒绝对 CPU 路径（VAD/diarize）的请求而非崩溃（`crispasr_server.cpp:1296-1316`；请求时再查 `crispasr_server.cpp:2175-2186`）。
- API Key 鉴权：`Authorization: Bearer <key>` 或 `X-API-Key`（`crispasr_server.cpp:302-330`）。本项目 key 固定 `default`（AGENTS.md 契约）；**启动参数 api_keys 为空时完全无鉴权**（`is_authorized`，`:314-330`）。

对 GUI 的意义：**服务器是"一次启动、长期服务"的进程；GUI 的职责 = 选 profile 启动进程 → /health 探就绪 → 发请求 → 换 profile 时重启（或 /load 热切换，见 §5.3）。**

## 2. 端点总表（源码确认）

| 端点 | 方法 | 行号 | 用途 | GUI v1 重要性 |
|---|---|---|---|---|
| `/v1/audio/transcriptions` | POST | `crispasr_server.cpp:2027` | ASR 主端点（OpenAI 兼容 + 扩展参数） | ★核心 |
| `/health` | GET | `:2388` | 就绪探测（公开，无鉴权） | ★核心 |
| `/progress` | GET | `:2403` | 当前任务进度 `{"busy":bool,"progress":-1..100}` | ★核心 |
| `/backends` | GET | `:2415` | 后端清单 + `active` | ★（健康详情） |
| `/v1/models` | GET | `:2433` | 当前加载的模型（OpenAI 风格） | ★（状态显示） |
| `/load` | POST | `:2319` | 模型热切换（限制见 §5.3） | ○（备选） |
| `/inference` | POST | `:1833` | 旧版端点；响应含 `audio_event` 等原生字段 | ★（步骤④数据通道） |
| `/v1/audio/separation` | POST | `:1684` | 人声/环境音分离（步骤①） | ★（未来步骤） |
| `/v1/translate` | POST | `:2459` | 内置文本翻译（需启动时装 m2m100 等翻译模型） | △（步骤⑤走用户 5113，不依赖） |
| `/v1/audio/speech` | POST | `:2577` | TTS | －（v1 不用） |
| `/v1/audio/speech-to-speech` | POST | `:3277` | 语音→语音 | － |
| `/v1/voices`（+`/voices`/`/v1/audio/voices`） | GET/POST/DELETE | `:3452-3602` | 音色管理 | － |
| `/v1/chat/completions` | POST | `:3744` | 内置文本 LLM 对话 | －（步骤⑤走 5113） |

参数既接受 multipart 字段也接受 form/查询参数（`form_string`/`req_has_part` 兼容两种来源，`crispasr_server.cpp:252-265`）。

## 3. `/v1/audio/transcriptions` 完整参数清单（源码确认）

解析代码：`crispasr_server.cpp:2040-2240`。**所有参数默认值取服务器启动参数（`whisper_params rp = params` 先拷贝再覆盖，`:2075`）——"启动默认值 + 请求级覆盖"模型。**

### 3.1 OpenAI 兼容字段

| 字段 | 类型 | 默认 | 说明 |
|---|---|---|---|
| `file` | multipart 文件 | 必填 | 音频（WAV/MP3/FLAC/OGG/Opus/AAC/M4A/WebM/AMR…，服务端自带解码） |
| `response_format` | enum | `json` | `json`/`verbose_json`/`text`/`srt`/`vtt`/`diarized_json`（`:2057-2065` 校验，非法 400） |
| `language` | string | 启动参数 | ISO-639-1 或全名，`auto` 可；按 backend 校验（`validate_request_language`，`crispasr_server.cpp:390-425`：whisper 用 100 语言表；单语 backend——moonshine=en、gigaam=ru——拒绝其他语言） |
| `prompt` | string | `""` | 初始提示词（非空覆盖 `rp.prompt`，`:2237`） |
| `temperature` | float | 启动参数 | |
| `seed` | u64 | 启动参数 | |
| `max_new_tokens` / `max_tokens` | int | 启动参数 | 等价；`max_tokens` 后读可覆盖（`:2050-2051`） |
| `frequency_penalty` | float | 启动参数 | |

### 3.2 CrispASR 扩展字段（全部可 per-request 覆盖，源码确认）

**语言/翻译**（`:2077-2080`）：`translate` (bool) · `source_lang` · `target_lang`（canary/cohere/voxtral；source≠target 即翻译）

**标点**（`:2081`）：`punctuation` (bool)

**说话人分离（步骤②，`:2083-2102`）**：`diarize` (bool) · `diarize_method`（缺省 energy；单声道自动 vad-turns，`crispasr_diarize_cli.cpp:909-924`）· `diarize_embedder`（声纹模型，`auto` 可）· `diarize_cluster_threshold`（显式传入才覆盖）· `diarize_max_speakers`

**VAD（`:2103-2109`）**：`vad` · `vad_threshold` · `vad_min_speech_duration_ms` · `vad_min_silence_duration_ms` · `vad_max_speech_duration_s` · `vad_speech_pad_ms`

**上下文偏置（`:2110-2112`）**：`hotwords`（逗号分隔）· `hotwords_boost`

**whisper 解码控制（`:2113-2146`）**：`suppress_regex` · `suppress_nst` · `grammar` · `grammar_rule` · `best_of` · `beam_size` · `return_logits` · `sensitivity`（预设 conservative/balanced/aggressive；**先应用**，同请求显式四阈值仍可覆盖；未知预设告警忽略，`:2160-2174`）· `entropy_thold` · `logprob_thold` · `no_speech_thold` · `temperature_inc` · `no_fallback`

**语言检测（`:2148-2150`）**：`detect_language` · `lid_backend` · `lid_model`

**长音频/分块（`:2152-2158`）**：`chunk_seconds`（显式传即 `chunk_seconds_explicit`）· `chunk_overlap` · `lcs_dedup` · `lcs_min_length` · `parakeet_decoder` · `att_context`（`"L,R"` 局部注意力窗，`:2215-2222`）

**片段切分（`:2159-2166`）**：`no_timestamps` · `split_on_word` · `max_len` · `split_on_punct`

**窗口（`:2167-2168`）**：`offset_t_ms` · `duration_ms`

**VAD 边界复用 #227（`:2186-2190`）**：`vad_export`（响应附 `vad_segments`）· `vad_import`（JSON 字符串复用边界跳过 VAD）· `vad_import_strict` · `vad_export_raw`

**严格管线 #311（`:2191-2195`）**：`strict_pipeline` · `require_vad` · `require_word_timestamps` · `require_punctuation`（必需阶段失败 → HTTP 错误而非降级 200）

**其他（`:2196-2237`）**：`max_context` · `audio_ctx` · `word_thold` · `carry_initial_prompt` · `no_auto_aligner` · `show_alternatives` · `alt_n` · `stream`（SSE）

### 3.3 参数生效语义（GUI"实时调参"的关键结论）

- 每个请求独立构建 `rp = params` 副本再逐字段覆盖（`crispasr_server.cpp:2075-2240`）→ **改参数 = 改下一个请求的 form 字段，立即生效，不污染服务器状态**。
- 真正 sticky 的只有服务器启动参数本身（模型、aligner、workers 等）。因此：
  - **"下一项生效"**：把新参数随下一条请求发送 ✅ 零成本；
  - **"会话级"**：GUI 在内存保存"当前参数集"，每条请求全量携带 ✅ 同样零成本。
- **两种语义 GUI 侧都可自由实现，服务器无需任何改动。** 这是 v1 架构的最大红利。

## 4. 响应结构（源码确认）

### 4.1 `response_format=srt` / `vtt`（步骤③核心）

`examples/cli/crispasr_output.cpp:1124-1145`：标准 SRT（逗号时间戳）/ VTT（句点时间戳）；**diarize 开启时说话人前缀直接写入字幕文本**（`prefix_speaker`，`crispasr_output.cpp:552-578`，形如 `(A) 文本`）。GUI 步骤③ = 发请求 + 响应体写盘，**服务器直接给成品 SRT**。

### 4.2 `verbose_json`（需要结构化数据时）

`crispasr_output.cpp:1163-1250`：顶层 `task/language/duration/text/segments[]`；每段 `id/start/end/text/temperature/avg_logprob/no_speech_prob`，可选 `speaker`（"A"/"B" 规范化标签，diarize 非空才出现）、`words[{word,start,end}]`（**秒，浮点**）、`tokens[]`。`words` 仅在后端产出词级时间戳时出现（`crispasr_output.cpp:1219-1231`）。

### 4.3 `diarized_json`（步骤②首选）

`crispasr_output.cpp:1279-1335`：同 verbose_json，但 `speaker` **始终存在**（缺省 "A"），多块运行附 `chunk_id`——**说话人标签是块局部的**，跨块同名不保证同一人（`crispasr_output.cpp:1294-1298`）。GUI 做人物合并必须按 chunk_id 分组。

### 4.4 旧版 `/inference` 的 `native_json`（步骤④数据通道）

`crispasr_output.cpp:763-795`：额外输出段级 `language`（LID）、**`audio_event`**（"Speech"/"Music"/"BGM"/"Applause"…）、`itn_flag`。**`/v1/audio/transcriptions` 的任何 response_format 都不携带 `audio_event`**——步骤④要事件标签，今天只能打 `/inference`（唯一调用 `crispasr_segments_to_native_json` 处：`crispasr_server.cpp:1980`）。

### 4.5 流式（SSE）

`stream=true` 且后端有 CAP_STREAMING（`crispasr_backend.h:121`）时，响应为 `text/event-stream`，`data: {"text":"<增量>"}` 逐块推送 + `data: [DONE]` 结束（`crispasr_server.cpp:2200-2250` 附近，增量按前缀 diff 计算）。批处理不需要它；实时转写界面可用。

### 4.6 通用错误格式

`{"error": {"message", "type", "code", "param"}}`，OpenAI 兼容（`crispasr_server.cpp:403` 附近注释）。GUI 解析错误只需取 `message` + `code`。

## 5. 服务器生命周期与 profile 机制

### 5.1 启动参数 → 进程（GUI 要做的事）

profile（JSONC）→ 启动脚本映射为 CLI 参数（`E:\AI\ASR\CrispASR\scripts\start-crispasr.ps1:189-207`）：

```
-m <model> -b <backend> --host 127.0.0.1 --port 5114 -t <threads>
[-am <aligner>] [--vad] [--force-aligner] [--chunk-seconds N] [-bs N] [-tp F]
```

关键结论：**模型、backend、aligner、API keys、workers 是启动期参数，改它们 = 重启进程**；其余一切（language、VAD、diarize、temperature、hotwords…）都是请求期参数，改它们 = 改下一个请求。GUI 的"模型切换"因此分两档：

1. **只换请求级参数**（含 language）→ 不重启，直接带参数发请求；
2. **换模型/aligner（即换 profile）**→ 停旧进程、起新进程、轮询 /health 至 200。

### 5.2 就绪与进度

- `/health`：`{"status":"ok","backend":"..."}` 或 503 `{"status":"loading"}`（`crispasr_server.cpp:2388-2401`）。公开无鉴权，是 GUI 的就绪探针。
- `/progress`：`{"busy":bool,"progress":-1..100}`，-1 = 空闲（`:2403-2414`；原子变量 `:527-536`）。**全局单值**——并发请求会互相覆盖，单实例串行模型下够用，GUI 批处理应按"当前正在处理的任务"显示它。

### 5.3 模型热切换 /load（备选路径）

`POST /load`，form 字段 `model`（必填）+ `backend`（可省，自动从 GGUF 探测）（`crispasr_server.cpp:2319-2386`）。行为：

- 加载成功 → 原子替换 backend，旧模型释放；失败 → 保留旧模型并报 500。
- **`CRISPASR_SERVER_WORKERS>1` 时直接 409 拒绝**（`:2325-2328`）——默认 1 无此限制。
- 局限：`/load` 只换模型和 backend，**aligner 等"启动期解析"的参数沿用旧值**（`np = params` 只覆盖 model/backend，`:2361-2364`）。而我们的日语 profile 恰恰以 aligner 配置为核心差异 → **跨 profile 切换必须走重启，/load 只适合同 profile 内换模型文件**。

### 5.4 单实例服务几个模型？

**一个进程 = 一个 ASR 模型**（§1）。`/v1/audio/separation` 的分离模型是唯一的"第二模型"例外——通过启动参数 `--separate-model` 额外加载（`crispasr_server.cpp:1600-1682`），与 ASR 模型并存。GUI 不应假设一个服务器进程能同时服务两个 ASR profile。

## 6. 六步工作流能力映射表（正式交付物）

| 步骤 | 能力 | 今天能不能用 | 怎么用 | 缺什么 |
|---|---|---|---|---|
| ① 分离人声/环境音 | `/v1/audio/separation`（htdemucs / mel-band-roformer） | **服务端支持，本地未实测**（STATUS.md 已标注）。需启动时 `--separate-model <gguf>`，模型需手动预置 | 启动服务器加 `--separate-model`；POST file + `stems`（CSV 或 "all"）；返回 multipart/mixed 分轨音频（`:1684-1830`，`stems` 校验 `:1700-1720`） | ①实测验证；②分离模型 gguf 下载放入 models；③GUI 需解析 multipart 响应保存分轨；④分离结果→人声轨再送 ASR 的编排是 GUI 侧工作 |
| ② 声纹区分对话人物 | 请求级 `diarize` 全套参数 | **服务端参数链路源码确认，本地未实测**（STATUS.md 标注；pyannote 分段器/声纹模型需先手动预置） | `diarize=true&diarize_method=pyannote&diarize_embedder=auto&diarize_max_speakers=N` + `response_format=diarized_json`（speaker/chunk_id，§4.3）；或 `srt` 直接得到 `(A) 文本` 字幕 | ①实测（模型预置 + 网络）；②跨 chunk 说话人合并逻辑在 GUI；③" named 声纹库"（--enroll-speaker）仅 CLI，服务端不暴露——GUI 人名映射只能做"标签→用户指定名字" |
| ③ ASR → .srt | `/v1/audio/transcriptions` + `response_format=srt` | ✅ **已完成实测**（四个日语 profile 全链路通过） | POST file + language + VAD/aligner 等（profile 默认已在启动参数中）；响应体直接写 .srt | 无。GUI 最小可用功能即此 |
| ④ 语气词/事件 → .srt | sensevoice 后端：**事件标签可用，情感标签被上游刻意移除** | 事件：服务端支持但**无专门 response_format 携带**；情感：**上游设计上不提供**（EU AI Act 合规决策） | 事件标签走 `/inference`（native_json 含 `audio_event`，§4.4）+ 自行把事件段转成字幕行；"语气/情感"如需继续，只能靠外部 LLM 对转写文本做文本级分析（可复用 5113 通道），**声学情感识别在本引擎内拿不到** | ①sensevoice 本地未实测（模型 ~900MB 需手动预置）；②GUI 需 native_json→SRT 的转换器；③若坚持声学情感，需另寻引擎（超出本审计）——**建议步骤④定义改为"事件 + 文本级语气"** |
| ⑤ 翻译字幕 | 用户自有 Hy-MT2-30B-A3B @127.0.0.1:5113（OpenAI 兼容） | ✅ 用户环境已就绪（KEY=MODEL=default） | GUI 直接 HTTP POST `/v1/chat/completions` 到 5113，输入 SRT 文本，输出译文，按时间轴重组 SRT。**与 CrispASR 服务器无关**（其内置 /v1/translate 需另装 m2m100 模型，不采用） | 纯 GUI 侧工作：SRT 解析/重组、prompt 设计、断句保持。注意 5113 归用户维护，GUI 只做客户端 |
| ⑥ 合并多个 .srt | 纯 GUI/文件处理 | ✅（无引擎依赖） | GUI 侧解析多个 SRT → 按策略合并（并行轨道/串行拼接/说话人分轨）→ 输出 | 策略定义是产品问题；引擎侧零依赖 |

**步骤链数据结构要求**：CrispASR 服务器天然是"单步执行者"（一次请求一步）。多步编排（①→③、③→⑤）完全由 GUI 的任务队列驱动：每个 job 记录 `steps[]`，v1 只填一步 `[asr]`，未来追加 `[separate, asr]`、`[asr, translate, merge]` 时服务器侧零改动。这印证了"任务模型从第一天起就是步骤链式"的要求——**引擎不需要为编排做任何事**。

## 7. C API（include/*.h）——未来切换进程内模式的可行性结论

按修正要求只给结论，不深入：

1. **可行性：高。** 三个头文件（`crispasr.h` 1080 行 / `crispasr_session.h` 878 行 / `crispasr_chat.h` 344 行）构成完整 C ABI，七种语言绑定（Python/Rust/Dart/Go/Java/Ruby/C#）与 WASM 全部构建其上（`docs/bindings.md`），CrisperWeaver 即 Dart 消费者实证。
2. **契约要点**（未来迁移时的注意事项，均有源码注释）：单会话单线程使用（`crispas.h:40-42`）；回调在调用线程触发且禁止重入（`crispas_session.h:26-49`）；结果对象 caller-free（`crispas_session_result_free` 等）；ABI 严格 append-only（`docs/contributing.md:400-431`）。
3. **迁移成本**：GUI 已是 C++，进程内化 ≈ 把 HTTP 客户端换成 `crispasr_session_*` 直调，业务层（队列/参数模型）不变。**但 v1 不做**——HTTP 边界提供崩溃隔离（引擎 panic 不拖垮 GUI）与独立升级，收益大于一次序列化开销。
4. 触发迁移的信号（供未来判断）：进程间文件传输开销不可接受 / 需要会话级热更新 C API 独有参数（如 `set_punc_model`）/ 需要流式逐 token 回调。

## 8. 构建与版本稳定性

- **构建**：CMake 单一入口。`crispasr-lib` 目标（`src/CMakeLists.txt:107`），Windows 下 `BUILD_SHARED_LIBS=ON` 即产出 DLL（`CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS=ON`，`src/CMakeLists.txt:6-9`）；官方发布走 CI（Windows CUDA/CPU、macOS、Linux）。**GUI v1 不需要构建它**——直接使用已部署的 `crispasr.exe`（`E:\AI\ASR\CrispASR\cuda13\`）。
- **版本稳定性**：`crispasr_c_api_version()` 返回 "0.7.0"（`src/crispasr_c_api.cpp:12600` 附近），每次扩展记录在注释中，历史全是"纯新增"；HTTP API 面向 OpenAI 兼容面，稳定性高于 C ABI。**GUI 的脆弱点**是扩展参数（3.2 节）随版本演进——建议 GUI 对未知响应字段/参数宽容（服务器对未知参数本身就是忽略的）。
- **风险**：CrispASR 迭代快（0.8.x 每周级）；升级后应回归 `/progress`、`diarized_json`、`/inference` 三个 GUI 依赖点。

## 9. 推荐精读源码文件列表

| 文件 | 规模 | 为什么值得读 | 与需求的关联 |
|---|---|---|---|
| `examples/cli/crispasr_server.cpp` | 4046 行 | 全部端点、form 解析、鉴权、生命周期、/load、分离、SSE 的唯一事实源 | GUI 客户端的"协议规范"；写 HTTP 层前必读 §2027-2320（主端点）与 §1263-1470（生命周期） |
| `examples/cli/crispasr_output.cpp` | ~1400 行 | 所有响应格式（srt/vtt/verbose_json/diarized_json/native_json）的生成逻辑 | GUI 解析响应的字段级依据；步骤③④的输出契约 |
| `examples/cli/crispasr_backend.h` | ~300 行 | `crispasr_segment` 结构（speaker/chunk_id/words/audio_event）与 CAP_* 能力位 | 理解哪些后端能做什么（CAP_STREAMING/CAP_DIARIZE…），GUI 功能开关的依据 |
| `docs/concurrency.md` | ~200 行 | 服务器并发模型、workers 语义、批量离线建议 | GUI 批处理设计（为何默认 1 worker、何时值得 >1） |
| `docs/diarization-speakers.md` | ~150 行 | 说话人标签两种路径（会话聚类 vs 命名声纹库）的语义与隐私边界 | 步骤②的产品设计输入（GUI 只做标签→名字映射） |
| `scripts/start-crispasr.ps1`（项目根，非 Source） | ~250 行 | profile JSONC → CLI 参数的映射实现 | GUI "按 profile 启动服务器"的直接参考实现 |
| `profile/Japanese/*.conf`（项目根） | 4 文件 | 已实测的种子配置，字段即 GUI 参数面板的初始集 | GUI profile 加载器的输入样例 |
| `include/crispasr_session.h` | 878 行 | 会话 C ABI 全貌（为未来进程内化储备） | §7 迁移路径的第一手资料 |
