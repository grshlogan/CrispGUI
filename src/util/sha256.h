#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace crisp::util {

// SHA-256 after FIPS 180-4, for content hashes such as a profile's in a job snapshot.
class Sha256 {
public:
    Sha256();

    void Update(const void* data, std::size_t size);
    void Update(std::string_view data) { Update(data.data(), data.size()); }

    // Completes the digest and returns it as 64 lowercase hexadecimal digits. Call it once;
    // the object is spent afterwards.
    std::string FinishHex();

private:
    void ProcessBlock(const unsigned char* block);

    std::uint32_t state_[8];
    unsigned char buffer_[64];
    std::size_t bufferSize_ = 0;
    std::uint64_t totalBytes_ = 0;
};

// The SHA-256 of |data| as 64 lowercase hexadecimal digits.
std::string Sha256Hex(std::string_view data);

}  // namespace crisp::util
