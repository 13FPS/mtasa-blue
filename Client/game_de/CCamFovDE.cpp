/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Keep finite native FOV bits and MTA's successful nonfinite fallback
 ****************************************************************************/
#include "CCamFovDE.h"
#include <cmath>
#include <cstring>
#include <stdexcept>
#include <string>
namespace mta::game_de
{
    void CCamFovDE::SetFOV(float value)
    {
        // Match CCamSA's filter before consulting even a missing provider.
        // No clamp, default70 conversion, current-selector lookup or readback
        // belongs to this SDK policy; the owner admits the selected raw store.
        if (!std::isfinite(value) || value <= 0.0f || value >= 180.0f)
            return;
        std::uint32_t bits = 0;
        std::memcpy(&bits, &value, sizeof(bits));
        const auto result = cameraOwner.WriteCameraFov(logicalRow, bits);
        if (result != ClockResult::Ok)
            throw std::runtime_error("DE camera owner refused SetFOV: " + std::to_string(static_cast<unsigned>(result)));
    }
    float CCamFovDE::GetFOV() const
    {
        std::uint32_t bits = 0;
        const auto    result = cameraOwner.ReadCameraFov(logicalRow, bits);
        if (result != ClockResult::Ok)
            throw std::runtime_error("DE camera owner refused GetFOV: " + std::to_string(static_cast<unsigned>(result)));
        float value = 0;
        static_assert(sizeof(value) == sizeof(bits));
        std::memcpy(&value, &bits, sizeof(value));
        // CCamSA admits every finite value, including signed zero and values
        // outside the setter's range. Only a successfully observed nonfinite
        // FOV becomes 70; provider failure is never an absent-interface default.
        return std::isfinite(value) ? value : 70.0f;
    }
}
