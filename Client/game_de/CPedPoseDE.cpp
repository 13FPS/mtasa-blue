/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Reuse classic position commit/teleport ordering without fake effects
 ****************************************************************************/
#include "CPedPoseDE.h"
#include <game/CEntityPlacementPolicy.h>
#include <stdexcept>
#include <string>
namespace mta::game_de
{
    void CPedPoseDE::Require(ClockResult result, const char* operation)
    {
        if (result != ClockResult::Ok)
            throw std::runtime_error(std::string("DE ped pose owner refused ") + operation + ": " + std::to_string(static_cast<unsigned>(result)));
    }
    void CPedPoseDE::SetPosition(float x, float y, float z)
    {
        MtaGame::CEntityPlacementPolicy::SetPosition(
            x, y, z,
            [this](const CVector& requested) -> const CVector*
            {
                Require(nativePlacementAccess.SetReferencedPedPosition(ReferenceToken(), requested), "SetPosition");
                // The qualified native transaction acknowledged these exact
                // bits. Use this current CPU value for the shared commit, not
                // a writable observation borrow or cached native matrix lease.
                return &requested;
            },
            [this](const CVector& committed) { RememberPhysicsPosition(committed); },
            []
            {
                // Actual native admission has proved this is a ped and refused
                // the special train model cases before any store. Object scale
                // and rail effects therefore are never emulated or omitted.
            });
    }
    void CPedPoseDE::SetPosition(CVector* position)
    {
        MtaGame::CEntityPlacementPolicy::SetPosition(position, [this](float x, float y, float z) { SetPosition(x, y, z); });
    }
    void CPedPoseDE::Teleport(float x, float y, float z)
    {
        MtaGame::CEntityPlacementPolicy::Teleport(
            x, y, z,
            [this]
            {
                NativePedTransformReader::Value current;
                Require(nativePoseObservationAccess.ReadReferencedPedTransform(ReferenceToken(), current), "Teleport matrix branch");
                return current.matrix != 0;
            },
            [this](const CVector& requested) { SetPosition(requested.fX, requested.fY, requested.fZ); },
            [this](const CVector& requested, bool)
            {
                // SetPosition and last-good CPU state already committed. This
                // same-callback completion must preserve that prefix and stop
                // its admitted owner on later refusal, never replay placement.
                Require(nativePlacementAccess.CompleteReferencedPedTeleport(ReferenceToken(), requested), "Teleport completion");
            });
    }
}
