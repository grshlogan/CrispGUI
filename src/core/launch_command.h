#pragma once

#include "core/cli_flags.h"
#include "platform/process.h"
#include "util/json.h"

#include <cstddef>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

// How the owner's launcher turns a profile into a server command line and environment
// (reference/crispasr/start-crispasr.ps1:162-221; docs/API-CRISPASR.md, section 3.2).
namespace crisp::core {

struct CrispAsrPaths;
struct Profile;

// The launcher's defaults (start-crispasr.ps1:82, 219-220), which the GUI shares.
constexpr const char* kDefaultServerHost = "127.0.0.1";
constexpr int kDefaultServerPort = 5114;
constexpr const char* kDefaultApiKey = "default";

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

struct LaunchOptions {
    std::filesystem::path serverTemp;  // TMP and TEMP of the server: <app data>/tmp/server
};

// How the GUI starts the server for a profile: the launcher's command line and environment, plus
// TMP and TEMP pointed at the app data folder (AGENTS.md, hard rule 4), with the CrispASR folder
// as the working directory.
struct LaunchCommand {
    platform::ProcessSpec process;
    std::string host;    // as passed with --host
    int port = 0;        // as passed with --port
    std::string apiKey;  // the first key the server accepts, for the GUI's own requests; empty when it needs none
    std::vector<std::string> warnings;  // what the launcher would also start without, such as a missing aligner
    std::string error;                  // why the server cannot start; |process| is incomplete then

    bool Ok() const { return error.empty(); }
    // The command line on one line, for display and the log.
    std::string CommandLineText() const;
};

// Builds the command for |profile|. Like the launcher, it refuses a missing server executable or
// model file and starts without a missing aligner (OQ-8, recommendation (a)); it checks the files
// again, because they may have changed since the scan.
LaunchCommand BuildLaunchCommand(const Profile& profile, const CrispAsrPaths& paths, const LaunchOptions& options);

}  // namespace crisp::core
