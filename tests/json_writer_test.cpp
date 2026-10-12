#include "check.h"

#include "util/json.h"

#include <cmath>
#include <limits>
#include <string>

using crisp::util::JsonValue;
using crisp::util::ParseJson;
using crisp::util::QuoteJson;
using crisp::util::WriteJson;

int main() {
    // Every value type, compact.
    JsonValue root = JsonValue::MakeObject();
    root.AddMember("text", JsonValue::MakeString("quote \" backslash \\ line\nfeed\ttab \x01 \x7F \xE6\x97\xA5"));
    root.AddMember("kept", JsonValue::MakeNumber(0.3, "0.30"));
    root.AddMember("whole", JsonValue::MakeInteger(-42));
    root.AddMember("big", JsonValue::MakeInteger(9007199254740993LL));
    root.AddMember("half", JsonValue::MakeNumber(0.5));
    root.AddMember("nan", JsonValue::MakeNumber(std::nan("")));
    root.AddMember("infinite", JsonValue::MakeNumber(std::numeric_limits<double>::infinity()));
    root.AddMember("yes", JsonValue::MakeBoolean(true));
    root.AddMember("none", JsonValue());
    JsonValue list = JsonValue::MakeArray();
    list.Append(JsonValue::MakeInteger(1));
    list.Append(JsonValue::MakeArray());
    list.Append(JsonValue::MakeObject());
    root.AddMember("list", list);
    root.AddMember("key \"q\"", JsonValue::MakeString(""));
    const std::string compact = WriteJson(root);
    CHECK(compact ==
          "{\"text\":\"quote \\\" backslash \\\\ line\\nfeed\\ttab \\u0001 \\u007f \xE6\x97\xA5\",\"kept\":0.30,"
          "\"whole\":-42,\"big\":9007199254740993,\"half\":0.5,\"nan\":null,\"infinite\":null,\"yes\":true,"
          "\"none\":null,\"list\":[1,[],{}],\"key \\\"q\\\"\":\"\"}");

    // Pretty: one member or item per line, two spaces per level, empty containers inline.
    JsonValue small = JsonValue::MakeObject();
    small.AddMember("a", list);
    small.AddMember("b", JsonValue::MakeObject());
    CHECK(WriteJson(small, true) == "{\n  \"a\": [\n    1,\n    [],\n    {}\n  ],\n  \"b\": {}\n}");
    CHECK(WriteJson(JsonValue::MakeArray(), true) == "[]");

    // What it writes reads back the same, and writing again gives the same text.
    const auto reparsed = ParseJson(compact);
    CHECK(reparsed.Ok());
    CHECK(WriteJson(reparsed.value) == compact);
    const auto pretty = ParseJson(WriteJson(root, true));
    CHECK(pretty.Ok() && WriteJson(pretty.value) == compact);
    const auto big = reparsed.value.Find("big");
    CHECK(big != nullptr && big->NumberText() == "9007199254740993");

    // Bytes that are not valid UTF-8 become U+FFFD, so the output still parses.
    const std::string broken = QuoteJson("a\xFF" "b\xE6\x97");
    CHECK(broken == "\"a\xEF\xBF\xBD" "b\xEF\xBF\xBD\xEF\xBF\xBD\"");
    CHECK(ParseJson(broken).Ok());
    CHECK(QuoteJson("a\"b") == "\"a\\\"b\"");
    CHECK(QuoteJson("\b\f\r") == "\"\\b\\f\\r\"");
    return crisp::test::Result();
}
