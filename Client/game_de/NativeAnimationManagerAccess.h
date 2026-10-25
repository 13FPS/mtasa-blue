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
        // Factories borrow current block identity, not a retained pointer or
        // a query-name copy. The runtime stamps its admitted CPU generation.
        virtual ClockResult ReadAnimationBlockIdentity(std::uint64_t, std::uint32_t, NativeAnimationBlockIdentity&) noexcept
        {
            return ClockResult::NotInitialized;
        }
        virtual ClockResult ReadAnimationBlockInterfaceIdentity(std::uint64_t, std::uintptr_t, NativeAnimationBlockIdentity&) noexcept
        {
            return ClockResult::NotInitialized;
        }
        // The CPU generation identifies the current borrowed runtime owner.
        // An absent name is Ok/-1; losing that owner must remain a refusal.
        virtual ClockResult ReadAnimationManagerCounts(std::uint64_t, NativeAnimationManagerCounts&) noexcept { return ClockResult::NotInitialized; }
        virtual ClockResult FindAnimationBlockIndex(std::uint64_t, const NativeAnimationBlockName&, std::int32_t&) noexcept
        {
            return ClockResult::NotInitialized;
        }
        // Integer reference operations use raw16 name affinity, not the
        // separate factory/name API's C-string admission contract.
        virtual ClockResult ReadAnimationBlockCounterIdentity(std::uint64_t, std::uint32_t, NativeAnimationBlockIdentity&) noexcept
        {
            return ClockResult::NotInitialized;
        }
        // The hierarchy tag is only a caller locator. The runtime qualifies
        // its current raw hash cell; no factory, string or FName is created.
        virtual ClockResult ReadAnimationHierarchyHash(std::uint64_t, std::uintptr_t, std::uint32_t&) noexcept { return ClockResult::NotInitialized; }
        // Association definitions have their own signed current count and
        // source name rows. Keep this channel separate from block/hierarchy
        // counts, association instances and a fabricated complete group SDK.
        virtual ClockResult ReadAnimationAssocDefinitionCount(std::uint64_t, std::int32_t&) noexcept { return ClockResult::NotInitialized; }
        // The owner returns the current exact native row's borrowed name;
        // full uint32 group IDs are qualified against its current definition
        // extent. A name borrow adds no lifetime or copied-string facade.
        virtual ClockResult ReadAnimationGroupName(std::uint64_t, std::uint32_t, std::uintptr_t&) noexcept { return ClockResult::NotInitialized; }
    };
}
