#include "platform/paths.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <cstdlib>
#include <cwctype>
#include <initializer_list>
#include <vector>

namespace crisp::platform {
namespace {

std::wstring Widen(const char* text) {
    const int size = MultiByteToWideChar(CP_UTF8, 0, text, -1, nullptr, 0);
    if (size <= 1) {
        return {};
    }
    std::wstring result(static_cast<std::size_t>(size - 1), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text, -1, result.data(), size);
    return result;
}

std::string Narrow(const std::wstring& text) {
    if (text.empty()) {
        return {};
    }
    const int length = static_cast<int>(text.size());
    const int size = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), length, nullptr, 0, nullptr, nullptr);
    std::string result(static_cast<std::size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.c_str(), length, result.data(), size, nullptr, nullptr);
    return result;
}

std::wstring GetEnvironmentWide(const wchar_t* name) {
    const DWORD size = GetEnvironmentVariableW(name, nullptr, 0);
    if (size == 0) {
        return {};
    }
    std::wstring value(size, L'\0');
    const DWORD length = GetEnvironmentVariableW(name, value.data(), size);
    value.resize(length < size ? length : 0);
    return value;
}

}  // namespace

std::filesystem::path ExecutableDirectory() {
    std::vector<wchar_t> buffer(MAX_PATH);
    for (;;) {
        const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (length == 0) {
            return {};
        }
        if (length < buffer.size()) {
            return std::filesystem::path(std::wstring(buffer.data(), length)).parent_path();
        }
        buffer.resize(buffer.size() * 2);
    }
}

bool SetProcessTempDirectory(const std::filesystem::path& directory) {
    const std::wstring value = directory.wstring();
    bool ok = true;
    for (const wchar_t* name : {L"TMP", L"TEMP"}) {
        ok = SetEnvironmentVariableW(name, value.c_str()) != 0 && ok;
        // Keep the C runtime's copy of the environment in step with the process block.
        const std::wstring assignment = std::wstring(name) + L"=" + value;
        ok = _wputenv(assignment.c_str()) == 0 && ok;
    }
    return ok;
}

std::string GetEnvironmentUtf8(const char* name) {
    return Narrow(GetEnvironmentWide(Widen(name).c_str()));
}

bool IsOnSystemDrive(const std::filesystem::path& path) {
    std::wstring systemDrive = GetEnvironmentWide(L"SystemDrive");
    if (systemDrive.empty()) {
        systemDrive = L"C:";
    }
    const std::wstring drive = path.root_name().wstring();
    if (drive.size() != systemDrive.size()) {
        return false;
    }
    for (std::size_t i = 0; i < drive.size(); ++i) {
        if (std::towupper(drive[i]) != std::towupper(systemDrive[i])) {
            return false;
        }
    }
    return true;
}

}  // namespace crisp::platform
