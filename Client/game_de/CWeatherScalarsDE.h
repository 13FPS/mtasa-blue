/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Genuine native reads behind existing MTA weather getter methods
 *****************************************************************************/
#pragma once
#include "CWeatherTransitionDE.h"
#include "NativeWeatherScalarProfile.h"

namespace mta::game_de
{
    class NativeWeatherScalarAccess
    {
    public:
        // The existing synchronous native owner supplies authority. Refusal is
        // an explicit result until it qualifies these additional producers.
        virtual ClockResult ReadScalar(NativeWeatherScalarProfile::Field, float&) noexcept { return ClockResult::NotInitialized; }

    protected:
        ~NativeWeatherScalarAccess() = default;
    };
    class CWeatherScalarsDE : public CWeatherTransitionDE
    {
    public:
        float GetAmountOfRain() override;
        float GetWetRoads() const override;
        float GetFoggyness() const override;
        float GetFog() const override;
        float GetRainFog() const override;
        float GetWaterFog() const override;
        float GetSandstorm() const override;
        float GetRainbow() const override;

    protected:
        CWeatherScalarsDE(NativeWeatherTransitionAccess& transition, NativeWeatherScalarAccess& scalar) noexcept
            : CWeatherTransitionDE(transition), access(scalar)
        {
        }

    private:
        float                      Read(NativeWeatherScalarProfile::Field) const;
        NativeWeatherScalarAccess& access;
    };
    // Set/Reset require suppression of native writers, which is a separate
    // unfinished service. They remain pure rather than fake cached overrides.
    static_assert(std::is_abstract_v<CWeatherScalarsDE> && std::is_base_of_v<CWeather, CWeatherScalarsDE>);
}
