/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual MTA reset policy and SDK dispatch without native factories
 *****************************************************************************/
#include <SharedUtil.Defines.h>
// Keep the real HUD colour ABI, including its legacy anonymous union. Limit
// existing utility-header diagnostics to that include; new code stays /W4 /WX.
#pragma warning(push)
#pragma warning(disable : 4201 4456)
#include <SharedUtil.Misc.h>
#pragma warning(pop)
#include "../../Client/game_de/CGameResetDE.h"
#include "../../Client/game_de/CWeaponStatManagerDE.h"
#include <game/CGameResetPolicy.h>
#include <game/CFireManager.h>
#include <game/CCamera.h>
#include <game/CHud.h>
#include <game/CPools.h>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <stdexcept>
#include <string>
#include <vector>
namespace reset_fixture
{
    [[noreturn]] inline void Unqualified(const char* method) noexcept
    {
        std::fprintf(stderr, "Unqualified Reset fixture call: %s\n", method);
        std::abort();
    }
}
#include "GameResetFixture.generated.h"
namespace
{
    using namespace mta::game_de;
    unsigned resetChecks = 0;
    void     CheckReset(bool value, const char* reason)
    {
        ++resetChecks;
        if (!value)
            throw std::runtime_error(reason);
    }
    struct Failure : std::runtime_error
    {
        explicit Failure(int index) : std::runtime_error("existing reset owner refused"), at(index) {}
        int at;
    };
    template <class Operation>
    void FailsAt(Operation&& operation, int index)
    {
        try
        {
            operation();
        }
        catch (const Failure& failure)
        {
            CheckReset(failure.at == index, "original failing owner exception propagated exactly");
            return;
        }
        CheckReset(false, "required owner refusal propagated");
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
            CheckReset(true, "missing required SDK service refused explicitly");
            return;
        }
        CheckReset(false, "missing required service cannot silently succeed");
    }
    // Independent golden order copied from the actual pre-extraction Reset
    // statements. Duplicate5 is the original second HUD Disable(false).
    const std::vector<int> FullOrder{0, 1, 2, 3, 4, 5, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
    struct Trace
    {
        std::vector<int>         calls;
        std::function<void(int)> inside;
        int                      failAt = -1;
        unsigned                 generation = 1, admitted = 1, preflights = 0;
        bool                     ready = true, inOperation = false;
        void                     Current() const
        {
            if (!ready || generation != admitted)
                throw Failure(-2);
        }
        void Preflight()
        {
            ++preflights;
            if (!ready || inOperation)
                throw Failure(-1);
            admitted = generation;
        }
        void Call(int kind)
        {
            Current();
            const auto index = static_cast<int>(calls.size());
            calls.push_back(kind);
            struct Restore
            {
                bool& value;
                ~Restore() { value = false; }
            } restore{inOperation};
            inOperation = true;
            if (inside)
                inside(kind);
            if (index == failAt)
                throw Failure(index);
            Current();
        }
    };
    void Prefix(const std::vector<int>& values, std::size_t count)
    {
        CheckReset(values.size() == count, "operation trace retains exact prefix length");
        for (std::size_t at = 0; at < count; ++at)
            CheckReset(values[at] == FullOrder[at], "actual reset order and repeated HUD call preserved");
    }
    void SharedReset(bool playing, Trace& trace, int current, int defaultSize, bool resizeResult)
    {
        MtaGame::CGameResetPolicy::Reset(
            playing, [&] { trace.Call(0); }, [&] { trace.Call(1); }, [&] { trace.Call(2); }, [&] { trace.Call(3); }, [&] { trace.Call(4); },
            [&] { trace.Call(5); }, [&] { trace.Call(6); }, [&] { trace.Call(7); }, [&] { trace.Call(8); }, [&] { trace.Call(9); }, [&] { trace.Call(10); },
            [&] { trace.Call(11); }, [&] { trace.Call(12); },
            [&]
            {
                trace.Call(13);
                return current;
            },
            [&]
            {
                trace.Call(14);
                return defaultSize;
            },
            [&](int target)
            {
                CheckReset(target == defaultSize, "default capacity forwarded unchanged");
                trace.Call(15);
                return resizeResult;
            });
    }
    void SharedPolicyChecks()
    {
        Trace inactive;
        inactive.ready = false;
        SharedReset(false, inactive, 200, 100, true);
        CheckReset(inactive.calls.empty(), "inactive shared gate invokes no callback, capacity read or missing service");
        for (int failed = -1; failed < static_cast<int>(FullOrder.size()); ++failed)
        {
            Trace trace;
            trace.failAt = failed;
            if (failed < 0)
                SharedReset(true, trace, 200, 100, false);
            else
                FailsAt([&] { SharedReset(true, trace, 200, 100, true); }, failed);
            Prefix(trace.calls, failed < 0 ? FullOrder.size() : static_cast<std::size_t>(failed + 1));
        }
        Trace equal;
        SharedReset(true, equal, 100, 100, true);
        Prefix(equal.calls, FullOrder.size() - 1);
        Trace repeated;
        for (int iteration = 0; iteration < 3; ++iteration)
        {
            repeated.calls.clear();
            SharedReset(true, repeated, 200, 100, false);
            Prefix(repeated.calls, FullOrder.size());
        }
        std::puts("Actual shared CGame Reset policy fixture completed");
    }
    class Fire final : public reset_fixture::FireFixtureBase
    {
    public:
        explicit Fire(Trace& value) : trace(value) {}
        void ExtinguishAllFires() override { trace.Call(0); }

    private:
        Trace& trace;
    };
    class Camera final : public reset_fixture::CameraFixtureBase
    {
    public:
        explicit Camera(Trace& value) : trace(value) {}
        void Restore() override { trace.Call(1); }
        void SetFadeColor(unsigned char red, unsigned char green, unsigned char blue) override
        {
            CheckReset(!red && !green && !blue, "actual camera receives black fade color");
            trace.Call(2);
        }
        void Fade(float duration, int mode) override
        {
            CheckReset(duration == 0.0f && mode == FADE_OUT, "actual camera receives immediate fade-out");
            trace.Call(3);
        }

    private:
        Trace& trace;
    };
    class Hud final : public reset_fixture::HudFixtureBase
    {
    public:
        explicit Hud(Trace& value) : trace(value) {}
        void Disable(bool disabled) override
        {
            CheckReset(!disabled, "both actual HUD restore calls pass false");
            trace.Call(5);
        }
        void SetComponentVisible(eHudComponent component, bool visible) override
        {
            CheckReset(component == HUD_ALL && visible, "actual HUD shows all components");
            trace.Call(6);
        }

    private:
        Trace& trace;
    };
    class Buildings final : public reset_fixture::BuildingsFixtureBase
    {
    public:
        explicit Buildings(Trace& value) : trace(value) {}
        int size = 18000;
        int GetSize() const override
        {
            trace.Call(13);
            return size;
        }

    private:
        Trace& trace;
    };
    class Pools final : public reset_fixture::PoolsFixtureBase
    {
    public:
        explicit Pools(Buildings& value) : buildings(value) {}
        CBuildingsPool& GetBuildingsPool() noexcept override
        {
            ++reads;
            return buildings;
        }
        unsigned reads = 0;

    private:
        Buildings& buildings;
    };
    class ClockOwner final : public NativeClockAccess
    {
        ClockResult Read(ClockValues&) noexcept override { reset_fixture::Unqualified("native clock read"); }
        ClockResult Set(std::uint8_t, std::uint8_t) noexcept override { reset_fixture::Unqualified("native clock set"); }
        ClockResult Freeze(bool) noexcept override { reset_fixture::Unqualified("native clock freeze"); }
        ClockResult Frozen(bool&) const noexcept override { reset_fixture::Unqualified("native clock frozen"); }
    };
    class ConstructorOwner final : public NativeGameTimingAccess
    {
    public:
        ClockResult SetTimeScale(float) noexcept override { reset_fixture::Unqualified("native time scale"); }
        ClockResult SetMinuteDuration(std::uint32_t) noexcept override { reset_fixture::Unqualified("native minute duration"); }
        ClockResult Pause(bool) noexcept override { reset_fixture::Unqualified("native timing pause"); }
        ClockResult RequireConstructorWeapons(const CClock* value, CWeaponStatManager*& output) noexcept override
        {
            ++constructorRequires;
            requestedClock = value;
            if (result == ClockResult::Ok)
                output = manager;
            return result;
        }
        ClockResult PrepareConstructorWeapons(NativeWeaponConstructorProgress&) noexcept override
        {
            ++preparations;
            return ClockResult::NotInitialized;
        }
        unsigned            constructorRequires = 0, preparations = 0;
        const CClock*       requestedClock = nullptr;
        CWeaponStatManager* manager = nullptr;
        ClockResult         result = ClockResult::NotInitialized;
    };
    class Game final : public reset_fixture::GameResetFixtureBase
    {
    public:
        Game(const NativeGameTimingReader& reader, CClockDE& clock, NativeGameTimingAccess& timing, const NativeStreamingFileIdsReader& streaming,
             const NativeGameStateReader& nativeState, Trace& value) noexcept
            : GameResetFixtureBase(reader, clock, timing, streaming, nativeState), trace(value)
        {
        }
        Game(const NativeGameTimingReader& reader, CClockDE& clock, NativeGameTimingAccess& timing, const NativeStreamingFileIdsReader& streaming,
             const NativeGameStateReader& nativeState, Trace& value, ConstructorServices services)
            : GameResetFixtureBase(reader, clock, timing, streaming, nativeState, services), trace(value)
        {
        }
        SystemState::Enum state = SystemState::GS_PLAYING_GAME;
        bool              failState = false, resizeResult = false;
        int               defaultCapacity = 17000;
        std::size_t       requestedCapacity = 0;
        unsigned          stateReads = 0, fireReads = 0, cameraReads = 0, hudReads = 0;
        mutable unsigned  poolReads = 0;
        CFireManager*     fire = nullptr;
        CCamera*          camera = nullptr;
        CHud*             hud = nullptr;
        CPools*           pools = nullptr;
        SystemState::Enum GetSystemState() override
        {
            ++stateReads;
            if (failState)
                throw Failure(-3);
            return state;
        }
        CFireManager* GetFireManager() override
        {
            ++fireReads;
            trace.Current();
            return fire;
        }
        CCamera* GetCamera() override
        {
            ++cameraReads;
            trace.Current();
            return camera;
        }
        CHud* GetHud() override
        {
            ++hudReads;
            trace.Current();
            return hud;
        }
        CPools* GetPools() const noexcept override
        {
            ++poolReads;
            return pools;
        }
        void Pause(bool paused) override
        {
            CheckReset(!paused, "actual SDK reset unpauses, preserving fade behavior");
            trace.Call(4);
        }
        void RestoreGameWorld() override { trace.Call(12); }
        bool SetBuildingPoolSize(std::size_t capacity) override
        {
            requestedCapacity = capacity;
            trace.Call(15);
            return resizeResult;
        }

    protected:
        void RequireResetReady() override
        {
            trace.Preflight();
            if (!fire || !camera || !hud || !pools)
                throw Failure(-1);
        }
        void ResetVehicleModelDummies() override { trace.Call(7); }
        void RestoreObjectModelGroups() override { trace.Call(8); }
        void RestoreDefaultObjectGroupProperties() override { trace.Call(9); }
        void ResetVehicleModelWheelSizes() override { trace.Call(10); }
        void ResetModelTextureDictionaries() override { trace.Call(11); }
        int  GetResetBuildingPoolCapacity() const override
        {
            trace.Call(14);
            return defaultCapacity;
        }
        void RequireInitializationReady() override { reset_fixture::Unqualified("later Initialize preflight"); }
        void FixStartupModelCollision(std::uint32_t target, std::uint32_t source) override
        {
            (void)target;
            (void)source;
            reset_fixture::Unqualified("startup collision");
        }
        void InitializeRenderer() override { reset_fixture::Unqualified("renderer initialization"); }
        void HideNativeMenu() override { reset_fixture::Unqualified("native menu hide"); }

    private:
        Trace& trace;
    };
    void Bind(Game& game, Fire& fire, Camera& camera, Hud& hud, Pools& pools)
    {
        game.fire = &fire;
        game.camera = &camera;
        game.hud = &hud;
        game.pools = &pools;
    }
    void SdkChecks()
    {
        NativeGameTimingReader       reader;
        NativeStreamingFileIdsReader streaming;
        NativeGameStateReader        nativeState;
        ClockOwner                   clockOwner;
        CClockDE                     clock(clockOwner);
        ConstructorOwner             constructor;
        Trace                        trace;
        Fire                         fire(trace);
        Camera                       camera(trace);
        Hud                          hud(trace);
        Buildings                    buildings(trace);
        Pools                        pools(buildings);
        Game                         game(reader, clock, constructor, streaming, nativeState, trace);
        Bind(game, fire, camera, hud, pools);
        CGame& sdk = game;
        CheckReset(trace.calls.empty() && !trace.preflights && !constructor.constructorRequires && !constructor.preparations,
                   "partial construction invokes neither Reset nor initialization/native preparation");
        for (const auto state : {SystemState::GS_INIT_PLAYING_GAME, SystemState::GS_FRONTEND})
        {
            game.state = state;
            trace.ready = false;
            const auto preflights = trace.preflights;
            sdk.Reset();
            CheckReset(trace.calls.empty() && trace.preflights == preflights && !game.fireReads && !game.cameraReads && !game.hudReads && !game.poolReads,
                       "nonplaying SDK reset skips readiness, every service and capacity access");
        }
        game.state = SystemState::GS_PLAYING_GAME;
        trace.ready = true;
        for (unsigned attempt = 0; attempt < 3; ++attempt)
        {
            trace.calls.clear();
            const auto preflights = trace.preflights;
            sdk.Reset();
            Prefix(trace.calls, FullOrder.size());
            CheckReset(trace.preflights == preflights + 1 && game.requestedCapacity == 17000,
                       "every explicit reset freshly preflights and uses genuine backend capacity, not classic MAX_BUILDINGS");
        }
        buildings.size = game.defaultCapacity;
        trace.calls.clear();
        sdk.Reset();
        Prefix(trace.calls, FullOrder.size() - 1);
        buildings.size = 18000;
        for (int failed = 0; failed < static_cast<int>(FullOrder.size()); ++failed)
        {
            trace.calls.clear();
            trace.failAt = failed;
            FailsAt([&] { sdk.Reset(); }, failed);
            Prefix(trace.calls, static_cast<std::size_t>(failed + 1));
        }
        trace.failAt = -1;
        trace.calls.clear();
        trace.ready = false;
        FailsAt([&] { sdk.Reset(); }, -1);
        CheckReset(trace.calls.empty(), "unavailable owner refuses before any effect");
        trace.ready = true;
        for (unsigned missing = 0; missing < 4; ++missing)
        {
            Bind(game, fire, camera, hud, pools);
            if (missing == 0)
                game.fire = nullptr;
            else if (missing == 1)
                game.camera = nullptr;
            else if (missing == 2)
                game.hud = nullptr;
            else
                game.pools = nullptr;
            FailsAt([&] { sdk.Reset(); }, -1);
            CheckReset(trace.calls.empty(), "preflight constructorRequires every genuine receiver before first effect");
        }
        Bind(game, fire, camera, hud, pools);
        game.failState = true;
        FailsAt([&] { sdk.Reset(); }, -3);
        CheckReset(trace.calls.empty(), "state refusal precedes preflight/effects");
        game.failState = false;
        trace.inside = [&](int kind)
        {
            if (kind == 3)
                game.hud = nullptr;
        };
        Refuses([&] { sdk.Reset(); });
        Prefix(trace.calls, 5);
        Bind(game, fire, camera, hud, pools);
        trace.inside = {};
        trace.calls.clear();
        bool reentrantRefused = false;
        trace.inside = [&](int kind)
        {
            if (kind == 1)
            {
                FailsAt([&] { sdk.Reset(); }, -1);
                reentrantRefused = true;
            }
        };
        sdk.Reset();
        Prefix(trace.calls, FullOrder.size());
        CheckReset(reentrantRefused, "existing readiness gate handles reentrancy; no second local owner or effect replay is created");
        trace.calls.clear();
        trace.inside = [&](int kind)
        {
            if (kind == 7)
                ++trace.generation;
        };
        FailsAt([&] { sdk.Reset(); }, -2);
        Prefix(trace.calls, 9);
        trace.inside = {};
        trace.calls.clear();
        sdk.Reset();
        Prefix(trace.calls, FullOrder.size());
        for (const int bad : {0, -1})
        {
            trace.calls.clear();
            game.defaultCapacity = bad;
            Refuses([&] { sdk.Reset(); });
            Prefix(trace.calls, FullOrder.size() - 1);
        }
        game.defaultCapacity = 17000;

        NativeWeaponInfoReader weaponReader;
        CWeaponInfoBankDE      bank(weaponReader, constructor, constructor);
        CWeaponStatManagerDE   manager(bank, constructor);
        constructor.manager = &manager;
        constructor.result = ClockResult::Ok;
        Trace     preparedTrace;
        Fire      preparedFire(preparedTrace);
        Camera    preparedCamera(preparedTrace);
        Hud       preparedHud(preparedTrace);
        Buildings preparedBuildings(preparedTrace);
        Pools     preparedPools(preparedBuildings);
        Game      prepared(reader, clock, constructor, streaming, nativeState, preparedTrace, Game::ConstructorServices::PreparedWeapons);
        Bind(prepared, preparedFire, preparedCamera, preparedHud, preparedPools);
        CheckReset(constructor.constructorRequires == 1 && constructor.requestedClock == &clock && preparedTrace.calls.empty(),
                   "tagged constructor retains exact prepared manager/clock consumption without Reset effects");
        prepared.Reset();
        Prefix(preparedTrace.calls, FullOrder.size());
        CheckReset(
            constructor.constructorRequires == 1 && !constructor.preparations && manager.GetStartupState() == CWeaponStatManagerDE::StartupState::NotStarted,
            "Reset never restarts constructor weapon preparation or invokes later Initialize");
        CheckReset(!reader.IsBound() && !streaming.IsBound() && !nativeState.IsBound() && !weaponReader.IsBound(),
                   "actual SDK dispatch executes no native reads/writes or complete-service publication");
        std::puts("Actual CGame Reset SDK dispatch fixture completed");
    }
}
int main()
{
    try
    {
        SharedPolicyChecks();
        SdkChecks();
        std::printf("Passed %u game reset SDK checks (64-bit; no native engine code executed)\n", resetChecks);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "FAIL %u: %s\n", resetChecks, error.what());
        return 1;
    }
}
