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
        const auto read = [](const void* context, std::uintptr_t& raw) noexcept -> ClockResult
        {
            const auto& self = *static_cast<const CCamTargetDE*>(context);
            return self.targetOwner.ReadCameraRowTarget(self.RowBorrowToken(), raw);
        };
        CEntity* result = nullptr;
        // The shared itinerary preserves every existing provider call and
        // final CPU-token anchor while this adapter selects the held row only.
        Require(CPedPooledDE::ReadRegisteredCameraTarget(targetPools, targetOwner, read, this, result), "GetTargetEntity");
        return result;
    }
}
