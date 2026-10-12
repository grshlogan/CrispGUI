#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

// The command-line flags of the CrispASR engine, for reading a profile's args[] (and, from
// Phase 3, for checking the server command line). The table comes from the CrispASR 0.8.42 CLI
// source through tools/extract-cli-flags.py (docs/OPEN-QUESTIONS.md, OQ-4 item 2).
namespace crisp::core {

enum class CliValueKind : std::uint8_t {
    kSwitch,     // takes no value and assigns |implied| to |param|
    kInt,        // the next argument, read with std::stoi
    kUnsigned,   // the next argument, read with std::stoull
    kFloat,      // the next argument, read with std::stof
    kString,     // the next argument as written
    kLowercase,  // the next argument, lowercased
    kExit,       // prints information and ends the program instead of running it
};

struct CliFlag {
    std::string_view spellings;  // separated by spaces, such as "-vt --vad-threshold"
    std::string_view param;      // the whisper_params field the flag sets first
    CliValueKind kind;
    std::string_view implied;  // the value a switch assigns, such as "true" or "false"
};

// The CrispASR version the table was extracted from.
constexpr std::string_view kCliFlagsVersion = "0.8.42";

std::size_t CliFlagCount();
const CliFlag& CliFlagAt(std::size_t index);
// The flag with the exact spelling |spelling|, such as "-vt"; nullptr when there is none.
const CliFlag* FindCliFlag(std::string_view spelling);
// The flag's longest spelling, such as "--vad-threshold".
std::string_view CanonicalSpelling(const CliFlag& flag);
bool TakesValue(const CliFlag& flag);

// One recognized flag in an argument list.
struct CliArgument {
    const CliFlag* flag = nullptr;
    std::size_t index = 0;  // position of the flag in the list
    std::string spelling;   // as written, such as "-vt"
    std::string value;      // the value as written; for a switch, the value it assigns
};

struct CliParseResult {
    std::vector<CliArgument> arguments;  // recognized flags, in order
    // What the CLI would reject, misread, or ignore, one sentence each.
    std::vector<std::string> problems;
};

// Reads |arguments| as the CrispASR 0.8.42 CLI does: a flag that takes a value takes the next
// argument, whatever it looks like, and a later flag overrides an earlier one that sets the same
// parameter. An unknown flag is reported and is assumed to take the next argument as its value
// when that argument does not start with '-'.
CliParseResult ParseCliArguments(const std::vector<std::string>& arguments);

// The last argument that sets |param|, which is the one the CLI keeps; nullptr when none does.
const CliArgument* FindLastArgument(const CliParseResult& result, std::string_view param);

}  // namespace crisp::core
