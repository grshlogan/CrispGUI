# Phase 3 Report — ServerManager

Status: built and tested on Linux; the owner's Windows run of the delivery is pending. Written by A003 on 2026-10-12 (round 7; dates are UTC). The plan is [TASKS.md Phase 3](../TASKS.md#phase-3--servermanager); the design is [ARCHITECTURE.md §2.2](../ARCHITECTURE.md#22-servermanager).

## What was built

- `src/core/launch_command.{h,cpp}`: `BuildLaunchCommand`, the launcher's whole command line and environment for a profile, plus `TMP` and `TEMP` pointed at `<appdata>/tmp/server` and the CrispASR folder as the working directory. It refuses a missing executable or model and starts without a missing aligner, with a warning (OQ-8, recommendation (a)), and it finds the API key the GUI's own requests need.
- `src/core/server_state.{h,cpp}`: the server's lifecycle as a state machine without threads or I/O: the check at launch, the pre-flight, start, adoption, unreachable adopted servers, stop, and failure, with epochs that drop stale reports; and the reading of `/health` and `/v1/models` answers.
- `src/core/server_manager.{h,cpp}`: the state machine with its worker threads: a poller for `/health` and `/v1/models`, a controller that starts and ends processes, and a monitor per process that forwards output lines and reports the exit, all through one inbox that the UI thread applies in `Compose`.
- `src/core/server_log.{h,cpp}`: `LogSplitter` (lines, carriage-return redraws, escape sequences, control characters, invalid UTF-8, long lines) and the 5000-line `LogBuffer`.
- `src/platform/process.{h,cpp}` with `process_win32.cpp` (`CreateProcessW` with a handle list, `CREATE_NO_WINDOW`, a Unicode environment block, a peeking pipe reader, `GetExtendedTcpTable`, `TerminateProcess`) and `process_posix.cpp` (`posix_spawn`, `poll`, `/proc/net/tcp`, SIGTERM and SIGKILL); `src/platform/command_line.{h,cpp}` with Windows argument quoting and environment changes as plain string functions; `src/platform/http.{h,cpp}` with `http_win32.cpp` (WinHTTP) and `http_curl.cpp` (libcurl): GET without proxies, with a body limit and cancellation; `src/platform/win32_text.{h,cpp}`.
- The dashboard: a server panel (state, profile, address and process, backend and model, warnings, the status message with its detail, the port's state while stopped, and Stop), a Start button for every profile with an "In use" mark, the server log below the scrolling area, and a confirmation before stopping an adopted server. 42 new string table entries in English and Chinese. Disabled buttons are dimmed.
- `CRISPGUI_DEV_AUTOSTART=<folder>/<profile>` starts that profile once the first scan and the check at launch are done, so development runs can exercise a start without clicks, which do not reach EUI-NEO under WSLg.
- Tests: `command_line_test`, `server_log_test`, `server_state_test`, and more cases in `launch_command_test` (unit); `process_test`, `http_test`, and `server_manager_test` (integration), which start `stub_crispasr` (`tests/stub_crispasr.cpp`), a stand-in that takes the server's command line, prints what it received, waits as if loading a model, and answers `/health` and `/v1/models` like CrispASR 0.8.42, or exits with code 0, or answers like another program. Deliveries carry the integration tests and the stub, and `run-tests.ps1` knows the `integration` kind.

## Linux results (WSL2, WSLg)

| Check | Command (in `<WSL_ROOT>`) | Result |
|---|---|---|
| Clean Debug build | `rm -rf build/linux && cmake -S . -B build/linux -G Ninja -DCMAKE_BUILD_TYPE=Debug && cmake --build build/linux` | exit 0; 206 steps; no warnings |
| Tests | `ctest --test-dir build/linux --output-on-failure` | exit 0; 14 of 14 passed (11 unit, 3 integration) |
| Integration tests repeated | `ctest --test-dir build/linux -L integration --repeat until-fail:20` | exit 0; 20 rounds in 52.8 s; afterwards no stub process runs (`ps`) |
| The tests catch changes | four deliberate faults, each built and tested, then the file restored (`cmp` against a copy) | trailing backslashes not doubled: `command_line_test` exit 1 (3 failed checks); a pre-flight that starts despite another program: `server_state_test` exit 1 (5) and `server_manager_test` exit 1 (3); `TMP` before the profile's `env`: `launch_command_test` exit 1 (1); standard error not redirected: `process_test` exit 1 (8) and `server_manager_test` exit 1 (7) |
| A start through the UI path | `CRISPGUI_CRISPASR_ROOT=<scratch>/p3-root CRISPGUI_DEV_AUTOSTART=Japanese/cohere-asr-ja-q8 build/linux/crisprustgui`, with the stub as `cuda13/crispasr`, the reference profiles on port 51141, and 4 s of model load; captures after 2 s and 8 s | after 2 s "Starting: loading the model…" with the profile, `http://127.0.0.1:51141`, and the process ID; after 8 s "Running" with backend `cohere` and the model path from `/v1/models`, read with the key `default`; the log pane follows the stub's output to its "listening" line |
| The server outlives the GUI | the same run, closed with `WM_DELETE_WINDOW` | the GUI exits with code 0; the stub still runs and answers `/health` with `{"status": "ok", "backend": "cohere"}`; ended by hand |
| A failed start | `CRISPGUI_DEV_AUTOSTART=Japanese/exits-early` (a cohere copy whose `env` makes the stub exit with code 0 after 0.5 s), window resized to 1100x1040 | "Failed", "The server exited before it answered. …", "Details: exit code 0", Stop dimmed; the broken profile's Start dimmed; the log shows what the server received: `TMP`, `TEMP`, and `TMPDIR` at `build/linux/appdata/tmp/server`, the `CRISPASR_*` folders below the root, `CRISPASR_API_KEYS=default`, `CUDA_VISIBLE_DEVICES=1`, the root as working directory, and the GUI's note "The server process exited with exit code 0." |
| Clean MinGW release build | the Windows commands of [ENVIRONMENT.md §2.2](../ENVIRONMENT.md#22-commands) after `rm -rf build/mingw` | exit 0; 207 steps; no warnings; `crisprustgui.exe` (3,970,048 bytes) imports only Windows system DLLs, now including `WINHTTP.dll` and `IPHLPAPI.DLL`; unit tests import `KERNEL32.dll` and `msvcrt.dll`, the integration tests also `WS2_32.dll`, `WINHTTP.dll`, and `IPHLPAPI.DLL`, the stub `KERNEL32.dll`, `msvcrt.dll`, and `WS2_32.dll` |
| Learn snapshot untouched | `find <Learn> -newer <marker>` and `-cnewer <marker>` around the clean builds | no entries |
| No contact with the Windows side | `wslinfo --networking-mode` | `nat`: 127.0.0.1 in WSL2 is not the Windows loopback; the GUI runs used port 51141 and the tests free ports, never the deployed 5114 with a server behind it |

Not checked on Linux:

- Clicks: Start and Stop buttons, the stop confirmation, scrolling the log, and the Chinese texts of the new section (the string table test checks that every key has both texts). Adoption and the stop of an adopted server are covered by `server_manager_test`, not by a capture.
- Every Win32 path is compiled and linked but has not run: `CreateProcessW` with the handle list, the pipe reader, the environment block, WinHTTP, `GetExtendedTcpTable`, and `TerminateProcess`. The delivery's integration tests run them against the stub on the owner's machine.
- The real CrispASR server.

## Decisions and deviations

- States: SPEC FR-1's `stopped`, `starting`, `running`, `stopping`, and `failed`, plus `checking` (the check at launch and the pre-flight) and `unreachable` (an adopted server that stopped answering, as SPEC FR-1 describes it).
- The pre-flight treats any answer that is not CrispASR's `/health` as another program, also a timeout; a CrispASR answer adopts the running server instead of starting a second one.
- Stopping an adopted server ends the process listening on the port only when its executable is `crispasr.exe`, after a confirmation; the check and the termination use the same process handle on Windows.
- Standard output and standard error share one pipe, which keeps the order of the lines.
- The working directory of the server is the CrispASR folder; the launcher uses its console's directory, which the GUI cannot know.
- Closing the GUI leaves its server running ([API-CRISPASR.md §6](../API-CRISPASR.md#6-lifecycle-and-resource-behavior), item 10); the next launch adopts it when it uses port 5114.
- OQ-8 stays open; the implementation follows its recommendation (a), like the launcher.
- Profile selection: each profile in the dashboard's folder groups has a Start button, instead of a dropdown, because EUI-NEO's dropdown reserves its popup's height in the layout ([ENVIRONMENT.md §4.4](../ENVIRONMENT.md#44-eui-neo-runtime-facts)). The folders and their profiles still form the launcher's two levels.
- The poller posts only changed results; the pre-flight and the check after a process starts always report, so a server that answered before its start report arrived still turns `running`.
- A log file and the cleanup of `<appdata>/tmp/server` after crashes are not part of this phase (Phase 8).

## Open items

- [Windows] The owner runs the delivery: `run-tests.cmd` (11 unit and 3 integration tests; `test-results.txt` back to A003), then `crisprustgui.exe` with the cohere profile: Start → "Running" with backend and model, server output in the log pane, Stop → "Stopped"; optionally a server started with the launcher first, which the GUI adopts and, after the confirmation, stops.
- The claim that Windows refuses a connection to a closed local port only after about 2 s is a `hypothesis` behind the 5 s connect timeout.
- Until Settings (Phase 8), the check at launch uses port 5114.
