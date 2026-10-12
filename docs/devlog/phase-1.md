# Phase 1 Report — Application Skeleton

Status: complete. The Linux and MinGW builds pass, and the owner's Windows run of the delivery passed on 2026-10-11 (UTC). Written by A003 on 2026-10-11 (round 5) and closed in round 6. The plan is [TASKS.md Phase 1](../TASKS.md#phase-1--application-skeleton).

## What was built

- `CMakeLists.txt` adds EUI-NEO from `CRISPGUI_EUI_NEO_DIR` as a system subdirectory with the options of [ENVIRONMENT.md §4.2](../ENVIRONMENT.md#42-eui-neo-build-facts) and builds three targets: `crisp_core` (string table, app data, platform seam; no UI dependency), `crisp_ui` (application state and pages on EUI-NEO), and the `crisprustgui` executable. MinGW executables link with `-static`.
- `src/main.cpp` defines EUI-NEO's entry points and owns the single `Application`. The window is titled CrispRustGUI, 1100x720 with a minimum of 800x560, and uses the dark theme.
- `src/app/application.cpp` composes EUI-NEO's `tabs` component over four pages. Dashboard shows the app data and temporary-file folders, or the reason they are unavailable; Transcribe and Batch are placeholders; Settings switches the UI language between English and Chinese without saving it.
- `src/app/app_data.cpp` creates `<folder of the executable>/appdata/tmp` in `onStart` and points `TMP` and `TEMP` (and `TMPDIR` on Linux) at it through `src/platform/paths_win32.cpp` or `paths_posix.cpp`. On Windows it also detects the system drive, and the dashboard warns when the folder is there.
- `src/util/string_table.cpp` holds every UI string with an English and a Chinese text, looked up by key.
- `tests/`: `string_table_test` and `app_data_test`, plain executables on `tests/check.h`.
- `tools/package-windows.sh` packages a Windows delivery of the application with its unit tests; the Phase 0 runner moved to `tools/windows/run-tests.{cmd,ps1}` so both packagers share it.

## Linux results (WSL2, WSLg)

| Check | Command (in `<WSL_ROOT>`) | Result |
|---|---|---|
| Debug build | `cmake -S . -B build/linux -G Ninja -DCMAKE_BUILD_TYPE=Debug && cmake --build build/linux` | exit 0; 159 steps; no warnings |
| Unit tests | `ctest --test-dir build/linux --output-on-failure` | exit 0; 2 of 2 passed |
| The app opens | `build/linux/crisprustgui` under WSLg | an 1100x720 capture shows the four tabs, the Dashboard page, and both folder paths under `build/linux/appdata` |
| App data | `ls build/linux/appdata` after the start | `tmp/` created |
| Clean close | `WM_DELETE_WINDOW` sent to the window through Xlib | the process exited with code 0 and printed nothing |
| MinGW release build | the Windows commands of [ENVIRONMENT.md §2.2](../ENVIRONMENT.md#22-commands) | exit 0; no warnings; `crisprustgui.exe` (3.2 MB) imports only Windows system DLLs; both tests import only `KERNEL32.dll` and `msvcrt.dll` |

Not checked on Linux: switching tabs and the language. Synthetic X11 clicks did not reach EUI-NEO, which presumably reads the real pointer position, and moving the real pointer under WSLg would disturb the owner's desktop. The Windows run covers both.

## Delivery

Both builds were rebuilt from a clean tree at commit `1817b8ee4cdf82244b30fca50a349ac9815e2a04` with the results above, and `tools/package-windows.sh` produced `crisprustgui-1817b8e.zip` in `<HANDOFF_OUT>`: 6,626,850 bytes, SHA-256 `dd5fe3845e9ec4f38ceb18f01dbe183c1c6cea6befbd7515c9289408addfd6a8`, `unzip -tq` without errors. It holds `crisprustgui.exe`, `assets/`, the two unit tests, `tests.txt`, `run-tests.cmd`/`.ps1`, and `BUILD-INFO.txt` (12 MB unpacked).

## Deviations from the plan

- No empty placeholder modules: the layout of ARCHITECTURE §2 is created module by module as phases add code.
- The Settings page already holds the language switch, so the Chinese strings are visible before Phase 8; the choice is not saved yet.
- The app data folder is derived from the executable's folder; for Linux development runs that is `build/linux/appdata` instead of `<WSL_ROOT>/appdata` as ENVIRONMENT first planned.
- The project requires CMake 3.25 instead of 3.14, for `add_subdirectory(... SYSTEM)`.
- EUI-NEO's default UI font is its bundled bold display font `JingNanJunJunTi`. Phase 1 keeps it. In round 6 the owner provided HarmonyOS Sans SC, which Phase 2 adopts ([Phase 2 report](phase-2.md)).

## Windows results (owner's machine)

The owner ran the delivery `crisprustgui-1817b8e` on 2026-10-11 UTC and reported that the Windows tests passed with no problems (owner's statement in round 6). `test-results.txt` was not sent back, so per-test exit codes are not recorded for this run.

Note added in round 8: the Phase 3 run showed that `app_data_test` fails on every Windows run because of a defect in the test since Phase 1 ([Phase 3 report](phase-3.md#windows-results-owners-machine)), so it most likely failed in this run too; without `test-results.txt` the run's record cannot show it. The application code was not affected.

## Acceptance

Phase 1 is accepted: the Linux results above and the owner's Windows run come from the same commit, `1817b8ee4cdf82244b30fca50a349ac9815e2a04`.
