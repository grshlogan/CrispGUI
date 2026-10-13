#pragma once

#include "core/api_client.h"
#include "core/server_state.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

// Transcribes one file into an SRT next to it (SPEC FR-3; docs/ARCHITECTURE.md, sections 2.2 and
// 2.3). The UI thread owns the object, as it owns the ServerManager: a job thread does the
// blocking work, a second thread reads /progress meanwhile, and both post what they observe to an
// inbox that Update() applies.
namespace crisp::core {

enum class TranscriptionPhase {
    kIdle,  // nothing was started
    kRunning,
    kDone,    // the SRT is written
    kFailed,  // see TranscriptionState::failure
};

// Why a transcription failed, after docs/API-CRISPASR.md, section 5. The page words each one; the
// detail carries what the server or the system said, in English.
enum class TranscriptionFailure {
    kNone,
    kServerUnreachable,  // no answer: the server is stopped, still starting, or gone
    kTimeout,
    kCancelled,
    kBadRequest,         // 400: the detail is the server's message
    kUnauthorized,       // 401: the key is not the server's
    kTooLarge,           // 413, or a source over the upload limit, which is then not sent
    kServerBusy,         // 503: the model is loading or being swapped
    kServerError,        // 500, also after the one retry
    kUnexpectedAnswer,   // another status, or an answer that is not a transcript
    kSourceUnreadable,   // the source is not a readable regular file
    kDestinationExists,  // the SRT exists and may not be replaced, or is the source itself
    kCannotWrite,        // the SRT could not be written; the detail is WriteFileAtomically's message
};

struct TranscriptionRequest {
    std::filesystem::path source;
    ServerEndpoint endpoint;       // Status().endpoint of the running server
    std::string apiKey;            // ServerManager::ApiKey()
    bool replaceExisting = false;  // whether an SRT that exists may be replaced
    // Request-tier fields for the server, such as {"language", "ja"} (TranscribeRequest::fields).
    std::vector<std::pair<std::string, std::string>> fields;
};

struct TranscriptionOptions {
    int progressIntervalMs = 500;  // between two reads of /progress
    int retryDelayMs = 1000;       // before the one retry of a 500
    int connectTimeoutMs = 5000;
    int requestTimeoutMs = 5000;               // for /progress
    int transcribeTimeoutMs = 30 * 60 * 1000;  // for the transcription request
    std::uint64_t maxUploadBytes = kServerMaxUploadBytes;
    std::size_t previewCues = 5;  // how many cue texts the state keeps for the preview
};

struct TranscriptionState {
    TranscriptionPhase phase = TranscriptionPhase::kIdle;
    std::filesystem::path source;
    std::filesystem::path destination;
    int attempt = 0;             // 1 for the request, 2 for the one retry of a 500
    int progress = -1;           // the server's progress, 0..100, while it is known; 100 once done
    std::int64_t elapsedMs = 0;  // since the start; fixed when the transcription ends

    // kDone
    int cues = 0;
    std::int64_t durationMs = 0;       // of the processed audio, from the transcript
    std::vector<std::string> preview;  // the texts of the first cues, as the file has them

    // kFailed
    TranscriptionFailure failure = TranscriptionFailure::kNone;
    std::string detail;
};

// Where the SRT of |source| goes: next to it, with the extension replaced by ".srt".
std::filesystem::path TranscriptionDestination(const std::filesystem::path& source);

class Transcription {
public:
    // |wake| is called from worker threads when Update() has something to apply, such as
    // app::requestUpdate(); it is never called after Shutdown() returns.
    Transcription(TranscriptionOptions options, std::function<void()> wake);
    ~Transcription();
    Transcription(const Transcription&) = delete;
    Transcription& operator=(const Transcription&) = delete;

    // Starts transcribing |request.source|: the state is kRunning at once. Before anything is sent,
    // the job thread checks that the source is a readable regular file within the upload limit and
    // that the destination is free or may be replaced. The request is retried once after a 500 and
    // never otherwise. Returns false and changes nothing while a transcription runs and after
    // Shutdown().
    bool Start(TranscriptionRequest request);
    // Aborts the request in progress: the state becomes kFailed with kCancelled, nothing is
    // written, and no temporary file stays. A cancel that arrives while the file is being written
    // lets the write finish, and the state becomes kDone. Ignored when nothing runs.
    void Cancel();
    // Cancels a running transcription and joins the threads. Calling it again does nothing.
    void Shutdown();

    // Applies what the threads reported and refreshes the elapsed time. Call on the UI thread
    // before reading the state; returns true when the state changed, at the latest when the
    // elapsed time passes a whole second.
    bool Update();

    const TranscriptionState& State() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace crisp::core
