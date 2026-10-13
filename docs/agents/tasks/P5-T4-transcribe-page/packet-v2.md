# P5-T4 The Transcribe page — packet v2

v2 replaces [v1](packet.md) (round 23) before A004 started it: it adds three changes to the controller of P5-T3, in answer to its Review (R01 F5 and F6). Everything else of v1 stands and is repeated here.

| Field | Value |
| --- | --- |
| Task / version / date | `P5-T4` / v2 / 2026-10-13 (round 23) |
| Coordinator / Coding / acceptance | A003 / A004 / A003 for Linux (captures and the saved file); the owner for Windows (Phase 5 delivery) |
| Profile | `strict`: the controller changes touch its threads and the written file (A004's request, round 23) |
| Base and branch | `origin/main` with P5-T3 merged; branch `a004/P5-T4` in `<CODING_ROOT>` |
| Write scope | `src/ui/pages.{h,cpp}`, `src/app/application.{h,cpp}`, `src/util/string_table.cpp` (English and Chinese strings), `src/core/transcription.{h,cpp}` and `tests/transcription_test.cpp` for the controller changes below, the line for the new development variable in [ENVIRONMENT.md §2.2](../../../ENVIRONMENT.md#22-commands) and in [tools/dev/README.md](../../../../tools/dev/README.md), this task folder |
| Protected | The rest of `src/core/` and `src/platform/` (report a needed change as a decision request), the stub, every other document |
| Read first | [AGENTS.md](../../../../AGENTS.md); [SPEC.md FR-3](../../../SPEC.md#fr-3-transcription-single-file); [CONVENTIONS.md](../../../CONVENTIONS.md) (UI strings, callbacks); [P5-T3's packet](../P5-T3-transcription/packet.md), receipt, and Review R01; EUI-NEO's `core/platform/platform.h` in the Learn library (`openFileDialog`) |

## Decisions for this page (A003, rounds 22 and 23)

- One file at a time. Several files are the Batch page's job (Phase 7).
- The SRT goes next to the source file. The output-directory setting comes with the Settings page (Phase 8): there is no settings store yet, and EUI-NEO offers no folder picker.
- An existing SRT is replaced only after the user confirms it on the page.
- Files are chosen with EUI-NEO's `openFileDialog`, filtered to the formats FR-3 names: wav, mp3, flac, ogg, m4a, aac, opus, webm, wma. Video containers wait until a Windows check shows that the deployed engine reads them. EUI-NEO has no drag and drop.
- A finished transcription is never lost because its SRT cannot be written: the page offers to retry and to save into the app data folder. No probe file is written to test a folder beforehand, because a virus scanner can leave such a file behind on Windows.

## Controller changes (P5-T3 Review R01 F5 and F6)

- Before any request, a destination that exists and is not a regular file (for example a folder named `<name>.srt`) fails as its own kind, "the destination is not a file".
- When writing the SRT fails after a successful transcription, the state keeps the SRT text with the failure. Two actions then write it with `WriteFileAtomically`: again to the same destination, or to a path the caller gives; the page gives `<app data folder>\output\<name>.srt`. A successful write turns the state into the result, with the path actually written.
- After a failure, the progress reading is unknown, not the last value.

## Page

The Transcribe tab replaces its placeholder with a page that, from top to bottom:

- states whether a server is running, with its profile and backend, and otherwise says to start one on the Dashboard;
- offers "Choose file…" and shows the chosen file's path and size and the SRT's destination, with a note and a confirmation when that SRT exists;
- offers Transcribe (enabled when a server runs, a file is chosen, and nothing runs) and, while it runs, Cancel;
- shows the phase, the server's progress as a bar or "unknown", and the elapsed time while it runs;
- shows the result: the destination, the number of cues, and a preview of the first cues; or the failure as a readable message with a suggested action, per the error list of FR-3 ("not running" or "still starting" decided from the server's state); after a failed write, the two actions above.

Under WSLg, `openFileDialog` needs `zenity` or `kdialog`, which may be missing. Add a development variable in the style of `CRISPGUI_DEV_AUTOSTART`, for example `CRISPGUI_DEV_TRANSCRIBE=<file>`, that chooses the file at launch and starts the transcription once a server runs, so that captures and the end-to-end check need no dialog.

## Acceptance

| ID | Criterion | Verification |
| --- | --- | --- |
| AC-1 | End to end under WSLg: with a stub server started from the GUI that answers `tests/data/deployed-parakeet/transcript.verbose.json`, the page writes `<name>.srt` next to the source, byte-equal to `tests/data/deployed-parakeet/transcript.srt` | The development variable and `tools/dev/guirun.sh`; `cmp` |
| AC-2 | Captures at 1100×720 of: no server; a server running and a file chosen; a transcription running with progress; the result with its preview; a failure (for example `400`); the confirmation for an existing SRT; and a failed write with its two actions; one capture each at 800×560 and in the Chinese interface. No overlapping or clipped text | Captures read as images |
| AC-3 | Every user-visible text is in the string table in English and Chinese | Reading; `string_table_test` |
| AC-4 | Leaving the page or closing the app during a transcription neither hangs nor writes a partial file | A capture or log, and a check for the destination and temporary files |
| AC-5 | The controller changes: a folder as destination fails before any request (the stub counts no request); after a failed write the SRT text is kept, retrying writes it once the cause is gone, and writing to another path works; the progress is unknown after a failure. On Windows, the failed-write case may use a destination held open without delete sharing, as `file_write_test` does | `transcription_test` |
| AC-6 | Mutation matrix, each caught by a test: M1, the pre-check for a destination that is not a file removed; M2, the SRT text dropped after a failed write; M3, the progress kept after a failure; M4, a save action that writes in place instead of through `WriteFileAtomically`; M5, a successful save that leaves the state in the failure, or reports the old path; M6, a save action accepted while a transcription runs | The Review runs each in its sandbox |
| AC-7 | Gates pass, G3 included | The [standard gates](../README.md#standard-gates) |

## Review and merge

Review `R01` by a fresh Sonnet 5.5 subagent with the prompt of [tasks/README.md](../README.md#review-dispatch), given `packet-v2.md`; profile `strict`, so the reviewer runs AC-6's matrix and reads its own captures. After PASS, merge per [agents/README.md](../../README.md#workflow) and notify A003 with the merged commit.

## Next event

A004 starts from the merged P5-T3 and writes `receipt.md`. The Phase 5 delivery (W5 checks) follows as its own packet.
