#include "util/json.h"

#include <charconv>
#include <cstdint>
#include <cstdio>
#include <system_error>
#include <utility>

namespace crisp::util {
namespace {

bool IsDigit(char c) {
    return c >= '0' && c <= '9';
}

int HexValue(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
}

void AppendUtf8(std::string& out, std::uint32_t codePoint) {
    if (codePoint < 0x80) {
        out.push_back(static_cast<char>(codePoint));
    } else if (codePoint < 0x800) {
        out.push_back(static_cast<char>(0xC0 | (codePoint >> 6)));
        out.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
    } else if (codePoint < 0x10000) {
        out.push_back(static_cast<char>(0xE0 | (codePoint >> 12)));
        out.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
    } else {
        out.push_back(static_cast<char>(0xF0 | (codePoint >> 18)));
        out.push_back(static_cast<char>(0x80 | ((codePoint >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
    }
}

// Recursive-descent parser over the whole document. The first error stops it.
class Parser {
public:
    explicit Parser(std::string_view text) : text_(text) {}

    JsonParseResult Run() {
        JsonParseResult result;
        SkipWhitespace();
        if (AtEnd()) {
            Fail("The document is empty.");
        } else if (ParseValue(result.value, 0)) {
            SkipWhitespace();
            if (!AtEnd()) {
                Fail("Unexpected text after the JSON value.");
            }
        }
        if (!error_.empty()) {
            result.value = JsonValue();
            result.error = error_;
            result.offset = errorOffset_;
            LineAndColumn(errorOffset_, result.line, result.column);
        }
        return result;
    }

private:
    bool AtEnd() const { return pos_ >= text_.size(); }
    char Peek() const { return text_[pos_]; }

    bool Fail(std::string message) { return FailAt(pos_, std::move(message)); }

    bool FailAt(std::size_t offset, std::string message) {
        if (error_.empty()) {
            error_ = std::move(message);
            errorOffset_ = offset;
        }
        return false;
    }

    std::string Describe(char c) const {
        const unsigned char byte = static_cast<unsigned char>(c);
        if (byte >= 0x20 && byte < 0x7F) {
            return std::string("'") + c + "'";
        }
        char buffer[16];
        std::snprintf(buffer, sizeof(buffer), "byte 0x%02X", byte);
        return buffer;
    }

    void SkipWhitespace() {
        while (!AtEnd()) {
            const char c = Peek();
            if (c != ' ' && c != '\t' && c != '\n' && c != '\r') {
                return;
            }
            ++pos_;
        }
    }

    bool ParseValue(JsonValue& out, int depth) {
        if (AtEnd()) {
            return Fail("The document ends where a value was expected.");
        }
        switch (Peek()) {
        case '{':
            return ParseObject(out, depth + 1);
        case '[':
            return ParseArray(out, depth + 1);
        case '"': {
            std::string value;
            if (!ParseString(value)) {
                return false;
            }
            out = JsonValue::MakeString(std::move(value));
            return true;
        }
        case 't':
            return ParseLiteral("true", JsonValue::MakeBoolean(true), out);
        case 'f':
            return ParseLiteral("false", JsonValue::MakeBoolean(false), out);
        case 'n':
            return ParseLiteral("null", JsonValue(), out);
        default:
            if (Peek() == '-' || IsDigit(Peek())) {
                return ParseNumber(out);
            }
            return Fail("Unexpected " + Describe(Peek()) + " where a value was expected.");
        }
    }

    bool ParseObject(JsonValue& out, int depth) {
        if (depth > kJsonMaxDepth) {
            return Fail("Arrays and objects are nested too deeply.");
        }
        ++pos_;  // '{'
        out = JsonValue::MakeObject();
        SkipWhitespace();
        if (!AtEnd() && Peek() == '}') {
            ++pos_;
            return true;
        }
        for (;;) {
            SkipWhitespace();
            if (AtEnd()) {
                return Fail("The document ends inside an object.");
            }
            if (Peek() == '}') {
                return Fail("Trailing comma before '}'.");
            }
            if (Peek() != '"') {
                return Fail("Unexpected " + Describe(Peek()) + " where a member name in double quotes was expected.");
            }
            std::string key;
            if (!ParseString(key)) {
                return false;
            }
            SkipWhitespace();
            if (AtEnd() || Peek() != ':') {
                return Fail("A ':' was expected after the member name.");
            }
            ++pos_;
            SkipWhitespace();
            JsonValue value;
            if (!ParseValue(value, depth)) {
                return false;
            }
            out.AddMember(std::move(key), std::move(value));
            SkipWhitespace();
            if (AtEnd()) {
                return Fail("The document ends inside an object.");
            }
            if (Peek() == ',') {
                ++pos_;
                continue;
            }
            if (Peek() == '}') {
                ++pos_;
                return true;
            }
            if (Peek() == '"') {
                return Fail("A ',' is missing before this member.");
            }
            return Fail("Unexpected " + Describe(Peek()) + " where ',' or '}' was expected.");
        }
    }

    bool ParseArray(JsonValue& out, int depth) {
        if (depth > kJsonMaxDepth) {
            return Fail("Arrays and objects are nested too deeply.");
        }
        ++pos_;  // '['
        out = JsonValue::MakeArray();
        SkipWhitespace();
        if (!AtEnd() && Peek() == ']') {
            ++pos_;
            return true;
        }
        for (;;) {
            SkipWhitespace();
            if (!AtEnd() && Peek() == ']') {
                return Fail("Trailing comma before ']'.");
            }
            JsonValue item;
            if (!ParseValue(item, depth)) {
                return false;
            }
            out.Append(std::move(item));
            SkipWhitespace();
            if (AtEnd()) {
                return Fail("The document ends inside an array.");
            }
            if (Peek() == ',') {
                ++pos_;
                continue;
            }
            if (Peek() == ']') {
                ++pos_;
                return true;
            }
            return Fail("Unexpected " + Describe(Peek()) + " where ',' or ']' was expected.");
        }
    }

    bool ParseLiteral(std::string_view word, JsonValue value, JsonValue& out) {
        if (text_.substr(pos_, word.size()) != word) {
            return Fail("Unexpected " + Describe(Peek()) + " where a value was expected.");
        }
        pos_ += word.size();
        out = std::move(value);
        return true;
    }

    bool ParseNumber(JsonValue& out) {
        const std::size_t start = pos_;
        if (Peek() == '-') {
            ++pos_;
        }
        if (AtEnd() || !IsDigit(Peek())) {
            return Fail("A digit was expected in the number.");
        }
        if (Peek() == '0') {
            ++pos_;
            if (!AtEnd() && IsDigit(Peek())) {
                return Fail("Numbers must not have leading zeros.");
            }
        } else {
            while (!AtEnd() && IsDigit(Peek())) {
                ++pos_;
            }
        }
        if (!AtEnd() && Peek() == '.') {
            ++pos_;
            if (AtEnd() || !IsDigit(Peek())) {
                return Fail("A digit was expected after the decimal point.");
            }
            while (!AtEnd() && IsDigit(Peek())) {
                ++pos_;
            }
        }
        if (!AtEnd() && (Peek() == 'e' || Peek() == 'E')) {
            ++pos_;
            if (!AtEnd() && (Peek() == '+' || Peek() == '-')) {
                ++pos_;
            }
            if (AtEnd() || !IsDigit(Peek())) {
                return Fail("A digit was expected in the exponent.");
            }
            while (!AtEnd() && IsDigit(Peek())) {
                ++pos_;
            }
        }
        const std::string_view lexeme = text_.substr(start, pos_ - start);
        double value = 0.0;
        const std::from_chars_result parsed = std::from_chars(lexeme.data(), lexeme.data() + lexeme.size(), value);
        if (parsed.ec == std::errc::result_out_of_range) {
            return FailAt(start, "The number is out of range.");
        }
        if (parsed.ec != std::errc() || parsed.ptr != lexeme.data() + lexeme.size()) {
            return FailAt(start, "The number could not be read.");
        }
        out = JsonValue::MakeNumber(value, std::string(lexeme));
        return true;
    }

    bool ParseString(std::string& out) {
        const std::size_t start = pos_;
        ++pos_;  // opening quote
        for (;;) {
            if (AtEnd()) {
                return FailAt(start, "The string is not terminated.");
            }
            const unsigned char c = static_cast<unsigned char>(Peek());
            if (c == '"') {
                ++pos_;
                return true;
            }
            if (c == '\\') {
                if (!ParseEscape(out)) {
                    return false;
                }
                continue;
            }
            if (c == '\n' || c == '\r') {
                return FailAt(start, "The string is not terminated before the end of the line.");
            }
            if (c < 0x20) {
                return Fail("Control characters must be escaped inside strings.");
            }
            if (c < 0x80) {
                out.push_back(static_cast<char>(c));
                ++pos_;
                continue;
            }
            const std::size_t length = Utf8SequenceLength();
            if (length == 0) {
                return Fail("The string is not valid UTF-8.");
            }
            out.append(text_.substr(pos_, length));
            pos_ += length;
        }
    }

    // The length of the valid UTF-8 sequence at pos_, or 0 when it is invalid: a stray
    // continuation byte, an overlong form, a surrogate, or a code point above U+10FFFF.
    std::size_t Utf8SequenceLength() const {
        const unsigned char lead = static_cast<unsigned char>(text_[pos_]);
        std::size_t length = 0;
        std::uint32_t codePoint = 0;
        std::uint32_t minimum = 0;
        if (lead >= 0xC2 && lead <= 0xDF) {
            length = 2;
            codePoint = lead & 0x1F;
            minimum = 0x80;
        } else if (lead >= 0xE0 && lead <= 0xEF) {
            length = 3;
            codePoint = lead & 0x0F;
            minimum = 0x800;
        } else if (lead >= 0xF0 && lead <= 0xF4) {
            length = 4;
            codePoint = lead & 0x07;
            minimum = 0x10000;
        } else {
            return 0;
        }
        if (pos_ + length > text_.size()) {
            return 0;
        }
        for (std::size_t i = 1; i < length; ++i) {
            const unsigned char next = static_cast<unsigned char>(text_[pos_ + i]);
            if ((next & 0xC0) != 0x80) {
                return 0;
            }
            codePoint = (codePoint << 6) | (next & 0x3F);
        }
        if (codePoint < minimum || codePoint > 0x10FFFF || (codePoint >= 0xD800 && codePoint <= 0xDFFF)) {
            return 0;
        }
        return length;
    }

    bool ReadHex4(std::uint32_t& value) {
        if (pos_ + 4 > text_.size()) {
            return false;
        }
        value = 0;
        for (std::size_t i = 0; i < 4; ++i) {
            const int digit = HexValue(text_[pos_ + i]);
            if (digit < 0) {
                return false;
            }
            value = (value << 4) | static_cast<std::uint32_t>(digit);
        }
        pos_ += 4;
        return true;
    }

    bool ParseEscape(std::string& out) {
        const std::size_t start = pos_;
        ++pos_;  // backslash
        if (AtEnd()) {
            return FailAt(start, "The string is not terminated.");
        }
        const char c = Peek();
        ++pos_;
        switch (c) {
        case '"':
            out.push_back('"');
            return true;
        case '\\':
            out.push_back('\\');
            return true;
        case '/':
            out.push_back('/');
            return true;
        case 'b':
            out.push_back('\b');
            return true;
        case 'f':
            out.push_back('\f');
            return true;
        case 'n':
            out.push_back('\n');
            return true;
        case 'r':
            out.push_back('\r');
            return true;
        case 't':
            out.push_back('\t');
            return true;
        case 'u':
            return ParseUnicodeEscape(start, out);
        default:
            return FailAt(start, "Invalid escape sequence \\" + std::string(1, c) +
                                     " in a string; write a backslash as \\\\, also in Windows paths.");
        }
    }

    bool ParseUnicodeEscape(std::size_t start, std::string& out) {
        std::uint32_t unit = 0;
        if (!ReadHex4(unit)) {
            return FailAt(start, "\\u must be followed by four hexadecimal digits.");
        }
        std::uint32_t codePoint = unit;
        if (unit >= 0xD800 && unit <= 0xDBFF) {
            std::uint32_t low = 0;
            if (pos_ + 1 >= text_.size() || text_[pos_] != '\\' || text_[pos_ + 1] != 'u') {
                return FailAt(start, "A \\u escape holds half of a surrogate pair without the other half.");
            }
            pos_ += 2;
            if (!ReadHex4(low)) {
                return FailAt(start, "\\u must be followed by four hexadecimal digits.");
            }
            if (low < 0xDC00 || low > 0xDFFF) {
                return FailAt(start, "A \\u escape holds half of a surrogate pair without the other half.");
            }
            codePoint = 0x10000 + ((unit - 0xD800) << 10) + (low - 0xDC00);
        } else if (unit >= 0xDC00 && unit <= 0xDFFF) {
            return FailAt(start, "A \\u escape holds half of a surrogate pair without the other half.");
        }
        AppendUtf8(out, codePoint);
        return true;
    }

    void LineAndColumn(std::size_t offset, int& line, int& column) const {
        line = 1;
        column = 1;
        for (std::size_t i = 0; i < offset && i < text_.size(); ++i) {
            const unsigned char c = static_cast<unsigned char>(text_[i]);
            if (c == '\n') {
                ++line;
                column = 1;
            } else if ((c & 0xC0) != 0x80) {
                ++column;  // count characters, not UTF-8 continuation bytes
            }
        }
    }

    std::string_view text_;
    std::size_t pos_ = 0;
    std::string error_;
    std::size_t errorOffset_ = 0;
};

}  // namespace

JsonValue JsonValue::MakeBoolean(bool value) {
    JsonValue result;
    result.type_ = JsonType::kBoolean;
    result.boolean_ = value;
    return result;
}

JsonValue JsonValue::MakeNumber(double value, std::string text) {
    JsonValue result;
    result.type_ = JsonType::kNumber;
    result.number_ = value;
    result.text_ = std::move(text);
    return result;
}

JsonValue JsonValue::MakeString(std::string value) {
    JsonValue result;
    result.type_ = JsonType::kString;
    result.text_ = std::move(value);
    return result;
}

JsonValue JsonValue::MakeArray() {
    JsonValue result;
    result.type_ = JsonType::kArray;
    return result;
}

JsonValue JsonValue::MakeObject() {
    JsonValue result;
    result.type_ = JsonType::kObject;
    return result;
}

bool JsonValue::IsIntegerLiteral() const {
    return type_ == JsonType::kNumber && text_.find_first_of(".eE") == std::string::npos;
}

const JsonValue* JsonValue::Find(std::string_view key) const {
    if (type_ != JsonType::kObject) {
        return nullptr;
    }
    for (std::size_t i = 0; i < keys_.size(); ++i) {
        if (keys_[i] == key) {
            return &values_[i];
        }
    }
    return nullptr;
}

void JsonValue::Append(JsonValue value) {
    if (type_ == JsonType::kArray) {
        values_.push_back(std::move(value));
    }
}

void JsonValue::AddMember(std::string key, JsonValue value) {
    if (type_ == JsonType::kObject) {
        keys_.push_back(std::move(key));
        values_.push_back(std::move(value));
    }
}

JsonParseResult ParseJson(std::string_view text) {
    return Parser(text).Run();
}

std::string FormatJsonNumber(double value) {
    char buffer[32];
    const std::to_chars_result written = std::to_chars(buffer, buffer + sizeof(buffer), value);
    if (written.ec != std::errc()) {
        return std::string();
    }
    return std::string(buffer, written.ptr);
}

}  // namespace crisp::util
