/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Reuse the existing raw key generator for classic sequence names
 ****************************************************************************/
#include "NativeAnimationSequenceName.h"
#include "CKeyGenDE.h"

namespace mta::game_de::NativeAnimationSequenceNamePolicy
{
    bool Hash(const NativeAnimationSequenceName& value, std::uint32_t& output) noexcept
    {
        const auto  input = value;
        std::size_t length = 0;
        if (!Length(input, length))
            return false;
        NativeAnimationSequenceName normalized{};
        for (std::size_t index = 0; index != length; ++index)
        {
            const auto byte = static_cast<unsigned char>(input[index]);
            normalized[index] = byte >= 'a' && byte <= 'z' ? static_cast<char>(byte - 0x20u) : input[index];
        }
        try
        {
            CKeyGenDE  keys;
            const auto hash = keys.GetKey(normalized.data());
            output = hash;
            return true;
        }
        catch (...)
        {
            // No caller-visible hash is published on a refused CPU policy.
            // The current input is nonnull/bounded, so this is conservative.
            return false;
        }
    }
}
