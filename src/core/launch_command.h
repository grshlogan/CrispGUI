#pragma once

#include "core/cli_flags.h"
#include "util/json.h"

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

// How the owner's launcher turns a profile into server arguments
// (reference/crispasr/start-crispasr.ps1:190-221; docs/API-CRISPASR.md, section 3.2). Phase 2
// covers the tuning keys and args[]; Phase 3 adds the rest of the command line.
namespace crisp::core {

struct Profile;

// When the launcher passes a tuning key on (docs/ARCHITECTURE.md, section 2.7).
enum class ApplyCondition {
    kTruthy,           // true, or a number other than 0
    kPresent,          // any value but null, 0 included
    kGreaterThanZero,  // a number above 0
};

enum class TuningValueKind { kBoolean, kInteger, kNumber };

// A tuning key of the profile schema and the flag the launcher turns it into.
struct TuningKey {
    std::string_view key;   // such as "beam_size"
    std::string_view flag;  // such as "-bs"
    TuningValueKind kind;
    ApplyCondition condition;
};

// The launcher's tuning keys, in the order it emits their flags.
std::size_t TuningKeyCount();
const TuningKey& TuningKeyAt(std::size_t index);

// True when the launcher passes a key with |value| on, under |condition|.
bool ConditionHolds(ApplyCondition condition, const util::JsonValue& value);

// A number as PowerShell writes it into an argument: integers as written, other numbers in
// their shortest form ("0.30" becomes "0.3").
std::string FormatLauncherNumber(const util::JsonValue& number);

// One argument of the part of the command line a profile controls, and its origin.
struct StartupArgument {
    std::string text;
    std::string source;  // the tuning key that produced it, such as "beam_size", or "args"
};

// The profile's startup arguments as the launcher emits them, read with the CLI rules.
struct ProfileStartup {
    std::vector<StartupArgument> arguments;  // tuning flags whose condition holds, then args[]
    CliParseResult parsed;                   // |arguments| read as the CLI reads them

    // The source of a recognized argument, such as "beam_size" or "args".
    const std::string& SourceOf(const CliArgument& argument) const { return arguments[argument.index].source; }
    // The arguments as one line, for display.
    std::string Text() const;
};

// The tuning flags whose condition holds, followed by args[] verbatim, read with the CLI rules
// so that a later flag overrides an earlier one. |profile| must be usable.
ProfileStartup ReadProfileStartup(const Profile& profile);

}  // namespace crisp::core
