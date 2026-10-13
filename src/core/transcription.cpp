#include "core/transcription.h"

#include "core/srt_writer.h"
#include "core/transcript.h"
#include "platform/files.h"
#include "platform/http.h"

#include <cerrno>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <mutex>
#include <system_error>
#include <thread>
#include <utility>

namespace crisp::core {
namespace {

using Clock = std::chrono::steady_clock;

// How a transcription ended, as the job thread reports it.
struct Outcome {
    TranscriptionPhase phase = TranscriptionPhase::kFailed;
    TranscriptionFailure failure = TranscriptionFailure::kNone;
    std::string detail;
    int cues = 0;
    std::int64_t durationMs = 0;
    std::vector<std::string> preview;
    std::int64_t elapsedMs = 0;
};

Outcome Failed(TranscriptionFailure failure, std::string detail = std::string()) {
    Outcome outcome;
    outcome.failure = failure;
    outcome.detail = std::move(detail);
    return outcome;
}

TranscriptionFailure FailureOf(ApiErrorKind kind) {
    switch (kind) {
    case ApiErrorKind::kNone:
        return TranscriptionFailure::kNone;
    case ApiErrorKind::kUnreachable:
        return TranscriptionFailure::kServerUnreachable;
    case ApiErrorKind::kTimeout:
        return TranscriptionFailure::kTimeout;
    case ApiErrorKind::kCancelled:
        return TranscriptionFailure::kCancelled;
    case ApiErrorKind::kBadRequest:
        return TranscriptionFailure::kBadRequest;
    case ApiErrorKind::kUnauthorized:
        return TranscriptionFailure::kUnauthorized;
    case ApiErrorKind::kPayloadTooLarge:
        return TranscriptionFailure::kTooLarge;
    case ApiErrorKind::kServerBusy:
        return TranscriptionFailure::kServerBusy;
    case ApiErrorKind::kServerError:
        return TranscriptionFailure::kServerError;
    case ApiErrorKind::kProtocol:
        return TranscriptionFailure::kUnexpectedAnswer;
    case ApiErrorKind::kFile:
        return TranscriptionFailure::kSourceUnreadable;
    }
    return TranscriptionFailure::kUnexpectedAnswer;
}

std::int64_t ElapsedMs(Clock::time_point since) {
    return std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - since).count();
}

struct Event {
    enum class Kind { kAttempt, kProgress, kFinished };
    Kind kind = Kind::kProgress;
    int attempt = 0;    // of kAttempt, and the attempt a kProgress event was read in
    int progress = -1;  // of kProgress: -1 when the server reported none
    Outcome outcome;    // of kFinished
};

// Where the worker threads post what they observe; Update() takes it on the UI thread.
class Inbox {
public:
    explicit Inbox(std::function<void()> wake) : wake_(std::move(wake)) {}

    void Post(Event event) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            events_.push_back(std::move(event));
        }
        if (wake_) {
            wake_();
        }
    }

    std::vector<Event> Take() {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<Event> events;
        events.swap(events_);
        return events;
    }

private:
    std::function<void()> wake_;
    std::mutex mutex_;
    std::vector<Event> events_;
};

// One transcription. The job thread runs Run(); a second thread reads /progress while the request
// waits; the UI thread may Cancel() at any time.
class Job {
public:
    Job(TranscriptionRequest request, const TranscriptionOptions& options, Inbox& inbox, Clock::time_point started)
        : request_(std::move(request)),
          destination_(TranscriptionDestination(request_.source)),
          options_(options),
          inbox_(inbox),
          started_(started) {}

    // Checks, requests, writes, and posts how it ended.
    void Run() {
        Outcome outcome = Execute();
        outcome.elapsedMs = ElapsedMs(started_);
        Event event;
        event.kind = Event::Kind::kFinished;
        event.outcome = std::move(outcome);
        inbox_.Post(std::move(event));
    }

    // Aborts the request in progress, and everything after it that has not begun.
    void Cancel() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            cancelRequested_ = true;
        }
        transcribeCancel_.Cancel();
        progressCancel_.Cancel();
        wakeUp_.notify_all();
    }

private:
    Outcome Execute() {
        Outcome outcome;
        if (!CheckSource(outcome) || !CheckDestination(outcome)) {
            return outcome;
        }
        const ApiClient client(ClientOptions());
        Transcript transcript;
        std::thread poller([this, &client] { PollProgress(client); });
        const bool received = Request(client, transcript, outcome);
        {
            std::lock_guard<std::mutex> lock(mutex_);
            pollerDone_ = true;
        }
        progressCancel_.Cancel();  // ends a read in progress, so the join is quick
        wakeUp_.notify_all();
        poller.join();
        return received ? Write(transcript) : outcome;
    }

    ApiClientOptions ClientOptions() const {
        ApiClientOptions client;
        client.endpoint = request_.endpoint;
        client.apiKey = request_.apiKey;
        client.connectTimeoutMs = options_.connectTimeoutMs;
        client.requestTimeoutMs = options_.requestTimeoutMs;
        client.transcribeTimeoutMs = options_.transcribeTimeoutMs;
        client.maxUploadBytes = options_.maxUploadBytes;
        return client;
    }

    // The source must be a regular file that opens and is within the upload limit. It is looked at
    // before it is opened, because opening a pipe waits for the other end.
    bool CheckSource(Outcome& outcome) const {
        const std::string name = request_.source.u8string();
        std::error_code lookup;
        const std::filesystem::file_status kind = std::filesystem::status(request_.source, lookup);
        if (lookup) {
            outcome = Failed(TranscriptionFailure::kSourceUnreadable,
                             "Could not open " + name + ": " + lookup.message() + ".");
            return false;
        }
        if (!std::filesystem::is_regular_file(kind)) {
            outcome = Failed(TranscriptionFailure::kSourceUnreadable, name + " is not a regular file.");
            return false;
        }
        std::FILE* file = platform::OpenFileForReading(request_.source);
        if (file == nullptr) {
            const int reason = errno;
            outcome = Failed(TranscriptionFailure::kSourceUnreadable,
                             "Could not open " + name + ": " + std::generic_category().message(reason) + ".");
            return false;
        }
        std::uint64_t size = 0;
        const bool regular = platform::OpenFileSize(file, size);
        std::fclose(file);
        if (!regular) {
            outcome = Failed(TranscriptionFailure::kSourceUnreadable, name + " is not a regular file.");
            return false;
        }
        if (size > options_.maxUploadBytes) {
            outcome = Failed(TranscriptionFailure::kTooLarge,
                             name + " is " + std::to_string(size) + " bytes; the upload limit is " +
                                 std::to_string(options_.maxUploadBytes) + " bytes.");
            return false;
        }
        return true;
    }

    // The SRT must not exist, unless the request allows replacing it; it is never the source.
    bool CheckDestination(Outcome& outcome) const {
        std::error_code error;
        if (!std::filesystem::exists(destination_, error)) {
            return true;
        }
        const std::string name = destination_.u8string();
        if (std::filesystem::equivalent(request_.source, destination_, error)) {
            outcome = Failed(TranscriptionFailure::kDestinationExists, name + " is the source file itself.");
            return false;
        }
        if (!request_.replaceExisting) {
            outcome = Failed(TranscriptionFailure::kDestinationExists, name + " already exists.");
            return false;
        }
        return true;
    }

    bool CancelRequested() {
        std::lock_guard<std::mutex> lock(mutex_);
        return cancelRequested_;
    }

    // Sends the request, once more after a 500 and never after any other failure. False, with the
    // failure in |outcome|, when no transcript came.
    bool Request(const ApiClient& client, Transcript& transcript, Outcome& outcome) {
        TranscribeRequest request;
        request.file = request_.source;
        request.fields = request_.fields;
        for (int attempt = 1;; ++attempt) {
            if (CancelRequested()) {
                outcome = Failed(TranscriptionFailure::kCancelled);
                return false;
            }
            Event event;
            event.kind = Event::Kind::kAttempt;
            event.attempt = attempt;
            inbox_.Post(std::move(event));
            SetWaitingAttempt(attempt);
            const ApiError error = client.Transcribe(request, transcript, &transcribeCancel_);
            SetWaitingAttempt(0);
            if (error.Ok()) {
                return true;
            }
            if (error.kind == ApiErrorKind::kServerError && attempt == 1 && PauseBeforeRetry()) {
                continue;
            }
            outcome = CancelRequested() ? Failed(TranscriptionFailure::kCancelled)
                                        : Failed(FailureOf(error.kind), error.message);
            return false;
        }
    }

    void SetWaitingAttempt(int attempt) {
        std::lock_guard<std::mutex> lock(mutex_);
        waitingAttempt_ = attempt;
    }

    // Waits for the retry delay; false when a cancel ends the wait.
    bool PauseBeforeRetry() {
        std::unique_lock<std::mutex> lock(mutex_);
        wakeUp_.wait_for(lock, std::chrono::milliseconds(options_.retryDelayMs), [this] { return cancelRequested_; });
        return !cancelRequested_;
    }

    // Reads /progress at a steady pace while a request waits, until the job is over or cancelled.
    void PollProgress(const ApiClient& client) {
        for (;;) {
            int attempt = 0;
            {
                std::unique_lock<std::mutex> lock(mutex_);
                wakeUp_.wait_for(lock, std::chrono::milliseconds(options_.progressIntervalMs),
                                 [this] { return pollerDone_ || cancelRequested_; });
                if (pollerDone_ || cancelRequested_) {
                    return;
                }
                attempt = waitingAttempt_;
            }
            if (attempt == 0) {
                continue;
            }
            ProgressReport report;
            const ApiError error = client.Progress(report, &progressCancel_);
            if (error.kind == ApiErrorKind::kCancelled) {
                return;
            }
            // The server's progress is global and reads idle while an upload is still arriving
            // (docs/API-CRISPASR.md, section 7): only a busy server tells about this request. An
            // event is posted either way, so that the page shows the elapsed time at this pace.
            Event event;
            event.kind = Event::Kind::kProgress;
            event.attempt = attempt;
            event.progress = error.Ok() && report.busy ? report.progress : -1;
            inbox_.Post(std::move(event));
        }
    }

    // Writes the SRT of |transcript| and describes it.
    Outcome Write(const Transcript& transcript) {
        if (CancelRequested()) {
            return Failed(TranscriptionFailure::kCancelled);
        }
        // The SRT may have appeared while the server worked.
        Outcome outcome;
        if (!CheckDestination(outcome)) {
            return outcome;
        }
        std::string error;
        if (!platform::WriteFileAtomically(destination_, SegmentsToSrt(transcript.segments), error)) {
            return Failed(TranscriptionFailure::kCannotWrite, error);
        }
        outcome.phase = TranscriptionPhase::kDone;
        outcome.durationMs = transcript.durationMs;
        for (const TranscriptSegment& segment : transcript.segments) {
            if (segment.text.empty()) {
                continue;  // SegmentsToSrt writes no cue for it
            }
            ++outcome.cues;
            if (outcome.preview.size() < options_.previewCues) {
                outcome.preview.push_back(SrtSpeakerPrefix(segment.speaker) + segment.text);
            }
        }
        return outcome;
    }

    const TranscriptionRequest request_;
    const std::filesystem::path destination_;
    const TranscriptionOptions options_;
    Inbox& inbox_;
    const Clock::time_point started_;
    platform::HttpCancel transcribeCancel_;
    platform::HttpCancel progressCancel_;
    std::mutex mutex_;
    std::condition_variable wakeUp_;
    // Guarded by mutex_:
    bool cancelRequested_ = false;
    bool pollerDone_ = false;
    int waitingAttempt_ = 0;  // the attempt whose request is in progress; 0 between requests
};

}  // namespace

std::filesystem::path TranscriptionDestination(const std::filesystem::path& source) {
    std::filesystem::path destination = source;
    destination.replace_extension(".srt");
    return destination;
}

struct Transcription::Impl {
    Impl(TranscriptionOptions optionsToUse, std::function<void()> wake)
        : options(std::move(optionsToUse)), inbox(std::move(wake)) {}

    const TranscriptionOptions options;
    Inbox inbox;
    TranscriptionState state;
    Clock::time_point started;
    std::unique_ptr<Job> job;
    std::thread thread;
    bool shutDown = false;
};

Transcription::Transcription(TranscriptionOptions options, std::function<void()> wake)
    : impl_(std::make_unique<Impl>(std::move(options), std::move(wake))) {}

Transcription::~Transcription() {
    Shutdown();
}

bool Transcription::Start(TranscriptionRequest request) {
    if (impl_->shutDown || impl_->state.phase == TranscriptionPhase::kRunning) {
        return false;
    }
    if (impl_->thread.joinable()) {
        impl_->thread.join();  // the previous job, which has posted its end
    }
    impl_->state = TranscriptionState();
    impl_->state.phase = TranscriptionPhase::kRunning;
    impl_->state.source = request.source;
    impl_->state.destination = TranscriptionDestination(request.source);
    impl_->started = Clock::now();
    impl_->job = std::make_unique<Job>(std::move(request), impl_->options, impl_->inbox, impl_->started);
    impl_->thread = std::thread([job = impl_->job.get()] { job->Run(); });
    return true;
}

void Transcription::Cancel() {
    if (impl_->state.phase == TranscriptionPhase::kRunning && impl_->job) {
        impl_->job->Cancel();
    }
}

void Transcription::Shutdown() {
    if (impl_->shutDown) {
        return;
    }
    impl_->shutDown = true;
    if (impl_->job) {
        impl_->job->Cancel();
    }
    if (impl_->thread.joinable()) {
        impl_->thread.join();
    }
}

bool Transcription::Update() {
    TranscriptionState& state = impl_->state;
    std::vector<Event> events = impl_->inbox.Take();
    for (Event& event : events) {
        switch (event.kind) {
        case Event::Kind::kAttempt:
            state.attempt = event.attempt;
            state.progress = -1;
            break;
        case Event::Kind::kProgress:
            // A read of an earlier attempt that arrives late tells nothing about this one.
            if (event.attempt == state.attempt && event.progress >= 0) {
                state.progress = event.progress;
            }
            break;
        case Event::Kind::kFinished: {
            Outcome& outcome = event.outcome;
            state.phase = outcome.phase;
            state.failure = outcome.failure;
            state.detail = std::move(outcome.detail);
            state.cues = outcome.cues;
            state.durationMs = outcome.durationMs;
            state.preview = std::move(outcome.preview);
            state.elapsedMs = outcome.elapsedMs;
            if (state.phase == TranscriptionPhase::kDone) {
                state.progress = 100;
            }
            break;
        }
        }
    }
    bool changed = !events.empty();
    if (state.phase == TranscriptionPhase::kRunning) {
        const std::int64_t elapsed = ElapsedMs(impl_->started);
        changed = changed || elapsed / 1000 != state.elapsedMs / 1000;
        state.elapsedMs = elapsed;
    }
    return changed;
}

const TranscriptionState& Transcription::State() const {
    return impl_->state;
}

}  // namespace crisp::core
