/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Preserve native CRC state and the existing four-method SDK contract
 *****************************************************************************/
#include "CKeyGenDE.h"

#include <array>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <stdexcept>

namespace mta::game_de
{
    namespace
    {
        constexpr std::uint32_t initialRemainder = 0xFFFFFFFF;

        constexpr std::array<std::uint32_t, 256> MakeTable() noexcept
        {
            std::array<std::uint32_t, 256> result{};
            for (std::uint32_t index = 0; index < result.size(); ++index)
            {
                auto value = index;
                for (unsigned bit = 0; bit < 8; ++bit)
                    value = (value >> 1) ^ ((value & 1) ? 0xEDB88320 : 0);
                result[index] = value;
            }
            return result;
        }

        // Generated polynomial entries match all 1024 bytes of DE's readonly
        // table at RVA0x430ad30. Its native string loops retain the raw remainder
        // (seed 0xffffffff, no final xor), unlike MTA's complemented zlib wrapper.
        constexpr auto crcTable = MakeTable();
        static_assert(sizeof(unsigned int) == sizeof(std::uint32_t));

        std::uint32_t Consume(std::uint32_t remainder, unsigned char byte) noexcept
        {
            return (remainder >> 8) ^ crcTable[static_cast<unsigned char>(remainder ^ byte)];
        }

        const unsigned char* RequireString(const char* text)
        {
            if (!text)
                throw std::invalid_argument("CKeyGenDE requires a non-null string");
            return reinterpret_cast<const unsigned char*>(text);
        }

        std::uint32_t Append(std::uint32_t remainder, const char* text)
        {
            const auto* bytes = RequireString(text);
            while (*bytes)
                remainder = Consume(remainder, *bytes++);
            return remainder;
        }
    }

    unsigned int CKeyGenDE::GetKey(const char* szString, int iLength)
    {
        // The sized/append contracts come from the address-matched classic
        // CKeyGen SDK: sized input includes embedded NUL, nonpositive lengths
        // do not read input, and append continues the caller's raw state.
        // These rules are distinct from DE's independently proven NUL loops.
        // Contract reference: gta-reversed KeyGen.h blob
        // 7075634aaa3847ae3e71865cd2eb3f0b29ace307; all four addresses match CKeyGenSA.
        auto remainder = initialRemainder;
        if (iLength <= 0)
            return remainder;
        const auto* bytes = RequireString(szString);
        for (int index = 0; index < iLength; ++index)
            remainder = Consume(remainder, bytes[index]);
        return remainder;
    }

    unsigned int CKeyGenDE::GetKey(const char* szString)
    {
        return Append(initialRemainder, szString);
    }

    unsigned int CKeyGenDE::GetUppercaseKey(const char* szString)
    {
        auto        remainder = initialRemainder;
        const auto* bytes = RequireString(szString);
        while (const auto byte = *bytes++)
        {
            // DE RVA0x1150f50 sign-extends each byte into the imported CRT
            // toupper. Keep that call's locale behavior rather than replacing
            // it with an ASCII lookup. Negative non-EOF values are outside its
            // defined domain, so refuse them instead of invoking CRT UB.
            if (byte > 0x7F && byte != 0xFF)
                throw std::invalid_argument("CKeyGenDE uppercase input is outside the native CRT domain");
            const auto upper = static_cast<unsigned char>(std::toupper(byte == 0xFF ? EOF : byte));
            remainder = Consume(remainder, upper);
        }
        return remainder;
    }

    unsigned int CKeyGenDE::AppendStringToKey(unsigned int uiKey, const char* szString)
    {
        return Append(uiKey, szString);
    }
}
