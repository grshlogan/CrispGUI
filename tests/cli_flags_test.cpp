#include "check.h"

#include "core/cli_flags.h"

#include <set>
#include <string>
#include <vector>

using crisp::core::CanonicalSpelling;
using crisp::core::CliFlag;
using crisp::core::CliFlagAt;
using crisp::core::CliFlagCount;
using crisp::core::CliParseResult;
using crisp::core::CliValueKind;
using crisp::core::FindCliFlag;
using crisp::core::FindLastArgument;
using crisp::core::ParseCliArguments;
using crisp::core::TakesValue;

namespace {

bool HasProblemWith(const CliParseResult& result, const std::string& text) {
    for (const std::string& problem : result.problems) {
        if (problem.find(text) != std::string::npos) {
            return true;
        }
    }
    return false;
}

}  // namespace

int main() {
    // The table: every spelling is unique and starts with '-'; switches assign a value.
    CHECK(CliFlagCount() > 200);
    std::set<std::string> spellings;
    for (std::size_t i = 0; i < CliFlagCount(); ++i) {
        const CliFlag& flag = CliFlagAt(i);
        std::string rest(flag.spellings);
        std::size_t start = 0;
        while (start < rest.size()) {
            const std::size_t space = rest.find(' ', start);
            const std::string spelling = rest.substr(start, space - start);
            CHECK(!spelling.empty() && spelling[0] == '-');
            CHECK(spellings.insert(spelling).second);
            CHECK(FindCliFlag(spelling) == &flag);
            start = space == std::string::npos ? rest.size() : space + 1;
        }
        CHECK(flag.kind != CliValueKind::kSwitch || !flag.implied.empty());
        CHECK(flag.kind == CliValueKind::kExit || !flag.param.empty());
    }

    // The flags the owner's launcher and profiles use, as the 0.8.42 source defines them.
    const CliFlag* threshold = FindCliFlag("-vt");
    CHECK(threshold != nullptr && threshold == FindCliFlag("--vad-threshold"));
    CHECK(threshold->param == "vad_threshold" && threshold->kind == CliValueKind::kFloat);
    CHECK(CanonicalSpelling(*threshold) == "--vad-threshold");
    CHECK(FindCliFlag("-bs")->param == "beam_size" && FindCliFlag("-bs")->kind == CliValueKind::kInt);
    CHECK(FindCliFlag("-tp")->param == "temperature");
    CHECK(FindCliFlag("-n")->param == "max_new_tokens");
    CHECK(FindCliFlag("--seed")->kind == CliValueKind::kUnsigned);
    CHECK(FindCliFlag("--chunk-seconds") == FindCliFlag("-ck"));
    CHECK(FindCliFlag("--frequency-penalty")->kind == CliValueKind::kFloat);
    CHECK(FindCliFlag("--vad")->kind == CliValueKind::kSwitch && FindCliFlag("--vad")->implied == "true");
    CHECK(FindCliFlag("--force-aligner") == FindCliFlag("-falign"));
    CHECK(FindCliFlag("--no-punctuation")->param == "punctuation");
    CHECK(FindCliFlag("--no-punctuation")->implied == "false");
    CHECK(FindCliFlag("-sp")->param == "split_on_punct");
    CHECK(FindCliFlag("--vad-max-speech-duration-s") == FindCliFlag("-vmsd"));
    CHECK(FindCliFlag("-l")->kind == CliValueKind::kLowercase);
    CHECK(FindCliFlag("-am")->param == "aligner_model");
    CHECK(FindCliFlag("--api-keys")->param == "server_api_keys");
    CHECK(FindCliFlag("--server") != nullptr && FindCliFlag("--host") != nullptr && FindCliFlag("--port") != nullptr);
    CHECK(FindCliFlag("--help")->kind == CliValueKind::kExit && !TakesValue(*FindCliFlag("--help")));
    CHECK(FindCliFlag("--accept-license") != nullptr && TakesValue(*FindCliFlag("--accept-license")));
    CHECK(FindCliFlag("--vad-thresh") == nullptr);
    CHECK(FindCliFlag("-") == nullptr);

    // A profile's args[]: values follow their flags; switches take none.
    const CliParseResult basic = ParseCliArguments({"--vad-max-speech-duration-s", "15", "--vad", "-vt", "0.3"});
    CHECK(basic.problems.empty());
    CHECK(basic.arguments.size() == 3);
    CHECK(basic.arguments[0].value == "15" && basic.arguments[0].index == 0);
    CHECK(basic.arguments[1].value == "true" && basic.arguments[1].index == 2);
    CHECK(basic.arguments[2].spelling == "-vt" && basic.arguments[2].index == 3);
    CHECK(FindLastArgument(basic, "vad_threshold") != nullptr && FindLastArgument(basic, "vad_threshold")->value == "0.3");
    CHECK(FindLastArgument(basic, "beam_size") == nullptr);

    // The last flag for a parameter wins, whatever its spelling; -fa and -nfa share one.
    const CliParseResult overridden = ParseCliArguments({"-bs", "1", "--beam-size", "5", "-fa", "-nfa"});
    CHECK(FindLastArgument(overridden, "beam_size")->value == "5");
    CHECK(FindLastArgument(overridden, "flash_attn")->value == "false");

    // A value is the next argument even when it starts with '-'.
    const CliParseResult negative = ParseCliArguments({"-tp", "-1", "--vad"});
    CHECK(negative.problems.empty() && negative.arguments.size() == 2);
    CHECK(negative.arguments[0].value == "-1");

    // What the CLI would reject or misread.
    CHECK(HasProblemWith(ParseCliArguments({"--vad-threshold"}), "needs a value"));
    CHECK(HasProblemWith(ParseCliArguments({"--bogus"}), "Unknown flag --bogus"));
    const CliParseResult unknownWithValue = ParseCliArguments({"--bogus", "7", "--vad"});
    CHECK(unknownWithValue.problems.size() == 1 && unknownWithValue.arguments.size() == 1);
    CHECK(HasProblemWith(ParseCliArguments({"stray"}), "input file"));
    CHECK(HasProblemWith(ParseCliArguments({"-vt", "abc"}), "not a number"));
    CHECK(HasProblemWith(ParseCliArguments({"-t", "8x"}), "only the leading number"));
    CHECK(HasProblemWith(ParseCliArguments({"-t", "99999999999"}), "out of range"));
    CHECK(HasProblemWith(ParseCliArguments({"-tp", "1e39"}), "out of range"));
    CHECK(HasProblemWith(ParseCliArguments({"--seed", "-1"}), "wraps around"));
    CHECK(HasProblemWith(ParseCliArguments({"--lcs-dedup", "maybe"}), "auto, on, off"));
    CHECK(HasProblemWith(ParseCliArguments({"--lcs-min-length", "0"}), "at least 1"));
    CHECK(HasProblemWith(ParseCliArguments({"--sensitivity", "extreme"}), "not one of"));
    CHECK(HasProblemWith(ParseCliArguments({"--help"}), "exit instead of running"));
    CHECK(ParseCliArguments({"-t", " 8"}).problems.empty());  // std::stoi skips leading space
    CHECK(ParseCliArguments({"-tp", "+0.5"}).problems.empty());
    CHECK(ParseCliArguments({"--sensitivity", "strict", "--lcs-dedup", "on", "--lcs-min-length", "3"}).problems.empty());
    return crisp::test::Result();
}
