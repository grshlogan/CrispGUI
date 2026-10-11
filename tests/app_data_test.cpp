#include "check.h"

#include "app/app_data.h"
#include "platform/paths.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

namespace fs = std::filesystem;
using crisp::app::AppDataResult;
using crisp::app::PrepareAppData;
using crisp::platform::GetEnvironmentUtf8;

int main() {
    std::error_code error;
    const fs::path scratch = fs::current_path(error) / "app_data_test.scratch";
    CHECK(!error);
    fs::remove_all(scratch, error);

    // Success: both folders exist, and the temporary-file variables point at tmp/.
    const AppDataResult prepared = PrepareAppData(scratch / "appdata");
    CHECK(prepared.Ok());
    CHECK(prepared.layout.temp == prepared.layout.root / "tmp");
    CHECK(fs::is_directory(prepared.layout.temp, error));
    const std::string temp = prepared.layout.temp.u8string();
    CHECK(GetEnvironmentUtf8("TMP") == temp);
    CHECK(GetEnvironmentUtf8("TEMP") == temp);
#if !defined(_WIN32)
    CHECK(GetEnvironmentUtf8("TMPDIR") == temp);
#endif
    CHECK(fs::equivalent(fs::temp_directory_path(error), prepared.layout.temp, error));

    // Failure: the root is a regular file, so tmp/ cannot be created and the variables stay.
    const fs::path blocker = scratch / "blocker";
    {
        std::ofstream file(blocker);
        file << "not a folder";
    }
    const AppDataResult failed = PrepareAppData(blocker);
    CHECK(!failed.Ok());
    CHECK(!failed.error.empty());
    CHECK(GetEnvironmentUtf8("TMP") == temp);

    // An unknown executable directory is reported, not guessed.
    CHECK(!PrepareAppData(fs::path()).Ok());
    CHECK(crisp::app::DefaultAppDataRoot().filename() == "appdata");

    fs::remove_all(scratch, error);
    std::printf("app_data_test: temp folder %s\n", temp.c_str());
    return crisp::test::Result();
}
