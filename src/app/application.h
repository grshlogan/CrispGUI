#pragma once

#include "app/app_data.h"
#include "util/string_table.h"

#include "eui_neo.h"

namespace crisp::app {

// Holds all application state behind EUI-NEO's free entry points; src/main.cpp owns the
// single instance.
class Application {
public:
    // DslAppConfig::onStart: runs once on the UI thread before the first Compose.
    void Start();

    // app::compose: declares the UI for the current frame.
    void Compose(eui::Ui& ui, const eui::Screen& screen);

private:
    util::StringTable strings_;
    AppDataResult appData_;
    int selectedPage_ = 0;
};

}  // namespace crisp::app
