#pragma once

#include "core/server_state.h"
#include "core/transcript.h"
#include "platform/http.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

// Typed calls to the CrispASR server (docs/ARCHITECTURE.md, section 2.3; the contract is
// docs/API-CRISPASR.md). /health and /v1/models belong to the ServerManager's poller (section 2.2).
namespace crisp::core {

// The largest request body CrispASR 0.8.42 accepts: set_payload_max_length(512 MiB)
// (crispasr_server.cpp:1547; docs/API-CRISPASR.md, section 6, item 7).
constexpr std::uint64_t kServerMaxUploadBytes = 512ull * 1024 * 1024;

// The failures of a request, after docs/API-CRISPASR.md, section 5. The ASR runner decides which
// ones to retry (docs/ARCHITECTURE.md, section 2.5).
enum class ApiErrorKind {
    kNone,
    kUnreachable,      // no answer: refused, reset, or closed; the server is stopped, starting, or gone
    kTimeout,          // no complete answer in time
    kCancelled,        // the caller cancelled the request
    kBadRequest,       // 400, or a request the client cannot send: the server cannot use the request
    kUnauthorized,     // 401: a missing or wrong API key
    kPayloadTooLarge,  // 413, or an upload over kServerMaxUploadBytes, which is then not sent
    kServerBusy,       // 503: a model swap through /load is in progress
    kServerError,      // 500: an exception inside the server
    kProtocol,         // another status, or an answer that is not what the endpoint returns
    kFile,             // the file to upload cannot be opened or read
};

struct ApiError {
    ApiErrorKind kind = ApiErrorKind::kNone;
    int status = 0;       // the HTTP status when the server answered
    std::string message;  // the server's error message, or what went wrong, in English

    bool Ok() const { return kind == ApiErrorKind::kNone; }
};

// "none", "unreachable", "timeout", ... for logs and reports.
const char* ApiErrorKindName(ApiErrorKind kind);

// Types an exchange with the server: a transport failure or an HTTP status other than 200, with
// the message of the error body when it has one.
ApiError ClassifyApiResponse(const platform::HttpResponse& response);

// The message of an error body: error.message of {"error": {"message": ...}}, the string of
// {"error": "..."}, or the start of a body that is not JSON. Empty for an empty body.
std::string ErrorBodyMessage(const std::string& body);

struct ProgressReport {
    bool busy = false;
    int progress = -1;  // 0..100 while a transcription runs, -1 when idle (docs/API-CRISPASR.md, section 7)
};

// Reads a GET /progress body: {"busy": bool, "progress": N}.
bool ParseProgress(const std::string& body, ProgressReport& report, std::string& error);

struct TranscribeRequest {
    std::filesystem::path file;
    // Request-tier fields, sent in this order after response_format and the window. The client
    // sets file, response_format, offset_t_ms, and duration_ms itself.
    std::vector<std::pair<std::string, std::string>> fields;
    std::int64_t offsetMs = 0;    // the window's start
    std::int64_t durationMs = 0;  // the window's length; 0 sends no window, so the whole file
    std::string responseFormat = "verbose_json";
};

// An answer as the server sent it.
struct ApiReply {
    ApiError error;
    int status = 0;    // the HTTP status when the server answered
    std::string body;  // the answer, or the error body of a 4xx or 5xx status
};

struct ApiClientOptions {
    ServerEndpoint endpoint;
    std::string apiKey;  // sent as "Authorization: Bearer <key>"; no header when empty
    int connectTimeoutMs = 5000;
    int requestTimeoutMs = 5000;  // GET /progress
    // One transcription request: the upload, the inference, and the answer (with WinHTTP the
    // upload and the wait each; platform/http.h).
    int transcribeTimeoutMs = 30 * 60 * 1000;
    std::size_t maxAnswerBytes = 64 * 1024 * 1024;
    std::uint64_t maxUploadBytes = kServerMaxUploadBytes;
};

// The calls block, so they run on worker threads. The object holds nothing but its options, so
// threads can share it; each call takes its own HttpCancel.
class ApiClient {
public:
    explicit ApiClient(ApiClientOptions options);

    const ApiClientOptions& Options() const { return options_; }

    // POST /v1/audio/transcriptions with the file streamed from disk; the answer unparsed.
    ApiReply TranscribeRaw(const TranscribeRequest& request, platform::HttpCancel* cancel = nullptr) const;
    // The same with response_format=verbose_json, parsed into |transcript|; an answer that does
    // not parse is kProtocol. |body| receives the answer as sent, when given.
    ApiError Transcribe(const TranscribeRequest& request, Transcript& transcript,
                        platform::HttpCancel* cancel = nullptr, std::string* body = nullptr) const;
    // GET /progress.
    ApiError Progress(ProgressReport& report, platform::HttpCancel* cancel = nullptr) const;

private:
    platform::HttpRequest BaseRequest(const std::string& path, int timeoutMs) const;

    ApiClientOptions options_;
};

}  // namespace crisp::core
