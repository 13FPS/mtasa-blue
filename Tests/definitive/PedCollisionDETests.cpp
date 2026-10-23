/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual CEntity collision-use dispatch through immutable borrowed identity
 ****************************************************************************/
#include <SharedUtil.Defines.h>
#include "../../Client/game_de/CPedCollisionDE.h"
#include <game/CGame.h>
#include <game/CPedIK.h>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <initializer_list>
#include <stdexcept>
#include <string>
namespace ped_collision_fixture
{
    [[noreturn]] inline void Unqualified(const char* method) noexcept
    {
        std::fprintf(stderr, "Unqualified collision fixture SDK call: %s\n", method);
        std::abort();
    }
}
#include "PedCollisionFixture.generated.h"
namespace
{
    using namespace mta::game_de;
    unsigned collisionChecks = 0;
    void     CheckCollision(bool ok, const char* why)
    {
        ++collisionChecks;
        if (!ok)
            throw std::runtime_error(why);
    }
    struct CollisionAccesses : NativePedVitalsAccess,
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
                               NativePedWaterFlagsAccess,
                               NativePedWaterTaskAccess,
                               NativePedOrientationAccess,
                               NativePedAreaAccess,
                               NativePedFireAccess,
                               NativePedCollisionAccess
    {
    };
    struct CollisionOwner final : CollisionAccesses
    {
        unsigned                writes = 0, fireReads = 0;
        NativePedReferenceToken received;
        ClockResult             result = ClockResult::Ok;
        bool                    requested = false;
        ClockResult             SetPedUsesCollision(const NativePedReferenceToken& token, bool enabled) noexcept override
        {
            ++writes;
            received = token;
            requested = enabled;
            return result;
        }
        ClockResult ReadPedOnFire(const NativePedReferenceToken&, bool& output) noexcept override
        {
            ++fireReads;
            output = true;
            return ClockResult::Ok;
        }
    };
    bool SameCollisionToken(const NativePedReferenceToken& a, const NativePedReferenceToken& b)
    {
        return a.version == b.version && a.ownerGeneration == b.ownerGeneration && a.slot == b.slot;
    }
}
int main()
{
    try
    {
        ped_collision_fixture::Game                game;
        CollisionOwner                             owner;
        NativePedReferenceToken                    token{0x123456789abcdef0ull, 0xfedcba9876543210ull, 17};
        const auto                                 expected = token;
        ped_collision_fixture::PedCollisionFixture ped(game, owner, owner, owner, owner, owner, owner, owner, owner, owner, owner, owner, owner, owner, owner,
                                                       owner, owner, owner, owner, owner, owner, token, 0x12345678);
        CEntity&                                   sdk = ped;
        token = {};
        CheckCollision(!owner.writes && !owner.fireReads && ped.GetType() == 0x12345678 && ped.GetCustomMoveAnim() == 0,
                       "collision construction copies borrowed immutable identity, preserving actual CPU bookkeeping with no owner/game effects");
        for (bool enabled : {false, true, true, false})
        {
            const auto before = owner.writes;
            sdk.SetUsesCollision(enabled);
            CheckCollision(owner.writes == before + 1 && owner.requested == enabled && SameCollisionToken(owner.received, expected) && !owner.fireReads &&
                               !ped.repairCalls,
                           "actual CEntity SDK setter forwards exact bool/token once without fire, repair or borrowed-game calls");
        }
        for (unsigned status = 1; status <= static_cast<unsigned>(ClockResult::OriginalException); ++status)
        {
            owner.result = static_cast<ClockResult>(status);
            const auto before = owner.writes;
            bool       refused = false;
            try
            {
                sdk.SetUsesCollision(true);
            }
            catch (const std::runtime_error& error)
            {
                refused = std::string(error.what()).find("SetUsesCollision") != std::string::npos;
            }
            CheckCollision(refused && owner.writes == before + 1 && owner.requested && !ped.repairCalls,
                           "every owner refusal remains explicit with operation context and exactly one SDK dispatch; no accepted-effect retry/repair");
        }
        CheckCollision(static_cast<const CPed&>(ped).IsOnFire() && owner.fireReads == 1,
                       "one SDK chain preserves the actual inherited const fire query independently of collision-use writes");
        CheckCollision(!sdk.IsOnFire() && owner.fireReads == 1,
                       "nonconst CEntity fire fallback remains the actual shared CPUfalse policy with no native fire dispatch");
        CheckCollision(ped.IsOnFire() && owner.fireReads == 2, "direct derivedped lookup preserves Fireconst name hiding just like actual classic CPed");
        CollisionAccesses                          unavailable;
        ped_collision_fixture::PedCollisionFixture missing(game, unavailable, unavailable, unavailable, unavailable, unavailable, unavailable, unavailable,
                                                           unavailable, unavailable, unavailable, unavailable, unavailable, unavailable, unavailable,
                                                           unavailable, unavailable, unavailable, unavailable, unavailable, unavailable, expected, 0);
        bool                                       refused = false;
        try
        {
            static_cast<CEntity&>(missing).SetUsesCollision(false);
        }
        catch (const std::runtime_error&)
        {
            refused = true;
        }
        CheckCollision(refused, "an older/read-only owner cannot fabricate accepted collision-use mutation");
        std::puts("Actual ped collision-use SDK fixture completed");
        std::printf("Passed %u ped collision-use SDK checks (64-bit; no native engine code executed)\n", collisionChecks);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "FAIL %u: %s\n", collisionChecks, error.what());
        return 1;
    }
}
