# P5-T1 receipt

- Identity, session, model: A004, Claude Code session `0d1710c9-17d2-4343-bd00-ec946bfba62a`, Claude Sonnet 5.5
- Packet: packet.md (v1)
- Base: origin/main `1268807` (the packet's commit) on 2026-10-13 (UTC); branch `a004/P5-T1`
- State: Review R01 (candidate committed; the Review is dispatched next)
- Candidate commits reviewed: R01 — the SHA is recorded in the commit that adds the report, because a commit cannot contain its own SHA

## Changes

| Path | Why | Criterion |
| --- | --- | --- |
| `src/core/srt_writer.h`, `src/core/srt_writer.cpp` (new) | `SegmentsToSrt(const std::vector<TranscriptSegment>&)` writes one cue per segment with non-empty text; `SrtTimestamp(std::int64_t)` formats `HH:MM:SS,mmm`; `SrtSpeakerPrefix(const std::string&)` gives the text before a cue's words for a speaker label. A pure function on `core/transcript.h`: no file, network, or global state | AC-1, AC-2 |
| `CMakeLists.txt` | `src/core/srt_writer.cpp` joins `crisp_core` | AC-4 |
| `tests/srt_writer_test.cpp` (new), `tests/CMakeLists.txt` | The test, registered as a unit test; the delivery's unit tests grow from 15 to 16 and its integration tests stay at 5 | AC-1, AC-2, AC-4 |
| `tests/data/README.md` | The words of `transcript.verbose.json` are "one per kana or kanji with the punctuation attached to the word before it"; the row of `transcript.srt` names `srt_writer_test` as its reader and says it compares the file byte for byte | AC-3 |
| `tests/transcript_test.cpp` | The comment above `CheckRecorded` says the same about the words, and now sits above the function; `ExpectedWord` has a comment of its own | AC-3 |
| `src/core/transcript.h` | The comment on `durationMs`: for a window request the part of the window inside the file, which is less than the window's length when the file ends first | AC-3 |
| `docs/agents/tasks/P5-T1-srt-writer/receipt.md` | This receipt | — |

`git diff --stat` plus the three new files lists these paths and nothing else. The recorded answers in `tests/data/deployed-parakeet/`, the parser in `src/core/transcript.cpp`, and every document outside this folder and `tests/data/README.md` are untouched.

Decisions inside the packet's freedom (the format itself is the packet's):

- The speaker prefix inverts `normalise_speaker`: `A` to `Z` give `(speaker 0) ` to `(speaker 25) `, and two letters whose first is at least `B` give `(speaker N) ` with N = (first − `A`) × 26 + (second − `A`), from `BA` (26) to `ZZ` (675), as the server writes them. A two-letter label that starts with `A` (such as `AB`) cannot come from a number and is treated as any other label. Any other label, such as a participant's name, gets `(<label>) `, the shape of the server's raw labels for claimed participants (`(Mustermann) `); no label gives no prefix. Diarization is not in v1, so no recorded answer carries a label.
- A negative time, which the parser can return for odd input but a server never sends, is written as `00:00:00,000` instead of a malformed `-` field.
- Times are written exactly from whole milliseconds. The server's own cues are in centiseconds, so for its answers the values agree (230 ms is `00:00:00,230`).
- A segment whose text is only spaces is a cue, as in the server (`seg.text.empty()` is the only test). The text is not trimmed.
- The code is written from the packet's description of the format, not adapted from `crispasr_output.cpp`, so no CrispASR notice is carried; the header names the function the format follows.

## Acceptance mapping

`<scratchpad>` is `/tmp/claude-1000/-home-claude-projects-CrispGUI-a004/0d1710c9-17d2-4343-bd00-ec946bfba62a/scratchpad`. Mutations M1 to M11 change `src/core/srt_writer.cpp` in place and are restored from a saved copy (`cmp` identical); D1 to D3 change the build's copies of the recorded files (`build/linux/tests/testdata/deployed-parakeet/`), never the sources.

| Criterion | Code | Check that fails without the change | Evidence |
| --- | --- | --- | --- |
| AC-1: `transcript.verbose.json`, parsed by `ParseVerboseJson` and written by the writer, equals `transcript.srt` byte for byte | `tests/srt_writer_test.cpp:57`; `SegmentsToSrt` in `src/core/srt_writer.cpp:56` | D1: one digit of a time in the build's copy of `transcript.srt` (`08,710` → `08,711`): ctest exit 8, line 57 fails. D2: the final empty line removed from the copy: exit 8, line 57. D3: the segment's `end` `8.71` → `8.72` in the build's copy of `transcript.verbose.json`: exit 8, line 57. Each restored (`cmp` identical) and passing again | The test passes on the first run: the writer's output for the recorded answer is the server's `srt`, 184 bytes |
| AC-2: rule cases: several segments numbered 1 to n; an empty-text segment skipped without a gap; times across hour and minute boundaries (3,723,450 ms is `01:02:03,450`) and above 99 hours; speaker `B` gives `(speaker 1) `; no segments give an empty string; `window.verbose.json` gives one cue `00:00:05,070 --> 00:00:08,710` | `tests/srt_writer_test.cpp`: numbering `:72-81` (also no CR and no byte order mark, `:79-80`); skipped segments `:82-91`; no segments `:92`; text kept as it is `:93-97`; times `:99-119` (`:110` is 3,723,450 ms, `:113` is 100 hours, `:115` is 10,000 hours, `:117-119` negative times); speaker prefixes `:121-137` (`:122` is `B`, `:130` is `BA`); the window answer `:60-68` | One mutation per rule, each caught (ctest exit 8, build 0): M1 numbering from 0 (10 checks fail); M2 empty-text segments not skipped (3); M3 numbering by position, a gap after a skipped segment (2); M4a to M4d one digit fewer for hours, minutes, seconds, and two for the milliseconds (22, 24, 23, 19); M4e a dot before the milliseconds (27); M5a hours capped at 99 (3); M5b hours wrapping at 24 (4); M6a no space after the prefix (8); M6b speaker numbers from 1 (4); M6c 25 letters in the two-letter rule (4); M6d unknown labels written bare (5); M7 no segments give a line feed (2); M8 no empty line after a cue (10); M9 trailing spaces trimmed (2); M10 negative times not clamped (3); M11 a shorter arrow (10) | `<scratchpad>/logs/P5T1-mutations.log` holds the lines above; every mutation applied (the script checks that the file changed) |
| AC-3: the three follow-ups say what the packet asks | `tests/data/README.md` (the `transcript.verbose.json` and `transcript.srt` rows), `tests/transcript_test.cpp` (above `CheckRecorded`), `src/core/transcript.h` (the comment on `durationMs`) | — | Reading: the words are "one per kana or kanji with the punctuation attached to the word before it"; a window's `duration` is the part of the window inside the file; the README names `srt_writer_test` as the reader of `transcript.srt` |
| AC-4: gates pass; the delivery's test count grows by the new test only | — | — | G1 to G4 in the Commands table; `ctest -N -L unit` lists 16 tests (15 before) and `-L integration` 5; the MinGW build holds 22 `.exe` files in `build/mingw/tests/` (21 tests and the stub, one more than before) |

## Commands

All run in `<CODING_ROOT>` on Linux (Debian 12 under WSL2) with `set -o pipefail`; the logs are in `<scratchpad>/logs/`. `-DCRISPGUI_FONT_DIR=/home/claude/projects/CrispGUI/fonts` is on the first configure of each tree.

| Where (alias) | Command | Exit code | Result |
| --- | --- | --- | --- |
| `<CODING_ROOT>` | `git fetch origin`; `git switch --no-track -c a004/P5-T1 origin/main` at `1268807` | 0; 0 | Branch created |
| `<CODING_ROOT>` | `cmake --build build/linux`; `ctest --test-dir build/linux -R '^srt_writer_test$' --output-on-failure` | 0; 0 | 0 warnings; the new test passed on its first run |
| `<CODING_ROOT>` | `bash <scratchpad>/scripts/p5t1-mutate.sh`: for each of M1 to M11 `sed` one line of `src/core/srt_writer.cpp`, `cmake --build build/linux --target srt_writer_test`, `ctest -R '^srt_writer_test$'`; then restore the saved copy and `cmp` | build 0 and ctest 8 for each of the 19 mutations | See the mapping; restored and identical |
| `<CODING_ROOT>` | D1 to D3: change the build's copy of `transcript.srt` or `transcript.verbose.json`, `ctest -R '^srt_writer_test$'`, restore with `cp`, `cmp` | 8 each | Line 57 fails each time; restored and identical; afterwards the test passes |
| `<CODING_ROOT>` | G1: `cmake -S . -B build/linux -G Ninja -DCMAKE_BUILD_TYPE=Debug …` and `cmake --build build/linux` | 0 | Incremental after the rebuild, 0 compiler warnings, 0 CMake warnings |
| `<CODING_ROOT>` | G2: `ctest --test-dir build/linux --output-on-failure` | 0 | 21 of 21 passed (20 before, plus `srt_writer_test`) |
| `<CODING_ROOT>` | G3: `for i in 1 2 3 4 5; do ctest --test-dir build/linux -L integration --output-on-failure \|\| exit 1; done` | 0 | Five rounds, each 5 of 5 passed |
| `<CODING_ROOT>` | G4: `cmake --build build/mingw` on the tree configured with the toolchain file in P4-T4 (CMake re-ran for the new sources) | 0 | 35 steps, 0 compiler warnings; `build/mingw/tests/srt_writer_test.exe` exists; 22 `.exe` files in `build/mingw/tests/`. The configure line of G4 is not repeated on this existing tree, which would only print the "CMAKE_TOOLCHAIN_FILE not used" notice; the Review runs both lines from a clean export |
| `<CODING_ROOT>` | `python3 -I tools/dev/mdcheck.py .`; `git diff --check` (with the new files added as intent-to-add); `awk` for lines over 120 columns in the changed and new files | 0; 0; none new | 0 problems; no whitespace errors; one line of 124 columns in `CMakeLists.txt:127` was there before and is not mine; two lines of the new test were split |

## Review findings and answers

| Round / ID | AGREE / DISAGREE / UNCERTAIN | Change or counter-evidence |
| --- | --- | --- |
| — | — | — |

## Uncovered, skipped, and decisions requested

- The Windows side is `not run`: `srt_writer_test.exe` is compiled and linked by G4 and runs on Windows with the next delivery's `run-tests.cmd` (16 unit and 5 integration tests); nothing in the writer is platform-specific (`std::to_string` and `std::string` only).
- The writer has no caller yet: the Transcribe page and the queue come with later packets, which will feed it the merged segments of the windows and hand its output to `WriteFileAtomically` ([P5-T2](../P5-T2-atomic-write/packet.md)).
- Subtitle shaping (characters per cue, splits at sentence ends) is not part of this task (the packet).
- A speaker label that no number gives is written as `(<label>) `; the packet left the choice open and asked for it to be recorded here. If A003 prefers another shape, it is one line in `SrtSpeakerPrefix`.
