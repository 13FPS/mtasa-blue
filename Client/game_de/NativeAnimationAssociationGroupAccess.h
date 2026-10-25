/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Re-admit current association-group row observations through one owner
 ****************************************************************************/
#pragma once
#include <game/CGameClockService.h>
#include <cstdint>
namespace mta::game_de
{
    struct NativeAnimationAssociationGroupReferenceToken
    {
        std::uint64_t  ownerGeneration = 0;
        std::uint32_t  index = 0;
        std::uintptr_t locator = 0;
    };
    struct NativeAnimationAssociationGroupValues
    {
        std::uintptr_t interfaceAddress = 0;
        std::int32_t   numAnimations = 0;
        std::int32_t   groupId = 0;
    };
    class NativeAnimationAssociationGroupAccess
    {
    public:
        virtual ~NativeAnimationAssociationGroupAccess() = default;
        // Tokens are immediate current-array/generation affinity, not a
        // retained row lease, allocation registry or same-generation ABA ID.
        virtual ClockResult ResolveAnimationAssociationGroup(std::uint64_t, std::uint32_t, NativeAnimationAssociationGroupReferenceToken&) noexcept
        {
            return ClockResult::NotInitialized;
        }
        // Scalar/source-tag reads do not acquire a child array or require a
        // loaded block, a C-string name, or a nonzero animation count.
        virtual ClockResult ReadAnimationAssociationGroup(const NativeAnimationAssociationGroupReferenceToken&, NativeAnimationAssociationGroupValues&) noexcept
        {
            return ClockResult::NotInitialized;
        }
        // Actual MTA Group::IsLoaded is block?block.loaded:false. Only this
        // channel qualifies a nonnull current block; it is not Has45B130.
        virtual ClockResult ReadAnimationAssociationGroupLoaded(const NativeAnimationAssociationGroupReferenceToken&, bool&) noexcept
        {
            return ClockResult::NotInitialized;
        }
    };
}
