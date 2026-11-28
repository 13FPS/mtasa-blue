/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Apply the SDK transition semantics to a coherent camera sample
 ****************************************************************************/
#include "CCameraTransitionDE.h"
#include <game/CCameraTransitionPose.h>
#include <stdexcept>
#include <string>

namespace mta::game_de
{
    CameraTransitionSample CCameraTransitionDE::ReadSample() const
    {
        CameraTransitionSample sample{};
        const auto             result = transitionSource.ReadCameraTransition(sample);
        if (result != ClockResult::Ok)
            throw std::runtime_error("DE camera owner refused ReadCameraTransition: " + std::to_string(static_cast<unsigned>(result)));
        return sample;
    }

    std::uint8_t CCameraTransitionDE::GetTransitionState() const
    {
        return ReadSample().state;
    }

    float CCameraTransitionDE::GetTransitionFOV() const
    {
        return ReadSample().fov;
    }

    bool CCameraTransitionDE::TryGetActiveTransitionFOV(float& fov) const
    {
        const auto sample = ReadSample();
        if (sample.state == 0)
            return false;
        fov = sample.fov;
        return true;
    }

    bool CCameraTransitionDE::GetTransitionMatrix(CMatrix& matrix) const
    {
        const auto sample = ReadSample();
        return sample.state != 0 && MtaGame::BuildCameraTransitionMatrix(sample.source, sample.target, sample.up, matrix);
    }
}
