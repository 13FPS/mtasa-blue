/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Exact existing weather getter dispatch with explicit owner refusal
 *****************************************************************************/
#include "CWeatherScalarsDE.h"
#include <stdexcept>
#include <string>

namespace mta::game_de
{
    float CWeatherScalarsDE::Read(NativeWeatherScalarProfile::Field field) const
    {
        float      value = 0;
        const auto result = access.ReadScalar(field, value);
        if (result != ClockResult::Ok)
            throw std::runtime_error("DE native weather scalar unavailable (status " + std::to_string(static_cast<std::uint32_t>(result)) + ")");
        return value;
    }
    float CWeatherScalarsDE::GetAmountOfRain()
    {
        return Read(NativeWeatherScalarProfile::Field::Rain);
    }
    float CWeatherScalarsDE::GetWetRoads() const
    {
        return Read(NativeWeatherScalarProfile::Field::WetRoads);
    }
    float CWeatherScalarsDE::GetFoggyness() const
    {
        return Read(NativeWeatherScalarProfile::Field::Foggyness);
    }
    float CWeatherScalarsDE::GetFog() const
    {
        return Read(NativeWeatherScalarProfile::Field::Fog);
    }
    float CWeatherScalarsDE::GetRainFog() const
    {
        return Read(NativeWeatherScalarProfile::Field::RainFog);
    }
    float CWeatherScalarsDE::GetWaterFog() const
    {
        return Read(NativeWeatherScalarProfile::Field::WaterFog);
    }
    float CWeatherScalarsDE::GetSandstorm() const
    {
        return Read(NativeWeatherScalarProfile::Field::Sandstorm);
    }
    float CWeatherScalarsDE::GetRainbow() const
    {
        return Read(NativeWeatherScalarProfile::Field::Rainbow);
    }
}
