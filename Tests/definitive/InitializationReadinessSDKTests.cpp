/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Verify actual later Initialize admits its owner before the first stage
 ****************************************************************************/
#include "../../Client/game_de/CGameStartupInitializationDE.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace fixture
{
    [[noreturn]] inline void Unqualified() noexcept
    {
        std::fputs("FAIL: unrelated effect or native probe reached initialization readiness fixture\n", stderr);
        std::abort();
    }
}
#include "InitializationReadinessGameFixture.generated.h"

namespace
{
    using namespace mta::game_de;
    unsigned checks = 0;
    void     Check(bool condition, const char* why)
    {
        ++checks;
        if (!condition)
            throw std::runtime_error(why);
    }
    struct FirstStageReached final : std::exception
    {
    };
    struct Trace
    {
        void Add(unsigned value) noexcept
        {
            if (count == events.size())
                fixture::Unqualified();
            events[count++] = value;
        }
        void ExpectPreflightOnly()
        {
            Check(count == 1 && events[0] == 1, "owner refusal stops before the first startup stage");
            count = 0;
        }
        void ExpectPreflightThenStage()
        {
            Check(count == 2 && events[0] == 1 && events[1] == 2, "actual Initialize enters the aggregate preflight before its first startup stage");
            count = 0;
        }
        std::array<unsigned, 8> events{};
        std::size_t             count = 0;
    };
    struct OlderOwner : NativeGameTimingAccess
    {
        ClockResult SetTimeScale(float) noexcept override { fixture::Unqualified(); }
        ClockResult SetMinuteDuration(std::uint32_t) noexcept override { fixture::Unqualified(); }
        ClockResult Pause(bool) noexcept override { fixture::Unqualified(); }
        ClockResult GetStartupNumericGarages(CGarages*&) noexcept override { fixture::Unqualified(); }
        ClockResult GetStartupStreaming(CStreaming*&) noexcept override { fixture::Unqualified(); }
        ClockResult CreateAndPublishPedModelMetadata(std::uint32_t) noexcept override { fixture::Unqualified(); }
        ClockResult RequestStreamingSpecialModel(std::uint32_t, const char*, std::uint32_t) noexcept override { fixture::Unqualified(); }
        ClockResult CopyStartupModelCollision(std::uint32_t, std::uint32_t) noexcept override { fixture::Unqualified(); }
        ClockResult HideStartupNativeMenu() noexcept override { fixture::Unqualified(); }
        ClockResult RequireStartupNumericGaragesReady() noexcept override { fixture::Unqualified(); }
        ClockResult RequireStartupPedModelCreationReady() noexcept override { fixture::Unqualified(); }
        ClockResult RequireStartupStreamingReceiverReady() noexcept override { fixture::Unqualified(); }
        ClockResult RequireStartupSpecialModelRequestReady() noexcept override { fixture::Unqualified(); }
        ClockResult RequireStartupCollisionCopyReady() noexcept override { fixture::Unqualified(); }
        ClockResult RequireStartupNativeMenuReady() noexcept override { fixture::Unqualified(); }
    };
    struct Owner final : OlderOwner
    {
        explicit Owner(Trace& trace) noexcept : trace(trace) {}
        ClockResult RequireStartupInitializationReady() noexcept override
        {
            trace.Add(1);
            ++calls;
            return result;
        }
        Trace&      trace;
        unsigned    calls = 0;
        ClockResult result = ClockResult::Ok;
    };
    struct ClockOwner final : NativeClockAccess
    {
        ClockResult Read(ClockValues&) noexcept override { fixture::Unqualified(); }
        ClockResult Set(std::uint8_t, std::uint8_t) noexcept override { fixture::Unqualified(); }
        ClockResult Freeze(bool) noexcept override { fixture::Unqualified(); }
        ClockResult Frozen(bool&) const noexcept override { fixture::Unqualified(); }
    };
    class Game final : public fixture::InitializationReadinessGameFixtureBase
    {
    public:
        Game(const NativeGameTimingReader& timing, CClockDE& clock, NativeGameTimingAccess& owner, const NativeStreamingFileIdsReader& layout,
             const NativeGameStateReader& state, Trace& trace)
            : InitializationReadinessGameFixtureBase(timing, clock, owner, layout, state), trace(trace)
        {
        }
        using CGameStartupInitializationDE::RequireInitializationReady;
        CGarages* GetGarages() override
        {
            // Stop at the first existing SDK stage so the new preflight can
            // be tested without replaying forty-four accepted model effects.
            trace.Add(2);
            ++firstStages;
            throw FirstStageReached();
        }
        bool     HasRendererRecord() { return GetRenderPreparation().GetNoTexture() != nullptr; }
        unsigned firstStages = 0;

    private:
        Trace& trace;
    };
    struct Scenario
    {
        Trace                        trace;
        NativeGameTimingReader       timing;
        NativeStreamingFileIdsReader layout;
        NativeGameStateReader        state;
        ClockOwner                   clockOwner;
        CClockDE                     clock{clockOwner};
        Owner                        owner{trace};
        Game                         game{timing, clock, owner, layout, state, trace};
    };
    template <class Action>
    void Refuses(Action action, ClockResult result)
    {
        const auto expected = "DE CGame startup initialization unavailable (status " + std::to_string(static_cast<unsigned>(result)) + ")";
        try
        {
            action();
        }
        catch (const std::runtime_error& error)
        {
            Check(error.what() == expected, "the original aggregate owner refusal propagates through the actual SDK layer");
            return;
        }
        Check(false, "missing aggregate admission cannot become successful startup");
    }
    void ReachesFirstStage(CGame& sdk)
    {
        try
        {
            sdk.Initialize();
        }
        catch (const FirstStageReached&)
        {
            Check(true, "accepted production Initialize reaches the independently intercepted first stage");
            return;
        }
        Check(false, "the original initialization itinerary cannot silently skip its first stage");
    }
    void FreshPreflight()
    {
        Scenario scenario;
        Check(!scenario.owner.calls && !scenario.trace.count && !scenario.game.firstStages && !scenario.game.HasRendererRecord(),
              "actual SDK construction performs no admission, service lookup or renderer initialization");
        scenario.game.RequireInitializationReady();
        scenario.trace.ExpectPreflightOnly();
        scenario.owner.result = ClockResult::Busy;
        Refuses([&] { scenario.game.RequireInitializationReady(); }, ClockResult::Busy);
        scenario.trace.ExpectPreflightOnly();
        scenario.owner.result = ClockResult::Ok;
        scenario.game.RequireInitializationReady();
        scenario.trace.ExpectPreflightOnly();
        Check(scenario.owner.calls == 3 && !scenario.game.firstStages && !scenario.game.HasRendererRecord(),
              "every direct production preflight uses the fresh owner result without a success latch or effects");
        std::puts("Production actual initialization readiness SDK fixture completed");
    }
    void InitializeAdmissionOrder()
    {
        Scenario scenario;
        CGame&   sdk = scenario.game;
        scenario.owner.result = ClockResult::Busy;
        Refuses([&] { sdk.Initialize(); }, ClockResult::Busy);
        scenario.trace.ExpectPreflightOnly();
        Check(scenario.owner.calls == 1 && !scenario.game.firstStages && !scenario.game.HasRendererRecord(),
              "immediate owner refusal prevents garage lookup and all subsequent native or renderer effects");
        scenario.owner.result = ClockResult::Ok;
        ReachesFirstStage(sdk);
        scenario.trace.ExpectPreflightThenStage();
        Check(scenario.owner.calls == 2 && scenario.game.firstStages == 1 && !scenario.game.HasRendererRecord(),
              "accepted current admission precedes the original first stage and no later effect is reached");
        scenario.owner.result = ClockResult::BindingChanged;
        Refuses([&] { sdk.Initialize(); }, ClockResult::BindingChanged);
        scenario.trace.ExpectPreflightOnly();
        Check(scenario.owner.calls == 3 && scenario.game.firstStages == 1, "earlier accepted admission cannot bypass a later current-owner refusal");
        scenario.owner.result = ClockResult::Ok;
        ReachesFirstStage(sdk);
        scenario.trace.ExpectPreflightThenStage();
        Check(scenario.owner.calls == 4 && scenario.game.firstStages == 2 && !scenario.game.HasRendererRecord() && !scenario.timing.IsBound() &&
                  !scenario.layout.IsBound() && !scenario.state.IsBound(),
              "an explicit later attempt rechecks admission without native image reads or replaying later stages");
        OlderOwner older;
        Check(older.RequireStartupInitializationReady() == ClockResult::NotInitialized,
              "older owners explicitly refuse the newly required aggregate capability");
        Trace  oldTrace;
        Game   oldGame(scenario.timing, scenario.clock, older, scenario.layout, scenario.state, oldTrace);
        CGame& oldSdk = oldGame;
        Refuses([&] { oldSdk.Initialize(); }, ClockResult::NotInitialized);
        Check(!oldTrace.count && !oldGame.firstStages && !oldGame.HasRendererRecord(),
              "the actual new SDK preflight rejects an older owner before every startup stage");
        std::puts("Production actual initialization refusal before effects fixture completed");
    }
}
int main()
{
    try
    {
        static_assert(std::is_abstract_v<CGameStartupInitializationDE> && !std::is_abstract_v<Game>);
        static_assert(std::is_base_of_v<CGameStartupStreamingDE, CGameStartupInitializationDE>);
        static_assert(std::is_same_v<decltype(&Game::Initialize), void (CGameInitializationDE::*)()>);
        static_assert(std::is_same_v<decltype(&Game::RequireInitializationReady), void (CGameStartupInitializationDE::*)()>);
        static_assert(std::is_same_v<decltype(&Game::GetStreaming), CStreaming* (CGameStartupStreamingDE::*)()>);
        FreshPreflight();
        InitializeAdmissionOrder();
        std::printf("Passed %u initialization readiness SDK checks (%u-bit)\n", checks, static_cast<unsigned>(sizeof(void*) * 8));
        return EXIT_SUCCESS;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "Initialization readiness SDK failure after %u checks: %s\n", checks, error.what());
        return EXIT_FAILURE;
    }
}
