#include "ui/pages.h"

#include "core/launch_command.h"

#include <algorithm>
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

// One profile: a status line like the launcher's menu entry, with the details below it.
void ComposeProfile(eui::Ui& ui, const PageContext& context, float width, const std::string& id,
                    const core::Profile& profile) {
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
            ComposeProfile(ui, context, width, folderId + ".p" + std::to_string(p), folder.profiles[p]);
        }
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
    // The scroll view composes its content twice per frame, once to measure it, each time with
    // its own Ui, so the content uses the Ui and width it is given.
    components::scrollView(ui, "page.dashboard")
        .size(context.width, context.height)
        .gap(kSectionGap)
        .offset(state.scrollOffset)
        .onChange(state.onScroll)
        .theme(context.theme)
        .content([&context, &state](eui::Ui& content, float width, float) {
            Title(content, context, "dashboard.title", "tab.dashboard");
            Line(content, context, width, "dashboard.body", Text(context, "page.dashboard.body"), Muted(context));
            ComposeAppData(content, context, width, state.appData);
            ComposeProfiles(content, context, width, state);
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
