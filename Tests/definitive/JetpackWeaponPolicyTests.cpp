/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Verify shared MTA jetpack policy independently of native hooks
 *****************************************************************************/
#include <windows.h>
#include <cstdint>
#include <game/CWeaponInfo.h>
#include <CJetpackWeaponPolicy.h>

#include <array>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <new>
#include <type_traits>

namespace
{
    using Policy = MtaGame::CJetpackWeaponPolicy<WEAPONTYPE_LAST_WEAPONTYPE, WEAPONTYPE_BRASSKNUCKLE>;
    unsigned checks = 0;

    void Check(bool value, const char* description)
    {
        ++checks;
        if (!value)
        {
            std::fprintf(stderr, "FAIL: %s\n", description);
            std::exit(EXIT_FAILURE);
        }
    }

    void CheckEnabledSet(const Policy& policy, const std::array<int, 4>& expected, unsigned expectedCount, const char* description)
    {
        for (int weapon = WEAPONTYPE_UNARMED; weapon <= WEAPONTYPE_LAST_WEAPONTYPE; ++weapon)
        {
            bool enabled = false;
            for (unsigned index = 0; index < expectedCount; ++index)
                enabled = enabled || weapon == expected[index];
            Check(policy.Get(weapon) == enabled, description);
        }
    }
}

int main()
{
    static_assert(std::is_standard_layout_v<Policy> && std::is_trivially_copyable_v<Policy>);
    static_assert(sizeof(Policy) == sizeof(bool) * WEAPONTYPE_LAST_WEAPONTYPE && alignof(Policy) == alignof(bool));

    // Construct over nonzero storage to prove this is explicit initialization,
    // not an accidental result of zeroed heap or static-storage allocation.
    alignas(Policy) std::array<unsigned char, sizeof(Policy)> storage;
    storage.fill(0xa5);
    Policy* first = new (storage.data()) Policy;
    Policy  second;
    for (int weapon = WEAPONTYPE_UNARMED; weapon <= WEAPONTYPE_LAST_WEAPONTYPE; ++weapon)
        Check(!first->Get(weapon) && !second.Get(weapon), "unconfigured permissions are disabled in independently initialized instances");

    const std::array<int, 8> invalid = {
        (std::numeric_limits<int>::min)(), -1, WEAPONTYPE_UNARMED, WEAPONTYPE_LAST_WEAPONTYPE, WEAPONTYPE_ARMOUR, WEAPONTYPE_INVALID, 256,
        (std::numeric_limits<int>::max)()};
    for (const int weapon : invalid)
    {
        first->Set(weapon, true);
        Check(!first->Get(weapon), "invalid and out-of-range weapon permissions always read false");
    }
    for (int weapon = WEAPONTYPE_BRASSKNUCKLE; weapon < WEAPONTYPE_LAST_WEAPONTYPE; ++weapon)
        Check(!first->Get(weapon), "rejected writes do not alter any valid weapon permission");

    for (int weapon = WEAPONTYPE_BRASSKNUCKLE; weapon < WEAPONTYPE_LAST_WEAPONTYPE; ++weapon)
    {
        first->Set(weapon, true);
        Check(first->Get(weapon), "all and only classic accepted weapon IDs can be enabled");
        Check(!second.Get(weapon), "one instance cannot mutate another table");
    }
    for (int weapon = WEAPONTYPE_LAST_WEAPONTYPE - 1; weapon >= WEAPONTYPE_BRASSKNUCKLE; --weapon)
    {
        first->Set(weapon, false);
        Check(!first->Get(weapon), "valid permissions can be cleared in reverse order");
        if (weapon > WEAPONTYPE_BRASSKNUCKLE)
            Check(first->Get(weapon - 1), "clearing one row preserves its enabled neighbour");
    }

    // Reproduce the existing server constructor's explicit allowed set. The
    // policy supplies no weapon defaults of its own beyond safe initial false.
    const std::array<int, 4> serverDefaults = {WEAPONTYPE_MICRO_UZI, WEAPONTYPE_TEC9, WEAPONTYPE_PISTOL, WEAPONTYPE_SAWNOFF_SHOTGUN};
    for (const int weapon : serverDefaults)
        first->Set(weapon, true);
    CheckEnabledSet(*first, serverDefaults, 4, "the existing four server startup selections remain representable");

    // Existing reset traffic clears every ID and re-enables the three classic
    // jetpack defaults; the sawnoff permission must not survive that traffic.
    for (int weapon = WEAPONTYPE_UNARMED; weapon < WEAPONTYPE_LAST_WEAPONTYPE; ++weapon)
        first->Set(weapon, false);
    for (unsigned index = 0; index < 3; ++index)
        first->Set(serverDefaults[index], true);
    CheckEnabledSet(*first, serverDefaults, 3, "the existing three reset selections replace earlier map permissions");
    Check(!first->Get(WEAPONTYPE_SAWNOFF_SHOTGUN), "reset explicitly removes earlier sawnoff permission");

    // The SDK enum is converted to the same signed integer by both backends.
    // Exercise representative malformed casts without indexing storage first.
    for (const auto weapon : {static_cast<eWeaponType>(-1), static_cast<eWeaponType>(WEAPONTYPE_INVALID), static_cast<eWeaponType>(WEAPONTYPE_LAST_WEAPONTYPE)})
    {
        first->Set(static_cast<int>(weapon), true);
        Check(!first->Get(static_cast<int>(weapon)), "malformed SDK enum casts stay outside the accepted table");
    }
    first->~Policy();
    std::printf("Passed %u shared MTA jetpack weapon policy checks (%zu-bit)\n", checks, sizeof(void*) * 8);
    return EXIT_SUCCESS;
}
