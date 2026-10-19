/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual MTA key generation with native DE raw CRC semantics
 *****************************************************************************/
#pragma once

#include <game/CKeyGen.h>

namespace mta::game_de
{
    // Hashing is a pure SDK service: it needs neither a live engine binding nor
    // a partially implemented CGame. The module retains ownership of this object.
    class CKeyGenDE final : public CKeyGen
    {
    public:
        CKeyGenDE() noexcept = default;

        unsigned int GetKey(const char* szString, int iLength) override;
        unsigned int GetKey(const char* szString) override;
        unsigned int GetUppercaseKey(const char* szString) override;
        unsigned int AppendStringToKey(unsigned int uiKey, const char* szString) override;

        // GetKey/Append accept every byte. Uppercase follows DE's signed-byte
        // CRT call only for ASCII and 0xff (EOF); 0x80..0xfe throw instead of
        // passing invalid negative arguments to toupper. Null strings throw,
        // except that a nonpositive sized hash performs no read and returns its seed.
    };
}
