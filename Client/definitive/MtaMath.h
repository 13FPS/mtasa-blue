/*****************************************************************************
 *
 *  PROJECT:     Multi Theft Auto - Definitive Edition experiment
 *  LICENSE:     See LICENSE in the top level directory
 *  PURPOSE:     Isolate reusable MTA vector math from the classic GTA client
 *
 *****************************************************************************/

#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>

// CVector4D needs FLOAT_EPSILON, and CVector needs size_t. These small headers
// let the x64 experiment reuse MTA math without loading its game or Lua modules.
// The standalone target must not define legacy WIN32: SharedUtil.Defines.h
// changes assert under that macro, which would obscure independent validation.
#include "../../Shared/sdk/SharedUtil.Defines.h"
#include "../../Shared/sdk/CVector.h"

namespace deonline
{
    inline CVector InterpolatePosition(const CVector& from, const CVector& to, float alpha)
    {
        // Snapshots can arrive beyond the display window. Clamping prevents
        // that timing jitter from extrapolating a remote player indefinitely.
        if (std::isnan(alpha))
            alpha = 0.0f;
        return from + (to - from) * std::clamp(alpha, 0.0f, 1.0f);
    }
}
