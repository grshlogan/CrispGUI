#include "platform/multipart.h"

#include "platform/files.h"

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <random>
#include <system_error>
#include <thread>

namespace crisp::platform {
namespace {

// A field name as CrispASR's fields are written: letters, digits, '_', '-', and '.'.
bool IsPlainName(const std::string& name) {
    if (name.empty()) {
        return false;
    }
    for (const char c : name) {
        const bool plain = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' ||
                           c == '-' || c == '.';
        if (!plain) {
            return false;
        }
    }
    return true;
}

// Printable ASCII only, so that a header line cannot be split or ended early.
bool IsHeaderText(const std::string& text) {
    return std::all_of(text.begin(), text.end(), [](char c) { return c >= 0x20 && c < 0x7F; });
}

// The length of the well-formed UTF-8 sequence at text[i] (RFC 3629: no overlong forms, no
// surrogates, nothing above U+10FFFF), or 0.
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

std::string FileError(const char* what) {
    const int code = errno;
    return std::string(what) + (code != 0 ? ": " + std::generic_category().message(code) : std::string()) + ".";
}

}  // namespace

std::string MultipartFileName(const std::string& name) {
    std::string result;
    for (std::size_t i = 0; i < name.size();) {
        const unsigned char c = static_cast<unsigned char>(name[i]);
        if (c < 0x80) {
            result.push_back(c < 0x20 || c == 0x7F || c == '"' || c == '\\' ? '_' : static_cast<char>(c));
            ++i;
            continue;
        }
        const std::size_t length = Utf8Length(name, i);
        if (length == 0) {
            result.push_back('_');
            ++i;
        } else {
            result.append(name, i, length);
            i += length;
        }
    }
    return result.empty() ? "audio" : result;
}

std::string RandomMultipartBoundary() {
    // The boundary only has to differ from the bytes of the upload, so a generator seeded from
    // clocks, the thread, and a counter is enough; std::random_device could throw.
    static std::atomic<std::uint64_t> counter{0};
    const auto steady = static_cast<std::uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count());
    const auto system = static_cast<std::uint64_t>(std::chrono::system_clock::now().time_since_epoch().count());
    const auto thread = static_cast<std::uint64_t>(std::hash<std::thread::id>()(std::this_thread::get_id()));
    const std::uint64_t count = counter.fetch_add(1);
    std::seed_seq seed{static_cast<std::uint32_t>(steady), static_cast<std::uint32_t>(steady >> 32),
                       static_cast<std::uint32_t>(system), static_cast<std::uint32_t>(system >> 32),
                       static_cast<std::uint32_t>(thread), static_cast<std::uint32_t>(count)};
    std::mt19937_64 generator(seed);
    static const char kHex[] = "0123456789abcdef";
    std::string boundary = "CrispGUI-";
    for (int part = 0; part < 2; ++part) {
        std::uint64_t bits = generator();
        for (int digit = 0; digit < 16; ++digit) {
            boundary.push_back(kHex[bits & 0xF]);
            bits >>= 4;
        }
    }
    return boundary;
}

MultipartBody::MultipartBody(const std::vector<std::pair<std::string, std::string>>& fields, const MultipartFile& file,
                             std::string boundary) {
    const bool given = !boundary.empty();
    boundary_ = given ? std::move(boundary) : RandomMultipartBoundary();
    for (const auto& field : fields) {
        if (!IsPlainName(field.first)) {
            error_ = "The field name \"" + field.first + "\" is not a plain name.";
            return;
        }
    }
    if (!IsPlainName(file.fieldName)) {
        error_ = "The field name \"" + file.fieldName + "\" is not a plain name.";
        return;
    }
    if (!IsHeaderText(file.contentType) || !IsHeaderText(boundary_)) {
        error_ = "The content type or the boundary contains characters a header cannot carry.";
        return;
    }
    // No field value may contain the delimiter; a random boundary is drawn again.
    for (int attempt = 0;; ++attempt) {
        const std::string delimiter = "--" + boundary_;
        const bool clash = std::any_of(fields.begin(), fields.end(), [&](const auto& field) {
            return field.second.find(delimiter) != std::string::npos;
        });
        if (!clash) {
            break;
        }
        if (given || attempt == 3) {
            error_ = "A field value contains the multipart boundary.";
            return;
        }
        boundary_ = RandomMultipartBoundary();
    }

    errno = 0;
    file_ = OpenFileForReading(file.path);
    if (file_ == nullptr) {
        error_ = FileError("The file cannot be opened");
        fileFailed_ = true;
        return;
    }
    if (!OpenFileSize(file_, fileSize_)) {
        error_ = "The path is not a regular file, or its size cannot be read.";
        fileFailed_ = true;
        return;
    }

    for (const auto& field : fields) {
        head_ += "--" + boundary_ + "\r\n";
        head_ += "Content-Disposition: form-data; name=\"" + field.first + "\"\r\n\r\n";
        head_ += field.second + "\r\n";
    }
    const std::string fileName = file.fileName.empty() ? file.path.filename().u8string() : file.fileName;
    head_ += "--" + boundary_ + "\r\n";
    head_ += "Content-Disposition: form-data; name=\"" + file.fieldName + "\"; filename=\"" +
             MultipartFileName(fileName) + "\"\r\n";
    head_ += "Content-Type: " + file.contentType + "\r\n\r\n";
    tail_ = "\r\n--" + boundary_ + "--\r\n";
}

MultipartBody::~MultipartBody() {
    if (file_ != nullptr) {
        std::fclose(file_);
    }
}

std::string MultipartBody::ContentType() const {
    return "multipart/form-data; boundary=" + boundary_;
}

std::uint64_t MultipartBody::Size() const {
    return error_.empty() ? head_.size() + fileSize_ + tail_.size() : 0;
}

bool MultipartBody::Read(char* buffer, std::size_t capacity, std::size_t& count, std::string& error) {
    count = 0;
    if (!error_.empty()) {
        error = error_;
        return false;
    }
    const std::uint64_t headEnd = head_.size();
    const std::uint64_t fileEnd = headEnd + fileSize_;
    const std::uint64_t end = fileEnd + tail_.size();
    while (count < capacity && position_ < end) {
        const std::uint64_t room = capacity - count;
        if (position_ < headEnd) {
            const auto bytes = static_cast<std::size_t>(std::min(room, headEnd - position_));
            std::memcpy(buffer + count, head_.data() + position_, bytes);
            count += bytes;
            position_ += bytes;
        } else if (position_ < fileEnd) {
            const auto wanted = static_cast<std::size_t>(std::min(room, fileEnd - position_));
            errno = 0;
            const std::size_t got = std::fread(buffer + count, 1, wanted, file_);
            count += got;
            position_ += got;
            if (got < wanted) {
                error_ = std::ferror(file_) != 0 ? FileError("The file could not be read")
                                                 : "The file became shorter while it was being uploaded.";
                error = error_;
                return false;
            }
        } else {
            if (position_ == fileEnd && std::fgetc(file_) != EOF) {
                error_ = "The file grew while it was being uploaded.";
                error = error_;
                return false;
            }
            const auto offset = static_cast<std::size_t>(position_ - fileEnd);
            const auto bytes = static_cast<std::size_t>(std::min<std::uint64_t>(room, tail_.size() - offset));
            std::memcpy(buffer + count, tail_.data() + offset, bytes);
            count += bytes;
            position_ += bytes;
        }
    }
    return true;
}

}  // namespace crisp::platform
