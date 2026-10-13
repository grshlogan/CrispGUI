# P5-T2 Writing a file atomically — packet v1

| Field | Value |
| --- | --- |
| Task / version / date | `P5-T2` / v1 / 2026-10-13 (round 20) |
| Coordinator / Coding / acceptance | A003 / A004 / A003 for Linux; the Windows cases run in the Phase 5 delivery's W5-1 |
| Profile | `strict` (persistence: a failure must never leave a damaged or half-written file) |
| Base and branch | `origin/main` when A004 starts (P5-T1 need not be merged); branch `a004/P5-T2` in `<CODING_ROOT>` |
| Write scope | `src/platform/files.h`, `src/platform/files.cpp`, `src/platform/files_win32.cpp`, `src/platform/files_posix.cpp`, a new test `tests/file_write_test.cpp`, `tests/CMakeLists.txt`, this task folder |
| Protected | Everything else |
| Read first | [AGENTS.md](../../../../AGENTS.md) (hard rule 4); [SPEC.md FR-3](../../../SPEC.md#fr-3-transcription-single-file) (atomic write); [CONVENTIONS.md](../../../CONVENTIONS.md); `src/platform/files.h` |

## Problem

FR-3 asks for the SRT to be written atomically: a temporary file in the destination directory, then a rename over the destination. Phase 7 needs the same for its job files. The platform layer has no such function yet.

## Change

A function in `src/platform/files.h`, for example `bool WriteFileAtomically(const std::filesystem::path& path, std::string_view bytes, std::string& error)`:

- Writes the bytes to a new temporary file in the destination's own directory (never the system TEMP folder, which may be on another volume), flushes it to the disk (`FlushFileBuffers` on Windows, `fsync` elsewhere), closes it, and replaces the destination with it: `MoveFileExW(..., MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)` on Windows, `rename` elsewhere.
- Any Unicode path works on Windows (the wide APIs, as `OpenFileForWriting` does).
- On any failure it removes its temporary file, leaves an existing destination unchanged, and returns false with a readable message naming the step that failed.
- The temporary file's name comes from a separate function that tests can call (for example `AtomicTemporaryPath(destination)`), so that its directory can be checked.

## Acceptance

| ID | Criterion | Verification |
| --- | --- | --- |
| AC-1 | A new file is created with exactly the bytes, including an empty file and bytes with a NUL and non-ASCII text | `file_write_test` |
| AC-2 | An existing file is replaced, and no temporary file remains | `file_write_test` |
| AC-3 | Failure cases leave the destination as it was and no temporary file: the directory does not exist; the destination is a directory; on Windows, the destination is held open by another handle without delete sharing, so the replacement fails | `file_write_test`; the Windows-only case runs in W5-1 |
| AC-4 | A path with Japanese characters works for creating and replacing | `file_write_test` |
| AC-5 | The temporary path is in the destination's directory and differs between two calls | `file_write_test` |
| AC-6 | Mutation matrix, each mutation caught by a test that runs on Linux, except M2: M1, the temporary file is not removed after a failure; M2, the replace flag is dropped on Windows (only W5-1 catches it; record it as such); M3, the temporary file goes to the system TEMP folder; M4, the bytes are written straight to the destination (on Linux a replacement by rename gives the destination a new inode, which a write in place does not) | The Review runs M1, M3, and M4 in its sandbox; M2 is `not run` on Linux |
| AC-7 | Gates pass; G4 compiles the Windows implementation | G1 to G4 of [P4-T1](../P4-T1-timeout-test/packet.md#gates) |

## Review and merge

Review `R01` by a fresh Sonnet 5.5 subagent with the prompt of [tasks/README.md](../README.md#review-dispatch); profile `strict`, so the reviewer also runs AC-6's mutation matrix. After PASS, merge per [agents/README.md](../../README.md#workflow) and notify A003 with the merged commit.

## Next event

A004 starts after P5-T1, or in parallel on its own branch, and writes `receipt.md`.
