/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual RebuildPlayer dispatch over the existing borrowed ped chain
 ****************************************************************************/
// Reuse strict owner/rejector definitions, never their historical cases. The
// old main has no static initialization and is deliberately never invoked.
#pragma push_macro("main")
#undef main
#define main HistoricalPedClothesDefinitionsMain
#include "PedClothesDETests.cpp"
#undef main
#pragma pop_macro("main")
#include "../../Client/game_de/CPedRebuildDE.h"

namespace ped_rebuild_fixture
{
    using ped_clothes_fixture::Unqualified;
}
#include "PedRebuildFixture.generated.h"

namespace
{
    struct RejectClothes final : NativePedClothesAccess
    {
        ClockResult SetPedClothes(const NativePedReferenceToken&, const char*, const char*, std::uint8_t) noexcept override
        {
            ped_clothes_fixture::Unqualified("SetPedClothes during RebuildPlayer");
        }
    };
    struct Rebuild final : NativePedRebuildAccess
    {
        unsigned                       calls = 0;
        ClockResult                    result = ClockResult::Ok;
        NativePedReferenceToken        received{};
        const NativePedReferenceToken* forbiddenBaseAddress = nullptr;
        const NativePedReferenceToken* forbiddenCallerAddress = nullptr;
        bool                           aliased = false;
        bool                           stableAcrossCallback = true;
        void (*callback)(void*) noexcept = nullptr;
        void*       context = nullptr;
        ClockResult RebuildPed(const NativePedReferenceToken& token) noexcept override
        {
            ++calls;
            received = token;
            aliased = aliased || &token == forbiddenBaseAddress || &token == forbiddenCallerAddress;
            const auto before = token;
            if (callback)
                callback(context);
            stableAcrossCallback = stableAcrossCallback && SameToken(before, token);
            return result;
        }
    };
    struct UnavailableRebuild final : NativePedRebuildAccess
    {
    };
    struct RebuildContext
    {
        NativePedReferenceToken* caller = nullptr;
        CPed*                    otherPed = nullptr;
        unsigned                 calls = 0;
    };
    void RebindAndDispatchOther(void* opaque) noexcept
    {
        auto& context = *static_cast<RebuildContext*>(opaque);
        ++context.calls;
        *context.caller = {};
        context.otherPed->RebuildPlayer();
    }
    ped_rebuild_fixture::PedRebuildFixture MakeRebuildPed(CGame& game, OtherOwners& other, Grant& grant, Clear& clear, NativePedClothesAccess& clothes,
                                                          NativePedRebuildAccess& rebuild, const NativePedReferenceToken& token,
                                                          ped_rebuild_fixture::TaskScenario& tasks, ped_rebuild_fixture::WeaponLookup& lookup)
    {
        return ped_rebuild_fixture::PedRebuildFixture(game, other, other, other, other, other, other, other, other, other, other, other, other, other, other,
                                                      other, other, other, other, other, other, other, other, other, grant, clear, clothes, rebuild, token, 7u,
                                                      tasks, lookup);
    }
    bool RebuildRefused(CPed& ped, ClockResult expected)
    {
        try
        {
            ped.RebuildPlayer();
        }
        catch (const std::runtime_error& error)
        {
            const std::string text(error.what());
            return text.find("RebuildPlayer") != std::string::npos && text.find(": " + std::to_string(static_cast<unsigned>(expected))) != std::string::npos;
        }
        return false;
    }
}

static_assert(std::is_abstract_v<mta::game_de::CPedRebuildDE>);
static_assert(std::is_base_of_v<mta::game_de::CPedClothesDE, mta::game_de::CPedRebuildDE>);
static_assert(std::is_same_v<decltype(&mta::game_de::CPedRebuildDE::RebuildPlayer), void (mta::game_de::CPedRebuildDE::*)()>);

int main()
{
    try
    {
        if (checks != 0)
            throw std::runtime_error("Historical clothing cases unexpectedly executed");
        ped_rebuild_fixture::Game         game;
        ped_rebuild_fixture::TaskScenario tasks;
        ped_rebuild_fixture::WeaponLookup lookup;
        OtherOwners                       other;
        Grant                             grant;
        Clear                             clear;
        RejectClothes                     clothes;
        Rebuild                           first, second;
        NativePedReferenceToken           caller{0xfedcba9876543210ull, 0x8000000000000001ull, 0xfffffffeu};
        const auto                        expected = caller;
        const NativePedReferenceToken     otherToken{17, 29, 3};
        auto                              ped = MakeRebuildPed(game, other, grant, clear, clothes, first, caller, tasks, lookup);
        auto                              otherPed = MakeRebuildPed(game, other, grant, clear, clothes, second, otherToken, tasks, lookup);
        CPed&                             sdk = ped;
        first.forbiddenBaseAddress = ped.FixtureTokenAddress();
        first.forbiddenCallerAddress = &caller;
        second.forbiddenBaseAddress = otherPed.FixtureTokenAddress();
        Check(!first.calls && !second.calls && !tasks.eventCount && !lookup.count,
              "construction only borrows the existing owners and token; no rebuild, data/model initialization or wrapper lookup runs");

        sdk.RebuildPlayer();
        Check(first.calls == 1 && SameToken(first.received, expected) && !first.aliased && !second.calls,
              "actual CPed virtual dispatch forwards one copied full token to its selected rebuild owner without extra arguments");
        RebuildContext context{&caller, &otherPed, 0};
        first.callback = RebindAndDispatchOther;
        first.context = &context;
        sdk.RebuildPlayer();
        Check(first.calls == 2 && SameToken(first.received, expected) && first.stableAcrossCallback && !first.aliased,
              "caller mutation and a nested different receiver cannot change the current copied token or cause a second outer attempt");
        Check(context.calls == 1 && caller.version == 0 && second.calls == 1 && SameToken(second.received, otherToken) && !second.aliased,
              "nested calls preserve separate owner/token identities without wrapper repair or lifetime compensation");
        first.callback = nullptr;
        first.context = nullptr;

        for (const auto status :
             {ClockResult::InvalidArgument, ClockResult::NotInitialized, ClockResult::WrongThread, ClockResult::BindingChanged, ClockResult::ReadFailure,
              ClockResult::WriteFailure, ClockResult::RestoreFailure, ClockResult::PatchFailure, ClockResult::Busy, ClockResult::Expired,
              ClockResult::UnsupportedBuild, ClockResult::OwnerConflict, ClockResult::Stopped, ClockResult::OriginalException})
        {
            first.result = status;
            const auto before = first.calls;
            Check(RebuildRefused(sdk, status) && first.calls == before + 1 && SameToken(first.received, expected) && second.calls == 1 && !tasks.eventCount &&
                      !lookup.count,
                  "each native-owner refusal names RebuildPlayer/status after one attempt; no retry, clothes edit, rebuild fallback or cache operation runs");
        }
        first.result = ClockResult::Ok;
        const auto fresh = first.calls;
        static_cast<CPedRebuildDE&>(ped).RebuildPlayer();
        Check(first.calls == fresh + 1 && SameToken(first.received, expected) && first.stableAcrossCallback && !first.aliased,
              "a later inherited call consults the current owner once and does not cache an earlier refusal");
        UnavailableRebuild unavailable;
        auto               missing = MakeRebuildPed(game, other, grant, clear, clothes, unavailable, expected, tasks, lookup);
        Check(RebuildRefused(missing, ClockResult::NotInitialized), "default access explicitly refuses rather than claiming a completed rebuild");
        const auto before = second.calls;
        {
            auto temporary = MakeRebuildPed(game, other, grant, clear, clothes, second, otherToken, tasks, lookup);
            static_cast<CPed&>(temporary).RebuildPlayer();
        }
        Check(second.calls == before + 1 && SameToken(second.received, otherToken) && !tasks.eventCount && !lookup.count,
              "borrowed wrapper destruction adds no rebuild or data/model release to the one requested operation");
        std::puts("Actual ped rebuild SDK fixture completed");
        std::printf("Passed %u actual ped rebuild SDK checks (%zu-bit; no native engine code executed)\n", checks, sizeof(void*) * 8);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "FAIL after %u checks: %s\n", checks, error.what());
        return 1;
    }
}
