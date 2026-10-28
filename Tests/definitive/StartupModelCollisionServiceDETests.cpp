/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Exercise the actual startup repair service and default game hook
 ****************************************************************************/
// Reuse only strict generated SDK declarations and constructor definitions.
// The historical initialization/reset/policy matrices never execute here.
#pragma push_macro("main")
#undef main
#define main StartupCollisionHistoricalInitializationDefinitionsMain
#include "GameInitializationDETests.cpp"
#undef main
#pragma pop_macro("main")
#include "../../Client/game_de/CGameModelPreparationDE.h"

namespace StartupCollisionServiceTests
{
    using Pair = std::pair<std::uint32_t, std::uint32_t>;
    const std::vector<Pair> expected{{3118, 3059}, {3553, 3554}};
    template <class Operation>
    void RefusesExactly(Operation operation, ClockResult status)
    {
        bool refused = false;
        try
        {
            operation();
        }
        catch (const std::runtime_error& error)
        {
            const auto expectedMessage = status == ClockResult::NotInitialized
                                             ? std::string("DE startup collision repair requires the qualified native borrowed-pointer copy")
                                             : std::string("DE model owner refused startup collision repair: ") + std::to_string(static_cast<unsigned>(status));
            refused = error.what() == expectedMessage;
        }
        Check(refused, "the actual startup service must preserve the explicit missing-capability or owner-status refusal");
    }
    class Owner final : public NativeGameTimingAccess
    {
    public:
        ClockResult SetTimeScale(float) noexcept override { fixture::Unqualified(); }
        ClockResult SetMinuteDuration(std::uint32_t) noexcept override { fixture::Unqualified(); }
        ClockResult Pause(bool) noexcept override { fixture::Unqualified(); }
        ClockResult CopyStartupModelCollision(std::uint32_t target, std::uint32_t source) noexcept override
        {
            calls.emplace_back(target, source);
            return failAt == calls.size() ? status : ClockResult::Ok;
        }
        ClockResult       CreateAndPublishPedModelMetadata(std::uint32_t) noexcept override { fixture::Unqualified(); }
        std::vector<Pair> calls;
        std::size_t       failAt = 0;
        ClockResult       status = ClockResult::OriginalException;
    };
    class Game final : public fixture::GameInitializationFixtureBase
    {
    public:
        Game(const NativeGameTimingReader& timing, CClockDE& clock, NativeGameTimingAccess& owner, const NativeStreamingFileIdsReader& streaming,
             const NativeGameStateReader& state) noexcept
            : GameInitializationFixtureBase(timing, clock, owner, streaming, state)
        {
        }
        void        RunBrokenModels() { SetupBrokenModels(); }
        CGarages*   GetGarages() override { fixture::Unqualified(); }
        CStreaming* GetStreaming() override { fixture::Unqualified(); }

    protected:
        void RequireInitializationReady() override { fixture::Unqualified(); }
        void InitializeRenderer() override { fixture::Unqualified(); }
        void HideNativeMenu() override { fixture::Unqualified(); }
        // No collision override: the actual CGameInitializationDE default
        // dispatch must obtain NativeWrites and run the real service below.
    };
    static_assert(!std::is_abstract_v<Game>);
}

int main()
{
    namespace T = StartupCollisionServiceTests;
    T::Owner                owner;
    CGameModelPreparationDE service(owner);
    for (const auto pair : {T::Pair{0, 0xffffffff}, T::Pair{0xffffffff, 0}, T::Pair{0x10000, 0x80000001}})
    {
        owner.calls.clear();
        service.FixStartupModelCollision(pair.first, pair.second);
        Check(owner.calls == std::vector<T::Pair>{pair}, "actual service forwards both full unsigned keys once without facade lookup or narrowing");
    }
    for (const auto status :
         {ClockResult::InvalidArgument, ClockResult::NotInitialized, ClockResult::WrongThread, ClockResult::BindingChanged, ClockResult::ReadFailure,
          ClockResult::WriteFailure, ClockResult::RestoreFailure, ClockResult::PatchFailure, ClockResult::Busy, ClockResult::Expired,
          ClockResult::UnsupportedBuild, ClockResult::OwnerConflict, ClockResult::Stopped, ClockResult::OriginalException})
    {
        owner.calls.clear();
        owner.failAt = 1;
        owner.status = status;
        T::RefusesExactly([&] { service.FixStartupModelCollision(3118, 3059); }, status);
        Check(owner.calls == std::vector<T::Pair>{{3118, 3059}}, "service refusal preserves exactly one entered owner call without retries");
    }
    NativeModelClassificationAccess missing;
    CGameModelPreparationDE         unavailable(missing);
    T::RefusesExactly([&] { unavailable.FixStartupModelCollision(3118, 3059); }, ClockResult::NotInitialized);
    owner.calls.clear();
    owner.failAt = 0;
    service.SetupBrokenModels();
    Check(owner.calls == T::expected, "actual service keeps MTA's two directed repairs in their original order");
    NativeGameTimingReader       timing;
    NativeStreamingFileIdsReader streaming;
    NativeGameStateReader        state;
    ClockOwner                   clockOwner;
    CClockDE                     clock(clockOwner);
    T::Game                      game(timing, clock, owner, streaming, state);
    owner.calls.clear();
    game.RunBrokenModels();
    Check(owner.calls == T::expected, "actual game default hook delegates both repairs to its retained native writer and real service");
    owner.calls.clear();
    owner.failAt = 2;
    owner.status = ClockResult::Busy;
    T::RefusesExactly([&] { game.RunBrokenModels(); }, ClockResult::Busy);
    Check(owner.calls == T::expected, "second repair refusal retains the first accepted prefix without automatic replay or later calls");
    Check(!timing.IsBound() && !streaming.IsBound() && !state.IsBound(), "default-hook CPU dispatch grants no live native startup readiness");
    std::puts("Actual MTA startup model collision service fixture completed");
    std::printf("Passed %u startup model collision service checks (%zu-bit; actual default hook)\n", checks, sizeof(void*) * 8);
    return 0;
}
