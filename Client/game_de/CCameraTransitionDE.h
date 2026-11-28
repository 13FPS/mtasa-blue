/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Expose a single camera transition observation through the MTA SDK
 ****************************************************************************/
#pragma once
#include "CCameraTargetDE.h"
#include <game/CCameraTransitionFOV.h>

namespace mta::game_de
{
    struct CameraTransitionSample
    {
        std::uint8_t state;
        float        fov;
        CVector      source;
        CVector      target;
        CVector      up;
    };

    class CameraTransitionSource
    {
    public:
        // Capture all fields from the same camera update. Ok publishes the whole
        // sample; unavailable owners must report their status even if state is zero.
        virtual ClockResult ReadCameraTransition(CameraTransitionSample& sample) const noexcept = 0;

    protected:
        ~CameraTransitionSource() = default;
    };

    class CCameraTransitionDE : public CCameraTargetDE, public CCameraTransitionFOV
    {
    public:
        std::uint8_t GetTransitionState() const override;
        float        GetTransitionFOV() const override;
        bool         TryGetActiveTransitionFOV(float& fov) const override;
        bool         GetTransitionMatrix(CMatrix& matrix) const override;

    protected:
        // The supplying session owns the source for the lifetime of this camera.
        CCameraTransitionDE(CPoolsDE& pools, NativeGameTimingAccess& owner, const CameraTransitionSource& source)
            : CCameraTargetDE(pools, owner), transitionSource(source)
        {
        }

    private:
        CameraTransitionSample        ReadSample() const;
        const CameraTransitionSource& transitionSource;
    };
}
