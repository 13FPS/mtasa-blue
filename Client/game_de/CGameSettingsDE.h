/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Use the DE session's Core profile for video settings
 ****************************************************************************/
#pragma once
#include <game/CGameSettingsProfile.h>

namespace mta::game_de
{
    class CGameSettingsDE : public MtaGame::CGameSettingsProfile
    {
    protected:
        // Core keeps its profile alive through the game settings lifetime.
        explicit CGameSettingsDE(CCVarsInterface& profile) noexcept : profile(profile) {}

    private:
        CCVarsInterface& Profile() const noexcept override { return profile; }
        CCVarsInterface& profile;
    };
}
