/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Preserve raw CPed vitals while reporting real owner refusal
 ****************************************************************************/
#include "CPedVitalsDE.h"
#include <game/CEntityFirePolicy.h>
#include <stdexcept>
#include <string>

namespace mta::game_de
{
    bool CPedVitalsDE::IsOnFire()
    {
        // This is the real classic nonconst CEntity fallback, independent of
        // native state. The later const CPed query still requires its actual
        // token owner and can refuse; it is never synthesized from false here.
        return MtaGame::CEntityFirePolicy::IsOnFire();
    }
    void CPedVitalsDE::Require(ClockResult result, const char* operation)
    {
        if (result != ClockResult::Ok)
            throw std::runtime_error(std::string("DE ped vitals owner refused ") + operation + ": " + std::to_string(static_cast<unsigned>(result)));
    }
    float CPedVitalsDE::Read(NativePedVitalField field, const char* operation) const
    {
        float value = 0;
        // A failed owner operation must not become plausible SDK health0.
        // The checked owner preserves the raw float representation; this
        // forwarding layer adds no clamp, resurrection or cached fallback.
        Require(native.ReadPedVital(reference, field, value), operation);
        return value;
    }
    float CPedVitalsDE::GetHealth() const
    {
        return Read(NativePedVitalField::Health, "GetHealth");
    }
    void CPedVitalsDE::SetHealth(float health)
    {
        Require(native.SetPedVital(reference, NativePedVitalField::Health, health), "SetHealth");
    }
    float CPedVitalsDE::GetArmor() const
    {
        return Read(NativePedVitalField::Armor, "GetArmor");
    }
    void CPedVitalsDE::SetArmor(float armor)
    {
        Require(native.SetPedVital(reference, NativePedVitalField::Armor, armor), "SetArmor");
    }
}
