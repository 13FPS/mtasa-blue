/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Re-admit current association-group row observations through one owner
 ****************************************************************************/
#pragma once
#include "NativeAnimationBlockAccess.h"
#include "NativeAnimationStaticAssociationAccess.h"
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
    struct NativeAnimationAssociationGroupBlock
    {
        std::uintptr_t               interfaceAddress = 0;
        NativeAnimationBlockIdentity identity{};
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
        // Null is an admitted current parent fact, not a missing provider.
        // Nonnull child identities remain coupled to that exact current row;
        // no block reference or child allocation is acquired by this read.
        virtual ClockResult ReadAnimationAssociationGroupBlock(const NativeAnimationAssociationGroupReferenceToken&,
                                                               NativeAnimationAssociationGroupBlock&) noexcept
        {
            return ClockResult::NotInitialized;
        }
        // Direct Group::GetAnimation has no manager Created/enum gate. It
        // resolves the raw modulo32 ID offset within the genuine child cookie.
        virtual ClockResult ResolveAnimationAssociationGroupStaticById(const NativeAnimationAssociationGroupReferenceToken&, std::uint32_t,
                                                                       NativeAnimationStaticAssociationReferenceToken&) noexcept
        {
            return ClockResult::NotInitialized;
        }
        // Source CopyAnimation returns a native dynamic interface, not a C++
        // facade. Its causal original-call record stays with the existing
        // private association owner; the Group wrapper never frees the copy.
        virtual ClockResult CopyAnimationAssociationGroup(const NativeAnimationAssociationGroupReferenceToken&, std::uint32_t, std::uintptr_t&) noexcept
        {
            return ClockResult::NotInitialized;
        }
        // Source4CDFB0 walks existing physical rows backwards and calls the
        // genuine static initializer only when their node pointer is zero.
        // Empty/all-valid groups never consume the supplied clump at all.
        virtual ClockResult InitializeEmptyAnimationAssociationGroup(const NativeAnimationAssociationGroupReferenceToken&, std::uintptr_t) noexcept
        {
            return ClockResult::NotInitialized;
        }
        // MTA's inline setter stores the full signed32 ID offset on the
        // current group. This is persistent scalar intent, not child-array
        // creation, ID reselection or a retained group allocation lease.
        virtual ClockResult SetAnimationAssociationGroupIdOffset(const NativeAnimationAssociationGroupReferenceToken&, std::int32_t) noexcept
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
