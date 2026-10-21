/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Check the shared MTA 47+3*11 weapon-info slot selection contract
 *****************************************************************************/
#include <windows.h>
#include <game/CWeaponInfoIndexPolicy.h>

#include <array>
#include <cstdio>
#include <cstdlib>

namespace
{
    using Policy = MtaGame::CWeaponInfoIndexPolicy;
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

    struct Row
    {
        eWeaponSkill skill;
        int          firstWeapon, lastWeapon;
        std::size_t  firstSlot;
    };
}

int main()
{
    static_assert(Policy::StandardCount == 47 && Policy::SkillCount == 11 && Policy::SlotCount == 80);
    const std::array<Row, 4>                rows = {{{WEAPONSKILL_STD, WEAPONTYPE_UNARMED, WEAPONTYPE_PARACHUTE, 0},
                                                     {WEAPONSKILL_POOR, WEAPONTYPE_PISTOL, WEAPONTYPE_TEC9, 47},
                                                     {WEAPONSKILL_PRO, WEAPONTYPE_PISTOL, WEAPONTYPE_TEC9, 58},
                                                     {WEAPONSKILL_SPECIAL, WEAPONTYPE_PISTOL, WEAPONTYPE_TEC9, 69}}};
    std::array<unsigned, Policy::SlotCount> seen{};
    for (const auto& row : rows)
    {
        for (int weapon = 0; weapon <= WEAPONTYPE_INVALID; ++weapon)
        {
            std::size_t index = 1000;
            const bool  accepted = Policy::TryGetIndex(static_cast<eWeaponType>(weapon), row.skill, index);
            const bool  expected = weapon >= row.firstWeapon && weapon <= row.lastWeapon;
            Check(accepted == expected, "each skill accepts exactly the established SDK weapon-ID range");
            if (expected)
            {
                Check(index == row.firstSlot + static_cast<std::size_t>(weapon - row.firstWeapon), "accepted pair selects the established contiguous slot");
                Check(index < seen.size(), "selected slot remains in the 80 actual wrapper entries");
                ++seen[index];
            }
            else
                Check(index == 1000, "refused lookup preserves the caller's output slot");
        }
    }
    for (const auto count : seen)
        Check(count == 1, "the four valid skill ranges cover each wrapper slot exactly once");

    // eWeaponSkill is a uint8_t SDK enum. Classic's switch deliberately falls
    // through to offset zero for these otherwise unknown skill values.
    for (const auto skill : {static_cast<eWeaponSkill>(4), static_cast<eWeaponSkill>(127), static_cast<eWeaponSkill>(255)})
    {
        for (int weapon = 0; weapon <= WEAPONTYPE_INVALID; ++weapon)
        {
            std::size_t index = 1000;
            const bool  accepted = Policy::TryGetIndex(static_cast<eWeaponType>(weapon), skill, index);
            const bool  expected = weapon >= WEAPONTYPE_PISTOL && weapon <= WEAPONTYPE_TEC9;
            Check(accepted == expected, "unknown skill retains classic handgun-to-Tec9 admission");
            Check(index == (expected ? static_cast<std::size_t>(weapon) : 1000), "unknown skill retains classic standard-slot fallback and refusal output");
        }
    }
    std::printf("Passed %u shared MTA weapon-info index policy checks (%zu-bit)\n", checks, sizeof(void*) * 8);
    return EXIT_SUCCESS;
}
