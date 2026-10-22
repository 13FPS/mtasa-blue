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
        // Reuse MTA's upgrade classification while the owner performs DE's
        // complete paired request; ordinary requests cannot substitute for it.
        if (MtaGame::CModelRequestPolicy::IsUpgradeModel(model))
            Require(access.RequestStreamingUpgrade(model, flags), "RequestModel vehicle upgrade");
        else
            Require(access.RequestStreamingModel(model, flags), "RequestModel");
    }

    bool CStreamingDE::HasModelLoaded(DWORD model)
    {
        if (MtaGame::CModelRequestPolicy::IsUpgradeModel(model))
        {
            // The selected row alone cannot prove its companion has loaded.
            bool loaded = false;
            Require(access.ReadStreamingUpgradeLoaded(model, loaded), "HasModelLoaded vehicle upgrade");
            return loaded;
        }
        NativeModelLoadedReader::Value value;
        Require(access.ReadStreamingModelLoaded(model, value), "HasModelLoaded");
        return value.streamedIn;
    }
}
