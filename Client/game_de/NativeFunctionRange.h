/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Bound every AMD64 unwind fragment of an owned native callback
 *****************************************************************************/
#pragma once
#include <cstddef>
#include <cstdint>

namespace mta::game_de
{
    struct NativeFunctionRange
    {
        std::uintptr_t begin = 0, end = 0;
        bool           Contains(std::uintptr_t address) const noexcept { return begin && begin < end && address >= begin && address < end; }
    };

    // Resolve before parking peers. Quiescence may then use Contains without
    // allocations, module lookup or unwind reads while those peers are held.
    bool FindNativeFunctionRange(const void* function, NativeFunctionRange& output) noexcept;

    namespace NativeFunctionRangeDetail
    {
        struct Entry
        {
            std::uint32_t begin = 0, end = 0, unwind = 0;
        };
        // Decoder seam for fixture-owned PE images; it confers no native hook
        // authority and must still match the supplied containing table entry.
        bool Decode(const void* image, std::size_t mappedBytes, std::uint32_t functionRva, const Entry& containing, NativeFunctionRange& output) noexcept;
    }
}
