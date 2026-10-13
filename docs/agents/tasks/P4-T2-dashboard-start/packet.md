# P4-T2 Start buttons on the dashboard's first screen — packet v1

| Field | Value |
| --- | --- |
| Task / version / date | `P4-T2` / v1 / 2026-10-13 (round 13) |
| Coordinator / Coding / acceptance | A003 / A004 / A003 for Linux (captures), the owner for Windows (W4-2 of the next delivery) |
| Profile | `standard` (UI layout; no change to server control) |
| Base and branch | `origin/main` when A004 starts, or `a004/P4-T1` after it merged; branch `a004/P4-T2` in `<CODING_ROOT>` |
| Write scope | `src/ui/pages.cpp` and, if the change needs them, other files under `src/ui/` and UI strings; step 2 of W4-2 in `tools/windows/CHECKLIST.md`; this task folder |
| Protected | `src/core/`, `src/platform/`, the server control behavior, other checks of `CHECKLIST.md`, every other document |
| Read first | [AGENTS.md](../../../../AGENTS.md); [CONVENTIONS.md](../../../CONVENTIONS.md) (UI strings); [SPEC.md](../../../SPEC.md) FR-1 and FR-2; [Phase 4 report, Windows results](../../../devlog/phase-4.md#windows-results-owners-machine); [tools/dev/README.md](../../../../tools/dev/README.md) |

## Problem

At the default window size of 1100×720, the profile list with its Start buttons begins below the visible part of the dashboard's scrolling area (WSLg capture, round 11). The page says "Start the server from a profile below", but the first screen shows no Start button, and on Windows the owner did not find how to start a server (round 11).

Current layout (`src/ui/pages.cpp`, `ComposeDashboard`): a scrolling column with the title, a body line, `ComposeServer`, `ComposeAppData`, and `ComposeProfiles`, in that order, above the server log pane, whose height is `clamp(height × 0.32, 140, 320)` whether or not it has lines.

## Change

The outcome is fixed; the means are A004's choice. A suggested way:

- Put the profile list right after the server panel, and the app-data section after the profiles.
- While the server log has no lines, give the log pane only its header and one line of text; at its full height once lines arrive.

Keep every current element and behavior: the server panel and its Stop, each profile row with its readiness and Start (`disabled` unless the server can be started), the app-data lines, and the log pane.

## Acceptance

| ID | Criterion | Verification |
| --- | --- | --- |
| AC-1 | At 1100×720, with the server stopped and the fake deployment, the first screen shows the profile list with the Start button of at least the first ready profile, without scrolling | Capture: `GUI_SIZE=1100x720 CRISPGUI_CRISPASR_ROOT=tools/dev/fake-deployment tools/dev/guirun.sh <scratch>/stopped 3` |
| AC-2 | With a server running from the GUI, the log pane shows the server's lines at its full height, and the profile list stays reachable | Capture with a copy of the fake deployment whose `cuda13/crispasr` is `build/linux/stub_crispasr` ([tools/dev/README.md](../../../../tools/dev/README.md)) and `CRISPGUI_DEV_AUTOSTART=Japanese/cohere-asr-ja-q8` (the profile's `<folder>/<name>`, [ENVIRONMENT.md §2.2](../../../ENVIRONMENT.md#22-commands)), after the server is Running |
| AC-3 | No overlapping or clipped text at 1100×720 and at 1600×1000 | The captures of AC-1 and AC-2, plus one at 1600×1000 |
| AC-4 | W4-2 in `CHECKLIST.md` no longer tells the owner to scroll, and says where the Start buttons are | Reading |
| AC-5 | Gates pass | G1 to G4 of [P4-T1](../P4-T1-timeout-test/packet.md#gates) |

The captures go into the receipt as file paths under A004's scratchpad; the Review makes its own from its export (run `tools/dev/guirun.sh` from the export after building it).

## Review and merge

Review `R01` by a fresh Sonnet 5.5 subagent with the prompt of [tasks/README.md](../README.md#review-dispatch); the reviewer reads the PNG captures it makes. After PASS, merge per [agents/README.md](../../README.md#workflow) and notify A003 with the merged commit.

## Next event

A004 starts the task after P4-T1, or in parallel on its own branch, and writes `receipt.md`.
