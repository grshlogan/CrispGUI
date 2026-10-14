# Build Environment

> How CrispGUI is developed and built. Revised on 2026-10-12 (round 6) with the UI fonts and the EUI-NEO runtime facts of Phase 2, on 2026-10-12 (round 7) with the system libraries, test entry points, and runtime facts of Phase 3, and on 2026-10-12 (round 10) with the upload, the probe, and the Windows checklist of Phase 4. The Windows executable is cross-compiled in WSL2 with MinGW-w64, and EUI-NEO comes from the shared Learn library. All development happens in WSL2, WSL2 never mounts Windows drives, and the Windows side does no development. Roles and delivery to Windows are in [agents/README.md](agents/README.md).

## 1. Target and toolchains

- **Target**: a Windows 10/11 x64 desktop application. C++17, CMake ≥ 3.25 (the project uses `add_subdirectory(... SYSTEM)`; EUI-NEO itself needs 3.14), Ninja.
- **Windows executable**: cross-compiled in WSL2 with MinGW-w64 GCC 12 and the POSIX thread model (owner decision of 2026-10-11, formerly OQ-1). The result is a statically linked executable plus EUI-NEO's `assets/` folder, so a delivery is one folder ([§4.2](#42-eui-neo-build-facts)).
- **Development builds**: Linux, in the WSL2 distribution `claude-debian12`, rendering through WSLg with GLFW's X11 backend. EUI-NEO's `eui_demo` renders correctly under WSLg ([Phase 0 report](devlog/phase-0.md)).
- **UI framework**: EUI-NEO v0.6.1 from the shared Learn library ([§4.1](#41-eui-neo-snapshot)), built into this project's build tree.
- **HTTP**: the in-repo transport of [ARCHITECTURE.md §2.3](ARCHITECTURE.md#23-apiclient-and-http-transport): WinHTTP on Windows and libcurl on Linux; GET since Phase 3, POST with a streamed multipart upload since Phase 4. EUI-NEO itself requires libcurl on Linux.

## 2. Development loop

1. The agent edits, builds, and tests in `<WSL_ROOT>`. Linux builds cover compilation, unit tests, HTTP tests against a local stub server, and UI behavior under WSLg; MinGW builds cover compiling and linking for Windows. These results are platform-limited.
2. Windows-native behavior — window composition, DPI, IME, file locking, the real engine, the GPU — is accepted only on the owner's Windows machine, using a delivery bound to an exact commit ([delivery to Windows](agents/README.md#delivery-to-windows)).
3. Windows drives are never mounted or accessed from WSL2 ([AGENTS.md](../AGENTS.md#environment-boundaries)): no `/mnt/<drive>` paths and no interop calls to Windows executables.
4. Build output stays in `<WSL_ROOT>/build*/`, which Git ignores.

### 2.1 WSL2 toolchain

Checked on 2026-10-11 in `<WSL_ROOT>` (Linux 6.18.40.1-microsoft-standard-WSL2) with `command -v`, `--version`, `dpkg-query -W`, and `pkg-config --modversion`. Everything listed is installed; the owner installed the packages.

| Tool or package | Installed version | Needed for |
|---|---|---|
| `g++-mingw-w64-x86-64-posix`, `gcc-mingw-w64-x86-64-posix` | 12.2.0-14+deb12u1+25.2+b1 | Windows builds. `x86_64-w64-mingw32-g++` resolves to the POSIX-thread variant; the win32-thread variant is not installed |
| `binutils-mingw-w64-x86-64`, `mingw-w64-x86-64-dev` | 2.40-2+10.4, 10.0.0-3 | Windows builds; the 10.0 headers lack one constant EUI-NEO uses ([§4.2](#42-eui-neo-build-facts)) |
| `g++` | 12.2.0-14+deb12u1 | Linux builds |
| `cmake`, `ninja-build` | 3.25.1, 1.11.1 | Both builds |
| `libgl1-mesa-dev` | 22.3.6-1+deb12u2 | OpenGL; EUI-NEO's OpenGL backend stops configuration without it |
| `libx11-dev`, `libxrandr-dev`, `libxinerama-dev`, `libxcursor-dev`, `libxi-dev`, `libxext-dev` | 1.8.4, 1.5.2, 1.1.4, 1.2.1, 1.8, 1.3.4 | GLFW 3.4's X11 backend, which stops configuration when any of them is missing |
| `libcurl4-openssl-dev` | 7.88.1-10+deb12u15 | EUI-NEO on Linux (`find_package(CURL REQUIRED)`) and the Linux HTTP transport |
| `libglib2.0-dev` | 2.74.6-2+deb12u9 | EUI-NEO's Linux tray backend (gio) |
| `gdb`, `ccache` | 13.1, 4.7.5 | Debugging and faster rebuilds (optional) |
| `make`, `pkg-config`, `python3`, `git` | GNU Make 4.3, 1.8.1, 3.11.2, 2.39.5 | General |

To rebuild this environment on Debian 12:

```bash
sudo apt install mingw-w64 g++ cmake ninja-build libgl1-mesa-dev libx11-dev libxrandr-dev \
  libxinerama-dev libxcursor-dev libxi-dev libxext-dev libcurl4-openssl-dev libglib2.0-dev
sudo apt install gdb ccache   # optional
```

Not installed and not needed now: the Wayland development packages (the Linux build uses GLFW's X11 backend only, as EUI-NEO's own Linux release build does), Vulkan and SDL2 (the backends are OpenGL and GLFW), `wine64`, and `zenity`. EUI-NEO's Linux file dialog runs `zenity` or `kdialog` at run time (`core/platform/platform.cpp`), so file pickers in Linux development builds need `zenity` once Phase 5 tests them.

### 2.2 Commands

Phase 0 smoke builds (`verified` on 2026-10-11; results in the [Phase 0 report](devlog/phase-0.md)):

```bash
tools/smoke/build-eui-smoke.sh all     # EUI-NEO into build/eui-smoke-linux and build/eui-smoke-mingw
ctest --test-dir build/eui-smoke-linux -L unit
ctest --test-dir build/eui-smoke-linux -L probe -E '^shadertoy_probe$'   # opens windows under WSLg
tools/smoke/package-eui-smoke.sh       # Windows delivery; needs a clean tree on origin/main
```

Never run `shadertoy_probe` against the Learn snapshot: it creates `build-shadertoy/shadertoy/` under its compile-time source path ([§4.2](#42-eui-neo-build-facts)).

Project builds (`verified` on 2026-10-11 and 2026-10-12; results in the [Phase 1](devlog/phase-1.md), [Phase 2](devlog/phase-2.md), [Phase 3](devlog/phase-3.md), and [Phase 4](devlog/phase-4.md) reports):

```bash
# Linux development build (WSLg); the app is build/linux/crispgui
cmake -S . -B build/linux -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build/linux
ctest --test-dir build/linux --output-on-failure      # -L unit or -L integration for one group
CRISPGUI_CRISPASR_ROOT=<folder> build/linux/crispgui       # reads <folder>/profile as the CrispASR root
CRISPGUI_DEV_AUTOSTART=<folder>/<profile> ...              # starts that profile once the first checks are done
CRISPGUI_DEV_TRANSCRIBE=[choose:]<file> ...                # chooses the file on the Transcribe page at launch, without the file dialog, and transcribes it once a server runs; with "choose:" it only chooses
build/linux/api_probe --port <port> --key <key> <audio file>  # probes a running server (tools/api_probe)

# Windows release build (MinGW-w64 cross-compilation) and its delivery
cmake -S . -B build/mingw -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/mingw-w64-x86_64.cmake
cmake --build build/mingw
tools/package-windows.sh               # needs a clean tree on origin/main
```

The top-level `CMakeLists.txt` sets the EUI-NEO options of [§4.2](#42-eui-neo-build-facts) and links every MinGW executable with `-static`, so these commands need no extra flags; `-DCRISPGUI_EUI_NEO_DIR=<path>` points at another EUI-NEO snapshot, and `-DCRISPGUI_FONT_DIR=<path>` at another folder with the UI fonts ([§4.3](#43-ui-fonts)). Build directory names must not contain the segments `vk`, `vulkan`, or `sdl2`, because EUI-NEO picks its backends from them.

## 3. Repository layout policy

- EUI-NEO is not copied into the repository. Builds read it from the path in `CRISPGUI_EUI_NEO_DIR` ([§4.1](#41-eui-neo-snapshot)) and write what they generate into the build tree.
- `cmake/toolchains/mingw-w64-x86_64.cmake`: the MinGW-w64 toolchain file.
- `CMakeLists.txt`, `src/`, `tests/`: the application, its two libraries (`crisp_core` without UI dependencies, `crisp_ui` on EUI-NEO), and the unit tests.
- `tools/package-windows.sh` packages a Windows delivery of the application; `tools/windows/` holds what deliveries carry besides the build: the test runner, the probe's wrapper `run-api-probe.cmd`, `CHECKLIST.md`, the Windows checks of the current phase, and `WINDOWS-AGENT.md`, the brief for the Windows-side agent; `tools/api_probe/` is a console tool that checks the ApiClient against a running server ([§6](#6-verification-entry-points)); `tools/diagnostics/` holds Windows-only measurement tools, which `tools/package-diagnostic.sh <target>` packages one at a time as a small delivery; `tools/smoke/` holds the Phase 0 smoke build and its packaging; `tools/extract-cli-flags.py` regenerates `src/core/cli_flags_table.inc` from a CrispASR source snapshot; `tools/dev/` holds WSL2 development helpers (window captures under WSLg, the Markdown check, a stand-in CrispASR root), described in its `README.md`.
- `tests/`: unit tests (CTest label `unit`), and integration tests (label `integration`) that start `stub_crispasr`, a stand-in for the CrispASR server built from `tests/stub_crispasr.cpp`, and talk to it over 127.0.0.1. The build copies the reference profiles and the server answers of `tests/data/` (provenance in `tests/data/README.md`) to `testdata/` next to the test executables; deliveries carry that folder and the stub.
- `fonts/`: the UI fonts the owner provides ([§4.3](#43-ui-fonts)); ignored by Git, so a fresh clone builds with EUI-NEO's default font.
- `params/`: the parameter catalog and model packs ([ARCHITECTURE.md §2.7](ARCHITECTURE.md#27-parameter-model-pluggable)); data, committed.
- `build*/`, `appdata/`, `CMakeUserPresets.json`, `.cache/`, and `fonts/` are ignored by Git (`.gitignore`).
- Line endings are LF (`.gitattributes`), except CRLF for `*.cmd` and `*.bat`, and except `reference/` and the recorded server answers in `tests/data/deployed-*/`, which Git stores byte for byte; the recorded answers are also exempt from Git's whitespace checks.
- `reference/`: read-only learning material, never part of the build.
- Deliveries are written outside the checkout, to `<HANDOFF_OUT>`.

## 4. Dependencies

| Dependency | Source | Notes |
|---|---|---|
| EUI-NEO v0.6.1 | Shared Learn library ([§4.1](#41-eui-neo-snapshot)) | UI framework, Apache-2.0; bundles GLFW 3.4, glad, FreeType, zlib 1.3.1, libpng 1.6.43, md4c, miniaudio, nanosvg, stb_image, a tray library, and yyjson 0.12.0 |
| WinHTTP | Windows system library | Windows HTTP transport; GET since Phase 3, POST since Phase 4 |
| IP Helper (`iphlpapi`) | Windows system library | Finds the process listening on a port, to stop an adopted server (Phase 3) |
| Winsock (`ws2_32`) | Windows system library | The stub server and the integration tests only |
| libcurl | Debian package, Linux builds only | Required by EUI-NEO on Linux; the Linux HTTP transport since Phase 3, POST since Phase 4 |
| HarmonyOS Sans SC | Owner-provided in `<WSL_ROOT>/fonts/` ([§4.3](#43-ui-fonts)) | UI font; not in Git; license `not verified` |
| Compilers, CMake, Ninja | [§2.1](#21-wsl2-toolchain) | Installed |
| CrispASR server | Deployed on Windows at `<CRISPASR_ROOT>\cuda13\crispasr.exe` | Runtime dependency only; never built here |
| Models | `<CRISPASR_ROOT>\models\` on Windows | Runtime data only |

### 4.1 EUI-NEO snapshot

- Location: `/home/claude/projects/Learn/sources/github.com/sudoevolve/EUI-NEO/v0.6.1`, which is `../Learn/sources/github.com/sudoevolve/EUI-NEO/v0.6.1` relative to `<WSL_ROOT>`. `tools/smoke/build-eui-smoke.sh` and, from Phase 1, the project build take it from `CRISPGUI_EUI_NEO_DIR`, whose default is that relative path. The directory was moved there from `EUI-NEO-v0.6.1` on 2026-10-11 to follow the Learn naming convention.
- Provenance (`verified`, 2026-10-11): tag `v0.6.1` of `sudoevolve/EUI-NEO` peels to commit `1647709068e36834ab26da5a97ef4ad98aa34c9d` (`git ls-remote`). The tag archive, downloaded with the owner's authorization to the Learn library's `.downloads/EUI-NEO-v0.6.1.zip`, has 15,890,020 bytes (as in the Learn index), SHA-256 `f0709a325b343691cf2d0ac1ce51cbe6d321e8a972304e0e7deffe6c71b3a2c6`, and that commit as its zip comment. Its extracted content and the snapshot directory are identical (`diff -rq`, exit 0; 1428 files). License: Apache-2.0.
- Read-only: never modified, patched, or built inside its own directory.

### 4.2 EUI-NEO build facts

From EUI-NEO v0.6.1's `CMakeLists.txt`, `3rd/dependencies.cmake`, and `.github/workflows/`, and from the Phase 0 smoke builds of 2026-10-11 (`verified` where a build observed it):

- Dependency mode: `EUI_DEPS_MODE` defaults to `auto`, which downloads any dependency missing from `3rd/`. The builds set `EUI_DEPS_MODE=bundled` and `FETCHCONTENT_FULLY_DISCONNECTED=ON`; configuration reported the bundled miniaudio, GLFW, glad, tray, FreeType, zlib, libpng, and MD4C sources (`verified`).
- As a subdirectory, EUI-NEO builds no examples, apps, modules, or install rules unless asked (`EUI_NEO_TOP_LEVEL_DEFAULT`); the smoke builds configure it as the top-level project instead and turn on its examples and tests.
- Generated files go to the build tree: the Windows icon resource, the patched GLFW `x11_window.c` on Linux, and the `assets/` copies next to the executables. Configuring, building, and running the unit tests and the other probes wrote nothing into the snapshot (`find -newer`/`-cnewer` checks, `verified`). Exception: `shadertoy_probe` creates `build-shadertoy/shadertoy/` under `EUI_TEST_SOURCE_DIR`, its compile-time source path (`tests/probes/shadertoy_probe.cpp:516`); one run on 2026-10-11 left two empty directories in the snapshot, which were removed.
- Backends: GLFW and OpenGL by default (`verified`). Vulkan is used when requested or when the build directory name contains `vk` or `vulkan`; SDL2 when it contains `sdl2`.
- Linux: GLFW builds its X11 and Wayland backends by default; the builds set `GLFW_BUILD_WAYLAND=OFF`, as EUI-NEO's Linux release build does. The tray uses gio (`libglib2.0-dev`).
- MinGW: GCC 12 passes EUI-NEO's version check, and its static-runtime probe passes without the UCRT fallback (`verified`). Applications link with `-static` and `-mwindows`; release builds add `-Wl,--gc-sections -s`. Upstream CI builds Windows only with MSVC and vcpkg, so EUI-NEO's MinGW path is not covered upstream.
- MinGW header gap (`verified`): the MinGW-w64 10.0 headers lack `CREATE_WAITABLE_TIMER_HIGH_RESOLUTION`, which `core/app/frame_pacing.h` passes to `CreateWaitableTimerExW`, so every application failed to compile. The toolchain file defines it as `0x00000002`, the Windows SDK value; on Windows versions without high-resolution timers, the call fails and EUI-NEO falls back to a normal waitable timer.
- MinGW test executables: EUI-NEO links its unit tests and probes without its static options, so they import `libstdc++-6.dll`, `libgcc_s_seh-1.dll`, and `libwinpthread-1.dll`. The smoke build adds `CMAKE_EXE_LINKER_FLAGS=-static`, after which every executable imports only Windows system DLLs (`verified` with `objdump -p`).
- Windows imports of `eui_demo.exe` (3.2 MB): `comdlg32`, `GDI32`, `IMM32`, `KERNEL32`, `msvcrt`, `pdh`, `SHELL32`, `urlmon`, `USER32`, `WINMM`. OpenGL is loaded at run time, not imported.
- Compiler flags: `eui_neo_configure_app()` compiles the application target with `-Os -fno-exceptions -fno-rtti` in every configuration except Debug; [CONVENTIONS.md](CONVENTIONS.md#c-conventions) states the consequence for application code.

### 4.3 UI fonts

- The owner provided HarmonyOS Sans SC on 2026-10-11 (round 6) and chose to keep it out of Git. Six weights in `<WSL_ROOT>/fonts/`, version 1.0 per their `name` tables, which read "Copyright 2021 Huawei Device Co., Ltd. All Rights Reserved." and carry no license text. The license terms are `not verified`; the folder holds no license file.
- SHA-256 (`sha256sum`, 2026-10-11): Black `5de9560908e88820df2e0a5ed9465bc44644ae2ce1cd6c194b76f2ed8e8f186e`, Bold `43a424b85e47fb53a17b3b32026a71801f86f8e022ca6798d186b47d39fa5f01`, Light `dd366290b40861bc6ced85801e850ab66d6fe4c5b33bc43095a9747fa29288d8`, Medium `6ed1553edccddc48eb27ff25d134a4a715cf54211238d4840b3038576cba1944`, Regular `297b088424be212207df2ce8b98e335468b782aa6b96832af0b8b773d711e2b1`, Thin `a88d9065dc144bf462e6553e308e9d5256bfe1a068abb47a3ee1081d636ea91a`.
- The UI uses Regular for text and components, Medium for section headings, and Bold for page titles. The build copies those three files to `fonts/` next to the executable (`CRISPGUI_UI_FONTS` in `CMakeLists.txt`), and `tools/package-windows.sh` adds that folder and the files' SHA-256 to the delivery and its `BUILD-INFO.txt`. Without the folder, the build prints a note and the UI uses EUI-NEO's default font.

### 4.4 EUI-NEO runtime facts

Read in the v0.6.1 source and observed in the Phase 2 builds of 2026-10-12:

- Fonts: `DslAppConfig::textFont` replaces the default UI font. EUI-NEO resolves a font path as given, then below `<executable folder>/assets/` by file name, then below the executable's folder (`core/render/text.cpp`, `resolveFontFilePath`), so the application passes paths relative to its folder. A `fontFamily` that contains a dot is taken as such a path. `fontWeight` only selects files for named families and never synthesizes bold, so each weight needs its own file. Missing glyphs fall back to system fonts; on Windows the list includes Segoe UI, Microsoft YaHei, and SimHei.
- Text height: a text element is measured with the wrap width from `TextBuilder::maxWidth`, not its layout width, so wrapped text sets both, or the lines below it overlap (`verified` under WSLg).
- Scroll view: `components::scrollView` composes its content twice per frame, first into a separate `Ui` to measure the height, and passes that `Ui` and the content width to the content function, which must use them. The caller keeps the scroll offset (`offset`, `onChange`).
- JSON: `eui/json.h` reads values by key or index but cannot list an object's members, which profiles need (`env`, unknown keys); the project has its own parser in `src/util/json.{h,cpp}` ([ARCHITECTURE.md §3](ARCHITECTURE.md#3-key-decisions-and-rejected-alternatives)).
- Async: `app::async::restart` runs work on a shared pool of `min(4, hardware threads − 1)` workers (at least one) and queues the completion for the UI thread, which runs it during the next update and recomposes (`core/platform/async.cpp`, `include/eui/detail/dsl_app_impl.h`). A newer task with the same key supersedes a running one, whose completion is dropped.
- Waking the UI: `app::requestUpdate()` sets an atomic flag and posts an empty GLFW event (`core/platform/platform.cpp`), so worker threads may call it; the next update recomposes.
- Shutdown: `DslAppConfig::onShutdown` runs inside `app::shutdown()` while the window still exists (`core/app/glfw_app_main.cpp`), so work done there keeps the last frame on screen.
- Components (Phase 3): a disabled `components::button` looks like an enabled one, so the pages dim disabled buttons; `components::dropdown` reserves the height of its popup in the layout even while closed (`components/dropdown.h`), which is why the profile list carries Start buttons instead; `components::virtualList` composes only the visible rows of a fixed row height (the log pane); `components::dialog` covers the given screen size and is composed as an overlay above the page.

### 4.5 MinGW-w64 runtime facts

From the toolchain's libraries and the Phase 3 Windows run of 2026-10-12 ([Phase 3 report](devlog/phase-3.md#windows-results-owners-machine)):

- The Windows build runs on `msvcrt.dll`. Its `_wstat64` reports a folder path that ends with a separator as missing (`verified` on the owner's machine on 2026-10-12: called from PowerShell, it returns 0 for an existing folder and -1 for the same path with a trailing backslash; `app_data_test` shows the same through `fs::equivalent`).
- MinGW-w64 10 works around that only in `wstat` and `_wstat64i32` (`libmingwex.a`); `_wstat64` is the plain `msvcrt.dll` import (`x86_64-w64-mingw32-nm`, `verified`).
- GCC 12's libstdc++ removes a trailing separator in `std::filesystem::status` (GCC PR 88881), so `exists`, `is_directory`, and `is_regular_file` are safe, but `equivalent` and `file_size` call `_wstat64` with the path as given, and `temp_directory_path` returns `GetTempPathW`'s result, which always ends with a separator (`objdump -dr` of `fs_ops.o` in `libstdc++.a`, `verified`).
- Consequence: paths given to `std::filesystem` carry no trailing separator ([CONVENTIONS.md](CONVENTIONS.md#c-conventions)).

### 4.6 HTTP transport facts

From the Phase 4 code and its Linux runs of 2026-10-12 ([Phase 4 report](devlog/phase-4.md)):

- Timeouts differ by transport (`src/platform/http.h`): WinHTTP applies the request timeout to each send and to the wait for the answer, and the transport sets `WINHTTP_OPTION_RECEIVE_RESPONSE_TIMEOUT` to the same value, so that the wait for the headers of a long transcription has no separate default limit; libcurl limits the whole exchange, connecting included, to the connect timeout plus the request timeout.
- WinHTTP enforces timeouts on a coarse timer. `winhttp_timeouts.exe` of `crisprustgui-diag-winhttp_timeouts-1907626`, run with `--long` on the owner's machine (WinHTTP 6.2.22621.4830, 2026-10-12, `observed`), shows it: counting the cases' durations from the first one's start, every request that timed out ended at about 4, 8, 16, or 20 s, and the two long ones at about 56 s, whatever their limits; requests answered before the next such moment got their answer although their 1000 ms limit had passed (S1, S3, S5, S6, S8, and the transport's own POST). A limit therefore ends a silent wait between about the limit and the limit plus 4 s, occasionally a few milliseconds early (991 ms for 1000 ms). That WinHTTP checks every 4 s is a `hypothesis` from these times. It explains W4-1's `api_client_test` failure: a 3000 ms answer beat the next check.
- The transport's settings take effect. Read back from the request handle before sending, `WinHttpSetTimeouts` on the session plus `WINHTTP_OPTION_RECEIVE_RESPONSE_TIMEOUT` on the request give 1000 ms for the response, receive, send, and connect timeouts (S1). The response timeout reads back as the larger of its own value (90000 ms by default) and the receive timeout, in all 13 cases.
- WinHTTP's defaults end a silent wait for the answer after about 30 s, not after the 90 s of the response timeout: with no settings, an answer due after 95 s failed after 32990 ms (GET) and 32989 ms (POST). The receive timeout bounds the wait for the answer's headers, so long transcriptions need the transport's explicit limits.
- With the transport's configuration and limits of 120 s, the 95 s answer arrived after 95006 ms (L4, and likewise with the session's timeouts alone or with the response timeout set on the session). Long transcriptions therefore work up to the transport's limit, `observed` to 95 s; the default limit of one transcription is 30 min.
- Cancellation: WinHTTP ends a call in progress when another thread closes the request handle: a cancel issued 300 ms into a request returned after 313 ms (`api_client_test`, owner's machine, `observed`). libcurl notices `HttpCancel` in its read callback during an upload and otherwise in its progress callback, which it calls about once per second while it waits for an answer: the same cancel returned after 1202 ms (`api_client_test`, Linux, `observed`).
- Uploads: both transports send `Content-Length` and the body of `platform::MultipartBody`, so the bytes on the wire are the same on Windows and Linux. libcurl is told not to send `Expect: 100-continue`; WinHTTP does not send it.

## 5. Runtime configuration defaults (Windows)

| Setting | Default | Where configured |
|---|---|---|
| CrispASR root (`<CRISPASR_ROOT>`) | `E:\AI\ASR\CrispASR`; the environment variable `CRISPGUI_CRISPASR_ROOT` overrides it, and Linux development runs have no default | GUI settings from Phase 8; until then `src/app/crispasr_location.cpp` |
| Server port | 5114 | Profile and GUI settings |
| API key | `default` | GUI settings |
| Server executable | `<root>\<profile.build>\crispasr.exe` | Derived from the profile |
| Profiles directory | `<root>\profile` | Derived from the root |
| Models directory | `<root>\models` | Derived from the root |
| UI fonts | `<folder of crispgui.exe>\fonts\HarmonyOS_Sans_SC_{Regular,Medium,Bold}.ttf` ([§4.3](#43-ui-fonts)) | Copied by the build; EUI-NEO's default font when absent |
| App data directory | `<folder of crispgui.exe>\appdata`; for Linux development runs `build/linux/appdata` | Derived from the executable's folder (Phase 1); a setting from Phase 8. Never on `C:\`: the dashboard warns when it is |
| Temporary files | `<appdata>\tmp` for the GUI, `<appdata>\tmp\server` as `TMP`/`TEMP` of the server process | Derived from the app data directory |
| Server working directory | `<CRISPASR_ROOT>` | Derived from the root (Phase 3) |
| Health check | `GET /health` every 1.5 s; connect timeout 5 s, request timeout 5 s | `ServerManagerOptions` (Phase 3) |
| Transcription request | 30 min for one request, upload and answer; at most 512 MiB per upload, the server's limit, checked before sending; answers up to 64 MiB | `ApiClientOptions` (Phase 4); the timeout becomes a setting in Phase 8 |
| Transcription window | 300 s (*proposed*, OQ-6) | GUI settings and each job snapshot |

The deployed engine reports itself as `crispasr 0.8.41 (git 340d7085, Release) [backends: cpu,cuda]` in a server log the owner provided on 2026-10-14 (`smoke_5114.log` in `/home/claude/projects/CrispGUI-handoff/scratch`, `observed`), with CUDA devices RTX 5080 and RTX 3080; its `--help` text is `cli_help_full.txt` in the same folder. The project's server contract and CLI flag table were checked against the 0.8.42 source ([API-CRISPASR.md](API-CRISPASR.md)); W4-3 found the deployed build answering as that contract describes.

## 6. Verification entry points

- Project: the Linux and MinGW builds and `ctest` of [§2.2](#22-commands). A Windows delivery carries the same unit and integration tests, the stub server, and `testdata/`; `run-tests.cmd` in the delivery folder runs them and writes `test-results.txt`. The integration tests start the stub on free ports of 127.0.0.1 and write their scratch folders next to the tests.
- Phase 0: the smoke commands of [§2.2](#22-commands).
- Integration tests with the stub server since Phase 3 (`process_test`, `http_test`, `server_manager_test`) and Phase 4 (`api_client_test`, `api_probe_test`).
- The real server: `api_probe` sends the ApiClient's requests to a running CrispASR server, checks the answers, and keeps them with a report in `probe-results/<UTC time>/` next to the executable, or in `--out`. Deliveries carry `api_probe.exe`, its drag-and-drop wrapper `run-api-probe.cmd`, and `CHECKLIST.md`, the numbered Windows checks of the phase with what to send back ([agents/README.md](agents/README.md#delivery-to-windows)).

Each result records its command, working-directory alias, platform, commit, and exit code.
