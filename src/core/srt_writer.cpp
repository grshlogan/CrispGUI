#include "core/srt_writer.h"

namespace crisp::core {
namespace {

// Appends |value| in decimal with at least |width| digits.
void AppendPadded(std::string& out, std::int64_t value, std::size_t width) {
    const std::string digits = std::to_string(value);
    if (digits.size() < width) {
        out.append(width - digits.size(), '0');
    }
    out += digits;
}

bool IsUpperLetter(char c) {
    return c >= 'A' && c <= 'Z';
}

}  // namespace

std::string SrtTimestamp(std::int64_t milliseconds) {
    if (milliseconds < 0) {
        milliseconds = 0;
    }
    constexpr std::int64_t kHour = 60 * 60 * 1000;
    constexpr std::int64_t kMinute = 60 * 1000;
    std::string text;
    AppendPadded(text, milliseconds / kHour, 2);
    text += ':';
    AppendPadded(text, milliseconds % kHour / kMinute, 2);
    text += ':';
    AppendPadded(text, milliseconds % kMinute / 1000, 2);
    text += ',';
    AppendPadded(text, milliseconds % 1000, 3);
    return text;
}

std::string SrtSpeakerPrefix(const std::string& label) {
    if (label.empty()) {
        return std::string();
    }
    std::int64_t number = -1;
    if (label.size() == 1 && IsUpperLetter(label[0])) {
        number = label[0] - 'A';
    } else if (label.size() == 2 && IsUpperLetter(label[0]) && IsUpperLetter(label[1]) && label[0] != 'A') {
        // The server writes the numbers from 26 up as two letters whose first is at least "B"; a
        // label that starts with "A" cannot come from a number.
        number = (label[0] - 'A') * 26 + (label[1] - 'A');
    }
    if (number < 0) {
        return "(" + label + ") ";
    }
    return "(speaker " + std::to_string(number) + ") ";
}

std::string SegmentsToSrt(const std::vector<TranscriptSegment>& segments) {
    std::string srt;
    std::size_t cues = 0;
    for (const TranscriptSegment& segment : segments) {
        if (segment.text.empty()) {
            continue;
        }
        ++cues;
        srt += std::to_string(cues);
        srt += '\n';
        srt += SrtTimestamp(segment.startMs);
        srt += " --> ";
        srt += SrtTimestamp(segment.endMs);
        srt += '\n';
        srt += SrtSpeakerPrefix(segment.speaker);
        srt += segment.text;
        srt += "\n\n";
    }
    return srt;
}

}  // namespace crisp::core
