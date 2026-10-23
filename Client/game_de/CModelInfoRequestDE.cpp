/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Preserve model request/refcount effects at their original boundaries
 *****************************************************************************/
#include "CModelInfoRequestDE.h"
#include <game/CGame.h>
#include <game/CStreaming.h>
#include <game/CModelRequestPolicy.h>
#include <stdexcept>

namespace mta::game_de
{
    CStreaming& CModelInfoRequestDE::RequireStreaming()
    {
        // Resolve the actual borrowed game's receiver at the original effect
        // boundary. A missing service cannot turn a request/load into success.
        auto* streaming = game.GetStreaming();
        if (!streaming)
            throw std::runtime_error("DE model request requires the qualified streaming service");
        return *streaming;
    }

    void CModelInfoRequestDE::Request(EModelRequestType type, const char* tag)
    {
        MtaGame::CModelRequestPolicy::Request(
            type, tag, [this] { return IsLoaded(); }, [this](EModelRequestType dependencyType, const char* dependencyTag)
            { RequestModelDependencies(dependencyType, dependencyTag); }, [this](std::uint32_t flags) { RequireStreaming().RequestModel(GetModel(), flags); },
            [this](bool onlyPriority, const char* loadTag) { RequireStreaming().LoadAllRequestedModels(onlyPriority, loadTag); },
            [this](bool loaded, int attempts, const char* reportTag) { ReportBlockingModelLoad(loaded, attempts, reportTag); });
    }

    bool CModelInfoRequestDE::IsLoaded()
    {
        return MtaGame::CModelRequestPolicy::IsLoaded(
            references, pendingInterfaceReference, [this] { return DoIsLoaded(); }, [this] { IncrementNativeModelInterfaceReference(); });
    }

    void CModelInfoRequestDE::ModelAddRef(EModelRequestType type, const char* tag)
    {
        MtaGame::CModelRequestPolicy::AddRef(
            references, pendingInterfaceReference, type, tag, [this] { return IsLoaded(); }, [this] { return game.IsASyncLoadingEnabled(); },
            [this](EModelRequestType requestedType, const char* requestTag) { Request(requestedType, requestTag); },
            [this] { IncrementNativeModelInterfaceReference(); });
    }

    void CModelInfoRequestDE::RemoveRef()
    {
        // Keep pending cancellation, non-CJ gating and CPU decrement ordering
        // in the same actual policy. A later refusal retains that CPU prefix;
        // this layer neither retries nor guesses a native final-release route.
        MtaGame::CModelRequestPolicy::RemoveRef(
            references, pendingInterfaceReference, GetModel(), [this] { return IsLoaded(); }, [this] { RemoveNativeModelInterfaceReference(); });
    }

    int CModelInfoRequestDE::GetRefCount()
    {
        // This is the original wrapper counter, not any native map/interface
        // count. Match the actual classic DWORD-to-int SDK conversion.
        return static_cast<int>(references);
    }
}
