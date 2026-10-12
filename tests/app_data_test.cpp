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

    // The process's temporary-file folder is now ours. GetTempPathW, behind temp_directory_path,
    // ends the folder with a separator, and with MinGW-w64 fs::equivalent hands the path to
    // msvcrt's _wstat64, which reports such a path as missing; so the folder itself is compared
    // (docs/ENVIRONMENT.md, section 4.5). The comparison with the separator is printed as
    // evidence of that behavior.
    const fs::path systemTemp = fs::temp_directory_path(error);
    CHECK(!error);
    const fs::path systemTempFolder = systemTemp.has_filename() ? systemTemp : systemTemp.parent_path();
    std::error_code sameError;
    CHECK(fs::equivalent(systemTempFolder, prepared.layout.temp, sameError));
    std::error_code rawError;
    const bool rawSame = fs::equivalent(systemTemp, prepared.layout.temp, rawError);
    std::printf("app_data_test: temp_directory_path %s\n", systemTemp.u8string().c_str());
    std::printf("app_data_test: equivalent with that path as given: %s (%s)\n", rawSame ? "true" : "false",
                rawError ? rawError.message().c_str() : "no error");

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
