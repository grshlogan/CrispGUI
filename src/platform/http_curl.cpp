#include "platform/http.h"

#include <curl/curl.h>

#include <mutex>

namespace crisp::platform {
namespace {

struct Transfer {
    std::string* body = nullptr;
    std::size_t maxBodyBytes = 0;
    bool tooLarge = false;
    HttpCancel* cancel = nullptr;
};

std::size_t WriteBody(char* data, std::size_t size, std::size_t count, void* user) {
    auto* transfer = static_cast<Transfer*>(user);
    const std::size_t bytes = size * count;
    if (transfer->body->size() + bytes > transfer->maxBodyBytes) {
        transfer->tooLarge = true;
        return 0;  // makes libcurl stop with CURLE_WRITE_ERROR
    }
    transfer->body->append(data, bytes);
    return bytes;
}

// libcurl calls this regularly, also while connecting; a non-zero result aborts the transfer.
int CheckCancel(void* user, curl_off_t, curl_off_t, curl_off_t, curl_off_t) {
    const auto* transfer = static_cast<const Transfer*>(user);
    return transfer->cancel != nullptr && transfer->cancel->Cancelled() ? 1 : 0;
}

}  // namespace

void HttpCancel::Cancel() {
    std::lock_guard<std::mutex> lock(mutex_);
    cancelled_ = true;
}

HttpResponse HttpGet(const HttpRequest& request, HttpCancel* cancel) {
    static std::once_flag initialized;
    std::call_once(initialized, [] { curl_global_init(CURL_GLOBAL_DEFAULT); });

    HttpResponse response;
    if (cancel != nullptr && cancel->Cancelled()) {
        response.failure = HttpFailure::kCancelled;
        response.error = "Cancelled.";
        return response;
    }
    CURL* curl = curl_easy_init();
    if (curl == nullptr) {
        response.failure = HttpFailure::kOther;
        response.error = "libcurl could not create a request.";
        return response;
    }

    Transfer transfer;
    transfer.body = &response.body;
    transfer.maxBodyBytes = request.maxBodyBytes;
    transfer.cancel = cancel;
    curl_slist* headers = nullptr;
    for (const auto& header : request.headers) {
        headers = curl_slist_append(headers, (header.first + ": " + header.second).c_str());
    }
    const std::string url = HttpUrl(request.host, request.port, request.path);
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_PROTOCOLS_STR, "http");
    curl_easy_setopt(curl, CURLOPT_PROXY, "");  // never a proxy, whatever the environment says
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 0L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, static_cast<long>(request.connectTimeoutMs));
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, static_cast<long>(request.connectTimeoutMs + request.timeoutMs));
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteBody);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &transfer);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, CheckCancel);
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &transfer);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);

    const CURLcode result = curl_easy_perform(curl);
    if (result == CURLE_OK) {
        long status = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
        response.status = static_cast<int>(status);
    } else {
        switch (result) {
        case CURLE_COULDNT_CONNECT:
            response.failure = HttpFailure::kRefused;
            break;
        case CURLE_OPERATION_TIMEDOUT:
            response.failure = HttpFailure::kTimeout;
            break;
        case CURLE_ABORTED_BY_CALLBACK:
            response.failure = HttpFailure::kCancelled;
            break;
        default:
            response.failure = HttpFailure::kOther;
            break;
        }
        response.error = transfer.tooLarge ? "The answer is larger than " + std::to_string(request.maxBodyBytes) + " bytes."
                                           : std::string(curl_easy_strerror(result)) + ".";
        response.body.clear();
    }
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return response;
}

}  // namespace crisp::platform
