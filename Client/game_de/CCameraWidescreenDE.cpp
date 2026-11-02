/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Forward exact configured-mode requests without deferred SCM effects
 ****************************************************************************/
#include "CCameraWidescreenDE.h"
#include "NativeGameTimingAccess.h"
#include <stdexcept>
#include <string>
namespace mta::game_de
{
    CCameraWidescreenDE::CCameraWidescreenDE(CPoolsDE& pools, NativeGameTimingAccess& owner) : CCameraCamsDE(pools, owner), widescreenOwner(owner)
    {
    }
    void CCameraWidescreenDE::Require(ClockResult result, const char* operation)
    {
        if (result != ClockResult::Ok)
            throw std::runtime_error(std::string("DE camera owner refused ") + operation + ": " + std::to_string(static_cast<unsigned>(result)));
    }
    void CCameraWidescreenDE::SetWidescreen(bool enabled)
    {
        // Classic MTA assigns only the configured bool. The native owner
        // preserves DE's distinct pending-off state rather than emulating
        // the SCM command's deferred enable/disable pair or retrying a write.
        Require(widescreenOwner.WriteCameraWidescreen(enabled), "SetWidescreen");
    }
    bool CCameraWidescreenDE::GetWidescreen()
    {
        bool configured = false;
        // Missing authority must not masquerade as a successful disabled mode.
        // Return only the current qualified native observation, not last intent.
        Require(widescreenOwner.ReadCameraWidescreen(configured), "GetWidescreen");
        return configured;
    }
}
