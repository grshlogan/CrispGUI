# P4-T1 A timeout check that allows for WinHTTP's coarse timer — packet v1

| Field | Value |
| --- | --- |
| Task / version / date | `P4-T1` / v1 / 2026-10-13 (round 13) |
| Coordinator / Coding / acceptance | A003 / A004 / A003 for Linux, the owner for Windows (W4-1 of the next delivery) |
| Profile | `standard` |
| Base and branch | `origin/main` when A004 starts (record the SHA in the receipt); branch `a004/P4-T1` in `<CODING_ROOT>` |
| Write scope | `tests/api_client_test.cpp`; the timeout comment in `src/platform/http.h` (comment only); this task folder (`receipt.md`, Review reports) |
| Protected | Everything else, in particular the transports' code, the stub, other tests, and every document outside this folder |
| Read first | [AGENTS.md](../../../../AGENTS.md); [CONVENTIONS.md](../../../CONVENTIONS.md); [ENVIRONMENT.md §4.6](../../../ENVIRONMENT.md#46-http-transport-facts); [Phase 4 report, Windows results](../../../devlog/phase-4.md#windows-results-owners-machine) |

## Problem

On the owner's machine, WinHTTP checks request timeouts on a coarse timer, about every 4 s ([ENVIRONMENT.md §4.6](../../../ENVIRONMENT.md#46-http-transport-facts), `observed`): a limit of 1000 ms ends a silent wait somewhere between about 1 s and 5 s after the request starts. The transport's settings themselves take effect.

`api_client_test` checks the timeout with a 1000 ms limit and a stub that answers after 3000 ms (`tests/api_client_test.cpp`, the block after "libcurl counts the connection's budget"). On Windows the answer can arrive before WinHTTP's next check: in W4-1 of `crisprustgui-c296f23` the request returned the answer after 3017 ms, and the check at line 305 failed. On Linux, libcurl ends the same request after connect plus limit, about 1500 ms.

## Change

- Make the stub answer the timeout request later than the limit plus one coarse tick, so that every correct transport times out first: `stub_delay_ms` 9000, and accept a timeout between 900 ms and 7500 ms on both platforms.
- A short comment at the check says why, citing ENVIRONMENT.md §4.6.
- The cancellation check that follows reuses the same `request` and server. The stub holds its model lock for the whole delay of each request, also after the client gave up, so a longer delay changes what the cancelled request waits for. Keep the cancellation's meaning (a cancel 300 ms in returns `kCancelled` within its bound); give it its own request or server if that is cleaner.
- Optional: the timeout comment in `src/platform/http.h` notes that WinHTTP enforces limits on a coarse timer, so a limit may end a wait up to about 4 s late.

## Acceptance

| ID | Criterion | Verification |
| --- | --- | --- |
| AC-1 | The timeout request is answered by the stub only after 9000 ms, and the test requires `ApiErrorKind::kTimeout` with an elapsed time of at least 900 ms and under 7500 ms | Reading; `api_client_test` output line "timeout after N ms" |
| AC-2 | The check still catches a transport that ignores the limit | Negative evidence in a sandbox: with the client's `transcribeTimeoutMs` raised to 20000, `api_client_test` fails at the timeout check; restored, it passes |
| AC-3 | The cancellation check keeps its meaning and bound | Reading; output line "cancelled after N ms" |
| AC-4 | Gates pass | G1 to G4 below |
| AC-5 | Windows: `api_client_test` passes | W4-1 of the next delivery, run by the owner or the Windows-side agent; not part of the Review |

## Gates

Run in `<CODING_ROOT>` by A004 and in the export by the Review, with `set -o pipefail`:

| ID | Command | Expected |
| --- | --- | --- |
| G1 | `cmake -S . -B build/linux -G Ninja -DCMAKE_BUILD_TYPE=Debug && cmake --build build/linux` | Exit 0, no compiler warnings |
| G2 | `ctest --test-dir build/linux --output-on-failure` | Exit 0, 20 of 20 |
| G3 | `for i in 1 2 3 4 5; do ctest --test-dir build/linux -L integration --output-on-failure \|\| exit 1; done` | Exit 0 |
| G4 | `cmake -S . -B build/mingw -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/mingw-w64-x86_64.cmake && cmake --build build/mingw` | Exit 0, no compiler warnings |

In `<CODING_ROOT>` add `-DCRISPGUI_FONT_DIR=/home/claude/projects/CrispGUI/fonts` to the first configure of each tree.

## Review and merge

Review `R01` by a fresh Sonnet 5.5 subagent with the prompt of [tasks/README.md](../README.md#review-dispatch). After PASS, merge per [agents/README.md](../../README.md#workflow) and notify A003 with the merged commit.

## Next event

A004 starts the task and writes `receipt.md` (state `Coding`).
