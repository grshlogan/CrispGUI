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
    {"page.dashboard.body", "Profiles are read from the CrispASR folder and never changed. Start the server from a profile below.",
     "配置文件从 CrispASR 文件夹读取，不会被修改。可在下方选择一个配置文件启动服务端。"},
    {"page.settings.body", "More settings arrive in Phase 8.", "更多设置将在 Phase 8 加入。"},
    {"page.transcribe.body", "Single-file transcription arrives in Phase 5.", "单文件转写将在 Phase 5 加入。"},
    {"profiles.active", "In use", "使用中"},
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
    {"profiles.start", "Start", "启动"},
    {"profiles.startup", "Startup flags: ", "启动参数："},
    {"profiles.status.error", "[ERROR]", "[ERROR]"},
    {"profiles.status.miss", "[MISS]", "[MISS]"},
    {"profiles.status.ok", "[OK]", "[OK]"},
    {"profiles.title", "Profiles", "配置文件"},
    {"profiles.warning", "Note: ", "注意："},
    {"server.address", "Address: ", "地址："},
    {"server.adopted", "Started outside this GUI session; its profile is unknown.", "此服务端不是由本次 GUI 启动的，配置文件未知。"},
    {"server.backend", "Backend: ", "后端："},
    {"server.confirm.cancel", "Cancel", "取消"},
    {"server.confirm.message",
     "This server was not started by this GUI session and may be in use elsewhere, for example through the launcher. "
     "Stopping it ends every request in progress.",
     "此服务端不是由本次 GUI 启动的，可能正在别处使用（例如通过启动脚本）。停止它会中断所有进行中的请求。"},
    {"server.confirm.stop", "Stop server", "停止服务端"},
    {"server.confirm.title", "Stop the adopted server?", "停止被接管的服务端？"},
    {"server.detail", "Details: ", "详情："},
    {"server.hint", "Start a server with the Start button of a profile below.", "点击下方配置文件的“启动”按钮即可启动服务端。"},
    {"server.log.clear", "Clear", "清空"},
    {"server.log.count", "Lines: ", "行数："},
    {"server.log.empty",
     "No output yet. The log shows the output of a server this GUI starts; an adopted server's output is not available.",
     "暂无输出。这里显示由本 GUI 启动的服务端的输出；被接管的服务端的输出无法获取。"},
    {"server.log.title", "Server log", "服务端日志"},
    {"server.model", "Model: ", "模型："},
    {"server.models_error", "Model details unavailable: ", "无法读取模型信息："},
    {"server.msg.adopted_at_launch", "A CrispASR server was already running when the GUI started; it is shown as adopted.",
     "GUI 启动时已有 CrispASR 服务端在运行，已接管显示。"},
    {"server.msg.adopted_instead",
     "A CrispASR server already answers on this port, so no second one was started; it is shown as adopted.",
     "此端口上已有 CrispASR 服务端在应答，因此没有再启动一个，已接管显示。"},
    {"server.msg.cannot_start", "The server cannot start.", "无法启动服务端。"},
    {"server.msg.exited", "The server exited unexpectedly. The log below shows why.", "服务端意外退出。原因见下方日志。"},
    {"server.msg.exited_while_starting",
     "The server exited before it answered. The log below shows why; a port that another program took also ends this way.",
     "服务端在应答之前就退出了。原因见下方日志；端口被其他程序占用时也会这样。"},
    {"server.msg.found_running", "A CrispASR server started outside the GUI now answers on the port; it is shown as adopted.",
     "检测到在 GUI 之外启动的 CrispASR 服务端，已接管显示。"},
    {"server.msg.not_responding", "The server process runs, but the server does not answer.", "服务端进程在运行，但没有应答。"},
    {"server.msg.port_busy", "Another program holds the port, so no server was started.", "端口被其他程序占用，因此没有启动服务端。"},
    {"server.msg.spawn_failed", "The server process could not be created.", "无法创建服务端进程。"},
    {"server.msg.stop_failed", "The server could not be stopped.", "无法停止服务端。"},
    {"server.msg.unreachable", "The adopted server does not answer.", "被接管的服务端没有应答。"},
    {"server.pid", "Process: ", "进程："},
    {"server.port.foreign", "Another program answers on the port: ", "端口被其他程序占用："},
    {"server.port.free", "The port is free.", "端口空闲。"},
    {"server.profile", "Profile: ", "配置文件："},
    {"server.state.checking", "Looking for a running server…", "正在查找运行中的服务端…"},
    {"server.state.failed", "Failed", "失败"},
    {"server.state.preflight", "Checking the port…", "正在检查端口…"},
    {"server.state.running", "Running", "运行中"},
    {"server.state.starting", "Starting: loading the model…", "正在启动：加载模型…"},
    {"server.state.stopped", "Stopped", "已停止"},
    {"server.state.stopping", "Stopping…", "正在停止…"},
    {"server.state.unreachable", "Not answering", "无应答"},
    {"server.stop", "Stop", "停止"},
    {"server.title", "Server", "服务端"},
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
