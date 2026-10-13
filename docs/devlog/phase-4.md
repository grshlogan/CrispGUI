# Phase 4 Report — ApiClient

Status: implemented; the Linux acceptance passes. On Windows, with `crisprustgui-c296f23`: W4-1 passed 19 of 20 tests, the failing one allowing too little time for WinHTTP's coarse timer ([P4-T1](../agents/tasks/P4-T1-timeout-test/packet.md)); W4-2 passed by the owner's statement; W4-3 passed; W4-4 passed by the owner's statement. The phase closes when the next delivery's W4-1 has no failures. Written by A003 on 2026-10-12 (rounds 10 and 11; dates are UTC). The plan is [TASKS.md Phase 4](../TASKS.md#phase-4--apiclient); the design is [ARCHITECTURE.md §2.3](../ARCHITECTURE.md#23-apiclient-and-http-transport). On 2026-10-12 the owner agreed to two additions: a numbered checklist in every Windows delivery, and an entry for the Windows-side agent in the identity register.

## What was built

- `src/platform/http.{h,cpp}` with `http_win32.cpp` and `http_curl.cpp`: `HttpPost`, which pulls the request body from an `HttpBody` in pieces, next to `HttpGet`; two new failure kinds, `kTooLarge` (the answer passes the size limit, formerly `kOther`) and `kBody` (the body cannot be produced). WinHTTP writes the body with `WinHttpWriteData` and sets `WINHTTP_OPTION_RECEIVE_RESPONSE_TIMEOUT` to the request's timeout; libcurl reads it through its read callback and sends no `Expect: 100-continue`.
- `src/platform/multipart.{h,cpp}`: `MultipartBody`, the `multipart/form-data` body of an upload: text fields, then the file, read from disk while the body is sent. It checks field names, draws a random boundary, cleans the file name for the header but keeps its extension, and fails when the file shrinks or grows while it is sent. `src/platform/files.h` gained `OpenFileForWriting` and `OpenFileSize`.
- `src/core/transcript.{h,cpp}`: `Transcript` and `ParseVerboseJson`, with segment, word, and duration times in whole milliseconds, read exactly from the decimal text (`SecondsToMilliseconds`). Members the GUI does not use are ignored.
- `src/core/api_client.{h,cpp}`: `ApiClient` with `TranscribeRaw`, `Transcribe` (`verbose_json`, parsed), and `Progress`; `ApiErrorKind` and `ClassifyApiResponse`, which type transport failures and status codes after [API-CRISPASR.md §5](../API-CRISPASR.md#5-errors) and keep the server's error message; a check of the upload's size against the server's 512 MiB limit before sending.
- `src/util/json.{h,cpp}`: `WriteJson` (compact or indented by two spaces) and `QuoteJson`, with `JsonValue::MakeNumber(double)` and `MakeInteger`. Invalid UTF-8 becomes U+FFFD, so what it writes always parses; the parser and the writer share one UTF-8 check.
- `tests/stub_crispasr.cpp`: one thread per connection; `POST /v1/audio/transcriptions` with its own strict multipart parser, the server's order of checks (body, key, `file`, `response_format`, decoding), its error bodies, a model lock, and `/progress`; a synthetic transcript in the server's layout that also reports what arrived (file name, size, FNV-1a checksum, fields in order), or a replayed answer file; the request fields `stub_reply` and `stub_delay_ms`; `STUB_MAX_BODY_BYTES` for `413`; `POST /stub/echo` for the transport tests. It shares no code with the GUI.
- `tools/api_probe/api_probe.cpp`: a console tool that runs ten checks against a running server with the ApiClient (`/health`, `/v1/models`, `/progress`, a wrong key, a bad `response_format`, a file that is not audio, the audio as `verbose_json` while it watches `/progress`, the audio as `srt`, an optional window, and a cancellation once the server reports busy) and keeps every answer as received, with `report.txt` and `report.json`, in `probe-results/<UTC time>/`.
- Windows deliveries: `api_probe.exe`, `run-api-probe.cmd` (drop an audio file on it), and `CHECKLIST.md` (`tools/windows/CHECKLIST.md`) with the checks W4-1 to W4-4.
- Tests: `json_writer_test`, `multipart_test`, `transcript_test`, and `api_response_test` (unit); `api_client_test` and `api_probe_test` (integration), and POST cases in `http_test`. `tests/data/verbose-synthetic.json` is a synthetic answer in the server's layout (`tests/data/README.md`).

## Linux results (WSL2)

| Check | Command (in `<WSL_ROOT>`) | Result |
|---|---|---|
| Clean Debug build | `rm -rf build/linux && cmake -S . -B build/linux -G Ninja -DCMAKE_BUILD_TYPE=Debug && cmake --build build/linux` | exit 0; 223 steps; no warnings |
| Clean MinGW Release build | `rm -rf build/mingw && cmake -S . -B build/mingw -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/mingw-w64-x86_64.cmake && cmake --build build/mingw` | exit 0; 224 steps; no warnings. `api_probe.exe` imports only `KERNEL32`, `msvcrt`, and `WINHTTP`; `stub_crispasr.exe` only `KERNEL32`, `msvcrt`, and `WS2_32` (`objdump -p`) |
| Tests | `ctest --test-dir build/linux --output-on-failure` | exit 0; 20 of 20 passed (15 unit, 5 integration) |
| Integration tests repeated | `ctest --test-dir build/linux -L integration --repeat until-fail:20` | exit 0; 20 rounds in 212.7 s; afterwards no stub or probe process runs (`ps`) |
| Timeout and cancellation | `build/linux/tests/api_client_test` | a 1000 ms request timeout with a 500 ms connect timeout ended after 1502 ms (libcurl counts the connect budget into the limit); a cancel issued after 300 ms returned after 1202 ms ([ENVIRONMENT.md §4.6](../ENVIRONMENT.md#46-http-transport-facts)) |
| The stub against another encoder | uploads with `curl -F` (curl 7.88.1) to the stub, with a Japanese file name and a field with a line break | the stub reported the file name, size, checksum, and fields as sent; the error answers matched the server's bodies |
| The probe against the stub | `build/linux/api_probe --port <port> --window 1000,3000 --out <scratch> <file>`, with `STUB_TRANSCRIBE_MS=1500` | exit 0; ten checks PASS; `/progress` seen moving from 0 to 83; the cancelled request returned after 0.15 s and the stub reported idle 1.4 s later |

Mutation checks (one change at a time in a source file, the named tests rebuilt and run, the file restored and rebuilt): 10 of 11 changes were caught by the tests meant to catch them — the closing delimiter without its line break, `503` typed as a server error, the window fields never sent, rounding of milliseconds, unescaped control characters in written JSON, a growing file not detected, a failed upload body reported as cancelled, file names not cleaned, `busy` read inverted, and `401` typed as a bad request. Not caught: keeping libcurl's `Expect: 100-continue`, because libcurl adds it only to larger bodies and both the stub and cpp-httplib answer it; the transports do not depend on it.

Process note: the first round of mutation checks restored each file with its old modification time, so Ninja kept the mutated object files, and two results were wrong (a "caught" that came from an earlier mutation). After the restore was changed to give the file a new time, the build was redone and every check was repeated; the table above has the repeated results.

## Delivery

Both builds were rebuilt from a clean tree at commit `c296f239748f65db24077a17820ea33b5df5e353` (223 and 224 steps, no warnings, `ctest` 20 of 20), and `tools/package-windows.sh` produced `crisprustgui-c296f23.zip` in `<HANDOFF_OUT>`: 37,060,506 bytes, SHA-256 `005bbfbc3255a16710423d01825d086cbefb881eed17485acd96e60b4f04259d`, `unzip -tq` without errors. It holds `crisprustgui.exe`, `assets/`, `fonts/`, the 15 unit and 5 integration tests, `stub_crispasr.exe`, `api_probe.exe`, `testdata/` (with `verbose-synthetic.json`), `tests.txt`, `run-tests.cmd`/`.ps1`, `run-api-probe.cmd`, `CHECKLIST.md`, and `BUILD-INFO.txt` (79 MB unpacked).

## Windows results (owner's machine)

The delivery's `CHECKLIST.md` asks for W4-1 (`run-tests.cmd`, 0 failures of 20), W4-2 (the cohere profile started from the GUI reaches Running), W4-3 (`api_probe.exe --window 5000,10000` with the sample, and the `probe-results` folder), and W4-4 (Stop ends the server).

W4-1 (round 11): the owner ran `run-tests.cmd` of `crisprustgui-c296f23` on 2026-10-12 (the run started at 01:17 on 2026-10-13, UTC+8) on Windows 11 Pro for Workstations 10.0.22631 and sent `test-results.txt`: 19 of 20 tests passed.

- Passed: every unit test, among them `multipart_test` with its checks of a file that shrinks or grows while open; `process_test`; `http_test` with the POST cases (300,000 bytes echoed, a body that fails, an upload cancelled after 300 ms, a refused POST); `server_manager_test`; and `api_probe_test`, which runs `api_probe.exe` against the stub. So the WinHTTP upload works against the stub.
- `api_client_test` failed at line 305: a request with a 1000 ms limit, which the stub answered after 3000 ms, returned after 3017 ms instead of failing with a timeout. Its other checks passed, among them the cancellation: a cancel issued after 300 ms returned after 313 ms.
- What it shows (`verified` by this run): the transport's limit did not end the wait for the answer. The transport sets the session's timeouts with `WinHttpSetTimeouts` and the request handle's `WINHTTP_OPTION_RECEIVE_RESPONSE_TIMEOUT`, both to 1000 ms, and neither ended the wait.
- What it does not show: which setting governs that wait, and whether WinHTTP applies a limit of its own to it. If it does, a transcription that takes longer than that limit fails on Windows whatever the transport sets (`hypothesis`). `winhttp_timeouts.exe` (`tools/diagnostics/winhttp_timeouts.cpp`) measures both: short cases with a 1000 ms limit set in each place, and with `--long` an answer after 95 s with WinHTTP's defaults and with limits of 120 s. Its diagnostic delivery `crisprustgui-diag-winhttp_timeouts-1907626.zip` (307,640 bytes, SHA-256 `e27f43eea253b1828fc0d5bc9d4511af9e14c872f76656fcf9d9559e914199d8`) was built at commit `1907626aecc659c0cd80b06a86984d9398dae057` without warnings; it imports only `KERNEL32`, `msvcrt`, `VERSION`, `WINHTTP`, and `WS2_32`.

W4-2 to W4-4 did not run: the owner did not find how to start the server from the GUI (round 11). At the default window size of 1100×720, the profile list with its Start buttons begins below the visible part of the dashboard's scrolling area; the page says "Start the server from a profile below", but the first screen shows no Start button (WSLg capture with the fake deployment, round 11). `CHECKLIST.md` now says where to scroll and what to do when a server started elsewhere holds the port, and the next delivery moves the profile list up.

Diagnostic (round 12): the owner returned the output of `winhttp_timeouts.exe --long`. A copy that A003 read first ended after the heading of the long cases; the file now in the diagnostic's folder holds a complete run, up to "Done." (WinHTTP 6.2.22621.4830). It shows that the transport's settings take effect, that WinHTTP checks timeouts on a coarse timer of about 4 s, that its defaults would end a silent wait after about 30 s, and that with the transport's configuration a 95 s answer arrives within a 120 s limit ([ENVIRONMENT.md §4.6](../ENVIRONMENT.md#46-http-transport-facts)). The `api_client_test` failure is therefore a test that allows too little time, not a transport defect; [P4-T1](../agents/tasks/P4-T1-timeout-test/packet.md) corrects the test. The error texts in the returned file are garbled ("操作超时" arrived as "鎿嶄綔瓒呮椂") because the output passed through a PowerShell pipe; the numbers are unaffected.

W4-2 (round 12, owner statement): the GUI started the server, and the owner reports "通过了软件测试，能正常生成字幕". W4-2 counts as passed on that statement; no screenshot was sent.

W4-3 (round 13): not run yet. The owner's screenshot of the delivery folder (`1.jpg`, returned into the delivery's folder in `<HANDOFF_OUT>`) showed the `appdata\` folder of the GUI's run but no `probe-results\` folder, which `api_probe.exe` creates next to itself when it runs.

W4-3 (round 14): passed. The owner ran `api_probe.exe --window 5000,10000` with `E:\AI\ASR\CrispASR\samples\ja-demo.mp3` (58,176 bytes, SHA-256 `5b471724bbb7f53077a6e011a48e33b433251503a80c1ffd28fbba58d79b6448`) at 2026-10-13T05:03:47Z and returned `probe-results\20261013T050347Z\`: all ten checks `PASS` and `Failures: 0`; the exit code was not sent. The server had the parakeet profile loaded (`/health` backend `fastconformer-ctc`, model `parakeet-ctc-1.1b-ja-q8_0.gguf`), not the cohere profile that W4-2 names; who started it is not recorded. What the answers show (`observed` on the deployed server):

- `verbose_json` for the 9.696 s sample: one segment from 0.23 s to 8.71 s with 46 word times, one per character, in the layout of `crispasr_output.cpp` that the synthetic fixture follows; 3226 bytes with two-space indentation.
- The window `offset_t_ms=5000`, `duration_ms=10000` answers in original-file time (its segment starts at 5.07 s), and its `duration` is 4.696 s, the part of the window inside the file, as [API-CRISPASR.md](../API-CRISPASR.md) §3.1 and §4 say.
- The cut at 5 s fell inside a word: the full transcription has タイムスタンプ from 4.61 s to 5.29 s, and the window begins with キャンプ instead. This is the case the boundary guard of [ARCHITECTURE.md §2.5](../ARCHITECTURE.md#25-steprunner-interface-and-the-asr-window-loop) is for.
- The server's `srt` is one cue whose text equals the segment's text, with the segment's times as `HH:MM:SS,mmm`, LF line ends, and an empty line after the cue. With the `verbose_json` of the same audio it is the equivalence fixture that [OQ-7](../OPEN-QUESTIONS.md#oq-7-local-srt-generation) needs.
- Error bodies: `{"error": {"message": ..., "type": "invalid_api_key"}}` for the `401`, and the type `invalid_request_error` for both `400`s; the client reports their messages.
- `/progress` reported `busy` with `progress` 0 at 0.32 s into a transcription; the cancellation returned at once, and the server reported idle 0.2 s later.

[P4-T5](../agents/tasks/P4-T5-server-answers/packet.md) adds these answers to `tests/data/`.

W4-4 (round 15, owner statement): passed. After Stop and its confirmation the server panel showed Stopped with exit code 1, and the GPU memory and RAM the server held were released. Exit code 1 is the code the GUI's stop gives the process: `src/platform/process_win32.cpp` ends it with `TerminateProcess(..., 1)`.

## Decisions and deviations

- The ApiClient offers the calls the transcription needs: the upload and `/progress`. `/health` and `/v1/models` stay with the ServerManager's poller, which already reads them; ARCHITECTURE's earlier list of four calls is updated.
- Error kinds: `File` joins the typed set for a file that cannot be opened or read. A connection that is reset or closed without an answer counts as `Unreachable`, as the "no response" row of API-CRISPASR §5 has it; an answer over the size limit counts as `Protocol`. For `413` the message is the client's own, because the server's body is the generic "not found" text.
- The multipart body is the project's own for both transports, so the bytes the Windows build sends are the ones the Linux tests check exactly; libcurl's MIME API would have tested a different encoder on Linux.
- The client refuses an upload over 512 MiB before sending: cpp-httplib stops reading at the limit, so a larger upload may end in a reset connection instead of a `413` ([API-CRISPASR.md §5](../API-CRISPASR.md#5-errors)).
- The file part carries the file's own name, cleaned for the header, because the server's decoder takes its format hint from the extension.
- Defaults: 5 s to connect, 30 min for one transcription request, 64 MiB for an answer. The timeout becomes a setting in Phase 8.
- The stub answers a malformed multipart body with a `400` that names the problem, where CrispASR sends its generic body, so a failing test shows what went wrong; the status, which the client types, is the same.
- The Windows-side agent is registered without a stable ID or display name; both wait for the owner ([agents/README.md](../agents/README.md#identities)). Round 13: `A005` is proposed.

## Open items

- [P4-T1](../agents/tasks/P4-T1-timeout-test/packet.md): a timeout check that allows for WinHTTP's coarse timer, merged as `c483964` after Review R01 passed it (its Windows check is W4-1 of the next delivery); [P4-T2](../agents/tasks/P4-T2-dashboard-start/packet.md): Start buttons on the dashboard's first screen; [P4-T3](../agents/tasks/P4-T3-delivery/packet-v2.md): the next delivery. With [P4-T4](../agents/tasks/P4-T4-rename/packet.md) (the rename to CrispGUI) and [P4-T5](../agents/tasks/P4-T5-server-answers/packet.md) (the server's answers as test data), all go to A004 under the [workflow](../agents/README.md#workflow) of round 13.
- W4-1 to W4-4 again with the next delivery; the phase closes when its W4-1 has no failures.
- The probe's recorded answers become test data in `tests/data/` ([P4-T5](../agents/tasks/P4-T5-server-answers/packet.md); [OQ-4](../OPEN-QUESTIONS.md#oq-4-source-material-needed) item 3).
- The project's code-level names still read CrispRustGUI (executable, window title, delivery names); [P4-T4](../agents/tasks/P4-T4-rename/packet.md) renames them to CrispGUI.
- That a client still sending an oversized body sees a reset rather than the `413` is a `hypothesis` from the source; the client never sends one.
- libcurl's cancellation latency of up to about a second affects only Linux development builds.
- [OQ-7](../OPEN-QUESTIONS.md#oq-7-local-srt-generation) (local SRT generation) is the next decision; Phase 5 needs it. A003 decides it as planner when it plans Phase 5.
