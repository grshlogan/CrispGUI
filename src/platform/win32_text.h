#pragma once

// Text helpers shared by the Windows implementations in src/platform/ (Windows builds only).

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <string>
#include <string_view>

namespace crisp::platform::win32 {

// UTF-8 to UTF-16 and back, as the wide-character Win32 APIs need (docs/CONVENTIONS.md).
std::wstring Widen(std::string_view text);
std::string Narrow(std::wstring_view text);

// The system's message for |error|, followed by its number: "Access is denied. (error 5)".
// |module| names a DLL with its own message table, such as winhttp.dll; nullptr for the system.
std::string ErrorMessage(DWORD error, HMODULE module = nullptr);

}  // namespace crisp::platform::win32
