// Xc* crypto exports against published test vectors.

#include <string>

#include "test_support.h"

namespace {

using ShaInit = void(XB_STDCALL*)(uint8_t*);
using ShaUpdate = void(XB_STDCALL*)(uint8_t*, const uint8_t*, uint32_t);
using ShaFinal = void(XB_STDCALL*)(uint8_t*, uint8_t*);
using Hmac = void(XB_STDCALL*)(const uint8_t*, uint32_t, const uint8_t*, uint32_t, const uint8_t*, uint32_t, uint8_t*);
using Rc4Key = void(XB_STDCALL*)(uint8_t*, uint32_t, const uint8_t*);
using Rc4Crypt = void(XB_STDCALL*)(uint8_t*, uint32_t, uint8_t*);

std::string hex(const uint8_t* bytes, size_t length) {
    std::string s;
    char buf[3];
    for (size_t i = 0; i < length; ++i) std::snprintf(buf, sizeof buf, "%02x", bytes[i]), s += buf;
    return s;
}

const uint8_t* u8(const char* s) { return reinterpret_cast<const uint8_t*>(s); }

}  // namespace

int main() {
    const auto init = test::find_export<ShaInit>("XcSHAInit");
    const auto update = test::find_export<ShaUpdate>("XcSHAUpdate");
    const auto final = test::find_export<ShaFinal>("XcSHAFinal");
    const auto hmac = test::find_export<Hmac>("XcHMAC");
    const auto rc4_key = test::find_export<Rc4Key>("XcRC4Key");
    const auto rc4 = test::find_export<Rc4Crypt>("XcRC4Crypt");

    uint8_t context[116], digest[20];
    auto sha = [&](const char* text, uint32_t split) {
        init(context);
        const uint32_t length = static_cast<uint32_t>(std::strlen(text));
        update(context, u8(text), split);
        update(context, u8(text) + split, length - split);
        final(context, digest);
        return hex(digest, 20);
    };
    CHECK(sha("abc", 1) == "a9993e364706816aba3e25717850c26c9cd0d89d");
    const char* two_blocks = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
    CHECK(sha(two_blocks, 0) == "84983e441c3bd26ebaae4aa1f95129e5e54670f1");
    CHECK(sha(two_blocks, 55) == "84983e441c3bd26ebaae4aa1f95129e5e54670f1");
    CHECK(sha("", 0) == "da39a3ee5e6b4b0d3255bfef95601890afd80709");

    // RFC 2202 test case 2, data split over the two inputs.
    hmac(u8("Jefe"), 4, u8("what do ya "), 11, u8("want for nothing?"), 17, digest);
    CHECK(hex(digest, 20) == "effcdf6ae5eb2fa2d27416d5f184df9c259a7c79", "%s", hex(digest, 20).c_str());

    uint8_t state[258];
    uint8_t text[9];
    std::memcpy(text, "Plaintext", 9);
    rc4_key(state, 3, u8("Key"));
    rc4(state, 9, text);
    CHECK(hex(text, 9) == "bbf316e8d940af0ad3", "%s", hex(text, 9).c_str());

    if (test::g_failures) return 1;
    std::printf("crypto_test passed\n");
    return 0;
}
