/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Share the row/main identity itinerary without aliasing their cells
 ****************************************************************************/
#include "CCameraTargetDE.h"
#include "CPedPooledDE.h"
#include "NativeGameTimingAccess.h"
#include <stdexcept>
#include <string>
namespace mta::game_de
{
    CCameraTargetDE::CCameraTargetDE(CPoolsDE& pools, NativeGameTimingAccess& owner) : CCameraWidescreenDE(pools, owner), targetPools(pools), targetOwner(owner)
    {
    }
    void CCameraTargetDE::Require(ClockResult result)
    {
        if (result != ClockResult::Ok)
            throw std::runtime_error("DE camera owner refused GetTargetEntity: " + std::to_string(static_cast<unsigned>(result)));
    }
    CEntity* CCameraTargetDE::GetTargetEntity()
    {
        const auto read = [](const void* context, std::uintptr_t& raw) noexcept -> ClockResult
        {
            const auto& self = *static_cast<const CCameraTargetDE*>(context);
            return self.targetOwner.ReadCameraMainTarget(raw);
        };
        CEntity* result = nullptr;
        // The typed reader selects only the main cell. Shared SDK identity
        // bookends must not replace it with an active row or acquire a row hold.
        Require(CPedPooledDE::ReadRegisteredCameraTarget(targetPools, targetOwner, read, this, result));
        return result;
    }
}
