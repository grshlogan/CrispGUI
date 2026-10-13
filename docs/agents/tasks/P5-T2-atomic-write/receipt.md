# P5-T2 receipt

- Identity, session, model: A004, Claude Code session `0d1710c9-17d2-4343-bd00-ec946bfba62a`, Claude Sonnet 5.5
- Packet: packet.md (v1); profile `strict`
- Base: origin/main `8d93c2e` (P5-T1 pushed at `6d53066`; `8d93c2e` is documentation only) on 2026-10-13 (UTC); branch `a004/P5-T2`. The candidate was written on `6d53066` and rebased onto `8d93c2e` before the Review, which touched none of its files
- State: Review R01 (candidate committed; the Review is dispatched next)
- Candidate commits reviewed: R01 — the SHA is recorded in the commit that adds the report, because a commit cannot contain its own SHA

## Changes

| Path | Why | Criterion |
| --- | --- | --- |
| `src/platform/files.h` | Declares `AtomicTemporaryPath`, `FlushFileToDisk`, `RenameOverFile`, and `WriteFileAtomically` (and includes `<string_view>`) | AC-1 to AC-7 |
| `src/platform/files.cpp` | The platform-neutral part: `RandomHexDigits` (a generator seeded from the clocks, the thread, and a counter, as `RandomMultipartBoundary` does; no `std::random_device`), `AtomicTemporaryPath`, and `WriteFileAtomically` | AC-1 to AC-6 |
| `src/platform/files_posix.cpp` | `FlushFileToDisk` (`fflush`, then `fsync`) and `RenameOverFile` (`rename`) | AC-2, AC-3, AC-6 |
| `src/platform/files_win32.cpp` | `FlushFileToDisk` (`fflush`, then `FlushFileBuffers` on the handle of `_get_osfhandle(_fileno(file))`) and `RenameOverFile` (`MoveFileExW(source, destination, MOVEFILE_REPLACE_EXISTING \| MOVEFILE_WRITE_THROUGH)`), with the system's message from `win32::ErrorMessage`; every path goes through the wide APIs | AC-2, AC-3, AC-4, AC-7 |
| `tests/file_write_test.cpp` (new), `tests/CMakeLists.txt` | The test, registered as a unit test: 17 unit tests and 5 integration tests now. It includes two cases that make the write and the flush fail with a limit on file sizes | AC-1 to AC-6 |
| `docs/agents/tasks/P5-T2-atomic-write/receipt.md` | This receipt | — |

`git diff --stat` plus the new test lists these paths and nothing else; every other file is untouched.

Behavior, in the order of `WriteFileAtomically` (`src/platform/files.cpp:73`): pick a temporary name `<name>.<16 hexadecimal digits>.tmp` in the destination's directory that does not exist (up to eight tries); create it (`OpenFileForWriting`); write the bytes; flush them to the disk; close it; `RenameOverFile` it over the destination. Any failure removes the temporary file, leaves an existing destination as it was, and returns false with a message that names the step and the system's reason: "Could not create the temporary file …", "… write …", "… flush … to the disk", "… close …", or "Could not replace <destination> with the temporary file …".

Decisions inside the packet's freedom:

- The packet's building blocks are named `AtomicTemporaryPath` and `WriteFileAtomically` as it suggests. The replace step is `RenameOverFile`, not `ReplaceFile`: `<windows.h>` defines `ReplaceFile` as a macro (`ReplaceFileA`), so the first MinGW build of the candidate (G4) failed in the test and the Windows implementation would have defined a different function than the header declared. The header says why.
- The temporary file's name keeps the destination's name and ends in `.tmp`, so a file left by a crash is recognizable. It is in the destination's directory and never in the system's temporary folder.
- No directory is flushed after the rename; the packet asks for the file's flush only, and the durability of the rename itself is the file system's. The mode and owner of an existing destination are not carried over: the new file has the default mode of `fopen` (`0666` and the umask on POSIX).
- The function holds no global state apart from the atomic counter of the name generator, so concurrent calls, even for the same destination, each write their own temporary file; the last replacement wins.
- `std::filesystem::exists`, `remove`, and `directory_iterator` are used through their `std::error_code` overloads, as the project's conventions ask (no exceptions).

## Acceptance mapping

`<scratchpad>` is `/tmp/claude-1000/-home-claude-projects-CrispGUI-a004/0d1710c9-17d2-4343-bd00-ec946bfba62a/scratchpad`.

| Criterion | Code | Check that fails without the change | Evidence |
| --- | --- | --- | --- |
| AC-1: a new file has exactly the bytes, also an empty file and bytes with a NUL and non-ASCII text | `WriteFileAtomically` in `src/platform/files.cpp:73` | `tests/file_write_test.cpp:72-92`: `empty.bin` (0 bytes), `odd.bin` (9 bytes: `a`, NUL, `b`, E3 81 82, CR, LF, FF), `big.bin` (1 MiB plus 17 bytes of a changing pattern); the directory then holds only those three files. Mutation X15 (no bytes are written) fails 14 checks, from line 82 | The test passes on Linux |
| AC-2: an existing file is replaced, and no temporary file remains | Same; `RenameOverFile` in `files_posix.cpp` and `files_win32.cpp` | `tests/file_write_test.cpp:94-106`: a longer old content replaced by `new`, then by nothing (the file stays, with 0 bytes), then by `last`; the directory holds only `out.srt`. Mutations X9 (the replace step skipped: 19 checks fail, from line 78) and X18 (`RenameOverFile` does nothing and reports success: 24 checks, from line 78) | The test passes on Linux; on Windows the same case runs in W5-1 |
| AC-3: failures leave the destination as it was and no temporary file: the directory does not exist; the destination is a directory; on Windows, the destination is held open without delete sharing | Same | Missing directory `:142-152`: false, the message starts "Could not create the temporary file ", nothing is created, and the next success clears the message. Destination is a directory `:154-168`: false, the message starts "Could not replace ", the directory keeps its file `keep.txt` unchanged, and the parent holds only `target`. Read-only folder `:186-200` (Linux; skipped when the process may write there, as the superuser does): the existing file keeps its content. Failures after the temporary file exists `:226-255` (Linux): a limit on file sizes (`RLIMIT_FSIZE`, with `SIGXFSZ` ignored) makes a 200,000-byte body fail while it is written ("Could not write the temporary file …") and a 100-byte body fail while it is flushed ("Could not flush the temporary file …"); each time the existing file keeps `keep`, no temporary file stays, and with the limit lifted the same call succeeds. Held open `:257-277` (Windows only): `CreateFileW` with share mode 0, the call fails, the file keeps `old` and no temporary file stays, and after `CloseHandle` the same call succeeds. Mutations M1 (the temporary file is not removed after a failure: 4 checks fail, lines 166, 244, 249, 254), X10 (a failure removes the destination instead of the temporary file: 6, lines 166, 243, 244, 248, 249, 254, among them the destination lost), X5 (the message is not cleared: 1, line 151), X6 (the result is always true: 3, from line 162), X13 (the first step's message does not name it: 2, line 147), X20 (a failed write is not reported: 4, from line 241), X21 (a failed flush is not reported: 3, from line 246) | The Linux cases pass; the Windows case is compiled by G4 and runs in W5-1 (`not run` here) |
| AC-4: a path with Japanese characters works for creating and replacing | `OpenFileForWriting` and `RenameOverFile` take the path as `std::filesystem::path` | `tests/file_write_test.cpp:108-119`: a directory `字幕 テスト` and a file `動画 の 字幕.srt` are created, replaced with a text that holds a Hiragana letter, and listed | The test passes on Linux; on Windows the wide APIs carry the same path (`_wfopen`, `MoveFileExW`), run in W5-1 |
| AC-5: the temporary path is in the destination's directory and differs between two calls | `AtomicTemporaryPath` in `src/platform/files.cpp:67` | `tests/file_write_test.cpp:121-140`: the parent equals the destination's parent for two calls, the two differ, neither equals the destination or the system's temporary folder, the name starts with the destination's name and ends in `.tmp`, a bare file name stays in the current directory, and 1000 calls give 1000 names. Mutations M3 (the system's temporary folder: 6 checks fail, from line 127), X11 (the current directory: 4, line 127), X8 (a fixed name: 2, line 129), X14 (no `.tmp`: 1, line 131) | The test passes on Linux |
| AC-6: mutation matrix: M1, M3, M4 caught on Linux; M2 only by W5-1 | See the table below | See the table below | `<scratchpad>/logs/P5T2-mutations.log` |
| AC-7: gates pass; G4 compiles the Windows implementation | `src/platform/files_win32.cpp` and the Windows-only block of the test | G4 failed once, on the macro `ReplaceFile` (see the decisions) | G1 to G4 in the Commands table, from the final code |

Mutation matrix of AC-6 and my own additions. Each mutation changes one place in `src/platform/files.cpp` (or `files_posix.cpp` for X16 and X18) with `sed`, builds `file_write_test`, runs it, and restores the file from a saved copy (`cmp` identical); `ctest` exit 8 means a check failed.

| ID | Mutation | Result |
| --- | --- | --- |
| M1 | The temporary file is not removed after a failure | Caught: 4 failed checks, lines 166, 244, 249, 254 (a stray temporary file next to a directory destination, and after each limited write) |
| M2 | The replace flag is dropped on Windows (`MOVEFILE_REPLACE_EXISTING`) | `not run` on Linux: only the Windows build has it. On Windows the cases "an existing file is replaced" (`:94-106`) and "held open" would fail, since `MoveFileExW` refuses an existing destination without the flag. W5-1 decides |
| M3 | The temporary file goes to the system's temporary folder | Caught: 6 failed checks, from line 127 |
| M4 | The bytes are written straight to the destination (the temporary path is the destination itself) | Caught: 10 failed checks. Line 215: the destination's inode does not change (a replacement by rename gives it a new one). Line 219: a reader that has the old file open no longer sees all of the old bytes. Lines 163, 194 to 196, 243 to 244, and 248 to 249: writing in place lets the call succeed where it must fail (a directory as destination, a folder or a size limit that refuses the write), and the existing file is no longer `keep` |
| X5 | The message is not cleared at the start | Caught: 1, line 151 |
| X6 | The result is always true | Caught: 3, from line 162 |
| X8 | The temporary name is not random | Caught: 2, line 129 |
| X9 | The replace step is skipped | Caught: 19, from line 78 |
| X10 | A failure removes the destination instead of the temporary file | Caught: 6, lines 166, 243, 244, 248, 249, 254 (the stray temporary file stays, and the destination is gone) |
| X11 | The temporary file goes to the current directory | Caught: 4, from line 127 |
| X13 | The message of the first step does not name it | Caught: 2, line 147 |
| X14 | The temporary name has no `.tmp` | Caught: 1, line 131 |
| X15 | No bytes are written | Caught: 14, from line 82 |
| X16 | `fsync` is not called | Survives, by nature: whether a flush reached the disk cannot be observed by a test that reads the file back. The packet's matrix does not list it |
| X18 | `RenameOverFile` does nothing and reports success (POSIX) | Caught: 24, from line 78 |
| X20 | A failed write is not reported | Caught: 4, from line 241 |
| X21 | A failed flush is not reported | Caught: 3, from line 246 |

## Commands

All run in `<CODING_ROOT>` on Linux (Debian 12 under WSL2) with `set -o pipefail`; the logs are in `<scratchpad>/logs/`. `-DCRISPGUI_FONT_DIR=/home/claude/projects/CrispGUI/fonts` is on the first configure of each tree.

| Where (alias) | Command | Exit code | Result |
| --- | --- | --- | --- |
| `<CODING_ROOT>` | `git fetch origin`; `git switch --no-track -c a004/P5-T2 origin/main` at `6d53066` | 0; 0 | Branch created; `8d93c2e` appeared upstream while I worked (documentation only, no overlap with my files) |
| `<CODING_ROOT>` | `cmake --build build/linux`; `ctest --test-dir build/linux -R '^file_write_test$' --output-on-failure` | 0; 0 | 0 warnings; the new test passed on its first run |
| `<CODING_ROOT>` | G4, first run: `cmake --build build/mingw` | 1 | Three compile errors in `tests/file_write_test.cpp`: `ReplaceFile` is a macro of `<windows.h>` and became `ReplaceFileA`. The Windows implementation would have defined `ReplaceFileA`, not the declared function |
| `<CODING_ROOT>` | `sed` of `ReplaceFile` to `RenameOverFile` in the header, the three sources, and the test; `cmake --build build/mingw`; `x86_64-w64-mingw32-nm -C build/mingw/libcrisp_core.a` | 0 | 0 warnings; `build/mingw/tests/file_write_test.exe` (2,711,673 bytes) links; the library defines `crisp::platform::RenameOverFile` and `FlushFileToDisk`, and nothing named `ReplaceFile` |
| `<CODING_ROOT>` | A first Review R01 (candidate `ef57ccd`, before the two cases of the limited writes were added) stopped producing output at 06:19 PDT, left no report and no process, and ignored a status message for 12 minutes; I stopped it with `TaskStop`. Its last note said it was about to test the write and flush failure branches with `RLIMIT_FSIZE`, which the test did not reach | — | A dispatch that wrote nothing is not a round. I added the two cases (`:226-255`), moved its scratch folder aside, amended the unpushed candidate, and dispatch a fresh subagent |
| `<CODING_ROOT>` | `bash <scratchpad>/scripts/p5t2-mutate.sh`: for each mutation `sed`, `cmake --build build/linux --target file_write_test`, `ctest -R '^file_write_test$'`, restore with `cp`, `cmp` (run on the final code) | build 0 for each; ctest 8 for 15 mutations, 0 for X16 | See the matrix; both files restored and identical; the tree rebuilt afterwards |
| `<CODING_ROOT>` | G1: `cmake -S . -B build/linux -G Ninja -DCMAKE_BUILD_TYPE=Debug …` and `cmake --build build/linux` | 0 | Incremental, 0 compiler warnings, 0 CMake warnings |
| `<CODING_ROOT>` | G2: `ctest --test-dir build/linux --output-on-failure` (again after the final rebuild) | 0; 0 | 22 of 22 passed (21 before, plus `file_write_test`); `ctest -N -L unit` lists 17 and `-L integration` 5 |
| `<CODING_ROOT>` | G3: `for i in 1 2 3 4 5; do ctest --test-dir build/linux -L integration --output-on-failure \|\| exit 1; done` | 0 | Five rounds, each 5 of 5 passed |
| `<CODING_ROOT>` | G4: `cmake --build build/mingw` on the tree configured with the toolchain file in P4-T4 (twice, after the rename and at the end) | 0; 0 | 0 compiler warnings; 23 `.exe` files in `build/mingw/tests/` (22 tests and the stub). The configure line of G4 is not repeated on this existing tree, which would only print the "CMAKE_TOOLCHAIN_FILE not used" notice; the Review runs both lines from a clean export |
| `<CODING_ROOT>` | `python3 -I tools/dev/mdcheck.py .`; `git diff --check` (the new test added as intent-to-add); `awk` for lines over 120 columns in the changed and new files | 0; 0; none | 0 problems; no whitespace errors; two lines were split |

## Review findings and answers

| Round / ID | AGREE / DISAGREE / UNCERTAIN | Change or counter-evidence |
| --- | --- | --- |
| — | — | — |

## Uncovered, skipped, and decisions requested

- The Windows side is `not run`: `file_write_test.exe` is compiled and linked by G4 (the Windows-only case included) and runs with the Phase 5 delivery's W5-1. What only Windows can show: the `MoveFileExW` flags (M2), the held-open case, the wide-path behavior, and `FlushFileBuffers` on a real handle.
- Durability (`fsync`, `FlushFileBuffers`, `MOVEFILE_WRITE_THROUGH`) is by design not observable in a test (X16 survives); the code follows the packet's description.
- Branches no test reaches: the failure of `fclose`, of `fsync` itself, and the exhaustion of the eight tries for a free temporary name; none can be produced on a regular file in a scratch folder without an interposed system call.
- The writer has no caller yet: the SRT output of the Transcribe page and Phase 7's job files will call `WriteFileAtomically`.
- A temporary file left by a crash stays until someone removes it; its name (`<name>.<digits>.tmp`) says what it belongs to. Nothing sweeps them in this task.
- Not preserved on an existing destination: its mode, owner, and extended attributes (the new file is created with the defaults).
