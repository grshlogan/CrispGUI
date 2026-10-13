#include "ui/pages.h"

#include "core/launch_command.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <string>
#include <utility>

namespace crisp::ui {
namespace {

constexpr float kTitleSize = 24.0f;
constexpr float kHeadingSize = 18.0f;
constexpr float kBodySize = 15.0f;
constexpr float kLineHeight = 22.0f;
constexpr float kDetailSize = 13.0f;
constexpr float kDetailLineHeight = 19.0f;
constexpr float kSectionGap = 12.0f;
constexpr float kStatusWidth = 72.0f;
constexpr float kDetailIndent = kStatusWidth + 10.0f;
constexpr float kLogRowHeight = 18.0f;
constexpr float kLogFontSize = 12.0f;
constexpr float kLogHeaderHeight = 34.0f;
constexpr float kLogGap = 6.0f;

std::string Text(const PageContext& context, std::string_view key) {
    return std::string(context.strings.Get(key));
}

eui::Color Muted(const PageContext& context) {
    return components::theme::withOpacity(context.theme.text, 0.64f);
}

eui::Color Good() {
    return components::theme::color(0.42f, 0.80f, 0.47f, 1.0f);
}

eui::Color Warning() {
    return components::theme::color(0.96f, 0.69f, 0.25f, 1.0f);
}

eui::Color Bad() {
    return components::theme::color(0.95f, 0.42f, 0.40f, 1.0f);
}

// EUI-NEO's buttons look the same when disabled, so disabled ones are dimmed.
float ButtonOpacity(bool disabled) {
    return disabled ? 0.4f : 1.0f;
}

void Title(eui::Ui& ui, const PageContext& context, const std::string& id, std::string_view key) {
    auto title = components::text(ui, id, context.theme).text(Text(context, key)).fontSize(kTitleSize);
    if (!context.fonts.title.empty()) {
        title.fontFamily(context.fonts.title);
    }
    title.build();
}

void Heading(eui::Ui& ui, const PageContext& context, const std::string& id, const std::string& text) {
    auto heading = components::text(ui, id, context.theme).text(text).fontSize(kHeadingSize).lineHeight(26.0f);
    if (!context.fonts.medium.empty()) {
        heading.fontFamily(context.fonts.medium);
    }
    heading.build();
}

void Line(eui::Ui& ui, const PageContext& context, float width, const std::string& id, const std::string& text,
          const eui::Color& color) {
    components::text(ui, id, context.theme)
        .width(width)
        .maxWidth(width)  // the wrap width EUI-NEO measures the height with
        .text(text)
        .fontSize(kBodySize)
        .lineHeight(kLineHeight)
        .wrap(true)
        .color(color)
        .build();
}

void Detail(eui::Ui& ui, const PageContext& context, float width, const std::string& id, const std::string& text,
            const eui::Color& color) {
    components::text(ui, id, context.theme)
        .width(width)
        .maxWidth(width)  // the wrap width EUI-NEO measures the height with
        .text(text)
        .fontSize(kDetailSize)
        .lineHeight(kDetailLineHeight)
        .wrap(true)
        .color(color)
        .build();
}

void ComposeAppData(eui::Ui& ui, const PageContext& context, float width, const app::AppDataResult& appData) {
    if (!appData.Ok()) {
        Line(ui, context, width, "dashboard.appdata.error", Text(context, "appdata.error") + " " + appData.error,
             Warning());
        Line(ui, context, width, "dashboard.appdata.action", Text(context, "appdata.error.action"), Warning());
        return;
    }
    Line(ui, context, width, "dashboard.appdata.root", Text(context, "appdata.root") + appData.layout.root.u8string(),
         context.theme.text);
    Line(ui, context, width, "dashboard.appdata.temp", Text(context, "appdata.temp") + appData.layout.temp.u8string(),
         context.theme.text);
    if (appData.onSystemDrive) {
        Line(ui, context, width, "dashboard.appdata.system_drive", Text(context, "appdata.system_drive"), Warning());
    }
}

std::string_view StateKey(const core::ServerStatus& server) {
    switch (server.state) {
    case core::ServerState::kStopped:
        return "server.state.stopped";
    case core::ServerState::kChecking:
        return server.pendingStart ? "server.state.preflight" : "server.state.checking";
    case core::ServerState::kStarting:
        return "server.state.starting";
    case core::ServerState::kRunning:
        return "server.state.running";
    case core::ServerState::kUnreachable:
        return "server.state.unreachable";
    case core::ServerState::kStopping:
        return "server.state.stopping";
    case core::ServerState::kFailed:
        return "server.state.failed";
    }
    return "server.state.stopped";
}

eui::Color StateColor(const PageContext& context, const core::ServerStatus& server) {
    switch (server.state) {
    case core::ServerState::kRunning:
        return Good();
    case core::ServerState::kStarting:
    case core::ServerState::kUnreachable:
    case core::ServerState::kStopping:
        return Warning();
    case core::ServerState::kFailed:
        return Bad();
    default:
        return Muted(context);
    }
}

// The string table key of a status message; empty for none.
std::string_view MessageKey(core::ServerMessage message) {
    switch (message) {
    case core::ServerMessage::kNone:
        return {};
    case core::ServerMessage::kAdoptedAtLaunch:
        return "server.msg.adopted_at_launch";
    case core::ServerMessage::kAdoptedInsteadOfStart:
        return "server.msg.adopted_instead";
    case core::ServerMessage::kFoundRunning:
        return "server.msg.found_running";
    case core::ServerMessage::kPortBusy:
        return "server.msg.port_busy";
    case core::ServerMessage::kCannotStart:
        return "server.msg.cannot_start";
    case core::ServerMessage::kSpawnFailed:
        return "server.msg.spawn_failed";
    case core::ServerMessage::kExitedWhileStarting:
        return "server.msg.exited_while_starting";
    case core::ServerMessage::kExited:
        return "server.msg.exited";
    case core::ServerMessage::kStopFailed:
        return "server.msg.stop_failed";
    case core::ServerMessage::kNotResponding:
        return "server.msg.not_responding";
    case core::ServerMessage::kUnreachable:
        return "server.msg.unreachable";
    }
    return {};
}

eui::Color MessageColor(const PageContext& context, core::ServerMessage message) {
    switch (message) {
    case core::ServerMessage::kAdoptedAtLaunch:
    case core::ServerMessage::kAdoptedInsteadOfStart:
    case core::ServerMessage::kFoundRunning:
        return context.theme.text;
    case core::ServerMessage::kNotResponding:
    case core::ServerMessage::kUnreachable:
        return Warning();
    default:
        return Bad();
    }
}

// The server: its state, where it runs, what it serves, and what went wrong.
void ComposeServer(eui::Ui& ui, const PageContext& context, float width, const DashboardState& state) {
    const core::ServerStatus& server = state.server;
    ui.row("server.header").width(width).gap(16.0f).alignItems(eui::Align::CENTER).content([&] {
        Heading(ui, context, "server.title", Text(context, "server.title"));
        components::text(ui, "server.state", context.theme)
            .text(Text(context, StateKey(server)))
            .fontSize(kBodySize)
            .color(StateColor(context, server))
            .build();
        components::button(ui, "server.stop")
            .size(120.0f, 34.0f)
            .text(Text(context, "server.stop"))
            .fontSize(14.0f)
            .theme(context.theme, false)
            .disabled(!server.CanStop())
            .opacity(ButtonOpacity(!server.CanStop()))
            .onClick(state.onStopServer)
            .build();
    }).build();

    if (server.adopted) {
        Line(ui, context, width, "server.profile", Text(context, "server.adopted"), Muted(context));
    } else if (!server.profileName.empty()) {
        Line(ui, context, width, "server.profile",
             Text(context, "server.profile") + server.profileFolder + " / " + server.profileName, context.theme.text);
    }
    std::string address = Text(context, "server.address") + server.endpoint.Url();
    if (server.pid != 0) {
        address += "    " + Text(context, "server.pid") + std::to_string(server.pid);
    }
    Line(ui, context, width, "server.address", address, context.theme.text);
    if (!server.backend.empty() || !server.model.empty()) {
        std::string serving;
        if (!server.backend.empty()) {
            serving = Text(context, "server.backend") + server.backend;
        }
        if (!server.model.empty()) {
            serving += (serving.empty() ? "" : "    ") + Text(context, "server.model") + server.model;
        }
        Line(ui, context, width, "server.serving", serving, context.theme.text);
    }
    if (!server.modelsDetail.empty()) {
        Line(ui, context, width, "server.models_error", Text(context, "server.models_error") + server.modelsDetail,
             Muted(context));
    }
    for (std::size_t i = 0; i < server.warnings.size(); ++i) {
        Line(ui, context, width, "server.warning" + std::to_string(i),
             Text(context, "profiles.warning") + server.warnings[i], Warning());
    }
    const std::string_view messageKey = MessageKey(server.message);
    if (!messageKey.empty()) {
        Line(ui, context, width, "server.message", Text(context, messageKey), MessageColor(context, server.message));
        if (!server.detail.empty()) {
            Line(ui, context, width, "server.detail", Text(context, "server.detail") + server.detail, Muted(context));
        }
    }
    const bool idle = server.state == core::ServerState::kStopped || server.state == core::ServerState::kFailed;
    if (idle && server.healthKnown) {
        Line(ui, context, width, "server.port",
             server.health == core::HealthKind::kForeign
                 ? Text(context, "server.port.foreign") + server.healthDetail
                 : Text(context, "server.port.free"),
             Muted(context));
    }
    if (idle) {
        Line(ui, context, width, "server.hint", Text(context, "server.hint"), Muted(context));
    }
}

// What is missing or wrong with a profile, and the startup flags the launcher would pass.
void ComposeProfileDetails(eui::Ui& ui, const PageContext& context, float width, const std::string& id,
                           const core::Profile& profile) {
    if (!profile.Usable()) {
        Detail(ui, context, width, id + ".error", Text(context, "profiles.error") + profile.error, Bad());
        return;
    }
    if (!profile.modelOk) {
        Detail(ui, context, width, id + ".model", Text(context, "profiles.model_missing") + profile.modelPath.u8string(),
               Warning());
        if (!profile.download.empty()) {
            Detail(ui, context, width, id + ".download", Text(context, "profiles.download") + profile.download,
                   Muted(context));
        }
    }
    if (!profile.executableOk) {
        Detail(ui, context, width, id + ".executable",
               Text(context, "profiles.executable_missing") + profile.executablePath.u8string(), Warning());
    }
    if (profile.HasAligner() && !profile.alignerOk) {
        Detail(ui, context, width, id + ".aligner",
               Text(context, "profiles.aligner_missing") + profile.alignerPath.u8string(), Warning());
    }
    Detail(ui, context, width, id + ".startup",
           Text(context, "profiles.startup") + core::ReadProfileStartup(profile).Text(), Muted(context));
    for (std::size_t i = 0; i < profile.warnings.size(); ++i) {
        Detail(ui, context, width, id + ".warning" + std::to_string(i),
               Text(context, "profiles.warning") + profile.warnings[i], Warning());
    }
}

// One profile: a status line like the launcher's menu entry with a Start button, and the details
// below it.
void ComposeProfile(eui::Ui& ui, const PageContext& context, float width, const std::string& id,
                    const core::Profile& profile, const DashboardState& state) {
    std::string_view statusKey = "profiles.status.ok";
    eui::Color statusColor = Good();
    if (!profile.Usable()) {
        statusKey = "profiles.status.error";
        statusColor = Bad();
    } else if (!profile.modelOk) {
        statusKey = "profiles.status.miss";
        statusColor = Warning();
    }

    ui.column(id).width(width).gap(4.0f).content([&] {
        ui.row(id + ".head").width(width).gap(10.0f).content([&] {
            components::text(ui, id + ".status", context.theme)
                .width(kStatusWidth)
                .text(Text(context, statusKey))
                .fontSize(kBodySize)
                .color(statusColor)
                .build();
            components::text(ui, id + ".name", context.theme).text(profile.name).fontSize(kBodySize).build();
            if (profile.Usable()) {
                components::text(ui, id + ".backend", context.theme)
                    .text("<" + profile.backend + ">")
                    .fontSize(kBodySize)
                    .color(Muted(context))
                    .build();
            }
            const core::ServerStatus& server = state.server;
            const bool startable = profile.Usable() && profile.executableOk && profile.modelOk && server.CanStart();
            components::button(ui, id + ".start")
                .size(84.0f, 28.0f)
                .text(Text(context, "profiles.start"))
                .fontSize(13.0f)
                .theme(context.theme, false)
                .disabled(!startable)
                .opacity(ButtonOpacity(!startable))
                .onClick([onStart = state.onStartProfile, path = profile.path] {
                    if (onStart) {
                        onStart(path);
                    }
                })
                .build();
            const bool inUse = !server.adopted && server.state != core::ServerState::kStopped &&
                               server.state != core::ServerState::kFailed && server.profileFolder == profile.folder &&
                               server.profileName == profile.name;
            if (inUse) {
                components::text(ui, id + ".in_use", context.theme)
                    .text(Text(context, "profiles.active"))
                    .fontSize(kBodySize)
                    .color(StateColor(context, server))
                    .build();
            }
        }).build();
        ui.column(id + ".details").width(width).padding(kDetailIndent, 0.0f, 0.0f, 0.0f).gap(2.0f).content([&] {
            ComposeProfileDetails(ui, context, std::max(0.0f, width - kDetailIndent), id, profile);
        }).build();
    }).build();
}

void ComposeProfiles(eui::Ui& ui, const PageContext& context, float width, const DashboardState& state) {
    ui.row("profiles.header").width(width).gap(16.0f).alignItems(eui::Align::CENTER).content([&] {
        Heading(ui, context, "profiles.title", Text(context, "profiles.title"));
        components::button(ui, "profiles.rescan")
            .size(120.0f, 34.0f)
            .text(Text(context, "profiles.rescan"))
            .fontSize(14.0f)
            .theme(context.theme, false)
            .disabled(state.scanning || state.crispAsrRoot.empty())
            .opacity(ButtonOpacity(state.scanning || state.crispAsrRoot.empty()))
            .onClick(state.onRescan)
            .build();
    }).build();

    if (state.crispAsrRoot.empty()) {
        Line(ui, context, width, "profiles.root_unset", Text(context, "profiles.root_unset"), Warning());
        return;
    }
    Line(ui, context, width, "profiles.root", Text(context, "profiles.root") + state.crispAsrRoot.u8string(),
         context.theme.text);
    if (state.scanning || !state.scanned) {
        Line(ui, context, width, "profiles.scanning", Text(context, "profiles.scanning"), Muted(context));
        if (!state.scanned) {
            return;
        }
    }

    const core::ProfileScan& scan = state.profiles;
    if (!scan.Ok()) {
        Line(ui, context, width, "profiles.scan_error", Text(context, "profiles.scan_error") + scan.error, Warning());
        return;
    }
    if (scan.folders.empty()) {
        Line(ui, context, width, "profiles.empty", Text(context, "profiles.empty"), Warning());
        return;
    }

    std::size_t ready = 0;
    for (const core::ProfileFolder& folder : scan.folders) {
        for (const core::Profile& profile : folder.profiles) {
            ready += profile.Usable() && profile.modelOk ? 1 : 0;
        }
    }
    Line(ui, context, width, "profiles.summary",
         Text(context, "profiles.count") + std::to_string(scan.ProfileCount()) + "    " +
             Text(context, "profiles.ok_count") + std::to_string(ready),
         Muted(context));

    for (std::size_t f = 0; f < scan.folders.size(); ++f) {
        const core::ProfileFolder& folder = scan.folders[f];
        const std::string folderId = "profiles.f" + std::to_string(f);
        Heading(ui, context, folderId, folder.name);
        if (!folder.error.empty()) {
            Line(ui, context, width, folderId + ".error", Text(context, "profiles.folder_error") + folder.error,
                 Warning());
        }
        if (folder.profiles.empty()) {
            Line(ui, context, width, folderId + ".empty", Text(context, "profiles.folder_empty"), Muted(context));
        }
        for (std::size_t p = 0; p < folder.profiles.size(); ++p) {
            ComposeProfile(ui, context, width, folderId + ".p" + std::to_string(p), folder.profiles[p], state);
        }
    }
}

// The server's output, newest at the bottom; the view follows new lines while it is scrolled to
// the end.
void ComposeServerLog(eui::Ui& ui, const PageContext& context, float width, float height, const DashboardState& state) {
    const core::LogBuffer& log = state.log;
    const float listHeight = std::max(0.0f, height - kLogHeaderHeight - kLogGap);
    ui.column("log").size(width, height).gap(kLogGap).content([&] {
        ui.row("log.header").size(width, kLogHeaderHeight).gap(16.0f).alignItems(eui::Align::CENTER).content([&] {
            Heading(ui, context, "log.title", Text(context, "server.log.title"));
            components::text(ui, "log.count", context.theme)
                .text(Text(context, "server.log.count") + std::to_string(log.Size()))
                .fontSize(kDetailSize)
                .color(Muted(context))
                .build();
            components::button(ui, "log.clear")
                .size(90.0f, 28.0f)
                .text(Text(context, "server.log.clear"))
                .fontSize(13.0f)
                .theme(context.theme, false)
                .disabled(log.Size() == 0)
                .opacity(ButtonOpacity(log.Size() == 0))
                .onClick(state.onClearLog)
                .build();
        }).build();

        if (log.Size() == 0) {
            Detail(ui, context, width, "log.empty", Text(context, "server.log.empty"), Muted(context));
            return;
        }
        const float maxOffset = std::max(0.0f, static_cast<float>(log.Size()) * kLogRowHeight - listHeight);
        const float offset = state.logFollow ? maxOffset : std::min(state.logOffset, maxOffset);
        const eui::Color noteColor = context.theme.primary;
        const eui::Color lineColor = components::theme::withOpacity(context.theme.text, 0.82f);
        components::virtualList(ui, "log.lines")
            .size(width, listHeight)
            .itemCount(static_cast<std::int64_t>(log.Size()))
            .rowHeight(kLogRowHeight)
            .offset(offset)
            .theme(context.theme)
            .onChange([onLogScroll = state.onLogScroll, maxOffset](float value) {
                if (onLogScroll) {
                    onLogScroll(value, value >= maxOffset - 1.0f);
                }
            })
            .row([&log, noteColor, lineColor](eui::Ui& row, const std::string& rowId, std::int64_t index, float rowWidth,
                                               float rowHeight) {
                const std::string& line = log.At(static_cast<std::size_t>(index));
                const bool note = line.rfind(core::kNotePrefix, 0) == 0;
                row.text(rowId + ".text")
                    .size(rowWidth, rowHeight)
                    .text(line)
                    .fontSize(kLogFontSize)
                    .lineHeight(rowHeight)
                    .color(note ? noteColor : lineColor)
                    .verticalAlign(::core::VerticalAlign::Center)
                    .build();
            })
            .build();
    }).build();
}

// The height of the log pane. Until the server has written a line it needs only its header and the
// text that says why it is empty (two lines, for a narrow window), which leaves the room to the
// profiles; with lines it keeps a fixed part of the page.
float ServerLogHeight(const PageContext& context, const core::LogBuffer& log) {
    if (log.Size() == 0) {
        return kLogHeaderHeight + kLogGap + 2.0f * kDetailLineHeight;
    }
    return std::clamp(context.height * 0.32f, 140.0f, 320.0f);
}

// "512 B", "12.3 KB", "12.3 MB", or "1.20 GB" for a size in bytes, counted in steps of 1024.
std::string FormatSize(std::uint64_t bytes) {
    char text[32];
    const double value = static_cast<double>(bytes);
    if (bytes < 1024ull) {
        std::snprintf(text, sizeof(text), "%llu B", static_cast<unsigned long long>(bytes));
    } else if (bytes < 1024ull * 1024) {
        std::snprintf(text, sizeof(text), "%.1f KB", value / 1024.0);
    } else if (bytes < 1024ull * 1024 * 1024) {
        std::snprintf(text, sizeof(text), "%.1f MB", value / (1024.0 * 1024.0));
    } else {
        std::snprintf(text, sizeof(text), "%.2f GB", value / (1024.0 * 1024.0 * 1024.0));
    }
    return text;
}

// "1:05", or "1:02:03" from one hour on, for a time in milliseconds.
std::string FormatClock(std::int64_t milliseconds) {
    const long long seconds = std::max<std::int64_t>(0, milliseconds) / 1000;
    char text[32];
    if (seconds >= 3600) {
        std::snprintf(text, sizeof(text), "%lld:%02lld:%02lld", seconds / 3600, seconds / 60 % 60, seconds % 60);
    } else {
        std::snprintf(text, sizeof(text), "%lld:%02lld", seconds / 60, seconds % 60);
    }
    return text;
}

bool Working(const core::TranscriptionState& job) {
    return job.phase == core::TranscriptionPhase::kRunning || job.phase == core::TranscriptionPhase::kSaving;
}

// The string table key of the message for a failure; the key plus ".action" says what to do (plus
// ".kept" when a kept SRT is in the way). A server that cannot be reached is not running, still
// starting, or silent, which its state tells.
std::string FailureKey(core::TranscriptionFailure failure, const core::ServerStatus& server) {
    switch (failure) {
    case core::TranscriptionFailure::kNone:
        return {};
    case core::TranscriptionFailure::kServerUnreachable:
        switch (server.state) {
        case core::ServerState::kRunning:
        case core::ServerState::kUnreachable:
            return "transcribe.fail.unreachable.running";
        case core::ServerState::kChecking:
        case core::ServerState::kStarting:
            return "transcribe.fail.unreachable.starting";
        default:
            return "transcribe.fail.unreachable.stopped";
        }
    case core::TranscriptionFailure::kTimeout:
        return "transcribe.fail.timeout";
    case core::TranscriptionFailure::kCancelled:
        return "transcribe.fail.cancelled";
    case core::TranscriptionFailure::kBadRequest:
        return "transcribe.fail.bad_request";
    case core::TranscriptionFailure::kUnauthorized:
        return "transcribe.fail.unauthorized";
    case core::TranscriptionFailure::kTooLarge:
        return "transcribe.fail.too_large";
    case core::TranscriptionFailure::kServerBusy:
        return "transcribe.fail.server_busy";
    case core::TranscriptionFailure::kServerError:
        return "transcribe.fail.server_error";
    case core::TranscriptionFailure::kUnexpectedAnswer:
        return "transcribe.fail.unexpected";
    case core::TranscriptionFailure::kSourceUnreadable:
        return "transcribe.fail.source_unreadable";
    case core::TranscriptionFailure::kDestinationExists:
        return "transcribe.fail.destination_exists";
    case core::TranscriptionFailure::kDestinationNotFile:
        return "transcribe.fail.destination_not_file";
    case core::TranscriptionFailure::kCannotWrite:
        return "transcribe.fail.write";
    }
    return {};
}

// A button that is dimmed when it cannot be used.
void ActionButton(eui::Ui& ui, const PageContext& context, const std::string& id, std::string_view textKey, float width,
                  bool primary, bool enabled, const std::function<void()>& onClick) {
    components::button(ui, id)
        .size(width, 34.0f)
        .text(Text(context, textKey))
        .fontSize(14.0f)
        .theme(context.theme, primary)
        .disabled(!enabled)
        .opacity(ButtonOpacity(!enabled))
        .onClick(onClick)
        .build();
}

// Which server there is: running with its profile and backend, or what to do about it.
void ComposeTranscribeServer(eui::Ui& ui, const PageContext& context, float width, const TranscribeState& state) {
    const core::ServerStatus& server = state.server;
    const bool running = server.state == core::ServerState::kRunning;
    ui.row("transcribe.server.header").width(width).gap(16.0f).alignItems(eui::Align::CENTER).content([&] {
        Heading(ui, context, "transcribe.server.title", Text(context, "server.title"));
        if (running) {
            components::text(ui, "transcribe.server.state", context.theme)
                .text(Text(context, "server.state.running"))
                .fontSize(kBodySize)
                .color(Good())
                .build();
        }
    }).build();
    if (!running) {
        const bool starting =
            server.state == core::ServerState::kChecking || server.state == core::ServerState::kStarting;
        const bool stopping = server.state == core::ServerState::kStopping;
        Line(ui, context, width, "transcribe.server.message",
             Text(context, starting ? "transcribe.server.starting"
                                    : (stopping ? "transcribe.server.stopping" : "transcribe.server.none")),
             Warning());
        return;
    }
    if (server.adopted) {
        Line(ui, context, width, "transcribe.server.profile", Text(context, "server.adopted"), Muted(context));
    } else if (!server.profileName.empty()) {
        Line(ui, context, width, "transcribe.server.profile",
             Text(context, "server.profile") + server.profileFolder + " / " + server.profileName, context.theme.text);
    }
    if (!server.backend.empty()) {
        Line(ui, context, width, "transcribe.server.backend", Text(context, "server.backend") + server.backend,
             context.theme.text);
    }
}

// The chosen file with its size, where its SRT goes, and why the file dialog did not open.
void ComposeTranscribeFile(eui::Ui& ui, const PageContext& context, float width, const TranscribeState& state) {
    const bool working = Working(state.transcription);
    ui.row("transcribe.file.header").width(width).gap(16.0f).alignItems(eui::Align::CENTER).content([&] {
        Heading(ui, context, "transcribe.file.title", Text(context, "transcribe.file.title"));
        ActionButton(ui, context, "transcribe.choose", "transcribe.actions.choose", 140.0f, false, !working,
                     state.onChooseFile);
    }).build();
    if (!state.dialogError.empty()) {
        Line(ui, context, width, "transcribe.dialog_error",
             Text(context, "transcribe.dialog.error") + state.dialogError, Warning());
    }
    if (state.file.empty()) {
        Line(ui, context, width, "transcribe.file.none", Text(context, "transcribe.file.none"), Muted(context));
        return;
    }
    std::string file = Text(context, "transcribe.file") + state.file.u8string();
    if (state.fileSizeKnown) {
        file += "    " + Text(context, "transcribe.file.size") + FormatSize(state.fileSize);
    }
    Line(ui, context, width, "transcribe.file.path", file, context.theme.text);
    Line(ui, context, width, "transcribe.file.destination",
         Text(context, "transcribe.destination") + state.destination.u8string(), context.theme.text);
    // The SRT that a finished transcription just wrote is its result, not something in the way.
    const core::TranscriptionState& job = state.transcription;
    const bool isResult = job.phase == core::TranscriptionPhase::kDone && job.destination == state.destination;
    if (state.destinationExists && !isResult) {
        Line(ui, context, width, "transcribe.file.exists", Text(context, "transcribe.destination.exists"), Warning());
    }
}

// The cues and the length of the transcript, and the first cues.
void ComposeTranscribeFacts(eui::Ui& ui, const PageContext& context, float width, const core::TranscriptionState& job) {
    if (job.cues == 0) {
        Line(ui, context, width, "transcribe.result.empty", Text(context, "transcribe.result.empty"), Muted(context));
        return;
    }
    Line(ui, context, width, "transcribe.result.facts",
         Text(context, "transcribe.result.cues") + std::to_string(job.cues) + "    " +
             Text(context, "transcribe.result.duration") + FormatClock(job.durationMs),
         context.theme.text);
    Heading(ui, context, "transcribe.result.preview.title", Text(context, "transcribe.result.preview"));
    for (std::size_t i = 0; i < job.preview.size(); ++i) {
        Detail(ui, context, width, "transcribe.result.preview." + std::to_string(i), job.preview[i], Muted(context));
    }
}

// The failure with what to do about it; a finished transcription whose SRT was not written is
// kept, with the two ways to write it.
void ComposeTranscribeFailure(eui::Ui& ui, const PageContext& context, float width, const TranscribeState& state) {
    const core::TranscriptionState& job = state.transcription;
    const std::string key = FailureKey(job.failure, state.server);
    // With a kept SRT, an SRT in the way is dealt with before the retry, not by transcribing again.
    const bool keptInTheWay = job.srtKept && (job.failure == core::TranscriptionFailure::kDestinationExists ||
                                              job.failure == core::TranscriptionFailure::kDestinationNotFile);
    const bool cancelled = job.failure == core::TranscriptionFailure::kCancelled;
    Line(ui, context, width, "transcribe.fail.message", Text(context, key), cancelled ? Warning() : Bad());
    Line(ui, context, width, "transcribe.fail.action", Text(context, key + (keptInTheWay ? ".kept" : ".action")),
         Muted(context));
    if (!job.detail.empty()) {
        Detail(ui, context, width, "transcribe.fail.detail", Text(context, "transcribe.fail.detail") + job.detail,
               Muted(context));
    }
    if (!job.srtKept) {
        return;
    }
    Line(ui, context, width, "transcribe.kept", Text(context, "transcribe.result.kept"), Warning());
    ui.row("transcribe.kept.actions").width(width).gap(12.0f).content([&] {
        ActionButton(ui, context, "transcribe.retry", "transcribe.actions.retry", 160.0f, true, true,
                     state.onRetryWrite);
        ActionButton(ui, context, "transcribe.save_app", "transcribe.actions.save_app", 260.0f, false,
                     !state.rescue.empty(), state.onSaveToAppData);
    }).build();
    if (!state.rescue.empty()) {
        Detail(ui, context, width, "transcribe.kept.rescue",
               Text(context, "transcribe.rescue") + state.rescue.u8string(), Muted(context));
    }
    ComposeTranscribeFacts(ui, context, width, job);
}

// The phase, and while it runs the progress and the elapsed time; at the end the result or the failure.
void ComposeTranscribeStatus(eui::Ui& ui, const PageContext& context, float width, const TranscribeState& state) {
    const core::TranscriptionState& job = state.transcription;
    if (job.phase == core::TranscriptionPhase::kIdle) {
        return;
    }
    Heading(ui, context, "transcribe.status.title", Text(context, "transcribe.status.title"));
    switch (job.phase) {
    case core::TranscriptionPhase::kRunning: {
        Line(ui, context, width, "transcribe.phase", Text(context, "transcribe.phase.running"), context.theme.text);
        if (job.attempt >= 2) {
            Line(ui, context, width, "transcribe.retrying", Text(context, "transcribe.retrying"), Warning());
        }
        // The server's progress, as a bar once it reports one.
        const std::string progress =
            job.progress >= 0 ? std::to_string(job.progress) + "%" : Text(context, "transcribe.progress.unknown");
        Line(ui, context, width, "transcribe.progress", Text(context, "transcribe.progress") + progress,
             context.theme.text);
        if (job.progress >= 0) {
            components::progress(ui, "transcribe.progress.bar")
                .size(std::min(width, 420.0f), 8.0f)
                .value(static_cast<float>(job.progress) / 100.0f)
                .theme(context.theme)
                .build();
        }
        Line(ui, context, width, "transcribe.elapsed", Text(context, "transcribe.elapsed") + FormatClock(job.elapsedMs),
             context.theme.text);
        break;
    }
    case core::TranscriptionPhase::kSaving:
        Line(ui, context, width, "transcribe.phase", Text(context, "transcribe.phase.saving"), context.theme.text);
        break;
    case core::TranscriptionPhase::kDone:
        Line(ui, context, width, "transcribe.phase", Text(context, "transcribe.phase.done"), Good());
        Line(ui, context, width, "transcribe.result.saved",
             Text(context, "transcribe.result.saved") + job.destination.u8string(), context.theme.text);
        if (job.destination != core::TranscriptionDestination(job.source)) {
            Line(ui, context, width, "transcribe.result.elsewhere", Text(context, "transcribe.result.elsewhere"),
                 Warning());
        }
        Line(ui, context, width, "transcribe.elapsed", Text(context, "transcribe.elapsed") + FormatClock(job.elapsedMs),
             Muted(context));
        ComposeTranscribeFacts(ui, context, width, job);
        break;
    case core::TranscriptionPhase::kFailed:
        if (job.failure == core::TranscriptionFailure::kCancelled) {
            Line(ui, context, width, "transcribe.phase", Text(context, "transcribe.phase.cancelled"), Warning());
        } else {
            Line(ui, context, width, "transcribe.phase", Text(context, "transcribe.phase.failed"), Bad());
        }
        ComposeTranscribeFailure(ui, context, width, state);
        break;
    case core::TranscriptionPhase::kIdle:
        break;
    }
}

}  // namespace

void ComposePlaceholder(eui::Ui& ui, const PageContext& context, const char* id, std::string_view titleKey,
                        std::string_view bodyKey) {
    const std::string base(id);
    ui.column(base)
        .size(context.width, context.height)
        .gap(kSectionGap)
        .content([&] {
            Title(ui, context, base + ".title", titleKey);
            Line(ui, context, context.width, base + ".body", Text(context, bodyKey), Muted(context));
        })
        .build();
}

void ComposeDashboard(eui::Ui& ui, const PageContext& context, const DashboardState& state) {
    // The log sits below the scrolling content, so the two scroll areas never nest.
    const float logHeight = ServerLogHeight(context, state.log);
    const float contentHeight = std::max(0.0f, context.height - logHeight - kSectionGap);
    ui.column("page.dashboard.layout").size(context.width, context.height).gap(kSectionGap).content([&] {
        // The scroll view composes its content twice per frame, once to measure it, each time
        // with its own Ui, so the content uses the Ui and width it is given.
        components::scrollView(ui, "page.dashboard")
            .size(context.width, contentHeight)
            .gap(kSectionGap)
            .offset(state.scrollOffset)
            .onChange(state.onScroll)
            .theme(context.theme)
            .content([&context, &state](eui::Ui& content, float width, float) {
                Title(content, context, "dashboard.title", "tab.dashboard");
                Line(content, context, width, "dashboard.body", Text(context, "page.dashboard.body"),
                     Muted(context));
                ComposeServer(content, context, width, state);
                ComposeProfiles(content, context, width, state);
                ComposeAppData(content, context, width, state.appData);
            })
            .build();
        ComposeServerLog(ui, context, context.width, logHeight, state);
    }).build();
}

void ComposeStopConfirmation(eui::Ui& ui, const PageContext& context, const StopConfirmation& confirmation) {
    components::dialog(ui, "server.confirm")
        .theme(context.theme)
        .screen(confirmation.screenWidth, confirmation.screenHeight)
        .size(460.0f, 240.0f)
        .open(confirmation.open)
        .title(Text(context, "server.confirm.title"))
        .message(Text(context, "server.confirm.message"))
        .primaryText(Text(context, "server.confirm.stop"))
        .secondaryText(Text(context, "server.confirm.cancel"))
        .onPrimary(confirmation.onConfirm)
        .onSecondary(confirmation.onCancel)
        .onOpenChange([onCancel = confirmation.onCancel](bool open) {
            if (!open && onCancel) {
                onCancel();
            }
        })
        .build();
}

void ComposeTranscribe(eui::Ui& ui, const PageContext& context, const TranscribeState& state) {
    components::scrollView(ui, "page.transcribe")
        .size(context.width, context.height)
        .gap(kSectionGap)
        .offset(state.scrollOffset)
        .onChange(state.onScroll)
        .theme(context.theme)
        .content([&context, &state](eui::Ui& content, float width, float) {
            Title(content, context, "transcribe.title", "tab.transcribe");
            ComposeTranscribeServer(content, context, width, state);
            ComposeTranscribeFile(content, context, width, state);
            const core::TranscriptionState& job = state.transcription;
            const bool canStart =
                state.server.state == core::ServerState::kRunning && !state.file.empty() && !Working(job);
            content.row("transcribe.actions").width(width).gap(12.0f).content([&] {
                ActionButton(content, context, "transcribe.start", "transcribe.actions.start", 140.0f, true, canStart,
                             state.onTranscribe);
                ActionButton(content, context, "transcribe.cancel", "transcribe.actions.cancel", 120.0f, false,
                             job.phase == core::TranscriptionPhase::kRunning, state.onCancel);
            }).build();
            ComposeTranscribeStatus(content, context, width, state);
        })
        .build();
}

void ComposeReplaceConfirmation(eui::Ui& ui, const PageContext& context, const ReplaceConfirmation& confirmation) {
    components::dialog(ui, "transcribe.confirm")
        .theme(context.theme)
        .screen(confirmation.screenWidth, confirmation.screenHeight)
        .size(460.0f, 240.0f)
        .open(confirmation.open)
        .title(Text(context, "transcribe.confirm.title"))
        .message(Text(context, "transcribe.confirm.message"))
        .primaryText(Text(context, "transcribe.confirm.replace"))
        .secondaryText(Text(context, "transcribe.confirm.cancel"))
        .onPrimary(confirmation.onConfirm)
        .onSecondary(confirmation.onCancel)
        .onOpenChange([onCancel = confirmation.onCancel](bool open) {
            if (!open && onCancel) {
                onCancel();
            }
        })
        .build();
}

void ComposeSettings(eui::Ui& ui, const PageContext& context, std::function<void(util::Language)> onLanguageChange) {
    ui.column("page.settings")
        .size(context.width, context.height)
        .gap(kSectionGap)
        .content([&] {
            Title(ui, context, "page.settings.title", "tab.settings");
            Line(ui, context, context.width, "page.settings.language.label", Text(context, "settings.language"),
                 context.theme.text);
            components::segmented(ui, "page.settings.language")
                .size(240.0f, 36.0f)
                .items({Text(context, "language.english"), Text(context, "language.chinese")})
                .selected(context.strings.GetLanguage() == util::Language::Chinese ? 1 : 0)
                .theme(context.theme)
                .onChange([onLanguageChange = std::move(onLanguageChange)](int index) {
                    onLanguageChange(index == 1 ? util::Language::Chinese : util::Language::English);
                })
                .build();
            Line(ui, context, context.width, "page.settings.body", Text(context, "page.settings.body"),
                 Muted(context));
        })
        .build();
}

}  // namespace crisp::ui
