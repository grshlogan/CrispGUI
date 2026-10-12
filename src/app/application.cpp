#include "app/application.h"

#include "app/crispasr_location.h"
#include "platform/paths.h"
#include "ui/pages.h"

#include <algorithm>
#include <string>
#include <utility>

namespace crisp::app {
namespace {

constexpr float kMargin = 24.0f;
constexpr float kGap = 20.0f;
constexpr float kTabsHeight = 40.0f;
constexpr float kTabsMaxWidth = 560.0f;

constexpr const char* kProfileScanTask = "crisp.profiles.scan";
constexpr const char* kAutostartVariable = "CRISPGUI_DEV_AUTOSTART";

enum Page { kDashboard, kTranscribe, kBatch, kSettings, kPageCount };

}  // namespace

void Application::Start() {
    fonts_ = ui::FindUiFonts();
    appData_ = PrepareAppData(DefaultAppDataRoot());
    crispAsrRoot_ = DefaultCrispAsrRoot();
    autostart_ = platform::GetEnvironmentUtf8(kAutostartVariable);
    // Worker threads wake the UI loop, which then applies their reports in Compose.
    server_ = std::make_unique<core::ServerManager>(core::ServerManagerOptions(), [] { ::app::requestUpdate(); });
    server_->Begin();
    StartProfileScan();
}

void Application::Shutdown() {
    if (server_) {
        server_->Shutdown();
    }
}

void Application::StartProfileScan() {
    if (crispAsrRoot_.empty()) {
        return;
    }
    profilesScanning_ = true;
    const std::filesystem::path root = crispAsrRoot_;
    ::app::async::restart(
        kProfileScanTask, [root] { return core::ScanProfiles(root); },
        [this](::app::async::Result<core::ProfileScan>& result) {
            profiles_ = std::move(result.value);
            profilesScanned_ = true;
            profilesScanning_ = false;
        });
}

void Application::StartServer(const std::filesystem::path& path) {
    if (!server_) {
        return;
    }
    for (const core::ProfileFolder& folder : profiles_.folders) {
        for (const core::Profile& profile : folder.profiles) {
            if (profile.path != path) {
                continue;
            }
            core::LaunchOptions options;
            if (appData_.Ok()) {
                options.serverTemp = appData_.layout.temp / "server";
            }
            server_->Start(profile, profiles_.paths, options);
            logFollow_ = true;
            return;
        }
    }
}

void Application::StopServer(bool confirmed) {
    if (!server_) {
        return;
    }
    const core::ServerStatus& status = server_->Status();
    if (status.adopted && status.CanStop() && !confirmed) {
        confirmStop_ = true;
        return;
    }
    confirmStop_ = false;
    server_->Stop();
}

void Application::Compose(eui::Ui& ui, const eui::Screen& screen) {
    static const core::ServerStatus kNoServer;
    static const core::LogBuffer kNoLog;
    if (server_) {
        server_->Update();
    }
    // The development autostart waits for the first scan and the check at launch.
    if (!autostart_.empty() && profilesScanned_ && server_ && server_->Status().state != core::ServerState::kChecking) {
        const std::string wanted = std::move(autostart_);
        autostart_.clear();
        for (const core::ProfileFolder& folder : profiles_.folders) {
            for (const core::Profile& profile : folder.profiles) {
                if (folder.name + "/" + profile.name == wanted) {
                    StartServer(profile.path);
                }
            }
        }
    }
    const core::ServerStatus& server = server_ ? server_->Status() : kNoServer;
    const core::LogBuffer& log = server_ ? server_->Log() : kNoLog;
    if (confirmStop_ && !(server.adopted && server.CanStop())) {
        confirmStop_ = false;
    }

    const components::theme::ThemeColorTokens theme = components::theme::dark();
    const float contentWidth = std::max(0.0f, screen.width - kMargin * 2.0f);
    const float pageHeight = std::max(0.0f, screen.height - kMargin * 2.0f - kTabsHeight - kGap);
    selectedPage_ = std::clamp(selectedPage_, 0, kPageCount - 1);
    const ui::PageContext context{strings_, theme, fonts_, contentWidth, pageHeight};

    ui.stack("root").size(screen.width, screen.height).content([&] {
        ui.column("shell")
            .size(screen.width, screen.height)
            .padding(kMargin)
            .gap(kGap)
            .content([&] {
                components::tabs(ui, "shell.tabs")
                    .size(std::min(contentWidth, kTabsMaxWidth), kTabsHeight)
                    .items({std::string(strings_.Get("tab.dashboard")), std::string(strings_.Get("tab.transcribe")),
                            std::string(strings_.Get("tab.batch")), std::string(strings_.Get("tab.settings"))})
                    .selected(selectedPage_)
                    .theme(theme)
                    .onChange([this](int index) { selectedPage_ = index; })
                    .build();

                switch (selectedPage_) {
                case kDashboard: {
                    const ui::DashboardState dashboard{appData_,
                                                       crispAsrRoot_,
                                                       profiles_,
                                                       server,
                                                       log,
                                                       profilesScanned_,
                                                       profilesScanning_,
                                                       dashboardScroll_,
                                                       logScroll_,
                                                       logFollow_,
                                                       [this] { StartProfileScan(); },
                                                       [this](float offset) { dashboardScroll_ = offset; },
                                                       [this](const std::filesystem::path& path) { StartServer(path); },
                                                       [this] { StopServer(false); },
                                                       [this](float offset, bool follow) {
                                                           logScroll_ = offset;
                                                           logFollow_ = follow;
                                                       },
                                                       [this] {
                                                           if (server_) {
                                                               server_->ClearLog();
                                                           }
                                                           logFollow_ = true;
                                                       }};
                    ui::ComposeDashboard(ui, context, dashboard);
                    break;
                }
                case kTranscribe:
                    ui::ComposePlaceholder(ui, context, "page.transcribe", "tab.transcribe", "page.transcribe.body");
                    break;
                case kBatch:
                    ui::ComposePlaceholder(ui, context, "page.batch", "tab.batch", "page.batch.body");
                    break;
                default:
                    ui::ComposeSettings(ui, context,
                                        [this](util::Language language) { strings_.SetLanguage(language); });
                    break;
                }
            })
            .build();

        const ui::StopConfirmation confirmation{confirmStop_, screen.width, screen.height, [this] { StopServer(true); },
                                                [this] { confirmStop_ = false; }};
        ui::ComposeStopConfirmation(ui, context, confirmation);
    }).build();
}

}  // namespace crisp::app
