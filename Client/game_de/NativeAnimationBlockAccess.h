/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Immediate borrowed animation-block facts without classic interfaces
 ****************************************************************************/
#pragma once
#include "NativePedPoolReader.h"
#include <array>
#include <cstdint>
namespace mta::game_de
{
    using NativeAnimationBlockName = std::array<unsigned char, 16>;
    struct NativeAnimationBlockIdentity
    {
        std::uint32_t            index = 0;
        NativeAnimationBlockName name{};
        // The runtime checks this CPU callback-generation affinity before and
        // after the native read. It does not retain or lease a native block.
        std::uint64_t ownerGeneration = 0;
    };
    struct NativeAnimationBlockValues
    {
        std::uint32_t index = 0;
        std::int32_t  idOffset = 0;
        std::uint32_t animationCount = 0;
        std::uint8_t  loaded = 0;
    };
    class NativeAnimationBlockAccess
    {
    public:
        virtual ~NativeAnimationBlockAccess() = default;
        virtual ClockResult ReadAnimationBlock(const NativeAnimationBlockIdentity&, NativeAnimationBlockValues&) noexcept
        {
            return ClockResult::NotInitialized;
        }
    };
}
