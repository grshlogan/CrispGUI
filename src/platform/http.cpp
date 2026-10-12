#include "platform/http.h"

namespace crisp::platform {

bool HttpCancel::Cancelled() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return cancelled_;
}

bool HttpCancel::Attach(void* handle) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (cancelled_) {
        return false;
    }
    handle_ = handle;
    return true;
}

bool HttpCancel::Detach() {
    std::lock_guard<std::mutex> lock(mutex_);
    const bool attached = handle_ != nullptr;
    handle_ = nullptr;
    return attached || !cancelled_;
}

std::string HttpUrl(const std::string& host, int port, const std::string& path) {
    const bool ipv6 = host.find(':') != std::string::npos;
    return "http://" + (ipv6 ? "[" + host + "]" : host) + ":" + std::to_string(port) + path;
}

}  // namespace crisp::platform
