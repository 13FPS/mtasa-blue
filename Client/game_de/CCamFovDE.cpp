/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Preserve MTA's scalar camera observations and ordered angle policy
 ****************************************************************************/
#include "CCamFovDE.h"
#include <cmath>
#include <cstring>
#include <stdexcept>
#include <string>
namespace mta::game_de
{
    namespace
    {
        // Keep CCamSA's float formula, including overflow for very large
        // finite inputs. Re-filtering its result would change that policy.
        float WrapCameraAngle(float angle) noexcept
        {
            constexpr float pi = 3.14159265358979323846f;
            constexpr float twoPi = 6.28318530717958647692f;
            angle -= twoPi * std::floor((angle + pi) / twoPi);
            if (angle <= -pi)
                angle += twoPi;
            else if (angle > pi)
                angle -= twoPi;
            return angle;
        }
        std::uint32_t CameraAngleBits(float value) noexcept
        {
            std::uint32_t bits = 0;
            std::memcpy(&bits, &value, sizeof(bits));
            return bits;
        }
        float CameraAngleValue(std::uint32_t bits) noexcept
        {
            float value = 0;
            std::memcpy(&value, &bits, sizeof(value));
            return value;
        }
    }
    unsigned int CCamFovDE::GetMode() const
    {
        std::uint16_t raw = 0;
        const auto    result = cameraOwner.ReadCameraMode(logicalRow, raw);
        if (result != ClockResult::Ok)
            throw std::runtime_error("DE camera owner refused GetMode: " + std::to_string(static_cast<unsigned>(result)));
        std::int16_t signedMode = 0;
        std::memcpy(&signedMode, &raw, sizeof(raw));
        // The actual classic field is signed short, then cast to unsigned
        // int. Native MOVSX independently confirms that signed WORD role.
        return static_cast<unsigned int>(signedMode);
    }
    void CCamFovDE::GetDirection(float& horizontal, float& vertical)
    {
        std::uint32_t originalH = 0, originalV = 0;
        auto          result = cameraOwner.ReadCameraDirection(logicalRow, originalH, originalV);
        if (result != ClockResult::Ok)
            throw std::runtime_error("DE camera owner refused GetDirection: " + std::to_string(static_cast<unsigned>(result)));
        const float sourceH = CameraAngleValue(originalH), sourceV = CameraAngleValue(originalV);
        const float nextH = std::isfinite(sourceH) ? WrapCameraAngle(sourceH) : 0.0f;
        const float nextV = std::isfinite(sourceV) ? WrapCameraAngle(sourceV) : 0.0f;
        if (!std::isfinite(sourceH) || !std::isfinite(sourceV))
        {
            // Compute once before the conditional native repair, exactly as
            // CCamSA does. A changed pair cannot receive a stale getter fix.
            result = cameraOwner.ConditionalRepairCameraDirection(logicalRow, originalH, originalV, CameraAngleBits(nextH), CameraAngleBits(nextV));
            if (result != ClockResult::Ok)
                throw std::runtime_error("DE camera owner refused GetDirection repair: " + std::to_string(static_cast<unsigned>(result)));
        }
        // Ordered assignments preserve the actual alias contract: when both
        // output references identify one float, vertical is the final value.
        horizontal = nextH;
        vertical = nextV;
    }
    void CCamFovDE::SetDirection(float horizontal, float vertical)
    {
        if (!std::isfinite(horizontal) || !std::isfinite(vertical))
            return;
        const float nextH = WrapCameraAngle(horizontal), nextV = WrapCameraAngle(vertical);
        const auto  result = cameraOwner.WriteCameraDirection(logicalRow, CameraAngleBits(nextH), CameraAngleBits(nextV));
        if (result != ClockResult::Ok)
            throw std::runtime_error("DE camera owner refused SetDirection: " + std::to_string(static_cast<unsigned>(result)));
    }

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
