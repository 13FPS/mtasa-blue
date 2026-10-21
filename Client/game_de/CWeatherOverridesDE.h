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
    // native writers, without rewinding the successful scalar value. Rain also
    // owns its direction-correct serializer gate and complete borrow interval.
    class CWeatherOverridesDE : public CWeatherScalarsDE
    {
    public:
        CWeatherOverridesDE(NativeWeatherTransitionAccess& transition, NativeWeatherScalarAccess& scalar, NativeWeatherOverrideAccess& overrides) noexcept
            : CWeatherScalarsDE(transition, scalar), native(overrides)
        {
        }
        void SetAmountOfRain(float amount) override;
        void ResetAmountOfRain() override;
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

    private:
        NativeWeatherOverrideAccess& native;
    };
    static_assert(!std::is_abstract_v<CWeatherOverridesDE> && std::is_base_of_v<CWeather, CWeatherOverridesDE>);
}
