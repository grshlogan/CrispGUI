# P4-T1 receipt

- Identity, session, model: A004, Claude Code session `0d1710c9-17d2-4343-bd00-ec946bfba62a`, Claude Sonnet 5.5
- Packet: packet.md (v1)
- Base: origin/main `a83efc6ad51c757e396e6cf6638b731fc6ffd612` on 2026-10-13 (UTC); branch `a004/P4-T1`
- State: Merged (pushed to `origin/main` as a fast-forward after a PASS)
- Candidate commits reviewed: R01 `46bb1ea7831b4bc23a51cd0dc440ae32f9f62d1a` (PASS, [review-R01.md](review-R01.md)). Before the push the branch was rebased onto `origin/main` `c27ad58aaf1cbab2716ba5af6b0dfb24f2491389` (documentation only); the candidate became `c483964a9297e2dd3b2563dfec439d176256fef3` with the same patch (`git range-diff` shows `=`; both have `git patch-id --stable` `ba674411ae640fe33eec0e829600e778b8bb9857`). The commits that add the report, this receipt, and the identity context follow it; the pushed tip cannot be named here, and the notice to A003 and `git log origin/main` carry it

## Changes

| Path | Why | Criterion |
| --- | --- | --- |
| `tests/api_client_test.cpp` | The timeout request is answered by the stub after 9000 ms (was 3000 ms) and the test accepts a timeout from 900 ms to under 7500 ms (was under 4000 ms). The cancellation moves to its own stub server and its own delay of 3000 ms, so it no longer queues behind the timed-out request, which keeps the stub's model lock for the whole 9000 ms. The comments say why and cite ENVIRONMENT.md §4.6 | AC-1, AC-3 |
| `src/platform/http.h` | Comment only: `HttpRequest::timeoutMs` notes that WinHTTP enforces limits on a coarse timer, so a limit may end a wait up to about 4 s late | the packet's optional item |
| `docs/agents/tasks/P4-T1-timeout-test/receipt.md`, `review-R01.md` | This receipt; the Review report, committed unchanged | — |
| `docs/identity/A004.md` | A004's own context, created at the owner's instruction in the start prompt. Outside the packet's scope and outside the candidate: it is committed after the PASS and was not reviewed (the Review report says so) | — |

Outside `docs`, `git diff --stat a83efc6 46bb1ea` lists the two code files above and nothing else.

## Acceptance mapping

| Criterion | Code | Check that fails without the change | Evidence |
| --- | --- | --- | --- |
| AC-1: the stub answers after 9000 ms; `kTimeout` with 900 ms ≤ elapsed < 7500 ms | `tests/api_client_test.cpp:304` (`stub_delay_ms` 9000), `:309` (`kTimeout`), `:310` (bounds) | WinHTTP's own late timeout cannot be produced on Linux, but a longer libcurl limit emulates it (Review R01, rows M-A2b and M-A2c): the base test with the client's limit raised from 1000 to 4000 ms (libcurl then ends the wait at 4500 ms, after the stub's 3000 ms answer, as WinHTTP's timer did in W4-1) fails at its line 305 with "timeout after 3002 ms"; the candidate with the same limit passes ("4502 ms"), and passes at WinHTTP's worst case of about 5 s (M-A3, "5001 ms"). Each new number is needed alone: delay back to 3000 ms fails at :309 (M-D1), bound back to 4000 ms fails at :310 (M-D2). The upper bound is live: a wait of 8001 ms fails at :310 (M-A4) | Linux: "timeout after 1502 ms" (libcurl: 500 ms connect + 1000 ms request); the numbers for Windows come with W4-1 |
| AC-2: the check still catches a transport that ignores the limit | Same lines | Mutation A (mine, and the Review's M-A): `shortWait.transcribeTimeoutMs = 1000` → `20000` at `tests/api_client_test.cpp:301`; `ctest -R api_client_test` exit 8, "CHECK failed: timeout.kind == ApiErrorKind::kTimeout" at :309 and "CHECK failed: timeoutMs >= 900 && timeoutMs < 7500" at :310, output "timeout after 9001 ms" (9002 ms in the Review); restored (`cmp` against the saved candidate copy: identical), G2 passes | Commands table, rows M-A and G2 |
| AC-3: the cancellation keeps its meaning and bound | `tests/api_client_test.cpp:319` (own server), `:320` (delay 3000 ms), `:328` (`Client(cancelPort).Transcribe(..., &cancel)`), `:331` (`kCancelled`), `:332` (`cancelMs < 2500`, the base's bound) | Mutation B (mine, and the Review's M-B): the `cancel.Cancel()` call in the canceller thread replaced by `(void)cancel;`; exit 8, "CHECK failed: cancelled.kind == ApiErrorKind::kCancelled" at :331 and "CHECK failed: cancelMs < 2500" at :332, output "cancelled after 3002 ms" (3001 ms in the Review); restored and identical | Linux: "cancelled after 1202 ms" (libcurl notices a cancel in its progress callback about once per second), the base's own value |
| AC-4: gates pass | — | — | G1 to G4 in the Commands table, all exit 0, no warnings; the Review reran them from clean exports with the same result |
| AC-5: Windows, `api_client_test` passes | — | — | `not run`: W4-1 of the next delivery, by the owner or the Windows-side agent; not part of the Review |

## Commands

All run in `<CODING_ROOT>` on Linux (Debian 12 under WSL2) with `set -o pipefail`; the logs are in the session scratchpad under `logs/`. `-DCRISPGUI_FONT_DIR=/home/claude/projects/CrispGUI/fonts` is on the first configure of each tree.

| Where (alias) | Command | Exit code | Result |
| --- | --- | --- | --- |
| `<CODING_ROOT>` | `git fetch origin` (before the branch, before the candidate commit, and before the rebase) | 0 | `origin/main` `a83efc6`, then `c27ad58` |
| `<CODING_ROOT>` | `git switch --no-track -c a004/P4-T1 origin/main` | 0 | Branch created at `a83efc6` |
| `<CODING_ROOT>` | Baseline on `a83efc6`: `cmake -S . -B build/linux -G Ninja -DCMAKE_BUILD_TYPE=Debug …` and `cmake --build build/linux` | 0, 0 | 223 steps, 0 warnings |
| `<CODING_ROOT>` | Baseline: `ctest --test-dir build/linux --output-on-failure` | 0 | 20 of 20 passed; `api_client_test` 4.38 s |
| `<CODING_ROOT>` | M-A: `sed` the limit to 20000 in `tests/api_client_test.cpp`, `cmake --build build/linux --target api_client_test`, `ctest --test-dir build/linux -R '^api_client_test$' --output-on-failure`, then restore the saved copy | build 0, ctest 8 | Fails at :309 and :310; "timeout after 9001 ms"; file restored and identical to the candidate copy |
| `<CODING_ROOT>` | M-B: `sed` the `cancel.Cancel();` call to `(void)cancel;`, same build and ctest, then restore | build 0, ctest 8 | Fails at :331 and :332; "cancelled after 3002 ms"; file restored and identical |
| `<CODING_ROOT>` | G1: `cmake -S . -B build/linux -G Ninja -DCMAKE_BUILD_TYPE=Debug …` and `cmake --build build/linux` | 0 | 35 steps (incremental after the mutations), 0 warnings |
| `<CODING_ROOT>` | G2: `ctest --test-dir build/linux --output-on-failure` | 0 | 20 of 20 passed; `api_client_test` 4.42 s |
| `<CODING_ROOT>` | G3: `for i in 1 2 3 4 5; do ctest --test-dir build/linux -L integration --output-on-failure \|\| exit 1; done` | 0 | Five rounds, each 5 of 5 passed (10.58 to 10.70 s) |
| `<CODING_ROOT>` | G4: `cmake -S . -B build/mingw -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/mingw-w64-x86_64.cmake …` and `cmake --build build/mingw` | 0, 0 | 226 steps from a fresh tree, 0 warnings; `crisprustgui.exe` and the test executables link |
| export of `46bb1ea` | Review R01 (fresh Sonnet 5.5 subagent): G1 to G4 from clean exports, nine mutation rows, a lock probe of the stub, and parallel runs under load | see [review-R01.md](review-R01.md) | PASS: no P0, P1, or P2 finding, three P3 notes; G1 to G4 exit 0, 0 warnings |
| `<CODING_ROOT>` | `git -c user.name=grshlogan -c user.email=grshlogan@gmail.com rebase origin/main` | 0 | One commit replayed onto `c27ad58`; no conflict |
| `<CODING_ROOT>` | `git range-diff a83efc6..46bb1ea origin/main..HEAD`; `git show <commit> \| git patch-id --stable` for both | 0 | `1: 46bb1ea = 1: c483964`; both patch IDs `ba674411ae640fe33eec0e829600e778b8bb9857` |
| `<CODING_ROOT>` | After the rebase: `cmake --build build/linux`; `ctest --test-dir build/linux --output-on-failure` | 0; 0 | 42 steps, 0 warnings; 20 of 20 passed. `c27ad58` changes no file under `src/`, `tests/`, `tools/`, or `cmake/`, so the packet's gates were not rerun in full for the rebase |
| `<CODING_ROOT>` | `python3 -I tools/dev/mdcheck.py .` | 0 | 0 problems |

G1 in my own run was incremental because the baseline tree already existed; the Review ran G1 and G4 from clean exports.

## Review findings and answers

| Round / ID | AGREE / DISAGREE / UNCERTAIN | Change or counter-evidence |
| --- | --- | --- |
| R01 / F1 (P3) | AGREE | The AC-1 row said the late timeout cannot be reproduced on Linux. The Review emulated it with a longer libcurl limit; the row above now carries that evidence. No code change |
| R01 / F2 (P3) | AGREE, not applied | The test comment explains the 9000 ms delay but not the 7500 ms bound. The bound's reasoning is recorded below and in ENVIRONMENT.md §4.6. A change to the comment after the PASS would make the merged patch differ from the reviewed one, so it waits for the next change to this check: "7500 ms stays 2.5 s above WinHTTP's worst case of about 5 s and 1.5 s below the stub's answer" |
| R01 / F3 (P3) | AGREE | Rebased onto `origin/main` `c27ad58` as the workflow says; the patch is unchanged (row above) |

## Uncovered, skipped, and decisions requested

- AC-5 `not run`: the Windows behavior of the new numbers (stub answer after 9000 ms, accepted timeout 900 ms to under 7500 ms) is not measured. The bound follows ENVIRONMENT.md §4.6: a limit of 1000 ms ends a silent wait between about 1 s and 5 s on the owner's machine, which sits inside the window, and the stub's answer comes later than that. W4-1 of the next delivery decides.
- The Windows build of the test is compiled by G4 but not run (`not run`, no Windows access from WSL2).
- Decision taken inside the packet's freedom: the cancellation gets its own stub server rather than a request on the timeout's server. The Review's lock probe confirms that the stub keeps its model lock after the client gave up (a request sent after a timed-out 4000 ms one waited 3000 ms), and its mutation M-E shows that on Linux the shared shape would also pass, because the cancel is client-side; the own server keeps the check independent of the stub's lock. `EndProcess(slowServer)` follows the timeout check so that no request is still running on that stub when the next one starts.
- `docs/identity/A004.md` is part of the pushed range but not of the candidate and not reviewed; it is documentation of A004's own identity.
