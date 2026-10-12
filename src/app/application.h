#pragma once

#include "app/app_data.h"
#include "core/profile_store.h"
#include "ui/fonts.h"
#include "util/string_table.h"

#include "eui_neo.h"

#include <filesystem>

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
    // Reads the profiles below crispAsrRoot_ on a worker thread; the result replaces profiles_
    // on the UI thread. A newer scan supersedes a running one.
    void StartProfileScan();

    util::StringTable strings_;
    ui::UiFonts fonts_;
    AppDataResult appData_;
    std::filesystem::path crispAsrRoot_;
    core::ProfileScan profiles_;
    bool profilesScanned_ = false;
    bool profilesScanning_ = false;
    float dashboardScroll_ = 0.0f;
    int selectedPage_ = 0;
};

}  // namespace crisp::app
