# EUI-NEO 审计报告（面向 CrispRustGUI v1）

> 撰写：A002。日期：2026-10-11。审计视角：**作为"HTTP 客户端 + 服务器进程管理器"的 GUI 框架**。
> 被审计对象：`E:\project\Learn\sources\github.com\sudoevolve\EUI-NEO-v0.6.1`（v0.6.1 快照，Apache-2.0，C++17），全程只读。
> 证据标注：`源码确认`（文件:行号）/ `文档确认`（docs/*.md）。

---

## 1. 框架概貌

- **技术栈**：C++17、CMake 唯一官方构建入口（v0.5.9 起，`docs/releases/v0.5.9.md`）、GLFW/SDL2 窗口 + OpenGL/Vulkan 渲染（四组合 CI 验证，`docs/releases/v0.6.1.md`）。
- **编程模型**：GPUI 风格 DSL——每帧 compose 重建 UI 树（`docs/组件.md:5`："组件内部也只声明 DSL 树，事件/hover/transition/dirty rect 交给内部 Runtime"），状态放在 compose 外的页面变量，`ui.state<T>` / `Signal<T>` 双向绑定（`docs/组件.md:62`）。
- **规模**：`components/` 39 个头文件组件，`core/` 运行时（layout/render/platform/window/app），`include/eui/` 公共 API，示例 9 个。

## 2. 能力清单（对照长生命周期工具应用的需求）

### 2.1 控件完备度（源码确认，`components/` 目录）

✅ 齐备：button / checkbox / radio / switch / slider / stepper / input / dropdown / segmented / tabs / panel / card / navbar / sidebar / dialog / toast / tooltip / contextmenu / progress / **datatable（带虚拟滚动）** / **virtuallist（固定行高虚拟列表，`docs/组件.md:115`）** / virtualmasonry / scrollview / carousel / barchart / linechart / piechart / markdown / image / colorpicker / datepicker / timepicker。

工具型 GUI（队列表格 + 参数面板 + 日志视图）所需的控件全部覆盖： datatable 用于任务队列表格、tabs 用于多页签、input/slider/dropdown 用于参数面板、toast/dialog 用于反馈、markdown 用于帮助页。

### 2.2 文本渲染与 CJK（源码确认）

- FreeType 渲染，字形图集缓存（`core/render/text.cpp:36-40`：2048 灰度图集 + 1024 彩色图集）。
- **CJK 有保障**：Windows 系统字体回退链含 `msyh.ttc`（微软雅黑）（`core/render/text.cpp:305-309`）；默认内置字体本身即中文字体"金楠军军体"（`text.cpp:37`）。字体栈逐级回退 + 缺字补字（`docs/DSL.md:374`）。
- 字体可替换：`DslAppConfig::fonts(textFont, iconFont)`（`include/eui/dsl_app.h:117`）。
- **输入法**：Windows + Linux X11 IME 支持，候选框跟随文本光标（`docs/releases/v0.6.1.md`、`docs/组件.md:474` 附近、`core/platform/ime_bridge.h`）。

### 2.3 主题

`theme.h` 组件 + DesignToken 体系（datatable 等组件均从 tokens 取色，`components/datatable.h:21-36`），暗色模式内建（`tokens.dark`）。可满足工具应用的一致外观需求。

## 3. 新增评估维度：HTTP 客户端 + 服务器进程管理器

这是 v1 架构下 GUI 的真实工作负载，逐项评估：

### 3.1 HTTP 请求——**最大的缺口，需要自备客户端**

- 框架自带 `eui::network::requestText(key, url)`（`include/eui/network.h:13`，实现 `core/platform/network.{h,cpp}`）：**仅 GET**，无 header/body/取消/进度/重试（`docs/网络.md` "当前限制"一节明示）。
- 底层：Windows 优先 libcurl，找不到 curl 则退化为 `URLDownloadToFileA`；非 Windows 必须 libcurl（`docs/网络.md`；`3rd/dependencies.cmake` 中 curl 为可选依赖，找到时以 `EUI_HAS_CURL` 导出，`CMakeLists.txt:链接行`）。
- **结论**：`/v1/audio/transcriptions` 需要 **multipart POST**（上传音频文件）——框架现成能力**完全不够**。两条路：
  1. **推荐**：C++ 侧直接用 libcurl 写一个 ~200 行的 HTTP 客户端封装（multipart/form-data 构造 + POST + 读响应），curl 已在 EUI-NEO 依赖图里（找到即链接），不引入新第三方；
  2. 备选：调用 Windows `WinHTTP` API（无新依赖，但代码更多、跨平台差）。
- JSON 解析：**够用**。`eui/json.h` 基于 yyjson（`3rd/yyjson-0.12.0`，已随框架集成）提供只读 Document/Value + JSON Pointer + 类型判断（`docs/网络.md` JSON 一节）。解析服务器响应（json/verbose_json/diarized_json）没有问题；**注意它是只读的**——GUI 生成请求体（如翻译步骤向 5113 发 OpenAI chat 请求）需要自己拼 JSON 字符串（SRT/文本转 JSON 转义，几十行）或直接用 yyjson 写 API。
- 网络回调已在框架 async 体系内（完成后回 UI 线程并唤醒 compose，`docs/网络.md`），自写客户端应复用 `app::async`（§3.3）而不是自己开线程。

### 3.2 服务器进程管理（子进程）——**框架无现成 API，用标准库即可**

- EUI-NEO 没有 subprocess 管理 API（`core/platform/` 只有 network/platform/async/audio/tray/ime）。这**不是缺陷**：C++17 标准做法足够：
  - Windows：`CreateProcess` + 管道读 stderr（启动日志）+ `TerminateProcess`（停止）；或 `QProcess` 式封装自写 ~100 行。
  - 项目已有参考实现：`scripts/start-crispasr.ps1` / `stop-server.cmd` 的启停逻辑可直接翻译成 C++（含 `CUDA_VISIBLE_DEVICES` 环境变量设置）。
- 进程存活检测：轮询 `/health`（HTTP GET，`requestText` 现成能力即可覆盖）比检测 PID 更可靠（同时验证"模型已加载"）。

### 3.3 UI 响应性（后台任务模型）——**框架强项**

- `app::async::runOnce/restart/cancel`（`include/eui/async.h`、`docs/异步.md`）：任务跑在框架线程池，**完成回调回 UI 线程**，可在回调里安全改状态触发重绘。带 Status 枚举（Idle/Running/Done/Failed/Canceled）与 CancelToken。
- 这正是批处理队列需要的模型：每个 job 的 HTTP 请求 = 一个 async 任务；`restart(key)` 适合"重试该任务"；`status(key)` 驱动队列行状态显示。
- 自建线程改状态后须调 `app::requestUpdate()` 唤醒（`docs/异步.md` 开头）——进程管理器线程（读服务器 stderr 日志）要遵守。

### 3.4 应用骨架与嵌入方式（文档确认）

- 官方嵌入方式（`docs/集成指南.md`、`docs/releases/v0.5.9.md`）：外部项目 CMake 调 `eui_neo_configure_app(my_app)` 即获得框架入口/资源部署；支持 `add_subdirectory()`（3rd/ 子目录）、FetchContent、安装后 find_package 三种接入。**3rd/EUI-NEO 子目录方式被官方一等支持**，README 给出模板——验证通过。
- 应用定义：`DslAppConfig`（窗口标题/尺寸/字体/`onStart()` 一次性回调，`include/eui/dsl_app.h`）+ 页面 compose 函数。v0.6.1 新增 `onStart()` 适合放"启动 CrispASR 服务器进程"的初始化。
- 窗口能力：自绘窗口移动/缩放委托（v0.6.1 新增，X11 EWMH）、透明 framebuffer（可选）——桌面工具常规需求覆盖。

## 4. 已知缺口清单（对 v1 的实际影响）

| 缺口 | 影响 | 对策 |
|---|---|---|
| HTTP 仅 GET | **阻塞核心功能**（multipart POST 上传音频） | 自写 libcurl 客户端封装（~200 行，curl 已在依赖图） |
| JSON 只读 | 请求体需手拼 | 自写 OpenAI/SRT JSON 序列化小工具，或用 yyjson 写 API |
| 无 save 文件对话框 | "导出 SRT 到指定位置" UX | 用 openFileDialog 变通（用户选目标文件），或平台 API 补一个 save 对话框（Windows `GetSaveFileNameW`，与框架 openFileDialog 同源同风格） |
| 无子进程 API | 进程管理器需自写 | C++ 标准做法 ~100 行；参考现有 PowerShell 启动脚本 |
| 0.x 版本 API 未承诺稳定 | 升级有成本 | **已锁定 v0.6.1 快照**（Learn 库存档）；升级视为独立任务重新评估 |
| 社区规模中等（~2k stars） | 框架 bug 需自修 | 快照在手可改；核心渲染/布局问题概率低（v0.6.1 已过四后端 CI） |

无缺口项：控件、CJK 字体、IME、主题、异步任务模型、虚拟列表/表格、markdown 渲染、剪贴板（`docs/组件.md` input 组件）、toast/dialog。

## 5. 与需求最接近的示例

| 示例 | 规模 | 相关点 |
|---|---|---|
| `examples/multipage_table.cpp` | 311 行 | **最接近目标形态**：多页签 + 大表格 + 状态分页，即"任务队列 + 转写结果"两页签的骨架 |
| `examples/eui_demo.cpp` | 43 行 | 最小应用骨架：DslAppConfig + compose 入口，v1 的 main.cpp 起点模板 |
| `examples/keyboard.cpp` | 194 行 | 键盘事件/快捷键处理（批处理"开始/停止"热键） |
| `examples/ffmpeg_video_player.cpp` | 516 行 | 后台线程 + 帧回调 + UI 更新的协作模式（对应"后台转写 + 进度条"的线程协作范式） |

## 6. 0.x 版本变动风险的实际评估（基于 CHANGELOG 实读）

- `docs/releases/` 只有 v0.5.8 / v0.5.9 / v0.6.1 三份。v0.5.9 → v0.6.1 的变化（`v0.6.1.md`）全部为**增量**：新增回调（onStart）、新增窗口 API、修复类改动；"升级说明"明确"新增配置默认不改变原有行为"。
- v0.5.9 的破坏性变更是构建系统层面（移除 Xmake、统一 CMake），对已用 CMake 的我们无影响。
- 判断：**API 面处于"快速增量期"而非"动荡重构期"**；DSL（ui.* 命令集）与组件 API 在 0.5.x→0.6.x 间保持兼容。锁定快照使用风险可控；真正的风险是"未来某个大版本（0.7+）可能重构 DSL"——到时以快照为准不跟进即可。

## 7. 推荐精读源码文件列表

| 文件 | 规模 | 为什么值得读 | 与需求的关联 |
|---|---|---|---|
| `docs/集成指南.md` | ~200 行 | 三种接入方式（子目录/FetchContent/SDK）的官方步骤 | 3rd/EUI-NEO 嵌入的操作手册 |
| `docs/组件.md` | ~500 行 | 全组件 API + 状态模型（ui.state/Signal） | 参数面板与队列表格的实现依据 |
| `docs/异步.md` | ~120 行 | runOnce/restart/cancel 全语义 | 批处理任务 → async 任务的映射设计 |
| `docs/网络.md` | ~110 行 | requestText 能力边界（GET-only）与 JSON 只读边界 | **明确必须自写 HTTP 客户端的证据** |
| `include/eui/dsl_app.h` | ~150 行 | DslAppConfig 全字段（fonts/onStart/窗口） | main.cpp 与应用初始化（启动服务器进程）的落点 |
| `components/datatable.h` | ~200 行 | 表格 API（rows/列/行高/虚拟滚动 tokens） | 任务队列表格组件 |
| `core/platform/network.h` + `network.cpp` | ~200 行 | requestText 的 libcurl 用法与回调回 UI 线程的实现 | 自写 POST 客户端时复用其线程回调模式 |
| `examples/multipage_table.cpp` | 311 行 | 多页签+表格+状态的最接近完整示例 | v1 主界面的原型参考 |
| `examples/eui_demo.cpp` | 43 行 | 最小骨架 | main.cpp 模板 |
