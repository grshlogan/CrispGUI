#include "check.h"
#include "test_support.h"

#include "core/launch_command.h"
#include "core/profile_store.h"

#include <cstdio>
#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

namespace fs = std::filesystem;
using crisp::core::ApplyCondition;
using crisp::core::ConditionHolds;
using crisp::core::FindLastArgument;
using crisp::core::FormatLauncherNumber;
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
    return crisp::test::Result();
}
