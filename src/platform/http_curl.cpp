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
    HttpBody* upload = nullptr;
    bool uploadFailed = false;
    std::string uploadError;
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

std::size_t ReadUpload(char* buffer, std::size_t size, std::size_t count, void* user) {
    auto* transfer = static_cast<Transfer*>(user);
    if (transfer->cancel != nullptr && transfer->cancel->Cancelled()) {
        return CURL_READFUNC_ABORT;
    }
    std::size_t produced = 0;
    if (!transfer->upload->Read(buffer, size * count, produced, transfer->uploadError)) {
        transfer->uploadFailed = true;
        if (transfer->uploadError.empty()) {
            transfer->uploadError = "The request body could not be produced.";
        }
        return CURL_READFUNC_ABORT;
    }
    return produced;
}

// libcurl calls this regularly, also while connecting; a non-zero result aborts the transfer.
int CheckCancel(void* user, curl_off_t, curl_off_t, curl_off_t, curl_off_t) {
    const auto* transfer = static_cast<const Transfer*>(user);
    return transfer->cancel != nullptr && transfer->cancel->Cancelled() ? 1 : 0;
}

// One request: GET without |upload|, POST with it.
HttpResponse Perform(const HttpRequest& request, HttpBody* upload, HttpCancel* cancel) {
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
    transfer.upload = upload;
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
    if (upload != nullptr) {
        // An empty "Expect:" keeps libcurl from waiting for "100 Continue" before large bodies,
        // so both transports send the body the same way.
        headers = curl_slist_append(headers, "Expect:");
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_READFUNCTION, ReadUpload);
        curl_easy_setopt(curl, CURLOPT_READDATA, &transfer);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE_LARGE, static_cast<curl_off_t>(upload->Size()));
    }
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
        response.error = std::string(curl_easy_strerror(result)) + ".";
        if (transfer.tooLarge) {
            response.failure = HttpFailure::kTooLarge;
            response.error = "The answer is larger than " + std::to_string(request.maxBodyBytes) + " bytes.";
        } else if (transfer.uploadFailed) {
            response.failure = HttpFailure::kBody;
            response.error = transfer.uploadError;
        } else if (cancel != nullptr && cancel->Cancelled()) {
            response.failure = HttpFailure::kCancelled;
        } else if (result == CURLE_COULDNT_CONNECT) {
            response.failure = HttpFailure::kRefused;
        } else if (result == CURLE_OPERATION_TIMEDOUT) {
            response.failure = HttpFailure::kTimeout;
        } else {
            response.failure = HttpFailure::kOther;
        }
        response.body.clear();
    }
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return response;
}

}  // namespace

void HttpCancel::Cancel() {
    std::lock_guard<std::mutex> lock(mutex_);
    cancelled_ = true;
}

HttpResponse HttpGet(const HttpRequest& request, HttpCancel* cancel) {
    return Perform(request, nullptr, cancel);
}

HttpResponse HttpPost(const HttpRequest& request, HttpBody& body, HttpCancel* cancel) {
    return Perform(request, &body, cancel);
}

}  // namespace crisp::platform
