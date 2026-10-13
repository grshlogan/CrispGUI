#include "platform/files.h"

#include <sys/stat.h>
#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <system_error>

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

bool FlushFileToDisk(std::FILE* file, std::string& reason) {
    if (std::fflush(file) != 0 || fsync(fileno(file)) != 0) {
        reason = std::generic_category().message(errno);
        return false;
    }
    return true;
}

bool RenameOverFile(const std::filesystem::path& source, const std::filesystem::path& destination,
                    std::string& reason) {
    if (std::rename(source.c_str(), destination.c_str()) != 0) {
        reason = std::generic_category().message(errno);
        return false;
    }
    return true;
}

}  // namespace crisp::platform
