#include "core/api_client.h"

#include "platform/multipart.h"
#include "util/json.h"

#include <utility>

namespace crisp::core {
namespace {

// The fields the client sets itself.
constexpr const char* kClientFields[] = {"file", "response_format", "offset_t_ms", "duration_ms"};

// The first |limit| bytes of |body| for a message: control characters as spaces, cut at a
// character boundary.
std::string Excerpt(const std::string& body, std::size_t limit) {
    std::size_t end = body.size();
    if (end > limit) {
        end = limit;
        while (end > 0 && (static_cast<unsigned char>(body[end]) & 0xC0) == 0x80) {
            --end;
        }
    }
    std::string text;
    for (std::size_t i = 0; i < end; ++i) {
        const unsigned char c = static_cast<unsigned char>(body[i]);
        text.push_back(c < 0x20 || c == 0x7F ? ' ' : body[i]);
    }
    const std::size_t first = text.find_first_not_of(' ');
    const std::size_t last = text.find_last_not_of(' ');
    text = first == std::string::npos ? std::string() : text.substr(first, last - first + 1);
    return end < body.size() ? text + "..." : text;
}

ApiError Failure(ApiErrorKind kind, std::string message) {
    ApiError error;
    error.kind = kind;
    error.message = std::move(message);
    return error;
}

}  // namespace

const char* ApiErrorKindName(ApiErrorKind kind) {
    switch (kind) {
    case ApiErrorKind::kNone:
        return "none";
    case ApiErrorKind::kUnreachable:
        return "unreachable";
    case ApiErrorKind::kTimeout:
        return "timeout";
    case ApiErrorKind::kCancelled:
        return "cancelled";
    case ApiErrorKind::kBadRequest:
        return "bad-request";
    case ApiErrorKind::kUnauthorized:
        return "unauthorized";
    case ApiErrorKind::kPayloadTooLarge:
        return "payload-too-large";
    case ApiErrorKind::kServerBusy:
        return "server-busy";
    case ApiErrorKind::kServerError:
        return "server-error";
    case ApiErrorKind::kProtocol:
        return "protocol";
    case ApiErrorKind::kFile:
        return "file";
    }
    return "unknown";
}

std::string ErrorBodyMessage(const std::string& body) {
    const util::JsonParseResult parsed = util::ParseJson(body);
    if (parsed.Ok() && parsed.value.IsObject()) {
        if (const util::JsonValue* error = parsed.value.Find("error")) {
            if (error->IsString()) {
                return error->AsString();
            }
            const util::JsonValue* message = error->Find("message");
            if (message != nullptr && message->IsString()) {
                return message->AsString();
            }
        }
    }
    return Excerpt(body, 200);
}

ApiError ClassifyApiResponse(const platform::HttpResponse& response) {
    switch (response.failure) {
    case platform::HttpFailure::kRefused:
    case platform::HttpFailure::kOther:
        return Failure(ApiErrorKind::kUnreachable, response.error);
    case platform::HttpFailure::kTimeout:
        return Failure(ApiErrorKind::kTimeout, response.error);
    case platform::HttpFailure::kCancelled:
        return Failure(ApiErrorKind::kCancelled, response.error);
    case platform::HttpFailure::kTooLarge:
        return Failure(ApiErrorKind::kProtocol, response.error);
    case platform::HttpFailure::kBody:
        return Failure(ApiErrorKind::kFile, response.error);
    case platform::HttpFailure::kNone:
        break;
    }
    ApiError error;
    error.status = response.status;
    if (response.status == 200) {
        return error;
    }
    const std::string message = ErrorBodyMessage(response.body);
    const std::string status = "HTTP " + std::to_string(response.status);
    switch (response.status) {
    case 400:
        error.kind = ApiErrorKind::kBadRequest;
        break;
    case 401:
        error.kind = ApiErrorKind::kUnauthorized;
        break;
    case 413:
        // CrispASR's error handler fills the empty 413 body with its "not found" text
        // (crispasr_server.cpp:3919-3925), so the body says nothing about the cause.
        error.kind = ApiErrorKind::kPayloadTooLarge;
        error.message = "The server refused the upload as too large (" + status + ").";
        return error;
    case 500:
        error.kind = ApiErrorKind::kServerError;
        break;
    case 503:
        error.kind = ApiErrorKind::kServerBusy;
        break;
    default:
        error.kind = ApiErrorKind::kProtocol;
        error.message = "Unexpected " + status + (message.empty() ? "." : ": " + message);
        return error;
    }
    error.message = message.empty() ? "The server answered " + status + "." : message;
    return error;
}

bool ParseProgress(const std::string& body, ProgressReport& report, std::string& error) {
    const util::JsonParseResult parsed = util::ParseJson(body);
    if (!parsed.Ok() || !parsed.value.IsObject()) {
        error = "The answer is not a JSON object.";
        return false;
    }
    const util::JsonValue* busy = parsed.value.Find("busy");
    const util::JsonValue* progress = parsed.value.Find("progress");
    if (busy == nullptr || !busy->IsBoolean() || progress == nullptr || !progress->IsNumber() ||
        !progress->IsIntegerLiteral()) {
        error = "The answer has no boolean \"busy\" and whole number \"progress\".";
        return false;
    }
    report.busy = busy->AsBoolean();
    report.progress = static_cast<int>(progress->AsNumber());
    return true;
}

ApiClient::ApiClient(ApiClientOptions options) : options_(std::move(options)) {}

platform::HttpRequest ApiClient::BaseRequest(const std::string& path, int timeoutMs) const {
    platform::HttpRequest request;
    request.host = ConnectHost(options_.endpoint.host);
    request.port = options_.endpoint.port;
    request.path = path;
    request.connectTimeoutMs = options_.connectTimeoutMs;
    request.timeoutMs = timeoutMs;
    if (!options_.apiKey.empty()) {
        request.headers.emplace_back("Authorization", "Bearer " + options_.apiKey);
    }
    return request;
}

ApiReply ApiClient::TranscribeRaw(const TranscribeRequest& request, platform::HttpCancel* cancel) const {
    ApiReply reply;
    std::vector<std::pair<std::string, std::string>> fields;
    fields.emplace_back("response_format", request.responseFormat);
    if (request.durationMs > 0) {
        fields.emplace_back("offset_t_ms", std::to_string(request.offsetMs));
        fields.emplace_back("duration_ms", std::to_string(request.durationMs));
    }
    for (const auto& field : request.fields) {
        for (const char* own : kClientFields) {
            if (field.first == own) {
                reply.error = Failure(ApiErrorKind::kBadRequest,
                                      "The field \"" + field.first + "\" is set by the client, not by the request.");
                return reply;
            }
        }
        fields.push_back(field);
    }

    platform::MultipartFile file;
    file.path = request.file;
    platform::MultipartBody body(fields, file);
    if (!body.Error().empty()) {
        reply.error = Failure(body.FileFailed() ? ApiErrorKind::kFile : ApiErrorKind::kBadRequest, body.Error());
        return reply;
    }
    if (body.Size() > options_.maxUploadBytes) {
        reply.error = Failure(ApiErrorKind::kPayloadTooLarge,
                              "The upload would be " + std::to_string(body.Size()) + " bytes; the server accepts " +
                                  std::to_string(options_.maxUploadBytes) + " bytes per request.");
        return reply;
    }

    platform::HttpRequest http = BaseRequest("/v1/audio/transcriptions", options_.transcribeTimeoutMs);
    http.headers.emplace_back("Content-Type", body.ContentType());
    http.maxBodyBytes = options_.maxAnswerBytes;
    platform::HttpResponse response = platform::HttpPost(http, body, cancel);
    reply.error = ClassifyApiResponse(response);
    reply.status = response.status;
    reply.body = std::move(response.body);
    return reply;
}

ApiError ApiClient::Transcribe(const TranscribeRequest& request, Transcript& transcript, platform::HttpCancel* cancel,
                               std::string* body) const {
    TranscribeRequest verbose = request;
    verbose.responseFormat = "verbose_json";
    ApiReply reply = TranscribeRaw(verbose, cancel);
    if (body != nullptr) {
        *body = reply.body;
    }
    if (!reply.error.Ok()) {
        return reply.error;
    }
    std::string error;
    if (!ParseVerboseJson(reply.body, transcript, error)) {
        ApiError failure = Failure(ApiErrorKind::kProtocol, "The answer is not verbose_json: " + error);
        failure.status = reply.status;
        return failure;
    }
    return reply.error;
}

ApiError ApiClient::Progress(ProgressReport& report, platform::HttpCancel* cancel) const {
    const platform::HttpResponse response =
        platform::HttpGet(BaseRequest("/progress", options_.requestTimeoutMs), cancel);
    ApiError error = ClassifyApiResponse(response);
    if (!error.Ok()) {
        return error;
    }
    std::string detail;
    if (!ParseProgress(response.body, report, detail)) {
        error.kind = ApiErrorKind::kProtocol;
        error.message = "GET /progress: " + detail;
    }
    return error;
}

}  // namespace crisp::core
