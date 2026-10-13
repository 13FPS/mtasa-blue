/*****************************************************************************
 *
 *  PROJECT:     Multi Theft Auto
 *  LICENSE:     See LICENSE in the top level directory
 *  PURPOSE:     Share existing ped error correction across game backends
 *
 *****************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include "SharedUtil.Defines.h"
#include "CVector.h"

namespace MtaPedInterpolation
{
    struct State
    {
        CVector       error;
        CVector       target;
        std::uint32_t startTime;
        std::uint32_t finishTime;
        float         lastAlpha;
    };

    struct Result
    {
        CVector position;
        State   state;
        bool    warped;
    };

    // Keep the correction independent of game memory so another backend can
    // reuse MTA's behavior instead of replacing it with a target-pose lerp.
    // Positions are relative to the caller's contact origin; engine movement
    // remains in currentPosition, while only unapplied error is compensated.
    // Timestamps retain the classic Windows client's 32-bit unsigned-long
    // contract. Conversion precedes subtraction, as in SharedUtil::Unlerp;
    // clock reversal and rollover are deliberately not reinterpreted here.
    inline Result Advance(const CVector& currentPosition, const State& state, std::uint32_t currentTime)
    {
        Result result{currentPosition, state, false};
        // This is the existing HasTargetPosition gate: zero means finished,
        // and subsequent engine motion must not receive old compensation.
        if (state.finishTime == 0)
            return result;

        const double from = state.startTime;
        const double positionTime = currentTime;
        const double to = state.finishTime;
        float        alpha = from == to ? 1.0f : static_cast<float>((positionTime - from) / (to - from));
        // Preserve SharedUtil::Clamp's comparison order and float arithmetic.
        alpha = alpha < 0.0f ? 0.0f : alpha > 1.0f ? 1.0f : alpha;

        const float currentAlpha = alpha - state.lastAlpha;
        result.state.lastAlpha = alpha;
        const CVector compensation = (state.error - CVector()) * currentAlpha + CVector();
        if (alpha == 1.0f)
            result.state.finishTime = 0;

        result.position = currentPosition + compensation;
        return result;
    }

    // An active target still needs this check when Advance has just completed
    // it. Staging allows the classic caller to publish completion before its
    // native velocity getter, which can repair physics, then use the live target.
    inline void ApplyWarpThreshold(Result& result, const CVector& currentPosition, float warpThreshold)
    {
        // The caller supplies its velocity/game-speed threshold. Retain the
        // negated <= comparison against the current pose so invalid distance
        // takes the existing target warp rather than propagating another NaN.
        if (!((currentPosition - result.state.target).Length() <= warpThreshold))
        {
            result.state.finishTime = 0;
            result.position = result.state.target;
            result.warped = true;
        }
    }

    // Consumers without a native-read boundary can use the same two stages in
    // one call. Preserve HasTargetPosition's gate rather than warping an idle ped.
    inline Result Update(const CVector& currentPosition, const State& state, std::uint32_t currentTime, float warpThreshold)
    {
        Result result = Advance(currentPosition, state, currentTime);
        if (state.finishTime != 0)
            ApplyWarpThreshold(result, currentPosition, warpThreshold);
        return result;
    }
}
