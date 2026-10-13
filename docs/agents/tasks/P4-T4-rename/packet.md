# P4-T4 The project's names in code and deliveries become CrispGUI — packet v1

| Field | Value |
| --- | --- |
| Task / version / date | `P4-T4` / v1 / 2026-10-13 (round 14) |
| Coordinator / Coding / acceptance | A003 / A004 / A003 for Linux, the owner for Windows (the next delivery) |
| Profile | `standard` |
| Starts when | P4-T1 and P4-T2 are merged (P4-T2 also edits `src/ui/pages.cpp` and `CHECKLIST.md`) |
| Base and branch | `origin/main` with both merged; branch `a004/P4-T4` in `<CODING_ROOT>` |
| Write scope | Code and build: `CMakeLists.txt`, `src/main.cpp`, `src/util/string_table.cpp`, `src/core/server_manager.{h,cpp}` (or `src/core/server_log.h` for the shared constant), `src/ui/pages.cpp`, `src/platform/http_win32.cpp`, `src/platform/multipart.{h,cpp}`, `tests/multipart_test.cpp`, `tests/server_manager_test.cpp`, `tools/api_probe/api_probe.cpp`, `tools/package-windows.sh`, `tools/package-diagnostic.sh`, `tools/smoke/package-eui-smoke.sh`, `tools/dev/guirun.sh`. Documents, only the lines that name the current artifacts: [ENVIRONMENT.md](../../../ENVIRONMENT.md) §2.2 and §5, the note prefix in [ARCHITECTURE.md §2.2](../../../ARCHITECTURE.md#22-servermanager), [CHECKLIST.md](../../../../tools/windows/CHECKLIST.md), [tools/dev/README.md](../../../../tools/dev/README.md), and step 2 of the delivery steps in [agents/README.md](../../README.md#delivery-to-windows). This task folder |
| Protected | Records of the past keep the old name: the rows of earlier deliveries, the phase reports, identity contexts, other task packets, the README's and ARCHITECTURE's notes on the former name, and everything under `reference/`. Everything else outside the write scope |
| Read first | [AGENTS.md](../../../../AGENTS.md); [CONVENTIONS.md](../../../CONVENTIONS.md); the README's note on the name ([README.md](../../../../README.md)) |

## Problem

The project is CrispGUI; it was called CrispRustGUI until 2026-10-13, although it never contained Rust and none is planned (owner, round 14). The documents now use CrispGUI, but the code and the deliveries still carry the old name, and the owner sees it in the window title, the executable, and the delivery folders.

## Change

| What | Now | After |
| --- | --- | --- |
| CMake project | `CrispRustGUI` | `CrispGUI` |
| Executable target | `crisprustgui` (`crisprustgui.exe`) | `crispgui` (`crispgui.exe`) |
| Window title, string `app.title` in both languages | CrispRustGUI | CrispGUI |
| EUI-NEO page ID in `src/main.cpp` | `crisprustgui` | `crispgui` |
| WinHTTP user agent | `CrispRustGUI` | `CrispGUI` |
| Multipart boundary | `CrispRustGUI-` and 32 hexadecimal digits | `CrispGUI-` and 32 hexadecimal digits |
| Notes in the server log | `[CrispRustGUI] ` | `[CrispGUI] ` |
| `api_probe` texts | `CrispRustGUI api_probe` | `CrispGUI api_probe` |
| Delivery and diagnostic archives | `crisprustgui-<sha>`, `crisprustgui-diag-<tool>-<sha>`, `crisprustgui-eui-smoke-<sha>` | `crispgui-…` |
| `BUILD-INFO.txt` texts | `CrispRustGUI …` | `CrispGUI …` |

- `src/core/server_manager.cpp` writes the note prefix and `src/ui/pages.cpp` recognizes it by a second literal. Let both use one constant, so the two cannot drift apart again.
- Check whether EUI-NEO keeps anything under the page ID (window placement, theme, or other state). If it does, record in the receipt what a user loses with the change.
- The app data directory is derived from the executable's folder, not its name, so it does not move; confirm that in the receipt.

## Acceptance

| ID | Criterion | Verification |
| --- | --- | --- |
| AC-1 | No `rust` (any case) remains in `src/`, `tests/`, `tools/`, `cmake/`, or `CMakeLists.txt`; in the documents, every remaining match is a record of an earlier delivery or a past state, or one of the notes on the former name | `git grep -n -i rust -- . ':!reference/**'`; the receipt lists each remaining match and why it stays |
| AC-2 | Clean builds produce `build/linux/crispgui` and `build/mingw/crispgui.exe`, and no `crisprustgui` binary | Clean G1 and G4 of [P4-T1](../P4-T1-timeout-test/packet.md#gates); `find build -name 'crisprustgui*'` prints nothing |
| AC-3 | The window title reads CrispGUI | WSLg capture with `tools/dev/guirun.sh` |
| AC-4 | The tests check the new boundary (prefix `CrispGUI-`, 41 characters) and the new note prefix, and notes in the log pane are still recognized | G2; reading; the capture of a started stub server shows the notes as before |
| AC-5 | The packaging scripts name their archives and texts `crispgui`; their checks for a clean tree and a release build still work | Reading; `bash -n` on each; running each script on a dirty tree refuses as before |
| AC-6 | The documents in the write scope name the new artifacts in the same commit as the code | Reading |
| AC-7 | Gates pass | G1 to G4 of P4-T1 |

## Review and merge

Review `R01` by a fresh Sonnet 5.5 subagent with the prompt of [tasks/README.md](../README.md#review-dispatch). After PASS, merge per [agents/README.md](../../README.md#workflow) and notify A003 with the merged commit.

## Next event

A004 starts after P4-T1 and P4-T2 are merged and writes `receipt.md`.
