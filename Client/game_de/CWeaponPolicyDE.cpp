/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Preserve actual CWeapon borrowed-ped current-slot delegation
 *****************************************************************************/
#include "CWeaponPolicyDE.h"
// The existing SDK forward-declares eBone before CPed defines it. Keep that
// legacy MSVC header contract local, as the existing CPed DE layers do.
#if defined(_MSC_VER)
    #pragma warning(push)
    #pragma warning(disable : 4471)
#endif
#include <game/CPed.h>
#if defined(_MSC_VER)
    #pragma warning(pop)
#endif

namespace mta::game_de
{
    void CWeaponPolicyDE::SetAsCurrentWeapon()
    {
        // The actual classic wrapper delegates to its borrowed ped. That
        // ped owns native selection admission; a second raw-slot write or
        // cached selected-slot state here would bypass the existing contract.
        if (owner)
            owner->SetCurrentWeaponSlot(weaponSlot);
    }
}
