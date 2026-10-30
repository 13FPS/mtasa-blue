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
