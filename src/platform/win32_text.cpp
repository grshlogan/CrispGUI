#include "platform/win32_text.h"

namespace crisp::platform::win32 {

std::wstring Widen(std::string_view text) {
    if (text.empty()) {
        return {};
    }
    const int length = static_cast<int>(text.size());
    const int size = MultiByteToWideChar(CP_UTF8, 0, text.data(), length, nullptr, 0);
    std::wstring result(static_cast<std::size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.data(), length, result.data(), size);
    return result;
}

std::string Narrow(std::wstring_view text) {
    if (text.empty()) {
        return {};
    }
    const int length = static_cast<int>(text.size());
    const int size = WideCharToMultiByte(CP_UTF8, 0, text.data(), length, nullptr, 0, nullptr, nullptr);
    std::string result(static_cast<std::size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.data(), length, result.data(), size, nullptr, nullptr);
    return result;
}

std::string ErrorMessage(DWORD error, HMODULE module) {
    wchar_t* buffer = nullptr;
    const DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_IGNORE_INSERTS |
                        (module != nullptr ? FORMAT_MESSAGE_FROM_HMODULE : FORMAT_MESSAGE_FROM_SYSTEM);
    const DWORD length = FormatMessageW(flags, module, error, 0, reinterpret_cast<wchar_t*>(&buffer), 0, nullptr);
    std::string text;
    if (length > 0 && buffer != nullptr) {
        text = Narrow(std::wstring_view(buffer, length));
        while (!text.empty() && (text.back() == '\n' || text.back() == '\r' || text.back() == ' ')) {
            text.pop_back();
        }
    }
    if (buffer != nullptr) {
        LocalFree(buffer);
    }
    if (text.empty()) {
        text = "Unknown error";
    }
    return text + " (error " + std::to_string(error) + ")";
}

}  // namespace crisp::platform::win32
