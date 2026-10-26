/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Explicit private dynamic-association construction and lifetime owner
 ****************************************************************************/
#pragma once
#include <game/CGameClockService.h>
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

    protected:
        ~NativeAnimationAssociationAccess() = default;
    };
}
