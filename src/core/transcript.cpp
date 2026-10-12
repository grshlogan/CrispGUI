#include "core/transcript.h"

#include "util/json.h"

namespace crisp::core {
namespace {

using util::JsonValue;

bool IsDigit(char c) {
    return c >= '0' && c <= '9';
}

// Reads the string member |key| of |object| into |out|; an absent member is an error only when
// |required|.
bool ReadString(const JsonValue& object, const char* key, bool required, std::string& out, const std::string& where,
                std::string& error) {
    const JsonValue* member = object.Find(key);
    if (member == nullptr) {
        if (required) {
            error = where + " has no \"" + key + "\".";
        }
        return !required;
    }
    if (!member->IsString()) {
        error = where + ": \"" + key + "\" is not a string.";
        return false;
    }
    out = member->AsString();
    return true;
}

// Reads the number of seconds |key| of |object| into whole milliseconds.
bool ReadMilliseconds(const JsonValue& object, const char* key, std::int64_t& out, const std::string& where,
                      std::string& error) {
    const JsonValue* member = object.Find(key);
    if (member == nullptr || !member->IsNumber()) {
        error = where + " has no number \"" + key + "\".";
        return false;
    }
    if (!SecondsToMilliseconds(member->NumberText(), out)) {
        error = where + ": \"" + key + "\" is out of range.";
        return false;
    }
    return true;
}

bool ReadSegment(const JsonValue& value, std::size_t index, TranscriptSegment& segment, std::string& error) {
    const std::string where = "Segment " + std::to_string(index);
    if (!value.IsObject()) {
        error = where + " is not an object.";
        return false;
    }
    if (const JsonValue* id = value.Find("id")) {
        if (!id->IsNumber() || !id->IsIntegerLiteral()) {
            error = where + ": \"id\" is not a whole number.";
            return false;
        }
        segment.id = static_cast<std::int64_t>(id->AsNumber());
    } else {
        segment.id = static_cast<std::int64_t>(index);
    }
    if (!ReadMilliseconds(value, "start", segment.startMs, where, error) ||
        !ReadMilliseconds(value, "end", segment.endMs, where, error) ||
        !ReadString(value, "text", true, segment.text, where, error) ||
        !ReadString(value, "speaker", false, segment.speaker, where, error)) {
        return false;
    }
    const JsonValue* words = value.Find("words");
    if (words == nullptr) {
        return true;
    }
    if (!words->IsArray()) {
        error = where + ": \"words\" is not an array.";
        return false;
    }
    for (std::size_t i = 0; i < words->Size(); ++i) {
        const JsonValue& item = words->At(i);
        const std::string wordWhere = where + ", word " + std::to_string(i);
        if (!item.IsObject()) {
            error = wordWhere + " is not an object.";
            return false;
        }
        TranscriptWord word;
        if (!ReadString(item, "word", true, word.word, wordWhere, error) ||
            !ReadMilliseconds(item, "start", word.startMs, wordWhere, error) ||
            !ReadMilliseconds(item, "end", word.endMs, wordWhere, error)) {
            return false;
        }
        segment.words.push_back(std::move(word));
    }
    return true;
}

}  // namespace

bool SecondsToMilliseconds(std::string_view text, std::int64_t& milliseconds) {
    // The JSON number grammar, collecting the digits of the integer and the fraction.
    std::size_t i = 0;
    const bool negative = i < text.size() && text[i] == '-';
    if (negative) {
        ++i;
    }
    std::string digits;
    const std::size_t integerStart = i;
    while (i < text.size() && IsDigit(text[i])) {
        digits.push_back(text[i++]);
    }
    const std::size_t integerLength = i - integerStart;
    if (integerLength == 0 || (integerLength > 1 && text[integerStart] == '0')) {
        return false;
    }
    long long fractionLength = 0;
    if (i < text.size() && text[i] == '.') {
        const std::size_t start = ++i;
        while (i < text.size() && IsDigit(text[i])) {
            digits.push_back(text[i++]);
        }
        fractionLength = static_cast<long long>(i - start);
        if (fractionLength == 0) {
            return false;
        }
    }
    long long exponent = 0;
    if (i < text.size() && (text[i] == 'e' || text[i] == 'E')) {
        ++i;
        bool exponentNegative = false;
        if (i < text.size() && (text[i] == '+' || text[i] == '-')) {
            exponentNegative = text[i] == '-';
            ++i;
        }
        const std::size_t start = i;
        while (i < text.size() && IsDigit(text[i])) {
            if (exponent < 100000) {
                exponent = exponent * 10 + (text[i] - '0');
            }
            ++i;
        }
        if (i == start) {
            return false;
        }
        if (exponentNegative) {
            exponent = -exponent;
        }
    }
    if (i != text.size()) {
        return false;
    }

    // The value is digits * 10^scale milliseconds.
    const long long scale = exponent - fractionLength + 3;
    const std::size_t first = digits.find_first_not_of('0');
    if (first == std::string::npos) {
        milliseconds = 0;
        return true;
    }
    digits.erase(0, first);
    constexpr long long kMaxDigits = 15;
    std::int64_t result = 0;
    if (scale >= 0) {
        if (static_cast<long long>(digits.size()) + scale > kMaxDigits) {
            return false;
        }
        for (const char c : digits) {
            result = result * 10 + (c - '0');
        }
        for (long long k = 0; k < scale; ++k) {
            result *= 10;
        }
    } else if (-scale <= static_cast<long long>(digits.size())) {
        const std::size_t keep = digits.size() - static_cast<std::size_t>(-scale);
        if (static_cast<long long>(keep) > kMaxDigits) {
            return false;
        }
        for (std::size_t k = 0; k < keep; ++k) {
            result = result * 10 + (digits[k] - '0');
        }
        if (digits[keep] >= '5') {
            ++result;  // half away from zero: the sign is applied afterwards
        }
    }
    milliseconds = negative ? -result : result;
    return true;
}

bool ParseVerboseJson(std::string_view body, Transcript& transcript, std::string& error) {
    transcript = Transcript();
    const util::JsonParseResult parsed = util::ParseJson(body);
    if (!parsed.Ok()) {
        error = "The answer is not JSON: " + parsed.error + " (line " + std::to_string(parsed.line) + ", column " +
                std::to_string(parsed.column) + ").";
        return false;
    }
    const JsonValue& root = parsed.value;
    if (!root.IsObject()) {
        error = "The answer is not a JSON object.";
        return false;
    }
    const std::string where = "The answer";
    if (!ReadString(root, "task", false, transcript.task, where, error) ||
        !ReadString(root, "language", false, transcript.language, where, error) ||
        !ReadString(root, "text", false, transcript.text, where, error) ||
        !ReadMilliseconds(root, "duration", transcript.durationMs, where, error)) {
        return false;
    }
    const JsonValue* segments = root.Find("segments");
    if (segments == nullptr || !segments->IsArray()) {
        error = "The answer has no array \"segments\".";
        return false;
    }
    transcript.segments.reserve(segments->Size());
    for (std::size_t i = 0; i < segments->Size(); ++i) {
        TranscriptSegment segment;
        if (!ReadSegment(segments->At(i), i, segment, error)) {
            return false;
        }
        transcript.segments.push_back(std::move(segment));
    }
    return true;
}

}  // namespace crisp::core
