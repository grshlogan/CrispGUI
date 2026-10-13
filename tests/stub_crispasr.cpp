// A stand-in for crispasr --server in the integration tests: it takes the server's command line,
// waits as if it loaded a model, and then answers on --host and --port like CrispASR 0.8.42
// (docs/API-CRISPASR.md), one thread per connection. It runs on Windows and Linux and shares no
// code with the GUI, so the GUI's request encoding meets an independent reader.
//
// Routes:
//   GET  /health                   {"status": "ok", "backend": ...}
//   GET  /v1/models                the loaded model; needs a key of CRISPASR_API_KEYS
//   GET  /progress                 {"busy": ..., "progress": ...}; needs a key
//   POST /v1/audio/transcriptions  a multipart upload; needs a key; see below
//   POST /stub/echo                {"bytes": N, "fnv1a": ..., "content_type": ...} of the body
//   GET  /stub/requests            {"transcriptions": N}: the transcription requests received so far
// Anything else gets 404 with CrispASR's "not found" body.
//
// A transcription "decodes" a file whose first bytes look like audio (RIFF, ID3, fLaC, OggS, an
// MPEG frame, or an MP4 "ftyp" box) and answers anything else with CrispASR's 400. It takes the
// model lock, so transcriptions run one at a time, and /progress reports busy while one runs. The
// answer is a synthetic transcript in the server's format plus a "stub" member that describes the
// upload, or the file named by STUB_TRANSCRIPT (verbose_json) or STUB_TRANSCRIPT_SRT (srt) as is.
// Unlike CrispASR, a malformed multipart body gets a 400 that says what is wrong.
//
// Request fields that steer it:
//   stub_reply     400 | 500 | 503 | garbage | close: answer that way instead of transcribing
//   stub_delay_ms  how long the transcription takes (default STUB_TRANSCRIBE_MS, else 0)
//
// Environment:
//   STUB_LOAD_MS         milliseconds before listening (default 200)
//   STUB_EXIT_CODE       exit with this code after the delay instead of listening
//   STUB_MODE            "foreign": answer 404 to every request, like another program on the port
//   STUB_IGNORE_TERM     "1": ignore SIGTERM (POSIX), so that a stop needs SIGKILL
//   STUB_LINES           print this many numbered lines at start
//   STUB_MAX_BODY_BYTES  bodies over this size get 413 after being read (default 512 MiB)
//   STUB_TRANSCRIBE_MS   the default duration of a transcription
//   STUB_FAIL_FIRST      "<status>:<count>", such as "500:1": the first <count> transcriptions that
//                        would run work for their delay and then answer with <status> (500 or 503)
//                        instead, as an error inside the model does; the next ones work
//   STUB_TRANSCRIPT      a verbose_json file to answer with
//   STUB_TRANSCRIPT_SRT  an srt file to answer with
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

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <mutex>
#include <string>
#include <system_error>
#include <thread>
#include <utility>
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

// The whole file at the UTF-8 |path|; false when it cannot be read.
bool ReadWholeFile(const std::string& path, std::string& bytes) {
#if defined(_WIN32)
    std::FILE* file = _wfopen(Widen(path.c_str()).c_str(), L"rb");
#else
    std::FILE* file = std::fopen(path.c_str(), "rb");
#endif
    if (file == nullptr) {
        return false;
    }
    char buffer[16384];
    std::size_t count = 0;
    while ((count = std::fread(buffer, 1, sizeof(buffer), file)) > 0) {
        bytes.append(buffer, count);
    }
    const bool ok = std::ferror(file) == 0;
    std::fclose(file);
    return ok;
}

// The length of the well-formed UTF-8 sequence at text[i], or 0.
std::size_t Utf8Length(const std::string& text, std::size_t i) {
    const unsigned char first = static_cast<unsigned char>(text[i]);
    std::size_t length = 0;
    unsigned char low = 0x80;
    unsigned char high = 0xBF;
    if (first >= 0xC2 && first <= 0xDF) {
        length = 2;
    } else if (first >= 0xE0 && first <= 0xEF) {
        length = 3;
        low = first == 0xE0 ? 0xA0 : 0x80;
        high = first == 0xED ? 0x9F : 0xBF;
    } else if (first >= 0xF0 && first <= 0xF4) {
        length = 4;
        low = first == 0xF0 ? 0x90 : 0x80;
        high = first == 0xF4 ? 0x8F : 0xBF;
    } else {
        return 0;
    }
    if (i + length > text.size()) {
        return 0;
    }
    for (std::size_t k = 1; k < length; ++k) {
        const unsigned char next = static_cast<unsigned char>(text[i + k]);
        if (k == 1 ? (next < low || next > high) : (next < 0x80 || next > 0xBF)) {
            return 0;
        }
    }
    return length;
}

// A JSON string body, escaped as crispasr_json_escape escapes it (crispasr_output.cpp:688).
std::string JsonEscape(const std::string& text) {
    static const char kHex[] = "0123456789abcdef";
    std::string escaped;
    for (std::size_t i = 0; i < text.size();) {
        const unsigned char c = static_cast<unsigned char>(text[i]);
        if (c >= 0x80) {
            const std::size_t length = Utf8Length(text, i);
            escaped += length == 0 ? std::string("\xEF\xBF\xBD") : text.substr(i, length);
            i += length == 0 ? 1 : length;
            continue;
        }
        ++i;
        switch (c) {
        case '"':
            escaped += "\\\"";
            break;
        case '\\':
            escaped += "\\\\";
            break;
        case '\b':
            escaped += "\\b";
            break;
        case '\f':
            escaped += "\\f";
            break;
        case '\n':
            escaped += "\\n";
            break;
        case '\r':
            escaped += "\\r";
            break;
        case '\t':
            escaped += "\\t";
            break;
        default:
            if (c < 0x20) {
                escaped += "\\u00";
                escaped.push_back(kHex[c >> 4]);
                escaped.push_back(kHex[c & 0xF]);
            } else {
                escaped.push_back(static_cast<char>(c));
            }
            break;
        }
    }
    return escaped;
}

std::string Lower(std::string text) {
    for (char& c : text) {
        if (c >= 'A' && c <= 'Z') {
            c = static_cast<char>(c - 'A' + 'a');
        }
    }
    return text;
}

// FNV-1a, 64 bits, as 16 hexadecimal digits: a checksum the tests compute too.
std::string Fnv1a(const std::string& bytes) {
    std::uint64_t hash = 0xcbf29ce484222325ull;
    for (const char c : bytes) {
        hash ^= static_cast<unsigned char>(c);
        hash *= 0x100000001b3ull;
    }
    char text[17];
    std::snprintf(text, sizeof(text), "%016llx", static_cast<unsigned long long>(hash));
    return text;
}

std::mutex outputMutex;

void Say(const std::string& text) {
    std::lock_guard<std::mutex> lock(outputMutex);
    std::fprintf(stderr, "%s\n", text.c_str());
    std::fflush(stderr);
}

// What every connection thread reads; set before the first connection and never changed.
struct Settings {
    std::string backend = "stub";
    std::string model;
    std::string keys;
    bool foreign = false;
    std::uint64_t maxBodyBytes = 512ull * 1024 * 1024;
    int transcribeMs = 0;
    int failStatus = 0;  // the status of the first failCount transcriptions
    int failCount = 0;
    bool replayJson = false;
    std::string transcriptJson;
    bool replaySrt = false;
    std::string transcriptSrt;
};
Settings settings;

// The model lock and the progress that /progress reports, as in crispasr_server.cpp:518-540.
std::mutex modelMutex;
std::atomic<int> busy{0};
std::atomic<int> progress{-1};
std::atomic<int> transcriptionRequests{0};  // answered or not
std::atomic<int> failedSoFar{0};

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

const char* Reason(int status) {
    switch (status) {
    case 200:
        return "OK";
    case 400:
        return "Bad Request";
    case 401:
        return "Unauthorized";
    case 404:
        return "Not Found";
    case 413:
        return "Payload Too Large";
    case 500:
        return "Internal Server Error";
    case 503:
        return "Service Unavailable";
    default:
        return "Status";
    }
}

std::string Response(int status, const std::string& body, const std::string& type = "application/json",
                     const std::string& extraHeaders = "") {
    return "HTTP/1.1 " + std::to_string(status) + " " + Reason(status) + "\r\nContent-Type: " + type +
           "\r\nContent-Length: " + std::to_string(body.size()) + "\r\n" + extraHeaders + "Connection: close\r\n\r\n" +
           body;
}

// CrispASR's json_error() body (crispasr_server.cpp:425-436).
std::string JsonError(int status, const std::string& message) {
    return Response(status,
                    "{\"error\": {\"message\": \"" + JsonEscape(message) + "\", \"type\": \"invalid_request_error\"}}");
}

// The body CrispASR's error handler gives an error without one (crispasr_server.cpp:3919-3925).
std::string NotFound(int status) {
    return Response(status, "{\"error\": \"not found. Use POST /v1/audio/transcriptions\"}");
}

std::string AuthError() {
    return Response(401, "{\"error\": {\"message\": \"invalid or missing API key\", \"type\": \"invalid_api_key\"}}",
                    "application/json", "WWW-Authenticate: Bearer\r\n");
}

struct Request {
    std::string method;
    std::string target;
    std::vector<std::pair<std::string, std::string>> headers;  // names in lower case
    std::string body;
    bool tooLarge = false;

    std::string Header(const std::string& name) const {
        for (const auto& header : headers) {
            if (header.first == name) {
                return header.second;
            }
        }
        return "";
    }
};

std::string Trim(const std::string& text) {
    const std::size_t first = text.find_first_not_of(" \t");
    if (first == std::string::npos) {
        return "";
    }
    return text.substr(first, text.find_last_not_of(" \t") - first + 1);
}

int Receive(SocketHandle client, char* buffer, std::size_t size) {
    return static_cast<int>(recv(client, buffer, static_cast<int>(size), 0));
}

// Reads one request. A body over the limit is read and dropped, and |tooLarge| set.
bool ReadRequest(SocketHandle client, Request& request) {
    std::string data;
    char buffer[65536];
    std::size_t headerEnd = std::string::npos;
    while ((headerEnd = data.find("\r\n\r\n")) == std::string::npos) {
        if (data.size() > 65536) {
            return false;
        }
        const int count = Receive(client, buffer, sizeof(buffer));
        if (count <= 0) {
            return false;
        }
        data.append(buffer, static_cast<std::size_t>(count));
    }
    const std::size_t lineEnd = data.find("\r\n");
    const std::string line = data.substr(0, lineEnd);
    const std::size_t space = line.find(' ');
    const std::size_t secondSpace = line.find(' ', space + 1);
    if (space == std::string::npos || secondSpace == std::string::npos) {
        return false;
    }
    request.method = line.substr(0, space);
    request.target = line.substr(space + 1, secondSpace - space - 1);
    std::size_t position = lineEnd + 2;
    while (position < headerEnd) {
        const std::size_t end = data.find("\r\n", position);
        const std::string header = data.substr(position, end - position);
        const std::size_t colon = header.find(':');
        if (colon != std::string::npos) {
            request.headers.emplace_back(Lower(header.substr(0, colon)), Trim(header.substr(colon + 1)));
        }
        position = end + 2;
    }
    std::string rest = data.substr(headerEnd + 4);
    const std::string lengthText = request.Header("content-length");
    const std::uint64_t length = lengthText.empty() ? 0 : std::strtoull(lengthText.c_str(), nullptr, 10);
    if (Lower(request.Header("expect")) == "100-continue") {
        SendAll(client, "HTTP/1.1 100 Continue\r\n\r\n");
    }
    request.tooLarge = length > settings.maxBodyBytes;
    std::uint64_t received = rest.size();
    if (!request.tooLarge) {
        request.body = std::move(rest);
    }
    while (received < length) {
        const std::size_t wanted = static_cast<std::size_t>(std::min<std::uint64_t>(sizeof(buffer), length - received));
        const int count = Receive(client, buffer, wanted);
        if (count <= 0) {
            return false;
        }
        received += static_cast<std::uint64_t>(count);
        if (!request.tooLarge) {
            request.body.append(buffer, static_cast<std::size_t>(count));
        }
    }
    if (!request.tooLarge && request.body.size() > length) {
        request.body.resize(static_cast<std::size_t>(length));
    }
    return true;
}

// True when a key of CRISPASR_API_KEYS (comma-separated) arrives as "Authorization: Bearer <key>"
// or "X-API-Key: <key>"; no keys means every request is authorized.
bool Authorized(const Request& request) {
    if (settings.keys.empty()) {
        return true;
    }
    std::string key = Trim(request.Header("x-api-key"));
    const std::string authorization = Trim(request.Header("authorization"));
    if (authorization.rfind("Bearer ", 0) == 0) {
        key = Trim(authorization.substr(7));
    }
    if (key.empty()) {
        return false;
    }
    std::size_t start = 0;
    while (start <= settings.keys.size()) {
        std::size_t end = settings.keys.find(',', start);
        if (end == std::string::npos) {
            end = settings.keys.size();
        }
        if (Trim(settings.keys.substr(start, end - start)) == key) {
            return true;
        }
        start = end + 1;
    }
    return false;
}

struct Part {
    std::string name;
    std::string fileName;
    bool hasFileName = false;
    std::string contentType;
    std::string content;
};

// The quoted parameter |name| of a Content-Disposition value, as cpp-httplib reads it: the text
// between the quotes, with no escapes.
bool DispositionParameter(const std::string& value, const std::string& name, std::string& out) {
    std::size_t position = 0;
    while ((position = value.find(name + "=\"", position)) != std::string::npos) {
        if (position == 0 || value[position - 1] == ' ' || value[position - 1] == ';') {
            const std::size_t start = position + name.size() + 2;
            const std::size_t end = value.find('"', start);
            if (end == std::string::npos) {
                return false;
            }
            out = value.substr(start, end - start);
            return true;
        }
        position += name.size();
    }
    return false;
}

// Splits a multipart/form-data body strictly: the layout of RFC 7578 that cpp-httplib accepts,
// and nothing looser.
bool ParseMultipart(const std::string& contentType, const std::string& body, std::vector<Part>& parts,
                    std::string& error) {
    const std::string type = Lower(contentType);
    const std::size_t parameter = type.find("boundary=");
    if (type.rfind("multipart/form-data", 0) != 0 || parameter == std::string::npos) {
        error = "the content type is not multipart/form-data with a boundary";
        return false;
    }
    std::string boundary = contentType.substr(parameter + 9);
    boundary = boundary.substr(0, boundary.find(';'));
    if (boundary.size() >= 2 && boundary.front() == '"' && boundary.back() == '"') {
        boundary = boundary.substr(1, boundary.size() - 2);
    }
    const std::string delimiter = "--" + boundary;
    if (boundary.empty() || body.compare(0, delimiter.size() + 2, delimiter + "\r\n") != 0) {
        error = "the body does not start with the boundary";
        return false;
    }
    std::size_t position = delimiter.size() + 2;
    for (;;) {
        const std::size_t headersEnd = body.find("\r\n\r\n", position);
        if (headersEnd == std::string::npos) {
            error = "a part has no end of its headers";
            return false;
        }
        Part part;
        std::size_t line = position;
        bool named = false;
        while (line < headersEnd + 2) {
            const std::size_t end = body.find("\r\n", line);
            const std::string header = body.substr(line, end - line);
            const std::size_t colon = header.find(':');
            if (colon == std::string::npos) {
                error = "a part header has no colon";
                return false;
            }
            const std::string name = Lower(header.substr(0, colon));
            const std::string value = Trim(header.substr(colon + 1));
            if (name == "content-disposition") {
                if (Lower(value).rfind("form-data;", 0) != 0 || !DispositionParameter(value, "name", part.name)) {
                    error = "a part's Content-Disposition is not form-data with a name";
                    return false;
                }
                part.hasFileName = DispositionParameter(value, "filename", part.fileName);
                named = true;
            } else if (name == "content-type") {
                part.contentType = value;
            }
            line = end + 2;
        }
        if (!named) {
            error = "a part has no Content-Disposition";
            return false;
        }
        const std::size_t contentStart = headersEnd + 4;
        const std::size_t contentEnd = body.find("\r\n" + delimiter, contentStart);
        if (contentEnd == std::string::npos) {
            error = "a part is not closed by the boundary";
            return false;
        }
        part.content = body.substr(contentStart, contentEnd - contentStart);
        parts.push_back(std::move(part));
        position = contentEnd + 2 + delimiter.size();
        if (body.compare(position, 2, "\r\n") == 0) {
            position += 2;
            continue;
        }
        if (body.compare(position, 2, "--") == 0 &&
            (position + 2 == body.size() || body.compare(position + 2, std::string::npos, "\r\n") == 0)) {
            return true;
        }
        error = "the boundary is followed by something other than CRLF or --";
        return false;
    }
}

const Part* FindPart(const std::vector<Part>& parts, const std::string& name) {
    for (const Part& part : parts) {
        if (part.name == name) {
            return &part;
        }
    }
    return nullptr;
}

bool LooksLikeAudio(const std::string& bytes) {
    const auto startsWith = [&](const char* magic, std::size_t offset = 0) {
        const std::size_t length = std::strlen(magic);
        return bytes.size() >= offset + length && bytes.compare(offset, length, magic) == 0;
    };
    const bool mpegFrame = bytes.size() >= 2 && static_cast<unsigned char>(bytes[0]) == 0xFF &&
                           (static_cast<unsigned char>(bytes[1]) & 0xE0) == 0xE0;
    return startsWith("RIFF") || startsWith("ID3") || startsWith("fLaC") || startsWith("OggS") || mpegFrame ||
           startsWith("ftyp", 4);
}

struct Segment {
    long long t0;  // centiseconds, as CrispASR keeps them
    long long t1;
    std::string text;
    std::vector<std::pair<std::string, std::pair<long long, long long>>> words;
};

std::vector<Segment> SyntheticSegments() {
    return {
        {0, 250, "こんにちは、世界。", {{"こんにちは", {0, 120}}, {"世界", {130, 250}}}},
        {250, 512, "Quote \" and backslash \\ in one line.", {}},
        {512, 600, "", {}},
    };
}

std::string Seconds(long long centiseconds) {
    char text[32];
    std::snprintf(text, sizeof(text), "%.2f", static_cast<double>(centiseconds) / 100.0);
    return text;
}

// crispasr_to_timestamp(cs, true) (crispasr_output.cpp:23-35).
std::string SrtTime(long long centiseconds) {
    long long ms = centiseconds * 10;
    const long long hours = ms / 3600000;
    ms -= hours * 3600000;
    const long long minutes = ms / 60000;
    ms -= minutes * 60000;
    const long long seconds = ms / 1000;
    ms -= seconds * 1000;
    char text[32];
    std::snprintf(text, sizeof(text), "%02d:%02d:%02d,%03d", static_cast<int>(hours), static_cast<int>(minutes),
                  static_cast<int>(seconds), static_cast<int>(ms));
    return text;
}

// crispasr_segments_to_text (crispasr_output.cpp:1113-1121).
std::string JoinedText(const std::vector<Segment>& segments) {
    std::string text;
    for (const Segment& segment : segments) {
        if (!text.empty()) {
            text += ' ';
        }
        text += segment.text;
    }
    return text;
}

// The upload as the stub received it, for the tests to compare with what they sent.
std::string UploadFacts(const std::vector<Part>& parts, const Part& file) {
    std::string facts = "{\"file_field\": \"" + JsonEscape(file.name) + "\", \"file_name\": \"" +
                        JsonEscape(file.fileName) + "\", \"file_content_type\": \"" + JsonEscape(file.contentType) +
                        "\", \"file_bytes\": " + std::to_string(file.content.size()) + ", \"file_fnv1a\": \"" +
                        Fnv1a(file.content) + "\", \"fields\": [";
    bool first = true;
    for (const Part& part : parts) {
        if (part.hasFileName) {
            continue;
        }
        facts += std::string(first ? "" : ", ") + "[\"" + JsonEscape(part.name) + "\", \"" + JsonEscape(part.content) +
                 "\"]";
        first = false;
    }
    return facts + "]}";
}

// The layout of crispasr_segments_to_openai_verbose_json (crispasr_output.cpp:1163-1253), with
// the upload facts as a last member.
std::string VerboseJson(const std::vector<Segment>& segments, const std::string& language, const std::string& facts) {
    std::string json = "{\n  \"task\": \"transcribe\",\n  \"language\": \"" + JsonEscape(language) +
                       "\",\n  \"duration\": 6.000,\n  \"text\": \"" + JsonEscape(JoinedText(segments)) +
                       "\",\n  \"segments\": [\n";
    for (std::size_t i = 0; i < segments.size(); ++i) {
        const Segment& segment = segments[i];
        json += "    {\n      \"id\": " + std::to_string(i) + ",\n      \"start\": " + Seconds(segment.t0) +
                ",\n      \"end\": " + Seconds(segment.t1) + ",\n      \"text\": \"" + JsonEscape(segment.text) +
                "\",\n      \"temperature\": 0.000000,\n      \"avg_logprob\": 0.000000,\n"
                "      \"no_speech_prob\": 0.0";
        if (!segment.words.empty()) {
            json += ",\n      \"words\": [\n";
            for (std::size_t j = 0; j < segment.words.size(); ++j) {
                const auto& word = segment.words[j];
                json += "        {\"word\": \"" + JsonEscape(word.first) +
                        "\", \"start\": " + Seconds(word.second.first) + ", \"end\": " + Seconds(word.second.second) +
                        "}" + (j + 1 < segment.words.size() ? "," : "") + "\n";
            }
            json += "      ]";
        }
        json += std::string("\n    }") + (i + 1 < segments.size() ? "," : "") + "\n";
    }
    return json + "  ],\n  \"stub\": " + facts + "\n}\n";
}

// crispasr_segments_to_srt without shaping (crispasr_output.cpp:1124-1134).
std::string Srt(const std::vector<Segment>& segments) {
    std::string srt;
    int index = 0;
    for (const Segment& segment : segments) {
        if (segment.text.empty()) {
            continue;
        }
        srt += std::to_string(++index) + "\n" + SrtTime(segment.t0) + " --> " + SrtTime(segment.t1) + "\n" +
               segment.text + "\n\n";
    }
    return srt;
}

std::string FieldValue(const std::vector<Part>& parts, const std::string& name, const std::string& fallback) {
    const Part* part = FindPart(parts, name);
    const std::string value = part != nullptr ? Trim(part->content) : std::string();
    return value.empty() ? fallback : value;
}

// "Transcribes" under the model lock for |delayMs|, moving /progress from 0 to 100.
void RunModel(int delayMs) {
    std::lock_guard<std::mutex> lock(modelMutex);
    ++busy;
    progress = 0;
    const auto start = std::chrono::steady_clock::now();
    for (;;) {
        const auto elapsed = static_cast<int>(
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count());
        if (elapsed >= delayMs) {
            break;
        }
        progress = elapsed * 100 / delayMs;
        std::this_thread::sleep_for(std::chrono::milliseconds(std::min(20, delayMs - elapsed)));
    }
    progress = 100;
    --busy;
    progress = -1;
}

// POST /v1/audio/transcriptions, in the order of crispasr_server.cpp:2027-2310. An empty result
// means: close the connection without an answer.
std::string Transcribe(const Request& request) {
    ++transcriptionRequests;
    // cpp-httplib splits the body while reading it, before the route's handler checks the key.
    std::vector<Part> parts;
    std::string error;
    if (!ParseMultipart(request.Header("content-type"), request.body, parts, error)) {
        Say("stub: malformed multipart body: " + error);
        return JsonError(400, "stub: malformed multipart body: " + error);
    }
    if (!Authorized(request)) {
        return AuthError();
    }
    const Part* file = FindPart(parts, "file");
    if (file == nullptr) {
        return JsonError(400, "missing required field 'file'");
    }
    Say("crispasr-server: /v1/audio/transcriptions received '" + file->fileName + "' (" +
        std::to_string(file->content.size()) + " bytes)");
    const std::string format = FieldValue(parts, "response_format", "json");
    if (format != "json" && format != "verbose_json" && format != "text" && format != "srt" && format != "vtt" &&
        format != "diarized_json") {
        return JsonError(400, "invalid response_format '" + format +
                                  "'; must be one of: json, verbose_json, text, srt, vtt, diarized_json");
    }
    const std::string reply = FieldValue(parts, "stub_reply", "");
    if (reply == "400") {
        return JsonError(400, "failed to decode audio (unsupported format or corrupt file)");
    } else if (reply == "500") {
        return JsonError(500, "internal error: stub failure");
    } else if (reply == "503") {
        return JsonError(503, "model is still loading");
    } else if (reply == "garbage") {
        return Response(200, "this is not JSON");
    } else if (reply == "close") {
        return "";
    } else if (!reply.empty()) {
        return JsonError(400, "stub: unknown stub_reply '" + reply + "'");
    }
    // The upload is decoded before the model lock is taken (docs/API-CRISPASR.md, section 7).
    if (!LooksLikeAudio(file->content)) {
        return JsonError(400, "failed to decode audio (unsupported format or corrupt file)");
    }
    const std::string delay = FieldValue(parts, "stub_delay_ms", "");
    const int delayMs = delay.empty() ? settings.transcribeMs : std::atoi(delay.c_str());
    const bool fails = settings.failCount > 0 && failedSoFar.fetch_add(1) < settings.failCount;
    RunModel(delayMs);
    if (fails) {
        return JsonError(settings.failStatus, settings.failStatus == 503 ? "model is still loading"
                                                                       : "internal error: stub failure");
    }

    const std::vector<Segment> segments = SyntheticSegments();
    if (format == "verbose_json") {
        return Response(200, settings.replayJson ? settings.transcriptJson
                                                 : VerboseJson(segments, FieldValue(parts, "language", "ja"),
                                                               UploadFacts(parts, *file)));
    }
    if (format == "srt") {
        return Response(200, settings.replaySrt ? settings.transcriptSrt : Srt(segments),
                        "application/x-subrip; charset=utf-8");
    }
    if (format == "json") {
        return Response(200, "{\"text\": \"" + JsonEscape(JoinedText(segments)) + "\"}");
    }
    if (format == "text") {
        return Response(200, JoinedText(segments), "text/plain; charset=utf-8");
    }
    return JsonError(400, "stub: response_format '" + format + "' is not implemented");
}

std::string Answer(const Request& request) {
    const std::string path = request.target.substr(0, request.target.find('?'));
    if (settings.foreign) {
        return Response(404, "{\"message\": \"not CrispASR\"}");
    }
    if (request.tooLarge) {
        return NotFound(413);
    }
    if (request.method == "GET" && path == "/health") {
        return Response(200, "{\"status\": \"ok\", \"backend\": \"" + JsonEscape(settings.backend) + "\"}");
    }
    if (request.method == "GET" && path == "/v1/models") {
        if (!Authorized(request)) {
            return AuthError();
        }
        return Response(200, "{\"object\": \"list\", \"data\": [{\"id\": \"" + JsonEscape(settings.model) +
                                 "\", \"object\": \"model\", \"owned_by\": \"crispasr\", \"backend\": \"" +
                                 JsonEscape(settings.backend) + "\"}]}");
    }
    if (request.method == "GET" && path == "/progress") {
        if (!Authorized(request)) {
            return AuthError();
        }
        return Response(200, std::string("{\"busy\": ") + (busy.load() > 0 ? "true" : "false") +
                                 ", \"progress\": " + std::to_string(progress.load()) + "}");
    }
    if (request.method == "POST" && path == "/v1/audio/transcriptions") {
        return Transcribe(request);
    }
    if (request.method == "GET" && path == "/stub/requests") {
        return Response(200, "{\"transcriptions\": " + std::to_string(transcriptionRequests.load()) + "}");
    }
    if (request.method == "POST" && path == "/stub/echo") {
        return Response(200, "{\"bytes\": " + std::to_string(request.body.size()) + ", \"fnv1a\": \"" +
                                 Fnv1a(request.body) + "\", \"content_type\": \"" +
                                 JsonEscape(request.Header("content-type")) + "\"}");
    }
    return NotFound(404);
}

void Serve(SocketHandle client) {
    Request request;
    if (ReadRequest(client, request)) {
        const std::string answer = Answer(request);
        if (!answer.empty()) {
            SendAll(client, answer);
        }
    }
    CloseSocket(client);
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
                settings.backend = argv[i + 1];
            } else if (argument == "-m") {
                settings.model = argv[i + 1];
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

    settings.keys = Env("CRISPASR_API_KEYS");
    settings.foreign = Env("STUB_MODE") == "foreign";
    const std::string maxBody = Env("STUB_MAX_BODY_BYTES");
    if (!maxBody.empty()) {
        settings.maxBodyBytes = std::strtoull(maxBody.c_str(), nullptr, 10);
    }
    settings.transcribeMs = std::atoi(Env("STUB_TRANSCRIBE_MS").c_str());
    {
        const std::string failFirst = Env("STUB_FAIL_FIRST");
        const std::size_t colon = failFirst.find(':');
        if (colon != std::string::npos) {
            settings.failStatus = std::atoi(failFirst.substr(0, colon).c_str());
            settings.failCount = std::atoi(failFirst.substr(colon + 1).c_str());
        }
    }
    const std::string json = Env("STUB_TRANSCRIPT");
    if (!json.empty() && !(settings.replayJson = ReadWholeFile(json, settings.transcriptJson))) {
        Say("stub: cannot read STUB_TRANSCRIPT " + json);
        return 2;
    }
    const std::string srt = Env("STUB_TRANSCRIPT_SRT");
    if (!srt.empty() && !(settings.replaySrt = ReadWholeFile(srt, settings.transcriptSrt))) {
        Say("stub: cannot read STUB_TRANSCRIPT_SRT " + srt);
        return 2;
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
    for (;;) {
        const SocketHandle client = accept(listener, nullptr, nullptr);
        if (client == kNoSocket) {
            continue;
        }
        std::thread(Serve, client).detach();
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
