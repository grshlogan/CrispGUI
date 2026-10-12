#include "platform/http.h"

#include "platform/win32_text.h"

#include <winhttp.h>

namespace crisp::platform {
namespace {

using win32::ErrorMessage;
using win32::Widen;

HttpFailure FailureFor(DWORD error) {
    switch (error) {
    case ERROR_WINHTTP_CANNOT_CONNECT:
        return HttpFailure::kRefused;
    case ERROR_WINHTTP_TIMEOUT:
        return HttpFailure::kTimeout;
    case ERROR_WINHTTP_OPERATION_CANCELLED:
        return HttpFailure::kCancelled;
    default:
        return HttpFailure::kOther;
    }
}

class InternetHandle {
public:
    explicit InternetHandle(HINTERNET handle) : handle_(handle) {}
    ~InternetHandle() {
        if (handle_ != nullptr) {
            WinHttpCloseHandle(handle_);
        }
    }
    InternetHandle(const InternetHandle&) = delete;
    InternetHandle& operator=(const InternetHandle&) = delete;
    HINTERNET get() const { return handle_; }

private:
    HINTERNET handle_;
};

// Status and body of an answered request; false with GetLastError() set on failure.
bool ReadAnswer(HINTERNET request, std::size_t maxBodyBytes, HttpResponse& response, bool& tooLarge) {
    DWORD status = 0;
    DWORD size = sizeof(status);
    if (!WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                             WINHTTP_HEADER_NAME_BY_INDEX, &status, &size, WINHTTP_NO_HEADER_INDEX)) {
        return false;
    }
    response.status = static_cast<int>(status);
    for (;;) {
        DWORD available = 0;
        if (!WinHttpQueryDataAvailable(request, &available)) {
            return false;
        }
        if (available == 0) {
            return true;
        }
        if (response.body.size() + available > maxBodyBytes) {
            tooLarge = true;
            return true;
        }
        const std::size_t offset = response.body.size();
        response.body.resize(offset + available);
        DWORD count = 0;
        if (!WinHttpReadData(request, &response.body[offset], available, &count)) {
            return false;
        }
        response.body.resize(offset + count);
        if (count == 0) {
            return true;
        }
    }
}

}  // namespace

void HttpCancel::Cancel() {
    std::lock_guard<std::mutex> lock(mutex_);
    cancelled_ = true;
    if (handle_ != nullptr) {
        // Closing the request handle is WinHTTP's documented way to end a synchronous call in
        // progress on another thread; that call then fails.
        WinHttpCloseHandle(static_cast<HINTERNET>(handle_));
        handle_ = nullptr;
    }
}

HttpResponse HttpGet(const HttpRequest& request, HttpCancel* cancel) {
    HttpResponse response;
    const HMODULE module = GetModuleHandleW(L"winhttp.dll");
    const auto fail = [&](DWORD error) {
        response.failure = cancel != nullptr && cancel->Cancelled() ? HttpFailure::kCancelled : FailureFor(error);
        response.error = ErrorMessage(error, module);
        response.status = 0;
        response.body.clear();
        return response;
    };

    InternetHandle session(WinHttpOpen(L"CrispRustGUI", WINHTTP_ACCESS_TYPE_NO_PROXY, WINHTTP_NO_PROXY_NAME,
                                       WINHTTP_NO_PROXY_BYPASS, 0));
    if (session.get() == nullptr) {
        return fail(GetLastError());
    }
    WinHttpSetTimeouts(session.get(), request.connectTimeoutMs, request.connectTimeoutMs, request.timeoutMs,
                       request.timeoutMs);
    InternetHandle connection(
        WinHttpConnect(session.get(), Widen(request.host).c_str(), static_cast<INTERNET_PORT>(request.port), 0));
    if (connection.get() == nullptr) {
        return fail(GetLastError());
    }
    HINTERNET handle = WinHttpOpenRequest(connection.get(), L"GET", Widen(request.path).c_str(), nullptr,
                                          WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
    if (handle == nullptr) {
        return fail(GetLastError());
    }
    DWORD redirects = WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
    WinHttpSetOption(handle, WINHTTP_OPTION_REDIRECT_POLICY, &redirects, sizeof(redirects));
    if (cancel != nullptr && !cancel->Attach(handle)) {
        WinHttpCloseHandle(handle);
        return fail(ERROR_WINHTTP_OPERATION_CANCELLED);
    }

    std::wstring headers;
    for (const auto& header : request.headers) {
        headers += Widen(header.first + ": " + header.second + "\r\n");
    }
    bool tooLarge = false;
    const bool answered =
        WinHttpSendRequest(handle, headers.empty() ? WINHTTP_NO_ADDITIONAL_HEADERS : headers.c_str(),
                           headers.empty() ? 0 : static_cast<DWORD>(-1L), WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
        WinHttpReceiveResponse(handle, nullptr) && ReadAnswer(handle, request.maxBodyBytes, response, tooLarge);
    const DWORD error = answered ? ERROR_SUCCESS : GetLastError();
    if (cancel == nullptr || cancel->Detach()) {
        WinHttpCloseHandle(handle);
    }
    if (!answered) {
        return fail(error);
    }
    if (tooLarge) {
        response.failure = HttpFailure::kOther;
        response.error = "The answer is larger than " + std::to_string(request.maxBodyBytes) + " bytes.";
        response.status = 0;
        response.body.clear();
    }
    return response;
}

}  // namespace crisp::platform
