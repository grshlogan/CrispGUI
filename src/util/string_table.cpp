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
    {"app.title", "CrispGUI", "CrispGUI"},
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
    {"transcribe.actions.cancel", "Cancel", "取消"},
    {"transcribe.actions.choose", "Choose file…", "选择文件…"},
    {"transcribe.actions.retry", "Retry writing", "重试写入"},
    {"transcribe.actions.save_app", "Save to the app data folder", "保存到应用数据文件夹"},
    {"transcribe.actions.start", "Transcribe", "转写"},
    {"transcribe.confirm.cancel", "Cancel", "取消"},
    {"transcribe.confirm.message", "An SRT with this name exists next to the file.\nTranscribing replaces it.",
     "文件旁边已有同名 SRT。转写会替换它。"},
    {"transcribe.confirm.replace", "Replace", "替换"},
    {"transcribe.confirm.title", "Replace the existing SRT?", "替换已有的 SRT？"},
    {"transcribe.destination", "SRT: ", "SRT："},
    {"transcribe.destination.exists", "This SRT exists already. It is replaced only after you confirm.",
     "此 SRT 已存在，确认后才会被替换。"},
    {"transcribe.dialog.error", "The file dialog could not open: ", "无法打开文件对话框："},
    {"transcribe.dialog.filter", "Audio files", "音频文件"},
    {"transcribe.dialog.prompt", "Choose an audio file", "选择音频文件"},
    {"transcribe.elapsed", "Elapsed: ", "已用时间："},
    {"transcribe.fail.bad_request", "The server refused the file.", "服务端拒绝了此文件。"},
    {"transcribe.fail.bad_request.action", "Check that it is a supported audio file and not damaged.",
     "请确认它是受支持且未损坏的音频文件。"},
    {"transcribe.fail.cancelled", "The transcription was cancelled. Nothing was written.", "转写已取消，没有写入任何内容。"},
    {"transcribe.fail.cancelled.action", "Choose Transcribe to start again.", "点击“转写”可重新开始。"},
    {"transcribe.fail.destination_exists", "An SRT with this name exists next to the file.", "文件旁边已有同名 SRT。"},
    {"transcribe.fail.destination_exists.action",
     "Choose Transcribe again and confirm the replacement, or move the existing file away.",
     "请再次点击“转写”并确认替换，或把已有的文件移走。"},
    {"transcribe.fail.destination_exists.kept",
     "Move or rename the existing file, then retry, or save the finished SRT in the app data folder.",
     "请移走或重命名已有的文件后重试，或把已完成的 SRT 保存到应用数据文件夹。"},
    {"transcribe.fail.destination_not_file", "The SRT's name belongs to a folder.", "SRT 的名称被一个文件夹占用。"},
    {"transcribe.fail.destination_not_file.action", "Rename or remove that folder, then try again.",
     "请重命名或删除该文件夹，然后重试。"},
    {"transcribe.fail.destination_not_file.kept",
     "Rename or remove that folder, then retry, or save the finished SRT in the app data folder.",
     "请重命名或删除该文件夹后重试，或把已完成的 SRT 保存到应用数据文件夹。"},
    {"transcribe.fail.detail", "Details: ", "详情："},
    {"transcribe.fail.server_busy", "The server is busy, for example loading a model.", "服务端正忙，例如正在加载模型。"},
    {"transcribe.fail.server_busy.action", "Wait a moment and try again.", "请稍候再试。"},
    {"transcribe.fail.server_error", "The server reported an internal error, also on the second try.",
     "服务端报告了内部错误，重试一次后仍然如此。"},
    {"transcribe.fail.server_error.action", "The log on the Dashboard shows why.", "原因见“概览”页的日志。"},
    {"transcribe.fail.source_unreadable", "The file cannot be read.", "无法读取该文件。"},
    {"transcribe.fail.source_unreadable.action", "Check that it exists and that you may read it, then choose it again.",
     "请确认文件存在且有读取权限，然后重新选择。"},
    {"transcribe.fail.timeout", "The server did not answer in time.", "服务端未在规定时间内应答。"},
    {"transcribe.fail.timeout.action", "Try again, or check that the server is not overloaded.", "请重试，或检查服务端是否过载。"},
    {"transcribe.fail.too_large", "The file is larger than the server accepts (512 MB).", "文件超过了服务端接受的大小（512 MB）。"},
    {"transcribe.fail.too_large.action", "Cut or compress the file, then choose it again.", "请裁剪或压缩文件后重新选择。"},
    {"transcribe.fail.unauthorized", "The server rejected the API key.", "服务端拒绝了 API 密钥。"},
    {"transcribe.fail.unauthorized.action", "The server runs with another key. Restart it from the Dashboard.",
     "此服务端使用的是另一个密钥。请在“概览”页重新启动它。"},
    {"transcribe.fail.unexpected", "The server's answer could not be understood.", "无法理解服务端的应答。"},
    {"transcribe.fail.unexpected.action", "Check that the server is a CrispASR server of a supported version.",
     "请确认服务端是受支持版本的 CrispASR 服务端。"},
    {"transcribe.fail.unreachable.running", "The server does not answer.", "服务端没有应答。"},
    {"transcribe.fail.unreachable.running.action", "Check the Dashboard and the server log, then try again.",
     "请查看“概览”页和服务端日志，然后重试。"},
    {"transcribe.fail.unreachable.starting", "The server is still starting.", "服务端仍在启动。"},
    {"transcribe.fail.unreachable.starting.action", "Wait until the Dashboard shows Running, then try again.",
     "请等“概览”页显示“运行中”后再试。"},
    {"transcribe.fail.unreachable.stopped", "The server is not running.", "服务端没有运行。"},
    {"transcribe.fail.unreachable.stopped.action", "Start a server on the Dashboard, then try again.",
     "请先在“概览”页启动服务端，然后重试。"},
    {"transcribe.fail.write", "The SRT could not be written.", "无法写入 SRT。"},
    {"transcribe.fail.write.action", "Fix the cause, then retry, or save the finished SRT in the app data folder.",
     "请排除原因后重试，或把已完成的 SRT 保存到应用数据文件夹。"},
    {"transcribe.file", "File: ", "文件："},
    {"transcribe.file.none", "No file chosen. Formats: wav, mp3, flac, ogg, m4a, aac, opus, webm, wma.",
     "尚未选择文件。支持的格式：wav、mp3、flac、ogg、m4a、aac、opus、webm、wma。"},
    {"transcribe.file.size", "Size: ", "大小："},
    {"transcribe.file.title", "File", "文件"},
    {"transcribe.phase.cancelled", "Cancelled", "已取消"},
    {"transcribe.phase.done", "Done", "已完成"},
    {"transcribe.phase.failed", "Failed", "失败"},
    {"transcribe.phase.running", "Transcribing…", "正在转写…"},
    {"transcribe.phase.saving", "Writing the SRT…", "正在写入 SRT…"},
    {"transcribe.progress", "Progress: ", "进度："},
    {"transcribe.progress.unknown", "unknown", "未知"},
    {"transcribe.rescue", "Save target: ", "保存位置："},
    {"transcribe.result.cues", "Cues: ", "字幕条数："},
    {"transcribe.result.duration", "Audio length: ", "音频时长："},
    {"transcribe.result.elsewhere", "The SRT could not be written next to the file, so it was saved here instead.",
     "SRT 无法写在文件旁边，因此保存到了这里。"},
    {"transcribe.result.empty", "The transcript has no text, so the SRT is empty.", "转写结果没有文字，因此 SRT 为空。"},
    {"transcribe.result.kept",
     "The transcription is finished, but its SRT could not be written. It is kept until you start another one.",
     "转写已完成，但 SRT 无法写入。结果会保留，直到你开始下一次转写。"},
    {"transcribe.result.preview", "Preview", "预览"},
    {"transcribe.result.saved", "Saved to: ", "已保存到："},
    {"transcribe.retrying", "The server reported an error; trying once more.", "服务端报告了错误，正在重试一次。"},
    {"transcribe.server.none", "No server is running. Start one on the Dashboard.", "没有运行中的服务端。请先在“概览”页启动一个。"},
    {"transcribe.server.starting", "The server is starting. Wait until the Dashboard shows Running.",
     "服务端正在启动。请等待“概览”页显示“运行中”。"},
    {"transcribe.server.stopping", "The server is stopping.", "服务端正在停止。"},
    {"transcribe.status.title", "Status", "状态"},
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
