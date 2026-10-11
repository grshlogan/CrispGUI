#pragma once

#include <filesystem>
#include <string>

namespace crisp::app {

// Where the GUI keeps its own data: settings, logs, job state, and temporary files
// (docs/ENVIRONMENT.md, section 5). On Windows it must stay off C:\ (AGENTS.md, hard rule 4).
struct AppDataLayout {
    std::filesystem::path root;
    std::filesystem::path temp;  // root/tmp, also TMP and TEMP of the GUI process
};

struct AppDataResult {
    AppDataLayout layout;
    std::string error;  // empty on success; otherwise what failed, for the UI to show
    bool onSystemDrive = false;

    bool Ok() const { return error.empty(); }
};

// <executable directory>/appdata; empty when the executable directory is unknown.
std::filesystem::path DefaultAppDataRoot();

// Creates |root| and |root|/tmp and points the process's temporary-file variables at
// |root|/tmp. Failures are reported in the result.
AppDataResult PrepareAppData(const std::filesystem::path& root);

}  // namespace crisp::app
