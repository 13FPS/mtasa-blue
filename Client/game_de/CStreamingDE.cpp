/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Preserve real MTA streaming dispatch and explicit native refusals
 ****************************************************************************/
#include "CStreamingDE.h"
#include <game/CModelRequestPolicy.h>
#include <stdexcept>
#include <string>

namespace mta::game_de
{
    void CStreamingDE::Require(ClockResult result, const char* operation)
    {
        if (result != ClockResult::Ok)
            throw std::runtime_error(std::string("DE streaming owner refused ") + operation + ": " + std::to_string(static_cast<unsigned>(result)));
    }

    void CStreamingDE::RequestModel(DWORD model, DWORD flags)
    {
        // MTA routes vehicle upgrades through a separate native operation.
        // Until that DE path is qualified, do not disguise a normal request
        // as the upgrade transaction or silently change its flags/model key.
        if (MtaGame::CModelRequestPolicy::IsUpgradeModel(model))
            Require(ClockResult::UnsupportedBuild, "RequestModel vehicle upgrade");
        Require(access.RequestStreamingModel(model, flags), "RequestModel");
    }

    bool CStreamingDE::HasModelLoaded(DWORD model)
    {
        if (MtaGame::CModelRequestPolicy::IsUpgradeModel(model))
            Require(ClockResult::UnsupportedBuild, "HasModelLoaded vehicle upgrade");
        NativeModelLoadedReader::Value value;
        Require(access.ReadStreamingModelLoaded(model, value), "HasModelLoaded");
        return value.streamedIn;
    }
}
