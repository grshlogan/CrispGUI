#include "platform/process.h"

#include <cstdio>

namespace crisp::platform {

std::string DescribeExitStatus(const ExitStatus& status) {
    if (status.signaled) {
        return "signal " + std::to_string(status.code);
    }
    std::string text = "exit code " + std::to_string(status.code);
    if (status.code > 0x7FFFFFFF) {
        // A Windows status such as an access violation reads better in hexadecimal.
        char hex[16];
        std::snprintf(hex, sizeof(hex), "0x%08llX", static_cast<unsigned long long>(status.code));
        text += " (" + std::string(hex) + ")";
    }
    return text;
}

}  // namespace crisp::platform
