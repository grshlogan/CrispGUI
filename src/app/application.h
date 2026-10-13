#pragma once

#include "app/app_data.h"
#include "core/profile_store.h"
#include "core/server_manager.h"
#include "core/transcription.h"
#include "ui/fonts.h"
#include "util/string_table.h"

#include "eui_neo.h"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>

namespace crisp::app {

// Holds all application state behind EUI-NEO's free entry points; src/main.cpp owns the
// single instance.
class Application {
public:
    // DslAppConfig::onStart: runs once on the UI thread before the first Compose.
    void Start();

    // DslAppConfig::onShutdown: stops the worker threads; a running server keeps running.
    void Shutdown();

    // app::compose: declares the UI for the current frame.
    void Compose(eui::Ui& ui, const eui::Screen& screen);

private:
    // Reads the profiles below crispAsrRoot_ on a worker thread; the result replaces profiles_
    // on the UI thread. A newer scan supersedes a running one.
    void StartProfileScan();

    // Starts the server from the scanned profile at |path|.
    void StartServer(const std::filesystem::path& path);
    // Stops the server; an adopted one only after the user confirms.
    void StopServer(bool confirmed);

    // The Transcribe page. ChooseFile() opens the system's file dialog, which blocks while it is open.
    void ChooseFile();
    void SetChosenFile(const std::filesystem::path& file);
    // Looks again at the SRT that the chosen file would get.
    void RefreshDestination();
    // Starts transcribing the chosen file; an SRT that exists is replaced only after |confirmed|.
    void StartTranscription(bool confirmed);
    // Where "save to the app data folder" writes the SRT of the last transcription; empty when
    // there is no app data folder.
    std::filesystem::path RescuePath() const;

    util::StringTable strings_;
    ui::UiFonts fonts_;
    AppDataResult appData_;
    std::filesystem::path crispAsrRoot_;
    core::ProfileScan profiles_;
    bool profilesScanned_ = false;
    bool profilesScanning_ = false;
    std::unique_ptr<core::ServerManager> server_;
    std::unique_ptr<core::Transcription> transcription_;
    std::filesystem::path chosenFile_;
    std::filesystem::path chosenDestination_;
    std::uint64_t chosenSize_ = 0;
    bool chosenSizeKnown_ = false;
    bool destinationExists_ = false;  // a file is there, which a transcription replaces after the confirmation
    std::string dialogError_;
    bool confirmReplace_ = false;
    float transcribeScroll_ = 0.0f;
    core::TranscriptionPhase lastPhase_ = core::TranscriptionPhase::kIdle;
    // CRISPGUI_DEV_TRANSCRIBE: [choose:]<file>, chosen at launch, with the Transcribe page selected;
    // without "choose:" the transcription starts once a server runs, so that development runs
    // need no file dialog (docs/ENVIRONMENT.md, section 2.2).
    std::string devTranscribe_;
    bool devTranscribeStart_ = false;
    // CRISPGUI_DEV_AUTOSTART: <folder>/<name> of a profile to start once the first scan and the
    // check at launch are done, so that development runs can exercise a start without clicking
    // (docs/ENVIRONMENT.md, section 2.2).
    std::string autostart_;
    bool confirmStop_ = false;
    float dashboardScroll_ = 0.0f;
    float logScroll_ = 0.0f;
    bool logFollow_ = true;
    int selectedPage_ = 0;
};

}  // namespace crisp::app
