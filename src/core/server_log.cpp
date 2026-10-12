#include "core/server_log.h"

#include <utility>

namespace crisp::core {
namespace {

constexpr const char* kReplacement = "\xEF\xBF\xBD";  // U+FFFD

unsigned char Byte(std::string_view text, std::size_t index) {
    return static_cast<unsigned char>(text[index]);
}

// The length of the UTF-8 sequence that starts at |index|, or 0 when it is invalid: a stray
// continuation byte, an overlong form, a surrogate, a value above U+10FFFF, or a cut sequence.
std::size_t ValidSequenceLength(std::string_view text, std::size_t index) {
    const unsigned char lead = Byte(text, index);
    std::size_t length = 0;
    unsigned char low = 0x80;
    unsigned char high = 0xBF;
    if (lead >= 0xC2 && lead <= 0xDF) {
        length = 2;
    } else if (lead >= 0xE0 && lead <= 0xEF) {
        length = 3;
        low = lead == 0xE0 ? 0xA0 : 0x80;
        high = lead == 0xED ? 0x9F : 0xBF;
    } else if (lead >= 0xF0 && lead <= 0xF4) {
        length = 4;
        low = lead == 0xF0 ? 0x90 : 0x80;
        high = lead == 0xF4 ? 0x8F : 0xBF;
    } else {
        return 0;
    }
    if (index + length > text.size()) {
        return 0;
    }
    for (std::size_t i = 1; i < length; ++i) {
        const unsigned char next = Byte(text, index + i);
        if (next < (i == 1 ? low : 0x80) || next > (i == 1 ? high : 0xBF)) {
            return 0;
        }
    }
    return length;
}

// The index after the escape sequence that starts with ESC at |index|.
std::size_t SkipEscape(std::string_view text, std::size_t index) {
    std::size_t i = index + 1;
    if (i >= text.size()) {
        return i;
    }
    if (text[i] == '[') {  // CSI: parameters and intermediates, then one final byte
        ++i;
        while (i < text.size() && Byte(text, i) >= 0x20 && Byte(text, i) <= 0x3F) {
            ++i;
        }
        return i < text.size() ? i + 1 : i;
    }
    if (text[i] == ']') {  // OSC: up to BEL or ESC backslash
        while (++i < text.size()) {
            if (text[i] == '\a') {
                return i + 1;
            }
            if (text[i] == '\x1B' && i + 1 < text.size() && text[i + 1] == '\\') {
                return i + 2;
            }
        }
        return i;
    }
    return i + 1;
}

// The length of the sequence a lead byte announces; 1 for ASCII and invalid bytes.
std::size_t AnnouncedLength(unsigned char lead) {
    return lead >= 0xF0 ? 4 : lead >= 0xE0 ? 3 : lead >= 0xC0 ? 2 : 1;
}

}  // namespace

std::string CleanLogLine(std::string_view raw) {
    while (!raw.empty() && raw.back() == '\r') {
        raw.remove_suffix(1);
    }
    const std::size_t carriageReturn = raw.rfind('\r');
    if (carriageReturn != std::string_view::npos) {
        raw.remove_prefix(carriageReturn + 1);
    }
    std::string line;
    line.reserve(raw.size());
    for (std::size_t i = 0; i < raw.size();) {
        const unsigned char c = Byte(raw, i);
        if (c == 0x1B) {
            i = SkipEscape(raw, i);
        } else if (c == '\t') {
            line.push_back(' ');
            ++i;
        } else if (c < 0x20 || c == 0x7F) {
            ++i;
        } else if (c < 0x80) {
            line.push_back(static_cast<char>(c));
            ++i;
        } else if (const std::size_t length = ValidSequenceLength(raw, i)) {
            line.append(raw.substr(i, length));
            i += length;
        } else {
            line += kReplacement;
            ++i;
        }
    }
    return line;
}

void LogSplitter::Feed(std::string_view bytes, std::vector<std::string>& lines) {
    for (const char c : bytes) {
        if (c == '\n') {
            lines.push_back(CleanLogLine(pending_));
            pending_.clear();
            continue;
        }
        pending_.push_back(c);
        if (pending_.size() < maxLineBytes_) {
            continue;
        }
        // Break before the last character unless it is complete, so no sequence is cut.
        std::size_t lead = pending_.size() - 1;
        for (int steps = 0; lead > 0 && steps < 3 && (Byte(pending_, lead) & 0xC0) == 0x80; ++steps) {
            --lead;
        }
        std::size_t cut = lead + AnnouncedLength(Byte(pending_, lead)) <= pending_.size() ? pending_.size() : lead;
        if (cut == 0) {
            cut = pending_.size();
        }
        lines.push_back(CleanLogLine(std::string_view(pending_).substr(0, cut)));
        pending_.erase(0, cut);
    }
}

void LogSplitter::Flush(std::vector<std::string>& lines) {
    if (!pending_.empty()) {
        lines.push_back(CleanLogLine(pending_));
        pending_.clear();
    }
}

void LogBuffer::Append(std::string line) {
    lines_.push_back(std::move(line));
    while (lines_.size() > capacity_) {
        lines_.pop_front();
    }
    ++version_;
}

void LogBuffer::Append(std::vector<std::string> lines) {
    for (std::string& line : lines) {
        Append(std::move(line));
    }
}

void LogBuffer::Clear() {
    lines_.clear();
    ++version_;
}

}  // namespace crisp::core
