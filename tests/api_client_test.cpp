#include "check.h"
#include "server_test_support.h"

#include "core/api_client.h"
#include "core/transcript.h"
#include "platform/files.h"
#include "platform/http.h"
#include "util/json.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <string>
#include <thread>
#include <vector>

namespace fs = std::filesystem;
using crisp::core::ApiClient;
using crisp::core::ApiClientOptions;
using crisp::core::ApiError;
using crisp::core::ApiErrorKind;
using crisp::core::ApiReply;
using crisp::core::ProgressReport;
using crisp::core::Transcript;
using crisp::core::TranscribeRequest;
using crisp::platform::ChildProcess;
using crisp::platform::HttpCancel;
using Clock = std::chrono::steady_clock;

namespace {

// The checksum the stub reports for an upload (tests/stub_crispasr.cpp).
std::string Fnv1a(const std::string& bytes) {
    std::uint64_t hash = 0xcbf29ce484222325ull;
    for (const char c : bytes) {
        hash ^= static_cast<unsigned char>(c);
        hash *= 0x100000001b3ull;
    }
    char text[17];
    std::snprintf(text, sizeof(text), "%016llx", static_cast<unsigned long long>(hash));
    return text;
}

ApiClient Client(int port, const std::string& key = "secret") {
    ApiClientOptions options;
    options.endpoint = {"127.0.0.1", port};
    options.apiKey = key;
    return ApiClient(options);
}

double MillisecondsSince(Clock::time_point start) {
    return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}

// Starts a stub on a free port with the key "secret" and |extra| settings.
bool StartServer(ChildProcess& process, int& port, std::vector<crisp::platform::EnvironmentChange> extra = {}) {
    port = crisp::test::FreePort();
    extra.push_back({"STUB_LOAD_MS", "0", false});
    extra.push_back({"CRISPASR_API_KEYS", "secret", false});
    return crisp::test::StartStub(process, crisp::test::StubExecutable(), port, std::move(extra)) &&
           crisp::test::WaitForAnswer(port, 15000);
}

TranscribeRequest Steered(const fs::path& file, const std::string& reply) {
    TranscribeRequest request;
    request.file = file;
    request.fields = {{"stub_reply", reply}};
    return request;
}

}  // namespace

int main() {
    std::error_code fsError;
    if (!fs::is_regular_file(crisp::test::StubExecutable(), fsError)) {
        std::fprintf(stderr, "api_client_test: the stub server is missing\n");
        return 1;
    }
    const fs::path scratch = crisp::test::FreshScratchDirectory("api_client_test.scratch");
    // A WAV header, so that the stub "decodes" it, then every byte value.
    std::string audioBytes("RIFF\x24\x00\x00\x00WAVEfmt ", 16);
    for (int i = 0; i < 200000; ++i) {
        audioBytes.push_back(static_cast<char>((i * 7) % 256));
    }
    const fs::path audio = scratch / fs::u8path(u8"音声 テスト.wav");
    CHECK(crisp::test::WriteFile(audio, audioBytes));
    const fs::path notAudio = scratch / "not-audio.mp3";
    CHECK(crisp::test::WriteFile(notAudio, "just text"));

    // Nothing listens.
    {
        const ApiClient client = Client(crisp::test::FreePort());
        Transcript transcript;
        TranscribeRequest request;
        request.file = audio;
        CHECK(client.Transcribe(request, transcript).kind == ApiErrorKind::kUnreachable);
        ProgressReport report;
        CHECK(client.Progress(report).kind == ApiErrorKind::kUnreachable);
    }

    int port = 0;
    ChildProcess server;
    CHECK(StartServer(server, port));
    const ApiClient client = Client(port);

    // A transcription of a window, with fields that need escaping; the stub reports what it got.
    {
        TranscribeRequest request;
        request.file = audio;
        request.fields = {{"language", "ja"}, {"prompt", "line1\nline2 \"quoted\" \\"}};
        request.offsetMs = 1000;
        request.durationMs = 5000;
        Transcript transcript;
        std::string body;
        const ApiError error = client.Transcribe(request, transcript, nullptr, &body);
        CHECK(error.Ok() && error.status == 200);
        CHECK(transcript.durationMs == 6000 && transcript.language == "ja");
        CHECK(transcript.segments.size() == 3);
        if (transcript.segments.size() == 3) {
            CHECK(transcript.segments[0].endMs == 2500 && transcript.segments[0].words.size() == 2);
            CHECK(transcript.segments[1].startMs == 2500 && transcript.segments[1].endMs == 5120);
            CHECK(transcript.segments[1].text == "Quote \" and backslash \\ in one line.");
            CHECK(transcript.segments[2].text.empty());
        }
        const auto parsed = crisp::util::ParseJson(body);
        const crisp::util::JsonValue* stub = parsed.Ok() ? parsed.value.Find("stub") : nullptr;
        CHECK(stub != nullptr);
        if (stub != nullptr) {
            CHECK(stub->Find("file_field")->AsString() == "file");
            CHECK(stub->Find("file_name")->AsString() == u8"音声 テスト.wav");
            CHECK(stub->Find("file_content_type")->AsString() == "application/octet-stream");
            CHECK(stub->Find("file_bytes")->NumberText() == std::to_string(audioBytes.size()));
            CHECK(stub->Find("file_fnv1a")->AsString() == Fnv1a(audioBytes));
            const crisp::util::JsonValue* fields = stub->Find("fields");
            const std::vector<std::pair<std::string, std::string>> expected = {
                {"response_format", "verbose_json"},
                {"offset_t_ms", "1000"},
                {"duration_ms", "5000"},
                {"language", "ja"},
                {"prompt", "line1\nline2 \"quoted\" \\"}};
            CHECK(fields != nullptr && fields->Size() == expected.size());
            for (std::size_t i = 0; fields != nullptr && i < fields->Size() && i < expected.size(); ++i) {
                CHECK(fields->At(i).At(0).AsString() == expected[i].first);
                CHECK(fields->At(i).At(1).AsString() == expected[i].second);
            }
        }
    }

    // No window: no offset or duration fields. The server's own srt comes back unparsed.
    {
        TranscribeRequest request;
        request.file = audio;
        request.responseFormat = "srt";
        const ApiReply reply = client.TranscribeRaw(request);
        CHECK(reply.error.Ok() && reply.status == 200);
        CHECK(reply.body ==
              "1\n00:00:00,000 --> 00:00:02,500\n\xE3\x81\x93\xE3\x82\x93\xE3\x81\xAB\xE3\x81\xA1\xE3\x81\xAF"
              "\xE3\x80\x81\xE4\xB8\x96\xE7\x95\x8C\xE3\x80\x82\n\n"
              "2\n00:00:02,500 --> 00:00:05,120\nQuote \" and backslash \\ in one line.\n\n");
    }

    // Answers the server gives (docs/API-CRISPASR.md, section 5).
    {
        Transcript transcript;
        TranscribeRequest request;
        request.file = audio;
        const ApiError wrongKey = Client(port, "wrong").Transcribe(request, transcript);
        CHECK(wrongKey.kind == ApiErrorKind::kUnauthorized && wrongKey.status == 401);
        CHECK(wrongKey.message == "invalid or missing API key");
        CHECK(Client(port, "").Transcribe(request, transcript).kind == ApiErrorKind::kUnauthorized);

        TranscribeRequest bogus = request;
        bogus.responseFormat = "bogus";
        const ApiReply format = client.TranscribeRaw(bogus);
        CHECK(format.error.kind == ApiErrorKind::kBadRequest && format.status == 400);
        CHECK(format.error.message.rfind("invalid response_format 'bogus'", 0) == 0);

        TranscribeRequest junk;
        junk.file = notAudio;
        const ApiError decode = client.Transcribe(junk, transcript);
        CHECK(decode.kind == ApiErrorKind::kBadRequest);
        CHECK(decode.message == "failed to decode audio (unsupported format or corrupt file)");

        const ApiError internal = client.Transcribe(Steered(audio, "500"), transcript);
        CHECK(internal.kind == ApiErrorKind::kServerError && internal.message == "internal error: stub failure");
        const ApiError busy = client.Transcribe(Steered(audio, "503"), transcript);
        CHECK(busy.kind == ApiErrorKind::kServerBusy && busy.message == "model is still loading");
        const ApiError garbage = client.Transcribe(Steered(audio, "garbage"), transcript);
        CHECK(garbage.kind == ApiErrorKind::kProtocol && garbage.status == 200);
        CHECK(garbage.message.rfind("The answer is not verbose_json: The answer is not JSON", 0) == 0);
        CHECK(client.Transcribe(Steered(audio, "close"), transcript).kind == ApiErrorKind::kUnreachable);
    }

    // Requests the client refuses to send.
    {
        Transcript transcript;
        TranscribeRequest own;
        own.file = audio;
        own.fields = {{"response_format", "json"}};
        const ApiError repeated = client.Transcribe(own, transcript);
        CHECK(repeated.kind == ApiErrorKind::kBadRequest && repeated.status == 0);
        TranscribeRequest badName;
        badName.file = audio;
        badName.fields = {{"bad name", "x"}};
        CHECK(client.Transcribe(badName, transcript).kind == ApiErrorKind::kBadRequest);
        TranscribeRequest missing;
        missing.file = scratch / "missing.wav";
        CHECK(client.Transcribe(missing, transcript).kind == ApiErrorKind::kFile);

        ApiClientOptions small = client.Options();
        small.maxUploadBytes = 1000;
        TranscribeRequest request;
        request.file = audio;
        const ApiError local = ApiClient(small).Transcribe(request, transcript);
        CHECK(local.kind == ApiErrorKind::kPayloadTooLarge && local.status == 0);

        HttpCancel cancel;
        cancel.Cancel();
        CHECK(client.Transcribe(request, transcript, &cancel).kind == ApiErrorKind::kCancelled);
    }

    // /progress while idle.
    {
        ProgressReport report;
        const ApiError error = client.Progress(report);
        CHECK(error.Ok() && !report.busy && report.progress == -1);
        CHECK(Client(port, "wrong").Progress(report).kind == ApiErrorKind::kUnauthorized);
    }
    crisp::test::EndProcess(server);

    // A server whose body limit is smaller than the upload answers 413.
    {
        int smallPort = 0;
        ChildProcess smallServer;
        CHECK(StartServer(smallServer, smallPort, {{"STUB_MAX_BODY_BYTES", "1000", false}}));
        Transcript transcript;
        TranscribeRequest request;
        request.file = audio;
        const ApiError large = Client(smallPort).Transcribe(request, transcript);
        CHECK(large.kind == ApiErrorKind::kPayloadTooLarge && large.status == 413);
        crisp::test::EndProcess(smallServer);
    }

    // A recorded answer, replayed by the stub as is.
    {
        int replayPort = 0;
        ChildProcess replayServer;
        const fs::path fixture = crisp::test::TestDataDirectory() / "verbose-synthetic.json";
        CHECK(StartServer(replayServer, replayPort, {{"STUB_TRANSCRIPT", fixture.u8string(), false}}));
        Transcript transcript;
        std::string body;
        TranscribeRequest request;
        request.file = audio;
        CHECK(Client(replayPort).Transcribe(request, transcript, nullptr, &body).Ok());
        CHECK(body == crisp::platform::ReadFileBytes(fixture, 1 << 20).bytes);
        CHECK(transcript.durationMs == 12345 && transcript.segments.size() == 3);
        crisp::test::EndProcess(replayServer);
    }

    // Slow transcriptions: progress while one runs, a timeout, and a cancellation. Each request
    // holds the stub's model lock for its whole delay, as CrispASR's model mutex does.
    {
        int slowPort = 0;
        ChildProcess slowServer;
        CHECK(StartServer(slowServer, slowPort));
        const ApiClient slow = Client(slowPort);

        std::atomic<bool> done{false};
        ApiError result;
        std::thread worker([&] {
            Transcript transcript;
            TranscribeRequest request;
            request.file = audio;
            request.fields = {{"stub_delay_ms", "1500"}};
            result = slow.Transcribe(request, transcript);
            done = true;
        });
        bool sawBusy = false;
        const bool finished = crisp::test::WaitUntil(
            [&] {
                ProgressReport report;
                if (slow.Progress(report).Ok() && report.busy && report.progress >= 0 && report.progress <= 100) {
                    sawBusy = true;
                }
                return done.load();
            },
            20000);
        worker.join();
        CHECK(finished && result.Ok());
        CHECK(sawBusy);

        // libcurl counts the connection's budget into the limit (platform/http.h), so it is small.
        ApiClientOptions shortWait = slow.Options();
        shortWait.connectTimeoutMs = 500;
        shortWait.transcribeTimeoutMs = 1000;
        TranscribeRequest request;
        request.file = audio;
        request.fields = {{"stub_delay_ms", "3000"}};
        Transcript transcript;
        auto start = Clock::now();
        const ApiError timeout = ApiClient(shortWait).Transcribe(request, transcript);
        const double timeoutMs = MillisecondsSince(start);
        CHECK(timeout.kind == ApiErrorKind::kTimeout);
        CHECK(timeoutMs >= 900 && timeoutMs < 4000);
        std::printf("api_client_test: timeout after %.0f ms\n", timeoutMs);

        HttpCancel cancel;
        ApiError cancelled;
        start = Clock::now();
        std::thread canceller([&] {
            std::this_thread::sleep_for(std::chrono::milliseconds(300));
            cancel.Cancel();
        });
        cancelled = slow.Transcribe(request, transcript, &cancel);
        const double cancelMs = MillisecondsSince(start);
        canceller.join();
        CHECK(cancelled.kind == ApiErrorKind::kCancelled);
        CHECK(cancelMs < 2500);
        std::printf("api_client_test: cancelled after %.0f ms\n", cancelMs);
        crisp::test::EndProcess(slowServer);
    }

    fs::remove_all(scratch, fsError);
    return crisp::test::Result();
}
