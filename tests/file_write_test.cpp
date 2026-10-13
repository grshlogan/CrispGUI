#include "check.h"
#include "test_support.h"

#include "platform/files.h"

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/resource.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#include <algorithm>
#include <cstdint>
#include <csignal>
#include <cstdio>
#include <filesystem>
#include <set>
#include <string>
#include <system_error>
#include <vector>

namespace fs = std::filesystem;
using crisp::platform::AtomicTemporaryPath;
using crisp::platform::ReadFileBytes;
using crisp::platform::RenameOverFile;
using crisp::platform::WriteFileAtomically;

namespace {

// A new empty directory |name| below |parent|.
fs::path Case(const fs::path& parent, const char* name) {
    const fs::path directory = parent / name;
    std::error_code error;
    fs::remove_all(directory, error);
    fs::create_directories(directory, error);
    return directory;
}

// The names in |directory|, sorted.
std::vector<std::string> Names(const fs::path& directory) {
    std::vector<std::string> names;
    std::error_code error;
    for (const fs::directory_entry& entry : fs::directory_iterator(directory, error)) {
        names.push_back(entry.path().filename().u8string());
    }
    std::sort(names.begin(), names.end());
    return names;
}

// The bytes of the file |path|, or a marker that no file content equals.
std::string Content(const fs::path& path) {
    const auto file = ReadFileBytes(path, 64u << 20);
    return file.Ok() ? file.bytes : std::string("<unreadable: ") + file.error + ">";
}

using Entries = std::vector<std::string>;

}  // namespace

int main() {
    const fs::path scratch = crisp::test::FreshScratchDirectory("file_write_test.scratch");
    std::string error;

    // A new file has exactly the bytes: none at all, a NUL and non-ASCII bytes, and a body that
    // takes many writes.
    {
        const fs::path directory = Case(scratch, "new");
        CHECK(WriteFileAtomically(directory / "empty.bin", "", error));
        CHECK(error.empty());
        CHECK(Content(directory / "empty.bin").empty() && fs::exists(directory / "empty.bin"));

        const std::string odd("a\0b\xE3\x81\x82\r\n\xFF", 9);
        CHECK(WriteFileAtomically(directory / "odd.bin", odd, error));
        CHECK(Content(directory / "odd.bin") == odd);

        std::string big;
        for (int i = 0; i < (1 << 20) + 17; ++i) {
            big.push_back(static_cast<char>((i * 31) % 251));
        }
        CHECK(WriteFileAtomically(directory / "big.bin", big, error));
        CHECK(Content(directory / "big.bin") == big);
        // Only the three files: no temporary file stays.
        CHECK(Names(directory) == Entries({"big.bin", "empty.bin", "odd.bin"}));
    }

    // An existing file is replaced, also by something shorter or empty, and no temporary file stays.
    {
        const fs::path directory = Case(scratch, "replace");
        const fs::path target = directory / "out.srt";
        CHECK(crisp::test::WriteFile(target, "the old content, which is longer than the new one"));
        CHECK(WriteFileAtomically(target, "new", error));
        CHECK(error.empty() && Content(target) == "new");
        CHECK(WriteFileAtomically(target, "", error));
        CHECK(Content(target).empty() && fs::exists(target));
        CHECK(WriteFileAtomically(target, "last", error));
        CHECK(Content(target) == "last");
        CHECK(Names(directory) == Entries({"out.srt"}));
    }

    // A path with Japanese characters works for creating and for replacing.
    {
        const fs::path directory = Case(scratch, "japanese") / fs::u8path(u8"字幕 テスト");
        std::error_code ignored;
        fs::create_directories(directory, ignored);
        const fs::path target = directory / fs::u8path(u8"動画 の 字幕.srt");
        CHECK(WriteFileAtomically(target, "one", error));
        CHECK(Content(target) == "one");
        CHECK(WriteFileAtomically(target, "two \xE3\x81\x82", error));
        CHECK(Content(target) == "two \xE3\x81\x82");
        CHECK(Names(directory) == Entries({fs::u8path(u8"動画 の 字幕.srt").filename().u8string()}));
    }

    // The temporary path is in the destination's directory, never in the system's temporary
    // folder, is named after the destination, and differs between calls.
    {
        const fs::path destination = scratch / "somewhere" / "name.srt";
        const fs::path first = AtomicTemporaryPath(destination);
        const fs::path second = AtomicTemporaryPath(destination);
        CHECK(first.parent_path() == destination.parent_path());
        CHECK(second.parent_path() == destination.parent_path());
        CHECK(first != second && first != destination);
        const std::string name = first.filename().u8string();
        CHECK(name.rfind("name.srt.", 0) == 0 && name.size() > 13 && name.compare(name.size() - 4, 4, ".tmp") == 0);
        std::error_code tempError;
        const fs::path systemTemp = fs::temp_directory_path(tempError);
        CHECK(tempError || first.parent_path() != systemTemp);
        // A bare file name stays in the current directory.
        CHECK(AtomicTemporaryPath("bare.srt").parent_path().empty());
        std::set<std::string> names;
        for (int i = 0; i < 1000; ++i) {
            names.insert(AtomicTemporaryPath(destination).filename().u8string());
        }
        CHECK(names.size() == 1000);
    }

    // The temporary name keeps only the start of a long destination name, cut at a character
    // boundary, so that it stays valid wherever the destination can be created.
    {
        const fs::path directory = scratch / "names";
        // What a temporary name keeps of the destination's name: the name without the 21 characters
        // of ".<16 digits>.tmp".
        const auto kept = [&](const std::string& destinationName) {
            const std::string name = AtomicTemporaryPath(directory / fs::u8path(destinationName)).filename().u8string();
            CHECK(name.size() > 21 && name.compare(name.size() - 4, 4, ".tmp") == 0);
            return name.size() > 21 ? name.substr(0, name.size() - 21) : std::string();
        };
        // A short name is kept whole.
        CHECK(kept("name.srt") == "name.srt");
        // A long one is cut to 40 units: ASCII characters count one each.
        const std::string longAscii = std::string(250, 'a') + ".srt";
        CHECK(kept(longAscii) == std::string(40, 'a'));
        // A cut that falls inside a character moves back before it, so that no character is split:
        // 39 letters, a character of four bytes in UTF-8 and two units in UTF-16, and more.
        const std::string split = std::string(39, 'a') + "\xF0\xA0\xAE\xB7" + "bbbb.srt";
        CHECK(kept(split) == std::string(39, 'a'));
        // Kanji: the kept part is a whole number of characters and starts the destination's name.
        std::string kanji;
        for (int i = 0; i < 80; ++i) {
            kanji += "\xE5\xAD\x97";
        }
        const std::string japanese = kanji + ".srt";
        const std::string start = kept(japanese);
        CHECK(!start.empty() && start.size() < japanese.size() && japanese.compare(0, start.size(), start) == 0);
        CHECK((static_cast<unsigned char>(japanese[start.size()]) & 0xC0) != 0x80);
    }

    // The directory does not exist: nothing is written, nothing is created, and the message names
    // the step.
    {
        const fs::path directory = Case(scratch, "missing");
        CHECK(!WriteFileAtomically(directory / "no such folder" / "out.srt", "x", error));
        CHECK(error.rfind("Could not create the temporary file ", 0) == 0);
        CHECK(Names(directory).empty());
        // The message is cleared by the next success.
        CHECK(WriteFileAtomically(directory / "out.srt", "x", error));
        CHECK(error.empty());
    }

    // The destination is a directory: it stays as it was, with what is in it, and no temporary
    // file stays next to it.
    {
        const fs::path directory = Case(scratch, "directory");
        const fs::path target = directory / "target";
        std::error_code ignored;
        fs::create_directories(target, ignored);
        CHECK(crisp::test::WriteFile(target / "keep.txt", "keep"));
        CHECK(!WriteFileAtomically(target, "new", error));
        CHECK(error.rfind("Could not replace ", 0) == 0);
        CHECK(fs::is_directory(target));
        CHECK(Content(target / "keep.txt") == "keep");
        CHECK(Names(directory) == Entries({"target"}));
        CHECK(Names(target) == Entries({"keep.txt"}));
    }

    // RenameOverFile on its own: the source moves over an existing destination; a missing source
    // fails and leaves the destination alone.
    {
        const fs::path directory = Case(scratch, "replace-file");
        CHECK(crisp::test::WriteFile(directory / "source", "source"));
        CHECK(crisp::test::WriteFile(directory / "destination", "destination"));
        std::string reason;
        CHECK(RenameOverFile(directory / "source", directory / "destination", reason));
        CHECK(Content(directory / "destination") == "source");
        CHECK(Names(directory) == Entries({"destination"}));
        CHECK(!RenameOverFile(directory / "source", directory / "destination", reason));
        CHECK(!reason.empty());
        CHECK(Content(directory / "destination") == "source");
    }

#if !defined(_WIN32)
    // A folder that cannot be written to: the existing file in it stays as it was. Skipped when
    // the process may write there anyway, as the superuser does.
    {
        const fs::path directory = Case(scratch, "read-only");
        const fs::path target = directory / "out.srt";
        CHECK(crisp::test::WriteFile(target, "keep"));
        chmod(directory.c_str(), 0555);
        if (access(directory.c_str(), W_OK) != 0) {
            CHECK(!WriteFileAtomically(target, "new", error));
            CHECK(error.rfind("Could not create the temporary file ", 0) == 0);
            CHECK(Content(target) == "keep");
        }
        chmod(directory.c_str(), 0755);
        CHECK(Names(directory) == Entries({"out.srt"}));
    }

    // The replacement is not a write in place: the destination becomes a new file, so a reader that
    // has the old one open still sees all of the old bytes, and never a mixture.
    {
        const fs::path directory = Case(scratch, "inode");
        const fs::path target = directory / "out.srt";
        CHECK(crisp::test::WriteFile(target, "the old bytes"));
        struct stat before;
        CHECK(stat(target.c_str(), &before) == 0);
        std::FILE* reader = std::fopen(target.c_str(), "rb");
        CHECK(reader != nullptr);
        CHECK(WriteFileAtomically(target, "new", error));
        struct stat after;
        CHECK(stat(target.c_str(), &after) == 0);
        CHECK(before.st_ino != after.st_ino);
        if (reader != nullptr) {
            char buffer[64];
            const std::size_t count = std::fread(buffer, 1, sizeof(buffer), reader);
            CHECK(std::string(buffer, count) == "the old bytes");
            std::fclose(reader);
        }
        CHECK(Content(target) == "new");
        CHECK(Names(directory) == Entries({"out.srt"}));
    }

    // A destination whose name is close to the limit of 255 bytes, in ASCII and in Japanese, is
    // written atomically, created and replaced, as a plain write could create it.
    {
        const fs::path directory = Case(scratch, "long-names");
        std::string kanji;
        for (int i = 0; i < 80; ++i) {
            kanji += "\xE5\xAD\x97";
        }
        const fs::path ascii = directory / (std::string(251, 'a') + ".srt");
        const fs::path japanese = directory / fs::u8path(kanji + ".srt");
        for (const fs::path& target : {ascii, japanese}) {
            CHECK(WriteFileAtomically(target, "one", error));
            CHECK(Content(target) == "one");
            CHECK(WriteFileAtomically(target, "two", error));
            CHECK(Content(target) == "two");
        }
        CHECK(Names(directory).size() == 2);
    }

#if defined(__linux__)
    // No descriptor stays open, on success and on each failure.
    {
        const auto openDescriptors = [] {
            std::size_t count = 0;
            std::error_code listError;
            for (const fs::directory_entry& entry : fs::directory_iterator("/proc/self/fd", listError)) {
                static_cast<void>(entry);
                ++count;
            }
            return count;
        };
        const fs::path directory = Case(scratch, "descriptors");
        const fs::path target = directory / "out.srt";
        CHECK(WriteFileAtomically(target, "warm up", error));
        const std::size_t before = openDescriptors();
        for (int i = 0; i < 50; ++i) {
            CHECK(WriteFileAtomically(target, "again", error));
            CHECK(!WriteFileAtomically(directory / "no such folder" / "out.srt", "x", error));
            CHECK(!WriteFileAtomically(directory, "x", error));
        }
        CHECK(openDescriptors() == before);
        CHECK(before > 0);
    }
#endif

    // A write that fails after the temporary file exists, made to fail by a limit on the size of
    // files (RLIMIT_FSIZE): the existing file stays as it was, no temporary file stays, and the
    // message names the step. A body larger than the stdio buffer fails while it is written, a
    // small one while it is flushed.
    {
        const fs::path directory = Case(scratch, "limit");
        const fs::path target = directory / "out.srt";
        CHECK(crisp::test::WriteFile(target, "keep"));
        std::signal(SIGXFSZ, SIG_IGN);  // the write then fails with an error instead of ending the process
        struct rlimit saved;
        CHECK(getrlimit(RLIMIT_FSIZE, &saved) == 0);
        struct rlimit limit = saved;
        limit.rlim_cur = 10;
        CHECK(setrlimit(RLIMIT_FSIZE, &limit) == 0);

        CHECK(!WriteFileAtomically(target, std::string(200000, 'x'), error));
        CHECK(error.rfind("Could not write the temporary file ", 0) == 0);
        CHECK(Content(target) == "keep");
        CHECK(Names(directory) == Entries({"out.srt"}));

        CHECK(!WriteFileAtomically(target, std::string(100, 'x'), error));
        CHECK(error.rfind("Could not flush the temporary file ", 0) == 0);
        CHECK(Content(target) == "keep");
        CHECK(Names(directory) == Entries({"out.srt"}));

        CHECK(setrlimit(RLIMIT_FSIZE, &saved) == 0);
        CHECK(WriteFileAtomically(target, std::string(100, 'y'), error));
        CHECK(Content(target) == std::string(100, 'y'));
        CHECK(Names(directory) == Entries({"out.srt"}));
    }
#else
    // The destination is held open by another handle that allows no delete sharing, so the
    // replacement fails: the file stays as it was, and no temporary file stays. When the handle is
    // closed, the same call succeeds.
    {
        const fs::path directory = Case(scratch, "held-open");
        const fs::path target = directory / fs::u8path(u8"held 字幕.srt");
        CHECK(crisp::test::WriteFile(target, "old"));
        HANDLE held =
            CreateFileW(target.c_str(), GENERIC_READ, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        CHECK(held != INVALID_HANDLE_VALUE);
        CHECK(!WriteFileAtomically(target, "new", error));
        CHECK(error.rfind("Could not replace ", 0) == 0);
        if (held != INVALID_HANDLE_VALUE) {
            CloseHandle(held);
        }
        CHECK(Content(target) == "old");
        CHECK(Names(directory) == Entries({target.filename().u8string()}));
        CHECK(WriteFileAtomically(target, "new", error));
        CHECK(Content(target) == "new");
        CHECK(Names(directory) == Entries({target.filename().u8string()}));
    }
#endif

    std::error_code ignored;
    fs::remove_all(scratch, ignored);
    return crisp::test::Result();
}
