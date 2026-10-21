/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Preserve genuine SDK weather override and reset dispatch
 *****************************************************************************/
#pragma once
#include "CWeatherScalarsDE.h"
#include "NativeWeatherOverrideAccess.h"

namespace mta::game_de
{
    // These methods preserve MTA's suppression/reset behavior: Reset restores
    // native writers, without rewinding the successful scalar value. Rain's
    // serializer escape remains unproved and its two void SDK methods stay pure.
    class CWeatherOverridesDE : public CWeatherScalarsDE
    {
    public:
        bool SetWetRoads(float amount) override;
        bool ResetWetRoads() override;
        bool SetFoggyness(float amount) override;
        bool ResetFoggyness() override;
        bool SetFog(float amount) override;
        bool ResetFog() override;
        bool SetRainFog(float amount) override;
        bool ResetRainFog() override;
        bool SetWaterFog(float amount) override;
        bool ResetWaterFog() override;
        bool SetSandstorm(float amount) override;
        bool ResetSandstorm() override;
        bool SetRainbow(float amount) override;
        bool ResetRainbow() override;

    protected:
        CWeatherOverridesDE(NativeWeatherTransitionAccess& transition, NativeWeatherScalarAccess& scalar, NativeWeatherOverrideAccess& overrides) noexcept
            : CWeatherScalarsDE(transition, scalar), native(overrides)
        {
        }

    private:
        NativeWeatherOverrideAccess& native;
    };
    static_assert(std::is_abstract_v<CWeatherOverridesDE> && std::is_base_of_v<CWeather, CWeatherOverridesDE>);
}
