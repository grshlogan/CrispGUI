#include "check.h"

#include "util/sha256.h"

#include <string>

using crisp::util::Sha256;
using crisp::util::Sha256Hex;

int main() {
    // FIPS 180-4 example messages.
    CHECK(Sha256Hex("") == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    CHECK(Sha256Hex("abc") == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    CHECK(Sha256Hex("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq") ==
          "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
    CHECK(Sha256Hex(std::string(1000000, 'a')) == "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");

    // Lengths around the padding boundary: 55 bytes fit one block, 56 need two.
    CHECK(Sha256Hex(std::string(55, 'a')) == "9f4390f8d30c2dd92ec9f095b65e2b9ae9b0a925a5258e241c9f1e910f734318");
    CHECK(Sha256Hex(std::string(56, 'a')) == "b35439a4ac6f0948b6d6f9e3c6af0f5f590ce20f1bde7090ef7970686ec6738a");
    CHECK(Sha256Hex(std::string(64, 'a')) == "ffe054fe7ae0cb6dc65c3af9b61d5209f439851db43d0ba5997337df154668eb");

    // Feeding the data in pieces gives the same digest.
    const std::string text = "The quick brown fox jumps over the lazy dog, again and again and again.";
    Sha256 pieces;
    for (std::size_t i = 0; i < text.size(); i += 7) {
        pieces.Update(text.substr(i, 7));
    }
    CHECK(pieces.FinishHex() == Sha256Hex(text));
    CHECK(Sha256Hex("The quick brown fox jumps over the lazy dog") ==
          "d7a8fbb307d7809469ca9abcb0082e4f8d5651e46d3cdb762d02d0bf37c9e592");
    return crisp::test::Result();
}
