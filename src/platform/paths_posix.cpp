#include "platform/paths.h"

#include <unistd.h>

#include <cstdlib>
#include <initializer_list>
#include <vector>

namespace crisp::platform {

std::filesystem::path ExecutableDirectory() {
    std::vector<char> buffer(4096);
    const ssize_t length = readlink("/proc/self/exe", buffer.data(), buffer.size() - 1);
    if (length <= 0) {
        return {};
    }
    return std::filesystem::path(std::string(buffer.data(), static_cast<std::size_t>(length))).parent_path();
}

bool SetProcessTempDirectory(const std::filesystem::path& directory) {
    const std::string value = directory.string();
    bool ok = true;
    for (const char* name : {"TMPDIR", "TMP", "TEMP"}) {
        ok = setenv(name, value.c_str(), 1) == 0 && ok;
    }
    return ok;
}

std::string GetEnvironmentUtf8(const char* name) {
    const char* value = std::getenv(name);
    return value != nullptr ? value : "";
}

bool IsOnSystemDrive(const std::filesystem::path&) {
    return false;
}

}  // namespace crisp::platform
