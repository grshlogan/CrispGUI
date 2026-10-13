// EUI-NEO's entry points. EUI-NEO calls these free functions, so this file is the composition
// root: it owns the single Application object (docs/CONVENTIONS.md, C++ conventions).
#include "app/application.h"
#include "ui/fonts.h"
#include "util/string_table.h"

#include "eui_neo.h"

#include <string>

namespace {

crisp::app::Application& TheApplication() {
    static crisp::app::Application application;
    return application;
}

}  // namespace

namespace app {

const DslAppConfig& dslAppConfig() {
    static const DslAppConfig config = [] {
        DslAppConfig result = DslAppConfig{}
                                  .title(std::string(crisp::util::StringTable().Get("app.title")))
                                  .pageId("crispgui")
                                  .windowSize(1100, 720)
                                  .minWindowSize(800, 560)
                                  .fps(60.0)
                                  .clearColor(components::theme::dark().background)
                                  .showDebugOverlay(false)
                                  .onStart([] { TheApplication().Start(); })
                                  .onShutdown([] { TheApplication().Shutdown(); });
        // HarmonyOS Sans SC when the build found it; otherwise EUI-NEO's default font.
        const crisp::ui::UiFonts fonts = crisp::ui::FindUiFonts();
        if (!fonts.text.empty()) {
            result.textFont(fonts.text);
        }
        return result;
    }();
    return config;
}

void compose(eui::Ui& ui, const eui::Screen& screen) {
    TheApplication().Compose(ui, screen);
}

}  // namespace app
