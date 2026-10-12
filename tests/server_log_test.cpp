#include "check.h"

#include "core/server_log.h"

#include <string>
#include <vector>

using crisp::core::CleanLogLine;
using crisp::core::LogBuffer;
using crisp::core::LogSplitter;

namespace {

const std::string kReplacement = "\xEF\xBF\xBD";

std::string Times(const std::string& text, int count) {
    std::string result;
    for (int i = 0; i < count; ++i) {
        result += text;
    }
    return result;
}

}  // namespace

int main() {
    // Cleaning one line.
    CHECK(CleanLogLine("plain text") == "plain text");
    CHECK(CleanLogLine("windows line\r") == "windows line");
    CHECK(CleanLogLine("10%\r20%\r30%") == "30%");
    CHECK(CleanLogLine("\x1B[32mgreen\x1B[0m done") == "green done");
    CHECK(CleanLogLine("\x1B[1;31;40mbold\x1B[K") == "bold");
    CHECK(CleanLogLine("\x1B]0;window title\x07" "after") == "after");
    CHECK(CleanLogLine("\x1B]2;title\x1B\\after") == "after");
    CHECK(CleanLogLine("a\tb") == "a b");
    CHECK(CleanLogLine("bell\x07 and nul") == "bell and nul");
    CHECK(CleanLogLine(std::string("zero\0byte", 9)) == "zerobyte");
    CHECK(CleanLogLine("del\x7F") == "del");

    // UTF-8: valid text stays, invalid bytes become U+FFFD one by one.
    CHECK(CleanLogLine("日本語の文字起こし") == "日本語の文字起こし");
    CHECK(CleanLogLine("\xF0\x9F\x98\x80 emoji") == "\xF0\x9F\x98\x80 emoji");
    CHECK(CleanLogLine("bad \xFF byte") == "bad " + kReplacement + " byte");
    CHECK(CleanLogLine("cut \xE6\x97") == "cut " + Times(kReplacement, 2));
    CHECK(CleanLogLine("\xC0\xAF") == Times(kReplacement, 2));          // overlong
    CHECK(CleanLogLine("\xED\xA0\x80") == Times(kReplacement, 3));      // surrogate
    CHECK(CleanLogLine("\xF4\x90\x80\x80") == Times(kReplacement, 4));  // above U+10FFFF
    CHECK(CleanLogLine("\x80") == kReplacement);                        // stray continuation byte

    // Splitting output into lines as it arrives.
    {
        LogSplitter splitter;
        std::vector<std::string> lines;
        splitter.Feed("one\ntw", lines);
        CHECK(lines == std::vector<std::string>({"one"}));
        splitter.Feed("o\r\nthree\n", lines);
        CHECK(lines == std::vector<std::string>({"one", "two", "three"}));
        splitter.Feed("a\r", lines);
        splitter.Feed("\nlast", lines);
        CHECK(lines.back() == "a");
        splitter.Flush(lines);
        CHECK(lines.back() == "last");
        const std::size_t count = lines.size();
        splitter.Flush(lines);
        CHECK(lines.size() == count);
    }
    {
        // Long lines break at the limit, never inside a UTF-8 sequence.
        LogSplitter splitter(10);
        std::vector<std::string> lines;
        splitter.Feed("abcdefghijKLM\n", lines);
        CHECK(lines == std::vector<std::string>({"abcdefghij", "KLM"}));
        lines.clear();
        splitter.Feed("abcdefgh日本\n", lines);
        CHECK(lines == std::vector<std::string>({"abcdefgh", "日本"}));
        lines.clear();
        splitter.Feed("abcdefg日本\n", lines);  // 日 ends exactly at the limit
        CHECK(lines == std::vector<std::string>({"abcdefg日", "本"}));
    }

    // The buffer keeps the newest lines.
    {
        LogBuffer buffer(3);
        CHECK(buffer.Size() == 0);
        const auto version = buffer.Version();
        for (int i = 1; i <= 5; ++i) {
            buffer.Append("line " + std::to_string(i));
        }
        CHECK(buffer.Size() == 3);
        CHECK(buffer.At(0) == "line 3");
        CHECK(buffer.At(2) == "line 5");
        CHECK(buffer.Version() != version);
        buffer.Append(std::vector<std::string>({"a", "b"}));
        CHECK(buffer.At(0) == "line 5" && buffer.At(2) == "b");
        const auto before = buffer.Version();
        buffer.Clear();
        CHECK(buffer.Size() == 0 && buffer.Version() != before);
    }
    return crisp::test::Result();
}
