/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Preserve one actual startup/reset/world SDK graph with pure native gates
 ****************************************************************************/
#include <SharedUtil.Defines.h>
#pragma warning(push)
#pragma warning(disable : 4201 4456)
#include <SharedUtil.Misc.h>
#pragma warning(pop)
#include "../../Client/game_de/CGameStartupLifecycleDE.h"
#include "../../Client/game_de/CGameBuildingPoolDefaultDE.h"
#include "../../Client/game_de/NativeGameTimingAccess.h"
#include "../../Client/game_de/CClockDE.h"
#include <game/CFireManager.h>
#include <game/CCamera.h>
#include <game/CHud.h>
#include <game/CPools.h>
#include <game/CDummyPool.h>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <limits>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>
#include "StartupLifecycleFixture.generated.h"
namespace
{
    using namespace mta::game_de;
    unsigned checks = 0;
    void     Check(bool result, const char* reason)
    {
        ++checks;
        if (!result)
            throw std::runtime_error(reason);
    }
    struct Failure : std::runtime_error
    {
        int position;
        explicit Failure(int at) : std::runtime_error("policy spy refused"), position(at) {}
    };
    struct Trace
    {
        std::vector<int> events;
        int              failAt = -1;
        bool             ready = true, playing = true;
        unsigned         resetPreflights = 0, worldPreflights = 0;
        void             Call(int event)
        {
            const int at = static_cast<int>(events.size());
            events.push_back(event);
            if (at == failAt)
                throw Failure(at);
        }
        void Clear()
        {
            events.clear();
            failAt = -1;
        }
    };
    template <class Call>
    void Fails(Call&& call, int at)
    {
        bool failed = false;
        try
        {
            call();
        }
        catch (const Failure& error)
        {
            failed = error.position == at;
        }
        Check(failed, "original policy exception and accepted prefix are preserved");
    }
    template <class Call>
    void Refuses(Call&& call)
    {
        bool failed = false;
        try
        {
            call();
        }
        catch (const std::runtime_error&)
        {
            failed = true;
        }
        Check(failed, "missing or stale existing owner is explicitly refused");
    }
    const std::vector<int> Removal{100, 101, 102, 103, 104, 105, 106}, Restoration{107, 108, 109};
    const std::vector<int> ResetOrder{0, 1, 2, 3, 4, 5, 5, 6, 7, 8, 9, 10, 11, 107, 108, 109, 110, 111};
    void                   Equal(const std::vector<int>& actual, const std::vector<int>& expected)
    {
        Check(actual == expected, "actual combined graph preserves the independent golden effect itinerary");
    }
    std::vector<int> Prefix(const std::vector<int>& values, std::size_t count)
    {
        return {values.begin(), values.begin() + count};
    }
    class Fire final : public lifecycle_fixture::FireBase
    {
        Trace& trace;

    public:
        explicit Fire(Trace& t) : trace(t) {}
        void ExtinguishAllFires() override { trace.Call(0); }
    };
    class Camera final : public lifecycle_fixture::CameraBase
    {
        Trace& trace;

    public:
        explicit Camera(Trace& t) : trace(t) {}
        void Restore() override { trace.Call(1); }
        void SetFadeColor(unsigned char r, unsigned char g, unsigned char b) override
        {
            if (r || g || b)
                throw Failure(-90);
            trace.Call(2);
        }
        void Fade(float time, int mode) override
        {
            if (time != 0 || mode != FADE_OUT)
                throw Failure(-91);
            trace.Call(3);
        }
    };
    class Hud final : public lifecycle_fixture::HudBase
    {
        Trace& trace;

    public:
        explicit Hud(Trace& t) : trace(t) {}
        void Disable(bool off) override
        {
            if (off)
                throw Failure(-92);
            trace.Call(5);
        }
        void SetComponentVisible(eHudComponent component, bool show) override
        {
            if (component != HUD_ALL || !show)
                throw Failure(-93);
            trace.Call(6);
        }
    };
    class Buildings final : public lifecycle_fixture::BuildingsBase
    {
        Trace& trace;

    public:
        int  size = 20;
        int  lastCapacity = -1;
        bool resizeResult = true;
        explicit Buildings(Trace& t) : trace(t) {}
        int GetSize() const override
        {
            trace.Call(110);
            return size;
        }
        bool Resize(int next) override
        {
            trace.Call(112);
            lastCapacity = next;
            if (resizeResult)
                size = next;
            return resizeResult;
        }
        void RemoveAllWithBackup() override { trace.Call(105); }
        void RestoreBackup() override { trace.Call(107); }
    };
    class Dummies final : public lifecycle_fixture::DummiesBase
    {
        Trace& trace;

    public:
        explicit Dummies(Trace& t) : trace(t) {}
        void RemoveAllWithBackup() override { trace.Call(104); }
        void RestoreBackup() override { trace.Call(108); }
    };
    class Pools final : public lifecycle_fixture::PoolsBase
    {
        Buildings& buildings;
        Dummies&   dummies;

    public:
        Pools(Buildings& b, Dummies& d) : buildings(b), dummies(d) {}
        CBuildingsPool& GetBuildingsPool() noexcept override { return buildings; }
        CDummyPool&     GetDummyPool() noexcept override { return dummies; }
    };
    class ClockOwner final : public NativeClockAccess
    {
        ClockResult Read(ClockValues&) noexcept override { std::abort(); }
        ClockResult Set(std::uint8_t, std::uint8_t) noexcept override { std::abort(); }
        ClockResult Freeze(bool) noexcept override { std::abort(); }
        ClockResult Frozen(bool&) const noexcept override { std::abort(); }
    };
    class Owner : public NativeGameTimingAccess
    {
    public:
        NativeGameConstructorServices bundle;
        unsigned                      validations = 0, captures = 0, preparations = 0, initializationChecks = 0, weaponRequires = 0;
        ClockResult                   SetTimeScale(float) noexcept override { std::abort(); }
        ClockResult                   SetMinuteDuration(std::uint32_t) noexcept override { std::abort(); }
        ClockResult                   Pause(bool) noexcept override { std::abort(); }
        ClockResult                   ValidateConstructorServices(const NativeGameConstructorServices& value) noexcept override
        {
            ++validations;
            return value.Same(bundle) ? ClockResult::Ok : ClockResult::OwnerConflict;
        }
        ClockResult CaptureConstructorServices(const CClock* clock, NativeGameConstructorServices& value) noexcept override
        {
            ++captures;
            if (clock != bundle.clock)
                return ClockResult::OwnerConflict;
            value = bundle;
            return ClockResult::Ok;
        }
        ClockResult RequireConstructorWeapons(const CClock* clock, CWeaponStatManager*& value) noexcept override
        {
            ++weaponRequires;
            if (clock != bundle.clock)
                return ClockResult::OwnerConflict;
            value = bundle.weaponManager;
            return ClockResult::Ok;
        }
        ClockResult PrepareConstructorWeapons(NativeWeaponConstructorProgress&) noexcept override
        {
            ++preparations;
            return ClockResult::NotInitialized;
        }
        ClockResult GetStats(CStats*& value) noexcept override
        {
            value = bundle.stats;
            return ClockResult::Ok;
        }
        ClockResult RequireStartupInitializationReady() noexcept override
        {
            ++initializationChecks;
            return ClockResult::NotInitialized;
        }
    };
    using Base = lifecycle_fixture::NoPresentationGameBase<CGameStartupLifecycleDE>;
    class Game final : public Base
    {
        Trace& trace;

    public:
        using ConstructorServices = CGameDE::ConstructorServices;
        Fire      fire;
        Camera    camera;
        Hud       hud;
        Buildings buildings;
        Dummies   dummies;
        Pools     pools;
        int       defaultCapacity = 20;
        template <class... Prepared>
        Game(const NativeGameTimingReader& reader, CClockDE& clock, Owner& owner, const NativeStreamingFileIdsReader& streaming,
             const NativeGameStateReader& state, Trace& t, Prepared&&... prepared)
            : Base(reader, clock, owner, streaming, state, std::forward<Prepared>(prepared)...),
              trace(t),
              fire(t),
              camera(t),
              hud(t),
              buildings(t),
              dummies(t),
              pools(buildings, dummies)
        {
        }
        SystemState::Enum GetSystemState() override { return trace.playing ? SystemState::GS_PLAYING_GAME : SystemState::GS_INIT_ONCE; }
        CFireManager*     GetFireManager() override { return &fire; }
        CCamera*          GetCamera() override { return &camera; }
        CHud*             GetHud() override { return &hud; }
        CPools*           GetPools() const noexcept override { return const_cast<Pools*>(&pools); }
        void              Pause(bool paused) override
        {
            if (paused)
                throw Failure(-94);
            trace.Call(4);
        }
        bool                    Removed() const noexcept { return this->IsGameWorldRemoved(); }
        TaskSimpleBeHitHandler* Hit() const noexcept { return this->GetTaskSimpleBeHitHandler(); }

    protected:
        // Explicit test-policy spies are not production native hooks. The
        // generated omission probes separately prove every hook remains pure.
        void RequireResetReady() override
        {
            ++trace.resetPreflights;
            if (!trace.ready)
                throw Failure(-1);
        }
        void ResetVehicleModelDummies() override { trace.Call(7); }
        void RestoreObjectModelGroups() override { trace.Call(8); }
        void RestoreDefaultObjectGroupProperties() override { trace.Call(9); }
        void ResetVehicleModelWheelSizes() override { trace.Call(10); }
        void ResetModelTextureDictionaries() override { trace.Call(11); }
        int  GetResetBuildingPoolCapacity() const override
        {
            trace.Call(111);
            return defaultCapacity;
        }
        void RequireWorldLifecycleReady() override
        {
            ++trace.worldPreflights;
            if (!trace.ready)
                throw Failure(-1);
        }
        void DisableWorldIplStreaming() override { trace.Call(100); }
        void RemoveWorldCovers() override { trace.Call(101); }
        void RemoveWorldPlants() override { trace.Call(102); }
        void DestroyWorldStencilShadows() override { trace.Call(103); }
        void DropWorldBuildingCaches() override { trace.Call(106); }
        void EnableWorldIplStreaming(MtaGame::CGameWorldPolicy::IplFilter filter) override
        {
            if (!filter || filter({'b', 'a', 'r', 'r', 'i', 'e', 'r', 's'}) || !filter({}))
                throw Failure(-95);
            trace.Call(109);
        }
    };
    static_assert(std::is_convertible_v<Game*, CGame*> && std::is_convertible_v<Game*, CGameStartupInitializationDE*> &&
                  std::is_base_of_v<CGameStartupInitializationDE, Game>);
    static_assert(std::is_abstract_v<CGameStartupLifecycleDE> && std::is_abstract_v<CGameStartupLifecyclePresentationDE> &&
                  std::is_convertible_v<CGameStartupLifecyclePresentationDE*, CGame*>);
    // A typed test owner lets the default hook extend this same service graph
    // without replacing the real inherited hook with a policy spy.
    template <class OwnerType>
    struct ContextWithOwner
    {
        NativeGameTimingReader          reader;
        NativeStreamingFileIdsReader    streaming;
        NativeGameStateReader           state;
        ClockOwner                      clockOwner;
        CClockDE                        clock{clockOwner};
        OwnerType                       owner;
        Trace                           trace;
        std::array<std::uintptr_t, 128> tags{};
        ContextWithOwner()
        {
            // Opaque test-only bundle identities are compared but never used
            // as service objects or native effect endpoints.
            auto& b = owner.bundle;
            b.generation = 7;
            b.clock = &clock;
            b.stats = reinterpret_cast<CStats*>(&tags[0]);
            b.playerInfo = reinterpret_cast<CPlayerInfo*>(&tags[1]);
            b.wanted = reinterpret_cast<CWanted*>(&tags[2]);
            b.ropes = reinterpret_cast<CRopes*>(&tags[3]);
            b.weather = reinterpret_cast<CWeather*>(&tags[4]);
            b.weaponManager = reinterpret_cast<CWeaponStatManager*>(&tags[5]);
            for (std::size_t i = 0; i < b.weapons.size(); ++i)
                b.weapons[i] = reinterpret_cast<CWeaponInfo*>(&tags[6 + i]);
        }
    };
    using Context = ContextWithOwner<Owner>;
    bool Pre(CPlayerPed*, bool)
    {
        return false;
    }
    void Post()
    {
    }
    void Hit(CPedSAInterface*, ePedPieceTypes, int, int)
    {
    }
    void Identity()
    {
        Context                       c;
        Game                          game(c.reader, c.clock, c.owner, c.streaming, c.state, c.trace, c.owner.bundle);
        CGame&                        sdk = game;
        CGameStartupInitializationDE& startup = game;
        CGameDE&                      common = game;
        Check(&sdk == static_cast<CGame*>(&startup) && &sdk == static_cast<CGame*>(&common), "startup reset and world share one unambiguous actual CGame base");
        Check(sdk.GetClock() == &c.clock && startup.GetClock() == &c.clock && c.trace.events.empty() && !c.owner.initializationChecks && !c.owner.preparations,
              "one prepared clock/bundle graph performs no later Initialize or native preparation during construction");
        Check(c.owner.validations == 1 && c.owner.captures == 1 && sdk.GetStats() == c.owner.bundle.stats && startup.GetStats() == c.owner.bundle.stats,
              "both chain views consume the same freshly re-admitted prepared service bundle");
        sdk.SetPreWeaponFireHandler(&Pre);
        startup.SetPostWeaponFireHandler(&Post);
        common.SetTaskSimpleBeHitHandler(&Hit);
        WeaponFireHandlerAccess& handlers = game;
        Check(handlers.GetPreWeaponFireHandler() == &Pre && handlers.GetPostWeaponFireHandler() == &Post && game.Hit() == &Hit,
              "one handler registration store spans every view of the linear graph");
        sdk.GetCheckStatus() = 37;
        Check(&sdk.GetCheckStatus() == &startup.GetCheckStatus() && common.GetCheckStatus() == 37,
              "SDK bookkeeping storage is not duplicated by lifecycle composition");
        ++c.owner.bundle.generation;
        Refuses([&] { sdk.GetStats(); });
        --c.owner.bundle.generation;
        Refuses([&] { sdk.Initialize(); });
        Check(c.owner.initializationChecks == 1 && c.trace.events.empty(),
              "actual ready Initialize preflight remains inherited and explicitly refuses a missing native owner before effects");
        Context prepared;
        Game    weaponOnly(prepared.reader, prepared.clock, prepared.owner, prepared.streaming, prepared.state, prepared.trace,
                           Game::ConstructorServices::PreparedWeapons);
        Check(prepared.owner.weaponRequires == 1 && !prepared.owner.preparations && !prepared.owner.initializationChecks &&
                  weaponOnly.GetClock() == &prepared.clock,
              "legacy prepared-weapon constructor route remains separate from later Initialize");
        std::puts("Actual lifecycle single CGame graph fixture completed");
        std::puts("Actual lifecycle shared services and handlers fixture completed");
        std::puts("Actual lifecycle inherited Initialize refusal fixture completed");
    }
    void Policies()
    {
        Context c;
        Game    game(c.reader, c.clock, c.owner, c.streaming, c.state, c.trace);
        CGame&  sdk = game;
        Check(c.trace.events.empty() && !c.owner.captures && !c.owner.preparations, "historical constructor keeps policy and preparation effects separate");
        sdk.RemoveGameWorld();
        Equal(c.trace.events, Removal);
        Check(game.Removed(), "one world state commits only after complete removal");
        c.trace.Clear();
        Check(sdk.SetBuildingPoolSize(20), "same-size removed world returns success");
        Equal(c.trace.events, {110, 106});
        Check(game.Removed(), "same-size removed path drops caches without changing the one world state");
        c.trace.Clear();
        sdk.Reset();
        Equal(c.trace.events, ResetOrder);
        Check(!game.Removed(), "Reset dispatches actual world restoration and shares its committed state");
        c.trace.Clear();
        sdk.Reset();
        Equal(c.trace.events, ResetOrder);
        c.trace.Clear();
        c.trace.playing = false;
        c.trace.ready = false;
        const auto resetPreflights = c.trace.resetPreflights;
        sdk.Reset();
        Check(c.trace.events.empty() && c.trace.resetPreflights == resetPreflights, "inactive Reset gates before readiness and every SDK service");
        c.trace.playing = true;
        Fails([&] { sdk.Reset(); }, -1);
        Check(c.trace.events.empty(), "failed lifecycle readiness preserves an empty effect prefix");
        for (std::size_t at = 0; at < ResetOrder.size(); ++at)
        {
            Context item;
            Game    current(item.reader, item.clock, item.owner, item.streaming, item.state, item.trace);
            item.trace.failAt = static_cast<int>(at);
            Fails([&] { current.Reset(); }, static_cast<int>(at));
            Equal(item.trace.events, Prefix(ResetOrder, at + 1));
        }
        for (std::size_t at = 0; at < Removal.size(); ++at)
        {
            Context item;
            Game    current(item.reader, item.clock, item.owner, item.streaming, item.state, item.trace);
            item.trace.failAt = static_cast<int>(at);
            Fails([&] { current.RemoveGameWorld(); }, static_cast<int>(at));
            Equal(item.trace.events, Prefix(Removal, at + 1));
            Check(!current.Removed(), "failed removal does not commit world-removed state");
        }
        for (std::size_t at = 0; at < Restoration.size(); ++at)
        {
            Context item;
            Game    current(item.reader, item.clock, item.owner, item.streaming, item.state, item.trace);
            current.RemoveGameWorld();
            item.trace.Clear();
            item.trace.failAt = static_cast<int>(at);
            Fails([&] { current.RestoreGameWorld(); }, static_cast<int>(at));
            Equal(item.trace.events, Prefix(Restoration, at + 1));
            Check(current.Removed(), "failed restoration preserves the prior removed state without rollback");
        }
        Context resize;
        Game    current(resize.reader, resize.clock, resize.owner, resize.streaming, resize.state, resize.trace);
        current.buildings.resizeResult = false;
        Check(!current.SetBuildingPoolSize(25), "false resize status is preserved");
        auto expected = std::vector<int>{110};
        expected.insert(expected.end(), Removal.begin(), Removal.end());
        expected.push_back(112);
        expected.insert(expected.end(), Restoration.begin(), Restoration.end());
        Equal(resize.trace.events, expected);
        Check(!current.Removed(), "false resize still restores the actual shared world");
        resize.trace.Clear();
        resize.trace.failAt = 8;
        Fails([&] { current.SetBuildingPoolSize(25); }, 8);
        Equal(resize.trace.events, Prefix(expected, 9));
        Check(current.Removed(), "throwing resize keeps its accepted removal prefix instead of synthesizing restoration");
        resize.trace.Clear();
        const auto worldPreflights = resize.trace.worldPreflights;
        Check(!current.SetBuildingPoolSize((std::numeric_limits<std::size_t>::max)()) && resize.trace.events.empty() &&
                  resize.trace.worldPreflights == worldPreflights,
              "oversize capacity refuses before readiness or SDK effects");
        std::puts("Actual lifecycle Reset World order and state fixture completed");
    }
#include "BuildingPoolDefaultSdkTests.inl"
}
int main()
{
    try
    {
        Identity();
        Policies();
        BuildingDefaults();
        std::puts("All fourteen lifecycle native hooks remain abstract");
        std::printf("Passed %u startup lifecycle SDK checks (%u-bit; policy doubles only)\n", checks, static_cast<unsigned>(sizeof(void*) * 8));
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "Lifecycle SDK failed after %u checks: %s\n", checks, error.what());
        return 1;
    }
}
