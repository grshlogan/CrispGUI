#pragma once

#include "platform/http.h"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

// multipart/form-data bodies (RFC 7578) for the transcription upload: text fields, then one file
// that is read from disk while the body is sent. Both transports send the same bytes, laid out
// as cpp-httplib 0.57.1, the CrispASR server's HTTP library, parses them
// (docs/API-CRISPASR.md, section 3).
namespace crisp::platform {

struct MultipartFile {
    std::string fieldName = "file";
    std::filesystem::path path;
    // The file name the server sees. CrispASR takes the decoder's format hint from its extension,
    // so it keeps the real one (MultipartFileName).
    std::string fileName;
    std::string contentType = "application/octet-stream";
};

class MultipartBody : public HttpBody {
public:
    // Opens the file and lays out the body. |boundary| is for tests; empty picks a random one.
    MultipartBody(const std::vector<std::pair<std::string, std::string>>& fields, const MultipartFile& file,
                  std::string boundary = std::string());
    ~MultipartBody() override;
    MultipartBody(const MultipartBody&) = delete;
    MultipartBody& operator=(const MultipartBody&) = delete;

    // Why the body cannot be sent: a field name that is not a plain name, a field value that
    // contains the given boundary, or a file that cannot be opened. Empty when it can be sent.
    const std::string& Error() const { return error_; }
    // True when Error() concerns the file rather than the fields.
    bool FileFailed() const { return fileFailed_; }
    // "multipart/form-data; boundary=<boundary>", for the Content-Type header.
    std::string ContentType() const;
    const std::string& Boundary() const { return boundary_; }
    std::uint64_t FileSize() const { return fileSize_; }

    std::uint64_t Size() const override;
    // Fails when the file cannot be read or changed its size since it was opened.
    bool Read(char* buffer, std::size_t capacity, std::size_t& count, std::string& error) override;

private:
    std::string error_;
    bool fileFailed_ = false;
    std::string boundary_;
    std::string head_;  // the fields and the headers of the file part
    std::string tail_;  // the end of the file part and the closing delimiter
    std::FILE* file_ = nullptr;
    std::uint64_t fileSize_ = 0;
    std::uint64_t position_ = 0;  // bytes produced so far
};

// |name| for the file part's Content-Disposition: quotes, backslashes, control characters, and
// bytes that are not valid UTF-8 become '_', so that a header parser keeps the name whole; other
// characters, the extension among them, stay. "audio" when |name| is empty.
std::string MultipartFileName(const std::string& name);

// "CrispGUI-" and 32 random hexadecimal digits.
std::string RandomMultipartBoundary();

}  // namespace crisp::platform
