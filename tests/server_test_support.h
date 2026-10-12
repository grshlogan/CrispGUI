#pragma once

// Helpers for the tests that run tests/stub_crispasr.cpp as a server process.

#include "test_support.h"

#include "platform/http.h"
#include "platform/paths.h"
#include "platform/process.h"

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include <chrono>
#include <filesystem>
#include <functional>
#include <string>
#include <system_error>
#include <thread>
#include <vector>

namespace crisp::test {

#if defined(_WIN32)
constexpr const char* kExecutableSuffix = ".exe";
#else
constexpr const char* kExecutableSuffix = "";
#endif

// The stub server the build places next to the tests.
inline std::filesystem::path StubExecutable() {
    return platform::ExecutableDirectory() / (std::string("stub_crispasr") + kExecutableSuffix);
}

// Copies the stub to |destination|, such as <root>/bin/crispasr.exe.
inline bool CopyStub(const std::filesystem::path& destination) {
    std::error_code error;
    std::filesystem::create_directories(destination.parent_path(), error);
    std::filesystem::copy_file(StubExecutable(), destination, std::filesystem::copy_options::overwrite_existing,
                               error);
    if (error) {
        return false;
    }
    std::filesystem::permissions(destination,
                                 std::filesystem::perms::owner_all | std::filesystem::perms::group_read |
                                     std::filesystem::perms::group_exec | std::filesystem::perms::others_read |
                                     std::filesystem::perms::others_exec,
                                 error);
    return true;
}

// A TCP port on 127.0.0.1 that was free a moment ago.
inline int FreePort() {
#if defined(_WIN32)
    static const bool started = [] {
        WSADATA data;
        return WSAStartup(MAKEWORD(2, 2), &data) == 0;
    }();
    (void)started;
    using Length = int;
#else
    using Length = socklen_t;
#endif
    const auto handle = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    sockaddr_in address = {};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = 0;
    Length length = sizeof(address);
    int port = 0;
    if (bind(handle, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) == 0 &&
        getsockname(handle, reinterpret_cast<sockaddr*>(&address), &length) == 0) {
        port = ntohs(address.sin_port);
    }
#if defined(_WIN32)
    closesocket(handle);
#else
    close(handle);
#endif
    return port;
}

// Polls |done| every 20 ms until it holds or |timeoutMs| passes.
inline bool WaitUntil(const std::function<bool()>& done, int timeoutMs) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    while (!done()) {
        if (std::chrono::steady_clock::now() >= deadline) {
            return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    return true;
}

inline platform::HttpResponse Get(int port, const std::string& path,
                                  std::vector<std::pair<std::string, std::string>> headers = {}) {
    platform::HttpRequest request;
    request.host = "127.0.0.1";
    request.port = port;
    request.path = path;
    request.headers = std::move(headers);
    return platform::HttpGet(request);
}

// Waits until something answers GET /health on |port| with any HTTP status.
inline bool WaitForAnswer(int port, int timeoutMs) {
    return WaitUntil([port] { return Get(port, "/health").Ok(); }, timeoutMs);
}

// All output of |process| until it exits, and its exit status.
inline std::string ReadUntilExit(platform::ChildProcess& process, platform::ExitStatus& status, int timeoutMs) {
    std::string output;
    char buffer[4096];
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    bool exited = false;
    for (;;) {
        bool ended = false;
        const std::size_t count = process.ReadOutput(buffer, sizeof(buffer), 50, ended);
        output.append(buffer, count);
        if (!exited) {
            exited = process.TryWait(status);
        }
        if ((exited && ended) || std::chrono::steady_clock::now() >= deadline) {
            return output;
        }
    }
}

// Starts the stub on |port| with |environment| and the server's flags.
inline bool StartStub(platform::ChildProcess& process, const std::filesystem::path& executable, int port,
                      std::vector<platform::EnvironmentChange> environment) {
    platform::ProcessSpec spec;
    spec.executable = executable;
    spec.arguments = {"--server", "--backend", "stubby", "-m", "stub-model.gguf", "--host", "127.0.0.1", "--port",
                      std::to_string(port)};
    spec.environment = std::move(environment);
    std::string error;
    return process.Start(spec, error);
}

// Ends |process| if it still runs and waits for its exit.
inline void EndProcess(platform::ChildProcess& process) {
    std::string error;
    process.Terminate(true, error);
    platform::ExitStatus status;
    WaitUntil([&] { return process.TryWait(status); }, 5000);
}

}  // namespace crisp::test
