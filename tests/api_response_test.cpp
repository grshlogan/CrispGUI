#include "check.h"

#include "core/api_client.h"
#include "platform/http.h"

#include <set>
#include <string>

using crisp::core::ApiError;
using crisp::core::ApiErrorKind;
using crisp::core::ApiErrorKindName;
using crisp::core::ClassifyApiResponse;
using crisp::core::ErrorBodyMessage;
using crisp::core::ParseProgress;
using crisp::core::ProgressReport;
using crisp::platform::HttpFailure;
using crisp::platform::HttpResponse;

namespace {

HttpResponse Answer(int status, const std::string& body) {
    HttpResponse response;
    response.status = status;
    response.body = body;
    return response;
}

HttpResponse Failed(HttpFailure failure, const std::string& error) {
    HttpResponse response;
    response.failure = failure;
    response.error = error;
    return response;
}

// CrispASR's json_error() body (crispasr_server.cpp:425-436).
std::string JsonError(const std::string& message) {
    return "{\"error\": {\"message\": \"" + message + "\", \"type\": \"invalid_request_error\"}}";
}

const std::string kNotFound = "{\"error\": \"not found. Use POST /v1/audio/transcriptions\"}";

}  // namespace

int main() {
    // Transport failures.
    {
        const ApiError refused = ClassifyApiResponse(Failed(HttpFailure::kRefused, "Couldn't connect to server."));
        CHECK(refused.kind == ApiErrorKind::kUnreachable && refused.status == 0);
        CHECK(refused.message == "Couldn't connect to server.");
        CHECK(ClassifyApiResponse(Failed(HttpFailure::kOther, "reset")).kind == ApiErrorKind::kUnreachable);
        CHECK(ClassifyApiResponse(Failed(HttpFailure::kTimeout, "t")).kind == ApiErrorKind::kTimeout);
        CHECK(ClassifyApiResponse(Failed(HttpFailure::kCancelled, "c")).kind == ApiErrorKind::kCancelled);
        CHECK(ClassifyApiResponse(Failed(HttpFailure::kTooLarge, "big")).kind == ApiErrorKind::kProtocol);
        const ApiError file = ClassifyApiResponse(Failed(HttpFailure::kBody, "The file grew."));
        CHECK(file.kind == ApiErrorKind::kFile && file.message == "The file grew.");
    }

    // Status codes, after docs/API-CRISPASR.md, section 5.
    {
        const ApiError ok = ClassifyApiResponse(Answer(200, "{}"));
        CHECK(ok.Ok() && ok.status == 200 && ok.message.empty());

        const ApiError bad = ClassifyApiResponse(
            Answer(400, JsonError("failed to decode audio (unsupported format or corrupt file)")));
        CHECK(bad.kind == ApiErrorKind::kBadRequest && bad.status == 400);
        CHECK(bad.message == "failed to decode audio (unsupported format or corrupt file)");

        const ApiError key = ClassifyApiResponse(Answer(
            401, "{\"error\": {\"message\": \"invalid or missing API key\", \"type\": \"invalid_api_key\"}}"));
        CHECK(key.kind == ApiErrorKind::kUnauthorized && key.message == "invalid or missing API key");

        const ApiError large = ClassifyApiResponse(Answer(413, kNotFound));
        CHECK(large.kind == ApiErrorKind::kPayloadTooLarge && large.status == 413);
        CHECK(large.message == "The server refused the upload as too large (HTTP 413).");

        const ApiError internal = ClassifyApiResponse(Answer(500, JsonError("internal error: boom")));
        CHECK(internal.kind == ApiErrorKind::kServerError && internal.message == "internal error: boom");

        const ApiError busy = ClassifyApiResponse(Answer(503, JsonError("model is still loading")));
        CHECK(busy.kind == ApiErrorKind::kServerBusy && busy.message == "model is still loading");

        const ApiError missing = ClassifyApiResponse(Answer(404, kNotFound));
        CHECK(missing.kind == ApiErrorKind::kProtocol && missing.status == 404);
        CHECK(missing.message == "Unexpected HTTP 404: not found. Use POST /v1/audio/transcriptions");
        CHECK(ClassifyApiResponse(Answer(502, "")).message == "Unexpected HTTP 502.");
        CHECK(ClassifyApiResponse(Answer(204, "")).kind == ApiErrorKind::kProtocol);

        CHECK(ClassifyApiResponse(Answer(500, "")).message == "The server answered HTTP 500.");
        CHECK(ClassifyApiResponse(Answer(400, "bad\r\nthing ")).message == "bad  thing");
    }

    // Messages of error bodies.
    CHECK(ErrorBodyMessage("") == "");
    CHECK(ErrorBodyMessage("{\"error\": {\"type\": \"x\"}}") == "{\"error\": {\"type\": \"x\"}}");
    CHECK(ErrorBodyMessage(std::string(300, 'x')) == std::string(200, 'x') + "...");
    {
        std::string kana;
        for (int i = 0; i < 100; ++i) {
            kana += "\xE3\x81\x82";  // 3 bytes each: the cut falls inside a character
        }
        std::string expected;
        for (int i = 0; i < 66; ++i) {
            expected += "\xE3\x81\x82";
        }
        CHECK(ErrorBodyMessage(kana) == expected + "...");
    }

    // GET /progress bodies (crispasr_server.cpp:2403-2410).
    {
        ProgressReport report;
        std::string error;
        CHECK(ParseProgress("{\"busy\": true, \"progress\": 42}", report, error));
        CHECK(report.busy && report.progress == 42);
        CHECK(ParseProgress("{\"busy\": false, \"progress\": -1}", report, error));
        CHECK(!report.busy && report.progress == -1);
        CHECK(!ParseProgress("{\"busy\": 1, \"progress\": 2}", report, error));
        CHECK(!ParseProgress("{\"busy\": false}", report, error));
        CHECK(!ParseProgress("{\"busy\": false, \"progress\": 1.5}", report, error));
        CHECK(!ParseProgress("x", report, error));
        CHECK(!error.empty());
    }

    // Every kind has its own name.
    std::set<std::string> names;
    for (const ApiErrorKind kind :
         {ApiErrorKind::kNone, ApiErrorKind::kUnreachable, ApiErrorKind::kTimeout, ApiErrorKind::kCancelled,
          ApiErrorKind::kBadRequest, ApiErrorKind::kUnauthorized, ApiErrorKind::kPayloadTooLarge,
          ApiErrorKind::kServerBusy, ApiErrorKind::kServerError, ApiErrorKind::kProtocol, ApiErrorKind::kFile}) {
        names.insert(ApiErrorKindName(kind));
    }
    CHECK(names.size() == 11 && names.count("unknown") == 0);
    return crisp::test::Result();
}
