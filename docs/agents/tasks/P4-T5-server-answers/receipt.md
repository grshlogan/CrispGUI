# P4-T5 receipt

- Identity, session, model: A004, Claude Code session `0d1710c9-17d2-4343-bd00-ec946bfba62a`, Claude Sonnet 5.5
- Packet: packet.md (v1)
- Base: origin/main `2559a9d` (P4-T4 pushed at `fbe11fd`; `2559a9d` is documentation only) on 2026-10-13 (UTC); branch `a004/P4-T5`. The candidate was written on `fbe11fd` and rebased onto `2559a9d` before the Review, which touched none of its files
- State: Review R01 (candidate committed; the Review is dispatched next)
- Candidate commits reviewed: R01 — the SHA is recorded in the commit that adds the report, because a commit cannot contain its own SHA

## Changes

| Path | Why | Criterion |
| --- | --- | --- |
| `tests/data/deployed-parakeet/` (eight files) | The server answers of the owner's W4-3 run, copied byte for byte from `probe-results/20261013T050347Z/` in the delivery's folder in `<HANDOFF_OUT>`: `transcript.verbose.json`, `window.verbose.json`, `transcript.srt`, `error-key.json`, `error-format.json`, `error-decode.json`, `health.json`, `models.json`. The probe's own files (`report.txt`, `report.json`, `not-audio.mp3`) stay there | AC-1 |
| `tests/CMakeLists.txt` | `CRISPGUI_DEPLOYED_ANSWERS` and a `configure_file(… COPYONLY)` loop copy the eight files to `testdata/deployed-parakeet/` next to the test executables | AC-1 |
| `tests/data/README.md` | A section on the recorded answers: delivery and commit, the probe report's place, the audio file's name, size, and SHA-256, the date of the run, the server's profile, and for each file its SHA-256, its reader, and what it is. `transcript.srt` is marked as the expected output of Phase 5's SRT writer for `transcript.verbose.json` and untested until then | AC-1, AC-5 |
| `tests/transcript_test.cpp` | `CheckRecorded` and two calls: both `verbose_json` answers parse into exact milliseconds | AC-2 |
| `tests/api_response_test.cpp` | The three recorded error bodies go through `ClassifyApiResponse` | AC-3 |
| `tests/server_state_test.cpp` | `health.json` and `models.json` go through `ClassifyHealth` and `ClassifyModels`, which take an `HttpResponse` and need no poller | AC-4 |
| `docs/agents/tasks/P4-T5-server-answers/receipt.md` | This receipt | — |

`git diff --stat` outside the data folder lists the five test and data files above; `src/`, the stub, the synthetic fixture, and every document outside this folder and `tests/data/README.md` are untouched. No test executable is added.

## Acceptance mapping

`<scratchpad>` is `/tmp/claude-1000/-home-claude-projects-CrispGUI-a004/0d1710c9-17d2-4343-bd00-ec946bfba62a/scratchpad`. The mutations below change the build's copy of a data file (`build/linux/tests/testdata/deployed-parakeet/`), never the source, and restore it from the source (`cmp` identical).

| Criterion | Code | Check that fails without the change | Evidence |
| --- | --- | --- | --- |
| AC-1: the eight files in `tests/data/deployed-parakeet/` with the packet's SHA-256, copied by the build, with provenance and readers in the README | `tests/data/deployed-parakeet/`, `tests/CMakeLists.txt` (`CRISPGUI_DEPLOYED_ANSWERS`), `tests/data/README.md` | Mutation M4: with `build/linux/tests/testdata/deployed-parakeet/` moved away, `transcript_test`, `api_response_test`, and `server_state_test` all fail (ctest exit 8; seven failed `Ok()` checks), so the tests depend on the copy; restored, they pass | `sha256sum -c` of the packet's table passes for the input files in `<HANDOFF_OUT>`, for the source files, for the copies in `build/linux/tests/testdata/deployed-parakeet/` and in `build/mingw/tests/testdata/deployed-parakeet/`, and for the blobs Git stages (`git show :<path> \| sha256sum`; `git ls-files --eol` says `i/none` for the five files without a trailing newline and `i/lf` for the other three, so `.gitattributes` changes none of them; none holds a CR or NUL byte) |
| AC-2: both `verbose_json` answers parse into exact milliseconds: `transcript.verbose.json` gives language `ja`, duration 9696, one segment 230 to 8710 with 46 words (first こ 230 to 250, last す 8690 to 8710); `window.verbose.json` gives duration 4696, one segment 5070 to 8710 with 25 words | `tests/transcript_test.cpp`: `CheckRecorded` and its two calls | Mutation M1: the first word's end `0.25` → `0.26` in the copy of `transcript.verbose.json`: `transcript_test` exits 8 and fails at the check of the first word. M1b: the window's `duration` `4.696` → `4.697`: exits 8 and fails at the check of the duration. Each restored (identical) and passing again | The numbers were first read from the files with an independent parser (Python's `json`: 46 and 25 words, first and last words, bounds, durations) and are what the test asserts. The test also asserts the segment id, that the segment's text equals the answer's `text`, no speaker label, and that word times stay inside the segment and never go backwards. The window's first word is キ (5070 to 5090) and its last す (8690 to 8710) |
| AC-3: the three error bodies give the server's messages through the client's error handling: `401` "invalid or missing API key" as `kUnauthorized`, both `400`s as `kBadRequest` with their messages | `tests/api_response_test.cpp` (the new block before "Messages of error bodies") | Mutation M2: `invalid or missing API key` → `…keyx` in the copy of `error-key.json`: `api_response_test` exits 8 and fails at the message check. M2b: `bogus` → `bogux` in `error-format.json`: exits 8 and fails at the message check. Restored | The statuses (`401`, `400`, `400`) come from the probe report, checks `[4]` to `[6]` |
| AC-4: `health.json` and `models.json` give backend `fastconformer-ctc` and the model path through the project's readers | `tests/server_state_test.cpp` (after the existing `ClassifyModels` checks) | Mutation M3: `fastconformer-ctc` → `fastconformer-ctx` in `health.json`: exits 8, fails at the backend check. M3b: `q8_0.gguf` → `q8_1.gguf` in `models.json`: exits 8, fails at the model path check. Restored | The readers can be called without the poller (`ClassifyHealth(const HttpResponse&)`, `ClassifyModels(const HttpResponse&)` in `src/core/server_state.h`), so the criterion is met by tests, not by a remark |
| AC-5: `transcript.srt` untested for now and marked as the expected output of Phase 5's SRT writer | `tests/data/README.md` | — | Reading: the README row says so and links OQ-7; no test reads the file, and the build still copies it |
| AC-6: gates pass; the test count of `run-tests.cmd` changes only if a test executable is added | — | — | G1 to G4 in the Commands table; no test executable is added: `ctest -N` lists 20 tests, and `build/mingw/tests/` holds 21 `.exe` files, the 20 tests and `stub_crispasr.exe`, as before |

## Commands

All run in `<CODING_ROOT>` on Linux (Debian 12 under WSL2) with `set -o pipefail`; the logs are in `<scratchpad>/logs/`. `-DCRISPGUI_FONT_DIR=/home/claude/projects/CrispGUI/fonts` is on the first configure of each tree.

| Where (alias) | Command | Exit code | Result |
| --- | --- | --- | --- |
| `<CODING_ROOT>` | `git fetch origin`; `git switch --no-track -c a004/P4-T5 origin/main` at `fbe11fd` | 0; 0 | Branch created; `2559a9d` appeared upstream while I worked (documentation only, no overlap with my files) |
| `<CODING_ROOT>` | `cp` of the eight files from `<HANDOFF_OUT>/crisprustgui-c296f23/probe-results/20261013T050347Z/` into `tests/data/deployed-parakeet/`; `sha256sum -c` against the packet's table | 0; 0 | Eight `OK` |
| `<CODING_ROOT>` | `tr -cd '\r' < file \| wc -c` and `tr -cd '\0' < file \| wc -c` for each file; `git add` then `git ls-files --eol` and `git show :<path> \| sha256sum`; then `git reset` of the index | 0 | No CR and no NUL byte; Git's blobs hash to the packet's values |
| `<CODING_ROOT>` | `cmake --build build/linux` after the edits (CMake re-ran by itself) | 0 | 0 warnings; the build's `testdata/deployed-parakeet/` holds the eight files, all `OK` |
| `<CODING_ROOT>` | `ctest --test-dir build/linux -R '^(transcript_test\|api_response_test\|server_state_test)$'` | 0 | 3 of 3 passed |
| `<CODING_ROOT>` | Mutations M1, M1b, M2, M2b, M3, M3b: `sed` one value in the build's copy, `ctest -R <test> --output-on-failure`, restore with `cp`, `cmp`; M4: move the build's `deployed-parakeet/` away, run the three tests, move it back | 8 each (M4: 8 for the run) | The failing checks named in the mapping; every copy restored and identical; afterwards the three tests pass |
| `<CODING_ROOT>` | G1: `cmake -S . -B build/linux -G Ninja -DCMAKE_BUILD_TYPE=Debug …` and `cmake --build build/linux` | 0 | Incremental, 0 compiler warnings, 0 CMake warnings |
| `<CODING_ROOT>` | G2: `ctest --test-dir build/linux --output-on-failure` | 0 | 20 of 20 passed |
| `<CODING_ROOT>` | G3: `for i in 1 2 3 4 5; do ctest --test-dir build/linux -L integration --output-on-failure \|\| exit 1; done` | 0 | Five rounds, each 5 of 5 passed |
| `<CODING_ROOT>` | G4: `cmake --build build/mingw` on the tree configured with `cmake/toolchains/mingw-w64-x86_64.cmake` in P4-T4 | 0 | 42 steps, 0 compiler warnings; `build/mingw/tests/testdata/deployed-parakeet/` holds the eight files, all `OK`. The configure line of G4 is not repeated on this existing tree, which would only print the "CMAKE_TOOLCHAIN_FILE not used" notice; the Review runs both lines from a clean export |
| `<CODING_ROOT>` | `python3 -I tools/dev/mdcheck.py .`; `git diff --check`; `awk` for lines over 120 columns in the changed C++ and CMake files | 0; 0; none | 0 problems; no whitespace errors; one line of 122 columns was split before the gates ran |

## Review findings and answers

| Round / ID | AGREE / DISAGREE / UNCERTAIN | Change or counter-evidence |
| --- | --- | --- |
| — | — | — |

## Uncovered, skipped, and decisions requested

- The Windows side is `not run`: `run-tests.cmd` runs these three tests there with the answers in `testdata\deployed-parakeet\`, which `tools/package-windows.sh` carries (it copies `build/mingw/tests/testdata` whole); the folder is in the MinGW build tree. The test code reads the files like the existing fixture does.
- The Review reads the data from the committed candidate; the packet's input folder in `<HANDOFF_OUT>` is not part of it. The Review can compare the files with the packet's table.
- `transcript.srt` has no test until Phase 5's SRT writer exists ([OQ-7](../../../OPEN-QUESTIONS.md#oq-7-local-srt-generation)).
- A decision inside the packet's freedom: the two `verbose_json` answers share one helper, `CheckRecorded`, that also asserts the structure the packet describes only in part (one segment per answer, the segment's text equals the answer's text, word times inside the segment and non-decreasing). These hold in both recorded answers (checked with an independent parser) and would catch a parser that mixes segment and word times.
