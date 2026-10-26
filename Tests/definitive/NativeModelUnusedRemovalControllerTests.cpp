/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual zero-only family removal without a native reference decrement
 ****************************************************************************/
#pragma push_macro("main")
#undef main
#define main NativeUnusedRemovalHistoricalCycleMain
#include "NativeModelUnmodifiedReferenceCycleTests.cpp"
#undef main
#pragma pop_macro("main")

// Reuse only the owned image/data and real primitive host seams. Historical
// cycle/release mains are compiled under distinct names and never invoked.
#define GetModuleHandleW  PoolTestHost::Module
#define ReadProcessMemory PoolTestHost::Read
#include "../../Client/game_de/NativeModelUnusedRemoval.cpp"
#undef ReadProcessMemory
#undef GetModuleHandleW

namespace
{
    RemoveFixture* unusedCompetitionFixture = nullptr;
    void           RaiseUnusedCountDuringRemovalAdmission()
    {
        unusedCompetitionFixture->Row(1, 4, std::uint32_t{1});
    }
    void UnusedZeroOrPositiveChecks(RemoveFixture& fixture, std::uintptr_t image, bool positive)
    {
        ResetCycleFixture(fixture);
        CycleController controller;
        CycleToken      token;
        ReserveClaimedCycle(controller, token);
        fixture.Row(1, 4, positive ? std::uint32_t{3} : std::uint32_t{0});
        if (positive)
            fixture.Global(AP::HeapRva, std::uintptr_t{0});
        const auto                        before = FinalMapBytes(fixture);
        bool                              removed = positive;
        NativeModelUnusedRemoval::Receipt receipt;
        Check(controller.UnloadUnused(image, cycleGeneration, token, &PoolTestHost::Owner, removed, &receipt) == ClockResult::Ok && removed == !positive &&
                  receipt.countObserved && receipt.count == (positive ? 3u : 0u) && receipt.verified && receipt.removal.entered == !positive &&
                  receipt.removal.returned == !positive,
              "positive count returns false; an occupied zero calls guarded native removal and returns true after complete acknowledgement");
        const auto state = ReadCycle(controller, token);
        Check(FinalMapBytes(fixture) == before && referenceCalls == 0 && finalCalls == 0 && reserveCalls == 0 && publishCalls == 0 &&
                  removeCalls == (positive ? 0u : 1u) && state.phase == CyclePhase::Bound && state.unusedRemovalCompleted && state.unusedRemoval.verified &&
                  !state.acquisition.entered && !controller.IsRetained() && !controller.RecoveryPending(),
              "zero-only removal never decrements or inserts a reference and keeps its independent completed receipt without inventing a held count");
        Check(controller.RevokeUnacquired(cycleGeneration) == ClockResult::Ok && !controller.HasPending(),
              "acknowledged unused removal leaves only revocable CPU family metadata");
    }
    void UnusedReleasedFamilyChecks(RemoveFixture& fixture, std::uintptr_t image)
    {
        ResetCycleFixture(fixture);
        CycleController controller;
        CycleToken      token;
        ReserveClaimedCycle(controller, token);
        Check(controller.Acquire(image, cycleGeneration, token, &PoolTestHost::Owner) == ClockResult::Ok &&
                  controller.Release(image, cycleGeneration, token, FR::ZeroPolicy::KeepResident, &PoolTestHost::Owner) == ClockResult::Ok,
              "released-family unused removal starts with an actually acknowledged backing cycle");
        const auto before = FinalMapBytes(fixture);
        bool       removed = false;
        Check(controller.UnloadUnused(image, cycleGeneration, token, &PoolTestHost::Owner, removed) == ClockResult::Ok && removed &&
                  ReadCycle(controller, token).phase == CyclePhase::Released && FinalMapBytes(fixture) == before && referenceCalls == 1 && finalCalls == 1 &&
                  removeCalls == 1,
              "a Released family may remove its occupied zero without a second reference release or revival of the consumed token");
    }
    void UnusedDomainRefusalChecks(RemoveFixture& fixture, std::uintptr_t image)
    {
        for (unsigned mode = 0; mode != 5; ++mode)
        {
            ResetCycleFixture(fixture);
            CycleController controller;
            CycleToken      token;
            Check(controller.Reserve(cycleGeneration, cycleModel, &PoolTestHost::Owner, token) == ClockResult::Ok &&
                      controller.Bind(cycleGeneration, token, &PoolTestHost::Owner) == ClockResult::Ok,
                  "unused refusal case owns one real bound family");
            if (mode != 0)
                Check(controller.ClaimUnmodifiedFamily(cycleGeneration, token, &PoolTestHost::Owner) == ClockResult::Ok,
                      "native unused-removal admission uses a positively claimed closed replacement domain");
            if (mode == 1)
                Check(controller.Acquire(image, cycleGeneration, token, &PoolTestHost::Owner) == ClockResult::Ok, "Held refusal has a real backing reference");
            if (mode == 2)
            {
                fixture.ReferenceFixture::Shape(4, 1, false, 1, std::uint32_t{18});
                fixture.Row(1, 4, std::uint32_t{0});
            }
            if (mode == 3)
                fixture.Stream(cycleModel, 0x40, 0);
            auto input = token;
            if (mode == 4)
                ++input.ownerGeneration;
            const auto                        before = FinalMapBytes(fixture);
            bool                              removed = true;
            NativeModelUnusedRemoval::Receipt receipt;
            const auto                        result = controller.UnloadUnused(image, cycleGeneration, input, &PoolTestHost::Owner, removed, &receipt);
            const auto expected = mode <= 1 ? ClockResult::OwnerConflict : mode == 4 ? ClockResult::BindingChanged : ClockResult::NotInitialized;
            Check(result == expected && removed && !receipt.removal.entered && removeCalls == 0 && finalCalls == 0 && referenceCalls == (mode == 1 ? 1u : 0u) &&
                      FinalMapBytes(fixture) == before,
                  "unclaimed, Held, missing-key, unloaded and foreign-generation domains refuse without false output, decrement or removal fallback");
        }
    }
    void UnusedGuardCompetitionChecks(RemoveFixture& fixture, std::uintptr_t image)
    {
        ResetCycleFixture(fixture);
        CycleController controller;
        CycleToken      token;
        ReserveClaimedCycle(controller, token);
        // Asset admission is reached only inside the existing removal path,
        // after our initial zero snapshot. Competing reference activity there
        // must be observed by the final guard before native entry.
        unusedCompetitionFixture = &fixture;
        PoolTestHost::watched = fixture.Address(fixture.assetRows);
        PoolTestHost::after = &RaiseUnusedCountDuringRemovalAdmission;
        bool                              removed = true;
        NativeModelUnusedRemoval::Receipt receipt;
        Check(controller.UnloadUnused(image, cycleGeneration, token, &PoolTestHost::Owner, removed, &receipt) == ClockResult::BindingChanged && removed &&
                  receipt.countObserved && receipt.count == 0 && !receipt.removal.entered && fixture.Count(1) == 1 && !PoolTestHost::after &&
                  referenceCalls == 0 && finalCalls == 0 && removeCalls == 0 && !controller.RecoveryPending(),
              "new nonzero native reference during removal admission prevents entry without a speculative decrement or partial retry");
    }
    void UnusedUnknownReceiptChecks(RemoveFixture& fixture, std::uintptr_t image, bool returned)
    {
        ResetCycleFixture(fixture);
        CycleController controller;
        CycleToken      token;
        ReserveClaimedCycle(controller, token);
        removeMode = returned ? 3u : 2u;
        bool       removed = true;
        const auto expected = returned ? ClockResult::Expired : ClockResult::OriginalException;
        Check(controller.UnloadUnused(image, cycleGeneration, token, &PoolTestHost::Owner, removed) == expected && removed,
              "entered partial removal or lost returned acknowledgement preserves the caller output and propagates failure");
        PoolTestHost::ownerResult = ClockResult::Ok;
        const auto state = ReadCycle(controller, token);
        Check(state.phase == CyclePhase::FaultDebt && state.unusedRemoval.removal.entered && state.unusedRemoval.removal.returned == returned &&
                  !state.unusedRemovalCompleted && !state.unusedRemoval.verified && !state.acquisition.entered && !state.acquisition.accepted &&
                  controller.IsRetained() && controller.HasPending() && controller.RecoveryPending(),
              "independent unused-removal uncertainty is retained even when this family never acquired a native reference");
        CycleToken successor{91, 92, 93};
        const auto sentinel = successor;
        Check(controller.UnloadUnused(image, cycleGeneration, token, &PoolTestHost::Owner, removed) == ClockResult::OwnerConflict &&
                  controller.RenewCycle(cycleGeneration, token, &PoolTestHost::Owner, successor) == ClockResult::OwnerConflict &&
                  SameCycleToken(successor, sentinel) &&
                  controller.Acquire(image, cycleGeneration, token, &PoolTestHost::Owner) == ClockResult::OwnerConflict &&
                  controller.RevokeUnacquired(cycleGeneration) == ClockResult::Ok && controller.IsRetained() && controller.HasPending() &&
                  referenceCalls == 0 && finalCalls == 0 && removeCalls == 1 && fixture.Count(1) == 0,
              "owner readmission or End-style revoke cannot replay removal, renew/acquire or erase entered debt with no acquisition receipt");
    }
}
int main(int argc, char** argv)
{
    Check(argc == 2 && std::filesystem::path(argv[1]).is_absolute(), "unused-removal evidence stays in its workspace-owned directory");
    RemoveFixture fixture(argv[1]);
    finalFixture = activeRemove = &fixture;
    activeAcquire = &fixture;
    activeReference = &fixture;
    const auto image = fixture.transform.image.image;
    PoolTestHost::image = image;
    UnusedZeroOrPositiveChecks(fixture, image, true);
    UnusedZeroOrPositiveChecks(fixture, image, false);
    UnusedReleasedFamilyChecks(fixture, image);
    UnusedDomainRefusalChecks(fixture, image);
    UnusedGuardCompetitionChecks(fixture, image);
    UnusedUnknownReceiptChecks(fixture, image, false);
    UnusedUnknownReceiptChecks(fixture, image, true);
    std::puts("Production native model unused-removal controller fixture completed");
    std::printf("Passed %u native model unused-removal controller checks (64-bit; no native engine code executed)\n", checks);
    return 0;
}
