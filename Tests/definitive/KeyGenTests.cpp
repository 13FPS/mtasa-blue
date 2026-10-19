/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Exercise all four actual CKeyGenDE SDK methods without a game
 *****************************************************************************/
#include "../../Client/game_de/CKeyGenDE.h"

#include <array>
#include <clocale>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <type_traits>

namespace
{
    unsigned int checks = 0;

    void Check(bool condition, const char* description)
    {
        ++checks;
        if (!condition)
        {
            std::fprintf(stderr, "FAIL: %s\n", description);
            std::exit(EXIT_FAILURE);
        }
    }

    template <typename Call>
    void CheckInvalidArgument(Call call, const char* description)
    {
        bool refused = false;
        try
        {
            call();
        }
        catch (const std::invalid_argument&)
        {
            refused = true;
        }
        Check(refused, description);
    }
}

int main()
{
    using mta::game_de::CKeyGenDE;
    static_assert(std::is_base_of_v<CKeyGen, CKeyGenDE> && !std::is_abstract_v<CKeyGenDE>);
    static_assert(std::is_nothrow_default_constructible_v<CKeyGenDE>);
    CKeyGenDE implementation;
    CKeyGen&  keys = implementation;

    // Every executable starts in the C locale. Query it without mutating global
    // CRT state: uppercase tests must exercise the same CRT calls as production.
    const auto* locale = std::setlocale(LC_CTYPE, nullptr);
    Check(locale && std::strcmp(locale, "C") == 0, "The fixture must retain its default C locale");

    // Independently calculated with Python zlib.crc32, then complemented once
    // to expose the raw remainder. No production table or copied reference
    // implementation supplies the expected values.
    struct KnownKey
    {
        const char*   text;
        std::uint32_t key;
    };
    constexpr std::array<KnownKey, 9> known = {{{"", 0xFFFFFFFF},
                                                {"a", 0x174841BC},
                                                {"abc", 0xCADBBE3D},
                                                {"ABC", 0x5C7CFCB7},
                                                {"Hello, World!", 0x13B53C2F},
                                                {"PLAYER", 0x971E421C},
                                                {"weapon_pistol", 0x60DBFC63},
                                                {"123456789", 0x340BC6D9},
                                                {"abcdef", 0xB471C610}}};
    for (const auto& value : known)
    {
        Check(keys.GetKey(value.text) == value.key, "NUL-string hashing must match the independent raw CRC");
        Check(keys.GetKey(value.text, static_cast<int>(std::strlen(value.text))) == value.key, "Sized hashing must match the same byte range");
        Check(keys.AppendStringToKey(0xFFFFFFFF, value.text) == value.key, "Append must accept the initial raw remainder unchanged");
    }

    Check(keys.GetKey(nullptr, 0) == 0xFFFFFFFF, "Zero length must not read even a null pointer");
    Check(keys.GetKey(nullptr, -1) == 0xFFFFFFFF, "Negative length must not read even a null pointer");
    Check(keys.GetKey(nullptr, (std::numeric_limits<int>::min)()) == 0xFFFFFFFF, "The most negative length must not be converted into a large unsigned range");
    Check(keys.GetKey("ignored", 0) == 0xFFFFFFFF, "A zero-length nonempty string must retain the initial remainder");

    const char embeddedNul[] = {'a', '\0', 'b'};
    Check(keys.GetKey(embeddedNul, 3) == 0xEA17878E, "Sized hashing must consume embedded NUL and subsequent bytes");
    Check(keys.GetKey(embeddedNul) == 0x174841BC, "NUL-string hashing must stop at the first embedded NUL");
    Check(keys.GetKey(embeddedNul, 1) == 0x174841BC, "Sized hashing must stop at its exact bound");

    const unsigned char highBytes[] = {0x80, 0xFE, 0xFF, 0};
    const auto*         highText = reinterpret_cast<const char*>(highBytes);
    Check(keys.GetKey(highText) == 0x46B61ED3, "Ordinary hashing must accept high bytes without signed-index corruption");
    Check(keys.GetKey(highText, 3) == 0x46B61ED3, "Sized ordinary hashing must preserve high-byte values");
    std::array<unsigned char, 256> allBytes{};
    for (std::size_t index = 0; index < allBytes.size(); ++index)
        allBytes[index] = static_cast<unsigned char>(index);
    Check(keys.GetKey(reinterpret_cast<const char*>(allBytes.data()), static_cast<int>(allBytes.size())) == 0xD6FA738C,
          "Sized hashing must preserve the complete 256-byte domain");

    struct KnownAppend
    {
        std::uint32_t seed;
        std::uint32_t key;
    };
    constexpr std::array<KnownAppend, 5> appended = {
        {{0, 0xCA6598D0}, {1, 0xCBA7F2E7}, {0xFFFFFFFF, 0xCADBBE3D}, {0x12345678, 0x5DD2AF4D}, {0x80000000, 0xCA659850}}};
    for (const auto& value : appended)
    {
        Check(keys.AppendStringToKey(value.seed, "") == value.seed, "An empty append must retain every caller seed bit");
        Check(keys.AppendStringToKey(value.seed, "abc") == value.key, "Append must not complement or replace its caller's raw seed");
    }
    constexpr char combined[] = "abcdef";
    for (int split = 0; split <= 6; ++split)
        Check(keys.AppendStringToKey(keys.GetKey(combined, split), combined + split) == 0xB471C610,
              "Incremental append at every split must produce the independent whole-string key");

    Check(keys.GetUppercaseKey("") == 0xFFFFFFFF, "Empty uppercase hashing must retain the raw seed");
    Check(keys.GetUppercaseKey("abc") == 0x5C7CFCB7, "Lowercase input must pass through CRT uppercase conversion");
    Check(keys.GetUppercaseKey("aBc") == 0x5C7CFCB7, "Mixed-case input must have the same uppercase key");
    const unsigned char eofByte[] = {0xFF, 0};
    Check(keys.GetUppercaseKey(reinterpret_cast<const char*>(eofByte)) == 0x00FFFFFF, "Native signed 0xff must remain the valid CRT EOF value");
    const unsigned char mixedEof[] = {'a', 0xFF, 'i', 0};
    Check(keys.GetUppercaseKey(reinterpret_cast<const char*>(mixedEof)) == 0xEDC2C55C, "EOF preservation must coexist with ordinary CRT case conversion");

    constexpr std::array<unsigned char, 3> invalidCaseBytes = {0x80, 0xC0, 0xFE};
    for (const auto byte : invalidCaseBytes)
    {
        const unsigned char text[] = {byte, 0};
        CheckInvalidArgument([&] { keys.GetUppercaseKey(reinterpret_cast<const char*>(text)); }, "Undefined negative non-EOF CRT inputs must be refused");
    }
    CheckInvalidArgument([&] { keys.GetKey(nullptr); }, "Null NUL strings must be refused");
    CheckInvalidArgument([&] { keys.GetKey(nullptr, 1); }, "Positive sized hashing must refuse null input");
    CheckInvalidArgument([&] { keys.GetUppercaseKey(nullptr); }, "Uppercase hashing must refuse null input");
    CheckInvalidArgument([&] { keys.AppendStringToKey(0, nullptr); }, "Append must refuse null input");

    std::printf("Passed %u actual CKeyGenDE checks (%zu-bit)\n", checks, sizeof(void*) * 8);
    return EXIT_SUCCESS;
}
