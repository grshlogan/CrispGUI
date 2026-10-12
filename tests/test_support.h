#pragma once

// Helpers shared by the unit tests that need files.

#include "platform/paths.h"

#include <cstdio>
#include <filesystem>
#include <string>
#include <system_error>

namespace crisp::test {

// The test data the build copies next to the test executables (tests/CMakeLists.txt); a Windows
// delivery carries it in testdata/ next to the tests.
inline std::filesystem::path TestDataDirectory() {
    return platform::ExecutableDirectory() / "testdata";
}

// An empty directory |name| below the working directory.
inline std::filesystem::path FreshScratchDirectory(const std::string& name) {
    std::error_code error;
    const std::filesystem::path directory = std::filesystem::current_path(error) / name;
    std::filesystem::remove_all(directory, error);
    std::filesystem::create_directories(directory, error);
    return directory;
}

// Writes |bytes| to |path|, creating its parent folders; any Unicode path works on Windows.
inline bool WriteFile(const std::filesystem::path& path, const std::string& bytes) {
    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);
#if defined(_WIN32)
    std::FILE* file = _wfopen(path.c_str(), L"wb");
#else
    std::FILE* file = std::fopen(path.c_str(), "wb");
#endif
    if (file == nullptr) {
        return false;
    }
    const bool written = std::fwrite(bytes.data(), 1, bytes.size(), file) == bytes.size();
    return std::fclose(file) == 0 && written;
}

}  // namespace crisp::test
