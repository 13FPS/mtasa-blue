/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual goggles-association SDK query without lifecycle/factory effects
 ****************************************************************************/
#include <SharedUtil.Defines.h>
#include "../../Client/game_de/CPedGogglesDE.h"
#include <game/CGame.h>
#include <game/CPedIK.h>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <initializer_list>
#include <stdexcept>
#include <string>
namespace ped_goggles_fixture
{
    [[noreturn]] inline void Unqualified(const char* method) noexcept
    {
        std::fprintf(stderr, "Unqualified goggles fixture SDK call: %s\n", method);
        std::abort();
    }
}
#include "PedGogglesFixture.generated.h"
namespace
{
    using namespace mta::game_de;
    unsigned gogglesChecks = 0;
    void     CheckGoggles(bool condition, const char* reason)
    {
        ++gogglesChecks;
        if (!condition)
            throw std::runtime_error(reason);
    }
    struct GogglesAccesses : NativePedVitalsAccess,
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
                             NativePedCollisionAccess,
                             NativePedGogglesAccess
    {
    };
    struct GogglesOwner final : GogglesAccesses
    {
        unsigned                reads = 0;
        NativePedReferenceToken received;
        ClockResult             result = ClockResult::Ok;
        bool                    present = false;
        std::function<void()>   onRead;
        ClockResult             ReadPedWearingGoggles(const NativePedReferenceToken& token, bool& output) noexcept override
        {
            ++reads;
            received = token;
            output = present;
            if (onRead)
                onRead();
            return result;
        }
    };
    bool SameToken(const NativePedReferenceToken& first, const NativePedReferenceToken& second)
    {
        return first.version == second.version && first.ownerGeneration == second.ownerGeneration && first.slot == second.slot;
    }
}
static_assert(std::is_same_v<decltype(&mta::game_de::CPedGogglesDE::IsWearingGoggles), bool (mta::game_de::CPedGogglesDE::*)() const>);
int main()
{
    try
    {
        ped_goggles_fixture::Game         game;
        GogglesOwner                      owner;
        ped_goggles_fixture::TaskScenario taskScenario;
        taskScenario.available = true;
        NativePedReferenceToken                token{0x123456789abcdef0ull, 0xfedcba9876543210ull, 17};
        const auto                             expected = token;
        ped_goggles_fixture::PedGogglesFixture ped(game, owner, owner, owner, owner, owner, owner, owner, owner, owner, owner, owner, owner, owner, owner,
                                                   owner, owner, owner, owner, owner, owner, owner, token, 0x12345678, taskScenario);
        token = {};
        const CPed& sdk = ped;
        CheckGoggles(!owner.reads && !taskScenario.eventCount && ped.GetType() == 0x12345678,
                     "construction copies immutable token and CPU type without native, game or inherited intelligence effects");
        for (const bool present : {false, true, false, true})
        {
            owner.present = present;
            const auto before = owner.reads;
            CheckGoggles(sdk.IsWearingGoggles() == present && owner.reads == before + 1 && SameToken(owner.received, expected),
                         "actual const CPed dispatch observes fresh associated-object presence once with exact immutable token");
            CheckGoggles(!taskScenario.eventCount, "goggles identity never consults inherited intelligence/task availability or getters");
        }
        owner.present = true;
        const auto     directBefore = owner.reads;
        CPedGogglesDE& direct = ped;
        CPed&          mutableSdk = ped;
        CheckGoggles(direct.IsWearingGoggles() && mutableSdk.IsWearingGoggles() && owner.reads == directBefore + 2,
                     "direct production and mutable CPed static types select the same actual const predicate without a duplicate overload");
        const auto beforeEntity = owner.reads;
        CheckGoggles(!static_cast<CEntity&>(ped).IsOnFire() && owner.reads == beforeEntity && !taskScenario.eventCount,
                     "new class retains the unrelated real nonconst entity fire CPU policy without aliasing goggles or tasks");
        for (const auto status :
             {ClockResult::InvalidArgument, ClockResult::NotInitialized, ClockResult::WrongThread, ClockResult::BindingChanged, ClockResult::ReadFailure,
              ClockResult::WriteFailure, ClockResult::RestoreFailure, ClockResult::PatchFailure, ClockResult::Busy, ClockResult::Expired,
              ClockResult::UnsupportedBuild, ClockResult::OwnerConflict, ClockResult::Stopped, ClockResult::OriginalException})
        {
            owner.result = status;
            owner.present = true;
            const auto before = owner.reads;
            bool       refused = false;
            try
            {
                sdk.IsWearingGoggles();
            }
            catch (const std::runtime_error& error)
            {
                refused = std::string(error.what()).find("IsWearingGoggles") != std::string::npos;
            }
            CheckGoggles(refused && owner.reads == before + 1 && SameToken(owner.received, expected) && !taskScenario.eventCount,
                         "every owner refusal throws after exactly one read without a successful false, cached value, retry or task fallback");
        }
        owner.result = ClockResult::Ok;
        owner.present = false;
        owner.onRead = [&token] { token = {99, 100, 101}; };
        CheckGoggles(!sdk.IsWearingGoggles() && SameToken(owner.received, expected),
                     "post-construction caller token changes cannot redirect the borrowed wrapper association");
        GogglesAccesses                        unavailable;
        ped_goggles_fixture::TaskScenario      missingScenario;
        ped_goggles_fixture::PedGogglesFixture missing(game, unavailable, unavailable, unavailable, unavailable, unavailable, unavailable, unavailable,
                                                       unavailable, unavailable, unavailable, unavailable, unavailable, unavailable, unavailable, unavailable,
                                                       unavailable, unavailable, unavailable, unavailable, unavailable, unavailable, expected, 0,
                                                       missingScenario);
        bool                                   refused = false;
        try
        {
            static_cast<const CPed&>(missing).IsWearingGoggles();
        }
        catch (const std::runtime_error&)
        {
            refused = true;
        }
        CheckGoggles(refused && !missingScenario.eventCount, "default owner refuses rather than inventing an absent goggles object or probing task state");
        CheckGoggles(!static_cast<CEntity&>(missing).IsOnFire(),
                     "existing engine-independent nonconst entity fallback remains independent of native goggles availability");
        std::puts("Actual ped goggles association SDK fixture completed");
        std::printf("Passed %u ped goggles SDK checks (64-bit; no native engine code executed)\n", gogglesChecks);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "FAIL %u: %s\n", gogglesChecks, error.what());
        return 1;
    }
}
