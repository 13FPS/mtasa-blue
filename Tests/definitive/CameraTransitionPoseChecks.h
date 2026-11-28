/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Check the SDK camera basis and failure output on both game backends
 ****************************************************************************/
#pragma once
#include <game/CCameraTransitionPose.h>
#include <limits>
#include <stdexcept>

template <typename Read>
void CheckCameraTransitionPoses(Read read)
{
    const auto vectorsClose = [](CVector a, CVector b) { return (a - b).Length() < 0.00001f; };
    const auto pose = [&](CVector source, CVector target, CVector inputUp, CVector right, CVector front, CVector up)
    {
        CMatrix matrix;
        if (!read(source, target, inputUp, matrix) || matrix.vPos != source || !vectorsClose(matrix.vRight, right) || !vectorsClose(matrix.vFront, front) ||
            !vectorsClose(matrix.vUp, up))
            throw std::runtime_error("incorrect camera transition position, roll or handedness");
    };
    const CVector x(1, 0, 0), y(0, 1, 0), z(0, 0, 1), origin(10, -7, 3);
    pose(origin, origin + y, z, x, y, z);
    pose(origin, origin + y, x, -z, y, x);
    pose(origin, origin + y, -z, -x, y, -z);
    pose(origin, origin + z, x * 4 + z * 3, y, z, x);
    pose(origin, origin - z, x, -y, -z, x);
    pose(origin, origin, x, -z, y, x);
    pose(origin, origin + x, x, -y, x, z);
    pose(origin, origin + z, z, x, z, -y);
    pose(origin, origin - z, CVector(), x, -z, y);
    const float big = (std::numeric_limits<float>::max)();
    pose(-y * big, y * big, x * big, -z, y, x);

    const CMatrix sentinel(CVector(4, 5, 6), CVector(0.3f, 0.4f, 0.5f));
    const auto    unchanged = [&](CVector source, CVector target, CVector up)
    {
        CMatrix output = sentinel;
        if (read(source, target, up, output) || output.To4x4Array() != sentinel.To4x4Array())
            throw std::runtime_error("invalid transition vectors changed the output matrix");
    };
    const auto nan = std::numeric_limits<float>::quiet_NaN();
    const auto inf = std::numeric_limits<float>::infinity();
    unchanged(CVector(nan, 0, 0), y, z);
    unchanged(origin, CVector(0, inf, 0), z);
    unchanged(origin, y, CVector(0, 0, -inf));
}
