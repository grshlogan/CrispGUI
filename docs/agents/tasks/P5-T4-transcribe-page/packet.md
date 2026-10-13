# P5-T4 The Transcribe page — packet v1

| Field | Value |
| --- | --- |
| Task / version / date | `P5-T4` / v1 / 2026-10-13 (round 22) |
| Coordinator / Coding / acceptance | A003 / A004 / A003 for Linux (captures and the saved file); the owner for Windows (Phase 5 delivery) |
| Profile | `standard` (UI wiring; the threads and the file are P5-T3's) |
| Starts when | P5-T3 is merged |
| Base and branch | `origin/main` with P5-T3 merged; branch `a004/P5-T4` in `<CODING_ROOT>` |
| Write scope | `src/ui/pages.{h,cpp}`, `src/app/application.{h,cpp}`, `src/util/string_table.cpp` (English and Chinese strings), the line for the new development variable in [ENVIRONMENT.md §2.2](../../../ENVIRONMENT.md#22-commands) and in [tools/dev/README.md](../../../../tools/dev/README.md), this task folder |
| Protected | `src/core/` and `src/platform/` (report a needed change as a decision request), the stub, every other document |
| Read first | [AGENTS.md](../../../../AGENTS.md); [SPEC.md FR-3](../../../SPEC.md#fr-3-transcription-single-file); [CONVENTIONS.md](../../../CONVENTIONS.md) (UI strings, callbacks); [P5-T3's packet](../P5-T3-transcription/packet.md) and receipt; EUI-NEO's `core/platform/platform.h` in the Learn library (`openFileDialog`) |

## Decisions for this page (A003, round 22)

- One file at a time. Several files are the Batch page's job (Phase 7).
- The SRT goes next to the source file. The output-directory setting comes with the Settings page (Phase 8): there is no settings store yet, and EUI-NEO offers no folder picker.
- An existing SRT is replaced only after the user confirms it on the page.
- Files are chosen with EUI-NEO's `openFileDialog`, filtered to the formats FR-3 names: wav, mp3, flac, ogg, m4a, aac, opus, webm, wma. Video containers wait until a Windows check shows that the deployed engine reads them. EUI-NEO has no drag and drop.

## Change

The Transcribe tab replaces its placeholder with a page that, from top to bottom:

- states whether a server is running, with its profile and backend, and otherwise says to start one on the Dashboard;
- offers "Choose file…" and shows the chosen file's path and size and the SRT's destination, with a note and a confirmation when that SRT exists;
- offers Transcribe (enabled when a server runs, a file is chosen, and nothing runs) and, while it runs, Cancel;
- shows the phase, the server's progress as a bar or "unknown", and the elapsed time while it runs;
- shows the result: the destination, the number of cues, and a preview of the first cues; or the failure as a readable message with a suggested action, per the error list of FR-3 ("not running" or "still starting" decided from the server's state).

Under WSLg, `openFileDialog` needs `zenity` or `kdialog`, which may be missing. Add a development variable in the style of `CRISPGUI_DEV_AUTOSTART`, for example `CRISPGUI_DEV_TRANSCRIBE=<file>`, that chooses the file at launch and starts the transcription once a server runs, so that captures and the end-to-end check need no dialog.

## Acceptance

| ID | Criterion | Verification |
| --- | --- | --- |
| AC-1 | End to end under WSLg: with a stub server started from the GUI that answers `tests/data/deployed-parakeet/transcript.verbose.json`, the page writes `<name>.srt` next to the source, byte-equal to `tests/data/deployed-parakeet/transcript.srt` | The development variable and `tools/dev/guirun.sh`; `cmp` |
| AC-2 | Captures at 1100×720 of: no server; a server running and a file chosen; a transcription running with progress; the result with its preview; a failure (for example `400`); and the confirmation for an existing SRT; one capture each at 800×560 and in the Chinese interface. No overlapping or clipped text | Captures read as images |
| AC-3 | Every user-visible text is in the string table in English and Chinese | Reading; `string_table_test` |
| AC-4 | Leaving the page or closing the app during a transcription neither hangs nor writes a partial file | A capture or log, and a check for the destination and temporary files |
| AC-5 | Gates pass | The [standard gates](../README.md#standard-gates) |

## Review and merge

Review `R01` by a fresh Sonnet 5.5 subagent with the prompt of [tasks/README.md](../README.md#review-dispatch); the reviewer reads its own captures. After PASS, merge per [agents/README.md](../../README.md#workflow) and notify A003 with the merged commit.

## Next event

A004 starts after P5-T3 is merged and writes `receipt.md`. The Phase 5 delivery (W5 checks) follows as its own packet.
