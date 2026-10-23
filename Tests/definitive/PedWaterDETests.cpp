/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual SDK physical water setter on the single DE ped inheritance chain
 ****************************************************************************/
#include <SharedUtil.Defines.h>
#include "../../Client/game_de/CPedWaterDE.h"
#include <game/CGame.h>
#include <game/CPedIK.h>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <stdexcept>
#include <type_traits>
namespace ped_water_fixture
{
    [[noreturn]] inline void Unqualified(const char* method) noexcept
    {
        std::fprintf(stderr, "Unqualified water fixture SDK call: %s\n", method);
        std::abort();
    }
}
#include "PedWaterFixture.generated.h"
namespace
{
    using namespace mta::game_de;
    unsigned checks = 0;
    void     Check(bool value, const char* why)
    {
        ++checks;
        if (!value)
            throw std::runtime_error(why);
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
                      NativePedCurrentWeaponSlotAccess,
                      NativePedPlacementAccess,
                      NativePedWaterFlagsAccess
    {
    };
    struct Owner final : Accesses
    {
        NativePedReferenceToken observed;
        bool                    value = false;
        unsigned                calls = 0;
        ClockResult             result = ClockResult::Ok;
        ClockResult             SetPedInWaterFlags(const NativePedReferenceToken& token, bool target) noexcept override
        {
            ++calls;
            observed = token;
            value = target;
            return result;
        }
    };
}
int main()
{
    try
    {
        static_assert(std::is_abstract_v<CPedWaterDE> && std::is_base_of_v<CPedWeaponStatsDE, CPedWaterDE>);
        ped_water_fixture::Game            game;
        Owner                              owner;
        const NativePedReferenceToken      token{0xfedcba9876543210ULL, 0x123456789abcdef0ULL, 17};
        ped_water_fixture::PedWaterFixture ped(game, owner, owner, owner, owner, owner, owner, owner, owner, owner, owner, owner, owner, owner, owner, owner,
                                               token, 0x12345678);
        CPed&                              sdk = ped;
        Check(!owner.calls && sdk.GetType() == 0x12345678 && sdk.GetCustomMoveAnim() == 0,
              "water layer preserves the actual Stats/Pose chain and required borrowed game without constructor native I/O");
        for (bool enabled : {false, true, true, false})
        {
            const auto before = owner.calls;
            sdk.SetInWaterFlags(enabled);
            Check(owner.calls == before + 1 && owner.value == enabled && owner.observed.version == token.version &&
                      owner.observed.ownerGeneration == token.ownerGeneration && owner.observed.slot == token.slot,
                  "actual CPed virtual setter forwards the immutable token and one bool without a task/script query or CPU cache");
        }
        for (const auto result :
             {ClockResult::NotInitialized, ClockResult::WrongThread, ClockResult::Expired, ClockResult::BindingChanged, ClockResult::UnsupportedBuild,
              ClockResult::OwnerConflict, ClockResult::WriteFailure, ClockResult::OriginalException, ClockResult::Stopped, ClockResult::Busy})
        {
            owner.result = result;
            const auto before = owner.calls;
            bool       refused = false;
            try
            {
                sdk.SetInWaterFlags(true);
            }
            catch (const std::runtime_error&)
            {
                refused = true;
            }
            Check(refused && owner.calls == before + 1 && !ped.repairCalls,
                  "refused/uncertain water storage is explicit and never retried, repaired or converted into successful SDK state");
        }
        Accesses                           absent;
        ped_water_fixture::PedWaterFixture unavailable(game, absent, absent, absent, absent, absent, absent, absent, absent, absent, absent, absent, absent,
                                                       absent, absent, absent, token, 2);
        bool                               refused = false;
        try
        {
            unavailable.SetInWaterFlags(false);
        }
        catch (const std::runtime_error&)
        {
            refused = true;
        }
        Check(refused, "default access cannot invent a water setter or read a different current player");
        std::puts("Production MTA ped physical water SDK fixture completed");
        std::printf("Passed %u actual CPed water SDK checks (%zu-bit)\n", checks, sizeof(void*) * 8);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "Water SDK failed after %u checks: %s\n", checks, error.what());
        return 1;
    }
}
