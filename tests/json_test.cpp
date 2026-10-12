#include "check.h"

#include "util/json.h"

#include <string>

using crisp::util::FormatJsonNumber;
using crisp::util::JsonParseResult;
using crisp::util::JsonType;
using crisp::util::JsonValue;
using crisp::util::ParseJson;

namespace {

bool Fails(const std::string& text) {
    return !ParseJson(text).Ok();
}

}  // namespace

int main() {
    // A document with every value type; members keep their order.
    const JsonParseResult parsed = ParseJson(
        " {\"name\": \"cohere\", \"port\": 5114, \"rate\": -0.25e1, \"vad\": true, \"off\": false,"
        " \"none\": null, \"args\": [\"--vad-threshold\", \"0.3\"], \"env\": {}} ");
    CHECK(parsed.Ok());
    const JsonValue& root = parsed.value;
    CHECK(root.IsObject());
    CHECK(root.Size() == 8);
    CHECK(root.KeyAt(0) == "name");
    CHECK(root.KeyAt(7) == "env");
    CHECK(root.Find("name") != nullptr && root.Find("name")->AsString() == "cohere");
    CHECK(root.Find("port")->IsNumber() && root.Find("port")->AsNumber() == 5114.0);
    CHECK(root.Find("port")->IsIntegerLiteral());
    CHECK(root.Find("rate")->AsNumber() == -2.5);
    CHECK(root.Find("rate")->NumberText() == "-0.25e1");
    CHECK(!root.Find("rate")->IsIntegerLiteral());
    CHECK(root.Find("vad")->IsBoolean() && root.Find("vad")->AsBoolean());
    CHECK(root.Find("off")->IsBoolean() && !root.Find("off")->AsBoolean());
    CHECK(root.Find("none")->IsNull());
    CHECK(root.Find("args")->IsArray() && root.Find("args")->Size() == 2);
    CHECK(root.Find("args")->At(1).AsString() == "0.3");
    CHECK(root.Find("env")->IsObject() && root.Find("env")->Size() == 0);
    CHECK(root.Find("missing") == nullptr);
    CHECK(root.Find("args")->Find("name") == nullptr);  // not an object

    // Duplicate keys are kept; Find returns the first.
    const JsonParseResult duplicates = ParseJson("{\"a\": 1, \"a\": 2}");
    CHECK(duplicates.Ok() && duplicates.value.Size() == 2 && duplicates.value.Find("a")->AsNumber() == 1.0);

    // Any value can be the document.
    CHECK(ParseJson("42").Ok() && ParseJson("42").value.Type() == JsonType::kNumber);
    CHECK(ParseJson("\"text\"").value.AsString() == "text");
    CHECK(ParseJson("[]").value.IsArray());

    // Escapes, including surrogate pairs, decode to UTF-8; raw UTF-8 passes through.
    const JsonParseResult escapes =
        ParseJson("\"q\\\"b\\\\s\\/n\\n t\\t \\u00e9 \\u65e5 \\ud83d\\ude00 \xE6\x97\xA5\xE6\x9C\xAC\"");
    CHECK(escapes.Ok());
    CHECK(escapes.value.AsString() ==
          "q\"b\\s/n\n t\t \xC3\xA9 \xE6\x97\xA5 \xF0\x9F\x98\x80 \xE6\x97\xA5\xE6\x9C\xAC");
    // A Windows path written the JSON way.
    CHECK(ParseJson("\"Japanese\\\\model.gguf\"").value.AsString() == "Japanese\\model.gguf");
    // An embedded NUL from \u0000 stays in the string.
    CHECK(ParseJson("\"a\\u0000b\"").value.AsString() == std::string("a\0b", 3));

    // Numbers.
    CHECK(ParseJson("0").value.AsNumber() == 0.0);
    CHECK(ParseJson("-0.5").value.AsNumber() == -0.5);
    CHECK(ParseJson("1E+2").value.AsNumber() == 100.0);
    CHECK(ParseJson("18446744073709551615").value.NumberText() == "18446744073709551615");

    // Rejected input.
    CHECK(Fails(""));
    CHECK(Fails("   "));
    CHECK(Fails("{\"a\": 1,}"));    // trailing comma
    CHECK(Fails("[1, 2,]"));        // trailing comma
    CHECK(Fails("{\"a\" 1}"));      // missing colon
    CHECK(Fails("{\"a\": 1 \"b\": 2}"));  // missing comma
    CHECK(Fails("{a: 1}"));         // unquoted key
    CHECK(Fails("{'a': 1}"));       // single quotes
    CHECK(Fails("// comment\n{}"));  // comments are JSONC, not JSON
    CHECK(Fails("01"));
    CHECK(Fails("1."));
    CHECK(Fails(".5"));
    CHECK(Fails("+1"));
    CHECK(Fails("1e"));
    CHECK(Fails("-"));
    CHECK(Fails("1e999"));          // out of range
    CHECK(Fails("NaN"));
    CHECK(Fails("Infinity"));
    CHECK(Fails("True"));
    CHECK(Fails("nul"));
    CHECK(Fails("\"unterminated"));
    CHECK(Fails("\"line\nbreak\""));
    CHECK(Fails("\"tab\there\""));   // raw control character
    CHECK(Fails("\"\\x41\""));       // invalid escape
    CHECK(Fails("\"\\u12\""));       // short \u escape
    CHECK(Fails("\"\\ud83d\""));     // high surrogate alone
    CHECK(Fails("\"\\ude00\""));     // low surrogate alone
    CHECK(Fails("\"\xC3\""));        // truncated UTF-8
    CHECK(Fails("\"\xC0\xAF\""));    // overlong UTF-8
    CHECK(Fails("\"\xED\xA0\x80\""));  // UTF-8-encoded surrogate
    CHECK(Fails("\"\xF4\x90\x80\x80\""));  // above U+10FFFF
    CHECK(Fails("\"\x80\""));        // stray continuation byte
    CHECK(Fails("{} {}"));           // text after the value
    CHECK(Fails(std::string(65, '[') + std::string(65, ']')));  // too deep
    CHECK(ParseJson(std::string(64, '[') + std::string(64, ']')).Ok());

    // Errors carry the line and the column, counted in characters.
    const JsonParseResult located = ParseJson("{\n  \"\xE6\x97\xA5\": 1,\n  \"b\": x\n}");
    CHECK(!located.Ok());
    CHECK(located.line == 3);
    CHECK(located.column == 8);
    const JsonParseResult wide = ParseJson("{\"\xE6\x97\xA5\xE6\x9C\xAC\": ?}");
    CHECK(wide.line == 1 && wide.column == 8);
    const JsonParseResult missingComma = ParseJson("{\n  \"a\": 1\n  \"b\": 2\n}");
    CHECK(missingComma.line == 3 && missingComma.column == 3);
    CHECK(missingComma.error.find("',' is missing") != std::string::npos);
    const JsonParseResult badEscape = ParseJson("{\"model\": \"Japanese\\model.gguf\"}");
    CHECK(badEscape.error.find("Windows paths") != std::string::npos);
    CHECK(!ParseJson("{\"a\": 1,}").value.IsObject());  // no partial value on failure

    // Shortest round-trip formatting.
    CHECK(FormatJsonNumber(30.0) == "30");
    CHECK(FormatJsonNumber(0.3) == "0.3");
    CHECK(FormatJsonNumber(-2.5) == "-2.5");
    CHECK(FormatJsonNumber(1e21) == "1e+21");
    CHECK(FormatJsonNumber(0.1 + 0.2) == "0.30000000000000004");
    return crisp::test::Result();
}
