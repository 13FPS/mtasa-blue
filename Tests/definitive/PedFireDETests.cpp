/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual ped const native query and inherited nonconst entity CPU policy
 ****************************************************************************/
#include <SharedUtil.Defines.h>
#include "../../Client/game_de/CPedFireDE.h"
#include <game/CGame.h>
#include <game/CEntityFirePolicy.h>
#include <game/CPedIK.h>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <initializer_list>
#include <stdexcept>
#include <string>
namespace ped_fire_fixture
{
    [[noreturn]] inline void Unqualified(const char* method) noexcept
    {
        std::fprintf(stderr, "Unqualified fire fixture SDK call: %s\n", method);
        std::abort();
    }
}
#include "PedFireFixture.generated.h"
namespace
{
    using namespace mta::game_de;
    unsigned fireChecks = 0;
    void     CheckFire(bool ok, const char* why)
    {
        ++fireChecks;
        if (!ok)
            throw std::runtime_error(why);
    }
    struct FireAccesses : NativePedVitalsAccess,
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
                          NativePedFireAccess
    {
    };
    struct FireOwner final : FireAccesses
    {
        unsigned                reads = 0;
        NativePedReferenceToken received;
        ClockResult             result = ClockResult::Ok;
        bool                    nativePresent = false;
        ClockResult             ReadPedOnFire(const NativePedReferenceToken& token, bool& output) noexcept override
        {
            ++reads;
            received = token;
            output = nativePresent;
            return result;
        }
    };
    bool SameFireToken(const NativePedReferenceToken& a, const NativePedReferenceToken& b)
    {
        return a.version == b.version && a.ownerGeneration == b.ownerGeneration && a.slot == b.slot;
    }
}
static_assert(!MtaGame::CEntityFirePolicy::IsOnFire());
static_assert(std::is_same_v<decltype(&mta::game_de::CPedFireDE::IsOnFire), bool (mta::game_de::CPedFireDE::*)() const>);
int main()
{
    try
    {
        ped_fire_fixture::Game           game;
        FireOwner                        owner;
        NativePedReferenceToken          token{0x123456789abcdef0ull, 0xfedcba9876543210ull, 17};
        const auto                       expected = token;
        ped_fire_fixture::PedFireFixture ped(game, owner, owner, owner, owner, owner, owner, owner, owner, owner, owner, owner, owner, owner, owner, owner,
                                             owner, owner, owner, owner, token, 0x12345678);
        const CPed&                      sdk = ped;
        token = {};
        CheckFire(!owner.reads && ped.GetType() == 0x12345678,
                  "fire query construction copies borrowed immutable token, with no native or borrowed-game effects");
        for (const bool present : {false, true, false, true})
        {
            owner.nativePresent = present;
            const auto before = owner.reads;
            CheckFire(sdk.IsOnFire() == present && owner.reads == before + 1 && SameFireToken(owner.received, expected) && !ped.repairCalls,
                      "actual const CPed virtual dispatch observes fresh native presence once with exact immutable token and no fire/physics effects");
        }
        for (const auto status : {ClockResult::NotInitialized, ClockResult::WrongThread, ClockResult::BindingChanged, ClockResult::ReadFailure,
                                  ClockResult::OwnerConflict, ClockResult::Expired})
        {
            owner.result = status;
            const auto before = owner.reads;
            bool       refused = false;
            try
            {
                sdk.IsOnFire();
            }
            catch (const std::runtime_error& error)
            {
                refused = std::string(error.what()).find("IsOnFire") != std::string::npos;
            }
            CheckFire(refused && owner.reads == before + 1, "owner refusal throws once without inventing no-fire, caching a previous result or retrying");
        }
        const auto before = owner.reads;
        CheckFire(!static_cast<CEntity&>(ped).IsOnFire() && owner.reads == before,
                  "actual nonconst CEntity dispatch retains the classic false CPU policy without native observation even when that owner refuses");
        owner.result = ClockResult::Ok;
        owner.nativePresent = true;
        CPed& mutablePed = ped;
        CheckFire(mutablePed.IsOnFire() && owner.reads == before + 1, "mutable CPed static type still selects its const native association query");
        CPedFireDE& directFire = ped;
        CheckFire(directFire.IsOnFire() && ped.IsOnFire() && owner.reads == before + 3,
                  "direct Fire/derived class lookup hides the inherited nonconst fallback exactly as classic CPedSA");
        FireAccesses                     unavailable;
        ped_fire_fixture::PedFireFixture missing(game, unavailable, unavailable, unavailable, unavailable, unavailable, unavailable, unavailable, unavailable,
                                                 unavailable, unavailable, unavailable, unavailable, unavailable, unavailable, unavailable, unavailable,
                                                 unavailable, unavailable, unavailable, expected, 0);
        bool                             refused = false;
        try
        {
            static_cast<const CPed&>(missing).IsOnFire();
        }
        catch (const std::runtime_error&)
        {
            refused = true;
        }
        CheckFire(refused, "default fire owner cannot fabricate successful no-fire status");
        CheckFire(!static_cast<CEntity&>(missing).IsOnFire(), "engine-independent entity fallback remains available without a native fire owner");
        std::puts("Actual shared nonconst entity fire fallback SDK fixture completed");
        std::puts("Actual const ped fire association SDK fixture completed");
        std::printf("Passed %u ped fire SDK checks (64-bit; no native engine code executed)\n", fireChecks);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "FAIL %u: %s\n", fireChecks, error.what());
        return 1;
    }
}
