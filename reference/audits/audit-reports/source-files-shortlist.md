# 源码文件精选清单（source-files-shortlist）

> 撰写：A002。日期：2026-10-11。目的：从三个源码树中挑出**最值得复制到 CrispRustGUI 工作区作为学习材料**的文件，按优先级排序。
> 架构前提：GUI = CrispASR HTTP 服务器（5114）的客户端（详见 crispasr-audit.md / crisperweaver-audit.md / eui-neo-audit.md）。
> "复制"指拷贝到工作区 `reference/` 类目录供精读仿写；被审计树本身不动。

## 优先级 P0（开始写代码前必读/必仿写）

| # | 来源 | 路径 | 规模 | 一句话理由 |
|---|---|---|---|---|
| 1 | CrispASR | `examples/cli/crispasr_server.cpp` | 4046 行 | HTTP 协议唯一事实源：所有端点、form 参数、鉴权、SSE、/load、分离端点——GUI 客户端层的"规格书" |
| 2 | CrispASR | `examples/cli/crispasr_output.cpp` | ~1400 行 | 全部响应格式（srt/vtt/verbose_json/diarized_json/native_json 含 audio_event）的生成逻辑，GUI 解析器的字段级依据 |
| 3 | CrisperWeaver | `lib/services/batch_queue_service.dart` | 517 行 | 批处理队列蓝本：BatchJob 任务模型（status/快照/resumeOffset）、按模型分组、崩溃两遍修复、指纹去重 |
| 4 | CrispASR | `scripts/start-crispasr.ps1`（项目根，非 Source） | ~250 行 | profile JSONC → 启动参数映射的现成实现，GUI 进程管理器的直接翻译来源 |
| 5 | CrispASR | `profile/Japanese/*.conf`（4 个） | 每个 ~60 行 | 已实测种子配置：GUI profile 加载器的输入样例 + 参数面板字段集 |
| 6 | EUI-NEO | `examples/eui_demo.cpp` + `examples/multipage_table.cpp` | 43 + 311 行 | 最小应用骨架 + 最接近目标形态的多页签表格示例，main.cpp 与主界面的起点 |

## 优先级 P1（写对应模块时精读）

| # | 来源 | 路径 | 规模 | 一句话理由 |
|---|---|---|---|---|
| 7 | EUI-NEO | `docs/异步.md` + `include/eui/async.h` | ~120 + ~100 行 | 批处理任务 → app::async 的映射设计（runOnce/restart/cancel 全语义） |
| 8 | EUI-NEO | `docs/网络.md` + `core/platform/network.cpp` | ~110 + ~200 行 | 确认 GET-only 边界 + libcurl 回调回 UI 线程的实现范式，自写 POST 客户端时仿写 |
| 9 | CrisperWeaver | `lib/services/batch_persistence_service.dart` | 338 行 | 每任务 JSON + 追加 checkpoint + manifest 的持久化布局（断点续跑） |
| 10 | CrisperWeaver | `lib/screens/transcription_screen.dart`（2804-2960 `_startBatchRun`） | ~160 行 | 批处理主循环行为规范：参数快照时机、模型加载一次、进度回写 |
| 11 | CrispASR | `docs/concurrency.md` | ~200 行 | 服务器并发模型与 workers 语义，批处理吞吐设计依据 |
| 12 | CrisperWeaver | `lib/services/transcription_service.dart`（30-206 AdvancedTranscribeOptions） | ~180 行 | 参数面板完整字段清单 + 到 HTTP form 字段的映射基础 |
| 13 | EUI-NEO | `components/datatable.h` | ~200 行 | 任务队列表格组件 API |

## 优先级 P2（特定问题出现时再读）

| # | 来源 | 路径 | 规模 | 一句话理由 |
|---|---|---|---|---|
| 14 | CrispASR | `examples/cli/crispasr_backend.h` | ~300 行 | CAP_* 能力位 + crispasr_segment 结构（speaker/chunk_id/audio_event），做功能开关与解析时查 |
| 15 | CrispASR | `docs/diarization-speakers.md` | ~150 行 | 步骤②说话人标签语义（会话聚类 vs 命名声纹库）与隐私边界 |
| 16 | CrisperWeaver | `lib/services/preset_service.dart` | 397 行 | schemaVersion + 双向兼容反序列化——profile 演进策略 |
| 17 | CrispASR | `include/crispasr_session.h` | 878 行 | 未来进程内化路径的第一手 C ABI 资料（v1 不用，留档） |
| 18 | EUI-NEO | `examples/keyboard.cpp` / `examples/ffmpeg_video_player.cpp` | 194 / 516 行 | 快捷键处理 / 后台线程+UI 回调协作范式 |
| 19 | EUI-NEO | `include/eui/dsl_app.h` | ~150 行 | DslAppConfig 全字段（onStart 放进程启动逻辑） |
| 20 | CrisperWeaver | `lib/engines/crispasr_engine.dart`（520-700） | ~180 行 | companion 加载顺序与失败清理，模型加载失败 UX 参照 |

## 明确不复制（防止误导工作区）

- CrisperWeaver 全部 `engines/`、`native/`（FFI/isolate 进程内接入）、`providers/`（Riverpod）、`l10n/`、web/mobile 路径。
- CrisperWeaver `lib/services/model_service.dart` + `model_catalog.dart`（8800 行下载/目录体系）——模型手动放置是项目硬规则，GUI 只做存在性检查。
- CrispASR 其余引擎实现（`src/` 327 个文件、`ggml/`）——v1 是客户端，不碰引擎内部。
- EUI-NEO `3rd/` 依赖源码（快照库已整体保存，工作区以 add_subdirectory 引用即可）。
