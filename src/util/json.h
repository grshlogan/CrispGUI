#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace crisp::util {

enum class JsonType { kNull, kBoolean, kNumber, kString, kArray, kObject };

// A parsed JSON value. Objects keep their members in document order, duplicates included;
// numbers keep the text they were written as next to their double value.
class JsonValue {
public:
    JsonValue() = default;  // null

    static JsonValue MakeBoolean(bool value);
    static JsonValue MakeNumber(double value, std::string text);
    // A number written as FormatJsonNumber writes it; NaN and infinity have no text and are
    // written as null.
    static JsonValue MakeNumber(double value);
    // A whole number, written exactly.
    static JsonValue MakeInteger(std::int64_t value);
    static JsonValue MakeString(std::string value);
    static JsonValue MakeArray();
    static JsonValue MakeObject();

    JsonType Type() const { return type_; }
    bool IsNull() const { return type_ == JsonType::kNull; }
    bool IsBoolean() const { return type_ == JsonType::kBoolean; }
    bool IsNumber() const { return type_ == JsonType::kNumber; }
    bool IsString() const { return type_ == JsonType::kString; }
    bool IsArray() const { return type_ == JsonType::kArray; }
    bool IsObject() const { return type_ == JsonType::kObject; }

    bool AsBoolean() const { return boolean_; }
    double AsNumber() const { return number_; }
    // The number as written, such as "0.30" or "1e3".
    const std::string& NumberText() const { return text_; }
    // True for a number written without a fraction or an exponent.
    bool IsIntegerLiteral() const;
    const std::string& AsString() const { return text_; }

    // The number of array items or object members; 0 for other types.
    std::size_t Size() const { return values_.size(); }
    // Array item or object member value |index| (< Size()).
    const JsonValue& At(std::size_t index) const { return values_[index]; }
    // Object member name |index| (< Size()).
    const std::string& KeyAt(std::size_t index) const { return keys_[index]; }
    // The first member named |key|; nullptr when there is none or this is not an object.
    const JsonValue* Find(std::string_view key) const;

    void Append(JsonValue value);
    void AddMember(std::string key, JsonValue value);

private:
    JsonType type_ = JsonType::kNull;
    bool boolean_ = false;
    double number_ = 0.0;
    std::string text_;               // string value, or the number as written
    std::vector<std::string> keys_;  // object member names, parallel to values_
    std::vector<JsonValue> values_;  // array items or object member values
};

struct JsonParseResult {
    JsonValue value;
    std::string error;       // empty on success
    std::size_t offset = 0;  // byte offset of the error
    int line = 0;            // 1-based line of the error
    int column = 0;          // 1-based column of the error, counted in characters

    bool Ok() const { return error.empty(); }
};

// Arrays and objects nested deeper than this are rejected.
constexpr int kJsonMaxDepth = 64;

// Parses strict JSON (RFC 8259): no comments, no trailing commas, no NaN or Infinity.
// Strings must be valid UTF-8, and escapes are decoded, \u surrogate pairs included.
JsonParseResult ParseJson(std::string_view text);

// The shortest text that reads back as |value|: "30", "0.3", "1e+21". Not for NaN or infinity.
std::string FormatJsonNumber(double value);

// JSON text for |value|: compact, or with |pretty| one member or item per line, indented by two
// spaces, without a final line break. Numbers are written as their text, and a number without
// text as null. Strings get the escapes RFC 8259 requires, and bytes that are not valid UTF-8
// become U+FFFD, so the output always parses.
std::string WriteJson(const JsonValue& value, bool pretty = false);

// |text| as a JSON string literal, quotes included, escaped as WriteJson escapes strings.
std::string QuoteJson(std::string_view text);

}  // namespace crisp::util
