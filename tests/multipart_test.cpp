#include "check.h"
#include "test_support.h"

#include "platform/multipart.h"

#include <cstdio>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

namespace fs = std::filesystem;
using crisp::platform::MultipartBody;
using crisp::platform::MultipartFile;
using crisp::platform::MultipartFileName;
using crisp::platform::RandomMultipartBoundary;

namespace {

using Fields = std::vector<std::pair<std::string, std::string>>;

// Everything |body| produces, read |capacity| bytes at a time; false when a read fails.
bool ReadAll(MultipartBody& body, std::size_t capacity, std::string& bytes, std::string& error) {
    std::string buffer(capacity, '\0');
    for (;;) {
        std::size_t count = 0;
        if (!body.Read(&buffer[0], capacity, count, error)) {
            return false;
        }
        if (count == 0) {
            return true;
        }
        bytes.append(buffer, 0, count);
    }
}

}  // namespace

int main() {
    const fs::path scratch = crisp::test::FreshScratchDirectory("multipart_test.scratch");
    std::string content("RIFF\x24\x00\x00\x00WAVE", 12);
    for (int i = 0; i < 3000; ++i) {
        content.push_back(static_cast<char>(i % 256));
    }
    const fs::path audio = scratch / fs::u8path(u8"音声 テスト.wav");
    CHECK(crisp::test::WriteFile(audio, content));

    // The exact layout, read in pieces of several sizes.
    const Fields fields = {{"language", "ja"}, {"prompt", "line1\r\nline2 \"quoted\""}};
    MultipartFile file;
    file.path = audio;
    const std::string expected =
        "--BOUNDARY\r\n"
        "Content-Disposition: form-data; name=\"language\"\r\n\r\n"
        "ja\r\n"
        "--BOUNDARY\r\n"
        "Content-Disposition: form-data; name=\"prompt\"\r\n\r\n"
        "line1\r\nline2 \"quoted\"\r\n"
        "--BOUNDARY\r\n"
        "Content-Disposition: form-data; name=\"file\"; filename=\"\xE9\x9F\xB3\xE5\xA3\xB0 "
        "\xE3\x83\x86\xE3\x82\xB9\xE3\x83\x88.wav\"\r\n"
        "Content-Type: application/octet-stream\r\n\r\n" +
        content + "\r\n--BOUNDARY--\r\n";
    for (const std::size_t capacity : {std::size_t(1), std::size_t(7), std::size_t(4096), std::size_t(65536)}) {
        MultipartBody body(fields, file, "BOUNDARY");
        CHECK(body.Error().empty());
        CHECK(body.ContentType() == "multipart/form-data; boundary=BOUNDARY");
        CHECK(body.FileSize() == content.size());
        CHECK(body.Size() == expected.size());
        std::string bytes;
        std::string error;
        CHECK(ReadAll(body, capacity, bytes, error));
        CHECK(bytes == expected);
    }

    // A given file name is cleaned for the header; a missing one comes from the path.
    {
        MultipartFile named = file;
        named.fileName = "a\"b\\c\r\n.mp3";
        MultipartBody body({}, named, "B");
        std::string bytes;
        std::string error;
        CHECK(ReadAll(body, 100, bytes, error));
        CHECK(bytes.find("filename=\"a_b_c__.mp3\"\r\n") != std::string::npos);
        CHECK(bytes.compare(0, 5, "--B\r\n") == 0);
    }
    CHECK(MultipartFileName("") == "audio");
    CHECK(MultipartFileName("\xFF\xFE" "abc.wav") == "__abc.wav");
    CHECK(MultipartFileName("\x7F.flac") == "_.flac");
    CHECK(MultipartFileName("\xE6\x97\xA5\xE6\x9C\xAC.m4a") == "\xE6\x97\xA5\xE6\x9C\xAC.m4a");
    CHECK(MultipartFileName("\xE6\x97\xA5\xE6\x9C") == "\xE6\x97\xA5__");  // a sequence cut short

    // Field names must be plain; a field value must not contain a given boundary.
    {
        MultipartBody body({{"bad name", "x"}}, file, "B");
        CHECK(!body.Error().empty() && !body.FileFailed());
        CHECK(body.Size() == 0);
        std::size_t count = 1;
        std::string error;
        char buffer[16];
        CHECK(!body.Read(buffer, sizeof(buffer), count, error) && count == 0 && error == body.Error());
    }
    CHECK(!MultipartBody({{"prompt", "x--B y"}}, file, "B").Error().empty());
    {
        const std::string boundary = RandomMultipartBoundary();
        CHECK(boundary.size() == 45 && boundary.compare(0, 13, "CrispRustGUI-") == 0);
        CHECK(RandomMultipartBoundary() != boundary);
        // Without a given boundary, the body draws one.
        MultipartBody body({{"prompt", "a"}}, file);
        CHECK(body.Error().empty() && body.Boundary().size() == 45);
    }

    // Files that cannot be sent.
    {
        MultipartFile missing;
        missing.path = scratch / "missing.wav";
        MultipartBody body({}, missing);
        CHECK(!body.Error().empty() && body.FileFailed());
        MultipartFile folder;
        folder.path = scratch;
        CHECK(MultipartBody({}, folder).FileFailed());
    }

    // A file that changes while it is sent.
    {
        const fs::path changing = scratch / "changing.wav";
        CHECK(crisp::test::WriteFile(changing, std::string(100, 'a')));
        MultipartFile shrinking;
        shrinking.path = changing;
        MultipartBody body({}, shrinking, "B");
        CHECK(body.Error().empty());
        if (crisp::test::WriteFile(changing, std::string(10, 'b'))) {
            std::string bytes;
            std::string error;
            CHECK(!ReadAll(body, 4096, bytes, error));
            CHECK(error == "The file became shorter while it was being uploaded.");
        } else {
            std::printf("multipart_test: the open file could not be rewritten; the shrink check is skipped\n");
        }
    }
    {
        const fs::path changing = scratch / "growing.wav";
        CHECK(crisp::test::WriteFile(changing, std::string(10, 'a')));
        MultipartFile growing;
        growing.path = changing;
        MultipartBody body({}, growing, "B");
        CHECK(body.Error().empty());
        if (crisp::test::WriteFile(changing, std::string(15, 'b'))) {
            std::string bytes;
            std::string error;
            CHECK(!ReadAll(body, 4096, bytes, error));
            CHECK(error == "The file grew while it was being uploaded.");
        } else {
            std::printf("multipart_test: the open file could not be rewritten; the growth check is skipped\n");
        }
    }

    std::error_code error;
    fs::remove_all(scratch, error);
    return crisp::test::Result();
}
