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
constexpr const char* kDevTranscribeVariable = "CRISPGUI_DEV_TRANSCRIBE";
constexpr const char* kDevChoosePrefix = "choose:";

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
    transcription_ =
        std::make_unique<core::Transcription>(core::TranscriptionOptions(), [] { ::app::requestUpdate(); });
    devTranscribe_ = platform::GetEnvironmentUtf8(kDevTranscribeVariable);
    StartProfileScan();
}

void Application::Shutdown() {
    // A transcription in progress is cancelled, so that no partial file is written; a server keeps running.
    if (transcription_) {
        transcription_->Shutdown();
    }
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

void Application::SetChosenFile(const std::filesystem::path& file) {
    chosenFile_ = file;
    std::error_code error;
    const std::uintmax_t size = std::filesystem::file_size(file, error);
    chosenSizeKnown_ = !error;
    chosenSize_ = error ? 0 : static_cast<std::uint64_t>(size);
    dialogError_.clear();
    RefreshDestination();
}

void Application::RefreshDestination() {
    chosenDestination_ = chosenFile_.empty() ? std::filesystem::path() : core::TranscriptionDestination(chosenFile_);
    std::error_code error;
    destinationExists_ = !chosenFile_.empty() && std::filesystem::is_regular_file(chosenDestination_, error);
}

void Application::ChooseFile() {
    if (transcription_ && (transcription_->State().phase == core::TranscriptionPhase::kRunning ||
                           transcription_->State().phase == core::TranscriptionPhase::kSaving)) {
        return;
    }
    eui::platform::FileDialogOptions options;
    options.prompt = std::string(strings_.Get("transcribe.dialog.prompt"));
    options.filterName = std::string(strings_.Get("transcribe.dialog.filter"));
    // The formats FR-3 names; video containers wait for a check on Windows.
    options.allowedExtensions = {"wav", "mp3", "flac", "ogg", "m4a", "aac", "opus", "webm", "wma"};
    if (!chosenFile_.empty()) {
        options.initialDirectory = chosenFile_.parent_path().u8string();
    }
    const eui::platform::FileDialogResult result = eui::platform::openFileDialog(options);
    if (result.selected()) {
        SetChosenFile(std::filesystem::u8path(result.paths.front()));
    } else if (result.status == eui::platform::FileDialogStatus::Failed) {
        dialogError_ = result.error;
    }
}

void Application::StartTranscription(bool confirmed) {
    if (!transcription_ || !server_ || chosenFile_.empty()) {
        return;
    }
    const core::ServerStatus& server = server_->Status();
    if (server.state != core::ServerState::kRunning) {
        return;
    }
    RefreshDestination();
    if (destinationExists_ && !confirmed) {
        confirmReplace_ = true;
        return;
    }
    confirmReplace_ = false;
    core::TranscriptionRequest request;
    request.source = chosenFile_;
    request.endpoint = server.endpoint;
    request.apiKey = server_->ApiKey();
    request.replaceExisting = destinationExists_;  // reached only with the confirmation
    transcription_->Start(std::move(request));
}

std::filesystem::path Application::RescuePath() const {
    if (!appData_.Ok() || !transcription_ || transcription_->State().source.empty()) {
        return {};
    }
    const std::filesystem::path name = core::TranscriptionDestination(transcription_->State().source).filename();
    return appData_.layout.root / "output" / name;
}

void Application::Compose(eui::Ui& ui, const eui::Screen& screen) {
    static const core::ServerStatus kNoServer;
    static const core::LogBuffer kNoLog;
    if (server_) {
        server_->Update();
    }
    if (transcription_) {
        transcription_->Update();
        // The SRT may exist now, or be gone: look again when a transcription ends.
        const core::TranscriptionPhase phase = transcription_->State().phase;
        if (phase != lastPhase_) {
            lastPhase_ = phase;
            if (phase == core::TranscriptionPhase::kDone || phase == core::TranscriptionPhase::kFailed) {
                RefreshDestination();
            }
        }
    }
    // The development variable chooses its file at launch and, unless it says "choose:", transcribes it
    // once a server runs.
    if (!devTranscribe_.empty()) {
        std::string wanted = std::move(devTranscribe_);
        devTranscribe_.clear();
        const std::string prefix = kDevChoosePrefix;
        devTranscribeStart_ = wanted.rfind(prefix, 0) != 0;
        if (!devTranscribeStart_) {
            wanted.erase(0, prefix.size());
        }
        SetChosenFile(std::filesystem::u8path(wanted));
        selectedPage_ = kTranscribe;
    }
    if (devTranscribeStart_ && server_ && server_->Status().state == core::ServerState::kRunning) {
        devTranscribeStart_ = false;
        StartTranscription(false);
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
                case kTranscribe: {
                    static const core::TranscriptionState kNoTranscription;
                    const ui::TranscribeState transcribe{
                        server,
                        transcription_ ? transcription_->State() : kNoTranscription,
                        chosenFile_,
                        chosenSize_,
                        chosenSizeKnown_,
                        chosenDestination_,
                        destinationExists_,
                        RescuePath(),
                        dialogError_,
                        transcribeScroll_,
                        [this] { ChooseFile(); },
                        [this] { StartTranscription(false); },
                        [this] {
                            if (transcription_) {
                                transcription_->Cancel();
                            }
                        },
                        [this] {
                            if (transcription_) {
                                transcription_->RetryWrite();
                            }
                        },
                        [this] {
                            const std::filesystem::path rescue = RescuePath();
                            if (transcription_ && !rescue.empty()) {
                                transcription_->SaveTo(rescue);
                            }
                        },
                        [this](float offset) { transcribeScroll_ = offset; }};
                    ui::ComposeTranscribe(ui, context, transcribe);
                    break;
                }
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

        const ui::ReplaceConfirmation replace{confirmReplace_ && selectedPage_ == kTranscribe,
                                              screen.width,
                                              screen.height,
                                              [this] { StartTranscription(true); },
                                              [this] { confirmReplace_ = false; }};
        ui::ComposeReplaceConfirmation(ui, context, replace);
    }).build();
}

}  // namespace crisp::app
