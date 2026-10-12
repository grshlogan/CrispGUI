#pragma once

#include <cstddef>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

// Platform seam for HTTP to the local CrispASR server: WinHTTP on Windows (http_win32.cpp) and
// libcurl elsewhere (http_curl.cpp), both without proxies (docs/ARCHITECTURE.md, section 2.3).
// Phase 3 needs GET for /health and /v1/models; Phase 4 adds the multipart upload.
namespace crisp::platform {

enum class HttpFailure {
    kNone,
    kRefused,    // nothing accepts connections on the port
    kTimeout,    // no complete answer in time
    kCancelled,  // HttpCancel::Cancel() ended the request
    kOther,      // a reset connection, an invalid response, or another transport error
};

struct HttpRequest {
    std::string host;  // a host name or an IPv4 address
    int port = 0;
    std::string path = "/";
    std::vector<std::pair<std::string, std::string>> headers;
    int connectTimeoutMs = 5000;
    int timeoutMs = 5000;  // for sending the request and receiving the answer
    std::size_t maxBodyBytes = 1024 * 1024;
};

struct HttpResponse {
    HttpFailure failure = HttpFailure::kNone;
    std::string error;  // the transport's description when |failure| is set
    int status = 0;
    std::string body;

    bool Ok() const { return failure == HttpFailure::kNone; }
};

// Lets another thread end requests: Cancel() aborts the request in progress and makes later
// requests fail at once.
class HttpCancel {
public:
    HttpCancel() = default;
    HttpCancel(const HttpCancel&) = delete;
    HttpCancel& operator=(const HttpCancel&) = delete;

    void Cancel();
    bool Cancelled() const;

    // For the transports: registers the native handle of the request in progress, which Cancel()
    // may close. Returns false when the request is already cancelled.
    bool Attach(void* handle);
    // Unregisters it. Returns false when Cancel() closed it, so the caller must not close it.
    bool Detach();

private:
    mutable std::mutex mutex_;
    void* handle_ = nullptr;
    bool cancelled_ = false;
};

// Sends a GET request and reads the whole answer. Never throws; failures are in the response.
HttpResponse HttpGet(const HttpRequest& request, HttpCancel* cancel = nullptr);

// "http://host:port/path", with brackets around an IPv6 address.
std::string HttpUrl(const std::string& host, int port, const std::string& path);

}  // namespace crisp::platform
