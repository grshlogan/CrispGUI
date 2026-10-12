#include "core/launch_command.h"

#include "core/profile_store.h"
#include "platform/paths.h"

#include <algorithm>
#include <system_error>

namespace crisp::core {
namespace {

namespace fs = std::filesystem;

bool IsRegularFile(const fs::path& path) {
    std::error_code error;
    return !path.empty() && fs::is_regular_file(path, error);
}

// Environment names compare as the platform compares them.
bool SameVariable(const std::string& a, const std::string& b) {
#if defined(_WIN32)
    if (a.size() != b.size()) {
        return false;
    }
    for (std::size_t i = 0; i < a.size(); ++i) {
        const auto upper = [](char c) { return c >= 'a' && c <= 'z' ? static_cast<char>(c - 'a' + 'A') : c; };
        if (upper(a[i]) != upper(b[i])) {
            return false;
        }
    }
    return true;
#else
    return a == b;
#endif
}

// The first non-empty key of a comma-separated list, trimmed as the server trims it.
std::string FirstKey(const std::string& keys) {
    std::size_t start = 0;
    while (start <= keys.size()) {
        std::size_t end = keys.find(',', start);
        if (end == std::string::npos) {
            end = keys.size();
        }
        std::string key = keys.substr(start, end - start);
        key.erase(0, std::min(key.find_first_not_of(" \t"), key.size()));
        key.erase(key.find_last_not_of(" \t") + 1);
        if (!key.empty()) {
            return key;
        }
        start = end + 1;
    }
    return {};
}

// start-crispasr.ps1:203-212, in its order.
constexpr TuningKey kTuningKeys[] = {
    {"vad", "--vad", TuningValueKind::kBoolean, ApplyCondition::kTruthy},
    {"force_aligner", "--force-aligner", TuningValueKind::kBoolean, ApplyCondition::kTruthy},
    {"chunk_seconds", "--chunk-seconds", TuningValueKind::kInteger, ApplyCondition::kPresent},
    {"beam_size", "-bs", TuningValueKind::kInteger, ApplyCondition::kTruthy},
    {"temperature", "-tp", TuningValueKind::kNumber, ApplyCondition::kGreaterThanZero},
    {"frequency_penalty", "--frequency-penalty", TuningValueKind::kNumber, ApplyCondition::kGreaterThanZero},
    {"max_new_tokens", "-n", TuningValueKind::kInteger, ApplyCondition::kTruthy},
    {"seed", "--seed", TuningValueKind::kInteger, ApplyCondition::kGreaterThanZero},
    {"no_punctuation", "--no-punctuation", TuningValueKind::kBoolean, ApplyCondition::kTruthy},
    {"split_on_punct", "-sp", TuningValueKind::kBoolean, ApplyCondition::kTruthy},
};

}  // namespace

std::size_t TuningKeyCount() {
    return sizeof(kTuningKeys) / sizeof(kTuningKeys[0]);
}

const TuningKey& TuningKeyAt(std::size_t index) {
    return kTuningKeys[index];
}

bool ConditionHolds(ApplyCondition condition, const util::JsonValue& value) {
    switch (condition) {
    case ApplyCondition::kTruthy:
        return value.IsBoolean() ? value.AsBoolean() : value.IsNumber() && value.AsNumber() != 0.0;
    case ApplyCondition::kPresent:
        return !value.IsNull();
    case ApplyCondition::kGreaterThanZero:
        return value.IsNumber() && value.AsNumber() > 0.0;
    }
    return false;
}

std::string FormatLauncherNumber(const util::JsonValue& number) {
    return number.IsIntegerLiteral() ? number.NumberText() : util::FormatJsonNumber(number.AsNumber());
}

std::string ProfileStartup::Text() const {
    std::string text;
    for (const StartupArgument& argument : arguments) {
        if (!text.empty()) {
            text.push_back(' ');
        }
        const bool quote = argument.text.empty() || argument.text.find(' ') != std::string::npos;
        text += quote ? "\"" + argument.text + "\"" : argument.text;
    }
    return text;
}

std::string LaunchCommand::CommandLineText() const {
    std::string text = platform::QuoteWindowsArgument(process.executable.u8string());
    for (const std::string& argument : process.arguments) {
        text.push_back(' ');
        text += platform::QuoteWindowsArgument(argument);
    }
    return text;
}

ProfileStartup ReadProfileStartup(const Profile& profile) {
    ProfileStartup startup;
    for (const TuningKey& key : kTuningKeys) {
        const util::JsonValue* value = profile.Find(key.key);
        if (value == nullptr || !ConditionHolds(key.condition, *value)) {
            continue;
        }
        startup.arguments.push_back({std::string(key.flag), std::string(key.key)});
        if (key.kind != TuningValueKind::kBoolean) {
            startup.arguments.push_back({FormatLauncherNumber(*value), std::string(key.key)});
        }
    }
    for (const std::string& argument : profile.args) {
        startup.arguments.push_back({argument, "args"});
    }
    std::vector<std::string> texts;
    texts.reserve(startup.arguments.size());
    for (const StartupArgument& argument : startup.arguments) {
        texts.push_back(argument.text);
    }
    startup.parsed = ParseCliArguments(texts);
    return startup;
}

LaunchCommand BuildLaunchCommand(const Profile& profile, const CrispAsrPaths& paths, const LaunchOptions& options) {
    LaunchCommand command;
    if (!profile.Usable()) {
        command.error = "The profile cannot be used: " + profile.error;
        return command;
    }
    if (profile.executablePath.empty()) {
        command.error = "\"build\" must name a folder inside the CrispASR folder.";
        return command;
    }
    if (!IsRegularFile(profile.executablePath)) {
        command.error = "The server program is missing: " + profile.executablePath.u8string();
        return command;
    }
    if (!IsRegularFile(profile.modelPath)) {
        command.error = "The model file is missing: " +
                        (profile.modelPath.empty() ? profile.model : profile.modelPath.u8string());
        return command;
    }
    if (options.serverTemp.empty()) {
        command.error = "The app data folder is unavailable, and the server needs it for TMP and TEMP.";
        return command;
    }

    // start-crispasr.ps1:190-221, in its order.
    std::vector<std::string>& arguments = command.process.arguments;
    arguments = {"--server", "--backend", profile.backend, "-m", profile.modelPath.u8string()};
    if (profile.HasAligner()) {
        if (IsRegularFile(profile.alignerPath)) {
            arguments.insert(arguments.end(), {"-am", profile.alignerPath.u8string()});
        } else {
            command.warnings.push_back("The aligner is missing, so the server starts without word timestamps: " +
                                       (profile.alignerPath.empty() ? profile.aligner : profile.alignerPath.u8string()));
        }
    }
    if (!profile.language.empty()) {
        arguments.insert(arguments.end(), {"-l", profile.language});
    }
    const ProfileStartup startup = ReadProfileStartup(profile);
    for (const StartupArgument& argument : startup.arguments) {
        arguments.push_back(argument.text);
    }
    command.host = profile.host.empty() ? kDefaultServerHost : profile.host;
    command.port = profile.port != 0 ? profile.port : kDefaultServerPort;
    arguments.insert(arguments.end(), {"--host", command.host, "--port", std::to_string(command.port)});

    // start-crispasr.ps1:182-187 and 213-215: the profile's env comes after the launcher's own
    // variables, and an empty value removes a variable, as PowerShell's Set-Item does.
    std::vector<platform::EnvironmentChange>& environment = command.process.environment;
    environment.push_back({"CRISPASR_MODELS_DIR", paths.models.u8string(), false});
    environment.push_back({"CRISPASR_CACHE_DIR", (paths.root / "cache").u8string(), false});
    environment.push_back({"CRISPASR_SCRATCH_DIR", (paths.root / "scratch").u8string(), false});
    environment.push_back({"CRISPASR_API_KEYS", profile.apiKeys.empty() ? kDefaultApiKey : profile.apiKeys, false});
    environment.push_back({"CUDA_VISIBLE_DEVICES", profile.device, profile.device.empty()});
    for (const auto& variable : profile.env) {
        environment.push_back({variable.first, variable.second, variable.second.empty()});
    }
    for (const std::string& name : platform::TempVariableNames()) {
        for (const auto& variable : profile.env) {
            if (SameVariable(variable.first, name)) {
                command.warnings.push_back("The profile sets " + variable.first +
                                           "; the GUI points it at its own folder instead.");
            }
        }
        environment.push_back({name, options.serverTemp.u8string(), false});
    }

    // The key for the GUI's requests: CRISPASR_API_KEYS as the server will see it, else --api-keys.
    std::string keys;
    for (const platform::EnvironmentChange& change : environment) {
        if (SameVariable(change.name, "CRISPASR_API_KEYS")) {
            keys = change.remove ? std::string() : change.value;
        }
    }
    command.apiKey = FirstKey(keys);
    if (command.apiKey.empty()) {
        const CliParseResult parsed = ParseCliArguments(arguments);
        if (const CliArgument* argument = FindLastArgument(parsed, "server_api_keys")) {
            command.apiKey = FirstKey(argument->value);
        }
    }

    command.process.executable = profile.executablePath;
    command.process.workingDirectory = paths.root;
    return command;
}

}  // namespace crisp::core
