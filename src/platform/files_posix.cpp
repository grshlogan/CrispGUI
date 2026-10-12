#include "platform/files.h"

namespace crisp::platform {

std::FILE* OpenFileForReading(const std::filesystem::path& path) {
    return std::fopen(path.c_str(), "rb");
}

bool IsHiddenEntry(const std::filesystem::path& path) {
    const std::string name = path.filename().string();
    return !name.empty() && name[0] == '.';
}

}  // namespace crisp::platform
