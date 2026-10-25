/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Reuse raw KeyGen with the classic source ASCII normalization
 ****************************************************************************/
#include "NativeClassicAnimationNameHash.h"
#include "CKeyGenDE.h"
#include <stdexcept>
#include <string>
namespace mta::game_de::NativeClassicAnimationNamePolicy
{
    std::uint32_t Hash(const char* name)
    {
        if (!name)
            throw std::invalid_argument("Classic animation name hash requires a non-null string");
        std::string normalized(name);
        for (char& value : normalized)
        {
            const auto byte = static_cast<unsigned char>(value);
            if (byte >= 'a' && byte <= 'z')
                value = static_cast<char>(byte - 0x20u);
        }
        // Classic 4CF2D0 uses the source ASCII key policy. DE's uppercase
        // entry follows CRT locale rules, so normalize explicitly and reuse
        // the already-qualified raw key generator instead of that entry.
        CKeyGenDE keys;
        return keys.GetKey(normalized.c_str());
    }
}
