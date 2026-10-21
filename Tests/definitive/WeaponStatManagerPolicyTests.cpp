/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Check actual MTA manager SDK policy, aliases and CPU lifetimes
 *****************************************************************************/
#include "../../Client/game_de/CWeaponStatManagerPolicyDE.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <memory>
#include <stdexcept>
#include <vector>

namespace
{
    using Manager = mta::game_de::CWeaponStatManagerPolicyDE;
    using Owned = MtaGame::CWeaponStatOwned;
    unsigned checks = 0;

    void Check(bool value, const char* description)
    {
        ++checks;
        if (!value)
        {
            std::fprintf(stderr, "Failed after %u checks: %s\n", checks, description);
            std::exit(EXIT_FAILURE);
        }
    }

    class CpuManager final : public Manager
    {
    public:
        CpuManager() = default;
        using Manager::RegisterLive;
        using Manager::UnregisterLive;

        // The production class intentionally keeps this native operation pure.
        // A CPU fixture cannot silently replace native wrappers with storage.
        void CreateWeaponStat(CWeaponInfo*, eWeaponType, eWeaponSkill) override
        {
            throw std::logic_error("native live-stat factory is not admitted by this CPU fixture");
        }
    };

    void OriginalListOrder()
    {
        static_assert(WEAPONTYPE_PISTOL == 22 && WEAPONTYPE_TEC9 == 32 && WEAPONTYPE_EXTINGUISHER == 42);
        CpuManager                   manager;
        CWeaponStatManager&          sdk = manager;
        std::array<CWeaponStat*, 43> standard{};
        std::array<CWeaponStat*, 11> poor{}, repeatedStandard{}, professional{};
        for (int index = 0; index != 43; ++index)
        {
            const auto type = static_cast<eWeaponType>(22 + index);
            auto*      value = sdk.GetOriginalWeaponStats(type);
            Check(value && value->GetWeaponType() == type && value->GetWeaponSkillLevel() == WEAPONSKILL_STD,
                  "original standard records preserve the actual classic 22..64 metadata loop");
            standard[index] = value;
            const auto* defaults = MtaGame::CWeaponStatDefaults::Select(type, WEAPONSKILL_STD);
            Check(value->GetWeaponRange() == (defaults ? defaults->weapon_range : 0.0f) && value->GetFlags() == (defaults ? defaults->flags : 0),
                  "supported defaults and unsupported zero CPU records remain distinct from live native rows");
            if (type > WEAPONTYPE_TEC9)
                Check(sdk.GetOriginalWeaponStats(type, static_cast<eWeaponSkill>(255)) == value,
                      "non-skill original lookup ignores even an unknown requested skill");
        }
        for (int index = 0; index != 11; ++index)
        {
            const auto type = static_cast<eWeaponType>(22 + index);
            poor[index] = sdk.GetOriginalWeaponStats(type, WEAPONSKILL_POOR);
            professional[index] = sdk.GetOriginalWeaponStats(type, WEAPONSKILL_PRO);
            Check(poor[index] && professional[index] && poor[index] != standard[index] && professional[index] != standard[index],
                  "skill-specific originals retain separate mutable CPU identities");
            standard[index]->SetWeaponType(WEAPONTYPE_INVALID);
            repeatedStandard[index] = sdk.GetOriginalWeaponStats(type, WEAPONSKILL_STD);
            standard[index]->SetWeaponType(type);
            Check(repeatedStandard[index] && repeatedStandard[index] != standard[index] && sdk.GetOriginalWeaponStats(type, WEAPONSKILL_STD) == standard[index],
                  "metadata edits expose the existing duplicate standard original while first-match order remains stable");
            Check(!sdk.GetOriginalWeaponStats(type, static_cast<eWeaponSkill>(255)), "skill weapons require an exact current metadata skill");
        }

        struct Identity
        {
            CWeaponStat* value;
            eWeaponType  type;
            eWeaponSkill skill;
        };
        std::vector<Identity> insertion;
        for (auto* value : standard)
            insertion.push_back({value, value->GetWeaponType(), value->GetWeaponSkillLevel()});
        for (const auto* list : {&poor, &repeatedStandard, &professional})
            for (auto* value : *list)
                insertion.push_back({value, value->GetWeaponType(), value->GetWeaponSkillLevel()});
        Check(insertion.size() == 76, "manager retains the exact current classic original-record count");
        for (auto& entry : insertion)
        {
            entry.value->SetWeaponType(static_cast<eWeaponType>(64));
            entry.value->SetWeaponSkillLevel(WEAPONSKILL_STD);
        }
        for (auto& entry : insertion)
        {
            Check(sdk.GetOriginalWeaponStats(static_cast<eWeaponType>(64)) == entry.value,
                  "all duplicate/metadata aliases preserve the real insertion order through the actual SDK");
            entry.value->SetWeaponType(WEAPONTYPE_INVALID);
        }
        Check(!sdk.GetOriginalWeaponStats(static_cast<eWeaponType>(64)), "exhausting mutable aliases reveals no hidden replacement originals");
        for (auto& entry : insertion)
        {
            entry.value->SetWeaponType(entry.type);
            entry.value->SetWeaponSkillLevel(entry.skill);
        }
        Check(sdk.GetOriginalWeaponStats(WEAPONTYPE_PISTOL) == standard.front(), "restored metadata resolves the same stable owned original");
        Check(!sdk.GetOriginalWeaponStats(WEAPONTYPE_UNARMED) && !sdk.GetOriginalWeaponStats(static_cast<eWeaponType>(65)),
              "originals are not invented outside the actual classic loop");
    }

    void LiveLookupAndSkills()
    {
        CpuManager          manager;
        CWeaponStatManager& sdk = manager;
        Owned poor(WEAPONTYPE_PISTOL, WEAPONSKILL_POOR), normal(WEAPONTYPE_PISTOL, WEAPONSKILL_STD), professional(WEAPONTYPE_PISTOL, WEAPONSKILL_PRO),
            duplicate(WEAPONTYPE_PISTOL, WEAPONSKILL_STD);
        normal.SetRequiredStatLevel(200);
        professional.SetRequiredStatLevel(700);
        Check(!sdk.GetWeaponStats(WEAPONTYPE_PISTOL) && !sdk.GetWeaponStatsFromSkillLevel(WEAPONTYPE_PISTOL, 1000.0f) &&
                  sdk.GetWeaponSkillFromSkillLevel(WEAPONTYPE_PISTOL, 1000.0f) == WEAPONSKILL_STD,
              "empty live registry supplies no CPU substitute for native weapon data");
        Check(manager.RegisterLive(normal) && !manager.RegisterLive(normal), "one borrowed identity is registered once without taking ownership");
        Check(manager.RegisterLive(duplicate) && sdk.GetWeaponStats(WEAPONTYPE_PISTOL) == &normal,
              "distinct wrappers with matching metadata retain classic first-match order");
        normal.SetWeaponType(WEAPONTYPE_DESERT_EAGLE);
        Check(sdk.GetWeaponStats(WEAPONTYPE_PISTOL) == &duplicate && sdk.GetWeaponStats(WEAPONTYPE_DESERT_EAGLE) == &normal,
              "registered metadata edits are visible rather than hidden behind a type cache");
        normal.SetWeaponType(WEAPONTYPE_PISTOL);
        Check(manager.UnregisterLive(duplicate) && !manager.UnregisterLive(duplicate), "unregistration retires exactly the borrowed pointer");
        Check(sdk.GetWeaponStatsFromSkillLevel(WEAPONTYPE_PISTOL, -100.0f) == &normal &&
                  sdk.GetWeaponSkillFromSkillLevel(WEAPONTYPE_PISTOL, std::numeric_limits<float>::quiet_NaN()) == WEAPONSKILL_STD,
              "missing poor/pro records preserve MTA's standard fallback including NaN");
        Check(manager.RegisterLive(poor) && sdk.GetWeaponStatsFromSkillLevel(WEAPONTYPE_PISTOL, 9999.0f) == &normal,
              "a missing professional record still selects the existing standard wrapper");
        Check(manager.RegisterLive(professional), "all actual borrowed skill records can be present");
        const std::array<float, 8> levels = {
            std::numeric_limits<float>::quiet_NaN(), -std::numeric_limits<float>::infinity(), -1.0f, 199.75f, 200.0f, 699.75f, 700.0f,
            std::numeric_limits<float>::infinity()};
        const std::array<CWeaponStat*, 8> selected = {&poor, &poor, &poor, &poor, &normal, &normal, &professional, &professional};
        const std::array<eWeaponSkill, 8> skills = {WEAPONSKILL_POOR, WEAPONSKILL_POOR, WEAPONSKILL_POOR, WEAPONSKILL_POOR,
                                                    WEAPONSKILL_STD,  WEAPONSKILL_STD,  WEAPONSKILL_PRO,  WEAPONSKILL_PRO};
        for (std::size_t index = 0; index != levels.size(); ++index)
            Check(sdk.GetWeaponStatsFromSkillLevel(WEAPONTYPE_PISTOL, levels[index]) == selected[index] &&
                      sdk.GetWeaponSkillFromSkillLevel(WEAPONTYPE_PISTOL, levels[index]) == skills[index],
                  "actual SDK selections preserve exact boundaries, NaN, infinities and comparison order");
        professional.SetRequiredStatLevel(200);
        Check(sdk.GetWeaponStatsFromSkillLevel(WEAPONTYPE_PISTOL, 200.0f) == &professional, "equal professional/standard thresholds choose professional first");
        professional.SetRequiredStatLevel(10);
        Check(sdk.GetWeaponStatsFromSkillLevel(WEAPONTYPE_PISTOL, 10.0f) == &professional, "later mutable thresholds are consumed without a stale snapshot");
        Check(manager.UnregisterLive(normal), "standard identity retires before its wrapper can be destroyed");
        Check(!sdk.GetWeaponStatsFromSkillLevel(WEAPONTYPE_PISTOL, 1000.0f) && sdk.GetWeaponSkillFromSkillLevel(WEAPONTYPE_PISTOL, 1000.0f) == WEAPONSKILL_STD,
              "poor/pro alone never invent a missing standard wrapper");
        Check(manager.RegisterLive(normal) && manager.UnregisterLive(poor) && manager.UnregisterLive(professional) && manager.UnregisterLive(normal),
              "each borrowed wrapper can retire independently without deletion");

        Owned nonSkill(WEAPONTYPE_SNIPERRIFLE, WEAPONSKILL_STD);
        nonSkill.SetRequiredStatLevel(5);
        Check(manager.RegisterLive(nonSkill), "non-skill native metadata uses the same ordered registry");
        Check(sdk.GetWeaponStats(WEAPONTYPE_SNIPERRIFLE, WEAPONSKILL_POOR) == &nonSkill &&
                  sdk.GetWeaponStats(WEAPONTYPE_SNIPERRIFLE, static_cast<eWeaponSkill>(255)) == &nonSkill,
              "types after TEC9 ignore requested skill in the actual list lookup");
        Check(sdk.GetWeaponStatsFromSkillLevel(WEAPONTYPE_SNIPERRIFLE, -1.0f) == &nonSkill &&
                  sdk.GetWeaponSkillFromSkillLevel(WEAPONTYPE_SNIPERRIFLE, -1.0f) == WEAPONSKILL_POOR &&
                  sdk.GetWeaponSkillFromSkillLevel(WEAPONTYPE_SNIPERRIFLE, 5.0f) == WEAPONSKILL_PRO,
              "non-skill aliases retain classic poor/pro enum results instead of being normalized");
        Check(manager.UnregisterLive(nonSkill), "last native alias is removed without altering its data");
        nonSkill.SetDamagePerHit(1234);
        Check(nonSkill.GetDamagePerHit() == 1234, "unregistration leaves caller-owned storage alive");
    }

    void MutableOriginalsAndFactories()
    {
        CpuManager          manager, independent;
        CWeaponStatManager& sdk = manager;
        auto*               original = sdk.GetOriginalWeaponStats(WEAPONTYPE_PISTOL);
        auto*               other = independent.GetOriginalWeaponStats(WEAPONTYPE_PISTOL);
        Check(original && other && original != other && original->GetFireOffset() != other->GetFireOffset(),
              "independent manager lifetimes own separate original records and borrowed vectors");
        original->SetFlags(0);
        original->SetAnimGroup(79);
        std::unique_ptr<Owned> unlisted(static_cast<Owned*>(sdk.CreateWeaponStatUnlisted(WEAPONTYPE_PISTOL, WEAPONSKILL_STD)));
        Check(unlisted && !sdk.GetWeaponStats(WEAPONTYPE_PISTOL), "unlisted CPU factory does not populate a live native registry");
        unlisted->SetFlags(0);
        unlisted->SetFlagBits(WEAPONTYPE_TWIN_PISTOLS);
        Check(unlisted->GetAnimGroup() == 17, "shared MTA flags enable the established dual animation");
        original->SetAnimGroup(83);
        unlisted->ClearFlagBits(WEAPONTYPE_TWIN_PISTOLS);
        Check(unlisted->GetAnimGroup() == 83, "unlisted flags lazily resolve the current mutable manager original");
        unlisted->SetFlags(0);
        unlisted->SetFlagBits(WEAPONTYPE_ANIM_RELOAD);
        Check(!unlisted->IsFlagSet(WEAPONTYPE_ANIM_RELOAD), "current unsupported original flags restrict new reload intent");
        original->SetFlags(WEAPONTYPE_ANIM_RELOAD);
        unlisted->SetFlagBits(WEAPONTYPE_ANIM_RELOAD);
        Check(unlisted->IsFlagSet(WEAPONTYPE_ANIM_RELOAD), "later original changes are consumed without recreating the unlisted object");
        auto* desert = sdk.GetOriginalWeaponStats(WEAPONTYPE_DESERT_EAGLE);
        desert->SetAnimGroup(91);
        unlisted->SetFlags(WEAPONTYPE_TWIN_PISTOLS);
        unlisted->SetWeaponType(WEAPONTYPE_DESERT_EAGLE);
        unlisted->ClearFlagBits(WEAPONTYPE_TWIN_PISTOLS);
        Check(unlisted->GetAnimGroup() == 91, "mutable unlisted metadata changes the lazy provider lookup");
        original->SetFlags(0);
        original->SetAnimGroup(99);
        original->SetFlagBits(WEAPONTYPE_TWIN_PISTOLS);
        original->ClearFlagBits(WEAPONTYPE_TWIN_PISTOLS);
        Check(original->GetAnimGroup() == 17, "mutable original flag transitions retain classic self-lookup semantics");
        const float otherRange = other->GetWeaponRange();
        original->SetWeaponRange(-99.5f);
        original->GetFireOffset()->fX = 333.0f;
        sdk.Init();
        sdk.ResetLists();
        Check(original->GetWeaponRange() == -99.5f && original->GetFireOffset()->fX == 333.0f && other->GetWeaponRange() == otherRange,
              "Init and the actual disabled ResetLists policy do not replay/reset CPU originals or another manager");
        Owned live(WEAPONTYPE_PISTOL, WEAPONSKILL_STD);
        live.SetWeaponRange(812.5f);
        Check(manager.RegisterLive(live), "fixture can register an actual complete SDK record for reset-policy observation");
        sdk.ResetLists();
        Check(live.GetWeaponRange() == 812.5f, "shared disabled list-reset policy performs no hidden native/default writes");
        Check(manager.UnregisterLive(live), "borrowed reset fixture retires before its own lifetime ends");
        unlisted->SetWeaponType(WEAPONTYPE_PISTOL);
        unlisted->SetWeaponRange(777.0f);
        Check(sdk.LoadDefault(unlisted.get(), WEAPONTYPE_DESERT_EAGLE) && unlisted->GetWeaponType() == WEAPONTYPE_PISTOL &&
                  unlisted->GetWeaponRange() == MtaGame::CWeaponStatDefaults::Select(WEAPONTYPE_DESERT_EAGLE, WEAPONSKILL_STD)->weapon_range,
              "actual default loading resets values through the SDK without rewriting metadata");
        const auto preserved = unlisted->GetWeaponRange();
        Check(sdk.LoadDefault(unlisted.get(), WEAPONTYPE_UNARMED) && sdk.LoadDefault(nullptr, WEAPONTYPE_PISTOL) && unlisted->GetWeaponRange() == preserved,
              "existing unsupported/null default-load success convention is unchanged");
        unlisted->SetWeaponType(WEAPONTYPE_UNARMED);
        unlisted->SetFlags(0);
        bool refused = false;
        try
        {
            unlisted->SetFlagBits(WEAPONTYPE_ANIM_RELOAD);
        }
        catch (const std::out_of_range&)
        {
            refused = true;
        }
        Check(refused && !unlisted->GetFlags(), "missing mutable original is an honest failure rather than a fabricated baseline");
        refused = false;
        try
        {
            sdk.CreateWeaponStat(nullptr, WEAPONTYPE_PISTOL, WEAPONSKILL_STD);
        }
        catch (const std::logic_error&)
        {
            refused = true;
        }
        Check(refused && !sdk.GetWeaponStats(WEAPONTYPE_PISTOL), "CPU fixture cannot manufacture the intentionally pure native factory");
    }

    void BorrowedLifetimes()
    {
        Owned caller(WEAPONTYPE_PISTOL, WEAPONSKILL_STD);
        caller.SetDamagePerHit(1001);
        {
            CpuManager manager;
            Check(manager.RegisterLive(caller), "caller owns the registered SDK object throughout manager lifetime");
        }
        Check(caller.GetDamagePerHit() == 1001, "manager destruction does not delete or change borrowed live objects");
        caller.SetDamagePerHit(1002);
        Check(caller.GetDamagePerHit() == 1002, "caller-owned record remains fully usable after registry retirement");
        for (unsigned generation = 0; generation != 8; ++generation)
        {
            CpuManager  manager;
            auto*       original = manager.GetOriginalWeaponStats(WEAPONTYPE_PISTOL);
            const auto* defaults = MtaGame::CWeaponStatDefaults::Select(WEAPONTYPE_PISTOL, WEAPONSKILL_STD);
            Check(original && original->GetWeaponRange() == defaults->weapon_range, "fresh manager lifecycle reconstructs genuine independent CPU originals");
            original->SetWeaponRange(static_cast<float>(generation) + 2000.0f);
        }
    }
}

int main()
{
    static_assert(std::is_abstract_v<Manager> && !std::is_copy_constructible_v<Manager> && !std::is_move_constructible_v<Manager>);
    OriginalListOrder();
    LiveLookupAndSkills();
    MutableOriginalsAndFactories();
    BorrowedLifetimes();
    std::printf("Passed %u weapon-stat manager policy checks (%zu-bit)\n", checks, sizeof(void*) * 8);
    return EXIT_SUCCESS;
}
