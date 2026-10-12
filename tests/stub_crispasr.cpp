// A stand-in for crispasr --server in the ServerManager tests: it takes the server's command
// line, waits as if it loaded a model, and then answers /health and /v1/models on --host and
// --port like CrispASR 0.8.42 (docs/API-CRISPASR.md). It runs on Windows and Linux.
//
// Environment:
//   STUB_LOAD_MS      milliseconds before listening (default 200)
//   STUB_EXIT_CODE    exit with this code after the delay instead of listening
//   STUB_MODE         "foreign": answer 404 to every request, like another program on the port
//   STUB_IGNORE_TERM  "1": ignore SIGTERM (POSIX), so that a stop needs SIGKILL
//   STUB_LINES        print this many numbered lines at start
// A port it cannot bind ends it with exit code 0 after the "listening" line, as CrispASR does.

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
using SocketHandle = SOCKET;
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/socket.h>
#include <unistd.h>
using SocketHandle = int;
#endif

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <system_error>
#include <thread>
#include <vector>

namespace {

#if defined(_WIN32)
constexpr SocketHandle kNoSocket = INVALID_SOCKET;
void CloseSocket(SocketHandle socket) {
    closesocket(socket);
}
#else
constexpr SocketHandle kNoSocket = -1;
void CloseSocket(SocketHandle socket) {
    close(socket);
}
#endif

#if defined(_WIN32)
std::string Narrow(const wchar_t* text) {
    const int size = WideCharToMultiByte(CP_UTF8, 0, text, -1, nullptr, 0, nullptr, nullptr);
    std::string result(size > 0 ? static_cast<std::size_t>(size - 1) : 0, '\0');
    if (size > 1) {
        WideCharToMultiByte(CP_UTF8, 0, text, -1, result.data(), size, nullptr, nullptr);
    }
    return result;
}

std::wstring Widen(const char* text) {
    const int size = MultiByteToWideChar(CP_UTF8, 0, text, -1, nullptr, 0);
    std::wstring result(size > 0 ? static_cast<std::size_t>(size - 1) : 0, L'\0');
    if (size > 1) {
        MultiByteToWideChar(CP_UTF8, 0, text, -1, result.data(), size);
    }
    return result;
}
#endif

// Environment variables as UTF-8, as the wide-character API reads them on Windows; |set| tells
// whether the variable exists.
std::string Env(const char* name, bool* set = nullptr) {
#if defined(_WIN32)
    const wchar_t* value = _wgetenv(Widen(name).c_str());
    if (set != nullptr) {
        *set = value != nullptr;
    }
    return value != nullptr ? Narrow(value) : "";
#else
    const char* value = std::getenv(name);
    if (set != nullptr) {
        *set = value != nullptr;
    }
    return value != nullptr ? value : "";
#endif
}

// A JSON string body, escaped as crispasr_json_escape does for paths and names.
std::string JsonEscape(const std::string& text) {
    std::string escaped;
    for (const char c : text) {
        if (c == '"' || c == '\\') {
            escaped.push_back('\\');
        }
        escaped.push_back(c);
    }
    return escaped;
}

void Say(const std::string& text) {
    std::fprintf(stderr, "%s\n", text.c_str());
    std::fflush(stderr);
}

void SendAll(SocketHandle socket, const std::string& data) {
    std::size_t sent = 0;
    while (sent < data.size()) {
#if defined(_WIN32)
        const int count = send(socket, data.data() + sent, static_cast<int>(data.size() - sent), 0);
#else
        const ssize_t count = send(socket, data.data() + sent, data.size() - sent, MSG_NOSIGNAL);
#endif
        if (count <= 0) {
            return;
        }
        sent += static_cast<std::size_t>(count);
    }
}

std::string Response(int status, const char* reason, const std::string& body) {
    return "HTTP/1.1 " + std::to_string(status) + " " + reason +
           "\r\nContent-Type: application/json\r\nContent-Length: " + std::to_string(body.size()) +
           "\r\nConnection: close\r\n\r\n" + body;
}

// True when |keys| (comma-separated) contains |key|; no keys means every request is authorized.
bool Authorized(const std::string& keys, const std::string& header) {
    if (keys.empty()) {
        return true;
    }
    const std::string prefix = "Bearer ";
    if (header.compare(0, prefix.size(), prefix) != 0) {
        return false;
    }
    const std::string key = header.substr(prefix.size());
    std::size_t start = 0;
    while (start <= keys.size()) {
        std::size_t end = keys.find(',', start);
        if (end == std::string::npos) {
            end = keys.size();
        }
        if (keys.substr(start, end - start) == key) {
            return true;
        }
        start = end + 1;
    }
    return false;
}

void Serve(SocketHandle client, const std::string& backend, const std::string& model, bool foreign) {
    std::string request;
    char buffer[2048];
    while (request.find("\r\n\r\n") == std::string::npos && request.size() < 65536) {
        const int count = static_cast<int>(recv(client, buffer, sizeof(buffer), 0));
        if (count <= 0) {
            return;
        }
        request.append(buffer, static_cast<std::size_t>(count));
    }
    const std::size_t lineEnd = request.find("\r\n");
    const std::string line = request.substr(0, lineEnd);
    std::string authorization;
    const std::string marker = "\r\nAuthorization: ";
    const std::size_t found = request.find(marker);
    if (found != std::string::npos) {
        const std::size_t valueStart = found + marker.size();
        authorization = request.substr(valueStart, request.find("\r\n", valueStart) - valueStart);
    }
    if (foreign) {
        SendAll(client, Response(404, "Not Found", "{\"message\": \"not CrispASR\"}"));
    } else if (line.rfind("GET /health ", 0) == 0) {
        SendAll(client, Response(200, "OK", "{\"status\": \"ok\", \"backend\": \"" + JsonEscape(backend) + "\"}"));
    } else if (line.rfind("GET /v1/models ", 0) == 0) {
        if (!Authorized(Env("CRISPASR_API_KEYS"), authorization)) {
            SendAll(client, Response(401, "Unauthorized",
                                     "{\"error\": {\"message\": \"invalid or missing API key\", "
                                     "\"type\": \"invalid_api_key\"}}"));
        } else {
            SendAll(client, Response(200, "OK",
                                     "{\"object\": \"list\", \"data\": [{\"id\": \"" + JsonEscape(model) +
                                         "\", \"object\": \"model\", \"owned_by\": \"crispasr\", \"backend\": \"" +
                                         JsonEscape(backend) + "\"}]}"));
        }
    } else {
        SendAll(client, Response(404, "Not Found", "{\"error\": \"not found. Use POST /v1/audio/transcriptions\"}"));
    }
}

}  // namespace

// The arguments as UTF-8. On Windows they come from the C runtime's wide-character parse of the
// command line, as an MSVC-built crispasr.exe sees them (wmain; the build links with -municode).
int Run(const std::vector<std::string>& argv) {
#if defined(_WIN32)
    WSADATA data;
    WSAStartup(MAKEWORD(2, 2), &data);
#else
    signal(SIGPIPE, SIG_IGN);
    if (Env("STUB_IGNORE_TERM") == "1") {
        signal(SIGTERM, SIG_IGN);
    }
#endif
    std::string host = "127.0.0.1";
    int port = 5114;
    std::string backend = "stub";
    std::string model;
    const int argc = static_cast<int>(argv.size());
    for (int i = 1; i < argc; ++i) {
        const std::string& argument = argv[i];
        Say("stub: argv[" + std::to_string(i) + "]=" + argument);
        if (i + 1 < argc) {
            if (argument == "--host") {
                host = argv[i + 1];
            } else if (argument == "--port") {
                port = std::atoi(argv[i + 1].c_str());
            } else if (argument == "--backend") {
                backend = argv[i + 1];
            } else if (argument == "-m") {
                model = argv[i + 1];
            }
        }
    }
    std::error_code error;
    Say("stub: cwd=" + std::filesystem::current_path(error).u8string());
    for (const char* name : {"CRISPASR_MODELS_DIR", "CRISPASR_CACHE_DIR", "CRISPASR_SCRATCH_DIR", "CRISPASR_API_KEYS",
                             "CUDA_VISIBLE_DEVICES", "TMP", "TEMP", "TMPDIR", "STUB_PROFILE_VALUE"}) {
        bool set = false;
        const std::string value = Env(name, &set);
        Say(std::string("stub: env ") + name + (set ? "=" + value : " unset"));
    }
    const int lines = std::atoi(Env("STUB_LINES").c_str());
    for (int i = 1; i <= lines; ++i) {
        Say("stub: line " + std::to_string(i));
    }

    const std::string load = Env("STUB_LOAD_MS");
    std::this_thread::sleep_for(std::chrono::milliseconds(load.empty() ? 200 : std::atoi(load.c_str())));
    const std::string exitCode = Env("STUB_EXIT_CODE");
    if (!exitCode.empty()) {
        Say("stub: exiting with code " + exitCode);
        return std::atoi(exitCode.c_str());
    }

    Say("crispasr-server: listening on " + host + ":" + std::to_string(port));
    const SocketHandle listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    int option = 1;
#if defined(_WIN32)
    setsockopt(listener, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, reinterpret_cast<const char*>(&option), sizeof(option));
#else
    setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option));
#endif
    sockaddr_in address = {};
    address.sin_family = AF_INET;
    address.sin_port = htons(static_cast<unsigned short>(port));
    if (inet_pton(AF_INET, host.c_str(), &address.sin_addr) != 1) {
        inet_pton(AF_INET, "127.0.0.1", &address.sin_addr);
    }
    if (listener == kNoSocket || bind(listener, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) != 0 ||
        listen(listener, 16) != 0) {
        Say("stub: cannot listen on " + host + ":" + std::to_string(port));
        return 0;  // CrispASR ignores the result of listen() and exits with code 0
    }
    const bool foreign = Env("STUB_MODE") == "foreign";
    for (;;) {
        const SocketHandle client = accept(listener, nullptr, nullptr);
        if (client == kNoSocket) {
            continue;
        }
        Serve(client, backend, model, foreign);
        CloseSocket(client);
    }
}

#if defined(_WIN32)
int wmain(int argc, wchar_t** argv) {
    std::vector<std::string> arguments;
    for (int i = 0; i < argc; ++i) {
        arguments.push_back(Narrow(argv[i]));
    }
    return Run(arguments);
}
#else
int main(int argc, char** argv) {
    return Run(std::vector<std::string>(argv, argv + argc));
}
#endif
