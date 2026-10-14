# P5-T4 receipt

- Identity, session, model: A004, Claude Code session `0d1710c9-17d2-4343-bd00-ec946bfba62a`, Claude Sonnet 5.5
- Packet: packet-v2.md; profile `strict`
- Base: origin/main `0cbb5d2` on 2026-10-13 (UTC) (P5-T3 merged, A003's packet v2 `32aa24e`, and the final text of Review R01 of P5-T3); branch `a004/P5-T4`
- State: Merged (pushed to `origin/main` as a fast-forward after a PASS in R01, rebased onto `c9097c6`; the commits that add R01's report and this receipt, and the identity context, follow the candidate; the pushed tip cannot be named here, and the notice to A003 and `git log origin/main` carry it)
- Candidate commits: `b3f7f52` (the page and the controller changes) and `74349d554d6d6b37075d1245cf7e6b121cfa3492` (two fixes of the test, found by the matrix). R01 reviewed `74349d5` ([review-R01.md](review-R01.md), PASS with one P2 and six P3 notes); every gate, the sanitizer runs, the matrix (`60` of `63` mutations caught), and the captures below were made on it, apart from the cases noted in the acceptance table that I made on `b3f7f52`, whose GUI code is the same

## Changes

| Path | Why | Criterion |
| --- | --- | --- |
| `src/core/transcription.h` | The phase `kSaving` and the failure `kDestinationNotFile`; `srtKept` and `srt` in the state; `RetryWrite()` and `SaveTo()`; the sentence of P5-T3 R01 F6 on what `progress` means | AC-5, AC-6 |
| `src/core/transcription.cpp` | `CheckDestination` as a function of its own (a destination that is not a file is refused; used before the request, again before the write, and by every save); the outcome keeps the SRT when the destination is refused or the write fails; `RunSave` and `StartSave` (a save thread); the `kSaved` event; `progress` is -1 after a failure | AC-5, AC-6 |
| `tests/transcription_test.cpp` | The cases of AC-5, and the follow-ups of the P5-T3 Review (F1: one attempt after an unreachable server and after a timeout; F2: a pipe as the source; F3: a quiet clock after the end); the timeout case no longer waits for libcurl's connect timeout | AC-5, AC-6 |
| `src/ui/pages.h`, `src/ui/pages.cpp` | `TranscribeState`, `ReplaceConfirmation`, `ComposeTranscribe`, `ComposeReplaceConfirmation` and their parts | AC-1 to AC-4 |
| `src/app/application.h`, `src/app/application.cpp` | The page's state; the file dialog; start, cancel, retry, and save; the replace confirmation; the development variable; `Update()` of the transcription in every frame and its `Shutdown()` before the server's | AC-1, AC-2, AC-4 |
| `src/util/string_table.cpp` | 72 keys in English and Chinese for the page; the placeholder key of the page is gone | AC-3 |
| `docs/ENVIRONMENT.md` (§2.2), `tools/dev/README.md` | The line for `CRISPGUI_DEV_TRANSCRIBE` | the packet |
| `docs/agents/tasks/P5-T4-transcribe-page/receipt.md` | This receipt | — |

`git diff --stat 0cbb5d2..74349d5` lists these paths and nothing else (the receipt as first committed is a skeleton); `src/platform/`, the rest of `src/core/`, the stub, and every other document are untouched.

## What the page does

- Top to bottom: the server (running with profile and backend, or what to do: not running, still starting, stopping), the file (a button, the path and size, where the SRT goes, a note when it exists), the actions, and a status block that appears with the first transcription: the phase, the progress as a bar or "unknown" with the elapsed time while it runs, the result (where it was saved, the cues, the audio length, the preview), or the failure with what to do about it. A kept SRT adds the two buttons, the target of the second, and the cues and preview of the kept result.
- Transcribe is enabled when a server runs, a file is chosen, and nothing runs or saves; Cancel while it runs; Choose file is dimmed while something runs. The server's state decides the words for an unreachable server (not running, still starting, or silent). A cancelled transcription is shown as cancelled, not failed.
- An SRT that exists, as a file, is replaced only after the confirmation dialog; a folder in its place goes straight to the controller, which refuses it. The note "this SRT exists" is not shown for the SRT that a finished transcription has just written.
- `Application::Compose` updates the transcription every frame, whatever page is selected, so leaving the page does not stop anything; `Shutdown()` cancels a running request and finishes a save before the server's own shutdown.
- `CRISPGUI_DEV_TRANSCRIBE=<file>` chooses the file at the first frame, selects the Transcribe page, and starts once a server runs; `choose:<file>` only chooses. (The packet leaves the syntax open: the prefix makes the idle state with a running server capturable.)

## Controller changes (packet v2)

- `CheckDestination(source, destination, replaceExisting)`: nothing there is fine; an entry that is not a regular file fails as `kDestinationNotFile`, with or without permission; the source itself and, without permission, any file fail as `kDestinationExists`. It runs before the request, again before the write, and for every save, so a folder that appears during the work is found too.
- The outcome of a transcription that finished but whose SRT was refused or not written keeps the SRT text, the cues, the duration, and the preview (`srtKept`, `srt`); `Start()` drops them.
- `RetryWrite()` writes the kept SRT again to the destination, under the permission of the request; `SaveTo(path)` writes it to `path`, creating the missing folders, replacing a file there, never the source and never a folder. Both run on a save thread and return false in any state but "failed with a kept SRT" (so not while a transcription runs, not twice at once, not after `Shutdown()`). The state is `kSaving` meanwhile; a success turns it into `kDone` with `destination` set to the path written, `srtKept` false, and progress 100; a failure returns it to `kFailed` with the new reason and the SRT still kept.
- After any failure `progress` is -1. The header says what `progress` means (P5-T3 R01 F6).

Decisions inside the packet's freedom:

- `kSaving` is a phase of its own, so that the page shows "writing" and `Start()`, `Cancel()`, and the save actions can tell a save in progress; a write cannot be cancelled, and `Shutdown()` waits for it.
- The page saves to `<app data folder>/output/<name>.srt` and replaces a file of that name there; the page offers the button only when there is an app data folder.
- A kept SRT stays until another transcription starts; the page says so. The page does not ask before `Start()` drops it.
- The text of the replace dialog has a line break, because the dialog wraps by characters and broke "replaces" in the middle.
- The 800×560 layout is a scroll view, as the dashboard is; the long path lines wrap.

## Acceptance mapping

`<scratchpad>` is `/tmp/claude-1000/-home-claude-projects-CrispGUI-a004/0d1710c9-17d2-4343-bd00-ec946bfba62a/scratchpad`. Line numbers are those of `74349d5`.

| Criterion | Code | Check that fails without the change | Evidence |
| --- | --- | --- | --- |
| AC-1: end to end under WSLg, byte-equal | `Application::StartTranscription` (`application.cpp:137`), the development variable (`:186`), `Transcription` | A GUI run with a stub server that the GUI starts from a profile with `STUB_TRANSCRIPT` set to `tests/data/deployed-parakeet/transcript.verbose.json`: `CRISPGUI_DEV_AUTOSTART=Test/stub CRISPGUI_DEV_TRANSCRIBE=<media>/talk.wav GUI_SIZE=1100x720 tools/dev/guirun.sh <prefix> 4 8`; `cmp <media>/talk.srt tests/data/deployed-parakeet/transcript.srt` exits 0 (184 bytes both) | `observed` (the recipe is below); `sha256` of both files `1ca527e2…`, on the final binary: the `done` run of the captures; `<scratchpad>/cap-final/done-6.png` |
| AC-2: captures | `ComposeTranscribe` (`pages.cpp:815`) and its parts | Each state below, 1100×720 unless said; I read every image | See "Captures" |
| AC-3: every text in the string table, English and Chinese | `src/util/string_table.cpp` | `string_table_test` (sorted, unique, both languages filled) passes with 151 entries (72 added, the placeholder removed). `grep -ohE 'Text\(context, "[a-z_.]+"\|strings_\.Get\("[a-z_.]+"' src/ui/pages.cpp src/app/application.cpp \| sed -E 's/.*"([^"]+)"/\1/' \| sort -u`, each key looked up as `{"<key>",` in the table: 88 keys, none missing; every failure key of `FailureKey` has its `.action`, and the two `.kept` variants exist. The Chinese capture shows no English page text but file names, the server's English detail, and the profile names | `observed`; `string_table_test` in G2 |
| AC-4: leaving the page, closing the app | `Application::Compose` (updates every frame), `Application::Shutdown` | Leaving: the slow profile (9 s), a click on Dashboard at 3.5 s, back at 8.6 s: the page shows Done with elapsed 0:09, and the SRT is byte-equal. Closing: the window closed 5 s into the run: exit 0, the whole scene took 8 s (2 s start, 5 s wait, the close), no `talk.srt` and no `*.tmp` in the media folder. Cancel through the button: "Cancelled", nothing written. The controller's own `Shutdown()` bounds are in `transcription_test` (`:1035`) | `observed`; `<scratchpad>/cap/leave2-9.8.png`, `cancel-7.png` (made on `b3f7f52`) |
| AC-5: the controller changes | `CheckDestination` (`transcription.cpp:114`), `Write` (`:347`), `RunSave` (`:138`), `StartSave` (`:453`), `Update` (`:488`, the `kSaved` case at `:521`) | `tests/transcription_test.cpp`: a folder as destination, with and without permission, fails as `kDestinationNotFile` with no request counted (`:613-626`); the same when the folder appears during the work, with the SRT kept (`:628-640`); an SRT that appears during the work is kept, a retry is refused while it is there and writes once it is gone (`:587-611`); a failed write (a read-only folder; on Windows a destination held open) keeps the SRT with its cues and preview and progress -1, a retry while the cause remains fails again and sends nothing, the retry after the cause is gone writes it, a save elsewhere creates its folders and turns the state into the result with the path written, a save over an existing file replaces it in one step (inode and open reader), a save to a folder or to the source is refused with its reason and keeps the SRT, and no save follows `Shutdown()` (`:776-926`); saves are refused while a transcription runs, when idle, after a success, and after a failure that keeps nothing (`:928-947`); the progress is -1 after a failure that had reported progress (`:949-969`). Follow-ups of P5-T3 R01: `attempt == 1` after an unreachable server (`:492-499`) and after a timeout (`:726-742`), the pipe (`:518-550`), the quiet clock (`:346-350`) | Passes on Linux (G2); the Windows branch is compiled by G4 and `not run` |
| AC-6: mutation matrix, each caught | See the matrix | See the matrix | `<scratchpad>/mut/v2-final.out` |
| AC-7: gates pass, G3 included | — | G1 to G4 on `74349d5` | Commands table |

### Captures

Made under WSLg (`DISPLAY=:0`) with the recipe below on the final binary (`74349d5`). Files are in `<scratchpad>/cap-final/`.

| State | File | What I read |
| --- | --- | --- |
| No server, a file chosen | `nosrv-3.png` | "No server is running. Start one on the Dashboard." in the warning colour; Transcribe dimmed |
| A server running, a file chosen | `idle-5.png` | "Running", profile and backend; the path, size, and SRT; Transcribe enabled, Cancel dimmed |
| A transcription running with progress | `running-5.png` | "Transcribing…", "Progress: 44%" with the bar, "Elapsed: 0:04"; Choose file and Transcribe dimmed, Cancel enabled |
| The result with its preview | `done-6.png` | "Done", "Saved to", "Cues: 1  Audio length: 0:09", the preview line; no "exists" note for the SRT just written |
| A failure (400) | `fail400-6.png` | "The server refused the file." with the action and the engine's message ("failed to decode audio …") |
| The confirmation for an existing SRT | `confirm-6.png` | The dialog "Replace the existing SRT?" over the dimmed page; the message in two lines (without the line break the dialog broke "replaces" into "repl / aces") |
| A failed write with its two actions | `kept-6.png` | "The SRT could not be written." with the action, the English detail ("Could not create the temporary file … Permission denied"), "kept until you start another one", the buttons "Retry writing" and "Save to the app data folder", the save target; the cues line below is cut by the scroll area, as the page scrolls |
| 800×560 | `kept-small-6.png` | The page scrolls; the long paths wrap at the window's width; nothing overlaps |
| The Chinese interface | `kept-zh-7.png` | The same page: tabs, headings, buttons, notes, and the action text in Chinese; the details line stays English by design |
| After "Save to the app data folder" | `save-app-8.png` | "Done", "Saved to: …/appdata/output/talk.srt", "The SRT could not be written next to the file, so it was saved here instead."; `cmp` of the copy with the recorded SRT exits 0 |
| After "Retry writing" (the folder made writable five seconds into the run) | not kept | `cmp` of the SRT next to the source with the recorded SRT exits 0; no copy in the app data folder (made on `b3f7f52`) |
| Cancelled (a click on Cancel five seconds into the 9 s run) | `<scratchpad>/cap/cancel-7.png` (made on `b3f7f52`) | "Cancelled", "The transcription was cancelled. Nothing was written."; no SRT |

Recipe (all paths are examples): a folder that the GUI treats as a CrispASR deployment, with the stub as its server.

```bash
R=<scratch>/root; M=<scratch>/media
mkdir -p $R/bin $R/models $R/profile/Test $M
cp build/linux/tests/stub_crispasr $R/bin/crispasr; : > $R/models/test-model.gguf
cat > $R/profile/Test/stub.conf <<EOT
{"build": "bin", "backend": "stubby", "model": "test-model.gguf", "api_keys": "testkey", "language": "ja", "port": 5114,
 "env": {"STUB_TRANSCRIPT": "$PWD/tests/data/deployed-parakeet/transcript.verbose.json", "STUB_LOAD_MS": "300"}}
EOT
# slow.conf: the same plus "STUB_TRANSCRIBE_MS": "9000" in env
# $M/talk.wav: "RIFF\x24\x00\x00\x00WAVEfmt " followed by any bytes; $M/notes.mp3: a text
CRISPGUI_CRISPASR_ROOT=$R CRISPGUI_DEV_AUTOSTART=Test/stub CRISPGUI_DEV_TRANSCRIBE=$M/talk.wav \
  GUI_SIZE=1100x720 tools/dev/guirun.sh <prefix> 4 8
pkill -f "$R/bin/crispasr"   # the GUI leaves a server it started running, by design
```

For a failed write, `chmod 0555` the folder of the source. For the Chinese capture, click Settings and the segment "中文", then Transcribe; under WSLg `xctl.py click` reaches the controls only after the real pointer was moved there with `XWarpPointer` (tools/dev/README.md). `GUI_SIZE` resizes after one second; if the window is not there yet the helper prints "window not found" and the capture keeps the default size (my 800×560 capture needed two seconds).

## Acceptance of the controller: mutation matrix

Each mutation changes one place of `src/core/transcription.cpp` (or `server_state.h`, `server_manager.cpp` for K1 and K2), builds `transcription_test`, runs it with `ctest`, and restores the file from a saved copy, touching it so that the next build recompiles it (`git status` clean afterwards); caught means exit 8, a failed check. The numbers are the failed checks and the first failing line of `tests/transcription_test.cpp`. The six of the packet (AC-6), with my variants:

| ID | Mutation | Result |
| --- | --- | --- |
| M1 (V1) | The pre-check for a destination that is not a file removed | Caught: 7 checks, from line 620 (20 s) |
| M2 (V2) | The SRT text dropped after a failed write | Caught: 31 checks, from line 813 (20 s) |
| M3 (V3) | The progress kept after a failure | Caught: 1 check, from line 968 (20 s) |
| M4 (V4) | A save action that writes in place instead of through `WriteFileAtomically` | Caught: 2 checks, from line 891 (20 s) |
| M5 (V5a) | A successful save that leaves the state in the failure | Caught: 4 checks, from line 608 (20 s) |
| M5 (V5b) | A successful save that reports the old path | Caught: 2 checks, from line 860 (20 s) |
| M6 (V6) | A save action accepted while a transcription runs (any state) | Caught: 18 checks, from line 820 (18 s) |

My other mutations of the v2 changes:

| ID | Mutation | Result |
| --- | --- | --- |
| V2b | the SRT text dropped when the destination is refused before the write | Caught: 7 checks, from line 596 (20 s) |
| V6b | a save accepted while another one runs (no phase check) | Caught: 5 checks, from line 820 (19 s) |
| V7 | RetryWrite replaces whatever is there | Caught: 4 checks, from line 603 (19 s) |
| V8 | a save does not create the missing folders | Caught: 5 checks, from line 859 (20 s) |
| V9 | a save skips the check of its path | Caught: 8 checks, from line 603 (20 s) |
| V10 | a successful save keeps the SRT | Caught: 2 checks, from line 835 (20 s) |
| V11 | a failed save drops the SRT | Caught: 15 checks, from line 603 (20 s) |
| V12 | no saving phase | Caught: 31 checks, from line 603 (19 s) |
| V14 | a folder is refused only when replacing is allowed | Caught: 2 checks, from line 620 (20 s) |
| V15 | the cues are not kept with a failed write | Caught: 9 checks, from line 341 (20 s) |
| V16 | the failure of a save keeps the old detail | Caught: 2 checks, from line 913 (20 s) |

The matrix of P5-T3, adapted to the changed code (the IDs begin with `T3-`, so that they are not mistaken for the packet's M1 to M6 of v2; `T3-X` is a mutation of my own that P5-T3 numbered):

| ID | Mutation | Result |
| --- | --- | --- |
| T3-M1 | no retry on 500 | Caught: 7 checks, from line 481 (17 s) |
| T3-M2 | retry on 400 | Caught: 1 check, from line 455 (20 s) |
| T3-M2b | retry on 503 | Caught: 1 check, from line 472 (20 s) |
| T3-M2c | retry on 401 | Caught: 1 check, from line 464 (20 s) |
| T3-M2d | retry on an unexpected answer | Caught: 1 check, from line 489 (20 s) |
| T3-M2e | retry twice after 500 | Caught: 3 checks, from line 481 (20 s) |
| T3-M2f | retry when the server is unreachable | Caught: 1 check, from line 498 (20 s) |
| T3-M2g | retry after a timeout | Caught: 1 check, from line 739 (21 s) |
| T3-M3 | SRT written in place | Caught: 3 checks, from line 766 (20 s) |
| T3-M4a | permission to replace never honored | Caught: 9 checks, from line 385 (20 s) |
| T3-M4b | permission to replace always assumed | Caught: 11 checks, from line 570 (20 s) |
| T3-M5 | Shutdown() does not join (wake after it) | Caught: 2 checks, from line 1054 (19 s) |
| T3-M6 | cancel not passed to the request | Caught: 8 checks, from line 1022 (46 s) |
| T3-X1 | destination not checked before the request | Caught: 6 checks, from line 571 (20 s) |
| T3-X2 | destination not checked again before the write | Caught: 9 checks, from line 595 (20 s) |
| T3-X3 | source itself may be replaced | Caught: 5 checks, from line 582 (20 s) |
| T3-X4 | source not checked before the request | Caught: 4 checks, from line 506 (22 s) |
| T3-X4b | upload limit not checked by the controller | Caught: 1 check, from line 559 (19 s) |
| T3-X5 | preview not limited | Caught: 1 check, from line 387 (20 s) |
| T3-X6 | preview without speaker prefix | Caught: 2 checks, from line 374 (20 s) |
| T3-X7 | cues count empty segments | Caught: 4 checks, from line 373 (20 s) |
| T3-X8 | elapsed time not refreshed | Caught: 1 check, from line 435 (20 s) |
| T3-X9 | progress not 100 when done | Caught: 2 checks, from line 340 (20 s) |
| T3-X9b | progress not 100 after a save | Caught: 2 checks, from line 835 (20 s) |
| T3-X10 | start while running accepted | Caught: 14 checks, from line 822 (19 s) |
| T3-X11 | Cancel() does nothing | Caught: 8 checks, from line 719 (29 s) |
| T3-X12 | pause before the retry not interruptible | Caught: 2 checks, from line 719 (39 s) |
| T3-X15 | timeout reported as unreachable | Caught: 1 check, from line 738 (20 s) |
| T3-X16 | 413 reported as bad request | Caught: 1 check, from line 657 (20 s) |
| T3-X17 | Update() never reports a change | Caught: 1 check, from line 345 (20 s) |
| T3-X18 | Update() reports the elapsed time after the end | Caught: 2 checks, from line 349 (20 s) |
| T3-X20 | destination appends .srt | Caught: 45 checks, from line 278 (38 s) |
| T3-X21 | write failure without the message | Caught: 1 check, from line 811 (20 s) |
| T3-X22 | no attempt event | Caught: 17 checks, from line 340 (20 s) |
| T3-X23 | Shutdown() does not cancel | Caught: 5 checks, from line 1050 (37 s) |
| T3-X24 | start after Shutdown accepted | Caught: 2 checks, from line 1057 (18 s) |
| T3-X24b | save after Shutdown accepted | Caught: 1 check, from line 921 (10 s) |
| T3-X25 | a failure during a cancel is not reported as cancelled | Caught: 2 checks, from line 720 (20 s) |
| T3-X26 | cancel not checked before the write | **Not caught** (NOT CAUGHT) |
| T3-X27 | progress ignores the attempt tag | **Not caught** (NOT CAUGHT) |
| T3-X28 | progress taken from an idle server | **Not caught** (NOT CAUGHT) |
| T3-X29 | progress not reset by a new attempt | Caught: 1 check, from line 701 (20 s) |
| T3-X30 | source not looked at before it is opened | Caught: 1 check, from line 539 (22 s) |
| T3-K1 | ApiKey() of the state machine returns the configured key | Caught: 1 check, from line 318 (20 s) |
| T3-K2 | ApiKey() of the manager returns nothing | Caught: 3 checks, from line 1095 (20 s) |

60 of 63 mutations are caught. The ones that survive (X26, X27, X28) are three of the four that P5-T3 disclosed and its Review confirmed: X26 (the cancel flag between the answer and the write, a window of microseconds), X27 (a reading of an earlier attempt, not reproducible with the stub), and X28 (the idle server's progress; the stub reports -1 when idle). The fourth, X30 (the lookup before opening a pipe), is caught now by the pipe case that P5-T3's Review asked for (F2); X24b (a save after `Shutdown()`) and the first version of that pipe case, which hung a mutant instead of failing it, were found by this matrix and fixed in `74349d5`. P5-T3 R01 F4's A20 (the in-flight `/progress` read cancelled before the join) still has no test: it needs a stub that delays `/progress`.

## Commands

All in `<CODING_ROOT>` (`/home/claude/projects/CrispGUI-a004`), Linux/WSL2, `set -o pipefail`; logs in `<scratchpad>/logs/`. The Windows run is `not run`.

| ID | Command | Exit | Evidence |
| --- | --- | --- | --- |
| G1 | `cmake --build build/linux --target clean`, then `cmake -S . -B build/linux -G Ninja -DCMAKE_BUILD_TYPE=Debug && cmake --build build/linux` | 0 | 231 of 231 steps, 0 lines with `warning:` (`P5T4-G1-final.log`) |
| G2 | `ctest --test-dir build/linux --output-on-failure` | 0 | 23 of 23 passed in 30.85 s; `transcription_test` 19.45 s (`P5T4-G2-final.log`) |
| G3 | `ctest --test-dir build/linux -L integration --output-on-failure`, five rounds | 0, 0, 0, 0, 0 | 6 of 6 each round (`P5T4-G3-final-1.log` to `-5.log`) |
| G4 | `cmake --build build/mingw --target clean`, then `cmake -S . -B build/mingw -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/mingw-w64-x86_64.cmake && cmake --build build/mingw` | 0 | 234 of 234 steps, 0 warnings (`P5T4-G4-final.log`) |
| ThreadSanitizer | A tree configured with `-fsanitize=thread`; `setarch x86_64 -R ctest --test-dir build/tsan -R '^transcription_test$'`, three runs | 0, 0, 0 | No `WARNING: ThreadSanitizer` line (`P5T4-tsan-final*.log`) |
| AddressSanitizer, UBSan | The same with `-fsanitize=address,undefined`, leak detection on | 0, 0, 0 | No report (`P5T4-asan-final*.log`) |
| Mutations | `python3 -I <scratchpad>/mut/mutate2.py` (63 mutations, one at a time) | 0 | `<scratchpad>/mut/v2-final.out`; the matrix above |
| String keys | The `grep` of the AC-3 row, each key looked up in `string_table.cpp` | 0 | 88 keys, none missing |
| GUI runs | `tools/dev/guirun.sh`-style runs of the recipe (`<scratchpad>/guiscene.sh`: the same with a two-second start delay and pointer-warped clicks), one per capture, then the AC-1 and AC-4 runs | 0 each | Captures, `cmp` exit 0, exit code of the GUI 0 |
| Markdown | `python3 -I tools/dev/mdcheck.py .` | 0 | 57 files, 0 problems |
| Whitespace and width | `git diff --check` on each commit; the lines I added are within 120 characters, apart from string table entries with CJK text, as the table's older entries are | 0 | — |

## Uncovered, risks, and decisions requested

- `not run`: everything on Windows: the file dialog (`GetOpenFileNameW` blocks the UI thread while it is open), `transcription_test.exe` with the v2 cases (a destination held open without delete sharing, `MoveFileExW`), and its run time (about 19 s on Linux, a dozen stub processes) against the 60 s limit of `run-tests.ps1`. W5 should carry the page and the test.
- Two lines of `pages.cpp` are 121 columns wide; they date from P4 and are not touched here. The string table has long lines of its own, written before this task.
- Not tested by a program: the pixels of the page (read by eye from the captures), the click handlers (one GUI run each of Save, Retry, and Cancel), and the file dialog itself (WSLg has no dialog tool).
- `Start()` drops a kept SRT without asking; the page says so in the kept note. If A003 wants a confirmation first, it is a page change.
- The save target replaces a file of the same name in `<app data>/output` without asking.
- At 800×560 the actions after a failed write lie below the fold of the scroll area; the page does not scroll to them by itself.
- The stub is protected here, so P5-T3 R01 F4's two stub variables (idle progress that is kept, a delayed `/progress`) still wait.

## Review

R01 was an independent top-level session, by the owner's direction. Two dispatches of the Review as a subagent (2026-10-13 and 2026-10-14) met the account's session limit (HTTP 429) before they wrote a report, so neither is a round. On 2026-10-14 the owner directed that R01 be run as a fresh top-level Claude Code session that the owner starts, which is the route of the Skill's Claude Code CLI profile (`agent-dev-loop`, `references/agent-profiles/claude-code-cli.md`); A003 recorded it as the way Reviews run on `origin/main` (`c9097c6`, [tasks/README.md](../README.md#review-dispatch)). The session ("P5-T4 R01 Review", Sonnet 5.5, cwd `/home/claude/projects/CrispGUI-review`) got the fixed prompt, filled: packet `packet-v2.md`, base `0cbb5d2`, candidate `74349d5`, previous report none.

Verdict: PASS ([review-R01.md](review-R01.md), committed unchanged after the owner said the session had ended). It reran G1 to G4 from a fresh export with the same step counts (231, 23 tests, 234; G3 five rounds), reproduced the receipt's numbers for M1 to M6, ran the packet's matrix in 13 variants and 18 mutations of its own (16 caught, one equivalent survivor, one gap in the tests: F3), ran ThreadSanitizer and AddressSanitizer/UBSan without a report, read its own captures of every state AC-2 lists, and drove the GUI through leaving the page, closing the window, Cancel, Retry writing, and Save to the app data folder. It did not rerun my 40 mutations of P5-T3's matrix, so "60 of 63" is `not verified` by it; the numbers for M1 to M6 reproduce exactly. No P0 or P1.

My answers to its notes. A finding that would change the reviewed patch is not applied after a PASS; each waits for the next change of that code or goes to A003.

| Note | Answer |
| --- | --- |
| F1 (P2): a second "Save to the app data folder" replaces the saved copy of the first, when two sources of the same name both fail to write | Agreed; it is the receipt's own open decision. The packet fixes the path `<app data folder>\output\<name>.srt`, so a different name or a question is a change of the page that A003 decides (a counter such as `<name> (2).srt`, chosen when the button is pressed, is the smallest repair). Question to A003 |
| F2 (P3): the confirmation stays open, and a click on Replace does nothing, when the server leaves while it is open | Agreed. `confirmReplace_ = false` before the early returns of `StartTranscription`, or close the dialog when the server leaves Running; at the next change of `application.cpp` |
| F3 (P3): nothing tests that the keys the page asks for exist in the table | Agreed (mutation S2). A test over the literals that `pages.cpp` and `application.cpp` pass as keys, or one shared list; at the next change of those files or of `string_table_test` |
| F4 (P3): as a user who may write into a read-only folder, `transcription_test` prints that the kept-SRT cases are skipped and still passes | Agreed. `SKIP_RETURN_CODE` on the test, with the test returning that code when it skipped them, at the next change of `tests/CMakeLists.txt` and the test |
| F5 (P3): Transcribe is enabled next to a kept SRT, and one click drops the finished result | The receipt's decision for A003: ask first, as for an existing SRT, or keep Transcribe dimmed until the result is retried, saved, or discarded |
| F6 (P3): after a failed write the facts line is cut by the edge of the scroll view, and at 800×560 the two buttons lie below the fold | Agreed; both are in the receipt. The two actions above the detail line, or scrolling them into view when the failure appears; next page change |
| F7 (P3): the comments of `transcription_test.cpp` label cases "AC-4", "AC-5", and "AC-6" with P5-T3's numbers, and one with P5-T4's, so "AC-5" names two criteria | Agreed; the task ID in each label, at the next change of the test |

Environment notes of the report: a stub server from the first Review subagent, which I had stopped, was left running on port 5114 (`<scratchpad>/r1-gui/root/bin/crispasr`, parent process 1) and made the Review's first capture show an adopted server; I ended it afterwards. The Review ran its GUI scenes in a private network namespace for that reason. The recipe of this receipt ends with a `pkill` of the stub the GUI starts; the report suggests a line in `tools/dev/README.md` about a stub left over from an earlier run, which is A003's to decide.
