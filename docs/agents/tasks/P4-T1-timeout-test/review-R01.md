# P4-T1 Review R01
- Reviewer: fresh subagent, Claude Sonnet 5.5 (`claude-sonnet-5-5`), dispatched by A004
- Packet: packet.md (v1); candidate `46bb1ea7831b4bc23a51cd0dc440ae32f9f62d1a`; base `a83efc6ad51c757e396e6cf6638b731fc6ffd612`; previous report: none
- Verdict: PASS

Profile `standard`. No P0, P1, or P2 finding; three P3 notes. AC-1 to AC-4 are met with evidence from a clean export. AC-5 (Windows) is outside the Review by the packet and is `not run`.

## Gates rerun

Aliases: `<DIR>` is the export of the candidate in the session scratchpad (`.../scratchpad/r01-export`, made with `git -C <CODING_ROOT> archive 46bb1ea... | tar -x -C <DIR>`, exit 0 under `set -o pipefail`); `<VERIFY>` is a second, untouched export of the same commit (`.../scratchpad/r01-verify`). Platform of every row: Linux 6.18.40.1-microsoft-standard-WSL2 (Debian 12), GCC 12.2.0, CMake 3.25.1, Ninja 1.11.1, MinGW-w64 GCC 12-posix, Bash with `set -o pipefail`, 2026-10-13 UTC. Every cmake configure carries `-DCRISPGUI_EUI_NEO_DIR=/home/claude/projects/Learn/sources/github.com/sudoevolve/EUI-NEO/v0.6.1` and `-DCRISPGUI_FONT_DIR=/home/claude/projects/CrispGUI/fonts`. Mutations edit only `<DIR>/tests/api_client_test.cpp` (candidate blob SHA-256 `2f9fdc80d5fa037bb84c0ad1eac88d3d49ece77ed4865415de9fcc601bc42c0c`), then `cmake --build build/linux --target api_client_test`, then `ctest --test-dir build/linux -R '^api_client_test$' -V`, then restore the saved copy with `cp` (new modification time) and rebuild.

| Command (in `<DIR>`) | Exit code | Result |
| --- | --- | --- |
| G1: `cmake -S . -B build/linux -G Ninja -DCMAKE_BUILD_TYPE=Debug ...` then `cmake --build build/linux` | 0, 0 | Clean tree, 223 steps; no line containing "warning" in the log |
| G2: `ctest --test-dir build/linux --output-on-failure` | 0 | 20 of 20 passed; `api_client_test` 4.43 s |
| `ctest --test-dir build/linux -R '^api_client_test$' -V`, three runs | 0, 0, 0 | Each run: "timeout after 1502 ms" and "cancelled after 1202 ms" |
| G3: `for i in 1 2 3 4 5; do ctest --test-dir build/linux -L integration --output-on-failure \|\| exit 1; done` | 0 | Five rounds, each 5 of 5 passed (10.60 to 10.71 s per round; `api_client_test` 4.42 to 4.43 s) |
| G4: `cmake -S . -B build/mingw -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/mingw-w64-x86_64.cmake ...` then `cmake --build build/mingw` | 0, 0 | Clean tree, 226 steps, 0 warnings; `crisprustgui.exe`, `tests/api_client_test.exe`, and the other test executables link (built, not run) |
| Extra: two rounds of 8 parallel runs of `build/linux/tests/api_client_test`, each in its own working directory | 0 in all 16 | Timeout 1501 to 1502 ms and cancel 1201 to 1202 ms in every run: no timing flake under load |
| M-A (the packet's AC-2 mutation): line 301 `transcribeTimeoutMs = 1000` to `20000` | build 0, ctest 8 | Fails at `:309` (`timeout.kind == ApiErrorKind::kTimeout`) and `:310` (bounds); "timeout after 9002 ms"; restored, `cmp` identical |
| M-A2b: the BASE `tests/api_client_test.cpp` (`a83efc6`) with its limit 1000 to 4000 (libcurl ends at 500 + 4000 = 4500 ms, later than the stub's 3000 ms answer, as WinHTTP's coarse timer did in W4-1) | build 0, ctest 8 | Fails at base `:305` (`timeout.kind == ApiErrorKind::kTimeout`); "timeout after 3002 ms" (W4-1: line 305, 3017 ms); restored, `cmp` identical |
| M-A2c: the candidate with limit 1000 to 4000 | build 0, ctest 0 | Passes; "timeout after 4502 ms" |
| M-A3: the candidate with limit 1000 to 4500 (ends at about 5000 ms, WinHTTP's worst case of 1 s + 4 s) | build 0, ctest 0 | Passes; "timeout after 5001 ms" |
| M-A4: the candidate with limit 1000 to 7500 (ends at about 8000 ms, below the stub's 9000 ms) | build 0, ctest 8 | Fails at `:310` only; "timeout after 8001 ms": the upper bound is a live check |
| M-D1: the candidate with only the stub delay put back to 3000 ms, plus limit 4000 | build 0, ctest 8 | Fails at `:309`; "timeout after 3001 ms": the 9000 ms delay is needed |
| M-D2: the candidate with only the bound put back to `< 4000`, plus limit 4500 | build 0, ctest 8 | Fails at `:310`; "timeout after 5002 ms": the 7500 ms bound is needed |
| M-B: the canceller thread's `cancel.Cancel();` (line 326) replaced by `(void)cancel;` | build 0, ctest 8 | Fails at `:331` (`kCancelled`) and `:332` (`cancelMs < 2500`); "cancelled after 3001 ms" |
| M-E: the cancellation put back on `slow` with the 9000 ms request (lines 312 and 314 to 320 deleted, `slow.Transcribe`, `EndProcess(slowServer)` at the end) | build 0, ctest 0 | Passes; "cancelled after 1202 ms": the own server is not needed for a pass on Linux |
| Lock probe: stub on a free port; `curl -m 1` with `stub_delay_ms=4000` (exit 28), then at once a request with `stub_delay_ms=0` | 28, 0 | The second request was answered after 3000 ms: the stub keeps its model lock after the client gave up |
| Final: `diff -r -q --exclude=build <VERIFY> <DIR>` after all restores | 0 | `<DIR>` is identical to the candidate; test file SHA-256 as above |
| Final: `cmake --build build/linux` and `ctest --test-dir build/linux --output-on-failure` after the last restore | 0, 0 | Nothing to rebuild; 20 of 20 passed |
| `pgrep -x stub_crispasr` after the runs, the failing mutations among them | 1 | No stub process left |
| `git -C <CODING_ROOT> diff --check a83efc6..46bb1ea` | 0 | No whitespace errors |
| `http.h` preprocessed with comments removed (`g++ -fpreprocessed -dD -E`), base against candidate, then `diff` | 1 | Differ by two blank lines only (the two added comment lines): code is unchanged |

## Acceptance

| Criterion | Code | Check and negative evidence | Met |
| --- | --- | --- | --- |
| AC-1: the stub answers the timeout request only after 9000 ms; `kTimeout` with 900 ms <= elapsed < 7500 ms | `tests/api_client_test.cpp:304` (`stub_delay_ms` 9000), `:309` (`kTimeout`), `:310` (`timeoutMs >= 900 && timeoutMs < 7500`), `:311` (prints "timeout after N ms"); the comment `:296-298` cites docs/ENVIRONMENT.md section 4.6 | Linux output "timeout after 1502 ms" (libcurl: 500 ms connect + 1000 ms limit). WinHTTP's late timer emulated with a longer libcurl limit: the base fails (M-A2b: base `:305`, "3002 ms", W4-1's symptom) and the candidate passes (M-A2c "4502 ms"; M-A3 "5001 ms", WinHTTP's worst case). Each of the two numbers is needed alone (M-D1 delay, M-D2 bound) and the upper bound rejects a wait that ends at 8001 ms (M-A4). All restored and rerun | Yes (Linux); Windows is AC-5 |
| AC-2: the check still catches a transport that ignores the limit | Same lines | M-A, the packet's mutation: limit 20000, ctest exit 8, fails at `:309` and `:310`, "timeout after 9002 ms" (the stub's answer arrives); restored, G2 20 of 20 | Yes |
| AC-3: the cancellation keeps its meaning and bound | `:317` to `:319` own `ChildProcess cancelServer` started with `StartServer`; `:320` delay 3000 ms; `:324` to `:327` canceller thread (sleep 300 ms, `cancel.Cancel()`); `:328` `Client(cancelPort).Transcribe(request, transcript, &cancel)`; `:331` `kCancelled`; `:332` `cancelMs < 2500` (the base's bound); `:312` and `:334` end both stubs | Linux output "cancelled after 1202 ms" (the base's own value). M-B: no `Cancel()` call, ctest exit 8, fails at `:331` and `:332`, "cancelled after 3001 ms". The reason for the own server holds: the lock probe shows the stub keeps its lock for the rest of a 4000 ms request after the client left (a following request waited 3000 ms), so on a shared server the cancelled request would queue behind the timed-out one. M-E shows the shared shape also passes on Linux, so the own server is a cleanliness choice inside the packet's freedom, not a correction | Yes |
| AC-4: gates pass | None | G1 to G4 above from clean trees: exit 0 each, 20 of 20, five rounds of 5 of 5, 0 warnings in both builds | Yes |
| AC-5: Windows, `api_client_test` passes | None | Not part of the Review, `not run`. Reading only: ENVIRONMENT.md section 4.6 puts a 1000 ms limit at about 1 s to 5 s on the owner's machine, inside [900, 7500) and below the 9000 ms answer; M-A3 shows the check accepts a wait that ends at 5001 ms | Not assessed (W4-1 of the next delivery decides) |
| Write scope | `git diff --name-status a83efc6..46bb1ea`: A `docs/agents/tasks/P4-T1-timeout-test/receipt.md`, M `src/platform/http.h`, M `tests/api_client_test.cpp` | Inside the packet's write scope. `http.h` is a comment-only change (see the last gate row). Outside `docs`, `git diff --stat` lists exactly the two files, as the receipt says. No transport code, stub, other test, or document outside the task folder changed | Yes |
| AGENTS.md hard rules, CONVENTIONS, error paths, cleanup, threads | The same lines | No engine, profile, download, Windows-drive, or data-location change; the test's scratch folder is unchanged. English text, C++17, camelCase names (`cancelPort`, `cancelServer`), no exceptions, RTTI, or raw `new`/`delete`, no line over 120 columns, no tab or CR, `-Wall -Wextra` clean in both builds. Both stubs are ended by `EndProcess` on every path (the CHECKs do not return early), and none was left after any run, the failing mutations included. The one canceller thread is joined before the block ends, and `HttpCancel` is mutex-guarded (`src/platform/http.h`), unchanged from the base. Commit subject follows `phase-4: ...` and the message ends with the Co-Authored-By trailer | Yes |

## Findings

| ID | Level (P0–P3) | Where | Reproduction, expected, actual | Repair direction |
| --- | --- | --- | --- | --- |
| F1 | P3 | `receipt.md`, Acceptance mapping, AC-1 row | The row says WinHTTP's late timeout "cannot be reproduced on Linux". It can be emulated: the base test file with `transcribeTimeoutMs` 4000 fails at base `:305` ("timeout after 3002 ms", W4-1's symptom), and the candidate with the same change passes ("timeout after 4502 ms"). Expected: AC-1's negative evidence is a check that fails on the base; actual: the receipt gives only the AC-2 mutation | Optional: record this emulation (M-A2b and M-A2c) as the AC-1 evidence. No code change |
| F2 | P3 | `tests/api_client_test.cpp:296-298` and `:310` | The comment explains the 9000 ms delay but not the 7500 ms bound | Optional: add a clause such as "7500 ms stays 2.5 s above WinHTTP's worst case of about 5 s and 1.5 s below the stub's answer" |
| F3 | P3 | Branch topology | `origin/main` is now `c27ad58aaf1cbab2716ba5af6b0dfb24f2491389`, one commit past the base: docs only (STATUS, TASKS, agents/README, tasks/README, phase-4 devlog, A003.md; nothing under `src/`, `tests/`, or the P4-T1 folder). The candidate is not rebased on it. The receipt's "origin/main still a83efc6" was true when it fetched | At merge, rebase onto `origin/main` (workflow step 5). No code changed underneath, so the gates need no rerun for code |

## Not checked, and why

- AC-5 and every Windows behavior: `not run`. WSL2 has no Windows access (AGENTS.md), and `api_client_test.exe` was only compiled and linked by G4. Wine is not installed, and a Wine result would not close a Windows gate anyway. WinHTTP's coarse timer was emulated only through a longer libcurl limit, which tests the check's arithmetic, not WinHTTP itself.
- `clang-format`: not installed; the style was checked by hand (no line over 120 columns, `git diff --check` exit 0).
- ThreadSanitizer and valgrind: not run. The cancellation thread is unchanged in shape from the base.
- `docs/identity/A004.md` is an untracked file in `<CODING_ROOT>`; it is not in the candidate and was not reviewed.
- Documents outside the task folder (ENVIRONMENT.md section 4.6, the phase-4 report, the task index) are left unedited by the candidate, as the packet requires; A003 updates them when it consumes the task.
- Working files: scripts, logs, the parallel-run folders, and the lock probe are in sibling folders of `<DIR>` in the scratchpad (`r01-scripts`, `r01-logs`, `r01-stress`, `r01-verify`, `r01-lockprobe`). This report is the only file written in the repository.
