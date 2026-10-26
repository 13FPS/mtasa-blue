/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Explicit private dynamic-association construction and lifetime owner
 ****************************************************************************/
#pragma once
#include <game/CGameClockService.h>
#include "NativeAnimationHierarchyAccess.h"
#include <cstdint>
namespace mta::game_de
{
    struct NativeAnimationAssociationReferenceToken
    {
        std::uint64_t ownerGeneration = 0, serial = 0;
    };
    struct NativeAnimationAssociationValues
    {
        std::uintptr_t interfaceAddress = 0;
        std::int16_t   animId = 0, animGroup = 0, flags = 0;
        std::uint32_t  blendAmountBits = 0, speedBits = 0;
        std::uint32_t  currentTimeBits = 0;
    };
    class NativeAnimationAssociationAccess
    {
    public:
        // Creation issues unconstructed native104 storage. Only the private
        // owner may transition it after a genuine constructor ACK; a raw tag
        // or a buffer of the same size is never adopted as that authority.
        virtual ClockResult CreatePrivateAnimationAssociation(std::uint64_t, NativeAnimationAssociationReferenceToken&) noexcept
        {
            return ClockResult::NotInitialized;
        }
        virtual ClockResult ConstructPrivateAnimationAssociationFromStatic(const NativeAnimationAssociationReferenceToken&, std::uintptr_t,
                                                                           std::uintptr_t&) noexcept
        {
            return ClockResult::NotInitialized;
        }
        virtual ClockResult ReadPrivateAnimationAssociation(const NativeAnimationAssociationReferenceToken&, NativeAnimationAssociationValues&) noexcept
        {
            return ClockResult::NotInitialized;
        }
        virtual ClockResult SetPrivateAnimationAssociationBlendAmount(const NativeAnimationAssociationReferenceToken&, std::uint32_t) noexcept
        {
            return ClockResult::NotInitialized;
        }
        virtual ClockResult SetPrivateAnimationAssociationCurrentSpeed(const NativeAnimationAssociationReferenceToken&, std::uint32_t) noexcept
        {
            return ClockResult::NotInitialized;
        }
        virtual ClockResult SetPrivateAnimationAssociationAnimID(const NativeAnimationAssociationReferenceToken&, std::int16_t) noexcept
        {
            return ClockResult::NotInitialized;
        }
        virtual ClockResult SetPrivateAnimationAssociationAnimGroup(const NativeAnimationAssociationReferenceToken&, std::int16_t) noexcept
        {
            return ClockResult::NotInitialized;
        }
        // The actual owned reference journal, not this source flag, decides
        // lifetime. The supported writer refuses changing bit4000 rather
        // than masking it or inventing a reference transfer.
        virtual ClockResult SetPrivateAnimationAssociationFlags(const NativeAnimationAssociationReferenceToken&, std::int16_t) noexcept
        {
            return ClockResult::NotInitialized;
        }
        virtual ClockResult RetirePrivateAnimationAssociation(const NativeAnimationAssociationReferenceToken&) noexcept { return ClockResult::NotInitialized; }
        virtual bool        HasPendingPrivateAnimationAssociations() const noexcept { return true; }
        // Length is a separate jointly qualified parent/BANK-H observation.
        // Mutable hierarchy time is neither an immutable constructor identity
        // nor a prerequisite for unrelated association scalar reads.
        virtual ClockResult ReadPrivateAnimationAssociationLength(const NativeAnimationAssociationReferenceToken&, std::uint32_t&) noexcept
        {
            return ClockResult::NotInitialized;
        }

        // The parent and its immutable bank-H origin are observed together;
        // a separate raw read/resolve could publish a torn hierarchy borrow.
        virtual ClockResult ResolvePrivateAnimationAssociationHierarchy(const NativeAnimationAssociationReferenceToken&,
                                                                        NativeAnimationHierarchyReferenceToken&) noexcept
        {
            return ClockResult::NotInitialized;
        }
        // Restriction only removes admitted node sequence pointers. The owner
        // retains each accepted null separately from its constructor baseline;
        // raw nulls never grant authority to change, free or adopt node storage.
        virtual ClockResult RestrictPrivateAnimationAssociationToBones(const NativeAnimationAssociationReferenceToken&, std::uint64_t) noexcept
        {
            return ClockResult::NotInitialized;
        }
        virtual ClockResult RestrictPrivateAnimationAssociationToBonesOf(const NativeAnimationAssociationReferenceToken&, std::uintptr_t) noexcept
        {
            return ClockResult::NotInitialized;
        }

        // Copy raw source progress once. This same owner must qualify the
        // current parent/H time, multiply one float and run the genuine native
        // helper; separate public duration and mutation calls would tear it.
        virtual ClockResult SetPrivateAnimationAssociationCurrentProgress(const NativeAnimationAssociationReferenceToken&, std::uint32_t) noexcept
        {
            return ClockResult::NotInitialized;
        }

        // Public node release leaves the receiver's pointer/count stale.
        // Only the owner may retain the allocation's returned/unknown ACK,
        // reject later node consumers, and clear the pointer during retirement.
        virtual ClockResult FreePrivateAnimationAssociationNodeArray(const NativeAnimationAssociationReferenceToken&) noexcept
        {
            return ClockResult::NotInitialized;
        }

    protected:
        ~NativeAnimationAssociationAccess() = default;
    };
}
