# P4-T2 receipt

- Identity, session, model: A004, Claude Code session `0d1710c9-17d2-4343-bd00-ec946bfba62a`, Claude Sonnet 5.5
- Packet: packet.md (v1)
- Base: origin/main `9c1f71ef2dca27c8a8b7598cf967bfae80e4a346` (P4-T1 was pushed with the tip `ff662ed` and its code commit `c483964`; `9c1f71e` is documentation only) on 2026-10-13 (UTC); branch `a004/P4-T2`
- State: Merged (pushed to `origin/main` as a fast-forward after a PASS)
- Candidate commits reviewed: R01 `5649b4f99cebeed13ac7c7622d1509b23151f3b5` (PASS, [review-R01.md](review-R01.md)). `origin/main` had not moved past the base when the branch was pushed, so no rebase was needed and the pushed commits contain the candidate itself. The first dispatch of R01 ended with an API error of its subagent ("Can't reach the API server", `EAI_AGAIN`) before it wrote a report; it left no file, process, or repository change behind, and a second fresh subagent produced the one report. The commits that add the report, this receipt, and the identity context follow the candidate; the pushed tip cannot be named here, and the notice to A003 and `git log origin/main` carry it

## Changes

| Path | Why | Criterion |
| --- | --- | --- |
| `src/ui/pages.cpp` | `ComposeDashboard` composes the profile list right after the server panel and the app-data lines after the profiles. The log pane takes only its header and the line that says why it is empty (`ServerLogHeight`: header 34 + gap 6 + two lines of 19 = 78 px) until the server has written a line, and keeps its fixed part of the page (32 % of the page, 140 to 320 px) once it has lines. The header height and the gap of the pane, local constants of `ComposeServerLog`, become file-level constants (`kLogHeaderHeight`, `kLogGap`) that both functions use | AC-1, AC-2, AC-3 |
| `src/ui/pages.h` | The comment of `ComposeDashboard` names the new order | — |
| `tools/windows/CHECKLIST.md` | Step 2 of W4-2 no longer asks the owner to scroll; it says that the Profiles list follows the Server line, that the first profiles are in view at the default size, and that each Start button sits to the right of its profile's name | AC-4 |
| `docs/agents/tasks/P4-T2-dashboard-start/receipt.md` | This receipt | — |

No UI string is new: the two texts that mention "a profile below" (`page.dashboard.body`, `server.hint`) stay true. `git diff --stat` outside `docs` lists the three files above and nothing else; `src/core/`, `src/platform/`, and the server control code are untouched.

## Acceptance mapping

Captures were made with `tools/dev/guirun.sh` under WSLg (Linux), after `GUI_SIZE=<w>x<h>`, and each was read as an image. `<scratchpad>` is `/tmp/claude-1000/-home-claude-projects-CrispGUI-a004/0d1710c9-17d2-4343-bd00-ec946bfba62a/scratchpad`.

| Criterion | Code | Check that fails without the change | Evidence |
| --- | --- | --- | --- |
| AC-1: at 1100×720, stopped, fake deployment: the first screen shows the profile list with the Start button of at least the first ready profile, without scrolling | `ComposeDashboard` (order of `ComposeServer`, `ComposeProfiles`, `ComposeAppData`) and `ServerLogHeight` in `src/ui/pages.cpp` | The base capture `<scratchpad>/captures/base-stopped-3.png` (the code of `ff662ed`, which `9c1f71e` leaves unchanged): below "Profiles: 5  OK: 2" the scrolling area ends, no profile row and no Start button is in view, and the log pane below it is mostly empty | `<scratchpad>/captures/new-stopped-1100-3.png`: the list shows `[ERROR] broken-example` (Start dimmed), `[OK] cohere-asr-ja-q8 <cohere>` with an enabled Start button, and the top of `[MISS] kotoba-whisper-v2.2-q8` |
| AC-2: with a server running from the GUI, the log pane shows the server's lines at its full height and the profile list stays reachable | `ServerLogHeight` returns the full height once `log.Size() > 0` | Mutation L (sandbox, restored): `ServerLogHeight` made to return the compact height whatever the log holds (`if (log.Size() == 0)` → `if (true)`). The running capture `<scratchpad>/captures/mutL-running-1100-8.png` shows a log of 98 lines squeezed into its header and two lines ("stub: line 60", "crispasr-server: listening on 127.0.0.1:5114"); restored (`cmp` against the saved candidate copy: identical) and rebuilt | `<scratchpad>/captures/new-running-1100-8.png`: copy of the fake deployment whose `cuda13/crispasr` is `build/linux/tests/stub_crispasr`, `STUB_LINES=60`, `CRISPGUI_DEV_AUTOSTART=Japanese/cohere-asr-ja-q8`, 8 s after start: Running, "Lines: 98", the pane at 196 px with the newest lines, the "Profiles" and "Japanese" headings in view and the rows reachable by scrolling. Also `new-running-1600-8.png`: "In use" next to `cohere-asr-ja-q8`, three profiles in view. Review R01 adds, from a clean export: the same base capture; mutations M1 (`ComposeAppData` before `ComposeProfiles`, as in the base) and M2 (the empty log kept at its full height), each leaving only `broken-example` in view, restored and recaptured byte-identical; a click on the Start button at (324, 536) that took the server from Stopped to Starting and Running; and the headroom: the button stays fully in view down to a window height of about 670 px |
| AC-3: no overlapping or clipped text at 1100×720 and at 1600×1000 | Same | — | The four captures above (`new-stopped-1100-3.png`, `new-running-1100-8.png`, `new-stopped-1600-3.png`, `new-running-1600-8.png`) show none; the only cut-off text is a row at the edge of the scrolling area, which is scrolling. An extra capture at 500×720, `new-stopped-500-3.png`, shows the empty log's text wrapped to two lines inside its pane, with no overlap, and the `cohere-asr-ja-q8` Start button still in view |
| AC-4: W4-2 no longer tells the owner to scroll and says where the Start buttons are | `tools/windows/CHECKLIST.md`, step 2 of W4-2 | — | Reading |
| AC-5: gates pass | — | — | G1 to G4 in the Commands table |

There is no automated test for this change: no test target links `crisp_ui`, and the layout is EUI-NEO's. The packet names captures as the verification.

## Commands

All run in `<CODING_ROOT>` on Linux (Debian 12 under WSL2, WSLg for the captures) with `set -o pipefail`; the logs are in `<scratchpad>/logs/`. The Linux tree `build/linux` and the MinGW tree `build/mingw` already existed from P4-T1, so G1 and G4 here are incremental; the Review runs them from a clean export.

| Where (alias) | Command | Exit code | Result |
| --- | --- | --- | --- |
| `<CODING_ROOT>` | `git fetch origin`; `git switch --no-track -c a004/P4-T2 origin/main` at `ff662ed`; after A003's `9c1f71e`, `git -c user.name=grshlogan -c user.email=grshlogan@gmail.com rebase origin/main` (no commit of mine yet) | 0; 0; 0 | Branch at `9c1f71e` |
| `<CODING_ROOT>` | Base capture: `GUI_SIZE=1100x720 CRISPGUI_CRISPASR_ROOT=tools/dev/fake-deployment tools/dev/guirun.sh <scratchpad>/captures/base-stopped 3` | 0 | `base-stopped-3.png`, 1100×720 |
| `<CODING_ROOT>` | `cmake --build build/linux` after the edit | 0 | 4 steps, 0 warnings |
| `<CODING_ROOT>` | Captures with the change: stopped at 1100×720 (`new-stopped-1100`, delay 3), 1600×1000 (`new-stopped-1600`, 3), and 500×720 (`new-stopped-500`, 3) with `CRISPGUI_CRISPASR_ROOT=tools/dev/fake-deployment`; running at 1100×720 (`new-running-1100`, delays 4 and 8) and 1600×1000 (`new-running-1600`, 8) with `STUB_LINES=60 CRISPGUI_CRISPASR_ROOT=<scratchpad>/fake-run CRISPGUI_DEV_AUTOSTART=Japanese/cohere-asr-ja-q8` | 0 each | Six images read; see the mapping above |
| `<CODING_ROOT>` | `kill <pid>` of the stub left by each running capture, then `ss -ltn \| grep -c ':5114 '` | 0; the count printed 0 (`grep -c` exits 1 for a count of 0) | Closing the GUI leaves its server running by design (ARCHITECTURE.md §3, "GUI exit"; Phase 3 report); the leftover stub was ended by hand and port 5114 was free before the next run |
| `<CODING_ROOT>` | Mutation L: `sed` the condition of `ServerLogHeight` to `true`, `cmake --build build/linux`, the running capture at 1100×720 (delay 8), `kill` of the leftover stub, restore the saved copy of `src/ui/pages.cpp`, rebuild | build 0, guirun 0, restore build 0 | The mutated build has one compiler warning, the unused parameter `log` that the mutation causes; the capture shows the squeezed log; `cmp` of the restored file against the candidate copy is identical |
| `<CODING_ROOT>` | G1: `cmake -S . -B build/linux -G Ninja -DCMAKE_BUILD_TYPE=Debug …` and `cmake --build build/linux` | 0 | No work left to do after the capture build; 0 warnings |
| `<CODING_ROOT>` | G2: `ctest --test-dir build/linux --output-on-failure` | 0 | 20 of 20 passed |
| `<CODING_ROOT>` | G3: `for i in 1 2 3 4 5; do ctest --test-dir build/linux -L integration --output-on-failure \|\| exit 1; done` | 0 | Five rounds, each 5 of 5 passed |
| `<CODING_ROOT>` | G4: `cmake -S . -B build/mingw -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/mingw-w64-x86_64.cmake …` and `cmake --build build/mingw` | 0 | 45 steps, 0 compiler warnings (`grep -c 'warning:'` is 0). The log has one "CMake Warning" notice, "Manually-specified variables were not used by the project: CMAKE_TOOLCHAIN_FILE", which comes from configuring the existing tree again with the same `-D` flags; a fresh configure (P4-T1's G4) prints none |
| `<CODING_ROOT>` | `python3 -I tools/dev/mdcheck.py .`; `git diff --check` | 0; 0 | 0 problems; no whitespace errors |
| export of `5649b4f` | Review R01, second dispatch (fresh Sonnet 5.5 subagent): G1 to G4 from clean exports, captures at several window sizes in English and Chinese, three mutations, a click test | see [review-R01.md](review-R01.md) | PASS: no P0, P1, or P2 finding, six P3 notes; G1 to G4 exit 0 with no warnings (223 and 226 steps) |
| `<CODING_ROOT>` | `git fetch origin` before the push | 0 | `origin/main` still `9c1f71e`; the push is a fast-forward |

## Review findings and answers

| Round / ID | AGREE / DISAGREE / UNCERTAIN | Change or counter-evidence |
| --- | --- | --- |
| R01 / F1 (P3) | AGREE, not applied | "Follows the Server line directly" in `CHECKLIST.md` step 2 is loose: the Address, port, and hint lines sit between the Server line and the Profiles heading. A change after the PASS would make the merged patch differ from the reviewed one, so I leave the wording to A003, who owns the checklist where no task does: "follows the Server panel" |
| R01 / F2 (P3) | AGREE, not applied | A named constant for the number of reserved lines is a code change; it waits for the next change to this layout |
| R01 / F3 (P3) | AGREE | An observation, no criterion fails: at the minimum window of 800×560 (`src/main.cpp`, `minWindowSize`) the first screen ends at the "Japanese" heading and the first ready Start is fully in view from a window height of about 670 px; the default size has about 50 px of headroom. Passed to A003 for W4-2's result; the packet asks to keep every element, so I changed nothing |
| R01 / F4 (P3) | AGREE | `tools/dev/README.md` names `build/linux/stub_crispasr`; the stub is `build/linux/tests/stub_crispasr`. The file is outside this packet's write scope; I correct that path in [P4-T4](../P4-T4-rename/packet.md), which edits the same file for the artifact names, unless A003 prefers otherwise |
| R01 / F5 (P3) | AGREE | The receipt's base line is tightened above (tip `ff662ed`, code commit `c483964`). The phase report's sentence about where to scroll is A003's to update |
| R01 / F6 (P3) | AGREE | `tools/dev/xctl.py`'s `click` needs a real pointer warp before it reaches EUI-NEO controls under WSLg. Outside this packet and not related to the candidate; passed to A003 |

## Uncovered, skipped, and decisions requested

- The Windows result is `not run`: the owner's W4-2 with the next delivery shows whether the Start buttons are found on the first screen of the real window (the packet's acceptance for Windows).
- The captures are WSLg (Linux) captures with a fake deployment; the Windows window, its fonts, and DPI scaling may lay text out differently. The layout needs no window-size assumption beyond the scroll area and the pane heights, which follow the page height.
- Decision inside the packet's freedom: the empty log pane keeps room for two lines of text, not one, because the pane's height is fixed and the explanation wraps in a window narrower than about 650 px or in a longer translation. The main window's minimum is 800×560 (`src/main.cpp`, `minWindowSize`), where the English text fits one line, so the extra 19 px is blank in every window the application allows; the 500×720 capture is below that minimum and shows the wrapped case.
- The log pane's growth from 78 px to 32 % of the page when the first server line arrives moves the bottom edge of the scrolling area up by about 120 px at the default size; the profile list's position at the top of the area does not change.
- Observed, not changed (outside the packet): closing the GUI leaves a server it started running, as documented; a capture run that starts a server leaves a process behind until it is ended by hand.
