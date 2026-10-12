#pragma once

#include <filesystem>

namespace crisp::app {

// The environment variable that overrides the CrispASR folder, for development runs on Linux
// and for testing; documented in docs/ENVIRONMENT.md, section 5.
constexpr const char* kCrispAsrRootVariable = "CRISPGUI_CRISPASR_ROOT";

// The CrispASR deployment the GUI reads until the Settings page stores one (Phase 8): the
// folder in CRISPGUI_CRISPASR_ROOT when it is set, otherwise E:\AI\ASR\CrispASR on Windows
// and nothing elsewhere.
std::filesystem::path DefaultCrispAsrRoot();

}  // namespace crisp::app
