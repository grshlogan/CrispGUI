#include "ui/fonts.h"

#include "platform/paths.h"

#include <filesystem>
#include <system_error>

namespace crisp::ui {
namespace {

// The same files as CRISPGUI_UI_FONTS in CMakeLists.txt.
constexpr const char* kTextFont = "fonts/HarmonyOS_Sans_SC_Regular.ttf";
constexpr const char* kMediumFont = "fonts/HarmonyOS_Sans_SC_Medium.ttf";
constexpr const char* kTitleFont = "fonts/HarmonyOS_Sans_SC_Bold.ttf";

std::string IfPresent(const std::filesystem::path& directory, const char* relative) {
    std::error_code error;
    return std::filesystem::is_regular_file(directory / relative, error) ? std::string(relative) : std::string();
}

}  // namespace

UiFonts FindUiFonts() {
    UiFonts fonts;
    const std::filesystem::path directory = platform::ExecutableDirectory();
    if (!directory.empty()) {
        fonts.text = IfPresent(directory, kTextFont);
        fonts.medium = IfPresent(directory, kMediumFont);
        fonts.title = IfPresent(directory, kTitleFont);
    }
    return fonts;
}

}  // namespace crisp::ui
