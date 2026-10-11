#include "app/app_data.h"

#include "platform/paths.h"

#include <system_error>

namespace crisp::app {

std::filesystem::path DefaultAppDataRoot() {
    const std::filesystem::path executableDirectory = platform::ExecutableDirectory();
    return executableDirectory.empty() ? std::filesystem::path() : executableDirectory / "appdata";
}

AppDataResult PrepareAppData(const std::filesystem::path& root) {
    AppDataResult result;
    if (root.empty()) {
        result.error = "The executable directory could not be determined.";
        return result;
    }

    std::error_code error;
    const std::filesystem::path absoluteRoot = std::filesystem::absolute(root, error);
    if (error) {
        result.error = "Could not resolve " + root.u8string() + ": " + error.message();
        return result;
    }
    result.layout.root = absoluteRoot;
    result.layout.temp = absoluteRoot / "tmp";
    result.onSystemDrive = platform::IsOnSystemDrive(absoluteRoot);

    std::filesystem::create_directories(result.layout.temp, error);
    if (error) {
        result.error = "Could not create " + result.layout.temp.u8string() + ": " + error.message() + " (" +
                       std::to_string(error.value()) + ")";
        return result;
    }
    if (!platform::SetProcessTempDirectory(result.layout.temp)) {
        result.error = "Could not point TMP and TEMP at " + result.layout.temp.u8string();
    }
    return result;
}

}  // namespace crisp::app
