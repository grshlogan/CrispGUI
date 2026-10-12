#include "platform/files.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <io.h>
#include <sys/stat.h>

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

}  // namespace crisp::platform
