/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual player movement-group dispatch over the real shared SDK base
 ****************************************************************************/
// Reuse strict definitions only. Their historical main is compiled under a
// different name and never executed; current checks start at zero.
#pragma push_macro("main")
#undef main
#define main HistoricalSharedPlayerDefinitionsMain
#include "PedPlayerSharedBaseDETests.cpp"
#undef main
#pragma pop_macro("main")
#include "../../Client/game_de/CPlayerPedMoveAnimDE.h"
#include "PlayerMoveAnimFixture.generated.h"

namespace player_move_anim_sdk_tests
{
    using namespace mta::game_de;
    using Player = player_move_anim_fixture::PlayerMoveAnimFixture;
    struct Move final : NativePedMoveAnimAccess
    {
        unsigned                       calls = 0;
        ClockResult                    result = ClockResult::Ok;
        std::int32_t                   value = 0;
        NativePedReferenceToken        received{};
        const NativePedReferenceToken* forbiddenCaller = nullptr;
        const NativePedReferenceToken* forbiddenPedToken = nullptr;
        bool                           aliased = false, stable = true;
        void (*callback)(void*) noexcept = nullptr;
        void*       context = nullptr;
        ClockResult ReadPedMoveAnim(const NativePedReferenceToken& token, std::int32_t& output) noexcept override
        {
            ++calls;
            received = token;
            aliased = aliased || &token == forbiddenCaller || &token == forbiddenPedToken;
            const auto before = token;
            if (callback)
                callback(context);
            stable = stable && SameToken(before, token);
            // Deliberately poison output even on refusal. The SDK must report
            // the exact refusal rather than publishing that apparent value.
            output = value;
            return result;
        }
    };
    struct Missing final : NativePedMoveAnimAccess
    {
    };
    struct Callback
    {
        NativePedReferenceToken* caller = nullptr;
        CPlayerPed*              peer = nullptr;
        unsigned                 calls = 0;
        eMoveAnim                observed = MOVE_DEFAULT;
    };
    void RebindCallerAndReadPeer(void* opaque) noexcept
    {
        auto& state = *static_cast<Callback*>(opaque);
        ++state.calls;
        *state.caller = {};
        state.observed = state.peer->GetMoveAnim();
    }
    Player Make(CGame& game, OtherOwners& other, Grant& grant, Clear& clear, NativePedClothesAccess& clothes, NativePedRebuildAccess& rebuild,
                NativePedMoveAnimAccess& move, const NativePedReferenceToken& token, ped_rebuild_fixture::TaskScenario& tasks,
                ped_rebuild_fixture::WeaponLookup& lookup, CWanted* wanted)
    {
        return Player(game, other, other, other, other, other, other, other, other, other, other, other, other, other, other, other, other, other, other, other,
                      other, other, other, other, grant, clear, clothes, rebuild, move, token, 7u, tasks, lookup, wanted);
    }
    bool Refused(CPlayerPed& player, ClockResult expected)
    {
        try
        {
            (void)player.GetMoveAnim();
        }
        catch (const std::runtime_error& error)
        {
            const std::string message(error.what());
            return message.find("GetMoveAnim") != std::string::npos &&
                   message.find(": " + std::to_string(static_cast<unsigned>(expected))) != std::string::npos;
        }
        return false;
    }
}
static_assert(std::is_abstract_v<mta::game_de::CPlayerPedMoveAnimDE>);
static_assert(std::is_base_of_v<mta::game_de::CPlayerPedWantedDE, mta::game_de::CPlayerPedMoveAnimDE>);
static_assert(std::is_same_v<decltype(&mta::game_de::CPlayerPedMoveAnimDE::GetMoveAnim), eMoveAnim (mta::game_de::CPlayerPedMoveAnimDE::*)()>);

int main()
{
    try
    {
        using namespace player_move_anim_sdk_tests;
        if (checks != 0)
            throw std::runtime_error("Historical shared-player cases unexpectedly executed");
        ped_rebuild_fixture::Game                  game;
        ped_rebuild_fixture::TaskScenario          tasks;
        ped_rebuild_fixture::WeaponLookup          lookup;
        OtherOwners                                other;
        Grant                                      grant;
        Clear                                      clear;
        shared_player_base_fixture::Clothes        clothes;
        shared_player_base_fixture::Rebuild        rebuild;
        shared_player_base_fixture::WantedCounters wantedCounters;
        shared_player_base_fixture::WantedView     wanted(wantedCounters);
        Move                                       first, second;
        NativePedReferenceToken                    caller{0xfedcba9876543210ull, 0x8000000000000001ull, 0xfffffffeu};
        const auto                                 expected = caller;
        const NativePedReferenceToken              otherToken{17, 29, 3};
        auto                                       player = Make(game, other, grant, clear, clothes, rebuild, first, caller, tasks, lookup, &wanted);
        auto                                       peer = Make(game, other, grant, clear, clothes, rebuild, second, otherToken, tasks, lookup, nullptr);
        CPlayerPed&                                sdk = player;
        first.forbiddenCaller = &caller;
        first.forbiddenPedToken = player.FixtureTokenAddress();
        second.forbiddenPedToken = peer.FixtureTokenAddress();
        Check(!first.calls && !second.calls && !tasks.eventCount && !lookup.count && !grant.calls && !clear.calls,
              "construction only borrows the explicit registered identity and owners; no native operation or factory runs");
        CPed* implementation = static_cast<mta::game_de::CPedRebuildDE*>(&player);
        Check(static_cast<CPed*>(&sdk) == implementation && dynamic_cast<CPlayerPed*>(implementation) == &sdk &&
                  dynamic_cast<mta::game_de::CPlayerPedMoveAnimDE*>(implementation) == static_cast<mta::game_de::CPlayerPedMoveAnimDE*>(&player),
              "actual player getter and Rebuild implementation share one virtual CPed identity and adjusted SDK dispatch");
        player.FixtureCustomMoveAnim(-1234567);
        first.value = MOVE_PLAYER;
        Check(sdk.GetMoveAnim() == MOVE_PLAYER && implementation->GetCustomMoveAnim() == -1234567 && first.calls == 1 && SameToken(first.received, expected) &&
                  !first.aliased && !second.calls,
              "GetMoveAnim observes the actual owner group once and never substitutes the unrelated signed custom CPU cache");
        // eMoveAnim is an unfixed enum with enumerators0..138, giving a
        // language-defined0..255 range. Raw negative/high-bit DWORD payloads
        // are exercised by native int32 tests, not invalid enum objects here.
        for (std::int32_t value = 0; value <= 255; ++value)
        {
            first.value = value;
            const auto before = first.calls;
            Check(static_cast<int>(sdk.GetMoveAnim()) == value && first.calls == before + 1 && SameToken(first.received, expected) &&
                      implementation->GetCustomMoveAnim() == -1234567 && !first.aliased && !second.calls,
                  "every language-defined enum value including setter-policy holes crosses actual SDK dispatch without filtering or cache mutation");
        }
        second.value = MOVE_SKATE;
        Callback callback{&caller, &peer};
        first.value = MOVE_SNEAK;
        first.callback = &RebindCallerAndReadPeer;
        first.context = &callback;
        const auto beforeNested = first.calls;
        Check(sdk.GetMoveAnim() == MOVE_SNEAK && first.calls == beforeNested + 1 && callback.calls == 1 && callback.observed == MOVE_SKATE &&
                  second.calls == 1 && SameToken(first.received, expected) && SameToken(second.received, otherToken) && caller.version == 0 && first.stable &&
                  !first.aliased && !second.aliased,
              "callback mutation of the caller token and nested peer dispatch preserve the selected copied identity and receiver isolation");
        first.callback = nullptr;
        const std::array<ClockResult, 14> failures{
            {ClockResult::InvalidArgument, ClockResult::NotInitialized, ClockResult::WrongThread, ClockResult::BindingChanged, ClockResult::ReadFailure,
             ClockResult::WriteFailure, ClockResult::RestoreFailure, ClockResult::PatchFailure, ClockResult::Busy, ClockResult::Expired,
             ClockResult::UnsupportedBuild, ClockResult::OwnerConflict, ClockResult::Stopped, ClockResult::OriginalException}};
        for (const auto status : failures)
        {
            first.result = status;
            first.value = MOVE_DEFAULT;
            const auto before = first.calls;
            Check(Refused(sdk, status) && first.calls == before + 1 && SameToken(first.received, expected) && second.calls == 1 &&
                      implementation->GetCustomMoveAnim() == -1234567 && sdk.GetWanted() == &wanted,
                  "each explicit owner refusal propagates once with operation and code, without publishing output, retrying or changing CPU associations");
        }
        Missing missing;
        auto    unavailable = Make(game, other, grant, clear, clothes, rebuild, missing, expected, tasks, lookup, nullptr);
        Check(Refused(unavailable, ClockResult::NotInitialized), "the default owner refuses rather than inventing a default movement group");
        first.result = ClockResult::Ok;
        first.value = MOVE_JETPACK;
        {
            auto temporary = Make(game, other, grant, clear, clothes, rebuild, first, expected, tasks, lookup, &wanted);
            Check(static_cast<CPlayerPed&>(temporary).GetMoveAnim() == MOVE_JETPACK,
                  "a temporary borrowed SDK view uses its supplied owner without acquiring lifetime");
        }
        const auto beforeFinal = first.calls;
        Check(sdk.GetMoveAnim() == MOVE_JETPACK && first.calls == beforeFinal + 1 && SameToken(first.received, expected) && sdk.GetWanted() == &wanted &&
                  peer.GetWanted() == nullptr && wantedCounters.constructed == 1 && !wantedCounters.methods && !wantedCounters.destroyed && !grant.calls &&
                  !clear.calls && !tasks.eventCount && !lookup.count,
              "retiring another view adds no native lifetime, wanted, weapon, task, rebuild or lookup effects and leaves this receiver usable");
        std::puts("Actual player movement animation SDK fixture completed");
        std::printf("Passed %u actual player movement animation SDK checks (%zu-bit; no native engine code executed)\n", checks, sizeof(void*) * 8);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "FAIL after %u checks: %s\n", checks, error.what());
        return 1;
    }
}
