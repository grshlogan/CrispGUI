#include "check.h"
#include "server_test_support.h"

#include "core/server_state.h"
#include "platform/http.h"

#include <cstdio>
#include <filesystem>
#include <string>

namespace fs = std::filesystem;
using crisp::core::ClassifyHealth;
using crisp::core::ClassifyModels;
using crisp::core::HealthKind;
using crisp::platform::ChildProcess;
using crisp::platform::HttpCancel;
using crisp::platform::HttpFailure;
using crisp::platform::HttpGet;
using crisp::platform::HttpRequest;
using crisp::platform::HttpResponse;

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
        CHECK(truncated.failure == HttpFailure::kOther && truncated.body.empty());

        // A cancelled request does not start.
        HttpCancel cancel;
        cancel.Cancel();
        CHECK(cancel.Cancelled());
        CHECK(HttpGet(small, &cancel).failure == HttpFailure::kCancelled);

        crisp::test::EndProcess(server);
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
