# Build Environment

> How CrispRustGUI is developed and built. Revised on 2026-10-11 (round 3): the Windows executable is cross-compiled in WSL2 with MinGW-w64, and EUI-NEO comes from the shared Learn library. All development happens in WSL2, WSL2 never mounts Windows drives, and the Windows side does no development. Roles and delivery to Windows are in [agents/README.md](agents/README.md).

## 1. Target and toolchains

- **Target**: a Windows 10/11 x64 desktop application. C++17, CMake ≥ 3.14 (EUI-NEO's minimum), Ninja.
- **Windows executable**: cross-compiled in WSL2 with MinGW-w64 GCC 12 and the POSIX thread model (owner decision of 2026-10-11, formerly OQ-1). The result is a statically linked executable plus EUI-NEO's `assets/` folder, so a delivery is one folder ([§4.2](#42-eui-neo-build-facts)).
- **Development builds**: Linux, in the WSL2 distribution `claude-debian12`, rendering through WSLg with GLFW's X11 backend ([§4.2](#42-eui-neo-build-facts)). WSLg is available (`DISPLAY` and `WAYLAND_DISPLAY` are set) but untested with EUI-NEO.
- **UI framework**: EUI-NEO v0.6.1 from the shared Learn library ([§4.1](#41-eui-neo-snapshot)), added with `add_subdirectory` into this project's build tree.
- **HTTP**: the in-repo transport of [ARCHITECTURE.md §2.3](ARCHITECTURE.md#23-apiclient-and-http-transport): WinHTTP on Windows and libcurl on Linux (*proposed*). EUI-NEO itself requires libcurl on Linux.

## 2. Development loop

1. The agent edits, builds, and tests in `<WSL_ROOT>`. Linux builds cover compilation, unit tests, HTTP tests against a local stub server, and UI behavior under WSLg; MinGW builds cover compiling and linking for Windows. These results are platform-limited.
2. Windows-native behavior — window composition, DPI, IME, file locking, the real engine, the GPU — is accepted only on the owner's Windows machine, using a delivery bound to an exact commit ([delivery to Windows](agents/README.md#delivery-to-windows)).
3. Windows drives are never mounted or accessed from WSL2 ([AGENTS.md](../AGENTS.md#environment-boundaries)): no `/mnt/<drive>` paths and no interop calls to Windows executables.
4. Build output stays in `<WSL_ROOT>/build*/`, which Git ignores.

### 2.1 WSL2 toolchain

Checked on 2026-10-11 in `<WSL_ROOT>` (Linux 6.18.40.1-microsoft-standard-WSL2) with `command -v`, `--version`, `dpkg-query`, and `apt-cache policy`:

| Tool | State |
|---|---|
| MinGW-w64 | Installed by the owner: `g++-mingw-w64-x86-64-posix` and `gcc-mingw-w64-x86-64-posix` 12.2.0-14+deb12u1+25.2+b1, `binutils-mingw-w64-x86-64` 2.40-2+10.4, `mingw-w64-x86-64-dev` 10.0.0-3. `x86_64-w64-mingw32-g++` resolves to the POSIX-thread variant; the win32-thread variant is not installed |
| `gcc` | 12.2.0 |
| `make`, `pkg-config`, `python3`, `git` | GNU Make 4.3, pkg-config 1.8.1, Python 3.11.2, Git 2.39.5 |
| `g++`, `cmake`, `ninja`, Linux development packages | Missing; the owner installs them from the list below |

MinGW probe (`verified`, 2026-10-11, built in the session scratchpad outside the repository): EUI-NEO's own static-runtime test program (its `CMakeLists.txt`, lines 62-96) compiled with `x86_64-w64-mingw32-g++ -std=c++17 -O2 -static` (exit 0) into a PE32+ x86-64 executable whose only DLL imports are `KERNEL32.dll` and `msvcrt.dll`. The same program also linked with `-mwindows` against `opengl32 winmm urlmon shell32 user32 imm32 pdh gdi32` (exit 0). EUI-NEO's static MinGW link mode is therefore available, and the C runtime is `msvcrt.dll`.

Packages for the owner to install (Debian 12 names; candidate versions from `apt-cache policy` on 2026-10-11):

| Package | Candidate | Needed for |
|---|---|---|
| `g++` | 12.2.0 | Linux C++ compiler |
| `cmake` | 3.25.1 | Build system for both builds (EUI-NEO needs ≥ 3.14) |
| `ninja-build` | 1.11.1 | Build generator for both builds |
| `libgl1-mesa-dev` | 22.3.6 | OpenGL headers and libraries; EUI-NEO's OpenGL backend stops configuration without them |
| `libx11-dev`, `libxrandr-dev`, `libxinerama-dev`, `libxcursor-dev`, `libxi-dev`, `libxext-dev` | 1.8.4, 1.5.2, 1.1.4, 1.2.1, 1.8, 1.3.4 | GLFW 3.4's X11 backend, which stops configuration when any of them is missing |
| `libcurl4-openssl-dev` | 7.88.1 | EUI-NEO on Linux (`find_package(CURL REQUIRED)`) and the Linux HTTP transport |
| `libglib2.0-dev` | 2.74.6 | EUI-NEO's Linux tray backend; without it, configure with `-DEUI_ENABLE_TRAY=OFF` |
| `gdb` (optional) | 13.1 | Debugging Linux builds |
| `ccache` (optional) | 4.7.5 | Faster rebuilds |

```bash
sudo apt update
sudo apt install g++ cmake ninja-build libgl1-mesa-dev libx11-dev libxrandr-dev libxinerama-dev \
  libxcursor-dev libxi-dev libxext-dev libcurl4-openssl-dev libglib2.0-dev
sudo apt install gdb ccache   # optional
```

Not needed: the Wayland development packages, because the Linux build uses GLFW's X11 backend only, as EUI-NEO's own Linux release build does (`.github/workflows/release.yml`); Vulkan and SDL2, because the backends are OpenGL and GLFW; `wine64`, until a test has to run a Windows executable inside WSL2.

### 2.2 Commands

`planned`; these can run once the packages above are installed and Phase 0 and Phase 1 add the build files:

```bash
# Linux development build (WSLg)
cmake -S . -B build/linux -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build/linux
ctest --test-dir build/linux --output-on-failure

# Windows build (MinGW-w64 cross-compilation)
cmake -S . -B build/mingw -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/mingw-w64-x86_64.cmake
cmake --build build/mingw
```

The project's `CMakeLists.txt` sets the EUI-NEO options of [§4.2](#42-eui-neo-build-facts), so these commands need no extra flags. Build directory names must not contain the segments `vk`, `vulkan`, or `sdl2`, because EUI-NEO picks its backends from them.

## 3. Repository layout policy

- EUI-NEO is not copied into the repository. The build reads it from the path in `CRISPGUI_EUI_NEO_DIR` ([§4.1](#41-eui-neo-snapshot)) and writes everything it generates into the build tree.
- `cmake/toolchains/mingw-w64-x86_64.cmake`: the MinGW-w64 toolchain file (*planned*, Phase 0).
- `params/`: the parameter catalog and model packs ([ARCHITECTURE.md §2.7](ARCHITECTURE.md#27-parameter-model-pluggable)); data, committed.
- `build*/`, `appdata/`, `CMakeUserPresets.json`, and `.cache/` are ignored by Git (`.gitignore`).
- Line endings are LF (`.gitattributes`), except under `reference/`, which Git stores byte for byte.
- `reference/`: read-only learning material, never part of the build.
- Deliveries are written outside the checkout, to `<HANDOFF_OUT>`.

## 4. Dependencies

| Dependency | Source | Notes |
|---|---|---|
| EUI-NEO v0.6.1 | Shared Learn library ([§4.1](#41-eui-neo-snapshot)) | UI framework, Apache-2.0; bundles GLFW 3.4, glad, FreeType, zlib 1.3.1, libpng 1.6.43, md4c, miniaudio, nanosvg, stb_image, a tray library, and yyjson 0.12.0 |
| WinHTTP | Windows system library | Windows HTTP transport (*proposed*) |
| libcurl | Debian package, Linux builds only | Required by EUI-NEO on Linux; also the Linux HTTP transport |
| Compilers, CMake, Ninja | [§2.1](#21-wsl2-toolchain) | MinGW-w64 installed; the Linux toolchain pending |
| CrispASR server | Deployed on Windows at `<CRISPASR_ROOT>\cuda13\crispasr.exe` | Runtime dependency only; never built here |
| Models | `<CRISPASR_ROOT>\models\` on Windows | Runtime data only |

### 4.1 EUI-NEO snapshot

- Location: `/home/claude/projects/Learn/sources/github.com/sudoevolve/EUI-NEO-v0.6.1`, which is `../Learn/sources/github.com/sudoevolve/EUI-NEO-v0.6.1` relative to `<WSL_ROOT>`. The build takes it from the CMake cache variable `CRISPGUI_EUI_NEO_DIR`, whose default is that relative path (*planned*).
- Provenance, from the Learn library index: tag `v0.6.1` of `sudoevolve/EUI-NEO`, commit `1647709068e36834ab26da5a97ef4ad98aa34c9d` (2026-10-10), Apache-2.0, 42M in 1428 files. Checked here on 2026-10-11: `project(EUI-NEO VERSION 0.6.1)` in its `CMakeLists.txt`, 42M and 1428 files (`du -sh`, `find -type f | wc -l`), and an Apache-2.0 `LICENSE`. The release archive is not in the Learn library's `.downloads/`, so its digest cannot be rechecked locally.
- Read-only: never modified, patched, or built inside its own directory.

### 4.2 EUI-NEO build facts

Read on 2026-10-11 from EUI-NEO v0.6.1's `CMakeLists.txt`, `3rd/dependencies.cmake`, and `.github/workflows/`. The first configure run will confirm them (`not run`).

- Dependency mode: `EUI_DEPS_MODE` defaults to `auto`, which downloads any dependency missing from `3rd/`. This project sets `EUI_DEPS_MODE=bundled`, so a missing dependency stops configuration instead of downloading, and sets `FETCHCONTENT_FULLY_DISCONNECTED=ON` as a second guard.
- As a subdirectory, EUI-NEO builds no examples, apps, modules, or install rules unless asked (`EUI_NEO_TOP_LEVEL_DEFAULT`).
- Generated files go to the build tree: the Windows icon resource (`eui_app_icon.rc`) and, on Linux, a patched copy of GLFW's `x11_window.c`. `eui_neo_configure_app()` copies `assets/` next to the executable after each build.
- Backends: GLFW and OpenGL by default. Vulkan is used when requested or when the build directory name contains `vk` or `vulkan`; SDL2 when it contains `sdl2`.
- Linux: GLFW builds its X11 and Wayland backends by default; this project sets `GLFW_BUILD_WAYLAND=OFF`, as EUI-NEO's Linux release build does. The tray needs glib/gio unless `EUI_ENABLE_TRAY=OFF`.
- MinGW: GCC 12 or newer is required. Applications link with `-static` (falling back to `-mcrtdll=ucrt` if the probe fails) and `-mwindows`; release builds add `-Wl,--gc-sections -s`. Upstream CI builds Windows only with MSVC and vcpkg, so EUI-NEO's MinGW path is not covered by upstream CI.
- Compiler flags: `eui_neo_configure_app()` compiles the application target with `-Os -fno-exceptions -fno-rtti` in every configuration except Debug; [CONVENTIONS.md](CONVENTIONS.md#c-conventions) states the consequence for application code.
- Windows libraries linked by `eui_neo`: `winmm urlmon shell32 user32 imm32 pdh`, plus OpenGL.

## 5. Runtime configuration defaults (Windows)

| Setting | Default | Where configured |
|---|---|---|
| CrispASR root (`<CRISPASR_ROOT>`) | `E:\AI\ASR\CrispASR` | GUI settings |
| Server port | 5114 | Profile and GUI settings |
| API key | `default` | GUI settings |
| Server executable | `<root>\<profile.build>\crispasr.exe` | Derived from the profile |
| Profiles directory | `<root>\profile` | Derived from the root |
| Models directory | `<root>\models` | Derived from the root |
| App data directory | `<app folder>\appdata`; for development runs `<WSL_ROOT>/appdata` | GUI settings; never on `C:\` |
| Temporary files | `<appdata>\tmp` for the GUI, `<appdata>\tmp\server` as `TMP`/`TEMP` of the server process | Derived from the app data directory |
| Transcription window | 300 s (*proposed*, OQ-6) | GUI settings and each job snapshot |

## 6. Verification entry points

None is configured yet. Planned gates after Phase 0: the Linux build, the MinGW build, unit tests through `ctest`, HTTP tests against the stub server, and a Windows smoke run by the owner. Each result records its command, working-directory alias, platform, commit, and exit code.
