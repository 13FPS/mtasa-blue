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
        // Counter ownership remains with the current native block. These
        // operations neither create a block nor own a classic raw interface.
        virtual ClockResult ReadAnimationBlockRefs(const NativeAnimationBlockIdentity&, std::uint16_t&) noexcept { return ClockResult::NotInitialized; }
        virtual ClockResult AddAnimationBlockRef(const NativeAnimationBlockIdentity&) noexcept { return ClockResult::NotInitialized; }
        // Borrow within the freshly admitted current callback.
        // This is the actual mutable image-bank name, not a stable CPU copy.
        virtual ClockResult ReadAnimationBlockName(const NativeAnimationBlockIdentity&, char*&) noexcept { return ClockResult::NotInitialized; }
        virtual ClockResult ReadAnimationBlock(const NativeAnimationBlockIdentity&, NativeAnimationBlockValues&) noexcept
        {
            return ClockResult::NotInitialized;
        }
    };
}
