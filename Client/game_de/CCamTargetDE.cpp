/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Preserve qualified null and refuse targets without a real SDK mapping
 ****************************************************************************/
#include "CCamTargetDE.h"
#include "CPedPooledDE.h"
#include "CPoolsDE.h"
#include "NativeGameTimingAccess.h"
#include <stdexcept>
#include <string>
namespace mta::game_de
{
    void CCamTargetDE::Require(ClockResult result, const char* operation)
    {
        if (result != ClockResult::Ok)
            throw std::runtime_error(std::string("DE camera owner refused ") + operation + ": " + std::to_string(static_cast<unsigned>(result)));
    }
    NativeGameTimingAccess& CCamTargetDE::RequireOwner(CPoolsDE& pools, NativeGameTimingAccess& owner)
    {
        // Run this CPU check while evaluating the base argument, before the
        // base acquires a hold. No later constructor failure may strand a
        // token on the engine callback instead of its retirement producer.
        Require(pools.HasNativeOwner(owner) ? ClockResult::Ok : ClockResult::OwnerConflict, "CCamTargetDE constructor");
        return owner;
    }
    CCamTargetDE::CCamTargetDE(std::uint32_t row, CPoolsDE& pools, NativeGameTimingAccess& owner)
        : CCamRowDE(row, RequireOwner(pools, owner)), targetPools(pools), targetOwner(owner)
    {
    }
    CEntity* CCamTargetDE::GetTargetEntity() const
    {
        Require(targetPools.HasNativeOwner(targetOwner) ? ClockResult::Ok : ClockResult::OwnerConflict, "GetTargetEntity");
        std::uintptr_t target = 0, confirmed = 0;
        Require(targetOwner.ReadCameraRowTarget(RowBorrowToken(), target), "GetTargetEntity");
        if (!target)
        {
            Require(targetOwner.ReadCameraRowTarget(RowBorrowToken(), confirmed), "GetTargetEntity");
            Require(confirmed ? ClockResult::BindingChanged : ClockResult::Ok, "GetTargetEntity");
            return nullptr;
        }
        SClientEntity<CPed>* first = nullptr;
        Require(targetPools.ReadPedByAddress(target, first), "GetTargetEntity");
        // A successful miss in a ped-only registry cannot stand in for the
        // general object/vehicle/ped/building GetEntity lookup. Never turn
        // an unknown nonzero native target into a plausible null SDK result.
        Require(first && first->pEntity ? ClockResult::Ok : ClockResult::NotInitialized, "GetTargetEntity");
        CPed* const                ped = first->pEntity;
        NativePedPoolReader::Value before, after;
        NativePedReferenceToken    beforeReference, afterReference;
        Require(CPedPooledDE::ReadRegisteredIdentity(targetPools, targetOwner, ped, before, beforeReference), "GetTargetEntity");
        Require(before.nativePed == target ? ClockResult::Ok : ClockResult::BindingChanged, "GetTargetEntity");
        Require(targetOwner.ReadCameraRowTarget(RowBorrowToken(), confirmed), "GetTargetEntity");
        Require(confirmed == target ? ClockResult::Ok : ClockResult::BindingChanged, "GetTargetEntity");
        SClientEntity<CPed>* current = nullptr;
        Require(targetPools.ReadPedByAddress(confirmed, current), "GetTargetEntity");
        Require(current == first && current && current->pEntity == ped ? ClockResult::Ok : ClockResult::BindingChanged, "GetTargetEntity");
        Require(CPedPooledDE::ReadRegisteredIdentity(targetPools, targetOwner, ped, after, afterReference), "GetTargetEntity");
        Require(CPedPooledDE::SameIdentity(before, after) && CPedPooledDE::SameReference(beforeReference, afterReference) ? ClockResult::Ok
                                                                                                                          : ClockResult::BindingChanged,
                "GetTargetEntity");
        Require(targetOwner.ReadCameraRowTarget(RowBorrowToken(), confirmed), "GetTargetEntity");
        Require(confirmed == target ? ClockResult::Ok : ClockResult::BindingChanged, "GetTargetEntity");
        NativePedReferenceToken reference;
        Require(targetPools.ReadBorrowedPedReference(ped, reference), "GetTargetEntity");
        // A withdrawn and replaced registration may reuse both the SDK object
        // and native value. Keep the CPU token anchored across the whole read.
        Require(CPedPooledDE::SameReference(beforeReference, reference) ? ClockResult::Ok : ClockResult::BindingChanged, "GetTargetEntity");
        // Ordinary conversion adjusts the actual virtual base. A native
        // address, StoredPointer or cast of a classic pool row is never used
        // as a public CEntity object, and no reference or wrapper is acquired.
        return static_cast<CEntity*>(ped);
    }
}
