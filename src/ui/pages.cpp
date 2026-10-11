#include "ui/pages.h"

#include <string>
#include <utility>

namespace crisp::ui {
namespace {

constexpr float kTitleSize = 24.0f;
constexpr float kBodySize = 15.0f;
constexpr float kLineHeight = 22.0f;
constexpr float kSectionGap = 12.0f;

std::string Text(const PageContext& context, std::string_view key) {
    return std::string(context.strings.Get(key));
}

eui::Color Muted(const PageContext& context) {
    return components::theme::withOpacity(context.theme.text, 0.64f);
}

eui::Color Warning() {
    return components::theme::color(0.96f, 0.69f, 0.25f, 1.0f);
}

void Title(eui::Ui& ui, const PageContext& context, const std::string& id, std::string_view key) {
    components::text(ui, id, context.theme).text(Text(context, key)).fontSize(kTitleSize).build();
}

void Line(eui::Ui& ui, const PageContext& context, const std::string& id, const std::string& text,
          const eui::Color& color) {
    components::text(ui, id, context.theme)
        .width(context.width)
        .text(text)
        .fontSize(kBodySize)
        .lineHeight(kLineHeight)
        .wrap(true)
        .color(color)
        .build();
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
            Line(ui, context, base + ".body", Text(context, bodyKey), Muted(context));
        })
        .build();
}

void ComposeDashboard(eui::Ui& ui, const PageContext& context, const app::AppDataResult& appData) {
    ui.column("page.dashboard")
        .size(context.width, context.height)
        .gap(kSectionGap)
        .content([&] {
            Title(ui, context, "page.dashboard.title", "tab.dashboard");
            Line(ui, context, "page.dashboard.body", Text(context, "page.dashboard.body"), Muted(context));
            if (appData.Ok()) {
                Line(ui, context, "page.dashboard.root", Text(context, "appdata.root") + appData.layout.root.u8string(),
                     context.theme.text);
                Line(ui, context, "page.dashboard.temp", Text(context, "appdata.temp") + appData.layout.temp.u8string(),
                     context.theme.text);
                if (appData.onSystemDrive) {
                    Line(ui, context, "page.dashboard.system_drive", Text(context, "appdata.system_drive"), Warning());
                }
            } else {
                Line(ui, context, "page.dashboard.error", Text(context, "appdata.error") + " " + appData.error,
                     Warning());
                Line(ui, context, "page.dashboard.action", Text(context, "appdata.error.action"), Warning());
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
            Line(ui, context, "page.settings.language.label", Text(context, "settings.language"), context.theme.text);
            components::segmented(ui, "page.settings.language")
                .size(240.0f, 36.0f)
                .items({Text(context, "language.english"), Text(context, "language.chinese")})
                .selected(context.strings.GetLanguage() == util::Language::Chinese ? 1 : 0)
                .theme(context.theme)
                .onChange([onLanguageChange = std::move(onLanguageChange)](int index) {
                    onLanguageChange(index == 1 ? util::Language::Chinese : util::Language::English);
                })
                .build();
            Line(ui, context, "page.settings.body", Text(context, "page.settings.body"), Muted(context));
        })
        .build();
}

}  // namespace crisp::ui
