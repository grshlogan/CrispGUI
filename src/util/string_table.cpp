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
    {"page.dashboard.body", "Profiles are read from the CrispASR folder and never changed. Server control arrives in Phase 3.",
     "配置文件从 CrispASR 文件夹读取，不会被修改。服务器控制将在 Phase 3 加入。"},
    {"page.settings.body", "More settings arrive in Phase 8.", "更多设置将在 Phase 8 加入。"},
    {"page.transcribe.body", "Single-file transcription arrives in Phase 5.", "单文件转写将在 Phase 5 加入。"},
    {"profiles.aligner_missing", "Aligner missing: ", "对齐模型缺失："},
    {"profiles.count", "Profiles: ", "配置文件："},
    {"profiles.download", "Download from: ", "下载地址："},
    {"profiles.empty", "No profile folders. Profiles must live in a subfolder, such as profile\\Japanese.",
     "没有配置文件夹。配置文件必须放在子文件夹里，例如 profile\\Japanese。"},
    {"profiles.error", "Cannot use this profile: ", "无法使用此配置文件："},
    {"profiles.executable_missing", "Server program missing: ", "服务端程序缺失："},
    {"profiles.folder_empty", "No .json or .conf profiles in this folder.", "此文件夹里没有 .json 或 .conf 配置文件。"},
    {"profiles.folder_error", "This folder could not be read: ", "无法读取此文件夹："},
    {"profiles.model_missing", "Model missing: ", "模型文件缺失："},
    {"profiles.ok_count", "OK: ", "可用："},
    {"profiles.rescan", "Rescan", "重新扫描"},
    {"profiles.root", "CrispASR folder: ", "CrispASR 文件夹："},
    {"profiles.root_unset", "No CrispASR folder is set. Development runs read it from CRISPGUI_CRISPASR_ROOT.",
     "尚未设置 CrispASR 文件夹。开发运行时从环境变量 CRISPGUI_CRISPASR_ROOT 读取。"},
    {"profiles.scan_error", "The profiles could not be read: ", "无法读取配置文件："},
    {"profiles.scanning", "Reading profiles…", "正在读取配置文件…"},
    {"profiles.startup", "Startup flags: ", "启动参数："},
    {"profiles.status.error", "[ERROR]", "[ERROR]"},
    {"profiles.status.miss", "[MISS]", "[MISS]"},
    {"profiles.status.ok", "[OK]", "[OK]"},
    {"profiles.title", "Profiles", "配置文件"},
    {"profiles.warning", "Note: ", "注意："},
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
