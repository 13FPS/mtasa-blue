/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Route actual camera operations through the current native owner
 *****************************************************************************/
#include "CCameraSphereVisibleDE.h"
#include <stdexcept>
#include <string>

namespace mta::game_de
{
    BYTE CCameraSphereVisibleDE::GetActiveCam()
    {
        std::uint8_t value = 0;
        const auto   result = cameraOwner.ReadCameraActiveIndex(value);
        if (result != ClockResult::Ok)
            throw std::runtime_error("DE camera owner refused GetActiveCam: " + std::to_string(static_cast<unsigned>(result)));
        // The actual MTA getter returns the raw selector BYTE. It does not
        // clamp the observation to the three separately constructed Cam rows.
        return value;
    }
    void CCameraSphereVisibleDE::Restore()
    {
        // Ordinary MTA Restore calls its original once and has no SDK extras.
        // The retained owner holds any entered uncertainty; this method must
        // propagate refusal without jumping, repairing fields or retrying.
        const auto result = cameraOwner.RestoreCamera();
        if (result != ClockResult::Ok)
            throw std::runtime_error("DE camera owner refused Restore: " + std::to_string(static_cast<unsigned>(result)));
    }
    bool CCameraSphereVisibleDE::IsSphereVisible(CVector* center, float radius) const
    {
        if (!center)
            throw std::invalid_argument("DE camera IsSphereVisible requires a center");
        // The caller may mutate its source during an owner callback. Forward
        // owned XYZ and the original radius without a CPU frustum, clamp or
        // ped/model prerequisite; only the genuine native boolean is returned.
        const CVector selected = *center;
        bool          visible = false;
        const auto    result = cameraOwner.ReadCameraSphereVisible(selected, radius, visible);
        if (result != ClockResult::Ok)
            throw std::runtime_error("DE camera owner refused IsSphereVisible: " + std::to_string(static_cast<unsigned>(result)));
        return visible;
    }
}
