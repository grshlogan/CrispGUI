# Phase 0 Report — Build Environment

Status: complete. The Linux and MinGW smoke builds pass, and the owner's Windows run of the first delivery passed on 2026-10-12. Written by A003 on 2026-10-11 (round 4) and closed in round 5. The plan is [TASKS.md Phase 0](../TASKS.md#phase-0--build-environment-no-application-code); environment facts are in [ENVIRONMENT.md](../ENVIRONMENT.md).

## What was built

- `tools/smoke/build-eui-smoke.sh` configures and builds EUI-NEO v0.6.1 from the shared Learn library, with its examples and tests, into `build/eui-smoke-linux` (GCC 12, X11, OpenGL) and `build/eui-smoke-mingw` (MinGW-w64 GCC 12, static). A clean build of both takes 49 s on this machine and prints no compiler warnings.
- `cmake/toolchains/mingw-w64-x86_64.cmake`, the cross-compilation toolchain file, which also fills a gap in the MinGW-w64 10.0 headers ([ENVIRONMENT.md §4.2](../ENVIRONMENT.md#42-eui-neo-build-facts)).
- `tools/smoke/package-eui-smoke.sh` packages the MinGW build as a Windows delivery: `eui_demo.exe` and `multipage_table.exe` for a visual check, plus EUI-NEO's unit tests and probes. `run-eui-smoke.cmd` and `run-eui-smoke.ps1` run them on Windows and record each exit code in `smoke-results.txt`.

## Linux results (WSL2, WSLg)

Both builds were rebuilt from a clean tree at commit `7e4e89eb7e021ea61bcb432aadcb3883c22e3479`, the commit of the first delivery, with the same results as below.

| Check | Command (in `<WSL_ROOT>`) | Result |
|---|---|---|
| Smoke builds | `tools/smoke/build-eui-smoke.sh all` | exit 0; 276 Linux and 295 MinGW build steps |
| Unit tests | `ctest --test-dir build/eui-smoke-linux -L unit` | exit 0; 26 of 26 passed |
| Rendering probes | `ctest --test-dir build/eui-smoke-linux -L probe -E '^shadertoy_probe$'` | exit 8; 7 of 8 passed, `frame_presentation_probe` failed (below) |
| `eui_demo` under WSLg | `timeout 12 build/eui-smoke-linux/eui_demo` | exit 124 (still running when stopped); no output; an 800x600 capture of its window shows the Chinese title and the "点我" button rendered |
| Learn snapshot untouched | `find <Learn> -newer <marker>` and `-cnewer <marker>` around the builds and test runs | no entries, except the `shadertoy_probe` incident below |
| MinGW executables | `x86_64-w64-mingw32-objdump -p` on all 55 `.exe` files | Windows system DLLs only |

`frame_presentation_probe` fails on every run with "window mapped before a frame at its current size was drawn" and "presented old-size frame after resize during update". The probe resizes a hidden window and expects the framebuffer to report the new size at once. The same probe passes on Windows (below), so the failure is specific to WSLg; the likely cause is WSLg's window manager applying the resize asynchronously, while upstream CI runs the probes under `xvfb-run` without a window manager (`hypothesis`).

## Deviations from the plan

- Shim in the toolchain file: the MinGW-w64 10.0 headers lack `CREATE_WAITABLE_TIMER_HIGH_RESOLUTION`, so the toolchain file defines it. The Learn snapshot stays unmodified.
- `-static` for all executables: EUI-NEO links its test executables dynamically against the MinGW runtime, so the MinGW smoke build adds `CMAKE_EXE_LINKER_FLAGS=-static`.
- `shadertoy_probe` excluded: it writes under its compile-time source path, the Learn snapshot. The first probe run left `build-shadertoy/shadertoy/` (two empty directories) there; A003 removed them, and the snapshot again matches the tag archive (`diff -rq`, exit 0). The probe is excluded from Linux runs and from the Windows delivery, where that path does not exist.
- The Learn library was tidied at the owner's request: the EUI-NEO snapshot moved to `sources/github.com/sudoevolve/EUI-NEO/v0.6.1`, its tag archive was downloaded and verified, and the CrispASR v0.8.42 archive was renamed, extracted, and indexed (Learn `README.md`, 2026-10-11 record).

## First delivery

`tools/smoke/package-eui-smoke.sh` at `7e4e89e` produced `crisprustgui-eui-smoke-7e4e89e.zip` in `<HANDOFF_OUT>`: 45,685,746 bytes, SHA-256 `fd35436b51a35e714d497178156561ba7f6335c7eefe1d343a61f4f5ba91ad5b`, `unzip -tq` without errors. It holds 36 executables (130 MB unpacked), `assets/`, `BUILD-INFO.txt`, `smoke-tests.txt`, and the two run scripts.

## Windows results (owner's machine)

The owner ran the first delivery on 2026-10-12 (`smoke-results.txt`, started 2026-10-12T02:34:09+08:00):

- System: Windows 11 Pro for Workstations 10.0.22631. Display adapters: NVIDIA GeForce RTX 3080 and RTX 5080 (driver 32.0.16.1062), plus the virtual adapters of Oray, ToDesk, GameViewer, and MuMu.
- Visual check: `eui_demo.exe` and `multipage_table.exe` showed no problems (owner's report).
- `run-eui-smoke.cmd`: 25 unit tests passed and `platform_dialog` was skipped (exit 77, its designed result outside Linux); all 8 probes passed, `frame_presentation_probe` included. Failures: 0 of 34.

## Acceptance

Phase 0 is accepted: the Linux and the Windows builds come from the same commit, `7e4e89eb7e021ea61bcb432aadcb3883c22e3479`, and both results are recorded above with their platforms.
