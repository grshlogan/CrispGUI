#include "app/crispasr_location.h"

#include "platform/paths.h"

#include <string>

namespace crisp::app {

std::filesystem::path DefaultCrispAsrRoot() {
    const std::string configured = platform::GetEnvironmentUtf8(kCrispAsrRootVariable);
    if (!configured.empty()) {
        return std::filesystem::u8path(configured);
    }
#if defined(_WIN32)
    return std::filesystem::u8path("E:\\AI\\ASR\\CrispASR");
#else
    return std::filesystem::path();
#endif
}

}  // namespace crisp::app
