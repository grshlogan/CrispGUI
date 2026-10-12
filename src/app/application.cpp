#include "app/application.h"

#include "app/crispasr_location.h"
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

enum Page { kDashboard, kTranscribe, kBatch, kSettings, kPageCount };

}  // namespace

void Application::Start() {
    fonts_ = ui::FindUiFonts();
    appData_ = PrepareAppData(DefaultAppDataRoot());
    crispAsrRoot_ = DefaultCrispAsrRoot();
    StartProfileScan();
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

void Application::Compose(eui::Ui& ui, const eui::Screen& screen) {
    const components::theme::ThemeColorTokens theme = components::theme::dark();
    const float contentWidth = std::max(0.0f, screen.width - kMargin * 2.0f);
    const float pageHeight = std::max(0.0f, screen.height - kMargin * 2.0f - kTabsHeight - kGap);
    selectedPage_ = std::clamp(selectedPage_, 0, kPageCount - 1);

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

            const ui::PageContext context{strings_, theme, fonts_, contentWidth, pageHeight};
            switch (selectedPage_) {
            case kDashboard: {
                const ui::DashboardState dashboard{appData_,
                                                   crispAsrRoot_,
                                                   profiles_,
                                                   profilesScanned_,
                                                   profilesScanning_,
                                                   dashboardScroll_,
                                                   [this] { StartProfileScan(); },
                                                   [this](float offset) { dashboardScroll_ = offset; }};
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
                ui::ComposeSettings(ui, context, [this](util::Language language) { strings_.SetLanguage(language); });
                break;
            }
        })
        .build();
}

}  // namespace crisp::app
