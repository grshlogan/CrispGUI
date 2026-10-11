#include "util/string_table.h"

#include <algorithm>
#include <iterator>

namespace crisp::util {
namespace {

struct Entry {
    std::string_view key;
    std::string_view english;
    std::string_view chinese;
};

// Sorted by key, which the lookup relies on; string_table_test checks the order.
constexpr Entry kEntries[] = {
    {"app.title", "CrispRustGUI", "CrispRustGUI"},
    {"appdata.error", "The app data folder could not be prepared:", "无法准备应用数据文件夹："},
    {"appdata.error.action", "Move the program folder to a writable location outside C:\\ and restart.",
     "请把程序文件夹移到 C 盘以外的可写位置，然后重新启动。"},
    {"appdata.root", "App data: ", "应用数据："},
    {"appdata.system_drive", "The app data folder is on the system drive. Move the program folder off C:\\.",
     "应用数据文件夹位于系统盘。请把程序文件夹移出 C 盘。"},
    {"appdata.temp", "Temporary files: ", "临时文件："},
    {"language.chinese", "中文", "中文"},
    {"language.english", "English", "English"},
    {"page.batch.body", "The batch queue arrives in Phase 7.", "批量队列将在 Phase 7 加入。"},
    {"page.dashboard.body", "Server control and profiles arrive in Phase 3.", "服务器控制和配置文件将在 Phase 3 加入。"},
    {"page.settings.body", "More settings arrive in Phase 8.", "更多设置将在 Phase 8 加入。"},
    {"page.transcribe.body", "Single-file transcription arrives in Phase 5.", "单文件转写将在 Phase 5 加入。"},
    {"settings.language", "Language (not saved yet)", "界面语言（暂不保存）"},
    {"tab.batch", "Batch", "批量"},
    {"tab.dashboard", "Dashboard", "概览"},
    {"tab.settings", "Settings", "设置"},
    {"tab.transcribe", "Transcribe", "转写"},
};

const Entry* Find(std::string_view key) {
    const auto found = std::lower_bound(std::begin(kEntries), std::end(kEntries), key,
                                        [](const Entry& entry, std::string_view value) { return entry.key < value; });
    return found != std::end(kEntries) && found->key == key ? &*found : nullptr;
}

std::string_view TextFor(const Entry& entry, Language language) {
    return language == Language::Chinese ? entry.chinese : entry.english;
}

}  // namespace

std::string_view StringTable::Get(std::string_view key) const {
    const Entry* entry = Find(key);
    if (entry == nullptr) {
        return key;
    }
    const std::string_view text = TextFor(*entry, language_);
    return text.empty() ? entry->english : text;
}

std::vector<std::string_view> StringTable::Keys() {
    std::vector<std::string_view> keys;
    keys.reserve(std::size(kEntries));
    for (const Entry& entry : kEntries) {
        keys.push_back(entry.key);
    }
    return keys;
}

std::string_view StringTable::Lookup(std::string_view key, Language language) {
    const Entry* entry = Find(key);
    return entry != nullptr ? TextFor(*entry, language) : std::string_view();
}

}  // namespace crisp::util
