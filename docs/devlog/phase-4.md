# Phase 4 Report — ApiClient

Status: implemented; the Linux acceptance passes, and the Windows acceptance waits for the owner's run of the delivery. Written by A003 on 2026-10-12 (round 10; dates are UTC). The plan is [TASKS.md Phase 4](../TASKS.md#phase-4--apiclient); the design is [ARCHITECTURE.md §2.3](../ARCHITECTURE.md#23-apiclient-and-http-transport). On 2026-10-12 the owner agreed to two additions: a numbered checklist in every Windows delivery, and an entry for the Windows-side agent in the identity register.

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

Packaged after the Phase 4 commit and recorded in the delivery record that follows it.

## Windows results (owner's machine)

Not run yet. The delivery's `CHECKLIST.md` asks for:

- W4-1: `run-tests.cmd` with 0 failures of 20, and `test-results.txt`.
- W4-2: the cohere profile started from the GUI reaches Running.
- W4-3: `api_probe.exe --window 5000,10000 "E:\AI\ASR\CrispASR\samples\ja-demo.mp3"` with ten checks PASS (INFO allowed for the wrong key and the cancellation), and the whole `probe-results\<UTC time>\` folder.
- W4-4: Stop ends the server.

## Decisions and deviations

- The ApiClient offers the calls the transcription needs: the upload and `/progress`. `/health` and `/v1/models` stay with the ServerManager's poller, which already reads them; ARCHITECTURE's earlier list of four calls is updated.
- Error kinds: `File` joins the typed set for a file that cannot be opened or read. A connection that is reset or closed without an answer counts as `Unreachable`, as the "no response" row of API-CRISPASR §5 has it; an answer over the size limit counts as `Protocol`. For `413` the message is the client's own, because the server's body is the generic "not found" text.
- The multipart body is the project's own for both transports, so the bytes the Windows build sends are the ones the Linux tests check exactly; libcurl's MIME API would have tested a different encoder on Linux.
- The client refuses an upload over 512 MiB before sending: cpp-httplib stops reading at the limit, so a larger upload may end in a reset connection instead of a `413` ([API-CRISPASR.md §5](../API-CRISPASR.md#5-errors)).
- The file part carries the file's own name, cleaned for the header, because the server's decoder takes its format hint from the extension.
- Defaults: 5 s to connect, 30 min for one transcription request, 64 MiB for an answer. The timeout becomes a setting in Phase 8.
- The stub answers a malformed multipart body with a `400` that names the problem, where CrispASR sends its generic body, so a failing test shows what went wrong; the status, which the client types, is the same.
- The Windows-side agent is registered without a stable ID or display name; both wait for the owner ([agents/README.md](../agents/README.md#identities)).

## Open items

- The Windows checks W4-1 to W4-4.
- The probe's recorded answers become test data in `tests/data/` and replace the synthetic answer where they cover the same case ([OQ-4](../OPEN-QUESTIONS.md#oq-4-source-material-needed) item 3).
- That a client still sending an oversized body sees a reset rather than the `413` is a `hypothesis` from the source; the client never sends one.
- libcurl's cancellation latency of up to about a second affects only Linux development builds.
- [OQ-7](../OPEN-QUESTIONS.md#oq-7-local-srt-generation) (local SRT generation) is the next decision; Phase 5 needs it.
