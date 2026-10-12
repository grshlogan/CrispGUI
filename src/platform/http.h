#pragma once

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

// Platform seam for HTTP to the local CrispASR server: WinHTTP on Windows (http_win32.cpp) and
// libcurl elsewhere (http_curl.cpp), both without proxies (docs/ARCHITECTURE.md, section 2.3).
// GET serves /health, /v1/models, and /progress; POST streams the transcription upload.
namespace crisp::platform {

enum class HttpFailure {
    kNone,
    kRefused,    // nothing accepts connections on the port
    kTimeout,    // no complete answer in time
    kCancelled,  // HttpCancel::Cancel() ended the request
    kTooLarge,   // the answer is larger than HttpRequest::maxBodyBytes
    kBody,       // the request body could not be produced, such as a file that changed
    kOther,      // a reset or closed connection, an invalid response, or another transport error
};

struct HttpRequest {
    std::string host;  // a host name or an IPv4 address
    int port = 0;
    std::string path = "/";
    std::vector<std::pair<std::string, std::string>> headers;
    int connectTimeoutMs = 5000;
    // For sending the request and receiving the answer. WinHTTP applies it to each send and to
    // the wait for the answer, so an upload can take this long and the wait as long again;
    // libcurl limits the whole exchange, connecting included, to connectTimeoutMs + timeoutMs.
    int timeoutMs = 5000;
    std::size_t maxBodyBytes = 1024 * 1024;
};

// A request body that the transport pulls in pieces, so that a large file is never held in
// memory whole.
class HttpBody {
public:
    virtual ~HttpBody() = default;

    // The total size in bytes, sent as Content-Length.
    virtual std::uint64_t Size() const = 0;
    // Copies up to |capacity| of the next bytes into |buffer| and sets |count|, which is 0 only
    // after the last byte. Returns false with |error| set when the bytes cannot be produced.
    virtual bool Read(char* buffer, std::size_t capacity, std::size_t& count, std::string& error) = 0;
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

// Sends a POST request with |body|, whose type belongs in request.headers ("Content-Type"), and
// reads the whole answer. No "Expect: 100-continue": the body follows the headers at once.
HttpResponse HttpPost(const HttpRequest& request, HttpBody& body, HttpCancel* cancel = nullptr);

// "http://host:port/path", with brackets around an IPv6 address.
std::string HttpUrl(const std::string& host, int port, const std::string& path);

}  // namespace crisp::platform
