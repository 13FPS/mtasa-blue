/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Exercise the actual CGameDE SDK registration and context policies
 *****************************************************************************/
#include "../../Client/game_de/CGameDE.h"
#include <SharedUtil.Defines.h>
#include <game/CPools.h>

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

    class UnusedTimingAccess final : public mta::game_de::NativeGameTimingAccess
    {
    public:
        mta::game_de::ClockResult SetTimeScale(float) noexcept override { fixture::Unqualified(); }
        mta::game_de::ClockResult SetMinuteDuration(std::uint32_t) noexcept override { fixture::Unqualified(); }
        mta::game_de::ClockResult Pause(bool) noexcept override { fixture::Unqualified(); }
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
        mta::game_de::ClockResult ReadCheatFlag(mta::game_de::NativeCheatProfile::Flag flag, bool& output) noexcept override
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
    for (const auto* name : std::array<const char*, 7>{nullptr, "", "Hovercars", "hovercars ", "tankmode", "noreload", "healtharmormoney"})
    {
        Check(!game.IsCheatEnabled(name), "unqualified registry names follow MTA's unknown-name false result");
        Check(!game.SetCheatEnabled(name, true), "unsupported native actions never reach the four world-property writes");
    }
    Check(timing.cheatReads == 0 && timing.cheatWrites == 0, "unknown names never ask the native owner for access");
    for (std::size_t index = 0; index < mta::game_de::NativeCheatProfile::Members.size(); ++index)
    {
        const auto* name = mta::game_de::NativeCheatProfile::Members[index].sdkName;
        const auto  flag = static_cast<mta::game_de::NativeCheatProfile::Flag>(index);
        timing.cheatResult = mta::game_de::ClockResult::NotInitialized;
        Refuses([&] { game.IsCheatEnabled(name); }, "known native property read requires the existing admitted owner");
        Refuses([&] { game.SetCheatEnabled(name, true); }, "known property setter cannot report success without a native write");
        timing.cheatResult = mta::game_de::ClockResult::Ok;
        timing.cheatValue = true;
        Check(game.IsCheatEnabled(name) && timing.requestedFlag == flag, "SDK property lookup reads the exact native flag identity");
        timing.cheatValue = false;
        Check(!game.IsCheatEnabled(name), "SDK returns the actual owner's disabled flag");
        Check(game.SetCheatEnabled(name, true) && timing.requestedFlag == flag && timing.requestedEnabled,
              "SDK forwards each canonical MTA enable request to its native owner");
        Check(game.SetCheatEnabled(name, false) && timing.requestedFlag == flag && !timing.requestedEnabled, "SDK forwards each canonical MTA disable request");
        timing.cheatResult = mta::game_de::ClockResult::Busy;
        const auto before = timing.cheatWrites;
        Refuses([&] { game.SetCheatEnabled(name, true); }, "intervening native changes remain explicit SDK refusals");
        Check(timing.cheatWrites == before + 1, "SDK never retries over a competing native property change");
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
    Check(!reader.IsBound() && !streaming.IsBound() && !state.IsBound(), "CPU SDK policies require no native service binding");

    std::printf("Passed %u actual CGameDE CPU SDK policy checks (%zu-bit)\n", checks, sizeof(void*) * 8);
    return EXIT_SUCCESS;
}
