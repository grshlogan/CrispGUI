#include "check.h"

#include "platform/command_line.h"

#include <string>
#include <vector>

using crisp::platform::ApplyEnvironmentChanges;
using crisp::platform::BuildWindowsCommandLine;
using crisp::platform::EnvironmentChange;
using crisp::platform::QuoteWindowsArgument;

namespace {

// Splits a command line the way the Microsoft C runtime does (2n backslashes before a quote
// give n and toggle quoting; 2n + 1 give n and a literal quote; other backslashes are literal),
// skipping the program name, which is in plain quotes.
std::vector<std::string> ParseWindowsCommandLine(const std::string& line) {
    std::vector<std::string> arguments;
    std::size_t i = line.find('"', 1) + 1;  // after the quoted program name
    while (i < line.size()) {
        while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) {
            ++i;
        }
        if (i >= line.size()) {
            break;
        }
        std::string argument;
        bool quoted = false;
        while (i < line.size() && (quoted || (line[i] != ' ' && line[i] != '\t'))) {
            std::size_t backslashes = 0;
            while (i < line.size() && line[i] == '\\') {
                ++backslashes;
                ++i;
            }
            if (i < line.size() && line[i] == '"') {
                argument.append(backslashes / 2, '\\');
                if (backslashes % 2 == 1) {
                    argument.push_back('"');
                } else {
                    quoted = !quoted;
                }
                ++i;
            } else {
                argument.append(backslashes, '\\');
                if (i < line.size() && (quoted || (line[i] != ' ' && line[i] != '\t'))) {
                    argument.push_back(line[i]);
                    ++i;
                }
            }
        }
        arguments.push_back(argument);
    }
    return arguments;
}

}  // namespace

int main() {
    // Quoting after the C runtime's rules.
    CHECK(QuoteWindowsArgument("abc") == "abc");
    CHECK(QuoteWindowsArgument("") == "\"\"");
    CHECK(QuoteWindowsArgument("a b") == "\"a b\"");
    CHECK(QuoteWindowsArgument("a\tb") == "\"a\tb\"");
    CHECK(QuoteWindowsArgument("a\"b") == "\"a\\\"b\"");
    CHECK(QuoteWindowsArgument("a\\b") == "a\\b");
    CHECK(QuoteWindowsArgument("a\\\\b c") == "\"a\\\\b c\"");
    CHECK(QuoteWindowsArgument("a b\\") == "\"a b\\\\\"");
    CHECK(QuoteWindowsArgument("a\\\"b") == "\"a\\\\\\\"b\"");
    CHECK(QuoteWindowsArgument("C:\\Program Files\\x\\") == "\"C:\\Program Files\\x\\\\\"");
    // A model path of the reference profiles needs no quotes.
    const std::string model = "E:\\AI\\ASR\\CrispASR\\models\\Japanese\\[CKHO]cohere-asr-ja-GGUF\\cohere-asr-ja-q8_0.gguf";
    CHECK(QuoteWindowsArgument(model) == model);

    // Every argument survives the round trip through one command line.
    const std::vector<std::string> arguments = {"--server", "", " ", "a b", "\"", "\\", "\\\\", "a\\", "a b\\\\",
                                                "a\\\"", "\\\"\\", "tab\there", "x \"y\" z", "end\\", model, "日本語"};
    const std::string line = BuildWindowsCommandLine("C:\\Program Files\\CrispASR\\crispasr.exe", arguments);
    CHECK(line.rfind("\"C:\\Program Files\\CrispASR\\crispasr.exe\" --server \"\" \" \" \"a b\"", 0) == 0);
    CHECK(ParseWindowsCommandLine(line) == arguments);

    // Environment changes with the Windows rules: names ignore case, sorted without regard to
    // case, the per-drive entries kept in front.
    const std::vector<std::string> base = {"PATH=C:\\bin", "Path2=x", "TMP=old", "=C:=C:\\work", "home=C:\\h"};
    const std::vector<EnvironmentChange> changes = {{"tmp", "new", false}, {"HOME", "", true}, {"NEW", "1", false}};
    CHECK(ApplyEnvironmentChanges(base, changes, true) ==
          std::vector<std::string>({"=C:=C:\\work", "NEW=1", "PATH=C:\\bin", "Path2=x", "tmp=new"}));

    // The POSIX rules: names are case-sensitive and sort byte by byte.
    CHECK(ApplyEnvironmentChanges(base, {{"tmp", "new", false}}, false) ==
          std::vector<std::string>({"=C:=C:\\work", "PATH=C:\\bin", "Path2=x", "TMP=old", "home=C:\\h", "tmp=new"}));

    // Changes apply in order: a later one wins, and a removed variable can be set again.
    CHECK(ApplyEnvironmentChanges({}, {{"A", "1", false}, {"A", "2", false}}, false) ==
          std::vector<std::string>({"A=2"}));
    CHECK(ApplyEnvironmentChanges({"A=0"}, {{"A", "", true}, {"A", "3", false}}, true) ==
          std::vector<std::string>({"A=3"}));
    CHECK(ApplyEnvironmentChanges({"A=0", "B=1"}, {{"A", "", true}}, true) == std::vector<std::string>({"B=1"}));
    // A value may contain '='.
    CHECK(ApplyEnvironmentChanges({"X=a=b"}, {{"Y", "c=d", false}}, false) ==
          std::vector<std::string>({"X=a=b", "Y=c=d"}));
    return crisp::test::Result();
}
