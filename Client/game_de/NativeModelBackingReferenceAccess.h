/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Same-runtime durable backing-reference identities for model facades
 *****************************************************************************/
#pragma once
#include "NativeModelReferenceAcquisitionReceipt.h"
#include "NativeModelReferenceRelease.h"
#include <game/CGameClockService.h>

namespace mta::game_de
{
    struct NativeModelBackingReferenceToken
    {
        std::uint64_t ownerGeneration = 0, serial = 0;
        std::uint32_t modelIndex = 0;
    };
    enum class NativeModelBackingReferencePhase : unsigned char
    {
        Prepared,
        Bound,
        Acquiring,
        Held,
        FaultDebt,
        Releasing,
        Released
    };
    struct NativeModelBackingReferenceState
    {
        NativeModelBackingReferenceToken        reference{};
        NativeModelBackingReferencePhase        phase = NativeModelBackingReferencePhase::Prepared;
        NativeModelReferenceAcquisitionReceipt  acquisition{};
        ClockResult                             lastResult = ClockResult::NotInitialized;
        NativeModelReferenceRelease::Receipt    release{};
        std::uint32_t                           releaseSelectedCount = 0;
        NativeModelReferenceRelease::ZeroPolicy releasePolicy = NativeModelReferenceRelease::ZeroPolicy::KeepResident;
        bool                                    releaseFinal = false;
        std::uint64_t                           familySerial = 0, successorSerial = 0;
        bool                                    unmodifiedFamily = false;
    };
    class NativeModelBackingReferenceAccess
    {
    public:
        virtual ~NativeModelBackingReferenceAccess() = default;
        // Reserve and one-shot Bind own CPU identity only. No address, wrapper
        // count or missing implementation can mint a native reference lease.
        virtual ClockResult ReserveModelBackingReference(std::uint64_t, std::uint32_t, NativeModelBackingReferenceToken&) noexcept
        {
            return ClockResult::NotInitialized;
        }
        virtual ClockResult BindModelBackingReference(const NativeModelBackingReferenceToken&) noexcept { return ClockResult::NotInitialized; }
        virtual ClockResult AcquireModelBackingReference(const NativeModelBackingReferenceToken&) noexcept { return ClockResult::NotInitialized; }
        virtual ClockResult ClaimUnmodifiedModelFamily(const NativeModelBackingReferenceToken&) noexcept { return ClockResult::NotInitialized; }
        virtual ClockResult RenewModelBackingReferenceCycle(const NativeModelBackingReferenceToken&, NativeModelBackingReferenceToken&) noexcept
        {
            return ClockResult::NotInitialized;
        }
        // Explicit caller-selected zero policy does not prove custom-clump/COL
        // absence. SDK automatic removal remains mandatory and unsupported.
        virtual ClockResult ReleaseModelBackingReference(const NativeModelBackingReferenceToken&, NativeModelReferenceRelease::ZeroPolicy) noexcept
        {
            return ClockResult::NotInitialized;
        }
        virtual ClockResult ReadModelBackingReference(const NativeModelBackingReferenceToken&, NativeModelBackingReferenceState&) noexcept
        {
            return ClockResult::NotInitialized;
        }
        virtual bool HasPendingModelBackingReferences() const noexcept { return true; }
    };
}
