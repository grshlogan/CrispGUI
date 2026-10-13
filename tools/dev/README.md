# Development Helpers (WSL2)

Helpers that A003 used for the Linux evidence of Phases 1 to 4, handed over in round 13 so that A004 and its reviewers can repeat those checks. They run only in WSL2, write only where their arguments point, and are not part of any build or delivery.

| Helper | Use |
| --- | --- |
| `guirun.sh <prefix> <delay>...` | Runs `build/linux/crispgui` of this checkout under WSLg, saves window captures `<prefix>-<delay>.png` after the given delays in seconds, closes the window, and prints the exit code. `GUI_SIZE=1100x720` resizes the window first; the GUI's own environment variables pass through ([ENVIRONMENT.md §2.2](../../docs/ENVIRONMENT.md#22-commands)) |
| `xctl.py` | Drives a window found by its process ID through libX11 (`ctypes`, no extra packages): `shot <pid> <png>`, `click <pid> <x> <y>`, `close <pid>`, `resize <pid> <w> <h>` |
| `mdcheck.py <repo-root>` | Checks the project's Markdown files: relative links and anchors, CR characters, trailing spaces, and open-item markers. Exit 0 when clean |
| `fake-deployment/` | A stand-in CrispASR root for `CRISPGUI_CRISPASR_ROOT`: the four Japanese reference profiles (copies of `reference/crispasr/profiles-japanese/`), a broken profile, one-byte stand-ins for the cohere, qwen3, and aligner models, and a placeholder `cuda13/crispasr`. The dashboard lists two profiles as ready, two with missing models, and one parse error |

To start a server from the GUI under WSLg, copy `fake-deployment/` to a scratch folder, replace `cuda13/crispasr` in that copy with `build/linux/tests/stub_crispasr`, and point `CRISPGUI_CRISPASR_ROOT` at the copy; the GUI then starts the stub as the server (as in the Phase 3 checks). Leave the checkout's own copy unchanged.
