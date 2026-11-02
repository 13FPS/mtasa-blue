/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Define the existing Reset itinerary once for either actual game base
 ****************************************************************************/
#pragma once
#include <SharedUtil.Defines.h>
// CHud declares unused by-value colour getters using the shared colour type.
// Reset needs only its identity here, avoiding the unrelated utility bodies.
namespace SharedUtil
{
    class SColor;
}
using SharedUtil::SColor;
#include <game/CGameResetPolicy.h>
#include <game/CFireManager.h>
// The camera SDK includes CEntity's legacy unscoped eBone declaration. Keep
// the existing MSVC SDK contract local, as the ped facade does, without
// changing the shared enum ABI or relaxing warnings for the reset code.
#if defined(_MSC_VER)
    #pragma warning(push)
    #pragma warning(disable : 4471)
#endif
#include <game/CCamera.h>
#if defined(_MSC_VER)
    #pragma warning(pop)
#endif
#include <game/CHud.h>
#include <game/CPools.h>
#include <stdexcept>
#include <string>
namespace mta::game_de
{
    namespace GameResetDetail
    {
        template <class Service>
        Service& RequireService(Service* service, const char* name)
        {
            if (!service)
                throw std::runtime_error(std::string("DE Reset requires the qualified ") + name + " service");
            return *service;
        }
    }
    template <class GameBase>
    void CGameResetMixinDE<GameBase>::Reset()
    {
        if (this->GetSystemState() != SystemState::GS_PLAYING_GAME)
            return;
        RequireResetReady();
        MtaGame::CGameResetPolicy::Reset(
            true, [this] { GameResetDetail::RequireService(this->GetFireManager(), "fire manager").ExtinguishAllFires(); },
            [this] { GameResetDetail::RequireService(this->GetCamera(), "camera").Restore(); },
            [this] { GameResetDetail::RequireService(this->GetCamera(), "camera").SetFadeColor(0, 0, 0); },
            [this] { GameResetDetail::RequireService(this->GetCamera(), "camera").Fade(0, FADE_OUT); }, [this] { this->Pause(false); },
            [this] { GameResetDetail::RequireService(this->GetHud(), "HUD").Disable(false); },
            [this] { GameResetDetail::RequireService(this->GetHud(), "HUD").SetComponentVisible(HUD_ALL, true); }, [this] { ResetVehicleModelDummies(); },
            [this] { RestoreObjectModelGroups(); }, [this] { RestoreDefaultObjectGroupProperties(); }, [this] { ResetVehicleModelWheelSizes(); },
            [this] { ResetModelTextureDictionaries(); }, [this] { this->RestoreGameWorld(); },
            [this] { return GameResetDetail::RequireService(this->GetPools(), "pools").GetBuildingsPool().GetSize(); },
            [this]
            {
                const auto capacity = GetResetBuildingPoolCapacity();
                if (capacity <= 0)
                    throw std::runtime_error("DE Reset requires a qualified positive default building capacity");
                return capacity;
            },
            [this](int capacity) { return this->SetBuildingPoolSize(static_cast<std::size_t>(capacity)); });
        // There is no local complete/failed flag: every explicit Reset may run
        // again after its actual owner is ready. Entered failures retain their
        // effect prefix; neither this layer nor the shared policy retries it.
    }
}
