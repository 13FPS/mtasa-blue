/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Borrowed animation-manager observations, separate from block refs
 ****************************************************************************/
#pragma once
#include "NativeAnimationBlockAccess.h"
#include <cstdint>
namespace mta::game_de
{
    struct NativeAnimationManagerCounts
    {
        std::int32_t blocks = 0;
        std::int32_t animations = 0;
    };
    class NativeAnimationManagerAccess
    {
    public:
        virtual ~NativeAnimationManagerAccess() = default;
        // The CPU generation identifies the current borrowed runtime owner.
        // An absent name is Ok/-1; losing that owner must remain a refusal.
        virtual ClockResult ReadAnimationManagerCounts(std::uint64_t, NativeAnimationManagerCounts&) noexcept { return ClockResult::NotInitialized; }
        virtual ClockResult FindAnimationBlockIndex(std::uint64_t, const NativeAnimationBlockName&, std::int32_t&) noexcept
        {
            return ClockResult::NotInitialized;
        }
    };
}
