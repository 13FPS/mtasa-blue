/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Borrow one admitted runtime's completed constructor services
 *****************************************************************************/
#pragma once

#include <game/CWeaponInfoIndexPolicy.h>
#include <array>
#include <cstdint>

class CClock;
class CStats;
class CPlayerInfo;
class CWanted;
class CRopes;
class CWeather;
class CWeaponStatManager;

namespace mta::game_de
{
    // This is a value-only identity bundle, not ownership or native admission.
    // Every borrowed SDK object and this game's access owner must outlive the
    // game, which must retire before the captured generation ends. A repeated
    // address in a later runtime generation cannot renew the original borrow.
    struct NativeGameConstructorServices
    {
        std::uint64_t                                                        generation = 0;
        CClock*                                                              clock = nullptr;
        CStats*                                                              stats = nullptr;
        CPlayerInfo*                                                         playerInfo = nullptr;
        CWanted*                                                             wanted = nullptr;
        CRopes*                                                              ropes = nullptr;
        CWeather*                                                            weather = nullptr;
        CWeaponStatManager*                                                  weaponManager = nullptr;
        std::array<CWeaponInfo*, MtaGame::CWeaponInfoIndexPolicy::SlotCount> weapons{};

        bool Complete() const noexcept
        {
            if (!generation || !clock || !stats || !playerInfo || !wanted || !ropes || !weather || !weaponManager)
                return false;
            for (auto* weapon : weapons)
                if (!weapon)
                    return false;
            return true;
        }

        bool Same(const NativeGameConstructorServices& other) const noexcept
        {
            return generation == other.generation && clock == other.clock && stats == other.stats && playerInfo == other.playerInfo && wanted == other.wanted &&
                   ropes == other.ropes && weather == other.weather && weaponManager == other.weaponManager && weapons == other.weapons;
        }
    };
}
