#include "core/profile_store.h"

#include "core/jsonc.h"
#include "core/launch_command.h"
#include "platform/files.h"
#include "util/sha256.h"

#include <algorithm>
#include <cmath>
#include <system_error>
#include <utility>

namespace crisp::core {
namespace {

namespace fs = std::filesystem;

char ToLowerAscii(char c) {
    return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c;
}

int CompareIgnoringCase(std::string_view a, std::string_view b) {
    const std::size_t length = std::min(a.size(), b.size());
    for (std::size_t i = 0; i < length; ++i) {
        const char left = ToLowerAscii(a[i]);
        const char right = ToLowerAscii(b[i]);
        if (left != right) {
            return static_cast<unsigned char>(left) < static_cast<unsigned char>(right) ? -1 : 1;
        }
    }
    return a.size() == b.size() ? 0 : (a.size() < b.size() ? -1 : 1);
}

bool EqualsIgnoringCase(std::string_view a, std::string_view b) {
    return a.size() == b.size() && CompareIgnoringCase(a, b) == 0;
}

// The launcher sorts with PowerShell's Sort-Object, which ignores case; ties keep a stable order.
bool LessByName(std::string_view a, std::string_view b) {
    const int compared = CompareIgnoringCase(a, b);
    return compared != 0 ? compared < 0 : a < b;
}

std::string Quoted(std::string_view text) {
    return "\"" + std::string(text) + "\"";
}

bool StartsWith(std::string_view text, std::string_view prefix) {
    return text.substr(0, prefix.size()) == prefix;
}

// A number without a fractional part that a double holds exactly.
bool IsWholeNumber(const util::JsonValue& value) {
    if (!value.IsNumber()) {
        return false;
    }
    const double number = value.AsNumber();
    return std::floor(number) == number && std::fabs(number) <= 9007199254740992.0;
}

bool IsRegularFile(const fs::path& path) {
    std::error_code error;
    return !path.empty() && fs::is_regular_file(path, error);
}

struct StringKey {
    std::string_view key;
    std::string Profile::*field;
};

const StringKey kStringKeys[] = {
    {"comment", &Profile::comment}, {"build", &Profile::build},       {"backend", &Profile::backend},
    {"model", &Profile::model},     {"aligner", &Profile::aligner},   {"language", &Profile::language},
    {"download", &Profile::download}, {"host", &Profile::host},       {"api_keys", &Profile::apiKeys},
};

constexpr std::string_view kRequiredKeys[] = {"build", "backend", "model"};

// Top-level keys that appear more than once: identical spellings keep the last value, as the
// launcher's ConvertFrom-Json does; spellings that differ only in case are rejected, as
// ConvertFrom-Json rejects them.
bool CheckDuplicateKeys(Profile& profile) {
    const util::JsonValue& raw = profile.raw;
    for (std::size_t i = 0; i < raw.Size(); ++i) {
        for (std::size_t j = 0; j < i; ++j) {
            if (!EqualsIgnoringCase(raw.KeyAt(i), raw.KeyAt(j))) {
                continue;
            }
            if (raw.KeyAt(i) != raw.KeyAt(j)) {
                profile.error = "The keys " + Quoted(raw.KeyAt(j)) + " and " + Quoted(raw.KeyAt(i)) +
                                " differ only in case; the launcher rejects such a file.";
                return false;
            }
            profile.warnings.push_back("The key " + Quoted(raw.KeyAt(i)) +
                                       " appears more than once; the last value counts, as in the launcher.");
            break;
        }
    }
    return true;
}

bool ReadKnownKeys(Profile& profile) {
    for (const StringKey& entry : kStringKeys) {
        const util::JsonValue* value = profile.Find(entry.key);
        if (value == nullptr) {
            continue;
        }
        if (!value->IsString()) {
            profile.error = Quoted(entry.key) + " must be a string.";
            return false;
        }
        profile.*entry.field = value->AsString();
    }

    if (const util::JsonValue* device = profile.Find("device")) {
        if (device->IsString()) {
            profile.device = device->AsString();
        } else if (IsWholeNumber(*device)) {
            profile.device = FormatLauncherNumber(*device);
        } else {
            profile.error = "\"device\" must be a string such as \"1\".";
            return false;
        }
    }

    if (const util::JsonValue* port = profile.Find("port")) {
        if (!IsWholeNumber(*port) || port->AsNumber() < 0.0 || port->AsNumber() > 65535.0) {
            profile.error = "\"port\" must be a whole number up to 65535.";
            return false;
        }
        profile.port = static_cast<int>(port->AsNumber());
    }

    if (const util::JsonValue* env = profile.Find("env")) {
        if (!env->IsObject()) {
            profile.error = "\"env\" must be an object of variable names and values.";
            return false;
        }
        for (std::size_t i = 0; i < env->Size(); ++i) {
            const std::string& name = env->KeyAt(i);
            const util::JsonValue& value = env->At(i);
            if (name.empty() || name.find('=') != std::string::npos) {
                profile.error = "\"env\" holds an invalid variable name: " + Quoted(name) + ".";
                return false;
            }
            if (value.IsString()) {
                profile.env.emplace_back(name, value.AsString());
            } else if (value.IsNumber()) {
                profile.env.emplace_back(name, FormatLauncherNumber(value));
            } else {
                profile.error = "The value of " + Quoted(name) + " in \"env\" must be a string.";
                return false;
            }
        }
    }

    if (const util::JsonValue* args = profile.Find("args")) {
        if (!args->IsArray()) {
            profile.error = "\"args\" must be an array of strings.";
            return false;
        }
        for (std::size_t i = 0; i < args->Size(); ++i) {
            const util::JsonValue& item = args->At(i);
            if (item.IsString()) {
                profile.args.push_back(item.AsString());
            } else if (item.IsNumber()) {
                profile.args.push_back(FormatLauncherNumber(item));
            } else {
                profile.error = "Item " + std::to_string(i + 1) + " of \"args\" must be a string.";
                return false;
            }
        }
    }

    for (std::size_t i = 0; i < TuningKeyCount(); ++i) {
        const TuningKey& key = TuningKeyAt(i);
        const util::JsonValue* value = profile.Find(key.key);
        if (value == nullptr) {
            continue;
        }
        switch (key.kind) {
        case TuningValueKind::kBoolean:
            if (!value->IsBoolean()) {
                profile.error = Quoted(key.key) + " must be true or false.";
                return false;
            }
            break;
        case TuningValueKind::kInteger:
            if (!IsWholeNumber(*value)) {
                profile.error = Quoted(key.key) + " must be a whole number.";
                return false;
            }
            break;
        case TuningValueKind::kNumber:
            if (!value->IsNumber()) {
                profile.error = Quoted(key.key) + " must be a number.";
                return false;
            }
            break;
        }
    }

    for (std::string_view key : kRequiredKeys) {
        const util::JsonValue* value = profile.Find(key);
        if (value == nullptr || value->AsString().empty()) {
            profile.error = Quoted(key) + " is missing.";
            return false;
        }
    }
    return true;
}

}  // namespace

CrispAsrPaths MakeCrispAsrPaths(const fs::path& root) {
    CrispAsrPaths paths;
    paths.root = root;
    if (!root.empty()) {
        paths.profiles = root / "profile";
        paths.models = root / "models";
    }
    return paths;
}

const util::JsonValue* Profile::Find(std::string_view key) const {
    const util::JsonValue* found = nullptr;
    if (raw.IsObject()) {
        for (std::size_t i = 0; i < raw.Size(); ++i) {
            if (EqualsIgnoringCase(raw.KeyAt(i), key)) {
                found = &raw.At(i);
            }
        }
    }
    return found != nullptr && !found->IsNull() ? found : nullptr;
}

std::size_t ProfileScan::ProfileCount() const {
    std::size_t count = 0;
    for (const ProfileFolder& folder : folders) {
        count += folder.profiles.size();
    }
    return count;
}

fs::path JoinProfilePath(const fs::path& base, const std::string& relative) {
    std::string text = relative;
    std::replace(text.begin(), text.end(), '\\', '/');
    text.erase(0, std::min(text.find_first_not_of('/'), text.size()));
    const bool driveLetter =
        text.size() >= 2 && text[1] == ':' && ((text[0] >= 'A' && text[0] <= 'Z') || (text[0] >= 'a' && text[0] <= 'z'));
    if (driveLetter) {
        return fs::path();
    }
    fs::path joined = base / fs::u8path(text);
    joined.make_preferred();
    return joined;
}

void ParseProfile(std::string_view bytes, Profile& profile) {
    std::string_view text = bytes;
    if (StartsWith(text, "\xEF\xBB\xBF")) {
        text.remove_prefix(3);  // UTF-8 byte order mark, which Get-Content -Encoding UTF8 drops as well
    } else if (StartsWith(text, "\xFF\xFE") || StartsWith(text, "\xFE\xFF")) {
        profile.error = "The file is saved as UTF-16; save it as UTF-8.";
        return;
    }
    const util::JsonParseResult parsed = ParseJsonc(text);
    if (!parsed.Ok()) {
        profile.error = "Not valid JSONC at line " + std::to_string(parsed.line) + ", column " +
                        std::to_string(parsed.column) + ": " + parsed.error;
        return;
    }
    if (!parsed.value.IsObject()) {
        profile.error = "The file must hold one JSON object.";
        return;
    }
    profile.raw = parsed.value;
    if (!CheckDuplicateKeys(profile) || !ReadKnownKeys(profile)) {
        return;
    }
    const ProfileStartup startup = ReadProfileStartup(profile);
    for (const std::string& problem : startup.parsed.problems) {
        profile.warnings.push_back(problem);
    }
}

void CheckReadiness(const CrispAsrPaths& paths, Profile& profile) {
    if (!profile.Usable() || paths.root.empty()) {
        return;
    }
    const fs::path buildDirectory = JoinProfilePath(paths.root, profile.build);
    if (buildDirectory.empty()) {
        profile.warnings.push_back("\"build\" must name a folder inside the CrispASR folder, without a drive letter.");
    } else {
        profile.executablePath = buildDirectory / kServerExecutableName;
        profile.executableOk = IsRegularFile(profile.executablePath);
    }

    profile.modelPath = JoinProfilePath(paths.models, profile.model);
    if (profile.modelPath.empty()) {
        profile.warnings.push_back("\"model\" must be a path inside the models folder, without a drive letter.");
    }
    profile.modelOk = IsRegularFile(profile.modelPath);

    if (profile.HasAligner()) {
        profile.alignerPath = JoinProfilePath(paths.models, profile.aligner);
        if (profile.alignerPath.empty()) {
            profile.warnings.push_back("\"aligner\" must be a path inside the models folder, without a drive letter.");
        }
        profile.alignerOk = IsRegularFile(profile.alignerPath);
    }
}

Profile LoadProfile(const fs::path& file, const CrispAsrPaths& paths) {
    Profile profile;
    profile.path = file;
    profile.folder = file.parent_path().filename().u8string();
    profile.name = file.stem().u8string();

    const platform::ReadFileResult read = platform::ReadFileBytes(file, kMaxProfileBytes);
    if (!read.Ok()) {
        profile.error = "The file could not be read: " + read.error;
        return profile;
    }
    profile.contentHash = util::Sha256Hex(read.bytes);
    ParseProfile(read.bytes, profile);
    CheckReadiness(paths, profile);
    return profile;
}

ProfileScan ScanProfiles(const fs::path& crispAsrRoot) {
    ProfileScan scan;
    scan.paths = MakeCrispAsrPaths(crispAsrRoot);
    if (crispAsrRoot.empty()) {
        scan.error = "No CrispASR folder is set.";
        return scan;
    }

    std::error_code error;
    if (!fs::is_directory(scan.paths.profiles, error)) {
        scan.error = "There is no profile folder at " + scan.paths.profiles.u8string() + ".";
        return scan;
    }

    fs::directory_iterator folders(scan.paths.profiles, error);
    if (error) {
        scan.error = "The profile folder could not be listed: " + error.message();
        return scan;
    }
    for (; folders != fs::directory_iterator(); folders.increment(error)) {
        if (error) {
            scan.error = "The profile folder could not be listed: " + error.message();
            return scan;
        }
        std::error_code entryError;
        if (!folders->is_directory(entryError) || platform::IsHiddenEntry(folders->path())) {
            continue;
        }
        ProfileFolder folder;
        folder.name = folders->path().filename().u8string();
        folder.path = folders->path();
        scan.folders.push_back(std::move(folder));
    }
    if (error) {
        scan.error = "The profile folder could not be listed: " + error.message();
        return scan;
    }
    std::sort(scan.folders.begin(), scan.folders.end(),
              [](const ProfileFolder& a, const ProfileFolder& b) { return LessByName(a.name, b.name); });

    for (ProfileFolder& folder : scan.folders) {
        std::vector<fs::path> files;
        std::error_code listError;
        fs::directory_iterator entries(folder.path, listError);
        for (; !listError && entries != fs::directory_iterator(); entries.increment(listError)) {
            std::error_code entryError;
            const fs::path& path = entries->path();
            const std::string extension = path.extension().u8string();
            if (!entries->is_regular_file(entryError) || platform::IsHiddenEntry(path) ||
                !(EqualsIgnoringCase(extension, ".json") || EqualsIgnoringCase(extension, ".conf"))) {
                continue;
            }
            files.push_back(path);
        }
        if (listError) {
            folder.error = "The folder could not be listed: " + listError.message();
        }
        std::sort(files.begin(), files.end(), [](const fs::path& a, const fs::path& b) {
            return LessByName(a.filename().u8string(), b.filename().u8string());
        });
        for (const fs::path& file : files) {
            folder.profiles.push_back(LoadProfile(file, scan.paths));
        }
    }
    return scan;
}

}  // namespace crisp::core
