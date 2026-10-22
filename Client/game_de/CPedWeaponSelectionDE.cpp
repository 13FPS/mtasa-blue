/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Preserve raw current-slot facts without creating an invalid C++ enum
 ****************************************************************************/
#include "CPedWeaponSelectionDE.h"
#include <stdexcept>
#include <string>
namespace mta::game_de
{
    eWeaponSlot CPedWeaponSelectionDE::GetCurrentWeaponSlot() const
    {
        std::uint32_t raw = 0;
        const auto    result = currentWeaponSelectionAccess.ReadPedCurrentWeaponSlot(ReferenceToken(), raw);
        if (result != ClockResult::Ok)
            throw std::runtime_error("DE ped weapon selection owner refused GetCurrentWeaponSlot: " + std::to_string(static_cast<unsigned>(result)));
        // eWeaponSlot is unfixed: its largest enumerator13 gives a legal C++
        // range0..15. Preserve13..15 observations, but never cast a larger raw
        // native byte into that enum or treat it as a safe array selector.
        static_assert(WEAPONSLOT_MAX == 13);
        if (raw > 15)
            throw std::runtime_error("DE ped GetCurrentWeaponSlot is outside the SDK enum representation");
        return static_cast<eWeaponSlot>(raw);
    }
}
