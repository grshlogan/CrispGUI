// api_probe: checks the GUI's ApiClient against a running CrispASR server and keeps the server's
// answers. Windows deliveries carry it for the Phase 4 acceptance (tools/windows/CHECKLIST.md),
// and its answers become test fixtures (docs/OPEN-QUESTIONS.md, OQ-4 item 3). It talks only to
// --host:--port, reads the audio file, and writes only into its output folder.
//
// Usage: api_probe [options] <audio file>
//   --host <host>              default 127.0.0.1
//   --port <port>              default 5114
//   --key <key>                default "default", sent as "Authorization: Bearer <key>"
//   --language <code>          sent as the language field; otherwise the server's startup value
//   --window <offset>,<length> also transcribe this window, in milliseconds
//   --timeout-s <seconds>      limit of one transcription request, default 1800
//   --out <folder>             default probe-results/<UTC time> next to the executable
//   --skip-errors              skip the checks of the error answers
//   --skip-cancel              skip the cancellation check
// Exit code: 0 when no check failed, 1 when one did, 2 for a usage error.

#include "core/api_client.h"
#include "core/transcript.h"
#include "platform/files.h"
#include "platform/http.h"
#include "platform/paths.h"
#include "util/json.h"
#include "util/sha256.h"

#if defined(_WIN32)
#include "platform/win32_text.h"
#endif

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <mutex>
#include <string>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

namespace {

namespace fs = std::filesystem;
using crisp::core::ApiClient;
using crisp::core::ApiClientOptions;
using crisp::core::ApiError;
using crisp::core::ApiErrorKind;
using crisp::core::ApiReply;
using crisp::core::ProgressReport;
using crisp::core::Transcript;
using crisp::core::TranscribeRequest;
using crisp::util::JsonValue;
using Clock = std::chrono::steady_clock;

#if defined(_WIN32)
constexpr const char* kNewline = "\r\n";
#else
constexpr const char* kNewline = "\n";
#endif

constexpr const char* kNotAudio = "CrispGUI api_probe: this file is not audio.\n";

struct Options {
    std::string host = "127.0.0.1";
    int port = 5114;
    std::string key = "default";
    std::string language;
    bool window = false;
    std::int64_t windowOffsetMs = 0;
    std::int64_t windowDurationMs = 0;
    int timeoutSeconds = 1800;
    fs::path out;
    bool skipErrors = false;
    bool skipCancel = false;
    fs::path audio;
};

fs::path PathFromUtf8(const std::string& text) {
#if defined(_WIN32)
    return fs::path(crisp::platform::win32::Widen(text));
#else
    return fs::path(text);
#endif
}

std::string Utf8(const fs::path& path) {
    return path.u8string();
}

double SecondsSince(Clock::time_point start) {
    return std::chrono::duration<double>(Clock::now() - start).count();
}

std::string Fixed(double value, int decimals) {
    char text[64];
    std::snprintf(text, sizeof(text), "%.*f", decimals, value);
    return text;
}

std::string UtcStamp(bool compact) {
    const std::time_t now = std::time(nullptr);
    const std::tm* utc = std::gmtime(&now);
    char text[32];
    std::strftime(text, sizeof(text), compact ? "%Y%m%dT%H%M%SZ" : "%Y-%m-%dT%H:%M:%SZ", utc);
    return text;
}

std::string Clip(const std::string& text, std::size_t limit) {
    std::string flat;
    for (const char c : text) {
        flat.push_back(c == '\r' || c == '\n' || c == '\t' ? ' ' : c);
    }
    if (flat.size() <= limit) {
        return flat;
    }
    std::size_t end = limit;
    while (end > 0 && (static_cast<unsigned char>(flat[end]) & 0xC0) == 0x80) {
        --end;
    }
    return flat.substr(0, end) + "...";
}

std::string Describe(const ApiError& error) {
    return std::string(crisp::core::ApiErrorKindName(error.kind)) +
           (error.status != 0 ? " (HTTP " + std::to_string(error.status) + ")" : std::string()) + ": " + error.message;
}

std::string Timestamp(std::int64_t ms) {
    char text[32];
    std::snprintf(text, sizeof(text), "%02lld:%02lld.%03lld", static_cast<long long>(ms / 60000),
                  static_cast<long long>(ms / 1000 % 60), static_cast<long long>(ms % 1000));
    return text;
}

// The lines go to the console and to report.txt as they happen; the checks also go to
// report.json at the end.
class Report {
public:
    explicit Report(fs::path folder) : folder_(std::move(folder)) {
        text_ = crisp::platform::OpenFileForWriting(folder_ / "report.txt");
        checks_ = JsonValue::MakeArray();
        files_ = JsonValue::MakeArray();
    }
    ~Report() {
        if (text_ != nullptr) {
            std::fclose(text_);
        }
    }
    Report(const Report&) = delete;
    Report& operator=(const Report&) = delete;

    bool Ok() const { return text_ != nullptr; }
    int Failures() const { return failures_; }

    void Line(const std::string& text) {
        std::printf("%s\n", text.c_str());
        std::fflush(stdout);
        if (text_ != nullptr) {
            std::fputs((text + kNewline).c_str(), text_);
            std::fflush(text_);
        }
    }

    // verdict: PASS, FAIL, INFO, or SKIP.
    void Check(const std::string& id, const std::string& name, const std::string& verdict, const std::string& detail,
               double seconds) {
        if (verdict == "FAIL") {
            ++failures_;
        }
        Line("[" + id + "] " + verdict + " " + name + (seconds >= 0 ? " (" + Fixed(seconds, 2) + " s)" : "") +
             (detail.empty() ? "" : ": " + detail));
        JsonValue check = JsonValue::MakeObject();
        check.AddMember("id", JsonValue::MakeString(id));
        check.AddMember("name", JsonValue::MakeString(name));
        check.AddMember("verdict", JsonValue::MakeString(verdict));
        check.AddMember("detail", JsonValue::MakeString(detail));
        check.AddMember("seconds", seconds >= 0 ? JsonValue::MakeNumber(seconds, Fixed(seconds, 3)) : JsonValue());
        checks_.Append(std::move(check));
    }

    // Writes |bytes| as |name| in the folder, byte for byte.
    void Save(const std::string& name, const std::string& bytes) {
        std::FILE* file = crisp::platform::OpenFileForWriting(folder_ / name);
        const bool written = file != nullptr && std::fwrite(bytes.data(), 1, bytes.size(), file) == bytes.size();
        const bool closed = file != nullptr && std::fclose(file) == 0;
        if (!written || !closed) {
            Line("    could not write " + name);
            return;
        }
        files_.Append(JsonValue::MakeString(name));
    }

    void Finish(JsonValue summary) {
        summary.AddMember("checks", std::move(checks_));
        summary.AddMember("files", std::move(files_));
        summary.AddMember("failures", JsonValue::MakeInteger(failures_));
        Save("report.json", crisp::util::WriteJson(summary, true) + "\n");
    }

private:
    fs::path folder_;
    std::FILE* text_ = nullptr;
    JsonValue checks_;
    JsonValue files_;
    int failures_ = 0;
};

// Polls /progress until stopped and records each change, with the time since |start|.
class ProgressWatch {
public:
    ProgressWatch(const ApiClient& client, Clock::time_point start)
        : thread_([this, &client, start] { Run(client, start); }) {}
    ~ProgressWatch() { Stop(); }
    ProgressWatch(const ProgressWatch&) = delete;
    ProgressWatch& operator=(const ProgressWatch&) = delete;

    std::vector<std::string> Stop() {
        stop_ = true;
        cancel_.Cancel();
        if (thread_.joinable()) {
            thread_.join();
        }
        std::lock_guard<std::mutex> lock(mutex_);
        return lines_;
    }

private:
    void Run(const ApiClient& client, Clock::time_point start) {
        bool first = true;
        ProgressReport last;
        while (!stop_) {
            ProgressReport report;
            const ApiError error = client.Progress(report, &cancel_);
            if (stop_) {
                break;
            }
            std::string line;
            if (!error.Ok()) {
                line = Describe(error);
            } else if (first || report.busy != last.busy || report.progress != last.progress) {
                line = std::string("busy=") + (report.busy ? "true" : "false") +
                       " progress=" + std::to_string(report.progress);
                last = report;
                first = false;
            }
            if (!line.empty()) {
                std::lock_guard<std::mutex> lock(mutex_);
                lines_.push_back("t=+" + Fixed(SecondsSince(start), 2) + "s " + line);
            }
            for (int i = 0; i < 5 && !stop_; ++i) {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
        }
    }

    std::atomic<bool> stop_{false};
    crisp::platform::HttpCancel cancel_;
    std::mutex mutex_;
    std::vector<std::string> lines_;
    std::thread thread_;
};

bool ParseOptions(const std::vector<std::string>& args, Options& options, std::string& error) {
    for (std::size_t i = 1; i < args.size(); ++i) {
        const std::string& arg = args[i];
        const bool hasValue = i + 1 < args.size();
        if (arg == "--skip-errors") {
            options.skipErrors = true;
        } else if (arg == "--skip-cancel") {
            options.skipCancel = true;
        } else if (arg.rfind("--", 0) == 0 && !hasValue) {
            error = arg + " needs a value";
            return false;
        } else if (arg == "--host") {
            options.host = args[++i];
        } else if (arg == "--port") {
            options.port = std::atoi(args[++i].c_str());
        } else if (arg == "--key") {
            options.key = args[++i];
        } else if (arg == "--language") {
            options.language = args[++i];
        } else if (arg == "--timeout-s") {
            options.timeoutSeconds = std::atoi(args[++i].c_str());
        } else if (arg == "--out") {
            options.out = PathFromUtf8(args[++i]);
        } else if (arg == "--window") {
            const std::string value = args[++i];
            const std::size_t comma = value.find(',');
            options.window = comma != std::string::npos;
            if (options.window) {
                options.windowOffsetMs = std::atoll(value.substr(0, comma).c_str());
                options.windowDurationMs = std::atoll(value.substr(comma + 1).c_str());
            }
            if (!options.window || options.windowOffsetMs < 0 || options.windowDurationMs <= 0) {
                error = "--window needs <offset>,<length> in milliseconds, with a length above 0";
                return false;
            }
        } else if (arg.rfind("--", 0) == 0) {
            error = "unknown option " + arg;
            return false;
        } else if (options.audio.empty()) {
            options.audio = PathFromUtf8(arg);
        } else {
            error = "more than one audio file";
            return false;
        }
    }
    if (options.audio.empty()) {
        error = "no audio file";
        return false;
    }
    if (options.port <= 0 || options.port > 65535 || options.timeoutSeconds <= 0) {
        error = "the port or the timeout is out of range";
        return false;
    }
    return true;
}

// Size and SHA-256 of the audio file, read in pieces.
bool HashFile(const fs::path& path, std::uint64_t& size, std::string& sha256) {
    std::FILE* file = crisp::platform::OpenFileForReading(path);
    if (file == nullptr) {
        return false;
    }
    crisp::util::Sha256 hash;
    char buffer[65536];
    size = 0;
    std::size_t count = 0;
    while ((count = std::fread(buffer, 1, sizeof(buffer), file)) > 0) {
        hash.Update(buffer, count);
        size += count;
    }
    const bool ok = std::ferror(file) == 0;
    std::fclose(file);
    sha256 = hash.FinishHex();
    return ok;
}

std::string TranscriptSummary(const Transcript& transcript) {
    bool words = false;
    for (const auto& segment : transcript.segments) {
        words = words || !segment.words.empty();
    }
    std::string summary = std::to_string(transcript.segments.size()) + " segments, duration " +
                          Fixed(static_cast<double>(transcript.durationMs) / 1000.0, 3) + " s, language \"" +
                          transcript.language + "\", word times " + (words ? "yes" : "no");
    if (!transcript.segments.empty()) {
        const auto& first = transcript.segments.front();
        const auto& last = transcript.segments.back();
        summary += "; first [" + Timestamp(first.startMs) + "-" + Timestamp(first.endMs) + "] " +
                   Clip(first.text, 60) + "; last ends " + Timestamp(last.endMs);
    }
    return summary;
}

int Run(const std::vector<std::string>& args) {
    Options options;
    std::string usageError;
    if (!ParseOptions(args, options, usageError)) {
        std::fprintf(stderr,
                     "api_probe: %s\nUsage: api_probe [--host H] [--port P] [--key K] [--language L] "
                     "[--window OFFSET_MS,LENGTH_MS] [--timeout-s S] [--out FOLDER] [--skip-errors] [--skip-cancel] "
                     "<audio file>\n",
                     usageError.c_str());
        return 2;
    }
    std::error_code fsError;
    if (options.out.empty()) {
        options.out = crisp::platform::ExecutableDirectory() / "probe-results" / UtcStamp(true);
    }
    // No trailing separator: std::filesystem hands some paths to msvcrt.dll's _wstat64, which
    // rejects one (docs/ENVIRONMENT.md, section 4.5).
    while (!options.out.has_filename() && options.out.has_relative_path()) {
        options.out = options.out.parent_path();
    }
    fs::create_directories(options.out, fsError);
    Report report(options.out);
    if (!report.Ok()) {
        std::fprintf(stderr, "api_probe: cannot write into %s\n", Utf8(options.out).c_str());
        return 2;
    }

    ApiClientOptions clientOptions;
    clientOptions.endpoint = {options.host, options.port};
    clientOptions.apiKey = options.key;
    clientOptions.transcribeTimeoutMs = options.timeoutSeconds * 1000;
    const ApiClient client(clientOptions);

    std::uint64_t audioBytes = 0;
    std::string audioSha256;
    const bool audioRead = HashFile(options.audio, audioBytes, audioSha256);
    report.Line("CrispGUI api_probe, started " + UtcStamp(false));
    report.Line("Server: " + crisp::platform::HttpUrl(options.host, options.port, "") + ", key \"" + options.key +
                "\"");
    report.Line("Audio: " + Utf8(options.audio) +
                (audioRead ? " (" + std::to_string(audioBytes) + " bytes, SHA-256 " + audioSha256 + ")"
                           : " (cannot be read)"));
    report.Line("Output: " + Utf8(options.out));
    report.Line("");

    JsonValue summary = JsonValue::MakeObject();
    summary.AddMember("started", JsonValue::MakeString(UtcStamp(false)));
    summary.AddMember("server", JsonValue::MakeString(crisp::platform::HttpUrl(options.host, options.port, "")));
    JsonValue audio = JsonValue::MakeObject();
    audio.AddMember("path", JsonValue::MakeString(Utf8(options.audio)));
    audio.AddMember("bytes", JsonValue::MakeInteger(static_cast<std::int64_t>(audioBytes)));
    audio.AddMember("sha256", JsonValue::MakeString(audioSha256));
    summary.AddMember("audio", std::move(audio));

    // 1. GET /health, public.
    crisp::platform::HttpRequest health;
    health.host = crisp::core::ConnectHost(options.host);
    health.port = options.port;
    health.path = "/health";
    auto start = Clock::now();
    const crisp::platform::HttpResponse healthAnswer = crisp::platform::HttpGet(health);
    const double healthSeconds = SecondsSince(start);
    report.Save("health.json", healthAnswer.body);
    const crisp::core::HealthReport healthReport = crisp::core::ClassifyHealth(healthAnswer);
    const bool serverUp = healthReport.kind == crisp::core::HealthKind::kCrispAsr && !healthReport.loading;
    report.Check("1", "GET /health", serverUp ? "PASS" : "FAIL",
                 healthAnswer.Ok() ? "HTTP " + std::to_string(healthAnswer.status) + " " + Clip(healthAnswer.body, 120)
                                   : healthAnswer.error,
                 healthSeconds);
    if (!serverUp) {
        report.Line("The server does not answer as CrispASR; the remaining checks are skipped.");
        report.Finish(std::move(summary));
        return 1;
    }

    // 2. GET /v1/models, with the key.
    crisp::platform::HttpRequest models = health;
    models.path = "/v1/models";
    models.headers.emplace_back("Authorization", "Bearer " + options.key);
    start = Clock::now();
    const crisp::platform::HttpResponse modelsAnswer = crisp::platform::HttpGet(models);
    const double modelsSeconds = SecondsSince(start);
    report.Save("models.json", modelsAnswer.body);
    const ApiError modelsError = crisp::core::ClassifyApiResponse(modelsAnswer);
    report.Check("2", "GET /v1/models", modelsError.Ok() ? "PASS" : "FAIL",
                 modelsError.Ok() ? Clip(modelsAnswer.body, 200) : Describe(modelsError), modelsSeconds);

    // 3. GET /progress while nothing runs.
    ProgressReport idle;
    start = Clock::now();
    const ApiError idleError = client.Progress(idle);
    report.Check("3", "GET /progress before the transcriptions", idleError.Ok() ? "PASS" : "FAIL",
                 idleError.Ok() ? std::string("busy=") + (idle.busy ? "true" : "false") +
                                      " progress=" + std::to_string(idle.progress)
                                : Describe(idleError),
                 SecondsSince(start));

    // 4-6. Error answers, with a small file that is not audio.
    const fs::path notAudio = options.out / "not-audio.mp3";
    report.Save("not-audio.mp3", kNotAudio);
    if (options.skipErrors) {
        report.Check("4-6", "error answers", "SKIP", "--skip-errors", -1);
    } else {
        ApiClientOptions wrongKeyOptions = clientOptions;
        wrongKeyOptions.apiKey = "api-probe-wrong-key";
        TranscribeRequest bogus;
        bogus.file = notAudio;
        bogus.responseFormat = "bogus";  // also rejected when the key were accepted, before any inference
        start = Clock::now();
        const ApiReply wrongKey = ApiClient(wrongKeyOptions).TranscribeRaw(bogus);
        const double wrongKeySeconds = SecondsSince(start);
        report.Save("error-key.json", wrongKey.body);
        const ApiErrorKind keyKind = wrongKey.error.kind;
        report.Check("4", "POST with a wrong key",
                     keyKind == ApiErrorKind::kUnauthorized ? "PASS"
                     : keyKind == ApiErrorKind::kBadRequest ? "INFO"
                                                             : "FAIL",
                     Describe(wrongKey.error) +
                         (keyKind == ApiErrorKind::kBadRequest ? " (the server accepts any key)" : ""),
                     wrongKeySeconds);

        start = Clock::now();
        const ApiReply badFormat = client.TranscribeRaw(bogus);
        const double badFormatSeconds = SecondsSince(start);
        report.Save("error-format.json", badFormat.body);
        report.Check("5", "POST with response_format=bogus",
                     badFormat.error.kind == ApiErrorKind::kBadRequest ? "PASS" : "FAIL", Describe(badFormat.error),
                     badFormatSeconds);

        TranscribeRequest junk;
        junk.file = notAudio;
        start = Clock::now();
        const ApiReply decode = client.TranscribeRaw(junk);
        const double decodeSeconds = SecondsSince(start);
        report.Save("error-decode.json", decode.body);
        report.Check("6", "POST a file that is not audio",
                     decode.error.kind == ApiErrorKind::kBadRequest ? "PASS" : "FAIL", Describe(decode.error),
                     decodeSeconds);
    }

    TranscribeRequest request;
    request.file = options.audio;
    if (!options.language.empty()) {
        request.fields.emplace_back("language", options.language);
    }

    // 7. The whole file as verbose_json, with /progress watched meanwhile.
    {
        Transcript transcript;
        std::string body;
        start = Clock::now();
        ProgressWatch watch(client, start);
        const ApiError error = client.Transcribe(request, transcript, nullptr, &body);
        const double seconds = SecondsSince(start);
        const std::vector<std::string> progress = watch.Stop();
        report.Save("transcript.verbose.json", body);
        report.Check("7", "POST the audio, verbose_json", error.Ok() ? "PASS" : "FAIL",
                     error.Ok() ? std::to_string(body.size()) + " bytes, " + TranscriptSummary(transcript)
                                : Describe(error),
                     seconds);
        for (const std::string& line : progress) {
            report.Line("    progress " + line);
        }
    }

    // 8. The whole file as the server's own srt.
    {
        TranscribeRequest srt = request;
        srt.responseFormat = "srt";
        start = Clock::now();
        const ApiReply reply = client.TranscribeRaw(srt);
        const double seconds = SecondsSince(start);
        report.Save("transcript.srt", reply.body);
        report.Check("8", "POST the audio, srt", reply.error.Ok() ? "PASS" : "FAIL",
                     reply.error.Ok() ? std::to_string(reply.body.size()) + " bytes" : Describe(reply.error), seconds);
    }

    // 9. A window of the file.
    if (options.window) {
        TranscribeRequest window = request;
        window.offsetMs = options.windowOffsetMs;
        window.durationMs = options.windowDurationMs;
        Transcript transcript;
        std::string body;
        start = Clock::now();
        const ApiError error = client.Transcribe(window, transcript, nullptr, &body);
        const double seconds = SecondsSince(start);
        report.Save("window.verbose.json", body);
        report.Check("9",
                     "POST the audio, window " + std::to_string(options.windowOffsetMs) + " ms + " +
                         std::to_string(options.windowDurationMs) + " ms",
                     error.Ok() ? "PASS" : "FAIL", error.Ok() ? TranscriptSummary(transcript) : Describe(error),
                     seconds);
    } else {
        report.Check("9", "window", "SKIP", "no --window", -1);
    }

    // 10. Cancel a transcription once the server reports it busy, then see how long the server
    // keeps working (docs/API-CRISPASR.md, section 6, item 6).
    if (options.skipCancel) {
        report.Check("10", "cancellation", "SKIP", "--skip-cancel", -1);
    } else {
        crisp::platform::HttpCancel cancel;
        std::atomic<bool> done{false};
        ApiError result;
        Transcript transcript;
        std::thread worker([&] {
            result = client.Transcribe(request, transcript, &cancel);
            done = true;
        });
        start = Clock::now();
        bool sawBusy = false;
        while (!done && SecondsSince(start) < 60.0) {
            ProgressReport state;
            if (client.Progress(state).Ok() && state.busy) {
                sawBusy = true;
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        const auto cancelAt = Clock::now();
        cancel.Cancel();
        worker.join();
        const double clientSeconds = SecondsSince(cancelAt);
        if (result.kind == ApiErrorKind::kCancelled) {
            const auto serverStart = Clock::now();
            bool idleAgain = false;
            while (SecondsSince(serverStart) < options.timeoutSeconds) {
                ProgressReport state;
                if (client.Progress(state).Ok() && !state.busy) {
                    idleAgain = true;
                    break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
            }
            report.Check("10", "cancel a transcription", "PASS",
                         std::string(sawBusy ? "cancelled while /progress reported busy" : "cancelled") +
                             "; the client returned after " + Fixed(clientSeconds, 2) + " s; the server " +
                             (idleAgain ? "reported idle " + Fixed(SecondsSince(serverStart), 1) + " s later"
                                        : "was still busy at the end of the wait"),
                         -1);
        } else if (result.Ok()) {
            report.Check("10", "cancel a transcription", "INFO", "the transcription finished before it was cancelled",
                         -1);
        } else {
            report.Check("10", "cancel a transcription", "FAIL", Describe(result), -1);
        }
    }

    report.Line("");
    report.Line("Failures: " + std::to_string(report.Failures()));
    report.Finish(std::move(summary));
    return report.Failures() == 0 ? 0 : 1;
}

}  // namespace

#if defined(_WIN32)
int wmain(int argc, wchar_t** argv) {
    // UTF-8 for the console while the probe runs; the previous code page comes back at the end.
    const UINT previous = GetConsoleOutputCP();
    SetConsoleOutputCP(CP_UTF8);
    std::vector<std::string> args;
    for (int i = 0; i < argc; ++i) {
        args.push_back(crisp::platform::win32::Narrow(argv[i]));
    }
    const int code = Run(args);
    SetConsoleOutputCP(previous);
    return code;
}
#else
int main(int argc, char** argv) {
    return Run(std::vector<std::string>(argv, argv + argc));
}
#endif
