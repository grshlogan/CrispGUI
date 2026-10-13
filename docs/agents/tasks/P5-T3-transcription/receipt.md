# P5-T3 receipt

- Identity, session, model: A004, Claude Code session `0d1710c9-17d2-4343-bd00-ec946bfba62a`, Claude Sonnet 5.5
- Packet: packet.md (v1); profile `strict`
- Base: origin/main `48b2579` on 2026-10-13 (UTC); branch `a004/P5-T3`; `origin/main` had not moved when the candidate was frozen, so no rebase
- State: Merged (pushed to `origin/main` as a fast-forward after a PASS in R01)
- Candidate commits: `130bc75` (first version), `b705173` (the source is looked at before it is opened; four more test cases; the stub fails after its delay), and `e6a060075d1a3ce520fa1fb82c471531e28fb9c4` (the stub's counter renamed). R01 reviewed `e6a0600` ([review-R01.md](review-R01.md), PASS with six P3 notes; it took two dispatches: the first met the session limit after it had read the packet and wrote no report). The gates below were made on `e6a0600`, the mutation matrix and the sanitizer runs on `b705173`, which differs from it only in the stub's variable name. The commits that add R01's report and this receipt, and the identity context, follow it; the pushed tip cannot be named here, and the notice to A003 and `git log origin/main` carry it

## Changes

| Path | Why | Criterion |
| --- | --- | --- |
| `src/core/transcription.h` (new) | The interface: `TranscriptionPhase`, `TranscriptionFailure`, `TranscriptionRequest`, `TranscriptionOptions`, `TranscriptionState`, `TranscriptionDestination`, and the class `Transcription` (`Start`, `Cancel`, `Shutdown`, `Update`, `State`) | AC-1 to AC-6 |
| `src/core/transcription.cpp` (new) | The inbox, the job (`Execute`, `CheckSource`, `CheckDestination`, `Request`, `PollProgress`, `Write`), and the class | AC-1 to AC-6 |
| `CMakeLists.txt` | The source in `crisp_core` | AC-8 |
| `src/core/server_state.h`, `src/core/server_manager.h`, `src/core/server_manager.cpp` | `ServerStateMachine::ApiKey()` and `ServerManager::ApiKey()`: the key the manager's own checks use (a started server's key, or the configured key of an adopted one) | the packet's last Change item |
| `tests/transcription_test.cpp` (new), `tests/CMakeLists.txt` | The test, registered as an integration test (six now) | AC-1 to AC-7 |
| `tests/stub_crispasr.cpp` | `STUB_FAIL_FIRST="<status>:<count>"` (the first requests work for their delay and then fail, as an error inside the model does) and `GET /stub/requests` (the number of transcription requests received); both in the header comment | AC-3 |
| `docs/agents/tasks/P5-T3-transcription/receipt.md` | This receipt | — |

`git diff --stat 48b2579..e6a0600` lists these paths (the receipt as first committed) and nothing else; the UI, the transports, the writer, the atomic write, and every document are untouched.

Behavior, in the order of `Job::Execute` (`src/core/transcription.cpp:141`):

1. `Start()` (UI thread, `:377`) makes the state `kRunning` at once (source, destination, attempt 0, progress -1) and starts the job thread; it returns false while a transcription runs and after `Shutdown()`.
2. The job thread checks the source (`CheckSource`, `:173`): `status` first (a pipe is never opened), a regular file, opens, size within `maxUploadBytes` (`kSourceUnreadable`, `kTooLarge`); then the destination (`CheckDestination`, `:210`): it must not exist unless `replaceExisting`, and is never the source itself (`kDestinationExists`). Nothing has been sent so far.
3. It starts the progress thread (`PollProgress`, `:275`) and calls `ApiClient::Transcribe` with the job's `HttpCancel` (`Request`, `:234`), posting `kAttempt` first. A 500 on the first attempt is followed by one pause (`retryDelayMs`, interrupted by a cancel) and one more attempt; every other failure ends the job. The progress thread reads `/progress` every `progressIntervalMs` while a request waits and posts every reading, with the attempt it belongs to; the value counts only when the server is busy.
4. After the request the progress thread is stopped and joined, then `Write` (`:307`) checks for a cancel and the destination again, writes `SegmentsToSrt` with `WriteFileAtomically`, and counts the cues (segments with text) and the first `previewCues` texts with their speaker prefix.
5. The job thread posts `kFinished` (phase, failure, detail, cues, duration, preview, elapsed). `Update()` (UI thread, `:413`) applies the events in order, ignores a reading of an earlier attempt, sets progress 100 on success, and refreshes the elapsed time; it returns true when an event arrived or the elapsed time passed a whole second.
6. `Cancel()` sets the flag, cancels both `HttpCancel`s, and wakes the waits. `Shutdown()` does the same and joins the job thread (which joined the progress thread), so `wake` is never called after it returns; the destructor calls it.

Decisions inside the packet's freedom:

- Two threads per transcription, a job thread that does the request and the writing and a progress thread, because `Transcribe` blocks. Both `HttpCancel` objects belong to the job (a cancel is sticky), so there is no pointer to a cancel object of another thread.
- The failure kinds are the packet's list one to one (`ApiErrorKind` maps through `FailureOf`, `:40`); `kFile` from the client maps to `kSourceUnreadable`. The detail is the server's or the system's English text and never a UI string.
- The order of the checks follows the packet: readable, within the limit, destination. The destination is checked again just before the write, because it may have appeared during minutes of transcription; the transcript is then lost, the file is not overwritten. A destination that is the source itself (`x.srt` as the source) is refused even with permission. Both are small additions to the packet's list.
- A cancel that arrives after the answer but before the write ends as `kCancelled` and writes nothing; one that arrives while the file is being written lets the write finish and the state is `kDone` (documented in the header). Any failure that follows a cancel is reported as cancelled.
- Progress: only a busy server's value is taken (docs/API-CRISPASR.md §7: it reads idle during the upload and is global); each reading carries its attempt, and a new attempt starts at unknown. Every reading is posted, so `wake` and `Update()` run at the polling pace and the page shows the elapsed time without a timer of its own.
- A transcript without cues is a success: an empty SRT is written and the state says 0 cues; the page words it. The preview lines are the cue texts exactly as the file has them (speaker prefix included, no trimming).
- `fields` passes request-tier fields through (the tests use it to steer the stub; the page will use it for the parameters of FR-4). The 30 min request timeout is the client's default (ARCHITECTURE §2.3).
- Not added: a check that the destination folder is writable before the transcription. A read-only folder is found only after the request, and the transcript is lost. A probe file or keeping the SRT text for a "save elsewhere" action would cure it; both are beyond the packet, so this is a question for A003.

## Acceptance mapping

`<scratchpad>` is `/tmp/claude-1000/-home-claude-projects-CrispGUI-a004/0d1710c9-17d2-4343-bd00-ec946bfba62a/scratchpad`. Line numbers are those of `e6a0600`.

| Criterion | Code | Check that fails without the change | Evidence |
| --- | --- | --- | --- |
| AC-1: the SRT equals `tests/data/deployed-parakeet/transcript.srt` byte for byte | `Job::Write` (`transcription.cpp:307`), `TranscriptionDestination` (`:351`) | `tests/transcription_test.cpp:283-304`: the stub answers the Parakeet `verbose_json` (`STUB_TRANSCRIPT`); the source is `音声 テスト.wav`; the SRT is `音声 テスト.srt`, equal to the recorded file; 1 cue, 9696 ms, the preview is the cue text, attempt 1, progress 100, one request, the folder holds the two files, `wake` and `Update()` fired. Also `:234-242` (the destination names), `:306-341` (3 cues from 4 segments, speaker prefixes, the preview follows `previewCues`), `:343-359` (no segments: an empty SRT, 0 cues). Mutations X20, X5, X6, X7, X9, X22 | Passes on Linux (G2) |
| AC-2: the state shows the server's progress before the result | `PollProgress` (`:275`), `Update()` (`:413`) | `:361-390`: a 2 s delay and a 100 ms interval; the samples of `progress` while running are non-empty, rise, and stay within 0 to 100; the elapsed time passed 1 s while running; the end has progress 100. Mutations X8, X17, X22 | Passes on Linux |
| AC-3: the failures, each with kind and nothing written | `Request` (`:234`), `FailureOf` (`:40`), `CheckSource`, `CheckDestination`, `Write` | `:392-536`: 400 with the server's message and no second request (`:400`); 401 (`:412`); 503, one request (`:420`); 500 twice, two requests, attempt 2 (`:428`); an answer that is not a transcript (`:437`); no server (`:445`); a missing source and a folder, nothing sent (`:453`); a source over the limit, refused before the question about an existing SRT (`:469`); an existing SRT without permission (`:484`); the source itself (`:495`); an SRT that appears during the work (`:506`); a folder in the SRT's place, after the request (`:519`); the runner works after all of it (`:532`). Also 413 from the server (`:538`), 500 once then success with attempt 2 (`:551`), the retry starting with unknown progress (`:566`), a cancel during the pause (`:594`), a timeout (`:616`). In each failure case the folder holds only what it held. Mutations M1, M2, M2b to M2e, X1 to X4b, X12, X15, X16, X21, X25, X29 | Passes on Linux |
| AC-4: an existing SRT with permission is replaced atomically | `Write` with `WriteFileAtomically` | `:632-662`: the destination's inode changes (`st_ino`), a reader that held the old file still reads all of the old bytes, the new content is the recorded SRT, the folder holds the two files. Mutations M3, M4a | Passes on Linux; the reader and inode part is POSIX-only |
| AC-5: a cancel ends the transcription within 2500 ms; no destination, no temporary file | `Job::Cancel` (`:130`), `Request` | `:701-726`: a 10 s delay, a cancel after the stub holds the request; cancelled in under 2500 ms, empty detail, attempt 1, the folder holds only the source; the runner starts again afterwards. Mutations M6, X11, X25 | Passes on Linux |
| AC-6: `Shutdown()` within 2500 ms, no `wake` afterwards | `Transcription::Shutdown` (`:400`), the destructor (`:373`) | `:728-768`: `Shutdown()` returns in under 2500 ms, the `wake` count stays equal for 1500 ms (libcurl needs up to 1.2 s to notice a cancel), `Start()` is refused afterwards, the state is cancelled, the second `Shutdown()` does nothing; the destructor of a running object returns in under 2500 ms. Mutations M5, X23, X24 | Passes on Linux |
| The API key | `ServerStateMachine::ApiKey()` (`server_state.h:174`), `ServerManager::ApiKey()` (`server_manager.cpp:636`) | `:244-277`: a machine that adopted a server returns the configured key; one that spawned returns the report's key. `:770-799`: a manager that adopts the stub returns the configured key, and a transcription with `Status().endpoint` and `ApiKey()` succeeds. Mutations K1, K2 | Passes on Linux |
| AC-7: mutation matrix M1 to M6, each caught | See the table below | See the table below | `<scratchpad>/mut/final.out` |
| AC-8: gates pass, G3 included | — | G1 to G4 on `e6a0600` | Commands table |

Mutation matrix (AC-7 and my own additions), run on `b705173`. Each mutation changes one place with a script (`<scratchpad>/mut/mutate.py`), builds `transcription_test`, runs it with `ctest`, and restores the file from a saved copy (`git status` clean afterwards); "caught" means exit 8, a failed check. The numbers are the failed checks and the first failing line of `tests/transcription_test.cpp`.

| ID | Mutation | Result |
| --- | --- | --- |
| M1 | No retry after a 500 | Caught: 7 checks, from 434 (the second attempt and the second request are missing), 560 to 563 (the retry that succeeds), 589, 610 |
| M2 | A retry after a 400 | Caught: 1 check, 408 (two requests instead of one) |
| M2b, M2c, M2d | A retry after a 503, a 401, or an answer that is not a transcript | Caught: 1 check each, 425, 417, 442 |
| M2e | A second retry after a 500 | Caught: 1 check, 434 (three requests) |
| M3 | The SRT written in place (open, truncate, write) instead of `WriteFileAtomically` | Caught: 3 checks, 526 (a folder in the SRT's place no longer fails at the replace step), 654 (the inode does not change), 658 (the reader no longer sees the old bytes) |
| M4a | The permission to replace never honored | Caught: 9 checks, from 338 |
| M4b | The permission to replace always assumed | Caught: 5 checks, from 489 |
| M5 | `Shutdown()` does not join the thread (its end calls `wake` later) | Caught: 2 checks, 747 and 753 (the `wake` count grew after `Shutdown()` returned) |
| M6 | The cancel not passed to the request | Caught: 8 checks, from 715 (45 s: the stub's delay ran out) |
| X1 | The destination not checked before the request | Caught: 2, 490 and 502 |
| X2 | The destination not checked again before the write | Caught: 2, 514 |
| X3 | The source may be replaced by its own transcript | Caught: 3, 501 |
| X4 | The source not checked before the request | Caught: 3, 458 (the message), 478 |
| X4b | The upload limit not checked by the controller (the client's check remains) | Caught: 1, 478 (the order of refusals) |
| X5, X6, X7 | The preview not limited; without speaker prefix; the cue count includes empty segments | Caught: 1, 2, 4 checks, from 340, 327, 326 |
| X8 | The elapsed time not refreshed by `Update()` | Caught: 1, 388 |
| X9 | Progress not 100 when done | Caught: 2, from 298 |
| X10 | `Start()` accepted while one runs | Caught: 4, from 679 |
| X11 | `Cancel()` does nothing | Caught: 8, from 609 |
| X12 | The pause before the retry cannot be interrupted | Caught: 2, 609 and 610 (38 s) |
| X15, X16 | A timeout reported as unreachable; a 413 as a bad request | Caught: 1 each, 627 and 547 |
| X17 | `Update()` never returns true | Caught: 1, 303 |
| X20 | The destination appends ".srt" to the source's name | Caught: 30, from 236 |
| X21 | The write failure without its message | Caught: 1, 526 |
| X22 | No attempt event | Caught: 14, from 298 |
| X23 | `Shutdown()` does not cancel | Caught: 5, from 743 (37 s) |
| X24 | `Start()` accepted after `Shutdown()` | Caught: 2, 750 |
| X25 | A failure during a cancel not reported as cancelled | Caught: 2, 610 and 718 |
| X29 | Progress not reset by a new attempt | Caught: 1, 591 |
| K1 | `ServerStateMachine::ApiKey()` returns the configured key | Caught: 1, 276 (the spawn path) |
| K2 | `ServerManager::ApiKey()` returns nothing | Caught: 3, from 788 |
| X26 | The cancel flag not checked before the write | **Not caught.** The window lies between the answer and the start of the write, a few microseconds with the stub's small answers; a test would need a hook or a very large answer. The check is two lines |
| X27 | A reading of an earlier attempt applied to a later one | **Not caught.** It needs a reading that arrives after the next attempt began; not reproducible with the stub |
| X28 | The progress of an idle server taken | **Not caught.** The stub reports -1 when idle; whether the real server keeps the last value is `not verified` (docs/API-CRISPASR.md §7 says "idle, or the previous job") |
| X30 | The source not looked at with `status` before it is opened | **Not caught on Linux.** Opening a folder succeeds there and `OpenFileSize` refuses it with the same message; the check protects against a pipe, whose open waits for a writer, and gives a folder the message "is not a regular file" on Windows, where `_wfopen` of a folder fails. A test with a pipe would need a helper thread to free a mutant |

A first run of the harness left objects built from mutated sources in the tree, because the restored file kept its old modification time; K2 was polluted by K1 and a test then failed on the stale object. The harness now touches a restored file, and the numbers above are from the second, complete run on `b705173`.

## Commands

All in `<CODING_ROOT>` (`/home/claude/projects/CrispGUI-a004`), Linux/WSL2, `set -o pipefail`; logs in `<scratchpad>/logs/`. The Windows run is `not run`: this is a Linux check.

| ID | Command | Exit | Evidence |
| --- | --- | --- | --- |
| G1 | `cmake --build build/linux --target clean`, then `cmake -S . -B build/linux -G Ninja -DCMAKE_BUILD_TYPE=Debug && cmake --build build/linux` | 0 | 231 of 231 steps, 0 lines with `warning:` (`P5T3-G1-c3b.log`) |
| G2 | `ctest --test-dir build/linux --output-on-failure` | 0 | 23 of 23 passed, 30.13 s; `transcription_test` 18.7 s (`P5T3-G2-c3.log`) |
| G3 | `ctest --test-dir build/linux -L integration --output-on-failure`, five rounds | 0, 0, 0, 0, 0 | 6 of 6 each round (`P5T3-G3-c3-1.log` to `-5.log`) |
| G4 | `cmake --build build/mingw --target clean`, then `cmake -S . -B build/mingw -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/mingw-w64-x86_64.cmake && cmake --build build/mingw` | 0 | 234 of 234 steps, 0 warnings; `transcription_test.exe` is built (`P5T3-G4-c3b.log`) |
| Whitespace and width | `git diff --check` on each commit; `awk 'length($0) > 120'` on the changed files | 0; no output | — |
| ThreadSanitizer | A tree configured with `-fsanitize=thread`; `setarch x86_64 -R ctest --test-dir build/tsan -R '^transcription_test$'`, three runs on `b705173` | 0, 0, 0 | No `WARNING: ThreadSanitizer` line (`P5T3-tsan-run*.log`). Without `setarch -R` the runtime stops with "unexpected memory mapping" on this kernel |
| AddressSanitizer and UBSan | A tree configured with `-fsanitize=address,undefined`; `setarch x86_64 -R ctest ... -R '^transcription_test$'`, three runs on `b705173`, leak detection on | 0, 0, 0 | No report (`P5T3-asan-run3*.log`). Without `setarch -R` two runs failed: stub processes died or spun on `DEADLYSIGNAL`, the same runtime problem, not a finding |

The test needs about 19 s on Linux; `tools/windows/run-tests.ps1` ends a test after 60 s.

## Uncovered, risks, and questions

- `not run`: everything on Windows: `transcription_test.exe` against WinHTTP (the cancel is immediate there, the timeout may end seconds late: the timeout case allows 12 s), the folder-in-place case (`MoveFileExW` over a folder), and the runtime of about 19 s plus the start of a dozen stub processes against the 60 s limit of `run-tests.ps1`. I suggest the Phase 5 delivery's W5 list carries the test.
- The three mutations X26, X27, and X28 survive (see above); X30 survives on Linux.
- Question for A003: should a transcription check that the destination folder is writable before the request? Now an unwritable folder (a read-only share or disc) fails after the work, with `kCannotWrite`, and the transcript is gone.
- The page (P5-T4) must call `Update()` every frame and pass `wake` that reaches `app::requestUpdate`; the state's `elapsedMs` moves only through `Update()`.

## Review

R01 (fresh Sonnet 5.5 subagent, prompt of [tasks/README.md](../README.md#review-dispatch), candidate `e6a0600`, base `48b2579`, previous report none): PASS. The report is [review-R01.md](review-R01.md), committed unchanged. It reran G1 to G4 from a fresh export with the same numbers (G3 in five rounds), reproduced the receipt's line numbers and its results for M1 to M6, K1, K2, and the four disclosed survivors, ran 39 mutations of its own (32 caught, 7 survive: the four disclosed ones and F1 to F3 below), ran ThreadSanitizer and AddressSanitizer/UBSan on the test and on a 17-scenario probe of its own (cancel and `Shutdown()` at 40 to 48 moments, odd names, a pipe, a link, a 300 MB source, a killed server, a read-only folder) without a report, and found no P0, P1, or P2.

My answers to its six P3 notes. A finding that would change the reviewed patch is not applied after a PASS; each waits for the next change of that code or goes to A003.

| Note | Answer |
| --- | --- |
| F1: nothing pins "no retry" for an unreachable server, and a retry after a timeout is caught only by the 12 s bound | Agreed. Two checks, `attempt == 1` in the no-server case (`tests/transcription_test.cpp:445`) and in the timeout case (`:616`), at the next change of the test |
| F2: no test pins the `status` lookup before the open (a pipe blocks `fopen`; the Review's probe hung without it) | Agreed, and it is the receipt's X30. A Linux-only case with a pipe and a helper thread that frees a mutant after 3 s, at the next change of the test |
| F3: nothing pins that `elapsedMs` stops and `Update()` is quiet after the end | Agreed. After the end, wait 1.1 s and check `Update()` is false and `elapsedMs` unchanged, at the next change of the test |
| F4: the guards X26, X27, X28 and the new A20 (the in-flight `/progress` read cancelled before the join) have no test | As disclosed. A18 needs a stub that keeps the last progress while idle, A20 a stub that delays `/progress`; X26 and X27 need a seam, and I leave them. Both stub variables with the next change of the stub |
| F5: an existing destination that is not a regular file (a folder) passes the check when replacing is allowed, and the work is lost at the write; a read-only folder does the same | Question for A003 together with the receipt's question about a writable folder: refuse a non-file destination before the request (cheap), and either probe the folder or keep the SRT text in the state for a "save elsewhere" action. Nothing is damaged in either case (S15 of the Review) |
| F6: after a failure `progress` keeps the last reading (76, 7) | The header says "while it is known; 100 once done". P5-T4 reads `progress` only while the phase is `kRunning` or `kDone`; the sentence in `transcription.h` says so at the next change of that header, or A003 decides on a reset to -1 |

Checks the Review added beyond mine: the object code of MinGW's `std::filesystem::equivalent` calls `GetFileInformationByHandle`, so the "destination is the source" check compares file identity on Windows too (the run on Windows is `not run`); a probe with 48 busy loops on 24 cores still passed (19.08 s); cancel to end took 803 ms at worst and `Shutdown()` 672 ms at worst over six runs of a 10 s request (limit 2500 ms).
