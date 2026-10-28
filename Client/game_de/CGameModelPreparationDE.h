/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Assemble MTA's real special-character itinerary from native services
 ****************************************************************************/
#pragma once
#include <windows.h>
#include <SharedUtil.Defines.h>
#include <SharedUtil.IntTypes.h>
#include <CVector.h>
#include <game/CStreaming.h>
#include <game/CGameModelPreparationPolicy.h>
#include <game/CModelPedCreationPolicy.h>
#include "NativeModelClassificationAccess.h"
#include <stdexcept>
#include <string>

namespace mta::game_de
{
    class CGameModelPreparationDE final
    {
    public:
        explicit CGameModelPreparationDE(NativeModelClassificationAccess& owner) noexcept : owner(owner) {}

        template <class ResolveStreaming>
        void MakePedModel(std::uint32_t model, const char* name, ResolveStreaming&& resolveStreaming)
        {
            MtaGame::CModelPedCreationPolicy::MakePedModel(
                [this, model]
                {
                    const auto status = owner.CreateAndPublishPedModelMetadata(model);
                    // Preserve the SDK creator's explicit capability refusal.
                    // No request may substitute for missing native publication.
                    if (status == ClockResult::NotInitialized)
                        throw std::runtime_error("DE MakePedModel requires qualified native ped metadata creation and bank publication");
                    if (status != ClockResult::Ok)
                        throw std::runtime_error("DE model owner refused ped metadata creation/publication: " + std::to_string(static_cast<unsigned>(status)));
                },
                [&resolveStreaming, model, name]
                {
                    // Classic startup selects its existing numeric ModelInfo
                    // entry even when no native model exists. Creation needs
                    // this owner capability, not a complete CModelInfo factory.
                    auto* streaming = resolveStreaming();
                    if (!streaming)
                        throw std::runtime_error("DE model request requires the qualified streaming service");
                    streaming->RequestSpecialModel(model, name, 0);
                });
        }

        template <class ResolveStreaming>
        void SetupSpecialCharacters(ResolveStreaming&& resolveStreaming)
        {
            MtaGame::CGameModelPreparationPolicy::SetupSpecialCharacters([this, &resolveStreaming](std::uint32_t model, const char* name)
                                                                         { MakePedModel(model, name, resolveStreaming); });
        }

        void FixStartupModelCollision(std::uint32_t target, std::uint32_t source)
        {
            const auto status = owner.CopyStartupModelCollision(target, source);
            if (status == ClockResult::NotInitialized)
                throw std::runtime_error("DE startup collision repair requires the qualified native borrowed-pointer copy");
            if (status != ClockResult::Ok)
                throw std::runtime_error("DE model owner refused startup collision repair: " + std::to_string(static_cast<unsigned>(status)));
        }

        void SetupBrokenModels()
        {
            // Preserve MTA's two original conditional copies and accepted
            // prefix. A later failure cannot undo or replay an earlier copy.
            MtaGame::CGameModelPreparationPolicy::SetupBrokenModels([this](std::uint32_t target, std::uint32_t source)
                                                                    { FixStartupModelCollision(target, source); });
        }

    private:
        // No facade registry or native lifetime is manufactured here. The
        // existing owner's creation journal and streaming service retain it.
        NativeModelClassificationAccess& owner;
    };
}
