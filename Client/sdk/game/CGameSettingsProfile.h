/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Apply Core video preferences through the current game settings
 ****************************************************************************/
#pragma once
#include "CSettings.h"
#include <core/CCVarsInterface.h>
#include <algorithm>
#include <cmath>

namespace MtaGame
{
    class CGameSettingsProfile : public CGameSettings
    {
    public:
        bool GetVolumetricShadowsEnabledByVideoSetting() const noexcept override { return ReadShadow("volumetric_shadows"); }
        bool IsDynamicPedShadowsEnabledByVideoSetting() const noexcept override { return ReadShadow("dynamic_ped_shadows"); }

        bool ResetVolumetricShadows() noexcept override { return ResetShadow("volumetric_shadows", &CGameSettings::SetVolumetricShadowsEnabled); }
        bool ResetDynamicPedShadows() noexcept override { return ResetShadow("dynamic_ped_shadows", &CGameSettings::SetDynamicPedShadowsEnabled); }

        void UpdateFieldOfViewFromSettings() override
        {
            const float saved = ProfileValue("fov", 70.f);
            const float angle = std::isfinite(saved) ? std::clamp(saved, 70.f, 100.f) : 70.f;
            // Leave script overrides in the backend's setters and apply the
            // profile in the same player, vehicle, vehicle-maximum order.
            SetFieldOfViewPlayer(angle, false);
            SetFieldOfViewVehicle(angle, false);
            SetFieldOfViewVehicleMax(100.f, false);
        }

    protected:
        // Classic receives Core after game construction. Resolve the profile
        // when it is used, so a backend need not change its initialization order.
        virtual CCVarsInterface& Profile() const noexcept = 0;

        template <typename T>
        T ProfileValue(const char* name, T fallback) const
        {
            T value = fallback;
            return Profile().Get(name, value) ? value : fallback;
        }

    private:
        bool ReadShadow(const char* name) const noexcept
        {
            try
            {
                return ProfileValue(name, false);
            }
            catch (...)
            {
                return false;
            }
        }

        bool ResetShadow(const char* name, void (CGameSettings::*set)(bool)) noexcept
        {
            try
            {
                // An unavailable entry uses the ordinary Core default. A backend
                // refusal must reach the caller through the noexcept SDK result.
                (this->*set)(ProfileValue(name, false));
                return true;
            }
            catch (...)
            {
                return false;
            }
        }
    };
}
