/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Qualify startup collision prerequisites without a collision attempt
 ****************************************************************************/
#pragma push_macro("main")
#undef main
#define main CollisionReadinessHistoricalDefinitionsMain
#include "NativeStartupModelCollisionTests.cpp"
#undef main
#pragma pop_macro("main")
#include "../../Client/game_de/NativeStartupPreparationAccess.h"
#include "../../Client/game_de/NativeGameTimingAccess.h"
#include <game/CGameModelPreparationPolicy.h>

namespace CollisionReadinessTests
{
    SpecialFixture* fixture = nullptr;
    void            ChangeRoot()
    {
        fixture->Global(NativeStreamingLoadAllProfile::RootRva, std::uintptr_t{0});
    }
    class OlderOwner final : public mta::game_de::NativeGameTimingAccess
    {
    public:
        ClockResult SetTimeScale(float) noexcept override { return ClockResult::NotInitialized; }
        ClockResult SetMinuteDuration(std::uint32_t) noexcept override { return ClockResult::NotInitialized; }
        ClockResult Pause(bool) noexcept override { return ClockResult::NotInitialized; }
    };
}

int main(int argc, char** argv)
{
    namespace C = mta::game_de::NativeStartupModelCollision;
    namespace Scope = mta::game_de::NativeStreamingCompletionContext;
    namespace P = mta::game_de::NativeModelObjectProfile;
    using Signature = ClockResult (*)(std::uintptr_t, C::CheckOwner) noexcept;
    static_assert(std::is_same_v<decltype(&C::CheckReady), Signature>);
    static_assert(std::is_base_of_v<mta::game_de::NativeStartupPreparationAccess, mta::game_de::NativeGameTimingAccess>);
    Check(argc == 2 && std::filesystem::path(argv[1]).is_absolute(), "collision readiness uses owned workspace evidence");
    CollisionReadinessTests::OlderOwner           older;
    mta::game_de::NativeStartupPreparationAccess& optional = older;
    Check(optional.RequireStartupPedModelCreationReady() == ClockResult::NotInitialized &&
              optional.RequireStartupSpecialModelRequestReady() == ClockResult::NotInitialized &&
              optional.RequireStartupCollisionCopyReady() == ClockResult::NotInitialized,
          "older retained owners explicitly refuse each new capability without native effects or a replacement ready hook");
    SpecialFixture fixture(argv[1]);
    CollisionReadinessTests::fixture = &fixture;
    const auto image = fixture.transform.image.image;
    const auto reset = [&]
    {
        fixture.Reset();
        PoolTestHost::image = image;
        fixture.transform.image.Patch(P::CollisionConsumerRva, P::CollisionConsumer.data(), P::CollisionConsumer.size(), PAGE_EXECUTE_READ);
        MtaGame::CGameModelPreparationPolicy::SetupSpecialCharacters([&](std::uint32_t key, const char*) { fixture.Slot(key, 0); });
        fixture.Slot(3118, 0);
        fixture.Slot(3059, 0);
        fixture.Slot(3553, 0);
        fixture.Slot(3554, 0);
        PoolTestHost::ownerResult = ClockResult::Ok;
        PoolTestHost::reads = PoolTestHost::failAt = PoolTestHost::ownerCalls = PoolTestHost::revokeAt = 0;
        PoolTestHost::watched = 0;
        PoolTestHost::after = nullptr;
        StartupCollisionTests::exchanges = 0;
    };
    const auto noEffects = [&]
    {
        Check(!StartupCollisionTests::exchanges, "readiness never calls the native field exchange seam");
        for (const auto key : {3118u, 3059u, 3553u, 3554u})
        {
            std::uintptr_t value = 1;
            std::memcpy(&value, reinterpret_cast<const void*>(image + NativeModelBankProfile::BankRva + key * NativeModelBankProfile::SlotBytes),
                        sizeof(value));
            Check(value == 0, "collision and future model metadata are not required or changed by capability readiness");
        }
        Check(Scope::TryEnter(), "temporary completion scope retires after every readiness outcome");
        Scope::Leave();
    };
    reset();
    Check(C::CheckReady(image, nullptr) == ClockResult::NotInitialized, "missing owner refuses before native graph observation");
    noEffects();
    Check(C::CheckReady(image, &PoolTestHost::Owner) == ClockResult::Ok, "current structural collision/world capability qualifies with absent model rows");
    const auto validReads = PoolTestHost::reads, validOwners = PoolTestHost::ownerCalls;
    noEffects();
    Check(C::CheckReady(image, &PoolTestHost::Owner) == ClockResult::Ok, "readiness has no completion latch and freshly repeats");
    noEffects();
    constexpr std::array<ClockResult, 14> failures{
        {ClockResult::InvalidArgument, ClockResult::NotInitialized, ClockResult::WrongThread, ClockResult::BindingChanged, ClockResult::ReadFailure,
         ClockResult::WriteFailure, ClockResult::RestoreFailure, ClockResult::PatchFailure, ClockResult::Busy, ClockResult::Expired,
         ClockResult::UnsupportedBuild, ClockResult::OwnerConflict, ClockResult::Stopped, ClockResult::OriginalException}};
    for (const auto failure : failures)
    {
        reset();
        PoolTestHost::ownerResult = failure;
        Check(C::CheckReady(image, &PoolTestHost::Owner) == failure, "exact current owner refusal propagates without translation or a native attempt");
        noEffects();
    }
    reset();
    Check(Scope::TryEnter(), "hold only the ordinary fixture completion scope");
    Check(C::CheckReady(image, &PoolTestHost::Owner) == ClockResult::Busy, "readiness cannot borrow an already active loading operation scope");
    Scope::Leave();
    noEffects();
    reset();
    fixture.Global(NativeStreamingLoadAllProfile::RootRva, std::uintptr_t{0});
    Check(C::CheckReady(image, &PoolTestHost::Owner) != ClockResult::Ok,
          "missing world refuses without treating a legitimate later conditional skip as current write authority");
    noEffects();
    reset();
    const unsigned char bad = P::CollisionConsumer[0] ^ 1;
    fixture.transform.image.Patch(P::CollisionConsumerRva, &bad, 1, PAGE_EXECUTE_READ);
    Check(C::CheckReady(image, &PoolTestHost::Owner) == ClockResult::BindingChanged, "collision field consumer proof remains a genuine readiness dependency");
    noEffects();
    reset();
    PoolTestHost::watched = image + NativeStreamingLoadAllProfile::RootRva;
    PoolTestHost::after = &CollisionReadinessTests::ChangeRoot;
    Check(C::CheckReady(image, &PoolTestHost::Owner) != ClockResult::Ok, "world identity revocation during observation cannot publish capability readiness");
    noEffects();
    for (const auto at : {1u, (validOwners + 1) / 2, validOwners})
    {
        reset();
        PoolTestHost::revokeAt = at;
        Check(C::CheckReady(image, &PoolTestHost::Owner) != ClockResult::Ok, "early, middle and final owner revocation remain explicit refusal");
        noEffects();
    }
    for (const auto at : {1u, (validReads + 1) / 2, validReads})
    {
        reset();
        PoolTestHost::failAt = at;
        Check(C::CheckReady(image, &PoolTestHost::Owner) != ClockResult::Ok, "early, middle and final guarded-read faults preserve refusal");
        noEffects();
    }
    std::puts("Production native startup collision readiness fixture completed");
    std::printf("Passed %u native startup collision readiness checks (64-bit; no native engine code executed)\n", checks);
    return 0;
}
