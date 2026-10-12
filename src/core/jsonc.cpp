#include "core/jsonc.h"

namespace crisp::core {

std::string StripJsonComments(std::string_view text) {
    std::string out;
    out.reserve(text.size());
    bool inString = false;
    bool escaped = false;
    bool inComment = false;
    for (std::size_t i = 0; i < text.size(); ++i) {
        const char c = text[i];
        if (c == '\n') {
            out.push_back(c);
            inString = false;
            escaped = false;
            inComment = false;
            continue;
        }
        if (inComment) {
            continue;
        }
        if (inString) {
            out.push_back(c);
            if (escaped) {
                escaped = false;
            } else if (c == '\\') {
                escaped = true;
            } else if (c == '"') {
                inString = false;
            }
            continue;
        }
        if (c == '"') {
            inString = true;
            out.push_back(c);
        } else if (c == '#' || (c == '/' && i + 1 < text.size() && text[i + 1] == '/')) {
            inComment = true;
        } else {
            out.push_back(c);
        }
    }
    return out;
}

util::JsonParseResult ParseJsonc(std::string_view text) {
    return util::ParseJson(StripJsonComments(text));
}

}  // namespace crisp::core
