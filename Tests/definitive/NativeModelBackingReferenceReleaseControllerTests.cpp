/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Explicit owner-issued release through actual native reference paths
 *****************************************************************************/
#pragma push_macro("main")
#undef main
#define main NativeBackingReleaseHistoricalFinalMain
#include "NativeModelReferenceFinalReleaseTests.cpp"
#undef main
#pragma pop_macro("main")
#include "../../Client/game_de/NativeModelBackingReferenceController.h"

namespace
{
    using ReleaseController = mta::game_de::NativeModelBackingReferenceController;
    using ReleaseToken = mta::game_de::NativeModelBackingReferenceToken;
    using ReleaseState = mta::game_de::NativeModelBackingReferenceState;
    using ReleasePhase = mta::game_de::NativeModelBackingReferencePhase;
    using ReleasePolicy = mta::game_de::NativeModelReferenceRelease::ZeroPolicy;
    constexpr std::uint64_t releaseGeneration = 17;
    constexpr std::uint32_t releaseModel = 17;
    unsigned                acknowledgedReleaseOwnerCalls = 0;
    RemoveFixture*          changingReleaseFixture = nullptr;

    ReleaseState ReadReleaseState(ReleaseController& controller, const ReleaseToken& token)
    {
        ReleaseState state;
        Check(controller.Read(releaseGeneration, token, &PoolTestHost::Owner, state) == ClockResult::Ok &&
                  state.reference.ownerGeneration == token.ownerGeneration && state.reference.serial == token.serial &&
                  state.reference.modelIndex == token.modelIndex,
              "actual controller exposes the durable state for the exact issued capability");
        return state;
    }

    void PrepareHeldRelease(RemoveFixture& fixture, ReleaseController& controller, ReleaseToken& token, std::uintptr_t image, std::uint32_t beforeAcquire)
    {
        fixture.Reset(releaseModel);
        // Asset and reference maps must keep independent inline bits/hash
        // storage. Reuse the existing fixture's real shaped native domains.
        fixture.ReferenceFixture::Shape(4, 1, false, 1, releaseModel);
        fixture.Row(1, 4, beforeAcquire);
        finalIndex = 1;
        reserveCalls = publishCalls = acquireMode = referenceCalls = referenceMode = finalCalls = finalMode = removeCalls = removeMode = 0;
        acquireKey = releaseModel;
        passedRemove = nullptr;
        publishAbiValid = false;
        Check(controller.Reserve(releaseGeneration, releaseModel, &PoolTestHost::Owner, token) == ClockResult::Ok &&
                  controller.Bind(releaseGeneration, token, &PoolTestHost::Owner) == ClockResult::Ok,
              "release begins with one actually reserved and bound owner identity");
        Check(controller.Acquire(image, releaseGeneration, token, &PoolTestHost::Owner) == ClockResult::Ok && fixture.Count(1) == beforeAcquire + 1 &&
                  referenceCalls == 1 && reserveCalls == 0 && publishCalls == 0 && ReadReleaseState(controller, token).phase == ReleasePhase::Held,
              "actual acquisition creates the Held backing obligation before any release test");
    }

    void CheckReleased(ReleaseController& controller, const ReleaseToken& token, std::uintptr_t image)
    {
        Check(ReadReleaseState(controller, token).phase == ReleasePhase::Released && !controller.IsRetained() && !controller.RecoveryPending() &&
                  controller.HasPending(),
              "acknowledged Released has no native debt while its completed CPU record remains until End");
        const auto adjustments = referenceCalls, finals = finalCalls, removals = removeCalls;
        Check(controller.Acquire(image, releaseGeneration, token, &PoolTestHost::Owner) == ClockResult::OwnerConflict &&
                  controller.Release(image, releaseGeneration, token, ReleasePolicy::KeepResident, &PoolTestHost::Owner) == ClockResult::OwnerConflict &&
                  referenceCalls == adjustments && finalCalls == finals && removeCalls == removals,
              "released capability cannot reacquire or release another reference");
        Check(controller.RevokeUnacquired(releaseGeneration) == ClockResult::Ok && !controller.HasPending() && !controller.IsRetained(),
              "explicit End revocation erases only the completed CPU record");
        ReleaseState unchanged;
        unchanged.reference.serial = 919;
        Check(controller.Read(releaseGeneration, token, &PoolTestHost::Owner, unchanged) == ClockResult::BindingChanged && unchanged.reference.serial == 919,
              "completed-token revocation preserves output and cannot restore native authority");
    }

    void InteriorReleaseChecks(RemoveFixture& fixture, std::uintptr_t image)
    {
        ReleaseController controller;
        ReleaseToken      token;
        PrepareHeldRelease(fixture, controller, token, image, 1);
        FR::Receipt receipt;
        Check(controller.Release(image, releaseGeneration, token, ReleasePolicy::RemoveAtZero, &PoolTestHost::Owner, &receipt) == ClockResult::Ok,
              "fresh count greater than one selects the actual interior decrement");
        const auto state = ReadReleaseState(controller, token);
        Check(state.releaseSelectedCount == 2 && !state.releaseFinal && state.releasePolicy == ReleasePolicy::RemoveAtZero && state.release.reference.entered &&
                  state.release.reference.accepted && state.release.reference.before == 2 && state.release.reference.target == 1 &&
                  state.release.reference.observed == 2 && fixture.Count(1) == 1 && referenceCalls == 2 && finalCalls == 0 && removeCalls == 0 &&
                  !state.release.removalPrepared && !state.release.removal.entered && receipt.reference.before == state.release.reference.before &&
                  receipt.reference.target == state.release.reference.target && receipt.reference.accepted == state.release.reference.accepted,
              "interior release records exactly one real Adjust(-1) and never falls through to final removal");
        CheckReleased(controller, token, image);
    }

    void FinalReleaseChecks(RemoveFixture& fixture, std::uintptr_t image, ReleasePolicy policy)
    {
        ReleaseController controller;
        ReleaseToken      token;
        PrepareHeldRelease(fixture, controller, token, image, 0);
        const auto  stream = fixture.Stream(releaseModel);
        const auto  ownersBefore = PoolTestHost::ownerCalls;
        FR::Receipt receipt;
        Check(controller.Release(image, releaseGeneration, token, policy, &PoolTestHost::Owner, &receipt) == ClockResult::Ok,
              "fresh count one selects the actual final primitive with explicit caller zero policy");
        if (policy == ReleasePolicy::KeepResident)
            acknowledgedReleaseOwnerCalls = PoolTestHost::ownerCalls - ownersBefore;
        const auto state = ReadReleaseState(controller, token);
        Check(state.releaseSelectedCount == 1 && state.releaseFinal && state.releasePolicy == policy && state.release.reference.entered &&
                  state.release.reference.accepted && state.release.reference.before == 1 && state.release.reference.target == 0 && fixture.Count(1) == 0 &&
                  referenceCalls == 1 && finalCalls == 1 && receipt.reference.accepted,
              "final release records the accepted real one-to-zero effect without another interior adjustment");
        if (policy == ReleasePolicy::KeepResident)
            Check(!state.release.removalPrepared && !state.release.removal.entered && removeCalls == 0 && fixture.Stream(releaseModel) == stream,
                  "explicit KeepResident acknowledges zero while preserving actual loaded streaming state");
        else
            Check(state.release.removalPrepared && state.release.removal.entered && state.release.removal.returned && removeCalls == 1 &&
                      fixture.Stream(releaseModel)[1] == 0 && passedRemove != nullptr && receipt.removal.entered && receipt.removal.returned,
                  "explicit RemoveAtZero records the genuine owned native-removal entry and completed acknowledgement");
        CheckReleased(controller, token, image);
    }

    void ZeroCountDebtChecks(RemoveFixture& fixture, std::uintptr_t image)
    {
        ReleaseController controller;
        ReleaseToken      token;
        PrepareHeldRelease(fixture, controller, token, image, 0);
        fixture.Row(1, 4, std::uint32_t{0});
        Check(controller.Release(image, releaseGeneration, token, ReleasePolicy::KeepResident, &PoolTestHost::Owner) == ClockResult::BindingChanged,
              "externally lost zero count is not acknowledgement of this capability's release");
        const auto state = ReadReleaseState(controller, token);
        Check(state.phase == ReleasePhase::FaultDebt && !state.release.reference.entered && !state.release.reference.accepted && controller.IsRetained() &&
                  controller.RecoveryPending() && referenceCalls == 1 && finalCalls == 0 && removeCalls == 0,
              "lost backing invariant becomes terminal debt without native entry or synthetic zero acknowledgement");
        fixture.Row(1, 4, std::uint32_t{2});
        Check(controller.Release(image, releaseGeneration, token, ReleasePolicy::KeepResident, &PoolTestHost::Owner) == ClockResult::OwnerConflict &&
                  controller.Acquire(image, releaseGeneration, token, &PoolTestHost::Owner) == ClockResult::OwnerConflict && fixture.Count(1) == 2 &&
                  referenceCalls == 1 && finalCalls == 0 && removeCalls == 0,
              "later unrelated positive count cannot authorize retry after the backing invariant disappeared");
        Check(controller.RevokeUnacquired(releaseGeneration) == ClockResult::Ok && controller.HasPending() && controller.IsRetained(),
              "CPU-only revocation cannot erase terminal zero-count debt");
    }

    void ChangeSelectedReleaseCount()
    {
        changingReleaseFixture->Row(1, 4, std::uint32_t{1});
    }

    void ReadDomainRefusalChecks(RemoveFixture& fixture, std::uintptr_t image)
    {
        {
            ReleaseController controller;
            ReleaseToken      token;
            PrepareHeldRelease(fixture, controller, token, image, 1);
            changingReleaseFixture = &fixture;
            PoolTestHost::watched = fixture.Address(fixture.references);
            PoolTestHost::after = &ChangeSelectedReleaseCount;
            Check(controller.Release(image, releaseGeneration, token, ReleasePolicy::RemoveAtZero, &PoolTestHost::Owner) == ClockResult::BindingChanged,
                  "fresh count selection refuses a row that changes between its actual map snapshots");
            // Nested context/map BindingChanged cannot distinguish a torn
            // count from an origin replacement. Quarantine the capability
            // rather than allowing a retry against potentially foreign state.
            Check(ReadReleaseState(controller, token).phase == ReleasePhase::FaultDebt && controller.RecoveryPending() && fixture.Count(1) == 1 &&
                      referenceCalls == 1 && finalCalls == 0 && removeCalls == 0,
                  "binding observation loss becomes terminal debt without falling back to the newly observed final-count route");
        }
        {
            ReleaseController controller;
            ReleaseToken      token;
            PrepareHeldRelease(fixture, controller, token, image, 1);
            fixture.Stream(releaseModel, 0x40, 0);
            Check(controller.Release(image, releaseGeneration, token, ReleasePolicy::KeepResident, &PoolTestHost::Owner) == ClockResult::NotInitialized &&
                      ReadReleaseState(controller, token).phase == ReleasePhase::Held && referenceCalls == 1 && finalCalls == 0 && removeCalls == 0,
                  "non-binding loaded-domain refusal stays Held before choosing any mutation or fallback");
        }
    }

    void FinalPreentryRefusalChecks(RemoveFixture& fixture, std::uintptr_t image)
    {
        ReleaseController controller;
        ReleaseToken      token;
        PrepareHeldRelease(fixture, controller, token, image, 0);
        fixture.Global(AP::HeapRva, std::uintptr_t{0});
        Check(controller.Release(image, releaseGeneration, token, ReleasePolicy::RemoveAtZero, &PoolTestHost::Owner) != ClockResult::Ok,
              "selected final removal still requires its actual complete native admission");
        const auto state = ReadReleaseState(controller, token);
        Check(state.phase == ReleasePhase::Held && state.releaseFinal && state.releaseSelectedCount == 1 && !state.release.reference.entered &&
                  !state.release.removal.entered && !controller.RecoveryPending() && controller.IsRetained() && fixture.Count(1) == 1 && referenceCalls == 1 &&
                  finalCalls == 0 && removeCalls == 0,
              "unentered final refusal keeps Held and cannot fall back to decrement or a different zero policy");
    }

    void EnteredReleaseDebtChecks(RemoveFixture& fixture, std::uintptr_t image, bool acceptedFinalLoss)
    {
        ReleaseController controller;
        ReleaseToken      token;
        PrepareHeldRelease(fixture, controller, token, image, acceptedFinalLoss ? 0u : 1u);
        if (acceptedFinalLoss)
            finalMode = 3;
        else
            referenceMode = 1;
        const auto expected = acceptedFinalLoss ? ClockResult::Expired : ClockResult::OriginalException;
        Check(controller.Release(image, releaseGeneration, token, ReleasePolicy::KeepResident, &PoolTestHost::Owner) == expected,
              "actual accepted post-loss or unknown exchange refusal propagates once through controller release");
        PoolTestHost::ownerResult = ClockResult::Ok;
        const auto state = ReadReleaseState(controller, token);
        const auto count = acceptedFinalLoss ? 0u : 2u;
        Check(state.phase == ReleasePhase::FaultDebt && state.lastResult == expected && state.release.reference.entered &&
                  state.release.reference.accepted == acceptedFinalLoss && state.releaseFinal == acceptedFinalLoss && fixture.Count(1) == count &&
                  referenceCalls == (acceptedFinalLoss ? 1u : 2u) && finalCalls == (acceptedFinalLoss ? 1u : 0u) && removeCalls == 0 &&
                  controller.IsRetained() && controller.RecoveryPending(),
              "accepted final zero or unknown interior exchange retains exact native effects as durable FaultDebt");
        Check(acceptedFinalLoss || state.release.reference.exceptionCode == 0xe0580001,
              "unknown interior exchange retains its genuine callback exception code");
        const auto adjustments = referenceCalls, finals = finalCalls;
        Check(controller.Release(image, releaseGeneration, token, ReleasePolicy::RemoveAtZero, &PoolTestHost::Owner) == ClockResult::OwnerConflict &&
                  controller.Acquire(image, releaseGeneration, token, &PoolTestHost::Owner) == ClockResult::OwnerConflict && fixture.Count(1) == count &&
                  referenceCalls == adjustments && finalCalls == finals && removeCalls == 0,
              "entered debt refuses release replay, fallback and reacquisition even after observation owner readmission");
        Check(controller.RevokeUnacquired(releaseGeneration) == ClockResult::Ok && controller.HasPending() && controller.IsRetained() &&
                  ReadReleaseState(controller, token).phase == ReleasePhase::FaultDebt,
              "explicit CPU revocation cannot discard entered release uncertainty");
    }

    void RemovalDebtChecks(RemoveFixture& fixture, std::uintptr_t image)
    {
        ReleaseController controller;
        ReleaseToken      token;
        PrepareHeldRelease(fixture, controller, token, image, 0);
        removeMode = 1;
        Check(controller.Release(image, releaseGeneration, token, ReleasePolicy::RemoveAtZero, &PoolTestHost::Owner) == ClockResult::OriginalException,
              "entered genuine removal failure propagates after the accepted zero");
        const auto state = ReadReleaseState(controller, token);
        Check(state.phase == ReleasePhase::FaultDebt && state.release.reference.accepted && state.release.removal.entered && !state.release.removal.returned &&
                  state.release.removal.exceptionCode == 0xe05a0001 && fixture.Count(1) == 0 && finalCalls == 1 && removeCalls == 1 &&
                  controller.IsRetained() && controller.RecoveryPending(),
              "unknown removal retains the accepted zero and separate native removal exception receipt");
        Check(controller.Release(image, releaseGeneration, token, ReleasePolicy::RemoveAtZero, &PoolTestHost::Owner) == ClockResult::OwnerConflict &&
                  finalCalls == 1 && removeCalls == 1 && fixture.Count(1) == 0,
              "failed removal acknowledgement cannot replay the decrement or native removal entry");
    }

    void CompletedReleaseOwnerLossChecks(RemoveFixture& fixture, std::uintptr_t image)
    {
        ReleaseController controller;
        ReleaseToken      token;
        PrepareHeldRelease(fixture, controller, token, image, 0);
        Check(acknowledgedReleaseOwnerCalls != 0, "focused successful final release measured its actual owner bookends");
        // Target only the controller's extra callback after the already
        // acknowledged primitive. This is one measured boundary, not an old
        // owner-fault matrix or a wall-clock callback lease.
        PoolTestHost::revokeAt = PoolTestHost::ownerCalls + acknowledgedReleaseOwnerCalls;
        Check(controller.Release(image, releaseGeneration, token, ReleasePolicy::KeepResident, &PoolTestHost::Owner) == ClockResult::WrongThread,
              "extra owner acknowledgement can fail after the primitive has completed release");
        const auto state = ReadReleaseState(controller, token);
        Check(state.phase == ReleasePhase::Released && state.lastResult == ClockResult::WrongThread && state.release.reference.accepted &&
                  fixture.Count(1) == 0 && !controller.IsRetained() && !controller.RecoveryPending() && finalCalls == 1 && removeCalls == 0,
              "known whole-primitive release stays Released on later owner loss and cannot resurrect native debt");
        CheckReleased(controller, token, image);
    }

    void ReleaseDestructionChecks(RemoveFixture& fixture, std::uintptr_t image)
    {
        {
            ReleaseController controller;
            ReleaseToken      token;
            PrepareHeldRelease(fixture, controller, token, image, 0);
        }
        Check(fixture.Count(1) == 1 && referenceCalls == 1 && finalCalls == 0 && removeCalls == 0,
              "controller destruction performs CPU cleanup only and never supplies an automatic native release");
        NativeModelBackingReferenceAccess missing;
        Check(missing.ReleaseModelBackingReference(ReleaseToken{17, 71, 17}, ReleasePolicy::KeepResident) == ClockResult::NotInitialized,
              "older optional capability owner refuses explicit release rather than accepting a no-op");
    }

    void RequiredOriginGuardChecks(RemoveFixture& fixture, std::uintptr_t image)
    {
        {
            ReleaseController controller;
            ReleaseToken      token;
            PrepareHeldRelease(fixture, controller, token, image, 1);
            const auto    origin = ReadReleaseState(controller, token).acquisition.origin;
            std::uint32_t count = 0xfeedface;
            Check(origin.root == fixture.Address(fixture.application) && origin.map == fixture.Address(fixture.Map()) && origin.rootIndex == 1 &&
                      origin.rootSerial == 11 &&
                      NativeModelReferenceMemory::ReadCount(image, releaseModel, &PoolTestHost::Owner, count, &origin) == ClockResult::Ok && count == 2,
                  "correct required origin comes from the actual acquired root membership and admits the fresh count");
            for (unsigned field = 0; field != 4; ++field)
            {
                auto wrong = origin;
                if (field == 0)
                    wrong.root += 8;
                else if (field == 1)
                    wrong.map += 8;
                else if (field == 2)
                    ++wrong.rootIndex;
                else
                    ++wrong.rootSerial;
                count = 0xfeedface;
                NativeModelReferenceMemory::Receipt receipt;
                Check(NativeModelReferenceMemory::ReadCount(image, releaseModel, &PoolTestHost::Owner, count, &wrong) == ClockResult::BindingChanged &&
                          count == 0xfeedface,
                      "wrong root, map, registry index or serial refuses without replacing the previous count");
                Check(NativeModelReferenceMemory::Adjust(image, releaseModel, -1, &PoolTestHost::Owner, &receipt, &ExchangeReference, &wrong) ==
                              ClockResult::BindingChanged &&
                          !receipt.entered && !receipt.accepted && fixture.Count(1) == 2 && referenceCalls == 1 && finalCalls == 0 && removeCalls == 0,
                      "origin mismatch refuses the actual interior primitive before exchange or fallback");
            }
        }
        {
            ReleaseController controller;
            ReleaseToken      token;
            PrepareHeldRelease(fixture, controller, token, image, 0);
            const auto origin = ReadReleaseState(controller, token).acquisition.origin;
            for (unsigned field = 0; field != 4; ++field)
            {
                auto wrong = origin;
                if (field == 0)
                    wrong.root += 8;
                else if (field == 1)
                    wrong.map += 8;
                else if (field == 2)
                    ++wrong.rootIndex;
                else
                    ++wrong.rootSerial;
                FR::Receipt receipt;
                Check(FR::ReleaseFinal(image, releaseModel, ReleasePolicy::KeepResident, &PoolTestHost::Owner, &receipt, &ExchangeFinal, nullptr, &wrong) ==
                              ClockResult::BindingChanged &&
                          !receipt.reference.entered && !receipt.reference.accepted && fixture.Count(1) == 1 && referenceCalls == 1 && finalCalls == 0 &&
                          removeCalls == 0,
                      "origin mismatch refuses the actual final primitive before decrement or native removal");
            }
        }
    }

    void PersistentOriginRefusalChecks(RemoveFixture& fixture, std::uintptr_t image)
    {
        fixture.Reset(releaseModel);
        fixture.ReferenceFixture::Shape(4, 1, false, 1, releaseModel);
        fixture.Row(1, 4, std::uint32_t{0});
        reserveCalls = publishCalls = acquireMode = referenceCalls = referenceMode = finalCalls = finalMode = removeCalls = removeMode = 0;
        acquireKey = releaseModel;
        // The existing raw root is registry index1 with serial11. Clear only
        // its real item serial; durable acquisition must not allocate a weak
        // serial or enter a native count effect to repair that absence.
        fixture.Put(fixture.items, 24 + 0x10, std::uint32_t{0});
        ReleaseController controller;
        ReleaseToken      token;
        Check(controller.Reserve(releaseGeneration, releaseModel, &PoolTestHost::Owner, token) == ClockResult::Ok &&
                  controller.Bind(releaseGeneration, token, &PoolTestHost::Owner) == ClockResult::Ok &&
                  controller.Acquire(image, releaseGeneration, token, &PoolTestHost::Owner) == ClockResult::BindingChanged,
              "durable acquisition refuses an unissued zero root serial before native entry");
        const auto state = ReadReleaseState(controller, token);
        Check(state.phase == ReleasePhase::Bound && !state.acquisition.entered && !state.acquisition.reserved && !state.acquisition.published &&
                  !state.acquisition.accepted && !controller.IsRetained() && fixture.Count(1) == 0 && referenceCalls == 0 && reserveCalls == 0 &&
                  publishCalls == 0 && finalCalls == 0 && removeCalls == 0 && fixture.Get<std::uint32_t>(fixture.items, 24 + 0x10) == 0,
              "zero-serial refusal preserves the raw membership and has no native acquisition or invented weak identity");
        Check(controller.RevokeUnacquired(releaseGeneration) == ClockResult::Ok && !controller.HasPending(),
              "unentered zero-serial acquisition claim may be explicitly revoked");
    }

    void ChangedOriginReleaseChecks(RemoveFixture& fixture, std::uintptr_t image)
    {
        ReleaseController controller;
        ReleaseToken      token;
        PrepareHeldRelease(fixture, controller, token, image, 1);
        const auto origin = ReadReleaseState(controller, token).acquisition.origin;
        // Reuse the same existing object/item addresses with a different real
        // registry serial. Raw membership remains valid; the acquired origin
        // cannot authorize a deferred release against the replacement identity.
        fixture.Put(fixture.items, 24 + 0x10, std::uint32_t{12});
        std::uint32_t count = 0xfeedface;
        Check(NativeModelReferenceMemory::ReadCount(image, releaseModel, &PoolTestHost::Owner, count, &origin) == ClockResult::BindingChanged &&
                  count == 0xfeedface &&
                  controller.Release(image, releaseGeneration, token, ReleasePolicy::KeepResident, &PoolTestHost::Owner) == ClockResult::BindingChanged,
              "same-address changed root serial refuses both guarded count observation and actual capability release");
        const auto state = ReadReleaseState(controller, token);
        Check(state.phase == ReleasePhase::FaultDebt && controller.IsRetained() && controller.RecoveryPending() && !state.release.reference.entered &&
                  fixture.Count(1) == 2 && referenceCalls == 1 && finalCalls == 0 && removeCalls == 0,
              "changed origin becomes terminal retained debt before any release effect");
        fixture.Put(fixture.items, 24 + 0x10, std::uint32_t{11});
        Check(controller.Release(image, releaseGeneration, token, ReleasePolicy::KeepResident, &PoolTestHost::Owner) == ClockResult::OwnerConflict &&
                  controller.Acquire(image, releaseGeneration, token, &PoolTestHost::Owner) == ClockResult::OwnerConflict && fixture.Count(1) == 2 &&
                  referenceCalls == 1 && finalCalls == 0 && removeCalls == 0,
              "later restored bytes cannot revive the quarantined origin or replay native intent");
    }
}

int main(int argc, char** argv)
{
    Check(argc == 2 && std::filesystem::path(argv[1]).is_absolute(), "release controller evidence uses a workspace-owned directory");
    // Reuse only the owned native image/data fixture and final-call stubs.
    // Every historical acquisition/removal/final-release main remains unused.
    RemoveFixture fixture(argv[1]);
    finalFixture = activeRemove = &fixture;
    activeAcquire = &fixture;
    activeReference = &fixture;
    const auto image = fixture.transform.image.image;
    PoolTestHost::image = image;
    InteriorReleaseChecks(fixture, image);
    FinalReleaseChecks(fixture, image, ReleasePolicy::KeepResident);
    FinalReleaseChecks(fixture, image, ReleasePolicy::RemoveAtZero);
    ZeroCountDebtChecks(fixture, image);
    ReadDomainRefusalChecks(fixture, image);
    FinalPreentryRefusalChecks(fixture, image);
    EnteredReleaseDebtChecks(fixture, image, true);
    EnteredReleaseDebtChecks(fixture, image, false);
    RemovalDebtChecks(fixture, image);
    CompletedReleaseOwnerLossChecks(fixture, image);
    ReleaseDestructionChecks(fixture, image);
    RequiredOriginGuardChecks(fixture, image);
    PersistentOriginRefusalChecks(fixture, image);
    ChangedOriginReleaseChecks(fixture, image);
    std::puts("Production native model backing reference weak-origin fixture completed");
    std::puts("Production native model backing reference release controller fixture completed");
    std::printf("Passed %u native model backing reference release controller checks (64-bit; no native engine code executed)\n", checks);
    return 0;
}
