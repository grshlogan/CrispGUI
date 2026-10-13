# P5-T2 Review R01

- Reviewer: fresh subagent, Sonnet 5.5, dispatched by A004
- Packet: [packet.md](packet.md) (v1); candidate `d841b2777273451b194c98b411abbedf8acaf94d`; base `8d93c2edc5e6dbcf282c2e645ab3d60d83e8414d`; previous report: none
- Verdict: PASS

Summary: every criterion is met on Linux with evidence, and the Windows implementation compiles under G4. The Windows-only parts (M2, the held-open case, wide paths on a real Windows file system) are `not run`, as the packet assigns them to W5-1. There is no P0 or P1 finding. One P2 (destination names within 21 bytes of the file name limit) and six P3 items are listed under Findings; none blocks the merge.

Aliases: `<CODING_ROOT>` is `/home/claude/projects/CrispGUI-a004` (read only for this Review). `<SP>` is the scratchpad `/tmp/claude-1000/-home-claude-projects-CrispGUI-a004/0d1710c9-17d2-4343-bd00-ec946bfba62a/scratchpad`. `<DIR>` is `<SP>/r01-export`, a fresh `git archive` of the candidate. The scratchpad also held other folders that were there before this Review started (for example `export`, `probe`, `pristine`); none of them was read or reused. Everything below was built and run in `<DIR>` or in `<SP>/r01-*` folders, on Linux (Debian 12 under WSL2, kernel 6.18.40.1, GCC 12.2.0, CMake 3.25.1, Ninja 1.11.1, user `claude` without superuser rights).

## Gates rerun

Each command ran through a wrapper with `set -o pipefail` that appends the real exit code to its log (`<SP>/r01-logs/`). Every configure line carried `-DCRISPGUI_EUI_NEO_DIR=/home/claude/projects/Learn/sources/github.com/sudoevolve/EUI-NEO/v0.6.1 -DCRISPGUI_FONT_DIR=/home/claude/projects/CrispGUI/fonts`.

| Command | Exit code | Result |
| --- | --- | --- |
| `git -C <CODING_ROOT> archive d841b27… \| tar -x -C <DIR>` | 0 | The six touched code files are `cmp`-identical to `git show` of the candidate. After all experiments `diff -r -q --exclude=build <DIR> <second fresh extraction>` printed nothing: the tree is unchanged |
| G1: `cmake -S . -B build/linux -G Ninja -DCMAKE_BUILD_TYPE=Debug … && cmake --build build/linux` | 0 | 228 steps; 0 lines containing "warning" in the log (compiler and CMake) |
| G2: `ctest --test-dir build/linux --output-on-failure` | 0 | 22 of 22 passed (17 unit, 5 integration); `file_write_test` passed in 0.06 s. P4-T1's "20 of 20" is stale, see F7 |
| G3: `for i in 1 2 3 4 5; do ctest --test-dir build/linux -L integration --output-on-failure \|\| exit 1; done` | 0 | Five rounds, each 5 of 5 passed |
| G4: `cmake -S . -B build/mingw -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/mingw-w64-x86_64.cmake … && cmake --build build/mingw` | 0 | Fresh configure with the MinGW-w64 GCC 12 posix compilers; 231 steps; 0 warnings; 23 `.exe` files in `build/mingw/tests/` (22 tests and the stub); `file_write_test.exe` is a PE32+ console x86-64 file of 2,711,673 bytes. `nm` shows the library defines `RenameOverFile`, `FlushFileToDisk`, `WriteFileAtomically`, `AtomicTemporaryPath`, and no symbol named `ReplaceFile` (MinGW's `winbase.h:2343` defines `ReplaceFile` as a macro, so the receipt's reason for the rename is right) |
| Static look at the Windows object: `x86_64-w64-mingw32-nm` and `objdump -d` on `files_win32.cpp.obj` | 0 | The object imports `MoveFileExW`, `FlushFileBuffers`, `_get_osfhandle`, `_fileno`, `_wfopen`; `RenameOverFile` passes the flags argument `0x9` (REPLACE_EXISTING 0x1 plus WRITE_THROUGH 0x8) |
| After all experiments: `cmake --build build/linux`, `ctest --test-dir build/linux --output-on-failure`, `cmake --build build/mingw` | 0; 0; 0 | 22 of 22 passed; the MinGW tree has 0 warnings and `file_write_test.exe` is 2,711,673 bytes again |
| `git -C <CODING_ROOT> diff --check 8d93c2e d841b27` (read only) | 0 | No whitespace errors |
| `awk 'length > 120'` on the five code files | 0 | No line over 120 columns |
| `python3 -I mdcheck_export.py <DIR>`: a copy of `tools/dev/mdcheck.py` whose only change is the file listing (`os.walk`, because the export is not a Git repository) | 0 | 49 files, 0 problems |
| `g++ -std=c++17 -fsyntax-only -fno-exceptions -fno-rtti -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion -Wold-style-cast -Wuseless-cast -Wnull-dereference` on `files.cpp`, `files_posix.cpp`, and the test; `-fanalyzer` on `files.cpp` | 0 (each) | One `-Wuseless-cast` at `files.cpp:21` (the same expression as in `multipart.cpp`); nothing else; the analyzer reports nothing |

Write scope: `git diff --stat` lists `docs/agents/tasks/P5-T2-atomic-write/receipt.md`, `src/platform/files.cpp`, `src/platform/files.h`, `src/platform/files_posix.cpp`, `src/platform/files_win32.cpp`, `tests/CMakeLists.txt`, `tests/file_write_test.cpp`. All are inside the packet's write scope; no protected path changed.

## Acceptance

| Criterion | Code | Check and negative evidence | Met |
| --- | --- | --- | --- |
| AC-1: a new file has exactly the bytes, also empty, with a NUL, and non-ASCII | `WriteFileAtomically` `files.cpp:73-117` (write at `:96`); `OpenFileForWriting` opens `"wb"` | `file_write_test.cpp:72-92`: `empty.bin`, a 9-byte body (`a`, NUL, `b`, E3 81 82, CR, LF, FF), a body of 1 MiB plus 17 bytes, and the directory listing. On the base the test does not compile (`AtomicTemporaryPath`, `RenameOverFile`, `WriteFileAtomically` not declared; ninja exit 1). Mutations: no bytes written, 14 checks fail (from `:82`); write cut at the first NUL, 2 (`:82`, `:89`); one byte short, 8; early return for an empty body, 3 (`:78`, `:91`, `:102`) | Yes |
| AC-2: an existing file is replaced, and no temporary file remains | `RenameOverFile` `files_posix.cpp:42-49` (`std::rename`); Windows `files_win32.cpp:61-68` (`MoveFileExW`, flags 0x9) | `:94-106`: replaced by shorter content, by nothing (file stays with 0 bytes), then by more; the directory holds only `out.srt`. Mutations: replace step skipped, 19 checks fail (from `:78`); `RenameOverFile` reports success without renaming, 24 (from `:78`); replace done by copying over the destination (same inode), 2 (`:215`, `:219`) | Yes on Linux. The Windows run is `not run` (W5-1) |
| AC-3: failures leave the destination and no temporary file: missing directory; destination is a directory; Windows held-open | Cleanup `files.cpp:113-115`; messages `:92`, `:97`, `:102`, `:106`, `:110` | Missing directory `:142-152`; destination is a directory `:154-168`; unwritable folder `:186-200`; write and flush failures under `RLIMIT_FSIZE` `:226-255`; Windows held-open `:257-277`, compiled by G4 and `not run`. Mutations: M1 (below); cleanup skipped only after a replace failure, 1 check (`:166`); skipped only after a write or flush failure, 3 (`:244`, `:249`, `:254`); cleanup removes the destination, 6; result always true, 3; a failed write not reported, 4; a failed flush not reported, 3; message of any one step not naming it, 1 or 2 each; message not cleared, 1. Fault injection in the branches the test cannot reach: see the table below | Yes on Linux. The held-open case is `not run` (W5-1) |
| AC-4: a path with Japanese characters works for creating and replacing | Paths stay `std::filesystem::path` end to end; Windows `_wfopen`, `MoveFileExW(source.c_str(), …)` (wide pointers, confirmed in the object code) | `:108-119`: a directory and a file name in Japanese are created, replaced, and listed. Mutation: the rename destination mangled to ASCII, 5 checks fail (`:114-118`), so the case has teeth for path handling on Linux | Yes on Linux. The Windows run is `not run` (W5-1) |
| AC-5: the temporary path is in the destination's directory and differs between two calls | `AtomicTemporaryPath` `files.cpp:67-71`; `RandomHexDigits` `:17-35` | `:121-140`: same parent for two calls, the two differ, not the destination, not the system temporary folder, `name.srt.` prefix and `.tmp` suffix, a bare name stays in the current directory, 1000 distinct names. Mutations: M3, 6 checks fail; temporary file in the current directory, 4; fixed name, 2; no `.tmp`, 1 | Yes |
| AC-6: mutation matrix, each caught on Linux except M2 | See the matrix below | M1, M3, M4 caught; M2 `not run` on Linux, recorded as W5-1 only | Yes |
| AC-7: gates pass, G4 compiles the Windows implementation | `files_win32.cpp`; Windows-only block of the test | G1 to G4 above, all exit 0, 0 warnings | Yes |

Hard rules and conventions: no downloads, no engine or profile writes. Rule 4 is not violated: the temporary file lives next to the destination as FR-3 and the packet require, never in the system TEMP, and the function writes no file of its own elsewhere. C++17 only, no exceptions or RTTI in the new code (the `std::error_code` overloads of `exists` and `remove`), no raw `new` or `delete`, platform code in `src/platform/`, PascalCase names, `kHex` constant, comments and messages in English. The one convention deviation is F2, in the test.

### AC-6 matrix

One mutation at a time in `<DIR>`, then `ninja -C build/linux file_write_test` and `ctest -R '^file_write_test$'`. Each build exited 0; "caught" means ctest exited 8 with the listed failing lines of `file_write_test.cpp`. After every mutation the six files were restored from copies taken with `git show` and checked with `cmp`: 33 mutation runs, 33 restores identical, no mismatch, no mutation that failed to apply or to build (M2 was a separate run).

| ID | Mutation | Result |
| --- | --- | --- |
| M1 | The temporary file is not removed after a failure (`files.cpp:113-115` emptied) | Caught: 4 checks, lines 166, 244, 249, 254 |
| M2 | `MOVEFILE_REPLACE_EXISTING` dropped (`files_win32.cpp:63`) | `not run` on Linux: the Linux build does not compile the file, and the Linux test still passes, as the packet expects. Compile-only: the flags argument in the MinGW object changed from 0x9 to 0x8. On Windows the replace cases (`:94-106`) and the second call after the handle is closed (`:274`) would fail; W5-1 decides |
| M3 | `AtomicTemporaryPath` returns `temp_directory_path() / name` | Caught: 6 checks, lines 127, 128, 132, 134, 147, 195 |
| M4a | `AtomicTemporaryPath` returns the destination | Caught: 20 checks, among them 129, 131, 139, 212, 215 |
| M4b | The bytes go straight to the destination: no free-name search, the temporary path is the destination, no rename, cleanup kept | Caught: 10 checks, lines 163, 194-196, 215, 219, 243, 244, 248, 249. Line 215: the inode did not change. Line 219: a reader holding the old file open saw the new bytes. The other lines: the write in place succeeded or damaged the file where it had to fail and keep `keep` |
| M4c | As M4b without any cleanup | Caught: 8 checks, lines 163, 194-196, 215, 219, 243, 248 |

A first M4b attempt kept the free-name search, which fails for every existing destination; it was discarded as a poor mutant and redone as above.

Further mutations of the Review (all caught unless marked):

| Mutation | Failing checks |
| --- | --- |
| Skip the replace step (the receipt's X9) | 19 (from line 78), as the receipt says |
| No bytes written (X15) | 14 (from line 82), as the receipt says |
| `fflush` removed, `fsync` kept; flush call skipped | 1 each (line 247) |
| A failed write ignored; a failed flush ignored | 4 (241-243, 248); 3 (246-248) |
| `error.clear()` removed | 1 (line 151) |
| Result always true | 3 (162, 241, 246) |
| Messages of the create, write, flush, or replace step renamed | 2 (147, 195); 1 (242); 1 (247); 1 (163) |
| `fsync` removed | Survives, by nature: a flush to the disk cannot be seen by reading the file back (the receipt's X16) |
| `fclose` skipped | Survives on Linux: a leaked `FILE*` does not stop `rename`. On Windows an open file blocks `MoveFileExW`, so W5-1 would catch it (F3) |
| Free-name search removed | Survives: the branch needs eight clashing names; exercised below by injection |

### Fault injection and stress (the Review's own probes)

The probes compile the candidate's `files.cpp` and `files_posix.cpp` unchanged, in `<SP>/r01-probe/`. Results are `observed`.

| Probe | Result |
| --- | --- |
| `rename` made to fail with EIO, EXDEV, EACCES (`LD_PRELOAD` shim) | `false`; "Could not replace … with the temporary file …: Input/output error." (and the other two reasons); the destination still holds `keep`; the directory holds only `out.srt` |
| `fsync` made to fail with EIO | `false`; "Could not flush the temporary file … to the disk: Input/output error."; destination `keep`; no temporary file |
| `fclose` made to fail with EIO | `false`; "Could not close the temporary file …: Input/output error."; destination `keep`; no temporary file |
| Every `*.tmp` name reported as existing (`stat`, `statx` shim) | `false`; "Could not find a free name for the temporary file next to …"; destination `keep`; nothing created |
| 8 threads, 40 writes each of 256 KiB to one destination, one reader thread | 0 failures, 0 torn reads (79 to 5059 reads per run over 5 runs, including one under ThreadSanitizer with ASLR off, which reported nothing), final file whole, no temporary file left |
| 6 processes, 60 writes each of 128 KiB to one destination | 0 failures, final file whole, no temporary file left (repeated 4 times) |
| SIGKILL at a random moment during a 96 MiB write, 60 rounds | The destination was whole in all 60 rounds (17 times the old content, 43 times the new); the kills left 19 stale `.tmp` files, as the receipt says it accepts |
| 150 calls over success, missing directory, and directory destination | Open descriptors 6 before and 6 after |
| 64 MiB body | Written and read back exactly |
| Odd destinations: bare file name in the current directory; empty directory; trailing slash; empty path; a file as parent; missing parent | Bare name works; all others return `false` with a readable message and leave nothing behind |
| The shipped test: 30 runs; 15 runs of a build with ASan, UBSan, and LeakSanitizer (ASLR off) | 0 failures in 30 runs; exit 0 in all 15 sanitized runs (a report would have ended the run with a failure) |

## Findings

| ID | Level (P0–P3) | Where | Reproduction, expected, actual | Repair direction |
| --- | --- | --- | --- | --- |
| F1 | P2 | `files.cpp:67-71` (the temporary name is the destination's name plus 21 bytes), `:90-94` | `observed` on Linux. A destination whose name is 234 bytes writes; 235 or 255 bytes returns `false` with "Could not create the temporary file …: File name too long." although `fopen` of the destination itself succeeds. A 238-byte name (78 kanji and `.srt`) fails the same way. Expected: a destination that a plain write can create can be written atomically. Actual: names within 21 bytes of the 255-byte limit fail, nothing is damaged. `hypothesis` for Windows: the same 21 bytes narrow the margin below MAX_PATH (260) when long paths are off; not verified | Bound the derived name: keep at most as many bytes of the destination's name as fit (cut at a UTF-8 boundary) or use a name that does not depend on it, and add a test with a 250-byte name and one of 80 CJK characters |
| F2 | P3 | `tests/file_write_test.cpp:132` | `observed`. `TMPDIR=/no/such/tmp build/linux/tests/file_write_test` ends with `terminate called after throwing an instance of 'std::filesystem::filesystem_error'`, exit 134, because `fs::temp_directory_path()` throws and the project builds with `-fno-exceptions`. CONVENTIONS ("Errors") asks for the `std::error_code` overloads; `app_data_test.cpp:40` uses `fs::temp_directory_path(error)` | Use the `std::error_code` overload and make the comparison only when it succeeds |
| F3 | P3 | `tests/file_write_test.cpp` (no check), `files.cpp:105` | `observed`. With the `fclose` call skipped, `file_write_test` still passes on Linux; the candidate itself does not leak (descriptors 6 and 6 over 150 calls). A leak would show only on Windows, where an open file blocks the rename | Add a Linux-only check that counts `/proc/self/fd` entries around a success and the failure paths |
| F4 | P3 | `files_posix.cpp:44`, `files.h:59-64` | `observed`. A 0600 destination is 0644 after the write (umask 0022). A symlink destination is replaced by a regular file; the file it pointed to keeps the old content. The receipt records the mode and owner, the header records neither, and the symlink case is in neither | State in the header that the new file has default attributes and replaces a link. If Phase 7 stores restricted files, copy the mode in the POSIX seam before the rename (Windows ACLs are inherited, so this is a Linux-build matter) |
| F5 | P3 | `files.cpp:78-90`, `files_posix.cpp:16-18`, `files_win32.cpp:26-28` | By reading. The free name is found by `exists` and then created by `fopen("wb")`, which truncates a file that appeared in between. With 64 random bits the clash is negligible and no test can reach it; the packet says "a new temporary file" | Create exclusively (`"wbx"`, or `_wsopen_s` with `_O_EXCL` on Windows), or record the accepted risk in the header |
| F6 | P3 | `files_win32.cpp:63` | `hypothesis`, `not run`. One `MoveFileExW` call; a virus scanner or an indexer that holds the fresh file open for a moment makes it fail with a sharing violation, and the SRT would be lost for that attempt. W5-1 should run on a machine with real-time scanning to see whether this happens | Retry a few times (for example 3 tries 50 ms apart) on ERROR_ACCESS_DENIED and ERROR_SHARING_VIOLATION, then report the last error; the held-open case still fails after the retries |
| F7 | P3 | `docs/agents/tasks/P4-T1-timeout-test/packet.md:43` (not part of the candidate) | G2 of P4-T1, which AC-7 cites, expects "20 of 20"; the candidate has 22 tests (17 unit, 5 integration), all passing | A003: word G2 as "all tests pass" or update the count in later packets |

## Not checked, and why

- Windows behavior: the `MoveFileExW` flags in a live replace, the held-open case, Unicode paths on NTFS, `FlushFileBuffers` on a real handle, and M2 are `not run`. WSL2 has no Windows runtime for them, and AGENTS.md says that Linux results never close a Windows acceptance gate. The packet assigns them to W5-1, which does not exist yet in `tools/windows/CHECKLIST.md`; the Phase 5 delivery must add it. The Windows code was read in full, compiled by G4, and its flag argument checked in the object code.
- Durability of `fsync`, `FlushFileBuffers`, and `MOVEFILE_WRITE_THROUGH` under power loss: not observable by a test, as the receipt says.
- A real full disk (ENOSPC) and a real cross-volume rename (EXDEV): simulated only, by `RLIMIT_FSIZE` in the shipped test and by injection in the Review's probes.
- File systems other than the ext4 of `/tmp`: tmpfs, network shares, FAT, case-insensitive volumes, and symlinked directories.
- The test as the superuser: only user 1000 ran it, so the skip branch of the unwritable-folder case was not run.
- Sanitizers: with address randomization on, ASan, LeakSanitizer, and ThreadSanitizer fail nondeterministically on this WSL2 kernel (a `DEADLYSIGNAL` loop in 2 of 8 runs of a probe that has nothing to do with this change; ThreadSanitizer aborts at start). All sanitizer results above were taken with `setarch x86_64 -R`. Valgrind and clang-format are not installed, so the style check was by eye plus the 120-column check.
- The Review's probes and scripts are in `<SP>/r01-probe/` and `<SP>/r01-scripts/`, outside the repository.
