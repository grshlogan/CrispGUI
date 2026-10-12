#include "core/launch_command.h"

#include "core/profile_store.h"

namespace crisp::core {
namespace {

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

}  // namespace crisp::core
