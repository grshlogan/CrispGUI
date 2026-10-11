#pragma once

#include <filesystem>
#include <string>

// Platform seam for process paths and environment variables, with Windows (paths_win32.cpp)
// and POSIX (paths_posix.cpp) implementations.
namespace crisp::platform {

// The directory of the running executable; empty when it cannot be determined.
std::filesystem::path ExecutableDirectory();

// Points this process's temporary-file variables at |directory|: TMP and TEMP on Windows;
// TMPDIR, TMP, and TEMP elsewhere. Child processes inherit them. Returns false when any of
// them could not be set.
bool SetProcessTempDirectory(const std::filesystem::path& directory);

// The value of environment variable |name| as UTF-8; empty when it is not set.
std::string GetEnvironmentUtf8(const char* name);

// True when |path| is on the Windows system drive (%SystemDrive%); always false elsewhere.
bool IsOnSystemDrive(const std::filesystem::path& path);

}  // namespace crisp::platform
