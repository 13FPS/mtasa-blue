/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Internal engine-owner writes for the existing CGame timing methods
 *****************************************************************************/
#pragma once

#include <game/CGameClockService.h>
#include <enums/SystemState.h>
#include "NativeCheatProfile.h"
#include "CWeatherTransitionDE.h"
#include "CWeatherScalarsDE.h"
#include "NativeWeaponInfoWriteAccess.h"
#include "NativeWeaponInfoVectorAccess.h"
#include "NativeWeatherOverrideAccess.h"
#include "NativeStatsWriteAccess.h"
#include "NativeStartupCompletionAccess.h"
#include "NativePedPoolReader.h"
#include "NativePedTransformReader.h"
#include "NativePedTeleportAccess.h"
#include "NativePedVelocityAccess.h"
#include "NativePedReferenceToken.h"
#include "NativePedVitalsAccess.h"
#include "NativePedMetadataAccess.h"
#include "NativePedMutationAccess.h"
#include "NativePedObservationAccess.h"
#include "NativePedIntelligenceReader.h"
#include "NativeTaskPoolReader.h"
#include "NativeTaskRemovalAccess.h"
#include "NativeModelBankReader.h"
#include "NativeStreamingModelAccess.h"
#include "NativePedPhysicalScalarsAccess.h"
#include "NativePlayerInfoAccess.h"
#include "NativeWantedAccess.h"
#include "NativeRopesAccess.h"
#include <cstdint>

class CStats;
class CPlayerInfo;
class CRopes;

namespace mta::game_de
{
    // Read-only startup binding does not authorize mutations. The qualified
    // native owner implements these methods and retains restoration debt after
    // a partial write; this interface is not another exported service protocol.
    class NativeGameTimingAccess : public NativeWeatherTransitionAccess,
                                   public NativeWeatherScalarAccess,
                                   public NativeWeaponInfoWriteAccess,
                                   public NativeWeaponInfoVectorAccess,
                                   public NativeWeatherOverrideAccess,
                                   public NativeStatsWriteAccess,
                                   public NativePlayerInfoAccess,
                                   public NativeWantedAccess,
                                   public NativeRopesAccess,
                                   public NativePedTeleportAccess,
                                   public NativePedVelocityAccess,
                                   public NativePedVitalsAccess,
                                   public NativePedMetadataAccess,
                                   public NativePedMutationAccess,
                                   public NativePedObservationAccess,
                                   public NativePedPhysicalScalarsAccess,
                                   public NativeTaskRemovalAccess,
                                   public NativeStreamingModelAccess
    {
    public:
        virtual ClockResult SetTimeScale(float value) noexcept = 0;
        virtual ClockResult SetMinuteDuration(std::uint32_t value) noexcept = 0;
        virtual ClockResult Pause(bool value) noexcept = 0;
        // Startup state shares the same natural engine callback admission.
        // Older timing-only owners explicitly refuse this additional write;
        // recognizing a state value never grants an engine transition lease.
        virtual ClockResult SetSystemState(SystemState::Enum) noexcept { return ClockResult::NotInitialized; }
        // The four existing SDK cheat properties use this same synchronous
        // native owner. A timing-only implementation must refuse them without
        // publishing a cached value or pretending to reset the native registry.
        virtual ClockResult ReadCheatFlag(NativeCheatProfile::Flag, bool&) noexcept { return ClockResult::NotInitialized; }
        // Shared names/permissions do not manufacture native bindings. The
        // same engine owner qualifies each admitted read, or explicitly refuses.
        virtual ClockResult ReadWorldCheat(MtaGame::CWorldCheatPolicy::Id, bool&) noexcept { return ClockResult::NotInitialized; }
        virtual ClockResult SetCheatFlag(NativeCheatProfile::Flag, bool) noexcept { return ClockResult::NotInitialized; }
        // Shared MTA lookup can publish only a genuine bank wrapper qualified
        // by this native owner, never a partial provider or a cached fake row.
        virtual ClockResult GetWeaponInfoSlot(std::size_t, CWeaponInfo*&) noexcept { return ClockResult::NotInitialized; }
        // A complete SDK object borrows the independently qualified native
        // reader and this callback owner; publication grants no later lease.
        virtual ClockResult GetStats(CStats*&) noexcept { return ClockResult::NotInitialized; }
        virtual ClockResult GetPlayerInfo(CPlayerInfo*&) noexcept { return ClockResult::NotInitialized; }
        virtual ClockResult GetRopes(CRopes*&) noexcept { return ClockResult::NotInitialized; }
        // Publish the complete runtime-owned SDK service only after its native
        // owner qualifies it. A borrowed object never grants a later callback.
        virtual ClockResult GetWeather(CWeather*&) noexcept { return ClockResult::NotInitialized; }
        // Native script references remain generation-qualified pool handles.
        // These immediate facts are not SDK CPed wrappers or a pool lease.
        virtual ClockResult ResolvePedPoolHandle(std::uint32_t, NativePedPoolReader::Value&) noexcept { return ClockResult::NotInitialized; }
        virtual ClockResult RecheckPedPoolHandle(const NativePedPoolReader::Value&) noexcept { return ClockResult::NotInitialized; }
        // These are immediate facts and real vector borrows through the same
        // callback. Neither an SDK entity nor a persistent wrapper is created.
        virtual ClockResult ReadPedTransform(std::uint32_t, NativePedTransformReader::Value&) noexcept { return ClockResult::NotInitialized; }
        virtual ClockResult RecheckPedTransform(const NativePedTransformReader::Value&) noexcept { return ClockResult::NotInitialized; }
        virtual ClockResult BorrowPedPosition(std::uint32_t, CVector*&) noexcept { return ClockResult::NotInitialized; }
        // Stable native reference storage belongs to this runtime. Tokens
        // select a versioned cell; they never replace callback admission or
        // publish an incomplete SDK ped/pool object.
        virtual ClockResult AcquirePedReference(std::uint32_t, NativePedReferenceToken&) noexcept { return ClockResult::NotInitialized; }
        virtual ClockResult ResolvePedReference(const NativePedReferenceToken&, NativePedPoolReader::Value&) noexcept { return ClockResult::NotInitialized; }
        virtual ClockResult ReleasePedReference(const NativePedReferenceToken&) noexcept { return ClockResult::NotInitialized; }
        // Observe the actual intelligence/embedded-manager pair through the
        // same registered ped lifetime. Task words remain opaque addresses;
        // they cannot be treated as live SDK task wrappers or retained leases.
        virtual ClockResult ReadReferencedPedIntelligence(const NativePedReferenceToken&, NativePedIntelligenceReader::Value&) noexcept
        {
            return ClockResult::NotInitialized;
        }
        // A current task-bank observation is separate from a retained ped
        // reference and grants no persistent CTask identity or deletion lease.
        virtual ClockResult ReadTaskPoolEntry(std::uintptr_t, NativeTaskPoolReader::Value&) noexcept { return ClockResult::NotInitialized; }
        // Observe only the current bounded bank association. An opaque model
        // word supplies no loaded state, model object, RwObject or reference.
        virtual ClockResult ReadModelBankEntry(std::uint32_t, NativeModelBankReader::Value&) noexcept { return ClockResult::NotInitialized; }
        // Only an occupied loaded row can receive this interior native count
        // adjustment. It neither inserts an absent native row nor removes
        // the final one and cannot stand in for an SDK model lifetime.
        virtual ClockResult AdjustModelReference(std::uint32_t, int) noexcept { return ClockResult::NotInitialized; }
        // The real request belongs to a distinct admitted normal-frame scope;
        // a gameplay timer or earlier observation never grants lifecycle calls.
        virtual ClockResult StartGame() noexcept { return ClockResult::NotInitialized; }
        // Native pump facts are scoped to an actually entered request in this
        // owner generation. They do not certify player or network readiness.
        virtual ClockResult ReadStartupProgress(NativeStartupCompletionAccess::Value&) noexcept { return ClockResult::NotInitialized; }

    protected:
        ~NativeGameTimingAccess() = default;
    };
}
