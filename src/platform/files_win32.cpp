#include "platform/files.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace crisp::platform {

std::FILE* OpenFileForReading(const std::filesystem::path& path) {
    return _wfopen(path.c_str(), L"rb");
}

bool IsHiddenEntry(const std::filesystem::path& path) {
    const DWORD attributes = GetFileAttributesW(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & (FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM)) != 0;
}

}  // namespace crisp::platform
