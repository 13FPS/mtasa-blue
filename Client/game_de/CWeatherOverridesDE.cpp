/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Route existing MTA weather methods through qualified writer ownership
 *****************************************************************************/
#include "CWeatherOverridesDE.h"

namespace mta::game_de
{
    bool CWeatherOverridesDE::SetWetRoads(float amount)
    {
        return native.SetWeatherScalarOverride(NativeWeatherScalarProfile::Field::WetRoads, amount) == ClockResult::Ok;
    }
    bool CWeatherOverridesDE::ResetWetRoads()
    {
        return native.ResetWeatherScalarOverride(NativeWeatherScalarProfile::Field::WetRoads) == ClockResult::Ok;
    }
    bool CWeatherOverridesDE::SetFoggyness(float amount)
    {
        return native.SetWeatherScalarOverride(NativeWeatherScalarProfile::Field::Foggyness, amount) == ClockResult::Ok;
    }
    bool CWeatherOverridesDE::ResetFoggyness()
    {
        return native.ResetWeatherScalarOverride(NativeWeatherScalarProfile::Field::Foggyness) == ClockResult::Ok;
    }
    bool CWeatherOverridesDE::SetFog(float amount)
    {
        return native.SetWeatherScalarOverride(NativeWeatherScalarProfile::Field::Fog, amount) == ClockResult::Ok;
    }
    bool CWeatherOverridesDE::ResetFog()
    {
        return native.ResetWeatherScalarOverride(NativeWeatherScalarProfile::Field::Fog) == ClockResult::Ok;
    }
    bool CWeatherOverridesDE::SetRainFog(float amount)
    {
        return native.SetWeatherScalarOverride(NativeWeatherScalarProfile::Field::RainFog, amount) == ClockResult::Ok;
    }
    bool CWeatherOverridesDE::ResetRainFog()
    {
        return native.ResetWeatherScalarOverride(NativeWeatherScalarProfile::Field::RainFog) == ClockResult::Ok;
    }
    bool CWeatherOverridesDE::SetWaterFog(float amount)
    {
        return native.SetWeatherScalarOverride(NativeWeatherScalarProfile::Field::WaterFog, amount) == ClockResult::Ok;
    }
    bool CWeatherOverridesDE::ResetWaterFog()
    {
        return native.ResetWeatherScalarOverride(NativeWeatherScalarProfile::Field::WaterFog) == ClockResult::Ok;
    }
    bool CWeatherOverridesDE::SetSandstorm(float amount)
    {
        return native.SetWeatherScalarOverride(NativeWeatherScalarProfile::Field::Sandstorm, amount) == ClockResult::Ok;
    }
    bool CWeatherOverridesDE::ResetSandstorm()
    {
        return native.ResetWeatherScalarOverride(NativeWeatherScalarProfile::Field::Sandstorm) == ClockResult::Ok;
    }
    bool CWeatherOverridesDE::SetRainbow(float amount)
    {
        return native.SetWeatherScalarOverride(NativeWeatherScalarProfile::Field::Rainbow, amount) == ClockResult::Ok;
    }
    bool CWeatherOverridesDE::ResetRainbow()
    {
        return native.ResetWeatherScalarOverride(NativeWeatherScalarProfile::Field::Rainbow) == ClockResult::Ok;
    }
}
