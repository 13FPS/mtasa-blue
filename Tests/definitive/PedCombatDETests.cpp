/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual fighting-style SDK dispatch without a native ped factory
 ****************************************************************************/
#include <SharedUtil.Defines.h>
#include <CVector.h>
#include "../../Client/game_de/CPedCombatDE.h"
#include <game/CPedIK.h>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <type_traits>
namespace fixture
{
    [[noreturn]] inline void Unqualified() noexcept
    {
        std::abort();
    }
}
#include "PedCombatFixture.generated.h"
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
    bool SameToken(const NativePedReferenceToken& a, const NativePedReferenceToken& b)
    {
        return a.version == b.version && a.ownerGeneration == b.ownerGeneration && a.slot == b.slot;
    }
    struct Owner final : NativePedVitalsAccess,
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
                         NativePedFightingStyleAccess
    {
        unsigned                reads = 0, writes = 0;
        std::uint8_t            style = 0, extra = 0;
        ClockResult             result = ClockResult::Ok;
        NativePedReferenceToken token;
        ClockResult             ReadPedFightingStyle(const NativePedReferenceToken& selected, std::uint8_t& output) noexcept override
        {
            ++reads;
            token = selected;
            output = style;
            return result;
        }
        ClockResult SetPedFightingStyle(const NativePedReferenceToken& selected, std::uint8_t value, std::uint8_t other) noexcept override
        {
            ++writes;
            token = selected;
            style = value;
            extra = other;
            return result;
        }
    };
    struct Absent final : NativePedVitalsAccess,
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
                          NativePedFightingStyleAccess
    {
    };
    template <class Action>
    void Refuses(Action action, const char* operation)
    {
        bool refused = false;
        try
        {
            action();
        }
        catch (const std::runtime_error& error)
        {
            refused = std::string(error.what()).find(operation) != std::string::npos;
        }
        Check(refused, "native combat refusal must be an explicit SDK exception, never a guessed style or silent success");
    }
}
int main()
{
    try
    {
        static_assert(std::is_abstract_v<CPedCombatDE> && std::is_base_of_v<CPedBehaviorDE, CPedCombatDE>);
        static_assert(std::is_same_v<std::underlying_type_t<eFightingStyle>, std::uint8_t>);
        Owner                     owner;
        NativePedReferenceToken   token{0xfedcba9876543210ULL, 0x123456789abcdef0ULL, 17};
        fixture::PedCombatFixture ped(owner, owner, owner, owner, owner, owner, owner, owner, owner, owner, owner, owner, token, CIVILIAN_PED);
        CPed*                     sdk = &ped;
        const CPed*               constant = &ped;
        Check(constant->GetCustomMoveAnim() == 0 && constant->GetType() == CIVILIAN_PED,
              "combat slice retains actual inherited CPU custom-animation bookkeeping and explicit wrapper identity");
        constexpr std::array<std::uint8_t, 9> extras{0, 1, 2, 3, 4, 5, 6, 7, 255};
        for (unsigned raw = 0; raw != 256; ++raw)
        {
            const auto style = static_cast<std::uint8_t>(raw);
            owner.style = style;
            const auto reads = owner.reads;
            Check(static_cast<std::uint8_t>(constant->GetFightingStyle()) == style && owner.reads == reads + 1 && SameToken(owner.token, token),
                  "actual const SDK getter returns every raw byte from the selected registered owner exactly once");
            for (const auto extra : extras)
            {
                const auto writes = owner.writes;
                sdk->SetFightingStyle(static_cast<eFightingStyle>(style), extra);
                Check(owner.writes == writes + 1 && owner.style == style && owner.extra == extra && SameToken(owner.token, token),
                      "SDK forwards both raw bytes once, leaving current-style comparison and shared extra-bit policy to the qualified native owner");
            }
        }
        const auto defaultWrites = owner.writes;
        ped.SetFightingStyle(static_cast<eFightingStyle>(37));
        Check(owner.writes == defaultWrites + 1 && owner.extra == 6 && owner.style == 37,
              "concrete SDK slice preserves classic default styleExtra6 without replacing the caller's raw style");
        for (unsigned status = 1; status <= static_cast<unsigned>(ClockResult::OriginalException); ++status)
        {
            owner.result = static_cast<ClockResult>(status);
            const auto reads = owner.reads, writes = owner.writes;
            Refuses([&] { (void)constant->GetFightingStyle(); }, "GetFightingStyle");
            Refuses([&] { sdk->SetFightingStyle(static_cast<eFightingStyle>(251), 255); }, "SetFightingStyle");
            Check(owner.reads == reads + 1 && owner.writes == writes + 1 && owner.style == 251 && owner.extra == 255 && constant->GetType() == CIVILIAN_PED,
                  "every uncertain combat effect remains persistent without SDK retry, rollback or a changed wrapper identity");
        }
        Absent                    absent;
        fixture::PedCombatFixture unbound(absent, absent, absent, absent, absent, absent, absent, absent, absent, absent, absent, absent, token, PLAYER_PED);
        Refuses([&] { (void)unbound.GetFightingStyle(); }, "GetFightingStyle");
        Refuses([&] { unbound.SetFightingStyle(static_cast<eFightingStyle>(0), 0); }, "SetFightingStyle");
        std::puts("Production MTA ped fighting-style SDK fixture completed");
        std::printf("Passed %u actual CPed combat SDK checks (%zu-bit)\n", checks, sizeof(void*) * 8);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "Ped combat SDK failed after %u checks: %s\n", checks, error.what());
        return 1;
    }
}
