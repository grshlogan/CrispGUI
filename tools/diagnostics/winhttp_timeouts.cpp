// winhttp_timeouts: measures which WinHTTP timeout settings end a synchronous request whose answer
// comes late. A server inside the process answers on 127.0.0.1 after a delay the request names;
// each case sets some of the timeouts, reads back what the request handle holds, and reports how
// long WinHttpReceiveResponse took and how it ended. Windows only; it talks only to 127.0.0.1 and
// writes nothing. Built after the Phase 4 Windows run, where a 1000 ms limit did not end a
// request answered after 3000 ms (docs/devlog/phase-4.md).
//
// Usage: winhttp_timeouts [--long]
//   The short cases ask for an answer after 3000 ms with every limit at 1000 ms (about 30 s in
//   all). --long adds cases whose answer comes after 95 s, run side by side (about 100 s): WinHTTP's
//   defaults, and limits of 120 s set in several places.

#include "platform/http.h"
#include "platform/win32_text.h"

#include <winhttp.h>
#include <winsock2.h>
#include <winver.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

namespace {

using crisp::platform::win32::ErrorMessage;
using Clock = std::chrono::steady_clock;

// Where a case sets its limit.
struct Case {
    const char* name;
    bool sessionTimeouts = false;   // WinHttpSetTimeouts on the session handle
    bool requestTimeouts = false;   // WinHttpSetTimeouts on the request handle
    bool sessionResponse = false;   // WINHTTP_OPTION_RECEIVE_RESPONSE_TIMEOUT on the session handle
    bool requestResponse = false;   // WINHTTP_OPTION_RECEIVE_RESPONSE_TIMEOUT on the request handle
    bool requestReceive = false;    // WINHTTP_OPTION_RECEIVE_TIMEOUT on the request handle
    bool post = false;              // a 64 KiB body through WinHttpWriteData, as HttpPost sends one
    int limitMs = 1000;
    int delayMs = 3000;
};

struct Outcome {
    std::string settings;  // what each setting call returned
    std::string readback;  // the request handle's limits just before sending
    double elapsedMs = 0;
    std::string result;
};

// The server: reads one request, waits the milliseconds named by its path (/slow/<ms>), answers 200.
void Serve(SOCKET client) {
    std::string data;
    char buffer[65536];
    std::size_t headerEnd = std::string::npos;
    while ((headerEnd = data.find("\r\n\r\n")) == std::string::npos) {
        const int count = recv(client, buffer, sizeof(buffer), 0);
        if (count <= 0) {
            closesocket(client);
            return;
        }
        data.append(buffer, static_cast<std::size_t>(count));
    }
    std::size_t length = 0;
    std::string lower = data.substr(0, headerEnd);
    for (char& c : lower) {
        c = static_cast<char>(c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c);
    }
    const std::size_t field = lower.find("\r\ncontent-length:");
    if (field != std::string::npos) {
        length = std::strtoul(lower.c_str() + field + 17, nullptr, 10);
    }
    std::size_t received = data.size() - (headerEnd + 4);
    while (received < length) {
        const int count = recv(client, buffer, sizeof(buffer), 0);
        if (count <= 0) {
            closesocket(client);
            return;
        }
        received += static_cast<std::size_t>(count);
    }
    const std::size_t slash = data.find("/slow/");
    const int delay = slash != std::string::npos ? std::atoi(data.c_str() + slash + 6) : 0;
    Sleep(static_cast<DWORD>(delay));
    const char answer[] = "HTTP/1.1 200 OK\r\nContent-Length: 2\r\nConnection: close\r\n\r\nok";
    send(client, answer, static_cast<int>(sizeof(answer) - 1), 0);
    closesocket(client);
}

int StartServer() {
    WSADATA data;
    WSAStartup(MAKEWORD(2, 2), &data);
    const SOCKET listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    sockaddr_in address = {};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    int length = sizeof(address);
    if (listener == INVALID_SOCKET || bind(listener, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0 ||
        listen(listener, 16) != 0 || getsockname(listener, reinterpret_cast<sockaddr*>(&address), &length) != 0) {
        return 0;
    }
    std::thread([listener] {
        for (;;) {
            const SOCKET client = accept(listener, nullptr, nullptr);
            if (client == INVALID_SOCKET) {
                return;
            }
            std::thread(Serve, client).detach();
        }
    }).detach();
    return ntohs(address.sin_port);
}

std::string Describe(BOOL ok) {
    return ok ? "ok" : "failed " + std::to_string(GetLastError());
}

std::string Query(HINTERNET handle, DWORD option) {
    DWORD value = 0;
    DWORD size = sizeof(value);
    if (!WinHttpQueryOption(handle, option, &value, &size)) {
        return "? (error " + std::to_string(GetLastError()) + ")";
    }
    return std::to_string(value);
}

Outcome Run(const Case& c, int port) {
    Outcome outcome;
    const HMODULE module = GetModuleHandleW(L"winhttp.dll");
    const DWORD limit = static_cast<DWORD>(c.limitMs);
    HINTERNET session =
        WinHttpOpen(L"winhttp_timeouts", WINHTTP_ACCESS_TYPE_NO_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (session == nullptr) {
        outcome.result = "WinHttpOpen failed " + std::to_string(GetLastError());
        return outcome;
    }
    if (c.sessionTimeouts) {
        outcome.settings += "session SetTimeouts " + Describe(WinHttpSetTimeouts(session, c.limitMs, c.limitMs,
                                                                                    c.limitMs, c.limitMs)) + "; ";
    }
    if (c.sessionResponse) {
        DWORD value = limit;
        outcome.settings += "session RECEIVE_RESPONSE_TIMEOUT " +
                            Describe(WinHttpSetOption(session, WINHTTP_OPTION_RECEIVE_RESPONSE_TIMEOUT, &value,
                                                      sizeof(value))) + "; ";
    }
    HINTERNET connection = WinHttpConnect(session, L"127.0.0.1", static_cast<INTERNET_PORT>(port), 0);
    const std::wstring path = L"/slow/" + std::to_wstring(c.delayMs);
    HINTERNET request = connection == nullptr
                            ? nullptr
                            : WinHttpOpenRequest(connection, c.post ? L"POST" : L"GET", path.c_str(), nullptr,
                                                 WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
    if (request == nullptr) {
        outcome.result = "WinHttpConnect or WinHttpOpenRequest failed " + std::to_string(GetLastError());
        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);
        return outcome;
    }
    if (c.requestTimeouts) {
        outcome.settings += "request SetTimeouts " + Describe(WinHttpSetTimeouts(request, c.limitMs, c.limitMs,
                                                                                    c.limitMs, c.limitMs)) + "; ";
    }
    if (c.requestResponse) {
        DWORD value = limit;
        outcome.settings += "request RECEIVE_RESPONSE_TIMEOUT " +
                            Describe(WinHttpSetOption(request, WINHTTP_OPTION_RECEIVE_RESPONSE_TIMEOUT, &value,
                                                      sizeof(value))) + "; ";
    }
    if (c.requestReceive) {
        DWORD value = limit;
        outcome.settings += "request RECEIVE_TIMEOUT " +
                            Describe(WinHttpSetOption(request, WINHTTP_OPTION_RECEIVE_TIMEOUT, &value, sizeof(value))) +
                            "; ";
    }
    if (outcome.settings.empty()) {
        outcome.settings = "none; ";
    }
    outcome.readback = "response " + Query(request, WINHTTP_OPTION_RECEIVE_RESPONSE_TIMEOUT) + ", receive " +
                       Query(request, WINHTTP_OPTION_RECEIVE_TIMEOUT) + ", send " +
                       Query(request, WINHTTP_OPTION_SEND_TIMEOUT) + ", connect " +
                       Query(request, WINHTTP_OPTION_CONNECT_TIMEOUT);

    const std::string body(c.post ? 65536 : 0, 'x');
    const auto start = Clock::now();
    BOOL ok = WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0,
                                 static_cast<DWORD>(body.size()), 0);
    std::size_t written = 0;
    while (ok && written < body.size()) {
        DWORD sent = 0;
        ok = WinHttpWriteData(request, body.data() + written, static_cast<DWORD>(body.size() - written), &sent);
        written += sent;
        if (ok && sent == 0) {
            break;
        }
    }
    ok = ok && WinHttpReceiveResponse(request, nullptr);
    const DWORD error = ok ? ERROR_SUCCESS : GetLastError();
    outcome.elapsedMs = std::chrono::duration<double, std::milli>(Clock::now() - start).count();
    if (ok) {
        DWORD status = 0;
        DWORD size = sizeof(status);
        WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX,
                            &status, &size, WINHTTP_NO_HEADER_INDEX);
        outcome.result = "answered HTTP " + std::to_string(status);
    } else {
        outcome.result = "error " + std::to_string(error) + ": " + ErrorMessage(error, module);
    }
    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connection);
    WinHttpCloseHandle(session);
    return outcome;
}

// The same request through the project's transport (src/platform/http_win32.cpp).
Outcome RunTransport(bool post, int port) {
    crisp::platform::HttpRequest request;
    request.host = "127.0.0.1";
    request.port = port;
    request.path = "/slow/3000";
    request.connectTimeoutMs = 1000;
    request.timeoutMs = 1000;
    Outcome outcome;
    outcome.settings = "connectTimeoutMs 1000, timeoutMs 1000; ";
    const auto start = Clock::now();
    crisp::platform::HttpResponse response;
    if (post) {
        class Body : public crisp::platform::HttpBody {
        public:
            std::uint64_t Size() const override { return 65536; }
            bool Read(char* buffer, std::size_t capacity, std::size_t& count, std::string&) override {
                count = std::min<std::size_t>(capacity, 65536 - position_);
                std::memset(buffer, 'x', count);
                position_ += count;
                return true;
            }

        private:
            std::size_t position_ = 0;
        } body;
        response = crisp::platform::HttpPost(request, body);
    } else {
        response = crisp::platform::HttpGet(request);
    }
    outcome.elapsedMs = std::chrono::duration<double, std::milli>(Clock::now() - start).count();
    outcome.result = response.Ok() ? "answered HTTP " + std::to_string(response.status)
                                   : "failure " + std::to_string(static_cast<int>(response.failure)) + ": " +
                                         response.error;
    return outcome;
}

std::string WinHttpVersion() {
    wchar_t path[MAX_PATH] = {};
    const HMODULE module = GetModuleHandleW(L"winhttp.dll");
    if (module == nullptr || GetModuleFileNameW(module, path, MAX_PATH) == 0) {
        return "unknown";
    }
    DWORD ignored = 0;
    const DWORD size = GetFileVersionInfoSizeW(path, &ignored);
    std::vector<char> data(size);
    VS_FIXEDFILEINFO* info = nullptr;
    UINT length = 0;
    if (size == 0 || !GetFileVersionInfoW(path, 0, size, data.data()) ||
        !VerQueryValueW(data.data(), L"\\", reinterpret_cast<void**>(&info), &length) || info == nullptr) {
        return crisp::platform::win32::Narrow(path);
    }
    return crisp::platform::win32::Narrow(path) + " " + std::to_string(HIWORD(info->dwFileVersionMS)) + "." +
           std::to_string(LOWORD(info->dwFileVersionMS)) + "." + std::to_string(HIWORD(info->dwFileVersionLS)) + "." +
           std::to_string(LOWORD(info->dwFileVersionLS));
}

void Print(const std::string& id, const char* name, const Outcome& outcome) {
    std::printf("[%s] %s\n    set: %s\n    request handle before sending: %s\n    after %.0f ms: %s\n", id.c_str(),
                name, outcome.settings.c_str(), outcome.readback.empty() ? "-" : outcome.readback.c_str(),
                outcome.elapsedMs, outcome.result.c_str());
    std::fflush(stdout);
}

}  // namespace

int main(int argc, char** argv) {
    const bool longCases = argc > 1 && std::strcmp(argv[1], "--long") == 0;
    const int port = StartServer();
    if (port == 0) {
        std::printf("winhttp_timeouts: the local server could not start\n");
        return 2;
    }
    WinHttpCloseHandle(WinHttpOpen(L"winhttp_timeouts", WINHTTP_ACCESS_TYPE_NO_PROXY, WINHTTP_NO_PROXY_NAME,
                                   WINHTTP_NO_PROXY_BYPASS, 0));  // loads winhttp.dll for the version
    std::printf("winhttp_timeouts: %s; local server on 127.0.0.1:%d\n", WinHttpVersion().c_str(), port);
    std::printf("Short cases: the answer comes after 3000 ms; every limit is 1000 ms.\n\n");

    std::vector<Case> cases;
    cases.push_back({"S1 GET: session SetTimeouts and request RECEIVE_RESPONSE_TIMEOUT (what the transport does)"});
    cases.back().sessionTimeouts = cases.back().requestResponse = true;
    cases.push_back({"S2 POST: the same with a body through WinHttpWriteData"});
    cases.back().sessionTimeouts = cases.back().requestResponse = cases.back().post = true;
    cases.push_back({"S3 GET: session SetTimeouts only"});
    cases.back().sessionTimeouts = true;
    cases.push_back({"S4 GET: request SetTimeouts only"});
    cases.back().requestTimeouts = true;
    cases.push_back({"S5 GET: session RECEIVE_RESPONSE_TIMEOUT only"});
    cases.back().sessionResponse = true;
    cases.push_back({"S6 GET: request RECEIVE_RESPONSE_TIMEOUT only"});
    cases.back().requestResponse = true;
    cases.push_back({"S7 GET: request RECEIVE_TIMEOUT only"});
    cases.back().requestReceive = true;
    cases.push_back({"S8 POST: session SetTimeouts and session RECEIVE_RESPONSE_TIMEOUT"});
    cases.back().sessionTimeouts = cases.back().sessionResponse = cases.back().post = true;
    for (std::size_t i = 0; i < cases.size(); ++i) {
        Print(std::to_string(i + 1), cases[i].name, Run(cases[i], port));
    }
    Print("T1", "GET through the project's HttpGet", RunTransport(false, port));
    Print("T2", "POST through the project's HttpPost", RunTransport(true, port));

    if (longCases) {
        std::printf("\nLong cases, run side by side: the answer comes after 95000 ms; set limits are 120000 ms.\n\n");
        std::vector<Case> slow;
        slow.push_back({"L1 GET: no settings (WinHTTP's defaults)"});
        slow.push_back({"L2 POST: no settings (WinHTTP's defaults)"});
        slow.back().post = true;
        slow.push_back({"L3 POST: session SetTimeouts only"});
        slow.back().sessionTimeouts = slow.back().post = true;
        slow.push_back({"L4 POST: session SetTimeouts and request RECEIVE_RESPONSE_TIMEOUT"});
        slow.back().sessionTimeouts = slow.back().requestResponse = slow.back().post = true;
        slow.push_back({"L5 POST: session SetTimeouts and session RECEIVE_RESPONSE_TIMEOUT"});
        slow.back().sessionTimeouts = slow.back().sessionResponse = slow.back().post = true;
        for (Case& c : slow) {
            c.limitMs = 120000;
            c.delayMs = 95000;
        }
        std::vector<Outcome> outcomes(slow.size());
        std::vector<std::thread> threads;
        for (std::size_t i = 0; i < slow.size(); ++i) {
            threads.emplace_back([&, i] { outcomes[i] = Run(slow[i], port); });
        }
        for (std::thread& thread : threads) {
            thread.join();
        }
        for (std::size_t i = 0; i < slow.size(); ++i) {
            Print("L" + std::to_string(i + 1), slow[i].name, outcomes[i]);
        }
    }
    std::printf("\nDone. Send this whole output back.\n");
    return 0;
}
