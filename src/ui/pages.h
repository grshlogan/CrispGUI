#pragma once

#include "app/app_data.h"
#include "core/profile_store.h"
#include "core/server_log.h"
#include "core/server_state.h"
#include "core/transcription.h"
#include "ui/fonts.h"
#include "util/string_table.h"

#include "eui_neo.h"

#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
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
    const core::ServerStatus& server;
    const core::LogBuffer& log;
    bool scanned = false;   // a scan has finished at least once
    bool scanning = false;  // a scan is running
    float scrollOffset = 0.0f;
    float logOffset = 0.0f;
    bool logFollow = true;  // the log view stays at its newest line
    std::function<void()> onRescan;
    std::function<void(float)> onScroll;
    std::function<void(const std::filesystem::path&)> onStartProfile;
    std::function<void()> onStopServer;
    std::function<void(float offset, bool follow)> onLogScroll;
    std::function<void()> onClearLog;
};

// The confirmation before stopping a server the GUI did not start (SPEC FR-1).
struct StopConfirmation {
    bool open = false;
    float screenWidth = 0.0f;
    float screenHeight = 0.0f;
    std::function<void()> onConfirm;
    std::function<void()> onCancel;
};

// What the Transcribe page shows and the actions it offers. The references name long-lived
// objects of the application; the callbacks capture by value or a long-lived object.
struct TranscribeState {
    const core::ServerStatus& server;
    const core::TranscriptionState& transcription;
    const std::filesystem::path& file;  // the chosen file; empty before one is chosen
    std::uint64_t fileSize = 0;         // of |file|, when fileSizeKnown
    bool fileSizeKnown = false;
    std::filesystem::path destination;  // where the SRT of |file| goes
    bool destinationExists = false;     // a file is there, which a transcription replaces after a confirmation
    std::filesystem::path rescue;       // where "save to the app data folder" writes; empty without one
    const std::string& dialogError;     // why the file dialog could not open; empty for none
    float scrollOffset = 0.0f;
    std::function<void()> onChooseFile;
    std::function<void()> onTranscribe;
    std::function<void()> onCancel;
    std::function<void()> onRetryWrite;
    std::function<void()> onSaveToAppData;
    std::function<void(float)> onScroll;
};

// The confirmation before a transcription replaces an SRT that exists (docs/SPEC.md, FR-3).
struct ReplaceConfirmation {
    bool open = false;
    float screenWidth = 0.0f;
    float screenHeight = 0.0f;
    std::function<void()> onConfirm;
    std::function<void()> onCancel;
};

// A page that shows its title and the phase that fills it.
void ComposePlaceholder(eui::Ui& ui, const PageContext& context, const char* id, std::string_view titleKey,
                        std::string_view bodyKey);

// The dashboard: the server with its controls, the profiles of the CrispASR folder with their
// readiness as the launcher would show them and their Start buttons, the app data folders, and
// the server log.
void ComposeDashboard(eui::Ui& ui, const PageContext& context, const DashboardState& state);

// The stop confirmation as an overlay over the whole window.
void ComposeStopConfirmation(eui::Ui& ui, const PageContext& context, const StopConfirmation& confirmation);

// The Transcribe page: the server, the chosen file and where its SRT goes, the actions, and the
// progress, the result with a preview, or the failure with what to do about it.
void ComposeTranscribe(eui::Ui& ui, const PageContext& context, const TranscribeState& state);

// The replace confirmation as an overlay over the whole window.
void ComposeReplaceConfirmation(eui::Ui& ui, const PageContext& context, const ReplaceConfirmation& confirmation);

// The settings page: for now the UI language, which is not persisted yet.
void ComposeSettings(eui::Ui& ui, const PageContext& context, std::function<void(util::Language)> onLanguageChange);

}  // namespace crisp::ui
