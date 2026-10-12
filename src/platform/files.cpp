#include "platform/files.h"

#include <cerrno>
#include <system_error>

namespace crisp::platform {

ReadFileResult ReadFileBytes(const std::filesystem::path& path, std::size_t maxBytes) {
    ReadFileResult result;
    std::FILE* file = OpenFileForReading(path);
    if (file == nullptr) {
        result.error = std::generic_category().message(errno);
        return result;
    }
    char buffer[16384];
    for (;;) {
        const std::size_t count = std::fread(buffer, 1, sizeof(buffer), file);
        if (result.bytes.size() + count > maxBytes) {
            result.bytes.clear();
            result.error = "The file is larger than " + std::to_string(maxBytes / 1024) + " KiB.";
            break;
        }
        result.bytes.append(buffer, count);
        if (count < sizeof(buffer)) {
            if (std::ferror(file) != 0) {
                result.bytes.clear();
                result.error = "The file could not be read completely.";
            }
            break;
        }
    }
    std::fclose(file);
    return result;
}

}  // namespace crisp::platform
