# P4-T1 receipt

- Identity, session, model: A004, Claude Code session `0d1710c9-17d2-4343-bd00-ec946bfba62a`, Claude Sonnet 5.5
- Packet: packet.md (v1)
- Base: origin/main `a83efc6ad51c757e396e6cf6638b731fc6ffd612` on 2026-10-13 (UTC); branch `a004/P4-T1`
- State: Review R01 (candidate committed; the Review is dispatched next)
- Candidate commits reviewed: R01 — the SHA is recorded in the commit that adds the report, because a commit cannot contain its own SHA

## Changes

| Path | Why | Criterion |
| --- | --- | --- |
| `tests/api_client_test.cpp` | The timeout request is answered by the stub after 9000 ms (was 3000 ms) and the test accepts a timeout from 900 ms to under 7500 ms (was under 4000 ms). The cancellation moves to its own stub server and its own delay of 3000 ms, so it no longer queues behind the timed-out request, which keeps the stub's model lock for the whole 9000 ms. The comments say why and cite ENVIRONMENT.md §4.6 | AC-1, AC-3 |
| `src/platform/http.h` | Comment only: `HttpRequest::timeoutMs` notes that WinHTTP enforces limits on a coarse timer, so a limit may end a wait up to about 4 s late | the packet's optional item |
| `docs/agents/tasks/P4-T1-timeout-test/receipt.md` | This receipt | — |

No other file changed: `git diff --stat a83efc6 -- . ':!docs'` lists the two files above.

## Acceptance mapping

| Criterion | Code | Check that fails without the change | Evidence |
| --- | --- | --- | --- |
| AC-1: the stub answers after 9000 ms; `kTimeout` with 900 ms ≤ elapsed < 7500 ms | `tests/api_client_test.cpp:304` (`stub_delay_ms` 9000), `:309` (`kTimeout`), `:310` (bounds) | Mutation A (below): with the client's limit at 20000 ms the request is answered after the stub's 9000 ms, and lines 309 and 310 fail. WinHTTP's own late timeout cannot be reproduced on Linux; its check is AC-5 | Linux: "timeout after 1502 ms" (libcurl: 500 ms connect + 1000 ms request); the numbers for Windows come with W4-1 |
| AC-2: the check still catches a transport that ignores the limit | Same lines | Mutation A: `shortWait.transcribeTimeoutMs = 1000` → `20000` in `tests/api_client_test.cpp:301`; `ctest -R api_client_test` exit 8, "CHECK failed: timeout.kind == ApiErrorKind::kTimeout" at :309 and "CHECK failed: timeoutMs >= 900 && timeoutMs < 7500" at :310, output "timeout after 9001 ms"; restored (`cmp` against the saved candidate copy: identical), G2 passes | Commands table, rows M-A and G2 |
| AC-3: the cancellation keeps its meaning and bound | `tests/api_client_test.cpp:319` (own server), `:320` (delay 3000 ms), `:328` (`Client(cancelPort).Transcribe(..., &cancel)`), `:331` (`kCancelled`), `:332` (`cancelMs < 2500`) | Mutation B: the `cancel.Cancel()` call in the canceller thread replaced by `(void)cancel;`; exit 8, "CHECK failed: cancelled.kind == ApiErrorKind::kCancelled" at :331 and "CHECK failed: cancelMs < 2500" at :332, output "cancelled after 3002 ms"; restored and identical | Linux: "cancelled after 1202 ms" (libcurl notices a cancel in its progress callback about once per second) |
| AC-4: gates pass | — | — | G1 to G4 in the Commands table, all exit 0, no warnings |
| AC-5: Windows, `api_client_test` passes | — | — | `not run`: W4-1 of the next delivery, by the owner or the Windows-side agent; not part of the Review |

## Commands

All run in `<CODING_ROOT>` on Linux (Debian 12 under WSL2) with `set -o pipefail`; the logs are in the session scratchpad under `logs/`. `-DCRISPGUI_FONT_DIR=/home/claude/projects/CrispGUI/fonts` is on the first configure of each tree.

| Where (alias) | Command | Exit code | Result |
| --- | --- | --- | --- |
| `<CODING_ROOT>` | `git fetch origin` (before the branch, and again before the candidate commit) | 0 | `origin/main` still `a83efc6` |
| `<CODING_ROOT>` | `git switch --no-track -c a004/P4-T1 origin/main` | 0 | Branch created at `a83efc6` |
| `<CODING_ROOT>` | Baseline on `a83efc6`: `cmake -S . -B build/linux -G Ninja -DCMAKE_BUILD_TYPE=Debug …` and `cmake --build build/linux` | 0, 0 | 223 steps, 0 warnings |
| `<CODING_ROOT>` | Baseline: `ctest --test-dir build/linux --output-on-failure` | 0 | 20 of 20 passed; `api_client_test` 4.38 s |
| `<CODING_ROOT>` | M-A: `sed` the limit to 20000 in `tests/api_client_test.cpp`, `cmake --build build/linux --target api_client_test`, `ctest --test-dir build/linux -R '^api_client_test$' --output-on-failure`, then restore the saved copy | build 0, ctest 8 | Fails at :309 and :310; "timeout after 9001 ms"; file restored and identical to the candidate copy |
| `<CODING_ROOT>` | M-B: `sed` the `cancel.Cancel();` call to `(void)cancel;`, same build and ctest, then restore | build 0, ctest 8 | Fails at :331 and :332; "cancelled after 3002 ms"; file restored and identical |
| `<CODING_ROOT>` | G1: `cmake -S . -B build/linux -G Ninja -DCMAKE_BUILD_TYPE=Debug …` and `cmake --build build/linux` | 0 | 35 steps (incremental after the mutations), 0 warnings |
| `<CODING_ROOT>` | G2: `ctest --test-dir build/linux --output-on-failure` | 0 | 20 of 20 passed; `api_client_test` 4.42 s |
| `<CODING_ROOT>` | G3: `for i in 1 2 3 4 5; do ctest --test-dir build/linux -L integration --output-on-failure \|\| exit 1; done` | 0 | Five rounds, each 5 of 5 passed (10.58 to 10.70 s) |
| `<CODING_ROOT>` | G4: `cmake -S . -B build/mingw -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/mingw-w64-x86_64.cmake …` and `cmake --build build/mingw` | 0, 0 | 226 steps from a fresh tree, 0 warnings; `crisprustgui.exe` and the test executables link |

G1 here is incremental because the baseline tree already existed; the Review runs G1 and G4 from a clean export.

## Review findings and answers

| Round / ID | AGREE / DISAGREE / UNCERTAIN | Change or counter-evidence |
| --- | --- | --- |
| R01 | pending | — |

## Uncovered, skipped, and decisions requested

- AC-5 `not run`: the Windows behavior of the new numbers (stub answer after 9000 ms, accepted timeout 900 ms to under 7500 ms) is not measured. The bound follows ENVIRONMENT.md §4.6: a limit of 1000 ms ends a silent wait between about 1 s and 5 s on the owner's machine, which sits inside the window, and the stub's answer comes later than that. W4-1 of the next delivery decides.
- The Windows build of the test is compiled by G4 but not run (`not run`, no Windows access from WSL2).
- Decision taken inside the packet's freedom: the cancellation gets its own stub server rather than a request on the timeout's server, because the stub keeps its model lock for the whole delay of a request whose client has gone. `EndProcess(slowServer)` follows the timeout check so that no request runs on the stub while the next one starts.
