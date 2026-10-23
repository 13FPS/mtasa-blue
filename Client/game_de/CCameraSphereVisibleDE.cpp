/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Fix caller sphere input before the current native camera owner runs
 *****************************************************************************/
#include "CCameraSphereVisibleDE.h"
#include <stdexcept>
#include <string>

namespace mta::game_de
{
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
