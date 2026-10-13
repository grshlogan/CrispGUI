#include "check.h"
#include "server_test_support.h"

#include "core/server_manager.h"
#include "core/server_state.h"
#include "core/transcription.h"
#include "platform/files.h"
#include "platform/process.h"
#include "util/json.h"

#if !defined(_WIN32)
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <string>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

namespace fs = std::filesystem;
using crisp::core::HealthKind;
using crisp::core::HealthReport;
using crisp::core::ServerCommand;
using crisp::core::ServerCommands;
using crisp::core::ServerEndpoint;
using crisp::core::ServerManager;
using crisp::core::ServerManagerOptions;
using crisp::core::ServerState;
using crisp::core::ServerStateMachine;
using crisp::core::SpawnReport;
using crisp::core::StartRequest;
using crisp::core::Transcription;
using crisp::core::TranscriptionDestination;
using crisp::core::TranscriptionFailure;
using crisp::core::TranscriptionOptions;
using crisp::core::TranscriptionPhase;
using crisp::core::TranscriptionRequest;
using crisp::core::TranscriptionState;
using crisp::platform::ReadFileBytes;
using Clock = std::chrono::steady_clock;
using Entries = std::vector<std::string>;
using Fields = std::vector<std::pair<std::string, std::string>>;

namespace {

constexpr const char* kKey = "secret";

double MillisecondsSince(Clock::time_point start) {
    return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}

void PauseMs(int milliseconds) {
    std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
}

// A new empty directory |name| below |parent|.
fs::path Case(const fs::path& parent, const char* name) {
    const fs::path directory = parent / name;
    std::error_code error;
    fs::remove_all(directory, error);
    fs::create_directories(directory, error);
    return directory;
}

// The names in |directory|, sorted.
Entries Names(const fs::path& directory) {
    Entries names;
    std::error_code error;
    for (const fs::directory_entry& entry : fs::directory_iterator(directory, error)) {
        names.push_back(entry.path().filename().u8string());
    }
    std::sort(names.begin(), names.end());
    return names;
}

// The bytes of the file |path|, or a marker that no file content equals.
std::string Content(const fs::path& path) {
    const auto file = ReadFileBytes(path, 64u << 20);
    return file.Ok() ? file.bytes : std::string("<unreadable: ") + file.error + ">";
}

// Line |index| (from 0) of |text|, without its line feed.
std::string LineOf(const std::string& text, int index) {
    std::size_t start = 0;
    for (int line = 0; line < index; ++line) {
        start = text.find('\n', start);
        if (start == std::string::npos) {
            return std::string();
        }
        ++start;
    }
    return text.substr(start, text.find('\n', start) - start);
}

// A WAV header, so that the stub "decodes" the file, then bytes.
std::string AudioBytes() {
    std::string bytes("RIFF\x24\x00\x00\x00WAVEfmt ", 16);
    for (int i = 0; i < 3000; ++i) {
        bytes.push_back(static_cast<char>((i * 7) % 256));
    }
    return bytes;
}

// A stub server on a free port that wants the key "secret".
struct Server {
    crisp::platform::ChildProcess process;
    int port = 0;

    ~Server() {
        if (process.Id() != 0) {
            crisp::test::EndProcess(process);
        }
    }

    bool Start(std::vector<crisp::platform::EnvironmentChange> extra = {}) {
        port = crisp::test::FreePort();
        extra.push_back({"STUB_LOAD_MS", "0", false});
        extra.push_back({"CRISPASR_API_KEYS", kKey, false});
        return crisp::test::StartStub(process, crisp::test::StubExecutable(), port, std::move(extra)) &&
               crisp::test::WaitForAnswer(port, 15000);
    }

    ServerEndpoint Endpoint() const { return {"127.0.0.1", port}; }

    // How many transcription requests the server has received, answered or not; -1 if it does not say.
    int Received() const {
        const auto parsed = crisp::util::ParseJson(crisp::test::Get(port, "/stub/requests").body);
        const crisp::util::JsonValue* count = parsed.Ok() ? parsed.value.Find("transcriptions") : nullptr;
        return count != nullptr && count->IsNumber() ? static_cast<int>(count->AsNumber()) : -1;
    }

    // Waits until the server has received |count| requests in all.
    bool WaitForRequests(int count, int timeoutMs) const {
        return crisp::test::WaitUntil([&] { return Received() >= count; }, timeoutMs);
    }
};

TranscriptionRequest Ask(const fs::path& source, const Server& server, bool replace = false, Fields fields = {},
                         const std::string& key = kKey) {
    TranscriptionRequest request;
    request.source = source;
    request.endpoint = server.Endpoint();
    request.apiKey = key;
    request.replaceExisting = replace;
    request.fields = std::move(fields);
    return request;
}

TranscriptionOptions FastOptions() {
    TranscriptionOptions options;
    options.progressIntervalMs = 100;
    options.retryDelayMs = 50;
    options.requestTimeoutMs = 2000;
    return options;
}

// A Transcription that the test drives from this thread as the UI thread would, with the calls of
// |wake| counted.
struct Runner {
    std::atomic<int> wakes{0};
    int changes = 0;  // the calls of Update() that returned true
    Transcription transcription;

    explicit Runner(const TranscriptionOptions& options = FastOptions())
        : transcription(options, [this] { ++wakes; }) {}

    const TranscriptionState& State() const { return transcription.State(); }

    // Applies the reports until the transcription is neither running nor saving; false after |timeoutMs|.
    bool Finish(int timeoutMs = 20000) {
        return crisp::test::WaitUntil(
            [this] {
                if (transcription.Update()) {
                    ++changes;
                }
                return State().phase != TranscriptionPhase::kRunning && State().phase != TranscriptionPhase::kSaving;
            },
            timeoutMs);
    }

    // Starts |request| and waits for its end.
    bool Run(TranscriptionRequest request, int timeoutMs = 20000) {
        return transcription.Start(std::move(request)) && Finish(timeoutMs);
    }
};

// Makes the write of |target| fail while it is active: a read-only folder on POSIX, which the
// superuser ignores (Active() is then false); on Windows an existing |target| held open without
// delete sharing, as file_write_test does.
class WriteBlocker {
public:
    explicit WriteBlocker(const fs::path& target) : folder_(target.parent_path()) {
#if defined(_WIN32)
        held_ = CreateFileW(target.c_str(), GENERIC_READ, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        active_ = held_ != INVALID_HANDLE_VALUE;
#else
        chmod(folder_.c_str(), 0555);
        active_ = access(folder_.c_str(), W_OK) != 0;
#endif
    }
    ~WriteBlocker() { Release(); }
    WriteBlocker(const WriteBlocker&) = delete;
    WriteBlocker& operator=(const WriteBlocker&) = delete;

    bool Active() const { return active_; }

    void Release() {
#if defined(_WIN32)
        if (held_ != INVALID_HANDLE_VALUE) {
            CloseHandle(held_);
            held_ = INVALID_HANDLE_VALUE;
        }
#else
        chmod(folder_.c_str(), 0755);
#endif
        active_ = false;
    }

private:
    fs::path folder_;
    bool active_ = false;
#if defined(_WIN32)
    HANDLE held_ = INVALID_HANDLE_VALUE;
#endif
};

bool Done(const TranscriptionState& state) {
    const bool done = state.phase == TranscriptionPhase::kDone && state.failure == TranscriptionFailure::kNone;
    if (!done) {
        std::fprintf(stderr, "transcription_test: phase %d, failure %d, detail: %s\n", static_cast<int>(state.phase),
                     static_cast<int>(state.failure), state.detail.c_str());
    }
    return done;
}

bool Failed(const TranscriptionState& state, TranscriptionFailure failure) {
    const bool failed = state.phase == TranscriptionPhase::kFailed && state.failure == failure;
    if (!failed) {
        std::fprintf(stderr, "transcription_test: phase %d, failure %d instead of %d, detail: %s\n",
                     static_cast<int>(state.phase), static_cast<int>(state.failure), static_cast<int>(failure),
                     state.detail.c_str());
    }
    return failed;
}

HealthReport Health(std::uint64_t epoch, HealthKind kind) {
    HealthReport report;
    report.epoch = epoch;
    report.kind = kind;
    report.backend = kind == HealthKind::kCrispAsr ? "stubby" : std::string();
    return report;
}

}  // namespace

int main() {
    std::error_code fsError;
    if (!fs::is_regular_file(crisp::test::StubExecutable(), fsError)) {
        std::fprintf(stderr, "transcription_test: the stub server is missing\n");
        return 1;
    }
    const fs::path scratch = crisp::test::FreshScratchDirectory("transcription_test.scratch");
    const fs::path sampleJson = crisp::test::TestDataDirectory() / "deployed-parakeet" / "transcript.verbose.json";
    const std::string sampleSrt = Content(crisp::test::TestDataDirectory() / "deployed-parakeet" / "transcript.srt");
    CHECK(sampleSrt.rfind("1\n00:00:00,230 --> 00:00:08,710\n", 0) == 0);

    // The SRT goes next to the source, with the extension replaced.
    {
        CHECK(TranscriptionDestination("dir/talk.mp4") == fs::path("dir/talk.srt"));
        CHECK(TranscriptionDestination("dir/talk.part.wav") == fs::path("dir/talk.part.srt"));
        CHECK(TranscriptionDestination("dir/talk") == fs::path("dir/talk.srt"));
        CHECK(TranscriptionDestination("dir/.talk") == fs::path("dir/.talk.srt"));
        CHECK(TranscriptionDestination("talk.mp3") == fs::path("talk.srt"));
        CHECK(TranscriptionDestination(fs::u8path(u8"音声/動画 の.m4a")) == fs::u8path(u8"音声/動画 の.srt"));
    }

    // The key that the transcription takes from the manager is the one the manager's own checks use:
    // the configured key for a server that was adopted, the profile's for one the GUI started.
    {
        ServerStateMachine adopted(ServerEndpoint{"127.0.0.1", 5114}, "configured");
        adopted.Begin();
        adopted.OnHealth(Health(1, HealthKind::kCrispAsr));
        CHECK(adopted.Status().state == ServerState::kRunning && adopted.Status().adopted);
        CHECK(adopted.ApiKey() == "configured");

        ServerStateMachine started(ServerEndpoint{"127.0.0.1", 5114}, "configured");
        started.Begin();
        started.OnHealth(Health(1, HealthKind::kRefused));
        StartRequest request;
        request.profile.folder = "Japanese";
        request.profile.name = "profile";
        request.profile.port = 5200;
        std::uint64_t epoch = 0;
        for (const ServerCommand& command : started.Start(request)) {
            if (command.kind == ServerCommand::Kind::kMonitor) {
                epoch = command.epoch;
            }
        }
        const ServerCommands spawn = started.OnHealth(Health(epoch, HealthKind::kRefused));
        CHECK(!spawn.empty());
        SpawnReport report;
        report.serial = spawn.empty() ? 0 : spawn[0].serial;
        report.ok = true;
        report.pid = 42;
        report.apiKey = "profilekey";
        started.OnSpawned(report);
        started.OnHealth(Health(epoch, HealthKind::kCrispAsr));
        CHECK(started.Status().state == ServerState::kRunning && started.Status().owned);
        CHECK(started.ApiKey() == "profilekey");
    }

    // The server answers with the deployed Parakeet transcript of tests/data.
    Server sample;
    CHECK(sample.Start({{"STUB_TRANSCRIPT", sampleJson.u8string(), false}}));

    // AC-1: the SRT next to the source equals the one the server's own answer has, byte for byte.
    {
        const fs::path directory = Case(scratch, "end-to-end");
        const fs::path source = directory / fs::u8path(u8"音声 テスト.wav");
        CHECK(crisp::test::WriteFile(source, AudioBytes()));
        Runner runner;
        const int before = sample.Received();
        CHECK(runner.transcription.Start(Ask(source, sample)));
        CHECK(runner.State().phase == TranscriptionPhase::kRunning);  // at once
        CHECK(runner.State().source == source);
        CHECK(runner.State().destination == directory / fs::u8path(u8"音声 テスト.srt"));
        CHECK(runner.Finish());
        const TranscriptionState& state = runner.State();
        CHECK(Done(state));
        CHECK(Content(state.destination) == sampleSrt);
        CHECK(state.attempt == 1 && state.progress == 100 && state.detail.empty());
        CHECK(state.cues == 1 && state.durationMs == 9696);
        CHECK(state.preview == Entries({LineOf(sampleSrt, 2)}));
        CHECK(Names(directory) == Entries({u8"音声 テスト.srt", u8"音声 テスト.wav"}));
        CHECK(sample.Received() - before == 1);
        CHECK(runner.wakes.load() >= 1 && runner.changes >= 1);
        // After the end the clock stops and Update() has nothing to report.
        const std::int64_t endedAt = state.elapsedMs;
        PauseMs(1100);
        CHECK(!runner.transcription.Update());
        CHECK(state.elapsedMs == endedAt);
    }

    // The cues skip a segment without text, the preview holds the first ones with their speaker
    // prefix, and its length follows the option.
    {
        const fs::path directory = Case(scratch, "cues");
        const fs::path json = directory / "speakers.verbose.json";
        CHECK(crisp::test::WriteFile(
            json,
            "{\"task\": \"transcribe\", \"language\": \"en\", \"duration\": 9.0, \"text\": \"x\", \"segments\": ["
            "{\"id\": 0, \"start\": 0.0, \"end\": 1.5, \"text\": \" Hello there.\", \"speaker\": \"A\"},"
            "{\"id\": 1, \"start\": 1.5, \"end\": 2.0, \"text\": \"\"},"
            "{\"id\": 2, \"start\": 2.0, \"end\": 5.0, \"text\": \" General Kenobi.\", \"speaker\": \"B\"},"
            "{\"id\": 3, \"start\": 5.0, \"end\": 9.0, \"text\": \"No label.\"}]}"));
        Server speakers;
        CHECK(speakers.Start({{"STUB_TRANSCRIPT", json.u8string(), false}}));
        const fs::path source = directory / "talk.wav";
        CHECK(crisp::test::WriteFile(source, AudioBytes()));

        Runner runner;
        CHECK(runner.Run(Ask(source, speakers)));
        CHECK(Done(runner.State()));
        CHECK(runner.State().cues == 3 && runner.State().durationMs == 9000);
        CHECK(runner.State().preview ==
              Entries({"(speaker 0)  Hello there.", "(speaker 1)  General Kenobi.", "No label."}));
        CHECK(Content(directory / "talk.srt") ==
              "1\n00:00:00,000 --> 00:00:01,500\n(speaker 0)  Hello there.\n\n"
              "2\n00:00:02,000 --> 00:00:05,000\n(speaker 1)  General Kenobi.\n\n"
              "3\n00:00:05,000 --> 00:00:09,000\nNo label.\n\n");

        TranscriptionOptions two = FastOptions();
        two.previewCues = 2;
        Runner capped(two);
        CHECK(capped.Run(Ask(source, speakers, true)));
        CHECK(Done(capped.State()));
        CHECK(capped.State().cues == 3);
        CHECK(capped.State().preview == Entries({"(speaker 0)  Hello there.", "(speaker 1)  General Kenobi."}));
    }

    // A transcript without segments is a success with no cue: the SRT is empty.
    {
        const fs::path directory = Case(scratch, "silence");
        const fs::path json = directory / "silence.verbose.json";
        CHECK(crisp::test::WriteFile(
            json,
            "{\"task\": \"transcribe\", \"language\": \"en\", \"duration\": 4.0, \"text\": \"\", \"segments\": []}"));
        Server silent;
        CHECK(silent.Start({{"STUB_TRANSCRIPT", json.u8string(), false}}));
        const fs::path source = directory / "talk.wav";
        CHECK(crisp::test::WriteFile(source, AudioBytes()));
        Runner runner;
        CHECK(runner.Run(Ask(source, silent)));
        CHECK(Done(runner.State()));
        CHECK(runner.State().cues == 0 && runner.State().durationMs == 4000 && runner.State().preview.empty());
        CHECK(fs::is_regular_file(directory / "talk.srt", fsError) && Content(directory / "talk.srt").empty());
    }

    // AC-2: while the server works, the state shows its progress and the elapsed time.
    {
        const fs::path directory = Case(scratch, "progress");
        const fs::path source = directory / "talk.wav";
        CHECK(crisp::test::WriteFile(source, AudioBytes()));
        Runner runner;
        CHECK(runner.transcription.Start(Ask(source, sample, false, {{"stub_delay_ms", "2000"}})));
        std::vector<int> seen;
        std::int64_t longestElapsed = 0;
        const bool finished = crisp::test::WaitUntil(
            [&] {
                runner.transcription.Update();
                const TranscriptionState& state = runner.State();
                if (state.phase == TranscriptionPhase::kRunning) {
                    if (state.progress >= 0) {
                        seen.push_back(state.progress);
                    }
                    longestElapsed = std::max(longestElapsed, state.elapsedMs);
                }
                return state.phase != TranscriptionPhase::kRunning;
            },
            20000);
        CHECK(finished);
        CHECK(Done(runner.State()));
        CHECK(!seen.empty());
        CHECK(std::is_sorted(seen.begin(), seen.end()));
        CHECK(!seen.empty() && seen.front() >= 0 && seen.back() <= 100 && seen.front() < seen.back());
        CHECK(longestElapsed >= 1000);  // the clock ran while the state was kRunning
        CHECK(runner.State().elapsedMs >= 1900 && runner.State().progress == 100);
    }

    // AC-3: the failures, each with its kind and nothing written next to the source.
    {
        const fs::path directory = Case(scratch, "failures");
        const fs::path audio = directory / "talk.wav";
        CHECK(crisp::test::WriteFile(audio, AudioBytes()));
        const Entries onlyAudio = {"talk.wav"};
        Runner runner;

        // 400: the server's message, and no second try.
        {
            const fs::path notAudio = directory / "notes.mp3";
            CHECK(crisp::test::WriteFile(notAudio, "just text"));
            const int before = sample.Received();
            CHECK(runner.Run(Ask(notAudio, sample)));
            CHECK(Failed(runner.State(), TranscriptionFailure::kBadRequest));
            CHECK(runner.State().detail.find("failed to decode audio") != std::string::npos);
            CHECK(runner.State().attempt == 1 && sample.Received() - before == 1);
            CHECK(Names(directory) == Entries({"notes.mp3", "talk.wav"}));
            fs::remove(notAudio, fsError);
        }
        // 401: a wrong key.
        {
            const int before = sample.Received();
            CHECK(runner.Run(Ask(audio, sample, false, {}, "wrong")));
            CHECK(Failed(runner.State(), TranscriptionFailure::kUnauthorized));
            CHECK(runner.State().attempt == 1 && sample.Received() - before == 1);
            CHECK(Names(directory) == onlyAudio);
        }
        // 503: not retried, because the page decides whether to wait.
        {
            const int before = sample.Received();
            CHECK(runner.Run(Ask(audio, sample, false, {{"stub_reply", "503"}})));
            CHECK(Failed(runner.State(), TranscriptionFailure::kServerBusy));
            CHECK(runner.State().attempt == 1 && sample.Received() - before == 1);
            CHECK(Names(directory) == onlyAudio);
        }
        // 500 twice: one retry, then the failure with the server's message.
        {
            const int before = sample.Received();
            CHECK(runner.Run(Ask(audio, sample, false, {{"stub_reply", "500"}})));
            CHECK(Failed(runner.State(), TranscriptionFailure::kServerError));
            CHECK(runner.State().detail.find("internal error") != std::string::npos);
            CHECK(runner.State().attempt == 2 && sample.Received() - before == 2);
            CHECK(Names(directory) == onlyAudio);
        }
        // An answer that is not a transcript.
        {
            const int before = sample.Received();
            CHECK(runner.Run(Ask(audio, sample, false, {{"stub_reply", "garbage"}})));
            CHECK(Failed(runner.State(), TranscriptionFailure::kUnexpectedAnswer));
            CHECK(runner.State().attempt == 1 && sample.Received() - before == 1);
            CHECK(Names(directory) == onlyAudio);
        }
        // No server listens.
        {
            TranscriptionRequest request = Ask(audio, sample);
            request.endpoint = {"127.0.0.1", crisp::test::FreePort()};
            CHECK(runner.Run(request));
            CHECK(Failed(runner.State(), TranscriptionFailure::kServerUnreachable));
            CHECK(runner.State().attempt == 1);
            CHECK(Names(directory) == onlyAudio);
        }
        // A source that is missing, or is not a file: nothing is sent.
        {
            const int before = sample.Received();
            CHECK(runner.Run(Ask(directory / "missing.wav", sample)));
            CHECK(Failed(runner.State(), TranscriptionFailure::kSourceUnreadable));
            CHECK(runner.State().detail.rfind("Could not open ", 0) == 0);
            CHECK(runner.State().attempt == 0);
            const fs::path folder = directory / "folder.wav";
            fs::create_directories(folder, fsError);
            CHECK(runner.Run(Ask(folder, sample)));
            CHECK(Failed(runner.State(), TranscriptionFailure::kSourceUnreadable));
            CHECK(runner.State().detail.find("is not a regular file") != std::string::npos);
            CHECK(sample.Received() == before);
            fs::remove(folder, fsError);
            CHECK(Names(directory) == onlyAudio);
        }
#if !defined(_WIN32)
        // A pipe is refused without being opened: opening one waits for the other end, so a lookup
        // that is missing would block here (a helper thread frees it after 3 s, to fail instead of hang).
        {
            const fs::path pipe = directory / "pipe.wav";
            CHECK(mkfifo(pipe.c_str(), 0600) == 0);
            std::atomic<bool> finished{false};
            std::thread rescue([&] {
                for (int i = 0; i < 30 && !finished.load(); ++i) {
                    PauseMs(100);
                }
                while (!finished.load()) {
                    const int writer = open(pipe.c_str(), O_WRONLY | O_NONBLOCK);
                    if (writer >= 0) {
                        close(writer);
                        return;
                    }
                    PauseMs(50);
                }
            });
            const int before = sample.Received();
            CHECK(runner.Run(Ask(pipe, sample), 2000));
            CHECK(Failed(runner.State(), TranscriptionFailure::kSourceUnreadable));
            CHECK(runner.State().detail.find("is not a regular file") != std::string::npos);
            CHECK(sample.Received() == before);
            finished = true;
            rescue.join();
            fs::remove(pipe, fsError);
            CHECK(Names(directory) == onlyAudio);
        }
#endif
        // A source over the upload limit: refused before sending, and before the question whether
        // the SRT may be replaced, since the file could not be sent in any case.
        {
            CHECK(crisp::test::WriteFile(directory / "talk.srt", "keep me"));
            TranscriptionOptions small = FastOptions();
            small.maxUploadBytes = 1000;
            Runner limited(small);
            const int before = sample.Received();
            CHECK(limited.Run(Ask(audio, sample)));
            CHECK(Failed(limited.State(), TranscriptionFailure::kTooLarge));
            CHECK(limited.State().attempt == 0 && sample.Received() == before);
            CHECK(Content(directory / "talk.srt") == "keep me");
            fs::remove(directory / "talk.srt", fsError);
            CHECK(Names(directory) == onlyAudio);
        }
        // An SRT that exists, without permission to replace it: refused before sending, and untouched.
        {
            CHECK(crisp::test::WriteFile(directory / "talk.srt", "keep me"));
            const int before = sample.Received();
            CHECK(runner.Run(Ask(audio, sample)));
            CHECK(Failed(runner.State(), TranscriptionFailure::kDestinationExists));
            CHECK(runner.State().attempt == 0 && sample.Received() == before);
            CHECK(Content(directory / "talk.srt") == "keep me");
            CHECK(Names(directory) == Entries({"talk.srt", "talk.wav"}));
            fs::remove(directory / "talk.srt", fsError);
        }
        // Even with permission, the source is never replaced by its own transcript.
        {
            const fs::path same = directory / "same.srt";
            CHECK(crisp::test::WriteFile(same, AudioBytes()));
            const int before = sample.Received();
            CHECK(runner.Run(Ask(same, sample, true)));
            CHECK(Failed(runner.State(), TranscriptionFailure::kDestinationExists));
            CHECK(sample.Received() == before);
            CHECK(Content(same) == AudioBytes());
            fs::remove(same, fsError);
        }
        // An SRT that appears while the server works is not replaced either.
        {
            const int before = sample.Received();
            CHECK(runner.transcription.Start(Ask(audio, sample, false, {{"stub_delay_ms", "1000"}})));
            CHECK(sample.WaitForRequests(before + 1, 10000));
            PauseMs(100);
            CHECK(crisp::test::WriteFile(directory / "talk.srt", "mine"));
            CHECK(runner.Finish());
            CHECK(Failed(runner.State(), TranscriptionFailure::kDestinationExists));
            CHECK(runner.State().srtKept && runner.State().srt == sampleSrt);  // the work is kept
            CHECK(Content(directory / "talk.srt") == "mine");
            CHECK(Names(directory) == Entries({"talk.srt", "talk.wav"}));
            // A retry follows the same rule: refused while the file is there, and once it is gone the
            // kept SRT is written.
            CHECK(runner.transcription.RetryWrite());
            CHECK(runner.Finish());
            CHECK(Failed(runner.State(), TranscriptionFailure::kDestinationExists) && runner.State().srtKept);
            CHECK(Content(directory / "talk.srt") == "mine");
            fs::remove(directory / "talk.srt", fsError);
            CHECK(runner.transcription.RetryWrite());
            CHECK(runner.Finish());
            CHECK(Done(runner.State()) && runner.State().destination == directory / "talk.srt");
            CHECK(Content(directory / "talk.srt") == sampleSrt);
            CHECK(sample.Received() - before == 1);  // no second request
            fs::remove(directory / "talk.srt", fsError);
        }
        // A folder in the SRT's place is refused before any request, with permission to replace or without.
        {
            fs::create_directories(directory / "talk.srt", fsError);
            CHECK(crisp::test::WriteFile(directory / "talk.srt" / "inside.txt", "inside"));
            const int before = sample.Received();
            for (const bool replace : {false, true}) {
                CHECK(runner.Run(Ask(audio, sample, replace)));
                CHECK(Failed(runner.State(), TranscriptionFailure::kDestinationNotFile));
                CHECK(runner.State().attempt == 0 && sample.Received() == before);
                CHECK(!runner.State().srtKept && runner.State().srt.empty());
            }
            CHECK(Content(directory / "talk.srt" / "inside.txt") == "inside");
            CHECK(Names(directory) == Entries({"talk.srt", "talk.wav"}));
            fs::remove_all(directory / "talk.srt", fsError);
        }
        // The same when the folder appears while the server works: the finished SRT is kept.
        {
            const int before = sample.Received();
            CHECK(runner.transcription.Start(Ask(audio, sample, false, {{"stub_delay_ms", "1000"}})));
            CHECK(sample.WaitForRequests(before + 1, 10000));
            PauseMs(100);
            fs::create_directories(directory / "talk.srt", fsError);
            CHECK(runner.Finish());
            CHECK(Failed(runner.State(), TranscriptionFailure::kDestinationNotFile));
            CHECK(runner.State().srtKept && runner.State().srt == sampleSrt && runner.State().cues == 1);
            CHECK(sample.Received() - before == 1);
            fs::remove_all(directory / "talk.srt", fsError);
            CHECK(Names(directory) == onlyAudio);
        }
        // The runner is still usable after all of it.
        CHECK(runner.Run(Ask(audio, sample)));
        CHECK(Done(runner.State()));
        CHECK(Content(directory / "talk.srt") == sampleSrt);
    }

    // 413 from the server: its upload limit is below the file.
    {
        const fs::path directory = Case(scratch, "limited");
        const fs::path source = directory / "talk.wav";
        CHECK(crisp::test::WriteFile(source, AudioBytes()));
        Server limited;
        CHECK(limited.Start({{"STUB_MAX_BODY_BYTES", "1000", false}}));
        Runner runner;
        CHECK(runner.Run(Ask(source, limited)));
        CHECK(Failed(runner.State(), TranscriptionFailure::kTooLarge));
        CHECK(Names(directory) == Entries({"talk.wav"}));
    }

    // 500 once, then success: the retry succeeds, and the state says it was the second attempt.
    {
        const fs::path directory = Case(scratch, "retry");
        const fs::path source = directory / "talk.wav";
        CHECK(crisp::test::WriteFile(source, AudioBytes()));
        Server flaky;
        CHECK(flaky.Start({{"STUB_TRANSCRIPT", sampleJson.u8string(), false}, {"STUB_FAIL_FIRST", "500:1", false}}));
        Runner runner;
        CHECK(runner.Run(Ask(source, flaky)));
        CHECK(Done(runner.State()));
        CHECK(runner.State().attempt == 2 && flaky.Received() == 2);
        CHECK(Content(directory / "talk.srt") == sampleSrt);
        CHECK(Names(directory) == Entries({"talk.srt", "talk.wav"}));
    }

    // The retry starts with the progress unknown, not with where the failed attempt had got to.
    {
        const fs::path directory = Case(scratch, "retry-progress");
        const fs::path source = directory / "talk.wav";
        CHECK(crisp::test::WriteFile(source, AudioBytes()));
        Server flaky;
        CHECK(flaky.Start({{"STUB_TRANSCRIPT", sampleJson.u8string(), false}, {"STUB_FAIL_FIRST", "500:1", false}}));
        Runner runner;
        CHECK(runner.transcription.Start(Ask(source, flaky, false, {{"stub_delay_ms", "1200"}})));
        int furthestOfFirst = -1;
        int firstOfSecond = -2;
        CHECK(crisp::test::WaitUntil(
            [&] {
                runner.transcription.Update();
                const TranscriptionState& state = runner.State();
                if (state.attempt == 1) {
                    furthestOfFirst = std::max(furthestOfFirst, state.progress);
                } else if (state.attempt == 2 && firstOfSecond == -2) {
                    firstOfSecond = state.progress;
                }
                return state.phase != TranscriptionPhase::kRunning;
            },
            20000));
        CHECK(Done(runner.State()) && runner.State().attempt == 2 && flaky.Received() == 2);
        CHECK(furthestOfFirst >= 50);  // the server got far before it failed
        CHECK(firstOfSecond < 50);     // and the retry did not start there
    }

    // A cancel during the pause before the retry ends it at once.
    {
        const fs::path directory = Case(scratch, "retry-cancel");
        const fs::path source = directory / "talk.wav";
        CHECK(crisp::test::WriteFile(source, AudioBytes()));
        Server flaky;
        CHECK(flaky.Start({{"STUB_FAIL_FIRST", "500:1", false}}));
        TranscriptionOptions slow = FastOptions();
        slow.retryDelayMs = 20000;
        Runner runner(slow);
        const auto begin = Clock::now();
        CHECK(runner.transcription.Start(Ask(source, flaky)));
        CHECK(flaky.WaitForRequests(1, 10000));
        PauseMs(300);
        runner.transcription.Cancel();
        CHECK(runner.Finish(5000));
        CHECK(Failed(runner.State(), TranscriptionFailure::kCancelled));
        CHECK(MillisecondsSince(begin) < 10000);
        CHECK(flaky.Received() == 1);
        CHECK(Names(directory) == Entries({"talk.wav"}));
    }

    // A request that takes longer than the timeout.
    {
        const fs::path directory = Case(scratch, "timeout");
        const fs::path source = directory / "talk.wav";
        CHECK(crisp::test::WriteFile(source, AudioBytes()));
        Server slow;
        CHECK(slow.Start());
        TranscriptionOptions options = FastOptions();
        options.connectTimeoutMs = 300;  // libcurl's limit is this plus the request timeout
        options.transcribeTimeoutMs = 1000;
        Runner runner(options);
        CHECK(runner.Run(Ask(source, slow, false, {{"stub_delay_ms", "20000"}}), 15000));
        CHECK(Failed(runner.State(), TranscriptionFailure::kTimeout));
        CHECK(runner.State().attempt == 1);       // a timeout is not retried
        CHECK(runner.State().elapsedMs < 12000);  // WinHTTP's timer may end the wait seconds late
        CHECK(Names(directory) == Entries({"talk.wav"}));
    }

    // AC-4: an SRT that exists is replaced, in one step, when the request allows it.
    {
        const fs::path directory = Case(scratch, "replace");
        const fs::path source = directory / "talk.wav";
        const fs::path target = directory / "talk.srt";
        CHECK(crisp::test::WriteFile(source, AudioBytes()));
        CHECK(crisp::test::WriteFile(target, "the old subtitles, longer than none"));
#if !defined(_WIN32)
        struct stat before;
        CHECK(stat(target.c_str(), &before) == 0);
        // A reader that has the old file open still sees all of the old bytes afterwards.
        std::FILE* reader = std::fopen(target.c_str(), "rb");
        CHECK(reader != nullptr);
#endif
        Runner runner;
        CHECK(runner.Run(Ask(source, sample, true)));
        CHECK(Done(runner.State()));
        CHECK(Content(target) == sampleSrt);
        CHECK(Names(directory) == Entries({"talk.srt", "talk.wav"}));
#if !defined(_WIN32)
        struct stat after;
        CHECK(stat(target.c_str(), &after) == 0);
        CHECK(before.st_ino != after.st_ino);
        if (reader != nullptr) {
            char buffer[128];
            const std::size_t count = std::fread(buffer, 1, sizeof(buffer), reader);
            CHECK(std::string(buffer, count) == "the old subtitles, longer than none");
            std::fclose(reader);
        }
#endif
    }

    // AC-5: when the write fails after the work, the finished SRT is kept. Retrying writes it once the
    // cause is gone, SaveTo() writes it elsewhere, and both are refused when there is nothing to save.
    {
        const fs::path directory = Case(scratch, "kept");
        const fs::path rescue = Case(scratch, "kept-rescue");
        const fs::path source = directory / "talk.wav";
        const fs::path target = directory / "talk.srt";
        CHECK(crisp::test::WriteFile(source, AudioBytes()));
        // Blocks the write of the SRT and runs a transcription that ends in that failure.
        const auto failWrite = [&](Runner& runner, WriteBlocker*& blocker, std::unique_ptr<WriteBlocker>& owner) {
            std::error_code ignored;
            fs::remove(target, ignored);
#if defined(_WIN32)
            CHECK(crisp::test::WriteFile(target, "old"));  // held open below, so replacing it fails
#endif
            owner = std::make_unique<WriteBlocker>(target);
            blocker = owner.get();
            CHECK(runner.Run(Ask(source, sample, true)));
            return blocker->Active();
        };

        Runner runner;
        std::unique_ptr<WriteBlocker> owner;
        WriteBlocker* blocker = nullptr;
        const int before = sample.Received();
        const bool active = failWrite(runner, blocker, owner);
        if (!active) {
            std::fprintf(stderr, "transcription_test: a write cannot be made to fail here (superuser?); "
                                 "the cases of a kept SRT are skipped\n");
        } else {
            const TranscriptionState& state = runner.State();
            CHECK(Failed(state, TranscriptionFailure::kCannotWrite));
#if defined(_WIN32)
            CHECK(state.detail.rfind("Could not replace ", 0) == 0);
#else
            CHECK(state.detail.rfind("Could not create the temporary file ", 0) == 0);
#endif
            CHECK(state.srtKept && state.srt == sampleSrt);
            CHECK(state.cues == 1 && state.durationMs == 9696 && state.preview == Entries({LineOf(sampleSrt, 2)}));
            CHECK(state.progress == -1 && state.attempt == 1 && sample.Received() - before == 1);

            // A retry while the cause remains fails again, keeps the SRT, and sends nothing.
            CHECK(runner.transcription.RetryWrite());
            CHECK(state.phase == TranscriptionPhase::kSaving);
            CHECK(!runner.transcription.RetryWrite());                       // not twice at once
            CHECK(!runner.transcription.SaveTo(rescue / "twice.srt"));       // nor another save meanwhile
            CHECK(!runner.transcription.Start(Ask(source, sample, true)));  // nor a transcription
            CHECK(runner.Finish());
            CHECK(Failed(state, TranscriptionFailure::kCannotWrite));
            CHECK(state.srtKept && state.srt == sampleSrt && state.cues == 1);
            CHECK(sample.Received() - before == 1);
            CHECK(!fs::exists(rescue / "twice.srt", fsError));

            // Once the cause is gone, a retry writes the SRT to the destination.
            blocker->Release();
            CHECK(runner.transcription.RetryWrite());
            CHECK(runner.Finish());
            CHECK(Done(state));
            CHECK(state.destination == target && Content(target) == sampleSrt);
            CHECK(!state.srtKept && state.srt.empty() && state.progress == 100);
            CHECK(state.cues == 1 && state.preview == Entries({LineOf(sampleSrt, 2)}));
            CHECK(state.detail.empty());
            CHECK(Names(directory) == Entries({"talk.srt", "talk.wav"}));
            CHECK(sample.Received() - before == 1);
            // Nothing is left to save now.
            CHECK(!runner.transcription.RetryWrite());
            CHECK(!runner.transcription.SaveTo(rescue / "nothing.srt"));
            CHECK(!fs::exists(rescue / "nothing.srt", fsError));
        }
        owner.reset();

        // The same failure again: the SRT is saved elsewhere, into folders that do not exist yet.
        if (active) {
            Runner second;
            std::unique_ptr<WriteBlocker> secondOwner;
            WriteBlocker* secondBlocker = nullptr;
            CHECK(failWrite(second, secondBlocker, secondOwner));
            const TranscriptionState& state = second.State();
            CHECK(Failed(state, TranscriptionFailure::kCannotWrite) && state.srtKept);
            const fs::path saved = rescue / "deeper" / "still" / "talk.srt";
            CHECK(second.transcription.SaveTo(saved));
            CHECK(state.phase == TranscriptionPhase::kSaving);
            CHECK(second.Finish());
            CHECK(Done(state));
            CHECK(state.destination == saved && Content(saved) == sampleSrt);
            CHECK(!state.srtKept && state.srt.empty() && state.progress == 100 && state.detail.empty());
            CHECK(state.source == source && state.cues == 1);
            CHECK(Names(saved.parent_path()) == Entries({"talk.srt"}));
#if !defined(_WIN32)
            CHECK(!fs::exists(target, fsError));  // the destination itself was never written
#endif
            secondOwner.reset();
        }

        // A save replaces a file at its path in one step, like the first write.
        if (active) {
            Runner third;
            std::unique_ptr<WriteBlocker> thirdOwner;
            WriteBlocker* thirdBlocker = nullptr;
            CHECK(failWrite(third, thirdBlocker, thirdOwner));
            const fs::path existing = rescue / "existing.srt";
            CHECK(crisp::test::WriteFile(existing, "the old subtitles"));
#if !defined(_WIN32)
            struct stat oldInfo;
            CHECK(stat(existing.c_str(), &oldInfo) == 0);
            std::FILE* reader = std::fopen(existing.c_str(), "rb");
            CHECK(reader != nullptr);
#endif
            CHECK(third.transcription.SaveTo(existing));
            CHECK(third.Finish());
            CHECK(Done(third.State()) && third.State().destination == existing);
            CHECK(Content(existing) == sampleSrt);
#if !defined(_WIN32)
            struct stat newInfo;
            CHECK(stat(existing.c_str(), &newInfo) == 0);
            CHECK(oldInfo.st_ino != newInfo.st_ino);  // a new file took the name
            if (reader != nullptr) {
                char buffer[64];
                const std::size_t count = std::fread(buffer, 1, sizeof(buffer), reader);
                CHECK(std::string(buffer, count) == "the old subtitles");  // the reader still has the old one
                std::fclose(reader);
            }
#endif
            CHECK(Names(rescue) == Entries({"deeper", "existing.srt"}));
            thirdOwner.reset();
        }

        // Never the source itself, and never anything that is not a file.
        if (active) {
            Runner fourth;
            std::unique_ptr<WriteBlocker> fourthOwner;
            WriteBlocker* fourthBlocker = nullptr;
            CHECK(failWrite(fourth, fourthBlocker, fourthOwner));
            fs::create_directories(rescue / "folder.srt", fsError);
            CHECK(fourth.transcription.SaveTo(rescue / "folder.srt"));
            CHECK(fourth.Finish());
            CHECK(Failed(fourth.State(), TranscriptionFailure::kDestinationNotFile) && fourth.State().srtKept);
            CHECK(fourth.State().detail.find("is not a file") != std::string::npos);  // the new reason
            CHECK(fourth.transcription.SaveTo(source));
            CHECK(fourth.Finish());
            CHECK(Failed(fourth.State(), TranscriptionFailure::kDestinationExists) && fourth.State().srtKept);
            CHECK(fourth.State().detail.find("is the source file itself") != std::string::npos);
            CHECK(Content(source) == AudioBytes());
            fourthOwner.reset();
        }
    }

    // Saving and retrying are refused while a transcription runs, and when no SRT is kept.
    {
        const fs::path directory = Case(scratch, "no-save");
        const fs::path rescue = Case(scratch, "no-save-rescue");
        const fs::path source = directory / "talk.wav";
        CHECK(crisp::test::WriteFile(source, AudioBytes()));
        Runner runner;
        CHECK(!runner.transcription.RetryWrite() && !runner.transcription.SaveTo(rescue / "idle.srt"));
        CHECK(runner.transcription.Start(Ask(source, sample, false, {{"stub_delay_ms", "1000"}})));
        CHECK(!runner.transcription.RetryWrite());
        CHECK(!runner.transcription.SaveTo(rescue / "running.srt"));
        CHECK(runner.State().phase == TranscriptionPhase::kRunning);
        CHECK(runner.Finish());
        CHECK(Done(runner.State()));
        CHECK(!runner.transcription.RetryWrite() && !runner.transcription.SaveTo(rescue / "done.srt"));
        CHECK(runner.Run(Ask(directory / "missing.wav", sample)));
        CHECK(Failed(runner.State(), TranscriptionFailure::kSourceUnreadable) && !runner.State().srtKept);
        CHECK(!runner.transcription.RetryWrite() && !runner.transcription.SaveTo(rescue / "failed.srt"));
        CHECK(Names(rescue).empty());
    }

    // After a failure the progress is unknown, whatever the last reading was.
    {
        const fs::path directory = Case(scratch, "failed-progress");
        const fs::path source = directory / "talk.wav";
        CHECK(crisp::test::WriteFile(source, AudioBytes()));
        Server flaky;
        CHECK(flaky.Start({{"STUB_FAIL_FIRST", "500:2", false}}));
        Runner runner;
        CHECK(runner.transcription.Start(Ask(source, flaky, false, {{"stub_delay_ms", "900"}})));
        int furthest = -1;
        CHECK(crisp::test::WaitUntil(
            [&] {
                runner.transcription.Update();
                furthest = std::max(furthest, runner.State().progress);
                return runner.State().phase != TranscriptionPhase::kRunning;
            },
            20000));
        CHECK(Failed(runner.State(), TranscriptionFailure::kServerError));
        CHECK(furthest >= 30);  // the server did report progress before it failed
        CHECK(runner.State().progress == -1);
    }

    // One transcription at a time; the next one starts clean after the end of the last; a cancel
    // with nothing running does nothing.
    {
        const fs::path directory = Case(scratch, "sequence");
        const fs::path first = directory / "first.wav";
        const fs::path second = directory / "second.wav";
        CHECK(crisp::test::WriteFile(first, AudioBytes()));
        CHECK(crisp::test::WriteFile(second, AudioBytes()));
        Runner runner;
        CHECK(!runner.transcription.Update() && runner.State().phase == TranscriptionPhase::kIdle);
        runner.transcription.Cancel();
        CHECK(runner.State().phase == TranscriptionPhase::kIdle);

        const int before = sample.Received();
        CHECK(runner.transcription.Start(Ask(first, sample, false, {{"stub_delay_ms", "800"}})));
        CHECK(!runner.transcription.Start(Ask(second, sample)));  // ignored while one runs
        CHECK(runner.State().source == first);
        CHECK(runner.Finish());
        CHECK(Done(runner.State()) && runner.State().source == first);
        CHECK(sample.Received() - before == 1);
        CHECK(Names(directory) == Entries({"first.srt", "first.wav", "second.wav"}));

        runner.transcription.Cancel();
        runner.transcription.Update();
        CHECK(Done(runner.State()));

        CHECK(runner.transcription.Start(Ask(second, sample)));
        const TranscriptionState& state = runner.State();
        CHECK(state.phase == TranscriptionPhase::kRunning && state.source == second);
        CHECK(state.destination == directory / "second.srt");
        CHECK(state.attempt == 0 && state.progress == -1 && state.cues == 0 && state.preview.empty());
        CHECK(state.failure == TranscriptionFailure::kNone && state.detail.empty());
        CHECK(runner.Finish());
        CHECK(Done(state) && state.attempt == 1 && state.cues == 1);
        CHECK(Names(directory) == Entries({"first.srt", "first.wav", "second.srt", "second.wav"}));
    }

    // AC-5: a cancel during a transcription ends it within 2.5 s; nothing is written.
    {
        const fs::path directory = Case(scratch, "cancel");
        const fs::path source = directory / "talk.wav";
        CHECK(crisp::test::WriteFile(source, AudioBytes()));
        Server slow;
        CHECK(slow.Start({{"STUB_TRANSCRIPT", sampleJson.u8string(), false}}));
        Runner runner;
        CHECK(runner.transcription.Start(Ask(source, slow, false, {{"stub_delay_ms", "10000"}})));
        CHECK(slow.WaitForRequests(1, 10000));
        PauseMs(300);
        CHECK(runner.State().phase == TranscriptionPhase::kRunning);
        const auto cancelled = Clock::now();
        runner.transcription.Cancel();
        CHECK(runner.Finish(5000));
        CHECK(MillisecondsSince(cancelled) < 2500);
        CHECK(Failed(runner.State(), TranscriptionFailure::kCancelled));
        CHECK(runner.State().detail.empty() && runner.State().attempt == 1);
        CHECK(Names(directory) == Entries({"talk.wav"}));  // no SRT, no temporary file
        // The runner can start again after a cancel.
        Server fast;
        CHECK(fast.Start({{"STUB_TRANSCRIPT", sampleJson.u8string(), false}}));
        CHECK(runner.Run(Ask(source, fast)));
        CHECK(Done(runner.State()));
        CHECK(Content(directory / "talk.srt") == sampleSrt);
    }

    // AC-6: Shutdown() during a transcription returns within 2.5 s, and the wake callback is not
    // called afterwards; so does the destructor.
    {
        const fs::path directory = Case(scratch, "shutdown");
        const fs::path source = directory / "talk.wav";
        CHECK(crisp::test::WriteFile(source, AudioBytes()));
        Server slow;
        CHECK(slow.Start());
        {
            Runner runner;
            CHECK(runner.transcription.Start(Ask(source, slow, false, {{"stub_delay_ms", "10000"}})));
            CHECK(slow.WaitForRequests(1, 10000));
            PauseMs(300);
            const auto begin = Clock::now();
            runner.transcription.Shutdown();
            CHECK(MillisecondsSince(begin) < 2500);
            const int wakesAtReturn = runner.wakes.load();
            CHECK(wakesAtReturn >= 1);
            PauseMs(1500);
            CHECK(runner.wakes.load() == wakesAtReturn);
            runner.transcription.Update();
            CHECK(Failed(runner.State(), TranscriptionFailure::kCancelled));
            CHECK(!runner.transcription.Start(Ask(source, slow)));  // not running, yet closed for good
            CHECK(runner.State().phase == TranscriptionPhase::kFailed);
            runner.transcription.Shutdown();  // again: nothing happens
            CHECK(runner.wakes.load() == wakesAtReturn);
        }
        {
            const int before = slow.Received();
            Clock::time_point begin;
            {
                Runner scoped;
                CHECK(scoped.transcription.Start(Ask(source, slow, false, {{"stub_delay_ms", "10000"}})));
                CHECK(slow.WaitForRequests(before + 1, 10000));
                PauseMs(300);
                begin = Clock::now();
            }  // the destructor cancels the request and joins the threads
            CHECK(MillisecondsSince(begin) < 2500);
        }
        CHECK(Names(directory) == Entries({"talk.wav"}));
    }

    // The manager's endpoint and key are what the transcription needs: an adopted server.
    {
        const fs::path directory = Case(scratch, "manager");
        const fs::path source = directory / "talk.wav";
        CHECK(crisp::test::WriteFile(source, AudioBytes()));
        ServerManagerOptions options;
        options.endpoint = sample.Endpoint();
        options.apiKey = kKey;
        options.pollIntervalMs = 200;
        options.requestTimeoutMs = 2000;
        ServerManager manager(options, nullptr);
        manager.Begin();
        CHECK(crisp::test::WaitUntil(
            [&] {
                manager.Update();
                return manager.Status().state == ServerState::kRunning;
            },
            15000));
        CHECK(manager.Status().adopted && manager.ApiKey() == kKey);
        TranscriptionRequest request;
        request.source = source;
        request.endpoint = manager.Status().endpoint;
        request.apiKey = manager.ApiKey();
        Runner runner;
        CHECK(runner.Run(request));
        CHECK(Done(runner.State()));
        CHECK(Content(directory / "talk.srt") == sampleSrt);
        manager.Shutdown();
    }

    return crisp::test::Result();
}
