/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Build an MTA camera pose from the interpolated look-at and up vectors
 ****************************************************************************/
#pragma once
#include <SharedUtil.Defines.h>
#include <CMatrix.h>
#include <cmath>

namespace MtaGame
{
    inline bool BuildCameraTransitionMatrix(const CVector& source, const CVector& target, const CVector& up, CMatrix& output) noexcept
    {
        if (!source.IsValid() || !target.IsValid() || !up.IsValid())
            return false;

        // Finite float positions can overflow even during subtraction. Normalize
        // in double before returning to the unit vectors used by CMatrix.
        const auto direction = [](double x, double y, double z, const CVector& fallback)
        {
            const double length = std::sqrt(x * x + y * y + z * z);
            return length <= FLOAT_EPSILON ? fallback : CVector(static_cast<float>(x / length), static_cast<float>(y / length), static_cast<float>(z / length));
        };
        const CVector front = direction(static_cast<double>(target.fX) - source.fX, static_cast<double>(target.fY) - source.fY,
                                        static_cast<double>(target.fZ) - source.fZ, CVector(0, 1, 0));
        CVector       cameraUp = direction(up.fX, up.fY, up.fZ, CVector());
        CVector       right = front;
        right.CrossProduct(&cameraUp);
        if (right.Normalize() == 0.0f)
        {
            // A parallel or missing up vector gives no roll. Use the level
            // horizon, and a horizontal reference when looking along world Z.
            cameraUp = std::abs(front.fZ) > 0.9999f ? CVector(0, -std::copysign(1.0f, front.fZ), 0) : CVector(0, 0, 1);
            right = front;
            right.CrossProduct(&cameraUp);
            right.Normalize();
        }
        cameraUp = right;
        cameraUp.CrossProduct(&front);
        cameraUp.Normalize();
        output.vPos = source;
        output.vFront = front;
        output.vRight = right;
        output.vUp = cameraUp;
        return true;
    }
}
