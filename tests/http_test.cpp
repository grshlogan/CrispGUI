#include "check.h"
#include "server_test_support.h"

#include "core/server_state.h"
#include "platform/http.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <thread>
#include <utility>

namespace fs = std::filesystem;
using crisp::core::ClassifyHealth;
using crisp::core::ClassifyModels;
using crisp::core::HealthKind;
using crisp::platform::ChildProcess;
using crisp::platform::HttpBody;
using crisp::platform::HttpCancel;
using crisp::platform::HttpFailure;
using crisp::platform::HttpGet;
using crisp::platform::HttpPost;
using crisp::platform::HttpRequest;
using crisp::platform::HttpResponse;

namespace {

// A body from memory that hands out at most |piece| bytes per read, after |pause|, and fails once
// |failAt| bytes are out.
class TestBody : public HttpBody {
public:
    explicit TestBody(std::string bytes, std::size_t piece = 65536, int pauseMs = 0,
                      std::size_t failAt = static_cast<std::size_t>(-1))
        : bytes_(std::move(bytes)), piece_(piece), pauseMs_(pauseMs), failAt_(failAt) {}

    std::uint64_t Size() const override { return bytes_.size(); }

    bool Read(char* buffer, std::size_t capacity, std::size_t& count, std::string& error) override {
        if (position_ >= failAt_) {
            error = "The test body fails here.";
            return false;
        }
        if (pauseMs_ > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(pauseMs_));
        }
        count = std::min({capacity, piece_, bytes_.size() - position_});
        std::memcpy(buffer, bytes_.data() + position_, count);
        position_ += count;
        return true;
    }

private:
    std::string bytes_;
    std::size_t piece_;
    int pauseMs_;
    std::size_t failAt_;
    std::size_t position_ = 0;
};

// The checksum the stub's /stub/echo reports.
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

HttpRequest Echo(int port) {
    HttpRequest request;
    request.host = "127.0.0.1";
    request.port = port;
    request.path = "/stub/echo";
    request.headers = {{"Content-Type", "application/x-test"}};
    return request;
}

}  // namespace

int main() {
    const fs::path stub = crisp::test::StubExecutable();
    std::error_code error;
    if (!fs::is_regular_file(stub, error)) {
        std::fprintf(stderr, "http_test: the stub server is missing at %s\n", stub.u8string().c_str());
        return 1;
    }

    // Nothing listens: refused.
    const int port = crisp::test::FreePort();
    const HttpResponse refused = crisp::test::Get(port, "/health");
    CHECK(refused.failure == HttpFailure::kRefused);
    CHECK(!refused.error.empty());
    CHECK(ClassifyHealth(refused).kind == HealthKind::kRefused);

    // A CrispASR-like server.
    {
        ChildProcess server;
        CHECK(crisp::test::StartStub(server, stub, port,
                                     {{"STUB_LOAD_MS", "0", false}, {"CRISPASR_API_KEYS", "first,secret", false}}));
        CHECK(crisp::test::WaitForAnswer(port, 15000));

        const HttpResponse health = crisp::test::Get(port, "/health");
        CHECK(health.Ok() && health.status == 200);
        CHECK(health.body == "{\"status\": \"ok\", \"backend\": \"stubby\"}");
        const auto report = ClassifyHealth(health);
        CHECK(report.kind == HealthKind::kCrispAsr && report.backend == "stubby");

        // /v1/models needs a key the server knows.
        CHECK(crisp::test::Get(port, "/v1/models").status == 401);
        CHECK(crisp::test::Get(port, "/v1/models", {{"Authorization", "Bearer wrong"}}).status == 401);
        const HttpResponse models = crisp::test::Get(port, "/v1/models", {{"Authorization", "Bearer secret"}});
        CHECK(models.status == 200);
        const auto modelsReport = ClassifyModels(models);
        CHECK(modelsReport.ok && modelsReport.model == "stub-model.gguf" && modelsReport.backend == "stubby");

        CHECK(crisp::test::Get(port, "/no/such/route").status == 404);

        // A body over the limit is refused rather than read.
        HttpRequest small;
        small.host = "127.0.0.1";
        small.port = port;
        small.path = "/health";
        small.maxBodyBytes = 8;
        const HttpResponse truncated = HttpGet(small);
        CHECK(truncated.failure == HttpFailure::kTooLarge && truncated.body.empty());

        // A cancelled request does not start.
        HttpCancel cancel;
        cancel.Cancel();
        CHECK(cancel.Cancelled());
        CHECK(HttpGet(small, &cancel).failure == HttpFailure::kCancelled);

        // POST: the body arrives whole, in the pieces it was produced in.
        std::string payload;
        for (int i = 0; i < 300000; ++i) {
            payload.push_back(static_cast<char>((i * 13) % 256));
        }
        TestBody body(payload, 1000);
        const HttpResponse echoed = HttpPost(Echo(port), body);
        CHECK(echoed.Ok() && echoed.status == 200);
        CHECK(echoed.body == "{\"bytes\": 300000, \"fnv1a\": \"" + Fnv1a(payload) +
                                 "\", \"content_type\": \"application/x-test\"}");
        TestBody empty("");
        CHECK(HttpPost(Echo(port), empty).body.rfind("{\"bytes\": 0,", 0) == 0);

        // A body that cannot be produced ends the request.
        TestBody failing(payload, 1000, 0, 5000);
        const HttpResponse failed = HttpPost(Echo(port), failing);
        CHECK(failed.failure == HttpFailure::kBody && failed.error == "The test body fails here.");
        CHECK(failed.status == 0 && failed.body.empty());

        // Cancel() ends an upload in progress.
        TestBody slow(payload, 1000, 20);
        HttpCancel stop;
        std::thread canceller([&stop] {
            std::this_thread::sleep_for(std::chrono::milliseconds(300));
            stop.Cancel();
        });
        const auto start = std::chrono::steady_clock::now();
        const HttpResponse stopped = HttpPost(Echo(port), slow, &stop);
        const auto elapsed = std::chrono::steady_clock::now() - start;
        canceller.join();
        CHECK(stopped.failure == HttpFailure::kCancelled);
        CHECK(elapsed < std::chrono::seconds(3));

        crisp::test::EndProcess(server);
    }

    // POST to a port where nothing listens.
    {
        TestBody body("x");
        CHECK(HttpPost(Echo(crisp::test::FreePort()), body).failure == HttpFailure::kRefused);
    }

    // Another program on the port.
    {
        const int foreignPort = crisp::test::FreePort();
        ChildProcess foreign;
        CHECK(crisp::test::StartStub(foreign, stub, foreignPort,
                                     {{"STUB_LOAD_MS", "0", false}, {"STUB_MODE", "foreign", false}}));
        CHECK(crisp::test::WaitForAnswer(foreignPort, 15000));
        const auto report = ClassifyHealth(crisp::test::Get(foreignPort, "/health"));
        CHECK(report.kind == HealthKind::kForeign);
        CHECK(report.detail == "GET /health answered HTTP 404");
        crisp::test::EndProcess(foreign);
    }
    return crisp::test::Result();
}
