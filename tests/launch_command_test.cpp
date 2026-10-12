#include "check.h"
#include "test_support.h"

#include "core/launch_command.h"
#include "core/profile_store.h"
#include "platform/paths.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

namespace fs = std::filesystem;
using crisp::core::ApplyCondition;
using crisp::core::ConditionHolds;
using crisp::core::FindLastArgument;
using crisp::core::BuildLaunchCommand;
using crisp::core::CrispAsrPaths;
using crisp::core::FormatLauncherNumber;
using crisp::core::LaunchCommand;
using crisp::core::LaunchOptions;
using crisp::core::LoadProfile;
using crisp::core::ParseProfile;
using crisp::core::Profile;
using crisp::core::ProfileStartup;
using crisp::core::ReadProfileStartup;
using crisp::util::JsonValue;
using crisp::util::ParseJson;

namespace {

std::vector<std::string> Texts(const ProfileStartup& startup) {
    std::vector<std::string> texts;
    for (const auto& argument : startup.arguments) {
        texts.push_back(argument.text);
    }
    return texts;
}

ProfileStartup StartupOf(const std::string& tuning) {
    Profile profile;
    ParseProfile("{\"build\": \"cuda13\", \"backend\": \"cohere\", \"model\": \"m.gguf\"" + tuning + "}", profile);
    CHECK(profile.Usable());
    return ReadProfileStartup(profile);
}

JsonValue Value(const std::string& json) {
    return ParseJson(json).value;
}

bool Contains(const std::vector<std::string>& texts, const std::string& text) {
    return std::find(texts.begin(), texts.end(), text) != texts.end();
}

// The value of an environment change, or "(removed)", or "(absent)".
std::string ChangeOf(const LaunchCommand& command, const std::string& name) {
    std::string value = "(absent)";
    for (const auto& change : command.process.environment) {
        if (change.name == name) {
            value = change.remove ? "(removed)" : change.value;
        }
    }
    return value;
}

// A CrispASR folder with a server executable, a model, and an aligner, all empty files.
struct FakeDeployment {
    fs::path root;
    CrispAsrPaths paths;
    fs::path model;
    fs::path aligner;

    explicit FakeDeployment(const std::string& name) {
        root = crisp::test::FreshScratchDirectory(name);
        paths = crisp::core::MakeCrispAsrPaths(root);
        crisp::test::WriteFile(root / "cuda13" / crisp::core::kServerExecutableName, "");
        model = paths.models / "m" / "model.gguf";
        aligner = paths.models / "m" / "aligner.gguf";
        crisp::test::WriteFile(model, "");
        crisp::test::WriteFile(aligner, "");
    }

    // A profile file in profile/Test with |keys| after the required ones.
    crisp::core::Profile Profile(const std::string& keys) const {
        const fs::path file = paths.profiles / "Test" / "p.conf";
        crisp::test::WriteFile(file, "{\"build\": \"cuda13\", \"backend\": \"cohere\", \"model\": \"m\\\\model.gguf\"" +
                                         keys + "}");
        return LoadProfile(file, paths);
    }
};

}  // namespace

int main() {
    const fs::path data = crisp::test::TestDataDirectory() / "profiles-japanese";
    std::error_code error;
    if (!fs::is_directory(data, error)) {
        std::fprintf(stderr, "launch_command_test: test data missing at %s\n", data.u8string().c_str());
        return 1;
    }

    // The four reference profiles, as start-crispasr.ps1:203-217 turns them into arguments.
    const struct {
        const char* file;
        std::vector<std::string> arguments;
    } expected[] = {
        {"cohere-asr-ja-q8.conf",
         {"--vad", "--force-aligner", "--chunk-seconds", "30", "-bs", "1", "-n", "512", "--vad-threshold", "0.3"}},
        {"kotoba-whisper-v2.2-q8.conf",
         {"--vad", "--force-aligner", "--chunk-seconds", "15", "-bs", "1", "--vad-max-speech-duration-s", "15"}},
        {"parakeet-ctc-1.1b-ja-q8.conf",
         {"--vad", "--force-aligner", "--chunk-seconds", "15", "--vad-max-speech-duration-s", "15"}},
        {"qwen3-asr-1.7b-ja-anime-q8.conf", {"--vad", "--force-aligner", "--chunk-seconds", "30", "-bs", "1", "-n", "512"}},
    };
    for (const auto& entry : expected) {
        const Profile profile = LoadProfile(data / entry.file, crisp::core::CrispAsrPaths());
        CHECK(profile.Usable());
        const ProfileStartup startup = ReadProfileStartup(profile);
        CHECK(Texts(startup) == entry.arguments);
        CHECK(startup.parsed.problems.empty());
    }

    // Where each value comes from, and the line shown on the dashboard.
    const ProfileStartup cohere = ReadProfileStartup(LoadProfile(data / "cohere-asr-ja-q8.conf", {}));
    const crisp::core::CliArgument* threshold = FindLastArgument(cohere.parsed, "vad_threshold");
    CHECK(threshold != nullptr && threshold->value == "0.3" && cohere.SourceOf(*threshold) == "args");
    const crisp::core::CliArgument* chunk = FindLastArgument(cohere.parsed, "chunk_seconds");
    CHECK(chunk != nullptr && chunk->value == "30" && cohere.SourceOf(*chunk) == "chunk_seconds");
    CHECK(FindLastArgument(cohere.parsed, "temperature") == nullptr);  // 0 is not passed on
    CHECK(cohere.Text() == "--vad --force-aligner --chunk-seconds 30 -bs 1 -n 512 --vad-threshold 0.3");

    // The launcher's conditions, key by key.
    CHECK(Texts(StartupOf(", \"temperature\": 0.20")) == std::vector<std::string>({"-tp", "0.2"}));
    CHECK(Texts(StartupOf(", \"temperature\": -1")).empty());
    CHECK(Texts(StartupOf(", \"temperature\": 0")).empty());
    CHECK(Texts(StartupOf(", \"frequency_penalty\": 0.5")) == std::vector<std::string>({"--frequency-penalty", "0.5"}));
    CHECK(Texts(StartupOf(", \"seed\": 42")) == std::vector<std::string>({"--seed", "42"}));
    CHECK(Texts(StartupOf(", \"seed\": 0")).empty());
    CHECK(Texts(StartupOf(", \"chunk_seconds\": 0")) == std::vector<std::string>({"--chunk-seconds", "0"}));
    CHECK(Texts(StartupOf(", \"chunk_seconds\": null")).empty());
    CHECK(Texts(StartupOf(", \"beam_size\": -1")) == std::vector<std::string>({"-bs", "-1"}));
    CHECK(Texts(StartupOf(", \"beam_size\": 0")).empty());
    CHECK(Texts(StartupOf(", \"max_new_tokens\": 30.0")) == std::vector<std::string>({"-n", "30"}));
    CHECK(Texts(StartupOf(", \"vad\": false, \"force_aligner\": false")).empty());
    CHECK(Texts(StartupOf(", \"no_punctuation\": true, \"split_on_punct\": true")) ==
          std::vector<std::string>({"--no-punctuation", "-sp"}));

    // The launcher's order, whatever the order in the file; args[] comes last, verbatim.
    CHECK(Texts(StartupOf(", \"args\": [\"-t\", \"8\"], \"seed\": 7, \"vad\": true")) ==
          std::vector<std::string>({"--vad", "--seed", "7", "-t", "8"}));

    // A flag in args[] overrides the tuning key, because the CLI keeps the last value.
    const ProfileStartup overridden = StartupOf(", \"beam_size\": 1, \"args\": [\"--beam-size\", \"5\"]");
    const crisp::core::CliArgument* beam = FindLastArgument(overridden.parsed, "beam_size");
    CHECK(beam != nullptr && beam->value == "5" && overridden.SourceOf(*beam) == "args");

    // Conditions and number formatting on their own.
    CHECK(ConditionHolds(ApplyCondition::kTruthy, Value("true")));
    CHECK(!ConditionHolds(ApplyCondition::kTruthy, Value("false")));
    CHECK(ConditionHolds(ApplyCondition::kTruthy, Value("-2")));
    CHECK(!ConditionHolds(ApplyCondition::kTruthy, Value("0.0")));
    CHECK(ConditionHolds(ApplyCondition::kPresent, Value("0")));
    CHECK(!ConditionHolds(ApplyCondition::kPresent, Value("null")));
    CHECK(ConditionHolds(ApplyCondition::kGreaterThanZero, Value("0.01")));
    CHECK(!ConditionHolds(ApplyCondition::kGreaterThanZero, Value("0")));
    CHECK(FormatLauncherNumber(Value("512")) == "512");
    CHECK(FormatLauncherNumber(Value("0.30")) == "0.3");
    CHECK(FormatLauncherNumber(Value("1e2")) == "100");
    CHECK(FormatLauncherNumber(Value("-1")) == "-1");

    // The whole command for the reference cohere profile, as start-crispasr.ps1:162-221 builds
    // it, plus TMP and TEMP.
    {
        FakeDeployment deployment("launch_command_test-cohere");
        const fs::path modelFile =
            deployment.paths.models / "Japanese" / "[CKHO]cohere-asr-ja-GGUF" / "cohere-asr-ja-q8_0.gguf";
        const fs::path alignerFile = deployment.paths.models / "Japanese" /
                                     "[cstr]wav2vec2-large-xlsr-53-japanese-GGUF" /
                                     "wav2vec2-large-xlsr-53-japanese-q8_0.gguf";
        crisp::test::WriteFile(modelFile, "");
        crisp::test::WriteFile(alignerFile, "");
        const Profile profile = LoadProfile(data / "cohere-asr-ja-q8.conf", deployment.paths);
        const fs::path temp = deployment.root / "appdata" / "tmp" / "server";
        const LaunchCommand command = BuildLaunchCommand(profile, deployment.paths, LaunchOptions{temp});
        CHECK(command.Ok());
        CHECK(command.warnings.empty());
        CHECK(command.process.executable == deployment.root / "cuda13" / crisp::core::kServerExecutableName);
        CHECK(command.process.workingDirectory == deployment.root);
        CHECK(command.process.arguments ==
              std::vector<std::string>({"--server", "--backend", "cohere", "-m", modelFile.u8string(), "-am",
                                        alignerFile.u8string(), "-l", "ja", "--vad", "--force-aligner",
                                        "--chunk-seconds", "30", "-bs", "1", "-n", "512", "--vad-threshold", "0.3",
                                        "--host", "127.0.0.1", "--port", "5114"}));
        std::vector<std::string> names;
        for (const auto& change : command.process.environment) {
            names.push_back(change.name);
        }
        std::vector<std::string> expectedNames = {"CRISPASR_MODELS_DIR", "CRISPASR_CACHE_DIR", "CRISPASR_SCRATCH_DIR",
                                                  "CRISPASR_API_KEYS", "CUDA_VISIBLE_DEVICES"};
        for (const std::string& name : crisp::platform::TempVariableNames()) {
            expectedNames.push_back(name);
        }
        CHECK(names == expectedNames);
        CHECK(ChangeOf(command, "CRISPASR_MODELS_DIR") == deployment.paths.models.u8string());
        CHECK(ChangeOf(command, "CRISPASR_CACHE_DIR") == (deployment.root / "cache").u8string());
        CHECK(ChangeOf(command, "CRISPASR_SCRATCH_DIR") == (deployment.root / "scratch").u8string());
        CHECK(ChangeOf(command, "CRISPASR_API_KEYS") == "default");  // "api_keys": ""
        CHECK(ChangeOf(command, "CUDA_VISIBLE_DEVICES") == "1");
        CHECK(ChangeOf(command, "TEMP") == temp.u8string());
        CHECK(command.apiKey == "default");
        CHECK(command.host == "127.0.0.1" && command.port == 5114);
        const std::string text = command.CommandLineText();
        CHECK(text.find("--chunk-seconds 30") != std::string::npos);
        CHECK(text.find("--port 5114") != std::string::npos);
    }

    {
        FakeDeployment deployment("launch_command_test-cases");
        const LaunchOptions options{deployment.root / "tmp"};

        // Defaults for a minimal profile: host, port, key, no device, no aligner or language.
        const LaunchCommand minimal = BuildLaunchCommand(deployment.Profile(""), deployment.paths, options);
        CHECK(minimal.Ok());
        CHECK(minimal.process.arguments ==
              std::vector<std::string>({"--server", "--backend", "cohere", "-m", deployment.model.u8string(), "--host",
                                        "127.0.0.1", "--port", "5114"}));
        CHECK(ChangeOf(minimal, "CUDA_VISIBLE_DEVICES") == "(removed)");
        CHECK(minimal.apiKey == "default");

        // A missing aligner is left out with a warning, as the launcher does (OQ-8).
        const LaunchCommand noAligner =
            BuildLaunchCommand(deployment.Profile(", \"aligner\": \"m\\\\missing.gguf\""), deployment.paths, options);
        CHECK(noAligner.Ok());
        CHECK(!Contains(noAligner.process.arguments, "-am"));
        CHECK(noAligner.warnings.size() == 1);
        const LaunchCommand withAligner =
            BuildLaunchCommand(deployment.Profile(", \"aligner\": \"m/aligner.gguf\""), deployment.paths, options);
        CHECK(Contains(withAligner.process.arguments, "-am"));
        CHECK(Contains(withAligner.process.arguments, deployment.aligner.u8string()));

        // Host and port pass through; the launcher's --host and --port come last, so they win
        // over the same flags in args[].
        const LaunchCommand endpoint = BuildLaunchCommand(
            deployment.Profile(", \"host\": \"0.0.0.0\", \"port\": 5200, \"args\": [\"--port\", \"9\"]"),
            deployment.paths, options);
        const std::vector<std::string>& arguments = endpoint.process.arguments;
        CHECK(std::vector<std::string>(arguments.end() - 4, arguments.end()) ==
              std::vector<std::string>({"--host", "0.0.0.0", "--port", "5200"}));
        CHECK(endpoint.host == "0.0.0.0" && endpoint.port == 5200);

        // API keys: the first of the profile's list, trimmed.
        CHECK(BuildLaunchCommand(deployment.Profile(", \"api_keys\": \" k1 , k2\""), deployment.paths, options).apiKey ==
              "k1");
        // The profile's env comes after the launcher's variables; an empty value removes one.
        const LaunchCommand env = BuildLaunchCommand(
            deployment.Profile(", \"device\": 0, \"env\": {\"CRISPASR_API_KEYS\": \"envkey\", \"OMP_NUM_THREADS\": 8, "
                               "\"EMPTY\": \"\"}"),
            deployment.paths, options);
        CHECK(env.apiKey == "envkey");
        CHECK(ChangeOf(env, "CUDA_VISIBLE_DEVICES") == "0");
        CHECK(ChangeOf(env, "OMP_NUM_THREADS") == "8");
        CHECK(ChangeOf(env, "EMPTY") == "(removed)");
        CHECK(ChangeOf(env, "CRISPASR_API_KEYS") == "envkey");
        // Without keys in the environment, --api-keys in args[] provides one; with none the GUI
        // sends no key.
        CHECK(BuildLaunchCommand(deployment.Profile(", \"env\": {\"CRISPASR_API_KEYS\": \"\"}, \"args\": "
                                                    "[\"--api-keys\", \"argkey,other\"]"),
                                 deployment.paths, options)
                  .apiKey == "argkey");
        CHECK(BuildLaunchCommand(deployment.Profile(", \"env\": {\"CRISPASR_API_KEYS\": \"\"}"), deployment.paths, options)
                  .apiKey.empty());
        // TMP and TEMP always point at the GUI's folder, after a profile's own values.
        const std::string tempName = crisp::platform::TempVariableNames().front();
        const LaunchCommand temp =
            BuildLaunchCommand(deployment.Profile(", \"env\": {\"" + tempName + "\": \"elsewhere\"}"), deployment.paths,
                               options);
        CHECK(ChangeOf(temp, tempName) == options.serverTemp.u8string());
        CHECK(temp.warnings.size() == 1);

        // What prevents a start.
        CHECK(!BuildLaunchCommand(deployment.Profile(", \"model\": \"m/missing.gguf\""), deployment.paths, options).Ok());
        CHECK(!BuildLaunchCommand(deployment.Profile(", \"build\": \"vulkan\""), deployment.paths, options).Ok());
        CHECK(!BuildLaunchCommand(deployment.Profile(", \"port\": \"5114\""), deployment.paths, options).Ok());
        CHECK(!BuildLaunchCommand(deployment.Profile(""), deployment.paths, LaunchOptions{}).Ok());
    }
    return crisp::test::Result();
}
