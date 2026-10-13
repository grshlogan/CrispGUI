#include "platform/files.h"

#include "platform/win32_text.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <io.h>
#include <sys/stat.h>

#include <cerrno>
#include <cstdio>
#include <system_error>

namespace crisp::platform {

std::FILE* OpenFileForReading(const std::filesystem::path& path) {
    return _wfopen(path.c_str(), L"rb");
}

std::FILE* OpenFileForWriting(const std::filesystem::path& path) {
    return _wfopen(path.c_str(), L"wb");
}

bool OpenFileSize(std::FILE* file, std::uint64_t& size) {
    struct _stat64 status;
    if (_fstat64(_fileno(file), &status) != 0 || (status.st_mode & _S_IFMT) != _S_IFREG || status.st_size < 0) {
        return false;
    }
    size = static_cast<std::uint64_t>(status.st_size);
    return true;
}

bool IsHiddenEntry(const std::filesystem::path& path) {
    const DWORD attributes = GetFileAttributesW(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & (FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM)) != 0;
}

bool FlushFileToDisk(std::FILE* file, std::string& reason) {
    if (std::fflush(file) != 0) {
        reason = std::generic_category().message(errno);
        return false;
    }
    const intptr_t handle = _get_osfhandle(_fileno(file));
    if (handle == -1) {
        reason = std::generic_category().message(errno);
        return false;
    }
    if (!FlushFileBuffers(reinterpret_cast<HANDLE>(handle))) {
        reason = win32::ErrorMessage(GetLastError());
        return false;
    }
    return true;
}

bool RenameOverFile(const std::filesystem::path& source, const std::filesystem::path& destination,
                    std::string& reason) {
    if (!MoveFileExW(source.c_str(), destination.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        reason = win32::ErrorMessage(GetLastError());
        return false;
    }
    return true;
}

}  // namespace crisp::platform
