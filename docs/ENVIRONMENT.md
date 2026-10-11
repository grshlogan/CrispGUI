# Build Environment

> How CrispRustGUI is developed and built. Revised on 2026-10-11 (round 4) after the Phase 0 smoke builds. The Windows executable is cross-compiled in WSL2 with MinGW-w64, and EUI-NEO comes from the shared Learn library. All development happens in WSL2, WSL2 never mounts Windows drives, and the Windows side does no development. Roles and delivery to Windows are in [agents/README.md](agents/README.md).

## 1. Target and toolchains

- **Target**: a Windows 10/11 x64 desktop application. C++17, CMake ≥ 3.25 (the project uses `add_subdirectory(... SYSTEM)`; EUI-NEO itself needs 3.14), Ninja.
- **Windows executable**: cross-compiled in WSL2 with MinGW-w64 GCC 12 and the POSIX thread model (owner decision of 2026-10-11, formerly OQ-1). The result is a statically linked executable plus EUI-NEO's `assets/` folder, so a delivery is one folder ([§4.2](#42-eui-neo-build-facts)).
- **Development builds**: Linux, in the WSL2 distribution `claude-debian12`, rendering through WSLg with GLFW's X11 backend. EUI-NEO's `eui_demo` renders correctly under WSLg ([Phase 0 report](devlog/phase-0.md)).
- **UI framework**: EUI-NEO v0.6.1 from the shared Learn library ([§4.1](#41-eui-neo-snapshot)), built into this project's build tree.
- **HTTP**: the in-repo transport of [ARCHITECTURE.md §2.3](ARCHITECTURE.md#23-apiclient-and-http-transport): WinHTTP on Windows and libcurl on Linux (*proposed*). EUI-NEO itself requires libcurl on Linux.

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

Project builds (`verified` on 2026-10-11; results in the [Phase 1 report](devlog/phase-1.md)):

```bash
# Linux development build (WSLg); the app is build/linux/crisprustgui
cmake -S . -B build/linux -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build/linux
ctest --test-dir build/linux --output-on-failure

# Windows release build (MinGW-w64 cross-compilation) and its delivery
cmake -S . -B build/mingw -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/mingw-w64-x86_64.cmake
cmake --build build/mingw
tools/package-windows.sh               # needs a clean tree on origin/main
```

The top-level `CMakeLists.txt` sets the EUI-NEO options of [§4.2](#42-eui-neo-build-facts) and links every MinGW executable with `-static`, so these commands need no extra flags; `-DCRISPGUI_EUI_NEO_DIR=<path>` points at another EUI-NEO snapshot. Build directory names must not contain the segments `vk`, `vulkan`, or `sdl2`, because EUI-NEO picks its backends from them.

## 3. Repository layout policy

- EUI-NEO is not copied into the repository. Builds read it from the path in `CRISPGUI_EUI_NEO_DIR` ([§4.1](#41-eui-neo-snapshot)) and write what they generate into the build tree.
- `cmake/toolchains/mingw-w64-x86_64.cmake`: the MinGW-w64 toolchain file.
- `CMakeLists.txt`, `src/`, `tests/`: the application, its two libraries (`crisp_core` without UI dependencies, `crisp_ui` on EUI-NEO), and the unit tests.
- `tools/package-windows.sh` packages a Windows delivery of the application; `tools/windows/` holds the test runner that deliveries carry; `tools/smoke/` holds the Phase 0 smoke build and its packaging.
- `params/`: the parameter catalog and model packs ([ARCHITECTURE.md §2.7](ARCHITECTURE.md#27-parameter-model-pluggable)); data, committed.
- `build*/`, `appdata/`, `CMakeUserPresets.json`, and `.cache/` are ignored by Git (`.gitignore`).
- Line endings are LF (`.gitattributes`), except CRLF for `*.cmd` and `*.bat`, and except `reference/`, which Git stores byte for byte.
- `reference/`: read-only learning material, never part of the build.
- Deliveries are written outside the checkout, to `<HANDOFF_OUT>`.

## 4. Dependencies

| Dependency | Source | Notes |
|---|---|---|
| EUI-NEO v0.6.1 | Shared Learn library ([§4.1](#41-eui-neo-snapshot)) | UI framework, Apache-2.0; bundles GLFW 3.4, glad, FreeType, zlib 1.3.1, libpng 1.6.43, md4c, miniaudio, nanosvg, stb_image, a tray library, and yyjson 0.12.0 |
| WinHTTP | Windows system library | Windows HTTP transport (*proposed*) |
| libcurl | Debian package, Linux builds only | Required by EUI-NEO on Linux; also the Linux HTTP transport |
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

## 5. Runtime configuration defaults (Windows)

| Setting | Default | Where configured |
|---|---|---|
| CrispASR root (`<CRISPASR_ROOT>`) | `E:\AI\ASR\CrispASR` | GUI settings |
| Server port | 5114 | Profile and GUI settings |
| API key | `default` | GUI settings |
| Server executable | `<root>\<profile.build>\crispasr.exe` | Derived from the profile |
| Profiles directory | `<root>\profile` | Derived from the root |
| Models directory | `<root>\models` | Derived from the root |
| App data directory | `<folder of crisprustgui.exe>\appdata`; for Linux development runs `build/linux/appdata` | Derived from the executable's folder (Phase 1); a setting from Phase 8. Never on `C:\`: the dashboard warns when it is |
| Temporary files | `<appdata>\tmp` for the GUI, `<appdata>\tmp\server` as `TMP`/`TEMP` of the server process | Derived from the app data directory |
| Transcription window | 300 s (*proposed*, OQ-6) | GUI settings and each job snapshot |

## 6. Verification entry points

- Project: the Linux and MinGW builds and `ctest` of [§2.2](#22-commands). A Windows delivery carries the same unit tests; `run-tests.cmd` in the delivery folder runs them and writes `test-results.txt`.
- Phase 0: the smoke commands of [§2.2](#22-commands).
- Planned: HTTP tests against a stub server (Phase 4).

Each result records its command, working-directory alias, platform, commit, and exit code.
