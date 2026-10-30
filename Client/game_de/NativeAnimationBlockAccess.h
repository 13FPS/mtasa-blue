/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Immediate borrowed animation-block facts without classic interfaces
 ****************************************************************************/
#pragma once
#include "NativePedPoolReader.h"
#include <array>
#include <cstddef>
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
        // Return only a freshly qualified hierarchy locator. The legacy SDK
        // pointer tag adds no classic layout or native lifetime guarantee.
        virtual ClockResult ReadAnimationBlockHierarchy(const NativeAnimationBlockIdentity&, std::size_t, std::uintptr_t&) noexcept
        {
            return ClockResult::NotInitialized;
        }
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
        // The integer manager channel preserves raw16 affinity, including
        // nonterminated names. It does not publish a name or block facade.
        virtual ClockResult ReadAnimationBlockCounterRefs(const NativeAnimationBlockIdentity&, std::uint16_t&) noexcept { return ClockResult::NotInitialized; }
        virtual ClockResult AddAnimationBlockCounterRef(const NativeAnimationBlockIdentity&) noexcept { return ClockResult::NotInitialized; }
        virtual ClockResult RemoveAnimationBlockRefWithoutDelete(const NativeAnimationBlockIdentity&) noexcept { return ClockResult::NotInitialized; }
        // Only the source-qualified empty domain may clear loaded/ref fields;
        // owning hierarchy or group children still requires native teardown.
        virtual ClockResult RemoveEmptyAnimationBlock(const NativeAnimationBlockIdentity&) noexcept { return ClockResult::NotInitialized; }
        // MTA's unsigned GetAnimation argument is a full hierarchy hash, not
        // an ordinal. A qualified complete miss returns Ok/null; owner/range
        // failure preserves output and never becomes a missing animation.
        virtual ClockResult FindAnimationBlockHierarchy(const NativeAnimationBlockIdentity&, std::uint32_t, std::uintptr_t&) noexcept
        {
            return ClockResult::NotInitialized;
        }
    };
}
