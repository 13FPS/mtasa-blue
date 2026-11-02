/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Preserve null clearing and qualify controlled-ped setter intent
 ****************************************************************************/
#include "CCamTargetSetterDE.h"
#include "CPoolsDE.h"
#include "NativeGameTimingAccess.h"
#include <stdexcept>
#include <string>
namespace mta::game_de
{
    void CCamTargetSetterDE::Require(ClockResult result)
    {
        if (result != ClockResult::Ok)
            throw std::runtime_error("DE camera owner refused SetTargetEntity: " + std::to_string(static_cast<unsigned>(result)));
    }
    void CCamTargetSetterDE::SetTargetEntity(CEntity* entity)
    {
        Require(setterPools.HasNativeOwner(setterOwner) ? ClockResult::Ok : ClockResult::OwnerConflict);
        if (!entity)
        {
            // Classic MTA explicitly clears null. In DE the same request must
            // retire the engine-owned weak registration, not just zero +1A0.
            Require(setterOwner.SetCameraRowTarget(RowBorrowToken(), nullptr));
            return;
        }
        CPed*                   first = nullptr;
        CPed*                   second = nullptr;
        NativePedReferenceToken intent, confirmed;
        Require(setterPools.ReadBorrowedPedEntityReference(entity, first, intent));
        Require(setterPools.ReadBorrowedPedEntityReference(entity, second, confirmed));
        Require(first && first == second && intent.version && intent.version == confirmed.version && intent.ownerGeneration == confirmed.ownerGeneration &&
                        intent.slot == confirmed.slot
                    ? ClockResult::Ok
                    : ClockResult::BindingChanged);
        // The proved DE domain is null or an already registered same-owner
        // ped. Unknown/foreign/object/vehicle SDK pointers refuse here rather
        // than calling arbitrary GetInterface or treating a miss as a clear.
        // Runtime copies this CPU intent and re-resolves its full identity
        // through the same retained owner before any native reference effect.
        Require(setterOwner.SetCameraRowTarget(RowBorrowToken(), &intent));
    }
}
