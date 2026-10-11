#pragma once

#include "app/app_data.h"
#include "util/string_table.h"

#include "eui_neo.h"

#include <functional>
#include <string_view>

namespace crisp::ui {

// What every page needs to lay itself out in the area below the tabs.
struct PageContext {
    const util::StringTable& strings;
    const components::theme::ThemeColorTokens& theme;
    float width = 0.0f;
    float height = 0.0f;
};

// A page that shows its title and the phase that fills it.
void ComposePlaceholder(eui::Ui& ui, const PageContext& context, const char* id, std::string_view titleKey,
                        std::string_view bodyKey);

// The dashboard: for now the app data and temporary-file folders, or why they are unavailable.
void ComposeDashboard(eui::Ui& ui, const PageContext& context, const app::AppDataResult& appData);

// The settings page: for now the UI language, which is not persisted yet.
void ComposeSettings(eui::Ui& ui, const PageContext& context, std::function<void(util::Language)> onLanguageChange);

}  // namespace crisp::ui
