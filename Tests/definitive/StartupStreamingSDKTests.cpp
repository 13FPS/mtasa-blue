/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Exercise the real scoped streaming receiver and late CGame getter
 ****************************************************************************/
#include "../../Client/game_de/CStreamingStartupDE.h"
#include "../../Client/game_de/CGameStartupStreamingDE.h"
#include "../../Client/game_de/CGameModelPreparationDE.h"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace fixture
{
    [[noreturn]] inline void Unqualified() noexcept
    {
        std::fputs("FAIL: unrelated native/SDK operation reached startup streaming fixture\n", stderr);
        std::abort();
    }
}
#include "StartupStreamingGameFixture.generated.h"

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
    template <class Action>
    void Refuses(Action action, const std::string& expected)
    {
        try
        {
            action();
        }
        catch (const std::runtime_error& error)
        {
            Check(error.what() == expected, "the exact operation-specific refusal propagates");
            return;
        }
        Check(false, "unavailable/refused operations cannot report successful SDK values");
    }
    struct StreamingOwner final : NativeStreamingModelAccess
    {
        ClockResult RequestStreamingSpecialModel(std::uint32_t model, const char* name, std::uint32_t channel) noexcept override
        {
            ++specialCalls;
            lastModel = model;
            lastName = name;
            lastChannel = channel;
            return result;
        }
        ClockResult ReadStreamingModelLoaded(std::uint32_t model, NativeModelLoadedReader::Value& output) noexcept override
        {
            ++stateCalls;
            lastModel = model;
            output.rawLoadState = state;
            return result;
        }
        ClockResult   ReadStreamingModelGroups(NativeStreamingModelGroupsReader::Value&) noexcept override { fixture::Unqualified(); }
        ClockResult   LoadStreamingModels(bool) noexcept override { fixture::Unqualified(); }
        ClockResult   LoadStreamingScene(const CVector*) noexcept override { fixture::Unqualified(); }
        ClockResult   RemoveStreamingModel(std::uint32_t) noexcept override { fixture::Unqualified(); }
        ClockResult   RequestStreamingModel(std::uint32_t, std::uint32_t) noexcept override { fixture::Unqualified(); }
        ClockResult   RequestStreamingUpgrade(std::uint32_t, std::uint32_t) noexcept override { fixture::Unqualified(); }
        ClockResult   ReadStreamingUpgradeLoaded(std::uint32_t, bool&) noexcept override { fixture::Unqualified(); }
        ClockResult   LoadStreamingSceneCollision(const CVector*) noexcept override { fixture::Unqualified(); }
        unsigned      specialCalls = 0, stateCalls = 0;
        std::uint32_t lastModel = 0, lastChannel = 0;
        const char*   lastName = nullptr;
        std::uint8_t  state = 4;
        ClockResult   result = ClockResult::Ok;
    };
    std::string StreamingRefusal(const char* operation, ClockResult result)
    {
        return std::string("DE streaming owner refused ") + operation + ": " + std::to_string(static_cast<unsigned>(result));
    }
    void ScopedReceiver()
    {
        StreamingOwner      owner, other;
        CStreamingStartupDE receiver(owner);
        Check(receiver.IsOwnedBy(owner) && !receiver.IsOwnedBy(other) && !owner.specialCalls && !owner.stateCalls,
              "construction and immutable CPU association perform no native calls or qualification probes");
        CStreaming&                  sdk = receiver;
        const std::array<wchar_t, 4> path{{L'x', L'y', L'z', 0}};
        const auto                   beforePath = path;
        const auto unavailable = [](const char* operation) { return std::string("DE startup streaming operation is unavailable: ") + operation; };
        Refuses([&] { (void)sdk.GetStreamingInfo(0xffffffffu); }, unavailable("GetStreamingInfo"));
        Refuses([&] { sdk.ReinitStreaming(); }, unavailable("ReinitStreaming"));
        Refuses([&] { (void)sdk.AddArchive(path.data()); }, unavailable("AddArchive"));
        Refuses([&] { sdk.RemoveArchive(0xffu); }, unavailable("RemoveArchive"));
        Refuses([&] { sdk.SetStreamingInfo(0xffffffffu, 0xffu, 0xffffffffu, 0xffffu, 0xffffffffu); }, unavailable("SetStreamingInfo"));
        Refuses([&] { (void)sdk.SetStreamingBufferSize(0xffffffffu); }, unavailable("SetStreamingBufferSize"));
        Refuses([&] { (void)sdk.GetStreamingBufferSize(); }, unavailable("GetStreamingBufferSize"));
        Refuses([&] { sdk.MakeSpaceFor(0xffffffffu); }, unavailable("MakeSpaceFor"));
        Refuses([&] { (void)sdk.GetMemoryUsed(); }, unavailable("GetMemoryUsed"));
        Refuses([&] { sdk.RemoveBigBuildings(); }, unavailable("RemoveBigBuildings"));
        Check(!owner.specialCalls && !owner.stateCalls && path == beforePath,
              "all ten explicit refusals preserve caller storage and never dispatch a substitute native operation");

        // The owned subclass must reuse the accepted owner implementation,
        // including exact borrowed pointer/full-width intent and fresh refusal.
        const std::array<char, 3> name{{'a', 'b', 'c'}};
        sdk.RequestSpecialModel(0xffffffffu, name.data(), 0xfedcba98u);
        Check(
            owner.specialCalls == 1 && !owner.stateCalls && owner.lastModel == 0xffffffffu && owner.lastName == name.data() && owner.lastChannel == 0xfedcba98u,
            "the inherited real special request forwards the exact native intent once");
        owner.result = ClockResult::OwnerConflict;
        Refuses([&] { sdk.RequestSpecialModel(7, nullptr, 0); }, StreamingRefusal("RequestSpecialModel", owner.result));
        Check(owner.specialCalls == 2 && !owner.stateCalls && owner.lastModel == 7 && !owner.lastName && !owner.lastChannel,
              "request refusal propagates from one owner call without fallback or a success latch");
        owner.result = ClockResult::Ok;
        Check(sdk.GetModelLoadState(0xffffffffu) == eModelLoadState::LOADSTATE_FINISHING && owner.stateCalls == 1 && owner.lastModel == 0xffffffffu,
              "one ordinary value query remains the genuine inherited raw-state observation");
        owner.result = ClockResult::ReadFailure;
        Refuses([&] { (void)sdk.GetModelLoadState(42); }, StreamingRefusal("GetModelLoadState", owner.result));
        Check(owner.stateCalls == 2 && owner.lastModel == 42 && owner.specialCalls == 2,
              "a current value refusal cannot become fabricated absence or a request effect");
        NativeStreamingModelAccess older;
        CStreamingStartupDE        unavailableOwner(older);
        Refuses([&] { unavailableOwner.RequestSpecialModel(1, name.data(), 0); }, StreamingRefusal("RequestSpecialModel", ClockResult::NotInitialized));
    }
    struct ClockOwner final : NativeClockAccess
    {
        ClockResult Read(ClockValues&) noexcept override { fixture::Unqualified(); }
        ClockResult Set(std::uint8_t, std::uint8_t) noexcept override { fixture::Unqualified(); }
        ClockResult Freeze(bool) noexcept override { fixture::Unqualified(); }
        ClockResult Frozen(bool&) const noexcept override { fixture::Unqualified(); }
    };
    struct OlderOwner : NativeGameTimingAccess
    {
        ClockResult SetTimeScale(float) noexcept override { fixture::Unqualified(); }
        ClockResult SetMinuteDuration(std::uint32_t) noexcept override { fixture::Unqualified(); }
        ClockResult Pause(bool) noexcept override { fixture::Unqualified(); }
    };
    struct GameOwner final : OlderOwner
    {
        GameOwner() noexcept : receiver(*this) {}
        ClockResult GetStartupStreaming(CStreaming*& output) noexcept override
        {
            Add(2);
            ++calls;
            output = absent ? nullptr : &receiver;
            return result;
        }
        ClockResult RequireStartupStreamingReceiverReady() noexcept override
        {
            ++readinessCalls;
            fixture::Unqualified();
        }
        ClockResult CreateAndPublishPedModelMetadata(std::uint32_t model) noexcept override
        {
            Add(1);
            lastModel = model;
            if (createResult == ClockResult::Ok)
                ++publications;
            return createResult;
        }
        ClockResult RequestStreamingSpecialModel(std::uint32_t model, const char* name, std::uint32_t channel) noexcept override
        {
            Add(3);
            ++requests;
            lastModel = model;
            lastName = name;
            lastChannel = channel;
            return requestResult;
        }
        void Add(unsigned event) noexcept
        {
            if (eventCount == events.size())
                fixture::Unqualified();
            events[eventCount++] = event;
        }
        void Expect(std::initializer_list<unsigned> expected)
        {
            Check(eventCount == expected.size() && std::equal(expected.begin(), expected.end(), events.begin()),
                  "actual MakePedModel preserves publication, late SDK getter and request ordering without probes");
            eventCount = 0;
        }
        unsigned                 calls = 0;
        unsigned                 readinessCalls = 0, publications = 0, requests = 0;
        ClockResult              result = ClockResult::Ok;
        ClockResult              createResult = ClockResult::Ok, requestResult = ClockResult::Ok;
        bool                     absent = false;
        std::array<unsigned, 16> events{};
        std::size_t              eventCount = 0;
        std::uint32_t            lastModel = 0, lastChannel = 0;
        const char*              lastName = nullptr;
        CStreamingStartupDE      receiver;
    };
    class Game final : public fixture::StartupStreamingGameFixtureBase
    {
    public:
        using StartupStreamingGameFixtureBase::StartupStreamingGameFixtureBase;

    private:
        void RequireInitializationReady() override { fixture::Unqualified(); }
    };
    std::string GameRefusal(ClockResult result)
    {
        return "DE CGame get startup streaming unavailable (status " + std::to_string(static_cast<unsigned>(result)) + ")";
    }
    void BorrowedGameGetter()
    {
        NativeGameTimingReader       timing;
        NativeStreamingFileIdsReader layout;
        NativeGameStateReader        state;
        ClockOwner                   clockOwner;
        CClockDE                     clock(clockOwner);
        GameOwner                    owner;
        Game                         game(timing, clock, owner, layout, state);
        Check(!owner.calls && owner.receiver.IsOwnedBy(owner) && !timing.IsBound() && !layout.IsBound() && !state.IsBound(),
              "actual SDK construction retains one CPU receiver without a native read, getter or readiness effect");
        CGame& sdk = game;
        Check(sdk.GetStreaming() == &owner.receiver && sdk.GetStreaming() == &owner.receiver && owner.calls == 2,
              "actual inherited CGame getter borrows the same real owned receiver freshly on every selection");
        owner.result = ClockResult::Busy;
        Refuses([&] { (void)sdk.GetStreaming(); }, GameRefusal(owner.result));
        Check(owner.calls == 3, "current owner refusal cannot leak its assigned pointer or reuse previous success");
        owner.result = ClockResult::Ok;
        owner.absent = true;
        Refuses([&] { (void)sdk.GetStreaming(); }, GameRefusal(ClockResult::NotInitialized));
        Check(owner.calls == 4, "Ok with no concrete receiver remains explicitly uninitialized");
        owner.absent = false;
        Check(sdk.GetStreaming() == &owner.receiver && owner.calls == 5, "an explicit later selection can recover without replacing the owned receiver");
        OlderOwner  older;
        CStreaming* untouched = &owner.receiver;
        Check(older.GetStartupStreaming(untouched) == ClockResult::NotInitialized && untouched == &owner.receiver,
              "older owners retain explicit capability refusal and preserve borrowed caller output");
        Game oldGame(timing, clock, older, layout, state);
        Refuses([&] { (void)oldGame.GetStreaming(); }, GameRefusal(ClockResult::NotInitialized));
    }
    void LateStartupLookup()
    {
        NativeGameTimingReader       timing;
        NativeStreamingFileIdsReader layout;
        NativeGameStateReader        state;
        ClockOwner                   clockOwner;
        CClockDE                     clock(clockOwner);
        GameOwner                    owner;
        Game                         game(timing, clock, owner, layout, state);
        CGameModelPreparationDE      preparation(owner);
        Check(!owner.calls && !owner.readinessCalls && !owner.eventCount,
              "constructing the actual preparation and game performs no service lookup or readiness probe");
        const char name[] = "startup-special";
        auto       resolve = [&] { return game.GetStreaming(); };
        preparation.MakePedModel(0xffffffffu, name, resolve);
        owner.Expect({1, 2, 3});
        Check(owner.publications == 1 && owner.calls == 1 && owner.requests == 1 && owner.lastModel == 0xffffffffu && owner.lastName == name &&
                  !owner.lastChannel,
              "actual accepted startup reaches the unchanged owner request with original key, pointer and zero channel");

        owner.createResult = ClockResult::WriteFailure;
        Refuses([&] { preparation.MakePedModel(7, name, resolve); },
                "DE model owner refused ped metadata creation/publication: " + std::to_string(static_cast<unsigned>(owner.createResult)));
        owner.Expect({1});
        Check(owner.publications == 1 && owner.calls == 1 && owner.requests == 1,
              "failed metadata publication never resolves streaming and preserves the accepted request prefix");

        owner.createResult = ClockResult::Ok;
        owner.result = ClockResult::BindingChanged;
        Refuses([&] { preparation.MakePedModel(8, name, resolve); }, GameRefusal(owner.result));
        owner.Expect({1, 2});
        Check(owner.publications == 2 && owner.calls == 2 && owner.requests == 1,
              "failed late getter prevents the request while retaining already accepted model publication");

        owner.result = ClockResult::Ok;
        owner.requestResult = ClockResult::OwnerConflict;
        Refuses([&] { preparation.MakePedModel(9, name, resolve); }, StreamingRefusal("RequestSpecialModel", owner.requestResult));
        owner.Expect({1, 2, 3});
        Check(owner.publications == 3 && owner.calls == 3 && owner.requests == 2 && !owner.readinessCalls,
              "current request refusal preserves accepted publication and late selection with no preflight effects or retries");
        std::puts("Production actual late startup streaming getter fixture completed");
    }
}
int main()
{
    try
    {
        static_assert(std::is_abstract_v<CStreamingDE> && !std::is_abstract_v<CStreamingStartupDE>);
        static_assert(std::is_abstract_v<CGameStartupStreamingDE> && !std::is_abstract_v<Game>);
        static_assert(!std::is_copy_constructible_v<CStreamingStartupDE> && !std::is_move_constructible_v<CStreamingStartupDE>);
        static_assert(!std::has_virtual_destructor_v<CStreaming>);
        static_assert(std::is_same_v<decltype(&CStreamingStartupDE::RequestSpecialModel), decltype(&CStreamingDE::RequestSpecialModel)>);
        static_assert(std::is_same_v<decltype(&CStreamingStartupDE::GetModelLoadState), decltype(&CStreamingDE::GetModelLoadState)>);
        static_assert(std::is_same_v<decltype(&Game::GetStreaming), CStreaming* (CGameStartupStreamingDE::*)()>);
        ScopedReceiver();
        BorrowedGameGetter();
        LateStartupLookup();
        std::puts("Production actual startup streaming SDK fixture completed");
        std::printf("Passed %u startup streaming SDK checks (%u-bit)\n", checks, static_cast<unsigned>(sizeof(void*) * 8));
        return EXIT_SUCCESS;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "Startup streaming SDK failure after %u checks: %s\n", checks, error.what());
        return EXIT_FAILURE;
    }
}
