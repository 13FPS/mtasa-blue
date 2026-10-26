/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Preserve model event/report IDs through the real Core service
 ****************************************************************************/
#include "CModelInfoDiagnosticsDE.h"
#include <functional>
#include <memory>
// The standalone backend has no classic PCH. Core's real SDK includes use
// SharedUtil strings, buffers, diagnostic enums and the existing Win32 types.
// These unchanged Core SDK headers normally inherit the classic PCH's
// warning policy. Contain that compatibility boundary here; adapter and test
// bodies retain the caller's strict warning level after the include returns.
#ifdef _MSC_VER
    #pragma warning(push, 0)
#endif
#include <SharedUtil.h>
#include <core/CCoreInterface.h>
#ifdef _MSC_VER
    #pragma warning(pop)
#endif
#include <string>

namespace mta::game_de
{
    void CModelInfoDiagnosticsDE::ReportMissingAnimationModel(std::uint32_t animation)
    {
        const auto body = std::to_string(GetModel()) + " (" + std::to_string(animation) + ")";
        core.LogEvent(505, "Model no anim", "", body.c_str());
    }
#ifdef MTA_DEBUG
    void CModelInfoDiagnosticsDE::ReportRequestingAnimationModel(std::uint32_t animation)
    {
        // Reuse Core's unfiltered diagnostic event instead of introducing a
        // second debugger/logger implementation in this backend module.
        const auto body = "Requesting anim file " + std::to_string(animation) + " for model " + std::to_string(GetModel());
        core.LogEvent(0, "Models", "", body.c_str());
    }
#endif
    void CModelInfoDiagnosticsDE::ReportBlockingModelLoad(bool loaded, int attempts, const char* tag)
    {
        const auto body = std::to_string(GetModel()) + " (" + (tag ? tag : "") + ")";
        if (!loaded)
        {
            // Core adds the report before its filtered event. Its existing
            // format is "context - body", so this preserves the diagnostic
            // meaning/IDs rather than classic's byte-identical ": " prefix.
            core.LogEvent(641, "Blocking load fail", "Blocking load fail", body.c_str(), 6641);
            dassert(0);
        }
        else
        {
            const auto success = body + " (Took " + std::to_string(attempts) + " attempts)";
            core.LogEvent(642, "Blocking load", "Blocking load", success.c_str(), 6642);
        }
    }
}
