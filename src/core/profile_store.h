#pragma once

#include "util/json.h"

#include <cstddef>
#include <filesystem>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// Reading CrispASR profiles: the JSONC files under <CrispASR root>/profile/<folder>/ that
// configure one model each (SPEC FR-2; docs/ARCHITECTURE.md, section 2.1). The GUI never
// writes them. The rules follow the owner's launcher, reference/crispasr/start-crispasr.ps1.
namespace crisp::core {

// The parts of a CrispASR deployment the GUI reads (docs/ENVIRONMENT.md, section 5).
struct CrispAsrPaths {
    std::filesystem::path root;
    std::filesystem::path profiles;  // <root>/profile
    std::filesystem::path models;    // <root>/models
};

CrispAsrPaths MakeCrispAsrPaths(const std::filesystem::path& root);

// The server executable inside <root>/<build>/.
#if defined(_WIN32)
constexpr const char* kServerExecutableName = "crispasr.exe";
#else
constexpr const char* kServerExecutableName = "crispasr";
#endif

// Larger files are not read as profiles; real ones are a few kilobytes.
constexpr std::size_t kMaxProfileBytes = 1024 * 1024;

// One profile file.
struct Profile {
    std::filesystem::path path;
    std::string folder;       // its folder under profile/, such as "Japanese"
    std::string name;         // the file name without its extension, as the launcher shows it
    std::string contentHash;  // SHA-256 of the file's bytes; empty when unreadable

    // Why the profile cannot be used: the file is unreadable or not JSONC, a required key is
    // missing, or a known key has the wrong type. Empty when the profile can be used.
    std::string error;
    // The profile can be used, but something may not work as intended.
    std::vector<std::string> warnings;

    // The known keys; empty strings and port 0 when absent.
    std::string comment, build, backend, model, aligner, language, download, host, device, apiKeys;
    int port = 0;
    std::vector<std::pair<std::string, std::string>> env;  // in file order
    std::vector<std::string> args;                         // passed to the server verbatim
    util::JsonValue raw;  // the whole object, tuning keys and unknown keys included

    // Readiness as the launcher checks it; filled for a usable profile only.
    std::filesystem::path executablePath;  // <root>/<build>/crispasr.exe
    std::filesystem::path modelPath;       // <models>/<model>
    std::filesystem::path alignerPath;     // <models>/<aligner>; empty without an aligner
    bool executableOk = false;
    bool modelOk = false;
    bool alignerOk = false;

    bool Usable() const { return error.empty(); }
    bool HasAligner() const { return !aligner.empty(); }

    // A top-level value by key. Keys are compared without regard to case, as PowerShell does
    // for the launcher. nullptr when the key is absent or null.
    const util::JsonValue* Find(std::string_view key) const;
};

struct ProfileFolder {
    std::string name;
    std::filesystem::path path;
    std::string error;              // why the folder could not be listed
    std::vector<Profile> profiles;  // sorted by file name
};

struct ProfileScan {
    CrispAsrPaths paths;
    std::string error;                   // why the profile folder could not be listed
    std::vector<ProfileFolder> folders;  // sorted by name

    bool Ok() const { return error.empty(); }
    std::size_t ProfileCount() const;
};

// Fills the known keys, error, and warnings of |profile| from the bytes of its file.
void ParseProfile(std::string_view bytes, Profile& profile);

// Checks the executable, model, and aligner of a usable |profile| against |paths|.
void CheckReadiness(const CrispAsrPaths& paths, Profile& profile);

// Reads, hashes, parses, and checks one profile file.
Profile LoadProfile(const std::filesystem::path& file, const CrispAsrPaths& paths);

// Lists the folders under <root>/profile and the .json and .conf files in each, as the launcher
// does: hidden entries, other extensions, and files directly inside profile/ are skipped.
ProfileScan ScanProfiles(const std::filesystem::path& crispAsrRoot);

// Joins a path from a profile, written for Windows, below |base| as the launcher's Join-Path
// does: backslashes and slashes both separate, and leading separators are dropped. Returns an
// empty path for a path with a drive letter, which cannot lie below |base|.
std::filesystem::path JoinProfilePath(const std::filesystem::path& base, const std::string& relative);

}  // namespace crisp::core
