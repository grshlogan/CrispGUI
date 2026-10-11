#pragma once

#include <string_view>
#include <vector>

namespace crisp::util {

enum class Language { English, Chinese };

// The UI string table. Every user-facing string has a stable key, such as "tab.dashboard",
// and an English and a Chinese text (docs/CONVENTIONS.md, UI strings).
class StringTable {
public:
    explicit StringTable(Language language = Language::English) : language_(language) {}

    void SetLanguage(Language language) { language_ = language; }
    Language GetLanguage() const { return language_; }

    // The text for |key| in the current language; the English text when the translation is
    // empty; |key| itself when the key is unknown, so a missing entry stays visible.
    std::string_view Get(std::string_view key) const;

    // Every key, in table order (for tests).
    static std::vector<std::string_view> Keys();
    // The stored text for |key| in |language|; empty when the key is unknown (for tests).
    static std::string_view Lookup(std::string_view key, Language language);

private:
    Language language_;
};

}  // namespace crisp::util
