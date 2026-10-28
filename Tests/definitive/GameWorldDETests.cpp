/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual shared world lifecycle and CGame SDK dispatch in isolation
 *****************************************************************************/
#include <SharedUtil.Defines.h>
#include "../../Client/game_de/CGameWorldDE.h"
#include "../../Client/game_de/CWeaponStatManagerDE.h"
#include <game/CPools.h>
#include <game/CDummyPool.h>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <limits>
#include <stdexcept>
#include <vector>
namespace world_fixture
{
    [[noreturn]] inline void Unqualified(const char* method) noexcept
    {
        std::fprintf(stderr, "Unqualified world fixture call: %s\n", method);
        std::abort();
    }
}
#include "GameWorldFixture.generated.h"
namespace
{
    using namespace mta::game_de;
    using Policy = MtaGame::CGameWorldPolicy;
    unsigned checks = 0;
    void     Check(bool value, const char* reason)
    {
        ++checks;
        if (!value)
            throw std::runtime_error(reason);
    }
    struct Failure : std::runtime_error
    {
        explicit Failure(int value) : std::runtime_error("existing world owner refused"), at(value) {}
        int at;
    };
    template <class Operation>
    void Fails(Operation&& operation, int at)
    {
        try
        {
            operation();
        }
        catch (const Failure& failure)
        {
            Check(failure.at == at, "original world owner exception propagated");
            return;
        }
        Check(false, "world owner failure must not become success");
    }
    template <class Operation>
    void Refuses(Operation&& operation)
    {
        try
        {
            operation();
        }
        catch (const std::runtime_error&)
        {
            Check(true, "missing service refused");
            return;
        }
        Check(false, "missing world service must not silently succeed");
    }
    enum Event
    {
        DisableIpl,
        Covers,
        Plants,
        Shadows,
        BackupDummies,
        BackupBuildings,
        Caches,
        RestoreBuildings,
        RestoreDummies,
        EnableIpl,
        GetSize,
        Resize
    };
    const std::vector<int> Removal{DisableIpl, Covers, Plants, Shadows, BackupDummies, BackupBuildings, Caches};
    const std::vector<int> Restoration{RestoreBuildings, RestoreDummies, EnableIpl};
    struct Trace
    {
        std::vector<int>         events;
        std::function<void(int)> inside;
        bool                     ready = true, inEffect = false;
        unsigned                 generation = 1, admitted = 1, preflights = 0;
        int                      failAt = -1;
        void                     Current() const
        {
            if (!ready || generation != admitted)
                throw Failure(-2);
        }
        void Preflight()
        {
            ++preflights;
            if (!ready || inEffect)
                throw Failure(-1);
            admitted = generation;
        }
        void Call(int event)
        {
            Current();
            const auto position = static_cast<int>(events.size());
            events.push_back(event);
            // The owner controls reentry, not a second SDK-local lock/lease.
            struct Restore
            {
                bool& value;
                bool  before;
                ~Restore() { value = before; }
            } restore{inEffect, inEffect};
            inEffect = event != GetSize;
            if (inside)
                inside(event);
            if (position == failAt)
                throw Failure(position);
            Current();
        }
    };
    void Equal(const std::vector<int>& actual, const std::vector<int>& expected)
    {
        Check(actual.size() == expected.size(), "world operation trace length");
        for (std::size_t index = 0; index < expected.size(); ++index)
            Check(actual[index] == expected[index], "world operation exact ordering");
    }
    std::vector<int> Prefix(const std::vector<int>& values, std::size_t size)
    {
        return {values.begin(), values.begin() + size};
    }
    void FilterChecks(Policy::IplFilter filter)
    {
        const Policy::IplName barriers{'b', 'a', 'r', 'r', 'i', 'e', 'r', 's'};
        Check(filter != nullptr && !filter(barriers), "exact eight-byte barriers name alone is suppressed");
        for (std::size_t byte = 0; byte < barriers.size(); ++byte)
            for (unsigned value = 0; value < 256; ++value)
            {
                auto changed = barriers;
                changed[byte] = static_cast<unsigned char>(value);
                Check(filter(changed) == (value != barriers[byte]), "barrier comparison preserves all eight raw bytes and case");
            }
        Check(filter(Policy::IplName{}), "empty raw IPL name remains eligible");
    }
    void SharedChecks()
    {
        FilterChecks(&Policy::ShouldRestoreIpl);
        for (int failed = -1; failed < static_cast<int>(Removal.size()); ++failed)
        {
            Trace trace;
            trace.failAt = failed;
            bool       removed = false;
            const auto remove = [&]
            {
                Policy::Remove([&] { trace.Call(DisableIpl); }, [&] { trace.Call(Covers); }, [&] { trace.Call(Plants); }, [&] { trace.Call(Shadows); },
                               [&] { trace.Call(BackupDummies); }, [&] { trace.Call(BackupBuildings); }, [&] { trace.Call(Caches); },
                               [&](bool value) { removed = value; });
            };
            if (failed < 0)
                remove();
            else
                Fails(remove, failed);
            Equal(trace.events, Prefix(Removal, failed < 0 ? Removal.size() : static_cast<std::size_t>(failed + 1)));
            Check(removed == (failed < 0), "remove phase commits only after all successful effects");
        }
        for (int failed = -1; failed < static_cast<int>(Restoration.size()); ++failed)
        {
            Trace trace;
            trace.failAt = failed;
            bool       removed = true;
            const auto restore = [&]
            {
                Policy::Restore([&] { trace.Call(RestoreBuildings); }, [&] { trace.Call(RestoreDummies); },
                                [&](Policy::IplFilter filter)
                                {
                                    Check(!filter(Policy::IplName{'b', 'a', 'r', 'r', 'i', 'e', 'r', 's'}), "restore supplies genuine filter");
                                    trace.Call(EnableIpl);
                                },
                                [&](bool value) { removed = value; });
            };
            if (failed < 0)
                restore();
            else
                Fails(restore, failed);
            Equal(trace.events, Prefix(Restoration, failed < 0 ? Restoration.size() : static_cast<std::size_t>(failed + 1)));
            Check(removed == (failed >= 0), "restore phase commits only after all successful effects");
        }
        const auto maximum = static_cast<std::size_t>(std::numeric_limits<int>::max());
        for (const auto requested : {maximum + 1, std::numeric_limits<std::size_t>::max()})
        {
            unsigned   calls = 0;
            const auto result = Policy::SetBuildingPoolSize(
                requested, false,
                [&]
                {
                    ++calls;
                    return 1;
                },
                [&] { ++calls; }, [&] { ++calls; },
                [&](int)
                {
                    ++calls;
                    return true;
                },
                [&] { ++calls; });
            Check(!result && !calls, "oversize returns false before any virtual receiver/callback");
        }
        for (const bool removed : {false, true})
            for (const int current : {-1, 0, 17})
                for (const std::size_t requested : {std::size_t{0}, std::size_t{17}, maximum})
                    for (const bool resizeResult : {false, true})
                    {
                        std::vector<int> order;
                        int              received = -1;
                        const auto       result = Policy::SetBuildingPoolSize(
                            requested, removed,
                            [&]
                            {
                                order.push_back(GetSize);
                                return current;
                            },
                            [&] { order.push_back(DisableIpl); }, [&] { order.push_back(Caches); },
                            [&](int value)
                            {
                                received = value;
                                order.push_back(Resize);
                                return resizeResult;
                            },
                            [&] { order.push_back(EnableIpl); });
                        const bool same = removed && current >= 0 && static_cast<std::size_t>(current) == requested;
                        Equal(order, same      ? std::vector<int>{GetSize, Caches}
                                     : removed ? std::vector<int>{GetSize, Caches, Resize}
                                               : std::vector<int>{GetSize, DisableIpl, Resize, EnableIpl});
                        Check(result == (same || resizeResult), "cache-only success versus exact resize bool result");
                        Check(same ? received == -1 : received == static_cast<int>(requested), "no default capacity or signed-negative coercion");
                    }
        bool     phase = false;
        unsigned removals = 0, restores = 0;
        Policy::SetBuildingPoolSize(
            17, phase,
            [&]
            {
                phase = true;
                return 17;
            },
            [&] { ++removals; }, [] {}, [](int) { return false; }, [&] { ++restores; });
        Check(phase && removals == 1 && restores == 1, "active decision captured before fresh size dispatch/reentry");
        phase = true;
        removals = restores = 0;
        Policy::SetBuildingPoolSize(
            17, phase,
            [&]
            {
                phase = false;
                return 18;
            },
            [&] { ++removals; }, [] {}, [](int) { return true; }, [&] { ++restores; });
        Check(!phase && !removals && !restores, "removed decision also remains captured before size dispatch");
        std::puts("Actual shared CGame world lifecycle policy fixture completed");
    }
    class Buildings final : public world_fixture::BuildingsFixtureBase
    {
    public:
        explicit Buildings(Trace& value) : trace(value) {}
        int  size = 17, lastCapacity = -1;
        bool resizeResult = false;
        int  GetSize() const override
        {
            trace.Call(::GetSize);
            return size;
        }
        bool Resize(int capacity) override
        {
            lastCapacity = capacity;
            trace.Call(::Resize);
            return resizeResult;
        }
        void RemoveAllWithBackup() override { trace.Call(BackupBuildings); }
        void RestoreBackup() override { trace.Call(RestoreBuildings); }

    private:
        Trace& trace;
    };
    class Dummies final : public world_fixture::DummiesFixtureBase
    {
    public:
        explicit Dummies(Trace& value) : trace(value) {}
        void RemoveAllWithBackup() override { trace.Call(BackupDummies); }
        void RestoreBackup() override { trace.Call(RestoreDummies); }

    private:
        Trace& trace;
    };
    class Pools final : public world_fixture::PoolsFixtureBase
    {
    public:
        Pools(Buildings& buildingValue, Dummies& dummyValue) : buildings(buildingValue), dummies(dummyValue) {}
        CBuildingsPool& GetBuildingsPool() noexcept override
        {
            ++buildingReads;
            return buildings;
        }
        CDummyPool& GetDummyPool() noexcept override
        {
            ++dummyReads;
            return dummies;
        }
        unsigned buildingReads = 0, dummyReads = 0;

    private:
        Buildings& buildings;
        Dummies&   dummies;
    };
    class ClockOwner final : public NativeClockAccess
    {
        ClockResult Read(ClockValues&) noexcept override { world_fixture::Unqualified("clock Read"); }
        ClockResult Set(std::uint8_t, std::uint8_t) noexcept override { world_fixture::Unqualified("clock Set"); }
        ClockResult Freeze(bool) noexcept override { world_fixture::Unqualified("clock Freeze"); }
        ClockResult Frozen(bool&) const noexcept override { world_fixture::Unqualified("clock Frozen"); }
    };
    class ConstructorOwner final : public NativeGameTimingAccess
    {
    public:
        ClockResult SetTimeScale(float) noexcept override { world_fixture::Unqualified("time scale"); }
        ClockResult SetMinuteDuration(std::uint32_t) noexcept override { world_fixture::Unqualified("minute duration"); }
        ClockResult Pause(bool) noexcept override { world_fixture::Unqualified("pause"); }
        ClockResult RequireConstructorWeapons(const CClock*, CWeaponStatManager*& output) noexcept override
        {
            ++consumptions;
            output = manager;
            return ClockResult::Ok;
        }
        ClockResult PrepareConstructorWeapons(NativeWeaponConstructorProgress&) noexcept override
        {
            ++preparations;
            return ClockResult::NotInitialized;
        }
        CWeaponStatManager* manager = nullptr;
        unsigned            consumptions = 0, preparations = 0;
    };
    class Game final : public world_fixture::GameWorldFixtureBase
    {
    public:
        Game(const NativeGameTimingReader& reader, CClockDE& clock, NativeGameTimingAccess& timing, const NativeStreamingFileIdsReader& streaming,
             const NativeGameStateReader& state, Trace& value) noexcept
            : GameWorldFixtureBase(reader, clock, timing, streaming, state), trace(value)
        {
        }
        Game(const NativeGameTimingReader& reader, CClockDE& clock, NativeGameTimingAccess& timing, const NativeStreamingFileIdsReader& streaming,
             const NativeGameStateReader& state, Trace& value, ConstructorServices prepared)
            : GameWorldFixtureBase(reader, clock, timing, streaming, state, prepared), trace(value)
        {
        }
        CPools*          pools = nullptr;
        mutable unsigned poolReads = 0;
        bool             lastFilterAccepted = false;
        CPools*          GetPools() const noexcept override
        {
            ++poolReads;
            return pools;
        }
        bool Removed() const noexcept { return IsGameWorldRemoved(); }

    protected:
        void RequireWorldLifecycleReady() override
        {
            trace.Preflight();
            if (!pools)
                throw Failure(-1);
        }
        void DisableWorldIplStreaming() override { trace.Call(DisableIpl); }
        void RemoveWorldCovers() override { trace.Call(Covers); }
        void RemoveWorldPlants() override { trace.Call(Plants); }
        void DestroyWorldStencilShadows() override { trace.Call(Shadows); }
        void DropWorldBuildingCaches() override { trace.Call(Caches); }
        void EnableWorldIplStreaming(Policy::IplFilter filter) override
        {
            lastFilterAccepted = filter && !filter(Policy::IplName{'b', 'a', 'r', 'r', 'i', 'e', 'r', 's'}) && filter(Policy::IplName{});
            Check(lastFilterAccepted, "actual backend hook receives exact barrier predicate");
            trace.Call(EnableIpl);
        }
        void RequireResetReady() override { world_fixture::Unqualified("Reset readiness"); }
        void ResetVehicleModelDummies() override { world_fixture::Unqualified("model dummies"); }
        void RestoreObjectModelGroups() override { world_fixture::Unqualified("model groups"); }
        void RestoreDefaultObjectGroupProperties() override { world_fixture::Unqualified("object properties"); }
        void ResetVehicleModelWheelSizes() override { world_fixture::Unqualified("wheel sizes"); }
        void ResetModelTextureDictionaries() override { world_fixture::Unqualified("texture dictionaries"); }
        int  GetResetBuildingPoolCapacity() const override { world_fixture::Unqualified("default capacity"); }
        void RequireInitializationReady() override { world_fixture::Unqualified("Initialize readiness"); }
        void FixStartupModelCollision(std::uint32_t target, std::uint32_t source) override
        {
            (void)target;
            (void)source;
            world_fixture::Unqualified("startup collision");
        }
        void InitializeRenderer() override { world_fixture::Unqualified("renderer"); }
        void HideNativeMenu() override { world_fixture::Unqualified("menu"); }

    private:
        Trace& trace;
    };
    struct Context
    {
        NativeGameTimingReader       reader;
        NativeStreamingFileIdsReader streaming;
        NativeGameStateReader        nativeState;
        ClockOwner                   clockOwner;
        CClockDE                     clock{clockOwner};
        ConstructorOwner             constructor;
        Trace                        trace;
        Buildings                    buildings{trace};
        Dummies                      dummies{trace};
        Pools                        pools{buildings, dummies};
        Game                         game{reader, clock, constructor, streaming, nativeState, trace};
        Context() { game.pools = &pools; }
        CGame& Sdk() { return game; }
    };
    void SdkChecks()
    {
        Context context;
        auto&   trace = context.trace;
        auto&   game = context.game;
        auto&   sdk = context.Sdk();
        Check(!game.Removed() && trace.events.empty() && !trace.preflights && !context.constructor.consumptions,
              "construction has no world effects or native preparation");
        for (unsigned attempt = 0; attempt < 3; ++attempt)
        {
            trace.events.clear();
            sdk.RemoveGameWorld();
            Equal(trace.events, Removal);
            Check(game.Removed(), "completed remove commits phase");
        }
        for (unsigned attempt = 0; attempt < 3; ++attempt)
        {
            trace.events.clear();
            sdk.RestoreGameWorld();
            Equal(trace.events, Restoration);
            Check(!game.Removed(), "completed restore commits active phase");
        }
        for (int failed = 0; failed < static_cast<int>(Removal.size()); ++failed)
        {
            trace.events.clear();
            trace.failAt = failed;
            Fails([&] { sdk.RemoveGameWorld(); }, failed);
            Equal(trace.events, Prefix(Removal, static_cast<std::size_t>(failed + 1)));
            Check(!game.Removed(), "partial remove retains previous active phase; no rollback/retry");
        }
        trace.failAt = -1;
        trace.events.clear();
        sdk.RemoveGameWorld();
        for (int failed = 0; failed < static_cast<int>(Restoration.size()); ++failed)
        {
            trace.events.clear();
            trace.failAt = failed;
            Fails([&] { sdk.RestoreGameWorld(); }, failed);
            Equal(trace.events, Prefix(Restoration, static_cast<std::size_t>(failed + 1)));
            Check(game.Removed(), "partial restore retains removed phase");
        }
        trace.failAt = -1;
        trace.events.clear();
        sdk.RestoreGameWorld();
        trace.events.clear();
        const auto preflights = trace.preflights;
        game.pools = nullptr;
        trace.ready = false;
        Check(!sdk.SetBuildingPoolSize(std::numeric_limits<std::size_t>::max()) && trace.preflights == preflights && trace.events.empty(),
              "oversize precedes unavailable owner and null receiver");
        Fails([&] { sdk.RemoveGameWorld(); }, -1);
        Check(trace.events.empty(), "missing owner refuses before first effect");
        trace.ready = true;
        Fails([&] { sdk.RestoreGameWorld(); }, -1);
        Check(trace.events.empty(), "missing required pools cannot synthesize success");
        game.pools = &context.pools;
        trace.inside = [&](int kind)
        {
            if (kind == Shadows)
                game.pools = nullptr;
        };
        Refuses([&] { sdk.RemoveGameWorld(); });
        Equal(trace.events, Prefix(Removal, 4));
        Check(!game.Removed(), "late receiver loss retains exact accepted prefix");
        trace.inside = {};
        game.pools = &context.pools;
        trace.events.clear();
        bool reentryRefused = false;
        trace.inside = [&](int kind)
        {
            if (kind == Plants)
            {
                Fails([&] { sdk.RemoveGameWorld(); }, -1);
                reentryRefused = true;
            }
        };
        sdk.RemoveGameWorld();
        Equal(trace.events, Removal);
        Check(reentryRefused && game.Removed(), "existing owner rejects effect reentry without SDK-local owner or replay");
        trace.inside = {};
        sdk.RestoreGameWorld();
        trace.events.clear();
        trace.inside = [&](int kind)
        {
            if (kind == Plants)
                ++trace.generation;
        };
        Fails([&] { sdk.RemoveGameWorld(); }, -2);
        Equal(trace.events, Prefix(Removal, 3));
        Check(!game.Removed(), "late generation loss retains effects and phase");
        trace.inside = {};
        trace.events.clear();
        const std::vector<int> activeResize{GetSize,         DisableIpl, Covers, Plants,           Shadows,        BackupDummies,
                                            BackupBuildings, Caches,     Resize, RestoreBuildings, RestoreDummies, EnableIpl};
        for (const int current : {-1, 0, 17})
            for (const std::size_t requested : {std::size_t{0}, std::size_t{17}, static_cast<std::size_t>(std::numeric_limits<int>::max())})
                for (const bool resizeResult : {false, true})
                {
                    trace.events.clear();
                    context.buildings.size = current;
                    context.buildings.resizeResult = resizeResult;
                    Check(sdk.SetBuildingPoolSize(requested) == resizeResult, "active resize returns exact bool including false");
                    Equal(trace.events, activeResize);
                    Check(!game.Removed() && context.buildings.lastCapacity == static_cast<int>(requested),
                          "active same/negative/zero/max branch restores exact capacity intent");
                }
        for (int failed = 0; failed < static_cast<int>(activeResize.size()); ++failed)
        {
            Context failure;
            failure.trace.failAt = failed;
            Fails([&] { failure.Sdk().SetBuildingPoolSize(17); }, failed);
            Equal(failure.trace.events, Prefix(activeResize, static_cast<std::size_t>(failed + 1)));
            Check(failure.game.Removed() == (failed >= 8), "resize failure phase matches only fully completed remove/restore sequences");
        }
        trace.events.clear();
        sdk.RemoveGameWorld();
        trace.events.clear();
        context.buildings.size = 17;
        Check(sdk.SetBuildingPoolSize(17), "already removed same-size returns true independent of prior resize false");
        Equal(trace.events, {GetSize, Caches});
        Check(game.Removed(), "cache-only same-size leaves removed phase");
        for (const int current : {-1, 0, 18})
            for (const bool resizeResult : {false, true})
            {
                trace.events.clear();
                context.buildings.size = current;
                context.buildings.resizeResult = resizeResult;
                Check(sdk.SetBuildingPoolSize(17) == resizeResult, "removed different/negative capacity forwards resize result");
                Equal(trace.events, {GetSize, Caches, Resize});
                Check(game.Removed(), "removed resize does not restore implicitly");
            }
        for (const bool same : {false, true})
            for (int failed = 0; failed < (same ? 2 : 3); ++failed)
            {
                Context failure;
                failure.Sdk().RemoveGameWorld();
                failure.trace.events.clear();
                failure.buildings.size = same ? 17 : 18;
                failure.trace.failAt = failed;
                Fails([&] { failure.Sdk().SetBuildingPoolSize(17); }, failed);
                Equal(failure.trace.events,
                      Prefix(same ? std::vector<int>{GetSize, Caches} : std::vector<int>{GetSize, Caches, Resize}, static_cast<std::size_t>(failed + 1)));
                Check(failure.game.Removed(), "already-removed cache/resize failure never invents restore or phase transition");
            }
        sdk.RestoreGameWorld();
        trace.events.clear();
        context.buildings.size = 17;
        bool reentered = false;
        trace.inside = [&](int kind)
        {
            if (kind == GetSize && !reentered)
            {
                reentered = true;
                sdk.RemoveGameWorld();
            }
        };
        Check(sdk.SetBuildingPoolSize(17) == context.buildings.resizeResult, "reentered GetSize retains captured active resize result");
        auto expected = std::vector<int>{GetSize};
        expected.insert(expected.end(), Removal.begin(), Removal.end());
        expected.insert(expected.end(), Removal.begin(), Removal.end());
        expected.push_back(Resize);
        expected.insert(expected.end(), Restoration.begin(), Restoration.end());
        Equal(trace.events, expected);
        Check(reentered && !game.Removed(), "fresh size callback cannot rewrite already-captured outer active itinerary");
        trace.inside = {};

        NativeWeaponInfoReader weaponReader;
        CWeaponInfoBankDE      bank(weaponReader, context.constructor, context.constructor);
        CWeaponStatManagerDE   manager(bank, context.constructor);
        context.constructor.manager = &manager;
        Trace preparedTrace;
        Game  prepared(context.reader, context.clock, context.constructor, context.streaming, context.nativeState, preparedTrace,
                       Game::ConstructorServices::PreparedWeapons);
        prepared.pools = &context.pools;
        Check(context.constructor.consumptions == 1 && preparedTrace.events.empty() && !context.constructor.preparations,
              "prepared constructor retains same manager without world/Initialize preparation effects");
        Check(!context.reader.IsBound() && !context.streaming.IsBound() && !context.nativeState.IsBound() && !weaponReader.IsBound(),
              "SDK lifecycle fixture never invokes native code or publishes complete CGame");
        std::puts("Actual CGame world lifecycle SDK dispatch fixture completed");
    }
}
int main()
{
    try
    {
        SharedChecks();
        SdkChecks();
        std::printf("Passed %u game world SDK checks (64-bit; no native engine code executed)\n", checks);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "FAIL %u: %s\n", checks, error.what());
        return 1;
    }
}
