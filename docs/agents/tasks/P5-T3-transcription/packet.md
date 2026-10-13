# P5-T3 Transcribing one file: the controller behind the Transcribe page — packet v1

| Field | Value |
| --- | --- |
| Task / version / date | `P5-T3` / v1 / 2026-10-13 (round 22) |
| Coordinator / Coding / acceptance | A003 / A004 / A003 for Linux; Windows in the Phase 5 delivery |
| Profile | `strict` (worker threads, cancellation, and a file written for the user) |
| Base and branch | `origin/main` when A004 starts; branch `a004/P5-T3` in `<CODING_ROOT>` |
| Write scope | `src/core/transcription.{h,cpp}` (new; the name is A004's choice), `CMakeLists.txt` (the new source in `crisp_core`), `src/core/server_manager.{h,cpp}` and `src/core/server_state.h` only to expose the API key the manager uses, `tests/transcription_test.cpp` (new), `tests/stub_crispasr.cpp` (a fail-once mode for the retry case), `tests/CMakeLists.txt`, this task folder |
| Protected | The UI (`src/ui/`, `src/app/`), which [P5-T4](../P5-T4-transcribe-page/packet.md) wires; the transports, the writer, and the atomic write (use them, do not change them); every document |
| Read first | [AGENTS.md](../../../../AGENTS.md); [SPEC.md FR-3](../../../SPEC.md#fr-3-transcription-single-file); [ARCHITECTURE.md §2.2](../../../ARCHITECTURE.md#22-servermanager) (the inbox pattern) and [§2.3](../../../ARCHITECTURE.md#23-apiclient-and-http-transport); [API-CRISPASR.md §5](../../../API-CRISPASR.md#5-errors) and [§7](../../../API-CRISPASR.md#7-progress); [CONVENTIONS.md](../../../CONVENTIONS.md); `src/core/server_manager.h` |

## Problem

Phase 5 transcribes one file from the GUI (SPEC FR-3). The pieces exist: `ApiClient::Transcribe` and `Progress`, `SegmentsToSrt` (P5-T1), and `WriteFileAtomically` (P5-T2). This task joins them in a core class the Transcribe page can drive; the page itself is P5-T4.

## Change

A class in `src/core/`, owned by the UI thread like `ServerManager`: worker threads post to an inbox, a `wake` callback (the app passes `app::requestUpdate`) is never called after `Shutdown()`, and `Update()` applies the inbox on the UI thread.

- Start, given a source file, the running server's endpoint and API key, and whether an existing SRT may be replaced. The destination is `<source folder>/<source name without extension>.srt`. Before any request: the source must be a readable regular file; a source over the client's upload limit fails as too large; an existing destination without permission to replace fails as "destination exists". One transcription at a time; a start while one runs is ignored.
- Running: one thread calls `ApiClient::Transcribe` with an `HttpCancel`; while it waits, `/progress` is read about every 500 ms. The state reports the phase, the server's progress (0 to 100, or unknown), and the elapsed time.
- Retries (FR-3): a `500` is retried once; nothing else is retried.
- Success: `SegmentsToSrt` of the transcript, written with `WriteFileAtomically` to the destination. The state then holds the destination, the number of cues, the transcript's duration, and the first few cue texts for a preview.
- Failure: a kind and a detail, never a UI string: the server is unreachable (the page decides between "not running" and "still starting" from the server's state), timeout, cancelled, bad request with the server's message, unauthorized, too large, server busy (`503`), server error after the retry, unexpected answer, the source cannot be read, the destination exists, the SRT cannot be written (with `WriteFileAtomically`'s message).
- Cancel: through the `HttpCancel`; nothing is written, and no temporary file remains.
- `Shutdown()` cancels a running transcription and joins every thread.
- The API key: `ServerStatus` has the endpoint but not the key the manager uses (the one a started server got, or the configured key for an adopted one). Expose it so that the transcription uses exactly what the manager's own checks use.

## Acceptance

| ID | Criterion | Verification |
| --- | --- | --- |
| AC-1 | End to end on Linux: with the stub answering `tests/data/deployed-parakeet/transcript.verbose.json` (`STUB_TRANSCRIPT`), a transcription writes `<name>.srt` byte-equal to `tests/data/deployed-parakeet/transcript.srt` | `transcription_test` |
| AC-2 | With a stub delay, the state shows the server's progress at least once before the result | `transcription_test` |
| AC-3 | Failures, each with its kind and nothing written: `400` with the server's message; a wrong key; `503`; `500` twice (one retry, then the failure); no server listening; a missing source; a source over a small upload limit set for the test; an existing destination without permission to replace. And `500` once, then success: the retry succeeds | `transcription_test`; the stub's fail-once mode |
| AC-4 | An existing destination with permission to replace is replaced atomically | `transcription_test` |
| AC-5 | A cancel during a delayed transcription returns cancelled within 2500 ms; no destination and no temporary file exist afterwards | `transcription_test` |
| AC-6 | `Shutdown()` during a transcription returns within 2500 ms, and `wake` is not called afterwards | `transcription_test` (count the calls) |
| AC-7 | Mutation matrix, each caught by a test: M1, no retry on `500`; M2, a retry on `400`; M3, the SRT written in place instead of through `WriteFileAtomically`; M4, the permission to replace ignored; M5, `wake` called after `Shutdown()`; M6, the cancel not passed to the request | The Review runs each in its sandbox |
| AC-8 | Gates pass, G3 included | The [standard gates](../README.md#standard-gates) |

## Review and merge

Review `R01` by a fresh Sonnet 5.5 subagent with the prompt of [tasks/README.md](../README.md#review-dispatch); profile `strict`. After PASS, merge per [agents/README.md](../../README.md#workflow) and notify A003 with the merged commit.

## Next event

A004 starts the task and writes `receipt.md`; P5-T4 follows.
