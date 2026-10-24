/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Bounded raw sequence names and source-owned classic hash policy
 ****************************************************************************/
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

namespace mta::game_de
{
    using NativeAnimationSequenceName = std::array<char, 64>;
    namespace NativeAnimationSequenceNamePolicy
    {
        inline bool Length(const NativeAnimationSequenceName& name, std::size_t& output) noexcept
        {
            for (std::size_t index = 0; index != name.size(); ++index)
                if (!name[index])
                {
                    output = index;
                    return true;
                }
            return false;
        }

        inline bool Copy(const char* input, NativeAnimationSequenceName& output) noexcept
        {
            if (!input)
                return false;
            // Copy before the first owner callback, preserving case and high
            // bytes. The zeroed unused transport bytes are not native stores:
            // the physical operation writes only the observed length + NUL.
            NativeAnimationSequenceName next{};
            for (std::size_t index = 0; index != next.size(); ++index)
            {
                next[index] = input[index];
                if (!next[index])
                {
                    output = next;
                    return true;
                }
            }
            return false;
        }

        // Classic53CF30 normalizes ASCII a-z only. Keep that compatibility
        // value separate from DE's raw physical name and CRT locale behavior.
        bool Hash(const NativeAnimationSequenceName&, std::uint32_t&) noexcept;
    }
}
