#include "check.h"

#include "core/jsonc.h"

#include <string>

using crisp::core::ParseJsonc;
using crisp::core::StripJsonComments;

int main() {
    // Line comments of both kinds go; line breaks stay.
    CHECK(StripJsonComments("{ // note\n  \"a\": 1 # note\n}") == "{ \n  \"a\": 1 \n}");
    CHECK(StripJsonComments("# whole line\n// whole line\n{}") == "\n\n{}");
    CHECK(StripJsonComments("{}  //") == "{}  ");

    // Comment markers inside strings stay: URLs and hashes.
    const std::string url = "{\"download\": \"https://huggingface.co/x#readme\"} // tail";
    CHECK(StripJsonComments(url) == "{\"download\": \"https://huggingface.co/x#readme\"} ");

    // An escaped quote does not end the string, so the // after it is text.
    const std::string escapedQuote = "{\"comment\": \"say \\\"hi\\\" // not a comment\"} // a comment";
    CHECK(StripJsonComments(escapedQuote) == "{\"comment\": \"say \\\"hi\\\" // not a comment\"} ");
    const auto parsed = ParseJsonc(escapedQuote);
    CHECK(parsed.Ok());
    CHECK(parsed.value.Find("comment") != nullptr &&
          parsed.value.Find("comment")->AsString() == "say \"hi\" // not a comment");

    // An escaped backslash at the end of a string does not escape the closing quote.
    const auto backslash = ParseJsonc("{\"path\": \"models\\\\\" // comment\n}");
    CHECK(backslash.Ok());
    CHECK(backslash.value.Find("path")->AsString() == "models\\");

    // A single slash is not a comment.
    CHECK(StripJsonComments("{\"a\": \"/\"} / x") == "{\"a\": \"/\"} / x");

    // Like the launcher, a string ends at the end of its line, so the next line's comment goes.
    CHECK(StripJsonComments("\"open\n// comment") == "\"open\n");

    // CRLF line endings: the comment takes the CR with it; the JSON still parses.
    const auto crlf = ParseJsonc("{\r\n  \"a\": 1, // one\r\n  \"b\": 2 # two\r\n}\r\n");
    CHECK(crlf.Ok());
    CHECK(crlf.value.Find("b") != nullptr && crlf.value.Find("b")->AsNumber() == 2.0);

    // Errors point at the original line, also after stripped comments.
    const auto error = ParseJsonc("{ // first\n  \"a\": 1, // second\n  \"b\": oops\n}");
    CHECK(!error.Ok());
    CHECK(error.line == 3);

    // Block comments are not part of the format, as in the launcher.
    CHECK(!ParseJsonc("{ /* no */ }").Ok());
    return crisp::test::Result();
}
