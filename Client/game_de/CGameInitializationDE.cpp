/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Dispatch MTA's real later initialization itinerary once per call
 *****************************************************************************/
#include "CGameInitializationDE.h"
#include <game/CGarages.h>
#include <game/CGameInitializationPolicy.h>
#include <game/CGameModelPreparationPolicy.h>
#include <game/CModelInfo.h>
#include <stdexcept>

namespace mta::game_de
{
    void CGameInitializationDE::Initialize()
    {
        RequireInitializationReady();
        MtaGame::CGameInitializationPolicy::Initialize(
            [this]
            {
                auto* garages = GetGarages();
                // A missing required receiver cannot turn initialization into
                // success or skip MTA's exact default-garage schedule.
                if (!garages)
                    throw std::runtime_error("DE Initialize requires the qualified numeric garage service");
                garages->Initialize();
            },
            [this] { SetupSpecialCharacters(); }, [this] { SetupBrokenModels(); }, [this] { InitializeRenderer(); }, [this] { HideNativeMenu(); });
    }

    void CGameInitializationDE::SetupSpecialCharacters()
    {
        MtaGame::CGameModelPreparationPolicy::SetupSpecialCharacters([this](std::uint32_t model, const char* name)
                                                                     { ResolveStartupPedModel(model).MakePedModel(name); });
    }

    void CGameInitializationDE::SetupBrokenModels()
    {
        MtaGame::CGameModelPreparationPolicy::SetupBrokenModels([this](std::uint32_t target, std::uint32_t source)
                                                                { FixStartupModelCollision(target, source); });
    }
}
