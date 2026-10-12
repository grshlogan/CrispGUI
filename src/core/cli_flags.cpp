#include "core/cli_flags.h"

#include <algorithm>
#include <cfloat>
#include <charconv>
#include <cmath>
#include <system_error>
#include <utility>

namespace crisp::core {
namespace {

constexpr CliFlag kCliFlags[] = {
#include "core/cli_flags_table.inc"
};

// The values the CLI accepts for a few string flags; any other value stops it with an error
// (examples/cli/cli.cpp of CrispASR 0.8.42, and src/core/asr_sensitivity.h for the presets).
struct ValueChoices {
    std::string_view spelling;  // canonical spelling
    std::string_view choices;   // separated by spaces
};

constexpr ValueChoices kValueChoices[] = {
    {"--lcs-dedup", "auto on off"},
    {"--sensitivity", "balanced default conservative strict aggressive loose"},
    {"--stream-final-mode", "redecode prefix"},
    {"--stream-punc", "off final partial"},
};

struct SpellingEntry {
    std::string_view spelling;
    const CliFlag* flag;
};

// Every spelling of every flag, sorted for lookup.
const std::vector<SpellingEntry>& SpellingIndex() {
    static const std::vector<SpellingEntry> index = [] {
        std::vector<SpellingEntry> entries;
        for (const CliFlag& flag : kCliFlags) {
            std::string_view rest = flag.spellings;
            while (!rest.empty()) {
                const std::size_t space = rest.find(' ');
                entries.push_back({rest.substr(0, space), &flag});
                rest = space == std::string_view::npos ? std::string_view() : rest.substr(space + 1);
            }
        }
        std::sort(entries.begin(), entries.end(),
                  [](const SpellingEntry& a, const SpellingEntry& b) { return a.spelling < b.spelling; });
        return entries;
    }();
    return index;
}

bool IsSpace(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\v' || c == '\f' || c == '\r';
}

bool IsDigit(char c) {
    return c >= '0' && c <= '9';
}

bool ContainsWord(std::string_view words, std::string_view word) {
    while (!words.empty()) {
        const std::size_t space = words.find(' ');
        if (words.substr(0, space) == word) {
            return true;
        }
        words = space == std::string_view::npos ? std::string_view() : words.substr(space + 1);
    }
    return false;
}

std::string JoinChoices(std::string_view words) {
    std::string joined;
    for (char c : words) {
        if (c == ' ') {
            joined += ", ";
        } else {
            joined.push_back(c);
        }
    }
    return joined;
}

std::string Quoted(std::string_view text) {
    return "\"" + std::string(text) + "\"";
}

enum class NumberCheck { kOk, kNotANumber, kTrailing, kOutOfRange, kNegativeWraps };

// Mirrors std::stoi, std::stoull, and std::stof: leading white space and a sign are accepted,
// and only the leading number is read.
NumberCheck CheckNumber(std::string_view text, CliValueKind kind, long long* integer) {
    std::size_t i = 0;
    while (i < text.size() && IsSpace(text[i])) {
        ++i;
    }
    bool negative = false;
    if (i < text.size() && (text[i] == '+' || text[i] == '-')) {
        negative = text[i] == '-';
        ++i;
    }
    const char* begin = text.data() + i;
    const char* end = text.data() + text.size();
    if (begin == end || *begin == '+' || *begin == '-') {
        return NumberCheck::kNotANumber;
    }
    if (kind == CliValueKind::kFloat) {
        double value = 0.0;
        const std::from_chars_result parsed = std::from_chars(begin, end, value);
        if (parsed.ec == std::errc::invalid_argument) {
            return NumberCheck::kNotANumber;
        }
        if (parsed.ec == std::errc::result_out_of_range || (std::isfinite(value) && std::fabs(value) > FLT_MAX)) {
            return NumberCheck::kOutOfRange;
        }
        return parsed.ptr == end ? NumberCheck::kOk : NumberCheck::kTrailing;
    }
    if (!IsDigit(*begin)) {
        return NumberCheck::kNotANumber;
    }
    unsigned long long magnitude = 0;
    const std::from_chars_result parsed = std::from_chars(begin, end, magnitude);
    if (parsed.ec == std::errc::result_out_of_range) {
        return NumberCheck::kOutOfRange;
    }
    if (kind == CliValueKind::kInt) {
        if (magnitude > (negative ? 2147483648ULL : 2147483647ULL)) {
            return NumberCheck::kOutOfRange;
        }
        if (integer != nullptr) {
            *integer = negative ? -static_cast<long long>(magnitude) : static_cast<long long>(magnitude);
        }
    } else if (negative && magnitude != 0) {
        return NumberCheck::kNegativeWraps;
    }
    return parsed.ptr == end ? NumberCheck::kOk : NumberCheck::kTrailing;
}

void CheckValue(const CliFlag& flag, const std::string& spelling, const std::string& value,
                std::vector<std::string>& problems) {
    const std::string_view canonical = CanonicalSpelling(flag);
    if (flag.kind == CliValueKind::kInt || flag.kind == CliValueKind::kUnsigned || flag.kind == CliValueKind::kFloat) {
        long long integer = 0;
        switch (CheckNumber(value, flag.kind, &integer)) {
        case NumberCheck::kOk:
            if (canonical == "--lcs-min-length" && integer < 1) {
                problems.push_back(spelling + " must be at least 1, not " + value + ": the CLI stops with an error.");
            }
            return;
        case NumberCheck::kNotANumber:
            problems.push_back("The value " + Quoted(value) + " of " + spelling +
                               " is not a number: the CLI stops with an error.");
            return;
        case NumberCheck::kTrailing:
            problems.push_back("The CLI reads only the leading number of " + Quoted(value) + " for " + spelling + ".");
            return;
        case NumberCheck::kOutOfRange:
            problems.push_back("The value " + Quoted(value) + " of " + spelling +
                               " is out of range: the CLI stops with an error.");
            return;
        case NumberCheck::kNegativeWraps:
            problems.push_back("The value " + Quoted(value) + " of " + spelling +
                               " is negative and wraps around to a very large number.");
            return;
        }
    }
    for (const ValueChoices& choices : kValueChoices) {
        if (choices.spelling == canonical && !ContainsWord(choices.choices, value)) {
            problems.push_back("The value " + Quoted(value) + " of " + spelling + " is not one of " +
                               JoinChoices(choices.choices) + ": the CLI stops with an error.");
        }
    }
}

}  // namespace

std::size_t CliFlagCount() {
    return sizeof(kCliFlags) / sizeof(kCliFlags[0]);
}

const CliFlag& CliFlagAt(std::size_t index) {
    return kCliFlags[index];
}

const CliFlag* FindCliFlag(std::string_view spelling) {
    const std::vector<SpellingEntry>& index = SpellingIndex();
    const auto found = std::lower_bound(index.begin(), index.end(), spelling,
                                        [](const SpellingEntry& entry, std::string_view value) {
                                            return entry.spelling < value;
                                        });
    return found != index.end() && found->spelling == spelling ? found->flag : nullptr;
}

std::string_view CanonicalSpelling(const CliFlag& flag) {
    std::string_view longest;
    std::string_view rest = flag.spellings;
    while (!rest.empty()) {
        const std::size_t space = rest.find(' ');
        const std::string_view spelling = rest.substr(0, space);
        if (spelling.size() > longest.size()) {
            longest = spelling;
        }
        rest = space == std::string_view::npos ? std::string_view() : rest.substr(space + 1);
    }
    return longest;
}

bool TakesValue(const CliFlag& flag) {
    return flag.kind != CliValueKind::kSwitch && flag.kind != CliValueKind::kExit;
}

CliParseResult ParseCliArguments(const std::vector<std::string>& arguments) {
    CliParseResult result;
    const std::string version(kCliFlagsVersion);
    for (std::size_t i = 0; i < arguments.size(); ++i) {
        const std::string& token = arguments[i];
        if (token.empty() || token[0] != '-' || token == "-") {
            result.problems.push_back("Unexpected argument " + Quoted(token) +
                                      ": the CLI treats it as an input file.");
            continue;
        }
        const CliFlag* flag = FindCliFlag(token);
        if (flag == nullptr) {
            result.problems.push_back("Unknown flag " + token + ": CrispASR " + version +
                                      " stops with \"unknown argument\"; a newer engine may accept it.");
            if (i + 1 < arguments.size() && !arguments[i + 1].empty() && arguments[i + 1][0] != '-') {
                ++i;  // most likely its value
            }
            continue;
        }
        if (flag->kind == CliValueKind::kExit) {
            result.problems.push_back(token + " makes CrispASR print information and exit instead of running.");
            continue;
        }
        CliArgument argument;
        argument.flag = flag;
        argument.index = i;
        argument.spelling = token;
        if (TakesValue(*flag)) {
            if (i + 1 >= arguments.size()) {
                result.problems.push_back(token + " needs a value: the CLI exits without starting.");
                continue;
            }
            argument.value = arguments[++i];
            CheckValue(*flag, token, argument.value, result.problems);
        } else {
            argument.value = std::string(flag->implied);
        }
        result.arguments.push_back(std::move(argument));
    }
    return result;
}

const CliArgument* FindLastArgument(const CliParseResult& result, std::string_view param) {
    for (auto it = result.arguments.rbegin(); it != result.arguments.rend(); ++it) {
        if (it->flag->param == param) {
            return &*it;
        }
    }
    return nullptr;
}

}  // namespace crisp::core
