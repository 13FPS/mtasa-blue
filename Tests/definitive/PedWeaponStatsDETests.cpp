/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual ped stat orchestration, manager selection and failure order
 ****************************************************************************/
#include "../../Client/game_de/CPedWeaponStatsDE.h"
#include "../../Client/game_de/CWeaponStatManagerPolicyDE.h"
#include "../../Client/game_de/CWeaponPolicyDE.h"
#include <game/CGame.h>
#include <game/CStats.h>
#include <game/CPedIK.h>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace fixture
{
    [[noreturn]] inline void Unqualified() noexcept
    {
        std::abort();
    }
    struct Scenario
    {
        std::vector<unsigned> trace;
        unsigned              throwAt = 0, statsGets = 0, rangeCalls = 0;
        CPed*                 context = nullptr;
        CWeapon*              weapon = nullptr;
        CStats*               indexStats = nullptr;
        CStats*               valueStats = nullptr;
        CWeaponStatManager*   manager = nullptr;
        CWeaponStat*          rangeStat = nullptr;
        eWeaponType           type = WEAPONTYPE_PISTOL;
        eWeaponSlot           observedSlot = WEAPONSLOT_TYPE_UNARMED;
        std::uint16_t         index = 65535, observedIndex = 0;
        eWeaponType           observedType = WEAPONTYPE_UNARMED;
        float                 level = 100, observedLevel = 0;
        void                  Event(unsigned value)
        {
            trace.push_back(value);
            if (value == throwAt)
                throw std::runtime_error("fixture original refusal");
        }
        void Reset()
        {
            trace.clear();
            statsGets = 0;
            throwAt = 0;
        }
    };
}
#include "PedWeaponStatsFixture.generated.h"
namespace
{
    using namespace mta::game_de;
    unsigned checks = 0;
    void     Check(bool ok, const char* why)
    {
        ++checks;
        if (!ok)
            throw std::runtime_error(why);
    }
    std::uint32_t Bits(float value)
    {
        std::uint32_t bits;
        std::memcpy(&bits, &value, sizeof(bits));
        return bits;
    }
    float Float(std::uint32_t bits)
    {
        float value;
        std::memcpy(&value, &bits, sizeof(value));
        return value;
    }
    void Trace(const fixture::Scenario& state, std::initializer_list<unsigned> expected, const char* why)
    {
        Check(state.trace == std::vector<unsigned>(expected), why);
    }
    class Stats final : public CStats
    {
    public:
        explicit Stats(fixture::Scenario& state, bool indexProvider) : state(state), indexProvider(indexProvider) {}
        unsigned short GetSkillStatIndex(eWeaponType type) override
        {
            Check(indexProvider, "skill index uses the first freshly resolved stat provider");
            state.observedType = type;
            state.Event(6);
            return state.index;
        }
        float GetStatValue(unsigned short index) override
        {
            Check(!indexProvider, "skill value uses the second freshly resolved stat provider");
            state.observedIndex = index;
            state.Event(8);
            return state.level;
        }
        void ModifyStat(unsigned short, float) override { fixture::Unqualified(); }
        void SetStatValue(unsigned short, float) override { fixture::Unqualified(); }

    private:
        fixture::Scenario& state;
        bool               indexProvider;
    };
    class Manager final : public CWeaponStatManagerPolicyDE
    {
    public:
        explicit Manager(fixture::Scenario& state) : state(state) {}
        using CWeaponStatManagerPolicyDE::RegisterLive;
        using CWeaponStatManagerPolicyDE::UnregisterLive;
        CWeaponStat* GetWeaponStatsFromSkillLevel(eWeaponType type, float level) override
        {
            state.observedType = type;
            state.observedLevel = level;
            state.Event(10);
            return CWeaponStatManagerPolicyDE::GetWeaponStatsFromSkillLevel(type, level);
        }
        // The fixture borrows actual custom CPU records for list-policy tests;
        // it cannot create native live wrappers or disguise them as such.
        void CreateWeaponStat(CWeaponInfo*, eWeaponType, eWeaponSkill) override { fixture::Unqualified(); }

    private:
        fixture::Scenario& state;
    };
    struct Access final : NativePedVitalsAccess,
                          NativePedMetadataAccess,
                          NativePedMutationAccess,
                          NativePedObservationAccess,
                          NativePedRotationAccess,
                          NativePedOxygenAccess,
                          NativePedDuckingAccess,
                          NativePedBikeFlagsAccess,
                          NativePedShotFlagsAccess,
                          NativePedBleedingAccess,
                          NativePedStayAccess,
                          NativePedFightingStyleAccess,
                          NativePedCurrentWeaponSlotAccess,
                          NativePedPlacementAccess
    {
        explicit Access(fixture::Scenario& state) : state(state) {}
        fixture::Scenario&      state;
        std::uint32_t           slot = 2;
        ClockResult             result = ClockResult::Ok;
        NativePedReferenceToken observed;
        unsigned                reads = 0;
        ClockResult             ReadPedCurrentWeaponSlot(const NativePedReferenceToken& token, std::uint32_t& output) noexcept override
        {
            ++reads;
            observed = token;
            state.trace.push_back(2);
            if (result == ClockResult::Ok)
                output = slot;
            return result;
        }
    };
    struct Case
    {
        fixture::Scenario         state;
        Access                    access{state};
        Stats                     indexStats{state, true}, valueStats{state, false};
        Manager                   manager{state};
        MtaGame::CWeaponStatOwned poor{WEAPONTYPE_PISTOL, WEAPONSKILL_POOR};
        MtaGame::CWeaponStatOwned normal{WEAPONTYPE_PISTOL, WEAPONSKILL_STD};
        MtaGame::CWeaponStatOwned pro{WEAPONTYPE_PISTOL, WEAPONSKILL_PRO};
        fixture::Game             game{state};
        fixture::Weapon           weapon{state};
        NativePedReferenceToken   token = []
        {
            NativePedReferenceToken value;
            value.ownerGeneration = 42;
            value.slot = 7;
            return value;
        }();
        fixture::Ped ped;
        Case()
            : ped(game, access, access, access, access, access, access, access, access, access, access, access, access, access, access, token, PLAYER_PED,
                  state)
        {
            state.context = &ped;
            state.weapon = &weapon;
            state.indexStats = &indexStats;
            state.valueStats = &valueStats;
            state.manager = &manager;
            poor.SetRequiredStatLevel(0);
            normal.SetRequiredStatLevel(100);
            pro.SetRequiredStatLevel(200);
            poor.SetWeaponRange(7);
            normal.SetWeaponRange(13);
            pro.SetWeaponRange(23);
            Check(manager.RegisterLive(poor) && manager.RegisterLive(normal) && manager.RegisterLive(pro),
                  "register actual borrowed custom records in real manager policy");
            Check(state.trace.empty() && !access.reads, "construction performs no context lookup, native read or default preparation");
        }
        ~Case()
        {
            // The SDK has no stat virtual destructor. Remove these borrowed
            // identities before their exact concrete automatic objects retire.
            manager.UnregisterLive(poor);
            manager.UnregisterLive(normal);
            manager.UnregisterLive(pro);
        }
    };
    void LazyAndExactOrder()
    {
        Case        test;
        const CPed& sdk = test.ped;
        test.state.context = nullptr;
        Check(sdk.GetCurrentWeaponStat() == nullptr, "actual context mismatch returns null before resolving any native weapon");
        Trace(test.state, {1}, "context guard is the first and only mismatch operation");
        test.state.Reset();
        Check(sdk.GetCurrentWeaponRange() == 1.0f, "the classic actual-null range result is exactly one");
        Trace(test.state, {1}, "null range keeps the same lazy context guard");
        fixture::Ped other(test.game, test.access, test.access, test.access, test.access, test.access, test.access, test.access, test.access, test.access,
                           test.access, test.access, test.access, test.access, test.access, test.token, PLAYER_PED, test.state);
        test.state.context = &other;
        test.state.Reset();
        Check(!sdk.GetCurrentWeaponStat(), "another genuine SDK receiver cannot substitute for this exact ped context");
        Trace(test.state, {1}, "another ped context does not reach selection");
        test.state.context = &test.ped;
        test.state.weapon = nullptr;
        test.state.Reset();
        Check(!sdk.GetCurrentWeaponStat(), "a real null weapon stops before type, stat or manager access");
        Trace(test.state, {1, 2, 3}, "current slot precedes the borrowed weapon lookup exactly once");
        test.state.weapon = &test.weapon;
        test.state.Reset();
        Check(sdk.GetCurrentWeaponStat() == &test.normal, "existing real manager selects the same standard custom record");
        Trace(test.state, {1, 2, 3, 4, 5, 6, 7, 8, 9, 10}, "exact classic context/slot/weapon/type/two-stat-lookups/manager order");
        Check(test.state.statsGets == 2 && test.state.observedIndex == 65535 && test.state.observedSlot == static_cast<eWeaponSlot>(2) &&
                  test.state.observedType == WEAPONTYPE_PISTOL && Bits(test.state.observedLevel) == Bits(100.0f),
              "full skill index, raw level, selected slot and type reach their original receivers without transformation");
        Check(test.access.observed.version == test.token.version && test.access.observed.ownerGeneration == test.token.ownerGeneration &&
                  test.access.observed.slot == test.token.slot,
              "inherited current-slot getter retains this wrapper's immutable token");
        test.state.Reset();
        Check(sdk.GetCurrentWeaponRange() == 13, "actual range getter reads the selected stat through the same fresh chain");
        Trace(test.state, {1, 2, 3, 4, 5, 6, 7, 8, 9, 10}, "range does not select or resolve any service twice");
    }
    void ManagerPolicyAndRawRanges()
    {
        Case        test;
        const CPed& sdk = test.ped;
        struct Sample
        {
            float        level;
            CWeaponStat* expected;
        };
        for (const auto sample : {Sample{-1, &test.poor}, Sample{99, &test.poor}, Sample{100, &test.normal}, Sample{199, &test.normal}, Sample{200, &test.pro},
                                  Sample{std::numeric_limits<float>::infinity(), &test.pro}, Sample{-std::numeric_limits<float>::infinity(), &test.poor},
                                  Sample{Float(0x7fc12345), &test.poor}})
        {
            test.state.Reset();
            test.state.level = sample.level;
            Check(sdk.GetCurrentWeaponStat() == sample.expected, "real MTA manager retains skill thresholds, infinities and NaN ordering");
            Check(Bits(test.state.observedLevel) == Bits(sample.level), "selection receives the exact original floating skill bits");
        }
        test.state.level = 100;
        for (const auto bits : {0u, 0x80000000u, 0x3f800000u, 0xc1c80000u, 0x7f800000u, 0x7fc56789u})
        {
            test.normal.SetWeaponRange(Float(bits));
            test.state.Reset();
            Check(Bits(sdk.GetCurrentWeaponRange()) == bits, "range forwards mutable actual stat bits without finite checks, clamp or cached defaults");
        }
        Check(test.manager.UnregisterLive(test.normal), "retire the standard borrowed record explicitly");
        test.state.Reset();
        Check(!sdk.GetCurrentWeaponStat(), "missing standard record follows real manager null selection despite poor/pro records");
        test.state.Reset();
        Check(sdk.GetCurrentWeaponRange() == 1.0f, "manager's actual null result uses the exact classic fallback");
        Check(test.manager.RegisterLive(test.normal), "restore borrowed standard registration for later cases");
        test.normal.SetWeaponType(WEAPONTYPE_DESERT_EAGLE);
        test.state.Reset();
        Check(!sdk.GetCurrentWeaponStat(), "mutable metadata remains visible through fresh real list selection");
        test.normal.SetWeaponType(WEAPONTYPE_PISTOL);
        test.state.type = WEAPONTYPE_INVALID;
        test.state.Reset();
        Check(!sdk.GetCurrentWeaponStat() && test.state.observedType == WEAPONTYPE_INVALID,
              "a legal unmapped SDK weapon type reaches real manager selection without a new guessed filter");
        test.state.weapon = nullptr;
        for (unsigned slot : {13u, 14u, 15u})
        {
            test.access.slot = slot;
            test.state.Reset();
            Check(!sdk.GetCurrentWeaponStat() && static_cast<unsigned>(test.state.observedSlot) == slot,
                  "legal SDK slot observations reach actual virtual GetWeapon; the policy never indexes a native array");
        }
    }
    void RefusalIsNotNull()
    {
        Case        test;
        const CPed& sdk = test.ped;
        for (unsigned event : {1u, 4u, 5u, 6u, 7u, 8u, 9u, 10u})
        {
            for (bool range : {false, true})
            {
                test.state.Reset();
                test.state.throwAt = event;
                bool refused = false;
                try
                {
                    if (range)
                        (void)sdk.GetCurrentWeaponRange();
                    else
                        (void)sdk.GetCurrentWeaponStat();
                }
                catch (const std::runtime_error& error)
                {
                    refused = std::string(error.what()) == "fixture original refusal";
                }
                Check(refused, "the original SDK exception propagates instead of becoming a null pointer or range one");
                Check(test.state.trace.size() == event, "failed operation preserves its already-observed prefix and invokes no later receiver");
                for (unsigned index = 0; index < event; ++index)
                    Check(test.state.trace[index] == index + 1, "exact accepted prefix is never repeated or reordered on failure");
            }
        }
        test.state.Reset();
        test.access.result = ClockResult::WrongThread;
        bool refused = false;
        try
        {
            (void)sdk.GetCurrentWeaponRange();
        }
        catch (const std::runtime_error&)
        {
            refused = true;
        }
        Check(refused, "inherited qualified slot refusal cannot become a successful default range");
        Trace(test.state, {1, 2}, "native selection refusal stops before GetWeapon");
        test.access.result = ClockResult::Ok;
        test.state.Reset();
        Check(sdk.GetCurrentWeaponStat() == &test.normal, "a later explicit caller can use the unchanged receiver; no hidden retry occurred");
    }
    class RangeProbe final : public fixture::Ped
    {
    public:
        using fixture::Ped::Ped;
        CWeaponStat* GetCurrentWeaponStat() const override
        {
            ++scenario.rangeCalls;
            scenario.Event(20);
            return scenario.rangeStat;
        }
    };
    void RangeVirtualDispatch()
    {
        Case       test;
        RangeProbe probe(test.game, test.access, test.access, test.access, test.access, test.access, test.access, test.access, test.access, test.access,
                         test.access, test.access, test.access, test.access, test.access, test.token, PLAYER_PED, test.state);
        test.state.Reset();
        test.state.rangeStat = &test.pro;
        Check(probe.GetCurrentWeaponRange() == 23 && test.state.rangeCalls == 1,
              "range resolves the current stat virtual once without bypassing an enclosing override");
        Trace(test.state, {20}, "range does not eagerly resolve another weapon or context");
        test.state.Reset();
        test.state.rangeStat = nullptr;
        Check(probe.GetCurrentWeaponRange() == 1.0f && test.state.rangeCalls == 2, "an actual null virtual result retains the original fallback");
    }
}
int main()
{
    static_assert(std::is_abstract_v<CPedWeaponStatsDE> && std::is_base_of_v<CPed, CPedWeaponStatsDE>);
    try
    {
        LazyAndExactOrder();
        ManagerPolicyAndRawRanges();
        RefusalIsNotNull();
        RangeVirtualDispatch();
        std::puts("Production actual MTA ped weapon-stat orchestration SDK fixture completed");
        std::printf("Passed %u actual CPed weapon-stat orchestration SDK checks (%u-bit)\n", checks, unsigned(sizeof(void*) * 8));
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "FAIL: %s\n", error.what());
        return 1;
    }
}
