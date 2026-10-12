#include "platform/files.h"

#include <sys/stat.h>

namespace crisp::platform {

std::FILE* OpenFileForReading(const std::filesystem::path& path) {
    return std::fopen(path.c_str(), "rb");
}

std::FILE* OpenFileForWriting(const std::filesystem::path& path) {
    return std::fopen(path.c_str(), "wb");
}

bool OpenFileSize(std::FILE* file, std::uint64_t& size) {
    struct stat status;
    if (fstat(fileno(file), &status) != 0 || !S_ISREG(status.st_mode) || status.st_size < 0) {
        return false;
    }
    size = static_cast<std::uint64_t>(status.st_size);
    return true;
}

bool IsHiddenEntry(const std::filesystem::path& path) {
    const std::string name = path.filename().string();
    return !name.empty() && name[0] == '.';
}

}  // namespace crisp::platform
