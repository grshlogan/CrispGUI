#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <string>
#include <string_view>

// Platform seam for file access, with Windows (files_win32.cpp) and POSIX (files_posix.cpp)
// implementations; files.cpp holds the platform-neutral part.
namespace crisp::platform {

// Opens |path| for binary reading: _wfopen on Windows, so any Unicode path works; fopen
// elsewhere. Returns nullptr with errno set on failure.
std::FILE* OpenFileForReading(const std::filesystem::path& path);

// Creates or truncates |path| for binary writing, with the same path handling. Returns nullptr
// with errno set on failure.
std::FILE* OpenFileForWriting(const std::filesystem::path& path);

// The size of the open |file| from its handle, so that it describes the same file that is read;
// false when the handle is not a regular file, such as a folder.
bool OpenFileSize(std::FILE* file, std::uint64_t& size);

// True for an entry that a directory listing hides by default: the hidden or system attribute
// on Windows, as PowerShell's Get-ChildItem without -Force; a leading dot elsewhere.
bool IsHiddenEntry(const std::filesystem::path& path);

struct ReadFileResult {
    std::string bytes;
    std::string error;  // empty on success

    bool Ok() const { return error.empty(); }
};

// Reads the whole file at |path|; a file larger than |maxBytes| is an error.
ReadFileResult ReadFileBytes(const std::filesystem::path& path, std::size_t maxBytes);

// The path of a new temporary file for |destination|, as "<name>.<16 hexadecimal digits>.tmp" in
// the destination's own directory, so that replacing the destination by it never crosses a
// volume. Two calls give two different paths. The system's temporary folder may lie on another
// volume and is never used.
std::filesystem::path AtomicTemporaryPath(const std::filesystem::path& destination);

// Hands what the C library still buffers for the open |file| to the system and then writes it to
// the disk: fsync on POSIX, FlushFileBuffers on Windows. False with the system's reason in
// |reason| when that fails.
bool FlushFileToDisk(std::FILE* file, std::string& reason);

// Replaces |destination| by |source| in one step, so that a reader sees the old file or the new
// one: rename on POSIX; MoveFileExW with MOVEFILE_REPLACE_EXISTING and MOVEFILE_WRITE_THROUGH on
// Windows, where any Unicode path works. False with the system's reason in |reason| when that
// fails, with both files as they were. (Not named ReplaceFile: <windows.h> defines that name as a
// macro, which would rename the Windows implementation.)
bool RenameOverFile(const std::filesystem::path& source, const std::filesystem::path& destination,
                    std::string& reason);

// Writes |bytes| to |path| so that a reader sees the old file or the whole new one, never a part
// (docs/SPEC.md, FR-3): the bytes go to a new temporary file next to |path|
// (AtomicTemporaryPath), are flushed to the disk, and the temporary file replaces |path|. On any
// failure the temporary file is removed, an existing |path| stays as it was, and the result is
// false with a message in |error| that names the step that failed.
bool WriteFileAtomically(const std::filesystem::path& path, std::string_view bytes, std::string& error);

}  // namespace crisp::platform
