#include "platform/process.h"

#include "platform/win32_text.h"

#include <winsock2.h>
#include <iphlpapi.h>

#include <algorithm>
#include <chrono>
#include <mutex>
#include <vector>

namespace crisp::platform {
namespace {

using win32::ErrorMessage;
using win32::Narrow;
using win32::Widen;

// This process's environment as NAME=value entries in UTF-8.
std::vector<std::string> CurrentEnvironment() {
    std::vector<std::string> entries;
    wchar_t* block = GetEnvironmentStringsW();
    if (block == nullptr) {
        return entries;
    }
    for (const wchar_t* entry = block; *entry != L'\0';) {
        const std::wstring_view text(entry);
        entries.push_back(Narrow(text));
        entry += text.size() + 1;
    }
    FreeEnvironmentStringsW(block);
    return entries;
}

// A Unicode environment block: entries separated and terminated by NUL, then one more NUL.
std::wstring EnvironmentBlock(const std::vector<std::string>& entries) {
    std::wstring block;
    for (const std::string& entry : entries) {
        block += Widen(entry);
        block.push_back(L'\0');
    }
    if (block.empty()) {
        block.push_back(L'\0');
    }
    block.push_back(L'\0');
    return block;
}

std::wstring ProcessImagePath(HANDLE process) {
    std::vector<wchar_t> buffer(MAX_PATH);
    for (int attempt = 0; attempt < 4; ++attempt) {
        DWORD size = static_cast<DWORD>(buffer.size());
        if (QueryFullProcessImageNameW(process, 0, buffer.data(), &size)) {
            return std::wstring(buffer.data(), size);
        }
        if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
            break;
        }
        buffer.resize(buffer.size() * 4);
    }
    return {};
}

bool EqualIgnoringCase(const std::wstring& a, const std::wstring& b) {
    return CompareStringOrdinal(a.c_str(), static_cast<int>(a.size()), b.c_str(), static_cast<int>(b.size()), TRUE) ==
           CSTR_EQUAL;
}

// The owning process of a listener on |port| in one address family's TCP table.
template <typename Table>
bool FindListener(ULONG family, int port, DWORD& pid) {
    std::vector<unsigned char> buffer;
    DWORD size = 0;
    DWORD result = ERROR_INSUFFICIENT_BUFFER;
    for (int attempt = 0; attempt < 4 && result == ERROR_INSUFFICIENT_BUFFER; ++attempt) {
        buffer.resize(size);
        result = GetExtendedTcpTable(buffer.empty() ? nullptr : buffer.data(), &size, FALSE, family,
                                     TCP_TABLE_OWNER_PID_LISTENER, 0);
    }
    if (result != NO_ERROR || buffer.empty()) {
        return false;
    }
    const auto* table = reinterpret_cast<const Table*>(buffer.data());
    for (DWORD i = 0; i < table->dwNumEntries; ++i) {
        // The port sits in network byte order in the low 16 bits.
        const DWORD value = table->table[i].dwLocalPort;
        if (static_cast<int>(((value & 0xFF) << 8) | ((value >> 8) & 0xFF)) == port) {
            pid = table->table[i].dwOwningPid;
            return true;
        }
    }
    return false;
}

}  // namespace

struct ChildProcess::Impl {
    HANDLE process = nullptr;
    HANDLE output = nullptr;
    DWORD pid = 0;
    std::mutex mutex;
    bool waited = false;
    ExitStatus status;
};

ChildProcess::ChildProcess() : impl_(new Impl) {}

ChildProcess::~ChildProcess() {
    if (impl_->output != nullptr) {
        CloseHandle(impl_->output);
    }
    if (impl_->process != nullptr) {
        CloseHandle(impl_->process);
    }
}

bool ChildProcess::Start(const ProcessSpec& spec, std::string& error) {
    if (impl_->process != nullptr) {
        error = "The process was already started.";
        return false;
    }
    SECURITY_ATTRIBUTES inheritable = {sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
    HANDLE readEnd = nullptr;
    HANDLE writeEnd = nullptr;
    if (!CreatePipe(&readEnd, &writeEnd, &inheritable, 0)) {
        error = "Could not create a pipe: " + ErrorMessage(GetLastError());
        return false;
    }
    SetHandleInformation(readEnd, HANDLE_FLAG_INHERIT, 0);
    HANDLE nullDevice = CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &inheritable,
                                    OPEN_EXISTING, 0, nullptr);
    if (nullDevice == INVALID_HANDLE_VALUE) {
        error = "Could not open NUL: " + ErrorMessage(GetLastError());
        CloseHandle(readEnd);
        CloseHandle(writeEnd);
        return false;
    }

    // Only the two handles the child needs are inherited, whatever else is inheritable here.
    SIZE_T listSize = 0;
    InitializeProcThreadAttributeList(nullptr, 1, 0, &listSize);
    std::vector<unsigned char> listBuffer(listSize);
    auto* attributes = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(listBuffer.data());
    HANDLE inherited[] = {nullDevice, writeEnd};
    const bool listReady =
        InitializeProcThreadAttributeList(attributes, 1, 0, &listSize) &&
        UpdateProcThreadAttribute(attributes, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, inherited, sizeof(inherited),
                                  nullptr, nullptr);
    if (!listReady) {
        error = "Could not prepare the process attributes: " + ErrorMessage(GetLastError());
        CloseHandle(nullDevice);
        CloseHandle(readEnd);
        CloseHandle(writeEnd);
        return false;
    }

    STARTUPINFOEXW startup = {};
    startup.StartupInfo.cb = sizeof(startup);
    startup.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
    startup.StartupInfo.hStdInput = nullDevice;
    startup.StartupInfo.hStdOutput = writeEnd;
    startup.StartupInfo.hStdError = writeEnd;
    startup.lpAttributeList = attributes;

    std::wstring commandLine = Widen(BuildWindowsCommandLine(spec.executable.u8string(), spec.arguments));
    std::wstring environment = EnvironmentBlock(ApplyEnvironmentChanges(CurrentEnvironment(), spec.environment, true));
    const std::wstring directory = spec.workingDirectory.wstring();
    PROCESS_INFORMATION information = {};
    const BOOL started = CreateProcessW(spec.executable.c_str(), commandLine.data(), nullptr, nullptr, TRUE,
                                        CREATE_NO_WINDOW | CREATE_UNICODE_ENVIRONMENT | EXTENDED_STARTUPINFO_PRESENT,
                                        environment.data(), directory.empty() ? nullptr : directory.c_str(),
                                        &startup.StartupInfo, &information);
    const DWORD startError = GetLastError();
    DeleteProcThreadAttributeList(attributes);
    CloseHandle(nullDevice);
    CloseHandle(writeEnd);
    if (!started) {
        CloseHandle(readEnd);
        error = "Could not start " + spec.executable.u8string() + ": " + ErrorMessage(startError);
        return false;
    }
    CloseHandle(information.hThread);
    impl_->process = information.hProcess;
    impl_->pid = information.dwProcessId;
    impl_->output = readEnd;
    return true;
}

long ChildProcess::Id() const {
    return static_cast<long>(impl_->pid);
}

std::size_t ChildProcess::ReadOutput(char* buffer, std::size_t capacity, int timeoutMs, bool& ended) {
    ended = false;
    if (impl_->output == nullptr) {
        ended = true;
        return 0;
    }
    // Anonymous pipes have no overlapped reads, so wait by peeking; 10 ms steps keep the reader
    // responsive and cheap.
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    for (;;) {
        DWORD available = 0;
        if (!PeekNamedPipe(impl_->output, nullptr, 0, nullptr, &available, nullptr)) {
            ended = true;  // ERROR_BROKEN_PIPE: every writer has closed the pipe
            return 0;
        }
        if (available > 0) {
            DWORD count = 0;
            const auto wanted = static_cast<DWORD>(std::min<std::size_t>(capacity, available));
            if (!ReadFile(impl_->output, buffer, wanted, &count, nullptr)) {
                ended = true;
                return 0;
            }
            return count;
        }
        if (std::chrono::steady_clock::now() >= deadline) {
            return 0;
        }
        Sleep(10);
    }
}

bool ChildProcess::TryWait(ExitStatus& status) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (impl_->process == nullptr) {
        return false;
    }
    if (!impl_->waited && WaitForSingleObject(impl_->process, 0) == WAIT_OBJECT_0) {
        DWORD code = 0;
        GetExitCodeProcess(impl_->process, &code);
        impl_->waited = true;
        impl_->status = ExitStatus{false, static_cast<std::int64_t>(code)};
    }
    if (impl_->waited) {
        status = impl_->status;
    }
    return impl_->waited;
}

bool ChildProcess::Terminate(bool, std::string& error) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (impl_->process == nullptr || impl_->waited) {
        return true;
    }
    if (!TerminateProcess(impl_->process, 1)) {
        const DWORD terminateError = GetLastError();
        if (WaitForSingleObject(impl_->process, 0) == WAIT_OBJECT_0) {
            return true;  // it had exited already
        }
        error = "Could not end process " + std::to_string(impl_->pid) + ": " + ErrorMessage(terminateError);
        return false;
    }
    return true;
}

bool FindPortOwner(int port, PortOwner& owner, std::string&) {
    owner = PortOwner();
    DWORD pid = 0;
    if (!FindListener<MIB_TCPTABLE_OWNER_PID>(AF_INET, port, pid) &&
        !FindListener<MIB_TCP6TABLE_OWNER_PID>(AF_INET6, port, pid)) {
        return true;
    }
    owner.pid = static_cast<long>(pid);
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (process != nullptr) {
        owner.executable = std::filesystem::path(ProcessImagePath(process));
        CloseHandle(process);
    }
    return true;
}

bool TerminatePortOwner(const PortOwner& owner, int timeoutMs, std::string& error) {
    const std::string pid = std::to_string(owner.pid);
    HANDLE process = OpenProcess(PROCESS_TERMINATE | PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE, FALSE,
                                 static_cast<DWORD>(owner.pid));
    if (process == nullptr) {
        const DWORD openError = GetLastError();
        error = "Could not open process " + pid + ": " + ErrorMessage(openError);
        if (openError == ERROR_ACCESS_DENIED) {
            error += ". It may run as administrator.";
        }
        return false;
    }
    // The handle pins the process, so the check and the termination concern the same one.
    if (!EqualIgnoringCase(ProcessImagePath(process), owner.executable.wstring())) {
        CloseHandle(process);
        error = "Process " + pid + " is no longer " + owner.executable.u8string() + ".";
        return false;
    }
    bool ok = true;
    if (!TerminateProcess(process, 1)) {
        error = "Could not end process " + pid + ": " + ErrorMessage(GetLastError());
        ok = false;
    } else if (WaitForSingleObject(process, static_cast<DWORD>(timeoutMs)) != WAIT_OBJECT_0) {
        error = "Process " + pid + " did not exit within " + std::to_string(timeoutMs / 1000) + " s.";
        ok = false;
    }
    CloseHandle(process);
    return ok;
}

bool SameFileName(const std::filesystem::path& a, const std::filesystem::path& b) {
    return EqualIgnoringCase(a.filename().wstring(), b.filename().wstring());
}

}  // namespace crisp::platform
