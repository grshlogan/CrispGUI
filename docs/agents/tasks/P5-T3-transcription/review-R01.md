# P5-T3 Review R01

- Reviewer: fresh subagent, Sonnet 5.5, dispatched by A004
- Packet: [packet.md](packet.md) (v1); candidate `e6a060075d1a3ce520fa1fb82c471531e28fb9c4`; base `48b2579396ada52626e4b63b4fa4a032c6577c35`; previous report: none
- Verdict: PASS

Summary: AC-1 to AC-8 are met on Linux, each with a check that fails when the behavior is taken away. G1 to G4 pass (G3 in five rounds). The packet's mutation matrix M1 to M6 is caught, one mutation at a time. Of 39 further mutations of the Review's own, 32 are caught and 7 survive (F1 to F4); four of the seven are the receipt's own disclosed survivors (X26, X27, X28, X30). ThreadSanitizer and AddressSanitizer/UBSan runs of the shipped test and of a 17-scenario probe of the Review report nothing. There is no P0 or P1 finding. Six P3 items are listed under Findings; none blocks the merge. The Windows-only parts are `not run`, as the packet assigns them to the Phase 5 delivery.

Aliases: `<CODING_ROOT>` is `/home/claude/projects/CrispGUI-a004` (read only for this Review; this report is the only file written there). `<SP>` is the scratchpad `/tmp/claude-1000/-home-claude-projects-CrispGUI-a004/0d1710c9-17d2-4343-bd00-ec946bfba62a/scratchpad`. `<DIR>` is `<SP>/rv1-x`, a fresh `git archive` of the candidate, where the gates and the probes ran. `<MUT>` is `<SP>/rv1-m`, a second extraction, the mutation sandbox. `<SAN>` is `<SP>/rv1-s`, a third one, for the sanitizer builds. `<BASE>` is `<SP>/rv1-base`, an extraction of the base. After all experiments `diff -r -q --exclude=build` showed `<DIR>`, `<MUT>`, and `<SAN>` identical to one another, and `src/core/transcription.cpp`, `src/core/transcription.h`, `tests/transcription_test.cpp`, and `tests/stub_crispasr.cpp` of `<DIR>` are `cmp`-identical to `git show e6a0600:<path>`. Scripts are in `<SP>/rv1-scripts/`, logs in `<SP>/rv1-logs/`, mutation logs and `results.jsonl` in `<SP>/rv1-mut/`. The scratchpad also held folders of the coding session (`mut`, `logs`, `export`, `p5t3-r01-e6a0600`, and others); I only listed their names and read or reused nothing from them. Every run was on Linux (Debian 12 under WSL2, kernel 6.18.40.1, GCC 12.2.0, CMake 3.25.1, Ninja 1.11.1, libcurl 7.88.1, user `claude`, no superuser rights). Labels follow AGENTS.md: `verified` is a command run in this Review with its exit code, `observed` an experiment's result, and `hypothesis`, `not run`, and `not verified` as defined there.

The receipt I read is the working-tree file (119 lines added, not committed); the copy committed in `e6a0600` is a 10-line skeleton. Its claims that I checked reproduce: the line numbers of `transcription.cpp` and of the test, the G1 to G4 numbers, the results for M1 to M6, K1, K2, and the survivors X26 to X28 and X30. `git diff b705173..e6a0600` is three lines in `tests/stub_crispasr.cpp` (the counter's name), as the receipt says, so the receipt's matrix on `b705173` and mine on `e6a0600` describe the same code.

## Gates rerun

Each command ran under `set -o pipefail`, and the exit code is the command's own. Every configure line carried `-DCRISPGUI_EUI_NEO_DIR=/home/claude/projects/Learn/sources/github.com/sudoevolve/EUI-NEO/v0.6.1 -DCRISPGUI_FONT_DIR=/home/claude/projects/CrispGUI/fonts`.

| Command (working directory) | Exit code | Result |
| --- | --- | --- |
| `git -C <CODING_ROOT> archive e6a0600… \| tar -x -C <DIR>`, and the same for `<MUT>` and `<SAN>` (the base into `<BASE>`) | 0 | Fresh trees; the identity checks are in the aliases above |
| G1 configure: `cmake -S . -B build/linux -G Ninja -DCMAKE_BUILD_TYPE=Debug …` (`<DIR>`) | 0 | Configured |
| G1 build: `cmake --build build/linux` (`<DIR>`) | 0 | 231 of 231 steps; 0 lines containing `warning:` |
| G2: `ctest --test-dir build/linux --output-on-failure` (`<DIR>`) | 0 | 23 of 23 passed in 30.01 s (17 unit, 6 integration); `transcription_test` 18.70 s |
| G3: `for i in 1 2 3 4 5; do ctest --test-dir build/linux -L integration --output-on-failure \|\| exit 1; done`, run as a loop that wrote one log per round (`<DIR>`) | 0, 0, 0, 0, 0 | 6 of 6 integration tests passed in every round; `transcription_test` 18.78 s in round 5 |
| G4: `cmake -S . -B build/mingw -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/mingw-w64-x86_64.cmake …`, then `cmake --build build/mingw` (`<DIR>`) | 0, 0 | 234 of 234 steps; 0 lines containing `warning`; `build/mingw/tests/transcription_test.exe` is built (3,582,478 bytes) |
| `git -C <CODING_ROOT> diff --check 48b2579..e6a0600` (read only) | 0 | No whitespace errors |
| `awk 'length($0) > 120'` over `git show e6a0600:<path>` of the seven changed source files | 0 | No line over 120 columns |
| The added lines of `git diff 48b2579..e6a0600 -- src tests CMakeLists.txt`, filtered with `grep -nE '\b(throw\|try\|catch\|dynamic_cast\|typeid)\b\|\bnew\b\|\bdelete\b'` | 0 | Four hits: two `= delete` declarations, and the words "new" and "try" in two test comments. No exception, RTTI, raw `new`, or `delete` in the added code |
| `g++ -std=c++17 -fno-exceptions -fno-rtti -Wall -Wextra -Wpedantic -fsyntax-only` on a file that includes only `core/transcription.h` | 0 | The header is self-contained |
| The same flags plus `-Wshadow -Wconversion` over `src/core/transcription.cpp` | none printed | No diagnostics |
| `x86_64-w64-mingw32-ar x …/12-posix/libstdc++.a fs_ops.o`, then `x86_64-w64-mingw32-objdump -dr` on `std::filesystem::equivalent` | 0, 0 | The function calls `_wstat64`, `CreateFileW`, `GetFileInformationByHandle`, and `CloseHandle`: on Windows it compares file identity, not `st_ino`, so the "destination is the source" check does not misfire (`observed` in the object; the run on Windows is `not run`) |
| ThreadSanitizer: `cmake … -DCMAKE_CXX_FLAGS="-fsanitize=thread -g -O1"` in `<SAN>`, then `setarch x86_64 -R ./transcription_test` three times | 0, 0, 0 | No `WARNING: ThreadSanitizer`, no failed check. Without `setarch -R` the runtime ends with exit 66 ("unexpected memory mapping"), a property of this kernel as the receipt says |
| AddressSanitizer, UBSan, LeakSanitizer: `-fsanitize=address,undefined -fno-sanitize=vptr`, `setarch x86_64 -R ./transcription_test` three times | 0, 0, 0 | No report |
| The Review's probe (below) built against each sanitizer tree and run whole | 0, 0 | No report; the cancel storm gave the same 42 cancelled and 6 done in both |
| `./transcription_test` while 48 busy loops ran on 24 cores (`<DIR>`) | 0 | 19.08 s, no failed check |
| `cmp "end-to-end/音声 テスト.srt" tests/data/deployed-parakeet/transcript.srt` in the test's scratch folder (`<DIR>`) | 0 | Identical (both sha256 `1ca527e2…53a`); `find -name '*.tmp'` in the scratch tree found nothing |
| `g++ -fsyntax-only` of the candidate's `tests/transcription_test.cpp` placed in `<BASE>` | 1 | `fatal error: core/transcription.h: No such file or directory`: the test depends on the change |
| Mutation harness (`<SP>/rv1-scripts/mutate.py`) in `<MUT>` | see AC-7 | 46 mutations, one at a time; after the last, `cmp` of the seven touched files, `diff -r -q`, and `./transcription_test` exit 0 |

The ThreadSanitizer tree printed one `-Wformat-truncation` warning in `src/platform/process.cpp:15`, a file the candidate does not touch; it appears only with `-O1` and the sanitizer flags, and G1 at `-O0` has none.

Write scope: `git diff --stat 48b2579..e6a0600` lists ten paths: `CMakeLists.txt`, `docs/agents/tasks/P5-T3-transcription/receipt.md`, `src/core/server_manager.cpp`, `src/core/server_manager.h`, `src/core/server_state.h`, `src/core/transcription.cpp`, `src/core/transcription.h`, `tests/CMakeLists.txt`, `tests/stub_crispasr.cpp`, `tests/transcription_test.cpp`. All are inside the packet's write scope; `src/ui/`, `src/app/`, the transports, the writer, the atomic write, and every document other than the task folder are untouched. The edits to `server_manager.*` and `server_state.h` are the accessor and nothing else (four lines each).

Hard rules and conventions: no downloads, no engine, profile, or script writes (rules 1 to 3). Rule 4 holds: the only file the new code writes is the SRT, through `WriteFileAtomically`, whose temporary file lives next to the destination as FR-3 requires; nothing goes to the system TEMP. Rule 5: the class does not look at the server's state; the page decides (header comment). Rule 6: request fields pass through `fields`; no parameter is named in the code. C++17 only; no exceptions, RTTI, raw `new`, or `delete`; no global or static state (`grep` for `static`, `extern`, `thread_local` finds nothing); `PascalCase` types and functions, `camelCase` fields, `kConstant` names; comments and messages in English, and no UI string in the core (the failure kinds are an enum plus an English detail). The new test carries the `integration` label and starts the stub on free ports, as CONVENTIONS asks.

## Acceptance

| Criterion | Code | Check and negative evidence | Met |
| --- | --- | --- | --- |
| AC-1: the SRT equals `tests/data/deployed-parakeet/transcript.srt` byte for byte | `transcription.cpp:307-332` (`Job::Write`; `WriteFileAtomically` at `:317`), `:351-355` (`TranscriptionDestination`), `:413-451` (`Update`) | `transcription_test.cpp:283-304`: the stub answers the recorded Parakeet `verbose_json`; the source is `音声 テスト.wav`; `Content(state.destination) == sampleSrt`, 1 cue, 9696 ms, the preview is the cue text, attempt 1, progress 100, one request, the folder holds the two files, `wake` and `Update()` fired. `verified` apart from the test: `cmp` of the produced file with the recorded one exits 0. Negative: A1 (an extra line feed after the SRT) fails 8 checks (from `:297`); A2 (destination = source name + `.srt`) fails 30 (from `:236`). On the base the test does not build | Yes |
| AC-2: the state shows the server's progress at least once before the result | `PollProgress` `:275-304` (the value at `:301`), `Update` `:422-427`, `:437-439`, `:445-449` | `:361-390`: a 2 s delay and a 100 ms interval; the progress samples taken while running are non-empty, sorted, within 0 to 100, and rise; the elapsed time passed 1 s while running; the end holds 100. Negative: A3 (no value posted) 3 checks (`:385`, `:387`, `:590`); A3b (`Update` ignores progress) the same 3; A11 (no 100 when done) 2 (`:298`, `:389`). Probe S11 with the packet's 500 ms interval and a 3 s request: the states were -1, 16, 32, 49, 66, 83, with 7 `wake` calls | Yes |
| AC-3: the failures, each with its kind and nothing written | `Request` `:234-260` (only a first-attempt 500 retries, `:253`), `FailureOf` `:40-66`, `CheckSource` `:173-207`, `CheckDestination` `:210-225`, `Write` `:307-332` | `:392-536`: 400 with the server's message and one request; a wrong key; 503 (one request); 500 twice (two requests, attempt 2, the server's message); an answer that is not a transcript; no server; a missing source and a folder as source (nothing sent); a source over a 1000-byte limit, refused before the question about the SRT; an existing SRT without permission, untouched; the source itself; an SRT that appears during the work; a folder in the SRT's place after the request. Also `:538-549` (413 from the server), `:551-564` (500 once, then success, attempt 2, two requests), `:566-592`, `:594-614`, `:616-630`. Every case lists the folder afterwards. Negative: M1 7 checks; M2 1; R503, R401, R2X, RTIMEOUT 1 each; A4a to A4f 1 each; A5 2; A5b 2; A5c 3; A5d 1; A13 1; A14 2. RUNREACH and A5e survive (F1, F2) | Yes |
| AC-4: an existing SRT with permission is replaced atomically | `:317`, `:220` | `:632-662`: an old `talk.srt` and permission to replace; the content equals the recorded SRT, the folder holds the two files, `st_ino` changes, and a reader that held the old file still reads all of its old bytes (the last two are POSIX-only). Negative: M3 (written in place) 3 checks (`:526`, `:654`, `:658`); M4a (permission never honored) 9 (from `:338`); M4b (always assumed) 5 (from `:489`). Probe S15: a failed replace (read-only folder) returns `kCannotWrite`, the old SRT keeps its bytes, no temporary file | Yes on Linux; Windows `not run` |
| AC-5: a cancel returns cancelled within 2500 ms; no destination and no temporary file | `Job::Cancel` `:130-138`, the `HttpCancel` passed at `:248`, members `:339-340` | `:701-726`: a 10 s delay, the cancel once the stub holds the request; `kCancelled` in under 2500 ms, empty detail, attempt 1, the folder holds only the source, the runner starts again. Negative: M6 8 checks (from `:715`; 45.1 s, the stub's delay ran out); A8 (`Cancel()` does nothing) 8; A14 2. Probes: worst cancel-to-end 803 ms and worst `Shutdown()` 672 ms over six runs of a 10 s request (S17); 141 ms worst over 48 cancels at different moments (S1); 20 ms for a cancel during a 300 MB upload (S16) | Yes |
| AC-6: `Shutdown()` returns within 2500 ms; `wake` is not called afterwards | `Transcription::Shutdown` `:400-411`, destructor `:373-375`; the only `wake` call is `Inbox::Post` `:85-93`, reached from the job and progress threads, both joined before `Shutdown()` returns (`:156`, `:409`) | `:728-768`: under 2500 ms; the `wake` count equal at the return and 1500 ms later; `Start()` refused afterwards; the state cancelled; a second `Shutdown()` does nothing; the destructor of a running object under 2500 ms. Negative: M5 2 checks (`:747`, `:753`); A7 (`Shutdown()` does not cancel) 5 (from `:743`); A9b 2. Probe S2: 40 `Shutdown()` calls at different moments, the `wake` count unchanged 80 ms later each time, worst 112 ms; TSan and ASan/UBSan clean | Yes |
| AC-7: the mutation matrix M1 to M6, each caught | See the matrix below | All six caught; each restored and the restoration checked | Yes |
| AC-8: gates pass, G3 included | See Gates rerun | G1 to G4 exit 0; no warnings; G3 five rounds | Yes |
| The API key the manager uses | `server_state.h:174` (`ServerStateMachine::ApiKey()`), `server_manager.h:55`, `server_manager.cpp:636-638` | `transcription_test.cpp:244-277`: a machine that adopted a server returns the configured key, one that spawned returns the report's key; `:770-798`: a manager that adopts the stub returns the configured key, and a transcription with `Status().endpoint` and `ApiKey()` succeeds. Negative: K1 (the machine returns the configured key) 1 check (`:276`); K2 (the manager returns nothing) 3 (from `:788`) | Yes |

### AC-7 mutation matrix

One mutation at a time in `<MUT>` from pristine copies of the touched files; the harness edits, runs `cmake --build build/linux --target transcription_test stub_crispasr`, runs `build/linux/tests/transcription_test` (exit 0 passes, 1 is a failed check, anything else a crash or a time limit), kills any stub it left, and restores the files with a fresh modification time. "Caught" means exit 1 with the listed failing lines of `tests/transcription_test.cpp`. The pristine run before the mutations passed (exit 0, 18.8 s). The numbers are the same as in the receipt.

| ID | Mutation (the packet's) | Result |
| --- | --- | --- |
| M1 | No retry after a 500: `false &&` in front of the condition at `transcription.cpp:253` | Caught, exit 1: 7 checks (`:434`, `:560-563`, `:589`, `:610`) |
| M2 | A retry after a 400: `kBadRequest` added to that condition | Caught, exit 1: 1 check (`:408`, two requests instead of one) |
| M3 | The SRT written in place (open, truncate, write) instead of `WriteFileAtomically` at `:317` | Caught, exit 1: 3 checks (`:526` the folder in the SRT's place no longer fails at the replace step, `:654` the inode does not change, `:658` the old reader sees the new bytes) |
| M4a | The permission to replace never honored (`if (true)` at `:220`) | Caught, exit 1: 9 checks (`:338-340`, `:525-527`, `:648`, `:649`, `:654`) |
| M4b | The permission to replace always assumed (`if (false)` at `:220`) | Caught, exit 1: 5 checks (`:489-491`, `:514`, `:515`) |
| M5 | `Shutdown()` does not join; the destructor does (the job thread's end calls `wake` after `Shutdown()` returned) | Caught, exit 1: 2 checks (`:747`, `:753`: the `wake` count grew) |
| M6 | The cancel not passed to the request (`nullptr` instead of `&transcribeCancel_` at `:248`) | Caught, exit 1: 8 checks (from `:715`); the run took 45.1 s |

### Further mutations of the Review

The same harness; "Survives" is exit 0 with the mutation in place. IDs R503 to RTIMEOUT are retry variants, A1 to A20 outputs, states and guards, K1 and K2 the key.

| ID | Mutation | Result |
| --- | --- | --- |
| R503 | A retry after a 503 | Caught: 1 check (`:425`) |
| R401 | A retry after a 401 | Caught: 1 check (`:417`) |
| RTIMEOUT | A retry after a timeout | Caught: 1 check (`:628`, the elapsed-time bound; see F1) |
| RUNREACH | A retry when the server is unreachable | **Survives** (F1) |
| R2X | A second retry after a 500 | Caught: 1 check (`:434`) |
| A1 | An extra line feed at the end of the SRT | Caught: 8 checks (`:297`, `:329`, `:358`, `:535`, `:562`, `:649`, `:725`, `:796`) |
| A2 | The destination is the source name plus `.srt` | Caught: 30 checks (from `:236`) |
| A3 | Progress events carry no value | Caught: 3 checks (`:385`, `:387`, `:590`) |
| A3b | `Update` ignores progress events | Caught: 3 checks (`:385`, `:387`, `:590`) |
| A4a to A4f | 401 as bad request; 503 as server error; timeout as unreachable; 413 as bad request; protocol as server error; 400 as unexpected answer | Caught: 1 check each (`:416`, `:424`, `:627`, `:547`, `:441`, `:406`) |
| A5 | The destination not checked again before the write | Caught: 2 checks (`:514`, `:515`) |
| A5b | The destination not checked before the request | Caught: 2 checks (`:490`, `:502`) |
| A5c | The source may be replaced by its own transcript | Caught: 3 checks (`:501-503`) |
| A5d | The upload limit not checked by the controller | Caught: 1 check (`:478`, the order of refusals) |
| A5e | The regular-file lookup before the open skipped | **Survives** (F2) |
| A7 | `Shutdown()` does not cancel | Caught: 5 checks (from `:743`); 36.9 s |
| A8 | `Cancel()` does nothing | Caught: 8 checks (from `:609`) |
| A9, A9b | `Start()` accepted while one runs; after `Shutdown()` | Caught: 4 checks (from `:679`); 2 checks (`:750`, `:751`) |
| A10, A10b, A10c | The preview not limited; without the speaker prefix; the cue count includes empty segments | Caught: 1 (`:340`); 2 (`:327`, `:340`); 2 (`:326`, `:339`) |
| A11 | Progress not 100 when done | Caught: 2 checks (`:298`, `:389`) |
| A12 | The elapsed time keeps running after the end | **Survives** (F3) |
| A13 | A write failure reported without its message | Caught: 1 check (`:526`) |
| A14 | A failure after a cancel not reported as cancelled | Caught: 2 checks (`:610`, `:718`) |
| A15 | The cancel flag not checked before the write | **Survives** (F4; the receipt's X26) |
| A16 | The pause before the retry cannot be interrupted | Caught: 2 checks (`:609`, `:610`); 38.4 s |
| A17 | A late reading of an earlier attempt applied | **Survives** (F4; X27) |
| A18 | The progress of an idle server taken | **Survives** (F4; X28) |
| A19 | Progress not reset by a new attempt | Caught: 1 check (`:591`) |
| A20 | The progress read not cancelled before the join | **Survives** (F4) |
| K1, K2 | The machine returns the configured key; the manager returns nothing | Caught: 1 check (`:276`); 3 checks (from `:788`) |

### Exploratory probes

`<SP>/rv1-scripts/probe.cpp`, linked against the candidate's `libcrisp_core.a` and run next to the stub in `<DIR>/build/linux/tests`. It uses the project's own helpers and the `Transcription` interface only. All results are `observed`; the whole probe (S1 to S17) exits 0.

| Probe | Result |
| --- | --- |
| S1: 48 cancels at 0 to 165 ms after `Start()` | 42 cancelled, 6 done, 0 other; the folder held only the source after a cancel and the source plus the SRT after a done, never a `.tmp`; worst cancel-to-end 141 ms |
| S2: 40 `Shutdown()` calls at 0 to 171 ms | The `wake` count unchanged 80 ms after every return; the state cancelled or done; folders tidy; worst 112 ms |
| S3: a named pipe (`mkfifo`) as the source | `kSourceUnreadable` in 20 ms: "…/talk.wav is not a regular file." |
| S4, S5: a symbolic link as the source; a source with mode 000 | The link works and the SRT takes the link's name (`link.srt`); mode 000 gives `kSourceUnreadable` with "Permission denied." |
| S6: the destination is a folder | Without permission `kDestinationExists` before any request; with permission the request is sent (attempt 1) and the job ends with `kCannotWrite`, "Could not replace …/talk.srt with the temporary file …: Is a directory." (F5) |
| S7, S15: a read-only folder, without and with an existing SRT | After a 500 ms request `kCannotWrite`, "Could not create the temporary file …: Permission denied."; the folder is as it was, and the old SRT keeps its bytes (S15); `progress` was still 76 in the failed state (F6) |
| S8: the server is killed 700 ms into a request | `kServerUnreachable` at 720 ms, attempt 1, nothing written, `progress` still 7 (F6) |
| S9: the server closes the connection without an answer | `kServerUnreachable`, attempt 1 |
| S10: the state after the end | `elapsedMs` 5 ms at the end and 5 ms 1.3 s later; `Update()` returned false |
| S11: progress with the packet's defaults | -1, 16, 32, 49, 66, 83; 7 `wake` calls in 3 s |
| S12: odd names | `we"ird 'name'.wav`, `a b  c.tar.gz.wav`, `no_extension`, `trailing.dot.`, `.hidden.wav`, a name with a tab: every one done, with `<name without extension>.srt` (`.hidden.srt`, `trailing.dot.srt`, `no_extension.srt`) |
| S13, S14: attempts for an unreachable server and for a timeout | 1 attempt each (the unreachable case ends in 20 ms) |
| S16: a 300 MB source | A cancel 40 ms after `Start()` ends in 20 ms with `kCancelled` and the folder untouched; a full upload and transcription takes 422 ms on the loopback |
| S17: latency with a 10 s request in flight | Worst cancel-to-end 803 ms and worst `Shutdown()` 672 ms (limit 2500 ms) |

## Findings

| ID | Level (P0–P3) | Where | Reproduction, expected, actual | Repair direction |
| --- | --- | --- | --- | --- |
| F1 | P3 | `tests/transcription_test.cpp:445-452`, `:616-630`; `transcription.cpp:253` | `observed`. Mutation RUNREACH (add `\|\| error.kind == ApiErrorKind::kUnreachable` to the retry condition at `:253`): `transcription_test` still exits 0. Probe S13 against a closed port: the candidate ends with `attempt=1` in 20 ms, the mutant with `attempt=2`. The packet says "a 500 is retried once; nothing else is retried". The test pins that for 400, 401, 503, and an answer that is not a transcript (`attempt == 1` and one request at `:408`, `:417`, `:425`, `:442`), but not for an unreachable server. A retry after a timeout (RTIMEOUT) is caught only by `elapsedMs < 12000` at `:628`, a bound chosen for WinHTTP's late timer, with the mutant ending near 12.05 s | Add `runner.State().attempt == 1` to the no-server case and to the timeout case |
| F2 | P3 | `transcription.cpp:182` (the check before the open); no test | `observed`. Mutation A5e (`:182` to `if (false)`): exit 0. Probe S3 (`mkfifo talk.wav`, then `Start`): the candidate fails in 20 ms; the mutant's job thread blocks in `fopen` on the pipe, `Finish(5000)` fails, and the `Runner`'s destructor, which calls `Shutdown()`, never returns (the probe was ended by `timeout -s KILL`). The receipt lists this as X30. On Linux opening a folder succeeds and `OpenFileSize` refuses it with the same message, so the folder case (`:460-466`) does not pin the check | A Linux-only case: `mkfifo`, `Start`, `Finish(2000)` must give `kSourceUnreadable`; a helper thread that opens the pipe for writing after 3 s frees a mutant, so that the test fails instead of hanging |
| F3 | P3 | `transcription.cpp:445-449`; `transcription.h:71` ("fixed when the transcription ends") | `observed`. Mutation A12 (`kRunning` to `!= kIdle` in `Update`): exit 0. Probe S10: the candidate shows `elapsedMs` 5 ms at the end and 5 ms 1.3 s later with `Update()` false; the mutant shows 20 ms, then 1320 ms, with `Update()` true, so a page would redraw a finished transcription every second | After `Finish`, wait 1.1 s, then check that `Update()` is false and `elapsedMs` is unchanged |
| F4 | P3 | `transcription.cpp:308` (cancel before the write), `:424` (attempt guard), `:301` (busy filter), `:154` (cancel of the in-flight `/progress` before the join) | `observed`. Mutations A15, A17, A18, and A20 each exit 0. The first three are the receipt's X26, X27, X28; A20 is new. Each guard works in the candidate and has no test. The stub cannot show A18 (it reports -1 when idle) or A17 (it needs a reading that arrives after the next attempt began). A18 matters for the real server: docs/API-CRISPASR.md section 7 says `/progress` reads idle "or the previous job" while an upload arrives, `not verified` against a real server. A20 matters only when a `/progress` read hangs at the end of a request: the candidate ends it and joins at once, the mutant waits for the read's timeout | A stub variable that keeps the last progress when idle would pin A18; a delay for `/progress` in the stub would pin A20; A15 and A17 need a seam, and leaving them is reasonable |
| F5 | P3 | `transcription.cpp:210-225` (`CheckDestination`), `:317` | `observed`. An existing destination that is not a regular file passes the check when replacing is allowed: probe S6 (a folder named `talk.srt`) sends the request and fails afterwards with `kCannotWrite`, so the transcript is lost. A read-only folder (S7) does the same; it is the receipt's question for A003, and the packet lists "the SRT cannot be written" as a kind. Nothing is damaged: S15 shows the old SRT intact after a failed replace, and no temporary file stays. `hypothesis`, `not run` on Windows: a destination held open by a player or editor, or a read-only file, would fail `MoveFileExW` in the same place | The answer to the receipt's question is A003's. Cheap part: refuse an existing destination that is not a regular file before the request. For the rest, either keep the SRT text in the state so that the page can offer "save elsewhere", or accept the loss and let the page say so |
| F6 | P3 | `transcription.h:70`; `transcription.cpp:428-441` | `observed`. After a failure `progress` keeps the last reading: S7 shows `kFailed`/`kCannotWrite` with `progress=76`, S8 `kServerUnreachable` with `progress=7`. The header says "the server's progress, 0..100, while it is known; 100 once done". A page that draws a bar whenever `progress >= 0` would show a stale bar on a failed job | Say in the header that `progress` is meaningful only in `kRunning` and `kDone`, or reset it to -1 on failure; P5-T4 should know which |

## Not checked, and why

- Windows behavior is `not run`: `transcription_test.exe` is built by G4 but not executed, so WinHTTP's cancel and its late timeout timer (the timeout case allows 12 s), `MoveFileExW` over a folder (`:519-531`), a destination held open by another program, `std::filesystem::equivalent` at run time (only its object code was read), and the test's run time against the 60 s limit of `tools/windows/run-tests.ps1` (18.7 s on Linux, a dozen stub processes) need the Phase 5 delivery's W5 list. AGENTS.md says Linux results never close a Windows gate.
- A real CrispASR server: the meaning of `/progress` when idle (the previous job's value or -1), its behavior while an upload arrives, and the real 500, 503, and 413 bodies are `not verified`; only the stub and the recorded answers in `tests/data` were used.
- A source on a slow or hung network share: `CheckSource` opens the file on the job thread, and neither `Cancel()` nor `Shutdown()` can interrupt that open (by reading, `hypothesis`; not tested).
- Cancel at the exact moments between the answer and the write (A15) and the late reading of an earlier attempt (A17): not reproducible with the stub; the guards were read and mutated, not exercised.
- The P5-T4 page, the UI thread's real `wake` (`app::requestUpdate`), and the use of `Transcription` from the app are outside this task.
- Sanitizer runs needed `setarch x86_64 -R`; without it ThreadSanitizer stops at start on this kernel. `clang-format`, `valgrind`, and `cppcheck` are not installed, so the style check was by eye plus the 120-column check and the strict `g++` warnings above.
- The coding session's own scripts and logs were not used; the harness, the probe, and every number above are the Review's own.
