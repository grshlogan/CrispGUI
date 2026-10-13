# P4-T4 receipt

- Identity, session, model: A004, Claude Code session `0d1710c9-17d2-4343-bd00-ec946bfba62a`, Claude Sonnet 5.5
- Packet: packet.md (v1)
- Base: origin/main `4a92384` (P4-T1 and P4-T2 pushed; `4a92384` is documentation only) on 2026-10-13 (UTC); branch `a004/P4-T4`. The candidate was written on `7305781` and rebased onto `4a92384` before the Review, which touched none of its files
- State: Review R01 (candidate committed; the Review is dispatched next)
- Candidate commits reviewed: R01 — the SHA is recorded in the commit that adds the report, because a commit cannot contain its own SHA

## Changes

| Path | Why | Criterion |
| --- | --- | --- |
| `CMakeLists.txt` | The project is `CrispGUI` and the executable target `crispgui` (its link, EUI-NEO configuration, compile options, and font dependency) | AC-2 |
| `src/main.cpp`, `src/util/string_table.cpp` | EUI-NEO page ID `crispgui`; `app.title` is "CrispGUI" in both languages | AC-3 |
| `src/core/server_log.h`, `src/core/server_manager.cpp`, `src/ui/pages.cpp` | One constant, `core::kNotePrefix = "[CrispGUI] "`, declared in `server_log.h`: the ServerManager writes the notes with it and the log pane recognizes them by it. The local constant of `server_manager.cpp` is gone, and the pane's literal `"[CrispRustGUI]"` is replaced by the constant, so the two cannot drift apart. The pane's check now includes the space after the bracket, which the ServerManager always writes | AC-4, the packet's single-constant item |
| `src/platform/http_win32.cpp` | WinHTTP user agent `CrispGUI` | — |
| `src/platform/multipart.{h,cpp}` | Boundary `CrispGUI-` and 32 hexadecimal digits (41 characters) | AC-4 |
| `tests/multipart_test.cpp`, `tests/server_manager_test.cpp` | The boundary check reads 41 characters and the prefix `CrispGUI-` (both places); the two note checks read `[CrispGUI] ` as literals, independent of the constant | AC-4 |
| `tools/api_probe/api_probe.cpp` | The probe's report line and the text of the not-audio file (44 bytes now, 48 before; nothing checks its length: `api_probe_test` checks only that the file exists) | — |
| `tools/package-windows.sh`, `tools/package-diagnostic.sh`, `tools/smoke/package-eui-smoke.sh` | Archives `crispgui-…`, `crispgui-diag-<tool>-…`, `crispgui-eui-smoke-…`; the `BUILD-INFO.txt` texts; the script copies `crispgui.exe`. Their checks are unchanged | AC-5 |
| `tools/dev/guirun.sh`, `tools/dev/README.md` | `guirun.sh` runs `build/linux/crispgui`. The README names the same path, and its stub path becomes `build/linux/tests/stub_crispasr` (P4-T2 Review F4, which A003 asked me to fix here) | AC-6 |
| `docs/ENVIRONMENT.md` (lines 63, 67, 179, 180), `docs/ARCHITECTURE.md` (line 87), `docs/agents/README.md` (line 42), `tools/windows/CHECKLIST.md` (line 20) | The names of the current artifacts, in the same commit as the code | AC-6 |
| `docs/agents/tasks/P4-T4-rename/receipt.md` | This receipt | — |

`git diff --stat` lists these 21 files and the receipt; the Windows and Linux sources outside the list are untouched.

Two facts the packet asked me to record:

- EUI-NEO keeps nothing under the page ID. v0.6.1 uses it as the prefix of element IDs inside the running process (`core/dsl.h:1568` and `:1590`) and writes no file or registry entry keyed by it: a search of `core/` and `include/` for file and registry writes finds only network downloads (`core/platform/network.cpp`). A user loses nothing with the change (`observed` in the source, not on Windows).
- The app data directory does not move: `DefaultAppDataRoot()` in `src/app/app_data.cpp:9-12` returns the executable's folder plus `appdata`, whatever the executable's name.

## Acceptance mapping

`<scratchpad>` is `/tmp/claude-1000/-home-claude-projects-CrispGUI-a004/0d1710c9-17d2-4343-bd00-ec946bfba62a/scratchpad`.

| Criterion | Code | Check that fails without the change | Evidence |
| --- | --- | --- | --- |
| AC-1: no `rust` (any case) in `src/`, `tests/`, `tools/`, `cmake/`, or `CMakeLists.txt`; every remaining document match is a record or a note on the former name | The files in Changes | `git grep -n -i rust` printed 31 lines under those paths on the base (`7305781`; 107 lines over the whole tree without `reference/`) and prints none under them now; the only match outside `docs/` is `README.md:7`, the note on the former name | The remaining matches and why they stay: `README.md:7` (the note on the former name); `docs/ARCHITECTURE.md:249` ("Rust GUIs", a rejected alternative) and `:259` (the language row: no Rust now or planned, "Rust glue", and the note on the former name); `docs/ENVIRONMENT.md:162` (the name of the diagnostic delivery of 2026-10-12); `docs/TASKS.md:23, 28, 40, 50` and `docs/agents/README.md:31-37` (records of the seven earlier deliveries); `docs/devlog/phase-0.md` to `phase-4.md` (1, 6, 4, 6, and 5 matches: reports of past phases); `docs/identity/A003.md` (13: A003's records); in `docs/agents/tasks/`: the P4-T1 packet, receipt, and review-R01, the P4-T2 review-R01, the P4-T3 packet-v2, the P4-T4 packet, and the P4-T5 packet, which name the old names as requirements, records, or past commands; and this receipt, which names the old names to describe the change |
| AC-2: clean builds produce `build/linux/crispgui` and `build/mingw/crispgui.exe` and no `crisprustgui` binary | `CMakeLists.txt` | `rm -rf build/linux build/mingw`, then G1 and G4 from fresh trees; `find build -name 'crisprustgui*'` printed nothing | `build/linux/crispgui` (24,199,568 bytes) and `build/mingw/crispgui.exe` (3,982,336 bytes) |
| AC-3: the window title reads CrispGUI | `src/util/string_table.cpp`, `src/main.cpp` | The base string was "CrispRustGUI" (`git show 7305781:src/util/string_table.cpp`) | A running `build/linux/crispgui` under WSLg: `_NET_WM_NAME='CrispGUI - 0 FPS'` and `WM_NAME` the same, read with a throwaway script that uses libX11 (EUI-NEO appends the frame rate to the title). A capture, `<scratchpad>/captures/t4-stopped-3.png`, made with `tools/dev/guirun.sh`, shows the dashboard; the title bar is not part of a capture |
| AC-4: the tests check the new boundary and the new note prefix, and the notes in the log pane are still recognized | `src/core/server_log.h` (`kNotePrefix`), `src/ui/pages.cpp:464`, `src/platform/multipart.cpp`, the two tests | Mutation M1: the constant back to `"[CrispRustGUI] "`: `server_manager_test` exits 8 with two failed checks (`tests/server_manager_test.cpp:128` and `:142`). Mutation M2: the boundary prefix back to `"CrispRustGUI-"`: `multipart_test` exits 8 with two failed checks (`:106`, `:110`). Mutation M3: the pane's check back to the old literal, as the two-literal drift the packet names: the capture shows the notes in the normal color, not the accent color. Each was restored (`cmp` against a saved copy: identical) | `<scratchpad>/captures/t4-log-top.png` (GUI-started stub server, `STUB_LINES=4`, the log scrolled to its top with real pointer events): four `[CrispGUI] …` notes ("Starting Japanese/cohere-asr-ja-q8", "Working directory", "Environment", "Started process") in the accent color, then the stub's own lines in the normal color. `<scratchpad>/captures/t4-M3-log-top.png`: the same notes in the normal color under M3 |
| AC-5: the packaging scripts name their archives and texts `crispgui`; their clean-tree and release-build checks still work | The three scripts | `git diff` of the scripts shows only the names changing; `bash -n` on each script and on `guirun.sh` exits 0; each of the three scripts, run with an output folder in the scratchpad on this dirty tree, prints "the working tree is not clean" and exits 1, and writes nothing | Commands table. The check for a commit that is not on `origin/main` and the release-build check are in code that the change does not touch; a full packaging run is P4-T3's |
| AC-6: the documents in the write scope name the new artifacts in the same commit | Changes table | — | Reading: `git diff -U0` of the five documents shows the artifact names changing and nothing else |
| AC-7: gates pass | — | — | G1 to G4 in the Commands table |

## Commands

All run in `<CODING_ROOT>` on Linux (Debian 12 under WSL2, WSLg for the captures) with `set -o pipefail`; the logs are in `<scratchpad>/logs/`. `-DCRISPGUI_FONT_DIR=/home/claude/projects/CrispGUI/fonts` is on the first configure of each tree.

| Where (alias) | Command | Exit code | Result |
| --- | --- | --- | --- |
| `<CODING_ROOT>` | `git fetch origin`; `git switch --no-track -c a004/P4-T4 origin/main` at `7305781` | 0; 0 | Branch created; `4a92384` appeared upstream while I worked (documentation only, no overlap with my files) |
| `<CODING_ROOT>` | `rm -rf build/linux build/mingw` | 0 | Build output only |
| `<CODING_ROOT>` | G1: `cmake -S . -B build/linux -G Ninja -DCMAKE_BUILD_TYPE=Debug …` and `cmake --build build/linux` | 0, 0 | Fresh tree, 223 steps, 0 compiler warnings, 0 CMake warnings |
| `<CODING_ROOT>` | G4: `cmake -S . -B build/mingw -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/mingw-w64-x86_64.cmake …` and `cmake --build build/mingw` | 0, 0 | Fresh tree, 226 steps, 0 compiler warnings, 0 CMake warnings; `crispgui.exe` links |
| `<CODING_ROOT>` | G2: `ctest --test-dir build/linux --output-on-failure` | 0 | 20 of 20 passed; again after the mutations were restored: 20 of 20 |
| `<CODING_ROOT>` | G3: `for i in 1 2 3 4 5; do ctest --test-dir build/linux -L integration --output-on-failure \|\| exit 1; done` | 0 | Five rounds, each 5 of 5 passed |
| `<CODING_ROOT>` | `ls -la build/linux/crispgui build/mingw/crispgui.exe`; `find build -name 'crisprustgui*'` | 0; 0 | Both artifacts exist; the search printed nothing |
| `<CODING_ROOT>` | `build/linux/crispgui` with `CRISPGUI_CRISPASR_ROOT=tools/dev/fake-deployment`, then `python3 -I <scratchpad>/scripts/wintitle.py <pid>` and `tools/dev/xctl.py close <pid>` | 0 | `_NET_WM_NAME='CrispGUI - 0 FPS'`, `WM_NAME` the same; the GUI exited with 0 |
| `<CODING_ROOT>` | `GUI_SIZE=1100x720 CRISPGUI_CRISPASR_ROOT=tools/dev/fake-deployment tools/dev/guirun.sh <scratchpad>/captures/t4-stopped 3` | 0 | `t4-stopped-3.png` |
| `<CODING_ROOT>` | The GUI starts the stub server (`<scratchpad>/fake-run`, a copy of the fake deployment whose `cuda13/crispasr` is `build/linux/tests/stub_crispasr`; `STUB_LINES=4`, `CRISPGUI_DEV_AUTOSTART=Japanese/cohere-asr-ja-q8`), 8 s after start `<scratchpad>/scripts/xdrive.py` warps the pointer over the log, sends 40 wheel-up notches, and saves `t4-log-top.png`; then `xctl.py close`, `kill` of the leftover stub (closing the GUI leaves its server running by design), and `ss -ltn \| grep -c ':5114 '` | 0; the count printed 0 | The notes are recognized (mapping above); port 5114 was free before and after |
| `<CODING_ROOT>` | M1, M2, M3 as in the mapping: `sed` the line, build the target, run `ctest` (M1, M2) or the capture (M3), restore the saved copy, `cmp` | M1 and M2: build 0, ctest 8; M3: build 0, capture 0 | M1, M2: the checks named above fail; M3: the notes lose the accent color; every file restored and identical; the tree rebuilt without warnings |
| `<CODING_ROOT>` | `bash -n` on `tools/package-windows.sh`, `tools/package-diagnostic.sh`, `tools/smoke/package-eui-smoke.sh`, `tools/dev/guirun.sh` | 0 each | Syntax is fine |
| `<CODING_ROOT>` | `tools/package-windows.sh <scratchpad>/pkg-out`; `tools/package-diagnostic.sh winhttp_timeouts <scratchpad>/pkg-out`; `tools/smoke/package-eui-smoke.sh <scratchpad>/pkg-out`, all on the dirty tree | 1 each | "the working tree is not clean"; `<scratchpad>/pkg-out` was not created |
| `<CODING_ROOT>` | `python3 -I tools/dev/mdcheck.py .`; `git diff --check` | 0; 0 | 0 problems; no whitespace errors |

## Review findings and answers

| Round / ID | AGREE / DISAGREE / UNCERTAIN | Change or counter-evidence |
| --- | --- | --- |
| — | — | — |

## Uncovered, skipped, and decisions requested

- The Windows side is `not run`: the new executable, window title, user agent, and archive names appear on Windows only with the next delivery ([P4-T3](../P4-T3-delivery/packet-v2.md)). The title was read on Linux under WSLg; the WinHTTP user agent and the Windows executable name are compiled by G4 and not run.
- A full run of the packaging scripts is not part of this task. Their refusal on a dirty tree was run; their refusal for a commit that is not on `origin/main` and for a non-release build is code this change does not touch.
- Decision inside the packet's freedom: the shared constant is in `src/core/server_log.h`, the header both the ServerManager and the UI already include, rather than in `server_manager.h`.
- The note check in the log pane now needs the space after the bracket. A line of the server's own output that starts with `[CrispGUI]` and no space is not taken for a note, which the old check would have done.
- Past deliveries and records keep the old names, as the packet requires: the seven earlier deliveries in `<HANDOFF_OUT>` and their rows are untouched.
