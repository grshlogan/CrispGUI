# P5-T5 A kept SRT is never overwritten or dropped by accident — packet v1

| Field | Value |
| --- | --- |
| Task / version / date | `P5-T5` / v1 / 2026-10-14 (round 25) |
| Coordinator / Coding / acceptance | A003 / A004 / A003 for Linux; the owner for Windows (W5-5 of the Phase 5 delivery) |
| Profile | `strict` (it chooses where a rescued SRT is written, and when one is discarded) |
| Base and branch | `origin/main` when A004 starts; branch `a004/P5-T5` in `<CODING_ROOT>` |
| Write scope | `src/core/transcription.{h,cpp}`, `src/app/application.{h,cpp}`, `src/ui/pages.{h,cpp}`, `src/util/string_table.cpp`, `tests/transcription_test.cpp`, `tests/CMakeLists.txt` (for a skip code), this task folder |
| Protected | Everything else |
| Read first | [AGENTS.md](../../../../AGENTS.md); [P5-T4 packet v2](../P5-T4-transcribe-page/packet-v2.md), receipt, and [Review R01](../P5-T4-transcribe-page/review-R01.md) (findings F1, F2, F4 to F7) |

## Problem

The Review of P5-T4 found (F1, P2) that "Save to the app data folder" for a second source of the same name replaces the SRT rescued earlier, and (F5) that Transcribe stays enabled next to a kept SRT, so one click discards a finished transcription. It also noted a confirmation that outlives a stopped server (F2), kept-SRT cases that report Passed when they were skipped (F4), actions below the fold (F6), and test comments that name the wrong task's criteria (F7).

## Decisions (A003, round 25)

- A rescue never replaces a file: "Save to the app data folder" chooses, when pressed, the first free name among `<name>.srt`, `<name> (2).srt`, `<name> (3).srt`, and so on, in `<app data folder>\output\`, and the page shows the path written.
- While an SRT is kept, Transcribe is disabled. The kept state offers three actions: Retry, Save to the app data folder, and Discard. Discard is the only way to drop a kept SRT, apart from closing the app.
- A confirmation to replace an existing SRT closes when the server stops (F2).
- A test case that cannot run where it runs reports itself as skipped to CTest (`SKIP_RETURN_CODE`), not as passed (F4).
- At 1100×720 the kept state shows its message and its three actions without scrolling (F6).
- Test comments name criteria with their task ID (F7).

## Acceptance

| ID | Criterion | Verification |
| --- | --- | --- |
| AC-1 | Two rescues of sources with the same name write two files, `<name>.srt` and `<name> (2).srt`; an existing file in the output folder is never replaced | `transcription_test` (or the application layer's test, if the name is chosen there) |
| AC-2 | In the kept state Transcribe is disabled; Discard drops the SRT and enables Transcribe; Retry and Save behave as in P5-T4 | `transcription_test`; WSLg captures |
| AC-3 | F2, F4, F6, F7 as decided above | Captures; `ctest` output showing a skip where a case cannot run; reading |
| AC-4 | Mutation matrix, each caught: M1, the rescue writes `<name>.srt` with permission to replace; M2, Transcribe enabled in the kept state; M3, Discard that leaves the SRT kept; M4, the replace confirmation left open after the server stops | The Review runs each in its sandbox |
| AC-5 | Gates pass, G3 included | The [standard gates](../README.md#standard-gates) |

## Review and merge

Review `R01` as a top-level session the owner starts, with the prompt and launch command of [tasks/README.md](../README.md#review-dispatch); profile `strict`. After PASS, merge per [agents/README.md](../../README.md#workflow) and notify A003 with the merged commit.

## Next event

A004 starts the task and writes `receipt.md`. The Phase 5 delivery, [P5-T6](../P5-T6-delivery/packet.md), follows.
