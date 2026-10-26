/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Preserve model request/refcount effects at their original boundaries
 *****************************************************************************/
#include "CModelInfoRequestDE.h"
#include <vector>
#include <game/CGame.h>
#include <game/CPools.h>
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

    void CModelInfoRequestDE::MakePedModel(const char* texture)
    {
        // The source publishes new ped metadata before resolving the
        // streaming service. Preserve that irreversible prefix if the
        // later request refuses; do not validate, retry or roll it back.
        CreateAndPublishNativePedModel(GetModel());
        RequireStreaming().RequestSpecialModel(GetModel(), texture, 0);
    }

    void CModelInfoRequestDE::CreateAndPublishNativePedModel(std::uint32_t)
    {
        // No existing read/reference provider proves creation or bank
        // publication. A concrete native owner must provide both before
        // this orchestration can issue even its first special request.
        throw std::runtime_error("DE MakePedModel requires qualified native ped metadata creation and bank publication");
    }

    void CModelInfoRequestDE::Request(EModelRequestType type, const char* tag)
    {
        MtaGame::CModelRequestPolicy::Request(
            type, tag, [this] { return IsLoaded(); }, [this](EModelRequestType dependencyType, const char* dependencyTag)
            { RequestModelDependencies(dependencyType, dependencyTag); }, [this](std::uint32_t flags) { RequireStreaming().RequestModel(GetModel(), flags); },
            [this](bool onlyPriority, const char* loadTag) { RequireStreaming().LoadAllRequestedModels(onlyPriority, loadTag); },
            [this](bool loaded, int attempts, const char* reportTag) { ReportBlockingModelLoad(loaded, attempts, reportTag); });
    }

    void CModelInfoRequestDE::RequestModelDependencies(EModelRequestType type, const char* tag)
    {
        // Keep the bike-animation workaround before the selected request.
        // The engine's internal request recursion uses different flags and
        // timing, so it cannot replace MTA's dependency type/tag policy.
        if (IsVehicle())
        {
            const auto animation = GetAnimFileIndex();
            if (animation != 0xffffffffu)
            {
                const auto animationId = animation + game.GetBaseIDforIFP();
                auto*      dependency = game.GetModelInfo(animationId);
                if (!dependency)
                {
                    if (animationId != game.GetBaseIDforIFP() + 139u)
                        ReportMissingAnimationModel(animationId);
                }
                else if (!dependency->IsLoaded())
                {
#ifdef MTA_DEBUG
                    ReportRequestingAnimationModel(animationId);
#endif
                    dependency->Request(type, tag);
                }
            }
        }
        else
        {
            const auto modelIndex = GetModel();
            if (modelIndex <= 288u && modelIndex != 7u)
            {
                auto* skin = game.GetModelInfo(7);
                if (!skin)
                    throw std::runtime_error("DE model dependencies require the real skin7 facade");
                if (!skin->IsLoaded())
                {
                    // The source resolves skin7 again at its request boundary.
                    // Do not turn a missing required receiver into readiness.
                    skin = game.GetModelInfo(7);
                    if (!skin)
                        throw std::runtime_error("DE model dependencies lost the real skin7 facade");
                    skin->Request(type, "Model 7");
                }
            }
        }
    }

    void CModelInfoRequestDE::ReportMissingAnimationModel(std::uint32_t)
    {
        throw std::runtime_error("DE model dependencies require the actual Core model-no-animation report505");
    }
#ifdef MTA_DEBUG
    void CModelInfoRequestDE::ReportRequestingAnimationModel(std::uint32_t)
    {
        throw std::runtime_error("DE model dependencies require the actual requesting-animation debug report");
    }
#endif

    bool CModelInfoRequestDE::DoIsLoaded()
    {
        // Keep the actual streaming operation: an upgrade can require its
        // paired resource, so selected-row flags cannot replace this query.
        if (!RequireStreaming().HasModelLoaded(GetModel()))
            return false;
        if (GetModel() >= game.GetBaseIDforTXD())
            return true;
        // Classic refreshes a cached interface even while unloaded. This DE
        // facade deliberately has no cache; these two current opaque borrows
        // preserve the loaded-DFF decisions without dereferencing SA layouts.
        return GetInterface() != nullptr && GetRwObject() != nullptr;
    }

    bool CModelInfoRequestDE::IsValid()
    {
        const auto modelIndex = GetModel();
        // Preserve MTA's fixed TXD-slot policy before dynamic file bounds.
        // The actual pool decides occupancy; an archive/type/loaded query
        // would change valid-but-unloaded and allocated custom-TXD behavior.
        constexpr std::uint32_t dffLimit = 20000, txdLimit = 25000;
        if (modelIndex >= dffLimit && modelIndex < txdLimit)
        {
            auto* pools = game.GetPools();
            if (!pools)
                throw std::runtime_error("DE model IsValid requires the qualified TXD pool service");
            return !pools->GetTxdPool().IsFreeTextureDictonarySlot(modelIndex - dffLimit);
        }
        if (modelIndex >= game.GetBaseIDforTXD() && modelIndex < game.GetCountOfAllFileIDs())
            return true;
        // This DE facade has no cached classic interface layout. Its current
        // opaque bank observation preserves the source's nonnull decision.
        return GetInterface() != nullptr;
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
