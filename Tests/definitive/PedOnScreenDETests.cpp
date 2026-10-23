/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual inherited entity on-screen SDK query on one registered ped
 ****************************************************************************/
#include <SharedUtil.Defines.h>
#include "../../Client/game_de/CPedOnScreenDE.h"
#include <game/CGame.h>
#include <game/CPedIK.h>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <initializer_list>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace ped_on_screen_fixture
{
    [[noreturn]] inline void Unqualified(const char* method) noexcept
    {
        std::fprintf(stderr, "Unqualified on-screen fixture SDK call: %s\n", method);
        std::abort();
    }
}
#include "PedOnScreenFixture.generated.h"

namespace
{
    using namespace mta::game_de;
    unsigned checks = 0;
    void     Check(bool condition, const char* reason)
    {
        ++checks;
        if (!condition)
            throw std::runtime_error(reason);
    }
    struct OtherOwners : NativePedVitalsAccess,
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
                         NativePedGogglesAccess,
                         NativePedVisibilityAccess
    {
        unsigned    visibilityCalls = 0;
        ClockResult ReadPedVisible(const NativePedReferenceToken&, bool& output) noexcept override
        {
            ++visibilityCalls;
            output = false;
            return ClockResult::Ok;
        }
        ClockResult SetPedVisible(const NativePedReferenceToken&, bool) noexcept override
        {
            ++visibilityCalls;
            return ClockResult::Ok;
        }
    };
    struct OnScreenOwner final : NativePedOnScreenAccess
    {
        unsigned                calls = 0;
        bool                    value = false;
        ClockResult             result = ClockResult::Ok;
        NativePedReferenceToken received;
        std::function<void()>   afterRead;
        ClockResult             ReadPedOnScreen(const NativePedReferenceToken& token, bool& output) noexcept override
        {
            ++calls;
            received = token;
            // Stage even on refusal. The native operation owns any accepted
            // prefix; SDK dispatch must not publish it, retry or roll it back.
            output = value;
            if (afterRead)
                afterRead();
            return result;
        }
    };
    struct UnavailableOwner final : NativePedOnScreenAccess
    {
    };
    bool SameToken(const NativePedReferenceToken& first, const NativePedReferenceToken& second) noexcept
    {
        return first.version == second.version && first.ownerGeneration == second.ownerGeneration && first.slot == second.slot;
    }
    ped_on_screen_fixture::PedOnScreenFixture MakePed(CGame& game, OtherOwners& other, NativePedOnScreenAccess& screen, const NativePedReferenceToken& token,
                                                      std::uint32_t type, ped_on_screen_fixture::TaskScenario& scenario)
    {
        return ped_on_screen_fixture::PedOnScreenFixture(game, other, other, other, other, other, other, other, other, other, other, other, other, other, other,
                                                         other, other, other, other, other, other, other, other, screen, token, type, scenario);
    }
}

static_assert(std::is_abstract_v<mta::game_de::CPedOnScreenDE>);
static_assert(std::is_base_of_v<mta::game_de::CPedVisibilityDE, mta::game_de::CPedOnScreenDE>);
static_assert(std::is_same_v<decltype(&mta::game_de::CPedOnScreenDE::IsOnScreen), bool (mta::game_de::CPedOnScreenDE::*)()>);

int main()
{
    try
    {
        ped_on_screen_fixture::Game         game;
        ped_on_screen_fixture::TaskScenario scenario;
        scenario.available = true;
        OtherOwners              other;
        OnScreenOwner            first, second;
        NativePedOnScreenAccess* selected = &first;
        NativePedReferenceToken  token{0x123456789abcdef0ull, 0xfedcba9876543210ull, 17};
        const auto               expected = token;
        auto                     ped = MakePed(game, other, *selected, token, 0x12345678u, scenario);
        token = {};
        selected = &second;
        CEntity& entitySdk = ped;
        CPed&    pedSdk = ped;
        Check(!first.calls && !second.calls && !other.visibilityCalls && !scenario.eventCount && ped.GetType() == 0x12345678u,
              "construction copies CPU token/type and borrows owners without native, game, visibility or intelligence calls");
        for (const bool onScreen : {false, true, false, true})
        {
            first.value = onScreen;
            const auto before = first.calls;
            Check(entitySdk.IsOnScreen() == onScreen && first.calls == before + 1 && SameToken(first.received, expected),
                  "actual nonconst CEntity dispatch returns the fresh owner boolean once for the immutable registered ped");
            Check(pedSdk.IsOnScreen() == onScreen && first.calls == before + 2 && SameToken(first.received, expected),
                  "actual CPed static type inherits the same production on-screen query without an inverted or cached result");
            Check(!second.calls && !other.visibilityCalls && !scenario.eventCount,
                  "on-screen queries do not substitute raw IsVisible, IsFullyVisible, current-player, task or game fallbacks");
        }
        first.value = true;
        const auto beforeDirect = first.calls;
        Check(static_cast<CPedOnScreenDE&>(ped).IsOnScreen() && first.calls == beforeDirect + 1,
              "direct production and both common SDK paths select one genuine owner operation");
        for (const auto result :
             {ClockResult::InvalidArgument, ClockResult::NotInitialized, ClockResult::WrongThread, ClockResult::BindingChanged, ClockResult::ReadFailure,
              ClockResult::WriteFailure, ClockResult::RestoreFailure, ClockResult::PatchFailure, ClockResult::Busy, ClockResult::Expired,
              ClockResult::UnsupportedBuild, ClockResult::OwnerConflict, ClockResult::Stopped, ClockResult::OriginalException})
        {
            first.result = result;
            first.value = false;
            bool       output = true, refused = false;
            const auto before = first.calls;
            try
            {
                output = entitySdk.IsOnScreen();
            }
            catch (const std::runtime_error& error)
            {
                const std::string text(error.what());
                refused = text.find("IsOnScreen") != std::string::npos && text.find(std::to_string(static_cast<unsigned>(result))) != std::string::npos;
            }
            Check(refused && output && first.calls == before + 1 && SameToken(first.received, expected),
                  "every explicit owner refusal leaves caller output unchanged and names its result without successful false, retry or rollback");
            Check(!second.calls && !other.visibilityCalls && !scenario.eventCount, "refusal cannot become a raw visibility, task, intelligence or game query");
        }
        first.result = ClockResult::Ok;
        first.value = true;
        first.afterRead = [&]
        {
            token = {99, 100, 101};
            first.value = false;
            first.result = ClockResult::BindingChanged;
        };
        bool       output = false, callbackRefused = false;
        const auto beforeCallback = first.calls;
        try
        {
            output = pedSdk.IsOnScreen();
        }
        catch (const std::runtime_error& error)
        {
            callbackRefused = std::string(error.what()).find("IsOnScreen") != std::string::npos;
        }
        Check(callbackRefused && !output && first.calls == beforeCallback + 1 && SameToken(first.received, expected),
              "owner refusal after staging does not leak partial output or retry, and caller token mutation cannot rebind the wrapper");
        first.afterRead = {};
        first.result = ClockResult::Ok;
        const auto beforeFresh = first.calls;
        Check(!entitySdk.IsOnScreen() && first.calls == beforeFresh + 1 && SameToken(first.received, expected),
              "the next accepted off-screen result is fresh and remains distinct from the earlier refused operation");
        const NativePedReferenceToken nextToken{0x1111222233334444ull, 0xaaaabbbbccccddddull, 29};
        second.value = true;
        auto       another = MakePed(game, other, *selected, nextToken, 7u, scenario);
        const auto firstBeforeSecond = first.calls;
        Check(static_cast<CEntity&>(another).IsOnScreen() && second.calls == 1 && SameToken(second.received, nextToken) && first.calls == firstBeforeSecond,
              "another wrapper uses its own exact token and borrowed owner without global receiver or cache substitution");
        UnavailableOwner unavailable;
        auto             missing = MakePed(game, other, unavailable, expected, 0u, scenario);
        output = true;
        bool missingRefused = false;
        try
        {
            output = static_cast<CPed&>(missing).IsOnScreen();
        }
        catch (const std::runtime_error& error)
        {
            missingRefused = std::string(error.what()).find("IsOnScreen") != std::string::npos;
        }
        Check(missingRefused && output && !other.visibilityCalls && !scenario.eventCount,
              "default unavailable owner refuses instead of fabricating off-screen state or a complete native ped");
        std::puts("Actual ped on-screen SDK fixture completed");
        std::printf("Passed %u actual ped on-screen SDK checks (%zu-bit; no native engine code executed)\n", checks, sizeof(void*) * 8);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "Ped on-screen SDK failed after %u checks: %s\n", checks, error.what());
        return 1;
    }
}
