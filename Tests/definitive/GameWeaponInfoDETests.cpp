/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Exercise actual CGame SDK weapon-info dispatch and missing wrappers
 *****************************************************************************/
#include "../../Client/game_de/CGameWeaponInfoDE.h"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>

namespace fixture
{
    [[noreturn]] inline void Unqualified() noexcept
    {
        std::fputs("FAIL: unrelated SDK method reached weapon-info fixture\n", stderr);
        std::abort();
    }
}

// Generated from current actual SDK declarations. GetWeaponInfo is supplied
// solely by the compiled production CGameWeaponInfoDE implementation.
#include "GameWeaponInfoFixture.generated.h"

namespace
{
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

    template <class Operation>
    void Refuses(Operation operation, const char* description)
    {
        try
        {
            operation();
        }
        catch (const std::runtime_error&)
        {
            Check(true, description);
            return;
        }
        catch (...)
        {
        }
        Check(false, description);
    }

    class UnusedTiming final : public mta::game_de::NativeGameTimingAccess
    {
    public:
        mta::game_de::ClockResult SetTimeScale(float) noexcept override { fixture::Unqualified(); }
        mta::game_de::ClockResult SetMinuteDuration(std::uint32_t) noexcept override { fixture::Unqualified(); }
        mta::game_de::ClockResult Pause(bool) noexcept override { fixture::Unqualified(); }
    };

    class TestGame final : public fixture::GameFixtureBase
    {
    public:
        TestGame(const mta::game_de::NativeGameTimingReader& reader, mta::game_de::NativeGameTimingAccess& writer) noexcept : GameFixtureBase(reader, writer)
        {
            // These are opaque pointer identities for virtual boundary checks.
            // No marker is dereferenced as a real or native weapon-info object.
            for (std::size_t slot = 0; slot < wrappers.size(); ++slot)
                wrappers[slot] = reinterpret_cast<CWeaponInfo*>(&identities[slot]);
        }

        std::array<CWeaponInfo*, MtaGame::CWeaponInfoIndexPolicy::SlotCount> wrappers{};
        unsigned                                                             reads = 0;
        std::size_t                                                          lastSlot = 1000;
        bool                                                                 refuse = false;

    protected:
        CWeaponInfo* ResolveWeaponInfoSlot(std::size_t slot) override
        {
            ++reads;
            lastSlot = slot;
            Check(slot < wrappers.size(), "actual SDK resolver never receives an out-of-range slot");
            if (refuse)
                throw std::runtime_error("fixture-owned weapon registry unavailable");
            return wrappers[slot];
        }

    private:
        std::array<unsigned char, MtaGame::CWeaponInfoIndexPolicy::SlotCount> identities{};
    };
}

int main()
{
    static_assert(std::is_abstract_v<mta::game_de::CGameWeaponInfoDE>);
    static_assert(!std::is_abstract_v<TestGame> && std::is_base_of_v<CGame, TestGame>);
    mta::game_de::NativeGameTimingReader reader;
    UnusedTiming                         writer;
    TestGame                             first(reader, writer), second(reader, writer);
    CGame&                               game = first;
    CGame&                               other = second;
    for (int weapon = WEAPONTYPE_UNARMED; weapon < WEAPONTYPE_LAST_WEAPONTYPE; ++weapon)
    {
        const auto slot = static_cast<std::size_t>(weapon);
        Check(game.GetWeaponInfo(static_cast<eWeaponType>(weapon)) == first.wrappers[slot] && first.lastSlot == slot,
              "actual CGame default skill selects each real standard-wrapper slot");
    }
    const std::array<eWeaponSkill, 3> skills = {WEAPONSKILL_POOR, WEAPONSKILL_PRO, WEAPONSKILL_SPECIAL};
    for (std::size_t row = 0; row < skills.size(); ++row)
        for (int weapon = WEAPONTYPE_PISTOL; weapon <= WEAPONTYPE_TEC9; ++weapon)
        {
            const auto slot = std::size_t{47} + row * 11 + static_cast<std::size_t>(weapon - WEAPONTYPE_PISTOL);
            Check(game.GetWeaponInfo(static_cast<eWeaponType>(weapon), skills[row]) == first.wrappers[slot] && first.lastSlot == slot,
                  "actual CGame virtual dispatch selects each of the three eleven-wrapper skill rows");
        }
    Check(first.reads == 80 && second.reads == 0, "all eighty valid lookups resolve once without touching another game registry");
    for (const auto weapon : {WEAPONTYPE_LAST_WEAPONTYPE, WEAPONTYPE_ARMOUR, WEAPONTYPE_INVALID})
        Check(game.GetWeaponInfo(weapon) == nullptr, "invalid standard weapon IDs return null without indexing a registry");
    Check(game.GetWeaponInfo(WEAPONTYPE_UNARMED, WEAPONSKILL_POOR) == nullptr && game.GetWeaponInfo(WEAPONTYPE_PARACHUTE, WEAPONSKILL_PRO) == nullptr &&
              first.reads == 80,
          "non-skill weapons are refused without a resolver call");
    Check(game.GetWeaponInfo(WEAPONTYPE_TEC9, static_cast<eWeaponSkill>(255)) == first.wrappers[WEAPONTYPE_TEC9] && first.lastSlot == WEAPONTYPE_TEC9,
          "actual SDK preserves classic unknown-skill standard-slot fallback");
    Check(
        other.GetWeaponInfo(WEAPONTYPE_PISTOL) == second.wrappers[WEAPONTYPE_PISTOL] && second.wrappers[WEAPONTYPE_PISTOL] != first.wrappers[WEAPONTYPE_PISTOL],
        "actual game instances resolve their own wrappers");
    first.wrappers[WEAPONTYPE_PISTOL] = nullptr;
    Refuses([&] { game.GetWeaponInfo(WEAPONTYPE_PISTOL); }, "missing accepted wrapper refuses explicitly rather than publishing false readiness");
    first.wrappers[WEAPONTYPE_PISTOL] = second.wrappers[WEAPONTYPE_PISTOL];
    Check(game.GetWeaponInfo(WEAPONTYPE_PISTOL) == second.wrappers[WEAPONTYPE_PISTOL], "wrapper lookup is fresh after an earlier unavailable slot");
    first.refuse = true;
    Refuses([&] { game.GetWeaponInfo(WEAPONTYPE_PISTOL); }, "registry refusal propagates through the actual SDK getter");
    Check(game.GetWeaponInfo(WEAPONTYPE_INVALID) == nullptr, "invalid IDs still return null while registry is unavailable");
    Check(!reader.IsBound(), "weapon selection policy never binds native timing or weapon storage");
    std::printf("Passed %u actual CGame weapon-info SDK selection checks (%zu-bit)\n", checks, sizeof(void*) * 8);
    return EXIT_SUCCESS;
}
