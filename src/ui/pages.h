#pragma once

#include "app/app_data.h"
#include "core/profile_store.h"
#include "ui/fonts.h"
#include "util/string_table.h"

#include "eui_neo.h"

#include <filesystem>
#include <functional>
#include <string_view>

namespace crisp::ui {

// What every page needs to lay itself out in the area below the tabs.
struct PageContext {
    const util::StringTable& strings;
    const components::theme::ThemeColorTokens& theme;
    const UiFonts& fonts;
    float width = 0.0f;
    float height = 0.0f;
};

// What the dashboard shows and the actions it offers. Callbacks outlive the frame, so they
// capture by value or capture a long-lived object (docs/CONVENTIONS.md).
struct DashboardState {
    const app::AppDataResult& appData;
    const std::filesystem::path& crispAsrRoot;
    const core::ProfileScan& profiles;
    bool scanned = false;   // a scan has finished at least once
    bool scanning = false;  // a scan is running
    float scrollOffset = 0.0f;
    std::function<void()> onRescan;
    std::function<void(float)> onScroll;
};

// A page that shows its title and the phase that fills it.
void ComposePlaceholder(eui::Ui& ui, const PageContext& context, const char* id, std::string_view titleKey,
                        std::string_view bodyKey);

// The dashboard: the app data folders and the profiles of the CrispASR folder with their
// readiness, as the launcher would show them.
void ComposeDashboard(eui::Ui& ui, const PageContext& context, const DashboardState& state);

// The settings page: for now the UI language, which is not persisted yet.
void ComposeSettings(eui::Ui& ui, const PageContext& context, std::function<void(util::Language)> onLanguageChange);

}  // namespace crisp::ui
