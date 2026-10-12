#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <string>

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

}  // namespace crisp::platform
