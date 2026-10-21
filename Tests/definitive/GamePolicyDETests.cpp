/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Exercise the actual CGameDE SDK registration and context policies
 *****************************************************************************/
#include "../../Client/game_de/CGameDE.h"
#include <SharedUtil.Defines.h>
#include <game/CPools.h>
#include <game/CWorldCheatPolicy.h>

#include <array>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <type_traits>

namespace fixture
{
    [[noreturn]] inline void Unqualified() noexcept
    {
        std::fputs("FAIL: unrelated SDK method reached CPU-policy fixture\n", stderr);
        std::abort();
    }
}

// The runner derives these rejecting fixtures from the current SDK declarations.
// Only unimplemented methods are supplied here; none of the production
// policies under test is replaced, copied, or reimplemented in the fixture.
#include "GamePolicyFixture.generated.h"

namespace
{
    unsigned checks = 0, handlerCalls = 0;

    void Check(bool value, const char* description)
    {
        ++checks;
        if (!value)
        {
            std::fprintf(stderr, "FAIL: %s\n", description);
            std::exit(EXIT_FAILURE);
        }
    }

    template <class Operation>
    void Refuses(Operation operation, const char* description)
    {
        try
        {
            operation();
        }
        catch (const std::runtime_error&)
        {
            Check(true, description);
            return;
        }
        catch (...)
        {
        }
        Check(false, description);
    }

    class UnusedClockAccess final : public mta::game_de::NativeClockAccess
    {
    public:
        mta::game_de::ClockResult Read(mta::game_de::ClockValues&) noexcept override { fixture::Unqualified(); }
        mta::game_de::ClockResult Set(std::uint8_t, std::uint8_t) noexcept override { fixture::Unqualified(); }
        mta::game_de::ClockResult Freeze(bool) noexcept override { fixture::Unqualified(); }
        mta::game_de::ClockResult Frozen(bool&) const noexcept override { fixture::Unqualified(); }
    };

    class LegacyTimingAccess final : public mta::game_de::NativeGameTimingAccess
    {
    public:
        mta::game_de::ClockResult SetTimeScale(float) noexcept override { fixture::Unqualified(); }
        mta::game_de::ClockResult SetMinuteDuration(std::uint32_t) noexcept override { fixture::Unqualified(); }
        mta::game_de::ClockResult Pause(bool) noexcept override { fixture::Unqualified(); }
    };

    class UnusedTimingAccess final : public mta::game_de::NativeGameTimingAccess
    {
    public:
        mta::game_de::ClockResult SetTimeScale(float) noexcept override { fixture::Unqualified(); }
        mta::game_de::ClockResult SetMinuteDuration(std::uint32_t) noexcept override { fixture::Unqualified(); }
        mta::game_de::ClockResult Pause(bool) noexcept override { fixture::Unqualified(); }
        mta::game_de::ClockResult StartGame() noexcept override
        {
            ++startRequests;
            return startResult;
        }
        unsigned                  startRequests = 0;
        mta::game_de::ClockResult startResult = mta::game_de::ClockResult::NotInitialized;
        mta::game_de::ClockResult GetStats(CStats*& output) noexcept override
        {
            ++statsReads;
            if (statsResult == mta::game_de::ClockResult::Ok)
                output = statsPointer;
            return statsResult;
        }
        unsigned                  statsReads = 0;
        CStats*                   statsPointer = nullptr;
        mta::game_de::ClockResult statsResult = mta::game_de::ClockResult::NotInitialized;
        mta::game_de::ClockResult GetPlayerInfo(CPlayerInfo*& output) noexcept override
        {
            ++playerInfoReads;
            if (playerInfoResult == mta::game_de::ClockResult::Ok)
                output = playerInfoPointer;
            return playerInfoResult;
        }
        unsigned                  playerInfoReads = 0;
        CPlayerInfo*              playerInfoPointer = nullptr;
        mta::game_de::ClockResult playerInfoResult = mta::game_de::ClockResult::NotInitialized;
        mta::game_de::ClockResult GetWeather(CWeather*& output) noexcept override
        {
            ++weatherReads;
            if (weatherResult == mta::game_de::ClockResult::Ok)
                output = weatherPointer;
            return weatherResult;
        }
        unsigned                  weatherReads = 0;
        CWeather*                 weatherPointer = nullptr;
        mta::game_de::ClockResult weatherResult = mta::game_de::ClockResult::NotInitialized;
        mta::game_de::ClockResult GetWeaponInfoSlot(std::size_t slot, CWeaponInfo*& output) noexcept override
        {
            ++weaponReads;
            requestedWeaponSlot = slot;
            if (weaponResult == mta::game_de::ClockResult::Ok)
                output = weaponResultPointer;
            return weaponResult;
        }
        unsigned                  weaponReads = 0;
        std::size_t               requestedWeaponSlot = 80;
        CWeaponInfo*              weaponResultPointer = nullptr;
        mta::game_de::ClockResult weaponResult = mta::game_de::ClockResult::NotInitialized;
        mta::game_de::ClockResult SetSystemState(SystemState::Enum value) noexcept override
        {
            ++stateWrites;
            requestedState = value;
            return stateResult;
        }
        mta::game_de::ClockResult ReadWorldCheat(MtaGame::CWorldCheatPolicy::Id id, bool& output) noexcept override
        {
            ++worldCheatReads;
            requestedWorldCheat = id;
            if (worldCheatResult == mta::game_de::ClockResult::Ok)
                output = worldCheatValue;
            return worldCheatResult;
        }
        unsigned                       worldCheatReads = 0;
        bool                           worldCheatValue = false;
        MtaGame::CWorldCheatPolicy::Id requestedWorldCheat = MtaGame::CWorldCheatPolicy::Id::Count;
        mta::game_de::ClockResult      worldCheatResult = mta::game_de::ClockResult::NotInitialized;
        mta::game_de::ClockResult      ReadCheatFlag(mta::game_de::NativeCheatProfile::Flag flag, bool& output) noexcept override
        {
            ++cheatReads;
            requestedFlag = flag;
            if (cheatResult == mta::game_de::ClockResult::Ok)
                output = cheatValue;
            return cheatResult;
        }
        mta::game_de::ClockResult SetCheatFlag(mta::game_de::NativeCheatProfile::Flag flag, bool enabled) noexcept override
        {
            ++cheatWrites;
            requestedFlag = flag;
            requestedEnabled = enabled;
            return cheatResult;
        }
        unsigned                               cheatReads = 0, cheatWrites = 0;
        bool                                   cheatValue = false, requestedEnabled = false;
        mta::game_de::NativeCheatProfile::Flag requestedFlag = mta::game_de::NativeCheatProfile::Flag::HoverCars;
        mta::game_de::ClockResult              cheatResult = mta::game_de::ClockResult::NotInitialized;
        unsigned                               stateWrites = 0;
        SystemState::Enum                      requestedState = SystemState::GS_START_UP;
        mta::game_de::ClockResult              stateResult = mta::game_de::ClockResult::NotInitialized;
    };

    class TestPools final : public fixture::PoolFixtureBase
    {
    public:
        CPed* GetPedFromRef(DWORD reference) override
        {
            ++reads;
            lastReference = reference;
            return result;
        }

        unsigned reads = 0;
        DWORD    lastReference = 0;
        CPed*    result = nullptr;
    };

    class TestGame final : public fixture::GameFixtureBase
    {
    public:
        TestGame(const mta::game_de::NativeGameTimingReader& reader, mta::game_de::CClockDE& clock, mta::game_de::NativeGameTimingAccess& timing,
                 const mta::game_de::NativeStreamingFileIdsReader& streaming, const mta::game_de::NativeGameStateReader& state) noexcept
            : GameFixtureBase(reader, clock, timing, streaming, state)
        {
        }

        CPools* GetPools() const noexcept override
        {
            ++poolReads;
            return pools;
        }

        using CGameDE::GetPostWeaponFireHandler;
        using CGameDE::GetPreWeaponFireHandler;
        using CGameDE::GetTaskSimpleBeHitHandler;

        CPools*          pools = nullptr;
        mutable unsigned poolReads = 0;
    };

    bool PreFirst(CPlayerPed*, bool)
    {
        ++handlerCalls;
        return true;
    }
    bool PreSecond(CPlayerPed*, bool)
    {
        ++handlerCalls;
        return false;
    }
    void PostFirst()
    {
        ++handlerCalls;
    }
    void PostSecond()
    {
        ++handlerCalls;
    }
    void HitFirst(CPedSAInterface*, ePedPieceTypes, int, int)
    {
        ++handlerCalls;
    }
    void HitSecond(CPedSAInterface*, ePedPieceTypes, int, int)
    {
        ++handlerCalls;
    }
}

int main()
{
    static_assert(std::is_abstract_v<mta::game_de::CGameDE>);
    static_assert(std::is_base_of_v<CGame, TestGame> && !std::is_abstract_v<TestGame>);
    mta::game_de::NativeGameTimingReader       reader;
    mta::game_de::NativeStreamingFileIdsReader streaming;
    mta::game_de::NativeGameStateReader        state;
    UnusedTimingAccess                         timing;
    UnusedClockAccess                          clockAccess;
    mta::game_de::CClockDE                     clock(clockAccess);
    TestGame                                   first(reader, clock, timing, streaming, state), second(reader, clock, timing, streaming, state);
    CGame&                                     game = first;
    CGame&                                     other = second;
    TestPools                                  firstPools, secondPools;

    Check(game.GetCheckStatus() == 0 && other.GetCheckStatus() == 0, "classic initial check status is zero per instance");
    int& checkStatus = game.GetCheckStatus();
    Check(&checkStatus == &game.GetCheckStatus(), "SDK returns a stable reference to its diagnostic slot");
    checkStatus = -17;
    Check(game.GetCheckStatus() == -17, "mutating SDK reference updates the actual game slot");
    Check(other.GetCheckStatus() == 0 && &other.GetCheckStatus() != &checkStatus, "separate games never share check status");
    game.GetCheckStatus() = 41;
    Check(checkStatus == 41, "writes through later SDK reference remain visible to earlier reference");

    // Opaque fixture identities are compared only; they are never dereferenced
    // or represented as real/native CPed objects in the production backend.
    unsigned char firstIdentity = 0, secondIdentity = 0, explicitIdentity = 0;
    CPed*         firstPed = reinterpret_cast<CPed*>(&firstIdentity);
    CPed*         secondPed = reinterpret_cast<CPed*>(&secondIdentity);
    CPed*         explicitPed = reinterpret_cast<CPed*>(&explicitIdentity);
    Refuses([&] { game.GetPedContext(); }, "missing actual pool service refuses explicitly");
    Check(first.poolReads == 1 && firstPools.reads == 0, "unavailable pools are not dereferenced");
    first.pools = &firstPools;
    Check(game.GetPedContext() == nullptr, "pool lookup may legitimately have no local ped yet");
    Check(firstPools.reads == 1 && firstPools.lastReference == DWORD{1}, "fallback uses actual SDK GetPedFromRef(1)");
    Check(game.GetPedContext() == nullptr && firstPools.reads == 2, "a null fallback is retried rather than cached as ready");
    firstPools.result = firstPed;
    Check(game.GetPedContext() == firstPed && firstPools.reads == 3, "lazy lookup can become available after earlier failures");
    firstPools.result = secondPed;
    const auto readsBeforeCache = first.poolReads;
    Check(game.GetPedContext() == firstPed && firstPools.reads == 3 && first.poolReads == readsBeforeCache, "non-null context preserves classic lazy cache");
    game.OnPedContextChange(explicitPed);
    first.pools = nullptr;
    Check(game.GetPedContext() == explicitPed && first.poolReads == readsBeforeCache, "explicit context bypasses local-pool fallback");
    game.OnPedContextChange(nullptr);
    Refuses([&] { game.GetPedContext(); }, "clearing context requires a fresh available pool service");
    first.pools = &secondPools;
    secondPools.result = secondPed;
    Check(game.GetPedContext() == secondPed && secondPools.reads == 1 && secondPools.lastReference == DWORD{1},
          "cleared context follows current virtual pool service");
    second.pools = &firstPools;
    firstPools.result = firstPed;
    Check(other.GetPedContext() == firstPed && game.GetPedContext() == secondPed, "context cache belongs to each actual game instance");
    other.OnPedContextChange(nullptr);
    firstPools.result = nullptr;
    Check(other.GetPedContext() == nullptr && game.GetPedContext() == secondPed, "clearing one context cannot affect another game");

    Check(first.GetPreWeaponFireHandler() == nullptr && first.GetPostWeaponFireHandler() == nullptr && first.GetTaskSimpleBeHitHandler() == nullptr,
          "all borrowed handler registrations start empty");
    game.SetPreWeaponFireHandler(PreFirst);
    game.SetPostWeaponFireHandler(PostFirst);
    game.SetTaskSimpleBeHitHandler(HitFirst);
    Check(first.GetPreWeaponFireHandler() == PreFirst, "SDK retains exact typed pre-fire callback");
    Check(first.GetPostWeaponFireHandler() == PostFirst, "SDK retains exact typed post-fire callback");
    Check(first.GetTaskSimpleBeHitHandler() == HitFirst, "SDK retains exact typed hit callback");
    Check(second.GetPreWeaponFireHandler() == nullptr && second.GetPostWeaponFireHandler() == nullptr && second.GetTaskSimpleBeHitHandler() == nullptr,
          "handler registrations belong to each game instance");
    game.SetPreWeaponFireHandler(PreSecond);
    Check(first.GetPreWeaponFireHandler() == PreSecond && first.GetPostWeaponFireHandler() == PostFirst && first.GetTaskSimpleBeHitHandler() == HitFirst,
          "replacing pre-fire callback preserves other registrations");
    game.SetPostWeaponFireHandler(PostSecond);
    game.SetTaskSimpleBeHitHandler(HitSecond);
    Check(first.GetPostWeaponFireHandler() == PostSecond && first.GetTaskSimpleBeHitHandler() == HitSecond, "later SDK registrations replace exact callbacks");
    game.SetPreWeaponFireHandler(nullptr);
    Check(first.GetPreWeaponFireHandler() == nullptr && first.GetPostWeaponFireHandler() == PostSecond && first.GetTaskSimpleBeHitHandler() == HitSecond,
          "clearing pre-fire callback preserves independent callback slots");
    game.SetPostWeaponFireHandler(nullptr);
    game.SetTaskSimpleBeHitHandler(nullptr);
    Check(first.GetPostWeaponFireHandler() == nullptr && first.GetTaskSimpleBeHitHandler() == nullptr, "SDK null registration clears remaining callbacks");
    Check(handlerCalls == 0, "registration and retrieval never pretend to dispatch native weapon hooks");

    for (int weapon = WEAPONTYPE_UNARMED; weapon <= WEAPONTYPE_LAST_WEAPONTYPE; ++weapon)
        Check(!game.GetJetpackWeaponEnabled(static_cast<eWeaponType>(weapon)), "actual SDK jetpack policy starts disabled before map configuration");
    for (int weapon = WEAPONTYPE_BRASSKNUCKLE; weapon < WEAPONTYPE_LAST_WEAPONTYPE; ++weapon)
    {
        game.SetJetpackWeaponEnabled(static_cast<eWeaponType>(weapon), true);
        Check(game.GetJetpackWeaponEnabled(static_cast<eWeaponType>(weapon)), "actual SDK accepts every classic jetpack policy row");
        Check(!other.GetJetpackWeaponEnabled(static_cast<eWeaponType>(weapon)), "actual game instances own independent jetpack policy tables");
        game.SetJetpackWeaponEnabled(static_cast<eWeaponType>(weapon), false);
        Check(!game.GetJetpackWeaponEnabled(static_cast<eWeaponType>(weapon)), "actual SDK can clear each accepted jetpack permission");
    }
    for (const auto weapon : {static_cast<eWeaponType>(-1), WEAPONTYPE_UNARMED, WEAPONTYPE_LAST_WEAPONTYPE, WEAPONTYPE_INVALID})
    {
        game.SetJetpackWeaponEnabled(weapon, true);
        Check(!game.GetJetpackWeaponEnabled(weapon), "actual SDK rejects malformed and excluded jetpack IDs");
    }
    Refuses([&] { game.SetSystemState(SystemState::GS_FRONTEND); }, "SDK refuses a state write without an admitted native owner");
    Check(timing.stateWrites == 1 && timing.requestedState == SystemState::GS_FRONTEND, "SDK forwards the exact typed state request to its existing owner");
    timing.stateResult = mta::game_de::ClockResult::Ok;
    game.SetSystemState(SystemState::GS_PLAYING_GAME);
    Check(timing.stateWrites == 2 && timing.requestedState == SystemState::GS_PLAYING_GAME, "SDK accepts only the native owner's successful state result");
    timing.stateResult = mta::game_de::ClockResult::Busy;
    Refuses([&] { game.SetSystemState(SystemState::GS_INIT_PLAYING_GAME); }, "a competing native state transition remains an explicit SDK refusal");
    Check(timing.stateWrites == 3, "SDK does not retry a refused state write over native progress");
    Refuses([&] { game.VerifySADataFileNames(); }, "SDK filename verification refuses the ordinary test executable instead of declaring its data valid");
    LegacyTimingAccess legacyTiming;
    for (bool sentinel : {false, true})
    {
        bool output = sentinel;
        Check(legacyTiming.ReadWorldCheat(MtaGame::CWorldCheatPolicy::Id::TankMode, output) == mta::game_de::ClockResult::NotInitialized && output == sentinel,
              "older timing owners explicitly refuse the added logical read without changing caller output");
    }
    for (const auto* name : std::array<const char*, 8>{nullptr, "", "Hovercars", "hovercars ", "RIPAZHA", "walk_underwater", "neverWanted", "KANGAROO"})
    {
        Check(!game.IsCheatEnabled(name), "unknown and noncanonical names preserve MTA's false result");
        Check(!game.SetCheatEnabled(name, true), "unknown/native command aliases never reach world-property writes");
    }
    Check(timing.worldCheatReads == 0 && timing.cheatReads == 0 && timing.cheatWrites == 0,
          "unknown names never ask either native access interface for a binding");
    constexpr std::array<bool, 19> availableWorldCheats = {true,  true, true,  true,  true, true,  true, true, true, true,
                                                           false, true, false, false, true, false, true, true, false};
    for (std::size_t index = 0; index != MtaGame::CWorldCheatPolicy::Members.size(); ++index)
    {
        const auto& member = MtaGame::CWorldCheatPolicy::Members[index];
        timing.worldCheatResult = mta::game_de::ClockResult::NotInitialized;
        auto reads = timing.worldCheatReads;
        Refuses([&] { game.IsCheatEnabled(member.name); }, "every known logical name requires its actual existing owner even before binding");
        Check(timing.worldCheatReads == reads + 1 && timing.requestedWorldCheat == member.id,
              "known names forward the exact shared logical identity once instead of becoming unknown-name false");
        timing.worldCheatResult = mta::game_de::ClockResult::UnsupportedBuild;
        reads = timing.worldCheatReads;
        Refuses([&] { game.IsCheatEnabled(member.name); }, "known unavailable native binding is an explicit SDK refusal rather than disabled success");
        Check(timing.worldCheatReads == reads + 1, "unavailable native rows are not retried or guessed from a previous read");
        if (availableWorldCheats[index])
        {
            timing.worldCheatResult = mta::game_de::ClockResult::Ok;
            timing.worldCheatValue = true;
            reads = timing.worldCheatReads;
            Check(game.IsCheatEnabled(member.name) && timing.worldCheatReads == reads + 1 && timing.requestedWorldCheat == member.id,
                  "each qualified logical SDK getter returns the actual owner-provided enabled result");
            timing.worldCheatValue = false;
            Check(!game.IsCheatEnabled(member.name) && timing.worldCheatReads == reads + 2,
                  "disabled native result is freshly read without a cached enabled value");
            timing.worldCheatResult = mta::game_de::ClockResult::OwnerConflict;
            Refuses([&] { game.IsCheatEnabled(member.name); }, "lost owner after a successful read cannot fall back to cached native state");
            Check(timing.worldCheatReads == reads + 3, "read refusal forwards exactly once without a native retry");
        }
        if (!member.canBeSet)
        {
            timing.cheatResult = mta::game_de::ClockResult::Ok;
            const auto writes = timing.cheatWrites;
            reads = timing.worldCheatReads;
            Check(!game.SetCheatEnabled(member.name, true) && !game.SetCheatEnabled(member.name, false) && timing.cheatWrites == writes &&
                      timing.worldCheatReads == reads,
                  "all fifteen read-only logical names retain their existing SDK write prohibition");
        }
    }
    Check(timing.cheatReads == 0, "expanded logical SDK reads never fall back to the legacy four-flag interface");
    for (std::size_t index = 0; index < mta::game_de::NativeCheatProfile::Members.size(); ++index)
    {
        const auto* name = mta::game_de::NativeCheatProfile::Members[index].sdkName;
        const auto  flag = static_cast<mta::game_de::NativeCheatProfile::Flag>(index);
        timing.cheatResult = mta::game_de::ClockResult::NotInitialized;
        Refuses([&] { game.SetCheatEnabled(name, true); }, "existing four-property setter requires the admitted native write owner");
        timing.cheatResult = mta::game_de::ClockResult::Ok;
        Check(game.SetCheatEnabled(name, true) && timing.requestedFlag == flag && timing.requestedEnabled,
              "the four settable names preserve their existing exact native Flag enable route");
        Check(game.SetCheatEnabled(name, false) && timing.requestedFlag == flag && !timing.requestedEnabled,
              "the four settable names preserve their existing exact native Flag disable route");
        timing.cheatResult = mta::game_de::ClockResult::Busy;
        const auto before = timing.cheatWrites;
        Refuses([&] { game.SetCheatEnabled(name, true); }, "intervening native changes remain explicit SDK setter refusals");
        Check(timing.cheatWrites == before + 1, "SDK setter never retries over a competing native property change");
    }
    // Only compare this fixture identity; the owner fixture separately exercises
    // the bank's actual polymorphic objects against its qualified native image.
    unsigned char weaponIdentity = 0;
    timing.weaponResultPointer = reinterpret_cast<CWeaponInfo*>(&weaponIdentity);
    Refuses([&] { game.GetWeaponInfo(WEAPONTYPE_PISTOL, WEAPONSKILL_PRO); }, "SDK weapon lookup requires the existing admitted owner");
    Check(timing.weaponReads == 1 && timing.requestedWeaponSlot == 58, "shared MTA selector forwards the exact professional-pistol slot");
    timing.weaponResult = mta::game_de::ClockResult::Ok;
    Check(game.GetWeaponInfo(WEAPONTYPE_TEC9, WEAPONSKILL_SPECIAL) == timing.weaponResultPointer && timing.requestedWeaponSlot == 79,
          "actual CGame dispatch forwards the final genuine wrapper identity from its native owner");
    const auto weaponReadsBeforeInvalid = timing.weaponReads;
    Check(game.GetWeaponInfo(WEAPONTYPE_INVALID) == nullptr && timing.weaponReads == weaponReadsBeforeInvalid,
          "invalid SDK weapon selection never requests native row access");
    timing.weaponResult = mta::game_de::ClockResult::Busy;
    Refuses([&] { game.GetWeaponInfo(WEAPONTYPE_PISTOL); }, "competing native weapon access remains an explicit SDK refusal");
    Check(timing.weaponReads == weaponReadsBeforeInvalid + 1, "SDK never retries a refused native wrapper request");
    timing.weaponResult = mta::game_de::ClockResult::Ok;
    timing.weaponResultPointer = nullptr;
    Refuses([&] { game.GetWeaponInfo(WEAPONTYPE_PISTOL); }, "accepted weapon selection cannot publish an empty wrapper registry");
    unsigned char statsIdentity = 0;
    timing.statsPointer = reinterpret_cast<CStats*>(&statsIdentity);
    Refuses([&] { game.GetStats(); }, "actual stats SDK publication requires the existing admitted native owner");
    Check(timing.statsReads == 1, "stats lookup forwards once and preserves an explicit unavailable result");
    timing.statsResult = mta::game_de::ClockResult::Ok;
    Check(game.GetStats() == timing.statsPointer && timing.statsReads == 2, "actual CGame returns the exact borrowed SDK stats identity");
    timing.statsResult = mta::game_de::ClockResult::Busy;
    Refuses([&] { game.GetStats(); }, "competing native stats ownership remains an explicit SDK refusal");
    Check(timing.statsReads == 3, "SDK publication does not retry over competing stats ownership");
    timing.statsResult = mta::game_de::ClockResult::Ok;
    timing.statsPointer = nullptr;
    Refuses([&] { game.GetStats(); }, "a successful owner result cannot publish a missing actual stats service");
    alignas(void*) unsigned char playerInfoIdentity = 0;
    timing.playerInfoPointer = reinterpret_cast<CPlayerInfo*>(&playerInfoIdentity);
    Refuses([&] { game.GetPlayerInfo(); }, "actual player-info SDK publication requires its admitted native owner");
    Check(timing.playerInfoReads == 1, "player-info lookup forwards once without hiding unavailable ownership");
    timing.playerInfoResult = mta::game_de::ClockResult::Ok;
    Check(game.GetPlayerInfo() == timing.playerInfoPointer && timing.playerInfoReads == 2,
          "CGame returns the exact complete borrowed player-info SDK identity");
    timing.playerInfoResult = mta::game_de::ClockResult::Busy;
    Refuses([&] { game.GetPlayerInfo(); }, "competing player-info ownership is an explicit SDK refusal");
    Check(timing.playerInfoReads == 3, "SDK publication never retries competing player-info ownership");
    timing.playerInfoResult = mta::game_de::ClockResult::Ok;
    timing.playerInfoPointer = nullptr;
    Refuses([&] { game.GetPlayerInfo(); }, "a successful result cannot publish a missing complete player-info service");
    Check(timing.playerInfoReads == 4, "null player-info publication does not retry or use stale identity");
    LegacyTimingAccess legacyPlayerInfo;
    auto*              defaultPlayerInfo = reinterpret_cast<CPlayerInfo*>(&playerInfoIdentity);
    Check(legacyPlayerInfo.GetPlayerInfo(defaultPlayerInfo) == mta::game_de::ClockResult::NotInitialized &&
              defaultPlayerInfo == reinterpret_cast<CPlayerInfo*>(&playerInfoIdentity),
          "legacy owners refuse default player-info borrowing without overwriting output");
    unsigned char weatherIdentity = 0;
    timing.weatherPointer = reinterpret_cast<CWeather*>(&weatherIdentity);
    Refuses([&] { game.GetWeather(); }, "actual weather SDK publication requires the existing admitted native owner");
    Check(timing.weatherReads == 1, "weather lookup forwards once and preserves an explicit unavailable result");
    timing.weatherResult = mta::game_de::ClockResult::Ok;
    Check(game.GetWeather() == timing.weatherPointer && timing.weatherReads == 2, "actual CGame returns the exact borrowed complete weather SDK identity");
    timing.weatherResult = mta::game_de::ClockResult::Busy;
    Refuses([&] { game.GetWeather(); }, "competing native weather ownership remains an explicit SDK refusal");
    Check(timing.weatherReads == 3, "SDK publication never retries over competing weather ownership");
    timing.weatherResult = mta::game_de::ClockResult::Ok;
    timing.weatherPointer = nullptr;
    Refuses([&] { game.GetWeather(); }, "a successful owner result cannot publish a missing actual weather service");
    const auto stateWritesBeforeStart = timing.stateWrites;
    Refuses([&] { game.StartGame(); }, "actual CGame StartGame requires a separately admitted native startup owner");
    Check(timing.startRequests == 1 && timing.stateWrites == stateWritesBeforeStart, "unavailable StartGame is forwarded once without a synthetic state write");
    timing.startResult = mta::game_de::ClockResult::Ok;
    game.StartGame();
    Check(timing.startRequests == 2 && timing.stateWrites == stateWritesBeforeStart,
          "actual CGame accepts the native startup request without appending state intent");
    timing.startResult = mta::game_de::ClockResult::OwnerConflict;
    Refuses([&] { game.StartGame(); }, "a consumed or competing native startup request remains an explicit SDK refusal");
    Check(timing.startRequests == 3 && timing.stateWrites == stateWritesBeforeStart,
          "SDK StartGame neither retries nor fabricates the native loading transaction");
    Check(!reader.IsBound() && !streaming.IsBound() && !state.IsBound(), "CPU SDK policies require no native service binding");

    std::printf("Passed %u actual CGameDE CPU SDK policy checks (%zu-bit)\n", checks, sizeof(void*) * 8);
    return EXIT_SUCCESS;
}
