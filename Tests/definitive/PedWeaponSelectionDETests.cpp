/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual const CPed current-slot getter without publishing a ped factory
 ****************************************************************************/
#include <SharedUtil.Defines.h>
#include <CVector.h>
#include "../../Client/game_de/CPedWeaponSelectionDE.h"
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
#include "PedWeaponSelectionFixture.generated.h"
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
    struct Accesses : NativePedVitalsAccess,
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
                      NativePedCurrentWeaponSlotAccess
    {
    };
    struct Owner final : Accesses
    {
        unsigned                reads = 0;
        std::uint32_t           selectedSlot = 0;
        ClockResult             result = ClockResult::Ok;
        NativePedReferenceToken token;
        ClockResult             ReadPedCurrentWeaponSlot(const NativePedReferenceToken& selected, std::uint32_t& output) noexcept override
        {
            ++reads;
            token = selected;
            output = selectedSlot;
            return result;
        }
    };
    template <class Action>
    void Refuses(Action action)
    {
        bool refused = false;
        try
        {
            action();
        }
        catch (const std::runtime_error& error)
        {
            refused = std::string(error.what()).find("GetCurrentWeaponSlot") != std::string::npos;
        }
        Check(refused, "unavailable or unrepresentable current slot is an explicit SDK refusal, never an invented default");
    }
}
int main()
{
    try
    {
        static_assert(std::is_abstract_v<CPedWeaponSelectionDE> && std::is_base_of_v<CPedCombatDE, CPedWeaponSelectionDE>);
        static_assert(sizeof(eWeaponSlot) == sizeof(std::uint32_t));
        Owner                              owner;
        const NativePedReferenceToken      token{0xfedcba9876543210ULL, 0x123456789abcdef0ULL, 17};
        fixture::PedWeaponSelectionFixture ped(owner, owner, owner, owner, owner, owner, owner, owner, owner, owner, owner, owner, owner, token, CIVILIAN_PED);
        const CPed*                        sdk = &ped;
        Check(!owner.reads && sdk->GetCustomMoveAnim() == 0 && sdk->GetType() == CIVILIAN_PED,
              "construction and inherited CPU bookkeeping do not fetch or infer a current weapon slot");
        for (std::uint32_t raw = 0; raw != 16; ++raw)
        {
            owner.selectedSlot = raw;
            const auto reads = owner.reads;
            Check(static_cast<std::uint32_t>(sdk->GetCurrentWeaponSlot()) == raw && owner.reads == reads + 1 && SameToken(owner.token, token),
                  "actual const getter forwards the same token once and preserves every representable slot value including13..15");
        }
        for (const auto raw : {std::uint32_t{16}, std::uint32_t{127}, std::uint32_t{128}, std::uint32_t{255}, std::uint32_t{256}, std::uint32_t{0xffffffff}})
        {
            owner.selectedSlot = raw;
            const auto reads = owner.reads;
            Refuses([&] { (void)sdk->GetCurrentWeaponSlot(); });
            Check(owner.reads == reads + 1 && SameToken(owner.token, token), "enum-range refusal never clamps, rereads or aliases an invalid array index");
        }
        owner.selectedSlot = 2;
        for (unsigned status = 1; status <= static_cast<unsigned>(ClockResult::OriginalException); ++status)
        {
            owner.result = static_cast<ClockResult>(status);
            const auto reads = owner.reads;
            Refuses([&] { (void)sdk->GetCurrentWeaponSlot(); });
            Check(owner.reads == reads + 1 && SameToken(owner.token, token) && sdk->GetType() == CIVILIAN_PED,
                  "every owner refusal remains explicit with no cached slot or changed wrapper identity");
        }
        Accesses                           absent;
        fixture::PedWeaponSelectionFixture unbound(absent, absent, absent, absent, absent, absent, absent, absent, absent, absent, absent, absent, absent,
                                                   token, PLAYER_PED);
        Refuses([&] { (void)unbound.GetCurrentWeaponSlot(); });
        std::puts("Production MTA ped current weapon selection SDK fixture completed");
        std::printf("Passed %u actual CPed weapon selection SDK checks (%zu-bit)\n", checks, sizeof(void*) * 8);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "Ped weapon selection SDK failed after %u checks: %s\n", checks, error.what());
        return 1;
    }
}
