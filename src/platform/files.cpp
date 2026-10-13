#include "platform/files.h"

#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <functional>
#include <random>
#include <system_error>
#include <thread>

namespace crisp::platform {
namespace {

// 16 random hexadecimal digits for a temporary file's name. Like the multipart boundary, the
// clocks, the thread, and a counter seed the generator; std::random_device could throw.
std::string RandomHexDigits() {
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
    std::uint64_t bits = generator();
    std::string digits;
    for (int digit = 0; digit < 16; ++digit) {
        digits.push_back(kHex[bits & 0xF]);
        bits >>= 4;
    }
    return digits;
}

// The start of a destination's file name that a temporary file's name keeps: at most
// |kKeptNameUnits| units of the platform's native encoding, cut so that no character is split
// (UTF-8 bytes on POSIX, UTF-16 units on Windows). A name that is not cut would add 21 units to
// the destination's, so that a name close to the limit of the file system could be written
// directly but not atomically.
constexpr std::size_t kKeptNameUnits = 40;

std::filesystem::path::string_type KeptStartOf(const std::filesystem::path::string_type& name) {
    if (name.size() <= kKeptNameUnits) {
        return name;
    }
    std::size_t cut = kKeptNameUnits;
    if constexpr (sizeof(std::filesystem::path::value_type) == 2) {
        // A high surrogate must not be the last unit kept.
        const auto last = static_cast<unsigned>(name[cut - 1]);
        if (last >= 0xD800 && last <= 0xDBFF) {
            --cut;
        }
    } else {
        // A continuation byte must not be the first one dropped.
        while (cut > 0 && (static_cast<unsigned char>(name[cut]) & 0xC0) == 0x80) {
            --cut;
        }
    }
    return name.substr(0, cut);
}

}  // namespace

ReadFileResult ReadFileBytes(const std::filesystem::path& path, std::size_t maxBytes) {
    ReadFileResult result;
    std::FILE* file = OpenFileForReading(path);
    if (file == nullptr) {
        result.error = std::generic_category().message(errno);
        return result;
    }
    char buffer[16384];
    for (;;) {
        const std::size_t count = std::fread(buffer, 1, sizeof(buffer), file);
        if (result.bytes.size() + count > maxBytes) {
            result.bytes.clear();
            result.error = "The file is larger than " + std::to_string(maxBytes / 1024) + " KiB.";
            break;
        }
        result.bytes.append(buffer, count);
        if (count < sizeof(buffer)) {
            if (std::ferror(file) != 0) {
                result.bytes.clear();
                result.error = "The file could not be read completely.";
            }
            break;
        }
    }
    std::fclose(file);
    return result;
}

std::filesystem::path AtomicTemporaryPath(const std::filesystem::path& destination) {
    std::filesystem::path name(KeptStartOf(destination.filename().native()));
    name += "." + RandomHexDigits() + ".tmp";
    return destination.parent_path() / name;
}

bool WriteFileAtomically(const std::filesystem::path& path, std::string_view bytes, std::string& error) {
    error.clear();
    // A name that is not taken: the random part makes a second try a formality.
    std::error_code ignored;
    std::filesystem::path temporary;
    for (int attempt = 0; attempt < 8; ++attempt) {
        temporary = AtomicTemporaryPath(path);
        if (!std::filesystem::exists(temporary, ignored)) {
            break;
        }
    }
    if (std::filesystem::exists(temporary, ignored)) {
        error = "Could not find a free name for the temporary file next to " + path.u8string() + ".";
        return false;
    }
    const std::string name = temporary.u8string();

    std::FILE* file = OpenFileForWriting(temporary);
    if (file == nullptr) {
        error = "Could not create the temporary file " + name + ": " + std::generic_category().message(errno) + ".";
        return false;
    }
    bool ok = true;
    if (!bytes.empty() && std::fwrite(bytes.data(), 1, bytes.size(), file) != bytes.size()) {
        error = "Could not write the temporary file " + name + ": " + std::generic_category().message(errno) + ".";
        ok = false;
    }
    std::string reason;
    if (ok && !FlushFileToDisk(file, reason)) {
        error = "Could not flush the temporary file " + name + " to the disk: " + reason + ".";
        ok = false;
    }
    if (std::fclose(file) != 0 && ok) {
        error = "Could not close the temporary file " + name + ": " + std::generic_category().message(errno) + ".";
        ok = false;
    }
    if (ok && !RenameOverFile(temporary, path, reason)) {
        error = "Could not replace " + path.u8string() + " with the temporary file " + name + ": " + reason + ".";
        ok = false;
    }
    if (!ok) {
        std::filesystem::remove(temporary, ignored);
    }
    return ok;
}

}  // namespace crisp::platform
