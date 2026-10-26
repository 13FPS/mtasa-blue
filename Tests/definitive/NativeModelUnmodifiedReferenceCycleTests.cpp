/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual owner family uniqueness and acknowledged native ref cycles
 *****************************************************************************/
#pragma push_macro("main")
#undef main
#define main NativeUnmodifiedCycleHistoricalFinalMain
#include "NativeModelReferenceFinalReleaseTests.cpp"
#undef main
#pragma pop_macro("main")
#include "../../Client/game_de/NativeModelBackingReferenceController.h"

namespace
{
    using CycleController = NativeModelBackingReferenceController;
    using CycleToken = NativeModelBackingReferenceToken;
    using CycleState = NativeModelBackingReferenceState;
    using CyclePhase = NativeModelBackingReferencePhase;
    constexpr std::uint64_t cycleGeneration = 17;
    constexpr std::uint32_t cycleModel = 17;

    bool SameCycleToken(const CycleToken& a, const CycleToken& b)
    {
        return a.ownerGeneration == b.ownerGeneration && a.serial == b.serial && a.modelIndex == b.modelIndex;
    }
    CycleState ReadCycle(CycleController& controller, const CycleToken& token)
    {
        CycleState state;
        Check(controller.Read(cycleGeneration, token, &PoolTestHost::Owner, state) == ClockResult::Ok && SameCycleToken(state.reference, token),
              "actual controller exposes the exact issued family-cycle record");
        return state;
    }
    void ResetCycleFixture(RemoveFixture& fixture)
    {
        fixture.Reset(cycleModel);
        fixture.ReferenceFixture::Shape(4, 1, false, 1, cycleModel);
        fixture.Row(1, 4, std::uint32_t{0});
        reserveCalls = publishCalls = acquireMode = referenceCalls = referenceMode = finalCalls = finalMode = removeCalls = removeMode = 0;
        finalIndex = 1;
        acquireKey = cycleModel;
        passedRemove = nullptr;
    }
    void ReserveClaimedCycle(CycleController& controller, CycleToken& token)
    {
        Check(controller.Reserve(cycleGeneration, cycleModel, &PoolTestHost::Owner, token) == ClockResult::Ok &&
                  controller.Bind(cycleGeneration, token, &PoolTestHost::Owner) == ClockResult::Ok &&
                  controller.ClaimUnmodifiedFamily(cycleGeneration, token, &PoolTestHost::Owner) == ClockResult::Ok,
              "one actual reserved and bound root cycle claims its source-owned unmodified family");
    }
    void ActualCycleChecks(RemoveFixture& fixture, std::uintptr_t image)
    {
        ResetCycleFixture(fixture);
        CycleController controller;
        CycleToken      first;
        ReserveClaimedCycle(controller, first);
        const auto claimed = ReadCycle(controller, first);
        Check(claimed.familySerial == first.serial && claimed.unmodifiedFamily && claimed.phase == CyclePhase::Bound &&
                  controller.ClaimUnmodifiedFamily(cycleGeneration, first, &PoolTestHost::Owner) == ClockResult::OwnerConflict,
              "family claim labels only the initial bound root record and cannot be claimed twice");
        CycleToken duplicate{91, 92, 93};
        const auto sentinel = duplicate;
        Check(controller.Reserve(cycleGeneration, cycleModel, &PoolTestHost::Owner, duplicate) == ClockResult::OwnerConflict &&
                  SameCycleToken(duplicate, sentinel),
              "another facade cannot reserve a parallel live family for the same model and generation");
        Check(controller.Acquire(image, cycleGeneration, first, &PoolTestHost::Owner) == ClockResult::Ok && fixture.Count(1) == 1 && referenceCalls == 1 &&
                  reserveCalls == 0 && publishCalls == 0,
              "first family cycle acquires exactly one actual existing native reference from zero");
        Check(
            controller.RenewCycle(cycleGeneration, first, &PoolTestHost::Owner, duplicate) == ClockResult::OwnerConflict && SameCycleToken(duplicate, sentinel),
            "Held backing obligation cannot be renewed as if released");
        // KeepResident keeps this same owned native domain loaded. The SDK
        // suite separately proves its closed Remove hook selects RemoveAtZero;
        // no invented resource reload or application lifecycle is needed here.
        Check(controller.Release(image, cycleGeneration, first, FR::ZeroPolicy::KeepResident, &PoolTestHost::Owner) == ClockResult::Ok &&
                  fixture.Count(1) == 0 && finalCalls == 1 && removeCalls == 0 && ReadCycle(controller, first).phase == CyclePhase::Released,
              "first actual family cycle acknowledges one-to-zero release without another count algorithm");
        Check(controller.Reserve(cycleGeneration, cycleModel, &PoolTestHost::Owner, duplicate) == ClockResult::OwnerConflict &&
                  SameCycleToken(duplicate, sentinel),
              "completed root family cannot fork through a second original reserve");
        Check(controller.Acquire(image, cycleGeneration, first, &PoolTestHost::Owner) == ClockResult::OwnerConflict && fixture.Count(1) == 0 &&
                  referenceCalls == 1 && finalCalls == 1,
              "consumed cycle token cannot reacquire the released backing reference");

        CycleToken second = sentinel;
        PoolTestHost::revokeAt = PoolTestHost::ownerCalls + 2;
        Check(controller.RenewCycle(cycleGeneration, first, &PoolTestHost::Owner, second) == ClockResult::WrongThread && SameCycleToken(second, sentinel),
              "lost CPU renewal acknowledgement preserves output after durable successor issuance");
        const auto predecessor = ReadCycle(controller, first);
        Check(predecessor.successorSerial != 0 && predecessor.successorSerial != first.serial && fixture.Count(1) == 0 && referenceCalls == 1 &&
                  finalCalls == 1 && removeCalls == 0,
              "renewal records one fresh serial without entering native acquire or release");
        Check(controller.RenewCycle(cycleGeneration, first, &PoolTestHost::Owner, second) == ClockResult::Ok && second.serial == predecessor.successorSerial &&
                  second.serial != first.serial && second.modelIndex == first.modelIndex && second.ownerGeneration == first.ownerGeneration,
              "retry after lost CPU acknowledgement returns the same prepared successor rather than forking");
        CycleToken repeated;
        Check(controller.RenewCycle(cycleGeneration, first, &PoolTestHost::Owner, repeated) == ClockResult::Ok && SameCycleToken(repeated, second),
              "duplicate renewal before acquire remains idempotent for the issued successor");
        const auto next = ReadCycle(controller, second);
        Check(next.phase == CyclePhase::Bound && next.familySerial == first.serial && next.unmodifiedFamily && next.successorSerial == 0,
              "fresh bound cycle preserves the one unmodified family identity");
        Check(controller.Acquire(image, cycleGeneration, second, &PoolTestHost::Owner) == ClockResult::Ok && fixture.Count(1) == 1 && referenceCalls == 2 &&
                  ReadCycle(controller, second).phase == CyclePhase::Held,
              "fresh successor acquires exactly one new actual native reference after prior zero acknowledgement");
        Check(controller.RenewCycle(cycleGeneration, first, &PoolTestHost::Owner, repeated) == ClockResult::OwnerConflict &&
                  controller.ClaimUnmodifiedFamily(cycleGeneration, second, &PoolTestHost::Owner) == ClockResult::OwnerConflict && fixture.Count(1) == 1 &&
                  referenceCalls == 2,
              "old cycle cannot replay renewal after its successor enters native ownership or claim another family");
        Check(controller.Release(image, cycleGeneration, second, FR::ZeroPolicy::KeepResident, &PoolTestHost::Owner) == ClockResult::Ok &&
                  fixture.Count(1) == 0 && finalCalls == 2 && removeCalls == 0 && !controller.IsRetained() && !controller.RecoveryPending(),
              "second real native cycle releases one-to-zero exactly once with no retained native debt");
        Check(controller.RevokeUnacquired(cycleGeneration) == ClockResult::Ok && !controller.HasPending(),
              "End-style CPU cleanup can erase the acknowledged completed family records");
    }
    void ConstructorFamilyDomainChecks(RemoveFixture& fixture)
    {
        ResetCycleFixture(fixture);
        CycleController controller;
        CycleToken      token{91, 92, 93};
        const auto      sentinel = token;
        Check(controller.Reserve(0, cycleModel, &PoolTestHost::Owner, token) == ClockResult::InvalidArgument && SameCycleToken(token, sentinel) &&
                  controller.Reserve(cycleGeneration, B::ModelCount, &PoolTestHost::Owner, token) == ClockResult::InvalidArgument &&
                  SameCycleToken(token, sentinel),
              "invalid generation/model reserve preserves the caller token before any native effects");
        Check(controller.Reserve(cycleGeneration, cycleModel, &PoolTestHost::Owner, token) == ClockResult::Ok &&
                  controller.ClaimUnmodifiedFamily(cycleGeneration, token, &PoolTestHost::Owner) == ClockResult::OwnerConflict &&
                  !ReadCycle(controller, token).unmodifiedFamily,
              "a prepared record alone cannot claim initialized-empty replacement ownership");
        CycleToken output = sentinel;
        Check(controller.RenewCycle(cycleGeneration, token, &PoolTestHost::Owner, output) == ClockResult::OwnerConflict && SameCycleToken(output, sentinel) &&
                  controller.RenewCycle(cycleGeneration + 1, token, &PoolTestHost::Owner, output) == ClockResult::BindingChanged &&
                  SameCycleToken(output, sentinel),
              "unreleased and foreign-generation renewal preserve output without minting a family cycle");
        auto foreign = token;
        ++foreign.serial;
        Check(controller.ClaimUnmodifiedFamily(cycleGeneration, foreign, &PoolTestHost::Owner) == ClockResult::BindingChanged &&
                  controller.Bind(cycleGeneration, token, &PoolTestHost::Owner) == ClockResult::Ok,
              "unissued serial refuses before the genuine root constructor binding");
        PoolTestHost::ownerResult = ClockResult::Expired;
        Check(controller.ClaimUnmodifiedFamily(cycleGeneration, token, &PoolTestHost::Owner) == ClockResult::Expired,
              "owner refusal before a claim cannot label unmodified ownership");
        PoolTestHost::ownerResult = ClockResult::Ok;
        Check(!ReadCycle(controller, token).unmodifiedFamily, "unentered claim refusal leaves replacement ownership unclaimed");
        PoolTestHost::revokeAt = PoolTestHost::ownerCalls + 2;
        Check(controller.ClaimUnmodifiedFamily(cycleGeneration, token, &PoolTestHost::Owner) == ClockResult::WrongThread &&
                  ReadCycle(controller, token).unmodifiedFamily &&
                  controller.ClaimUnmodifiedFamily(cycleGeneration, token, &PoolTestHost::Owner) == ClockResult::OwnerConflict,
              "a lost final CPU claim acknowledgement preserves the accepted one-shot label without replay");
        const auto oldSerial = token.serial;
        Check(controller.RevokeUnacquired(cycleGeneration) == ClockResult::Ok && !controller.HasPending() &&
                  controller.Reserve(cycleGeneration, cycleModel, &PoolTestHost::Owner, token) == ClockResult::Ok && token.serial > oldSerial &&
                  ReadCycle(controller, token).familySerial == token.serial && !ReadCycle(controller, token).unmodifiedFamily,
              "explicit no-effect cleanup permits a new family while never reusing an old issued serial or ownership label");
        Check(referenceCalls == 0 && finalCalls == 0 && reserveCalls == 0 && publishCalls == 0 && removeCalls == 0 && fixture.Count(1) == 0,
              "family construction guards and acknowledgements perform CPU bookkeeping only");
    }
    void UnclaimedFamilyChecks(RemoveFixture& fixture, std::uintptr_t image)
    {
        ResetCycleFixture(fixture);
        CycleController controller;
        CycleToken      first, next;
        Check(controller.Reserve(cycleGeneration, cycleModel, &PoolTestHost::Owner, first) == ClockResult::Ok &&
                  controller.Bind(cycleGeneration, first, &PoolTestHost::Owner) == ClockResult::Ok &&
                  controller.Acquire(image, cycleGeneration, first, &PoolTestHost::Owner) == ClockResult::Ok &&
                  controller.ClaimUnmodifiedFamily(cycleGeneration, first, &PoolTestHost::Owner) == ClockResult::OwnerConflict &&
                  !ReadCycle(controller, first).unmodifiedFamily,
              "an already-held unclaimed capability cannot retroactively claim custom-resource absence");
        Check(controller.Release(image, cycleGeneration, first, FR::ZeroPolicy::KeepResident, &PoolTestHost::Owner) == ClockResult::Ok &&
                  controller.RenewCycle(cycleGeneration, first, &PoolTestHost::Owner, next) == ClockResult::Ok &&
                  !ReadCycle(controller, next).unmodifiedFamily && ReadCycle(controller, next).familySerial == first.serial &&
                  controller.ClaimUnmodifiedFamily(cycleGeneration, next, &PoolTestHost::Owner) == ClockResult::OwnerConflict && fixture.Count(1) == 0 &&
                  referenceCalls == 1 && finalCalls == 1 && removeCalls == 0,
              "KeepResident renewal carries an unclaimed domain unchanged and cannot manufacture initialized-empty replacement ownership");
    }
    void FaultCannotRenewChecks(RemoveFixture& fixture, std::uintptr_t image)
    {
        ResetCycleFixture(fixture);
        CycleController controller;
        CycleToken      token;
        ReserveClaimedCycle(controller, token);
        referenceMode = 3;
        Check(controller.Acquire(image, cycleGeneration, token, &PoolTestHost::Owner) == ClockResult::Expired && fixture.Count(1) == 1 && referenceCalls == 1,
              "accepted native acquire with lost owner acknowledgement retains its actual count");
        PoolTestHost::ownerResult = ClockResult::Ok;
        CycleToken output{91, 92, 93};
        const auto sentinel = output;
        Check(ReadCycle(controller, token).phase == CyclePhase::FaultDebt &&
                  controller.RenewCycle(cycleGeneration, token, &PoolTestHost::Owner, output) == ClockResult::OwnerConflict &&
                  SameCycleToken(output, sentinel) && referenceCalls == 1 && finalCalls == 0 && fixture.Count(1) == 1 && controller.IsRetained() &&
                  controller.RecoveryPending(),
              "faulted family cannot renew, change caller output or replay acquisition after readmission");
    }
    void UnknownReleaseCannotRenewChecks(RemoveFixture& fixture, std::uintptr_t image)
    {
        ResetCycleFixture(fixture);
        CycleController controller;
        CycleToken      token;
        ReserveClaimedCycle(controller, token);
        Check(controller.Acquire(image, cycleGeneration, token, &PoolTestHost::Owner) == ClockResult::Ok,
              "unknown-release case starts with one genuine native held reference");
        finalMode = 11;
        Check(controller.Release(image, cycleGeneration, token, FR::ZeroPolicy::KeepResident, &PoolTestHost::Owner) == ClockResult::OriginalException &&
                  fixture.Count(1) == 0 && finalCalls == 1,
              "the real final exchange can consume the count then return an unknown acknowledgement");
        const auto state = ReadCycle(controller, token);
        CycleToken output{91, 92, 93};
        const auto sentinel = output;
        Check(state.phase == CyclePhase::FaultDebt && state.release.reference.entered && !state.release.reference.accepted &&
                  state.release.reference.exceptionCode == 0xe05c0002 &&
                  controller.RenewCycle(cycleGeneration, token, &PoolTestHost::Owner, output) == ClockResult::OwnerConflict &&
                  SameCycleToken(output, sentinel) && controller.Acquire(image, cycleGeneration, token, &PoolTestHost::Owner) == ClockResult::OwnerConflict &&
                  controller.RevokeUnacquired(cycleGeneration) == ClockResult::Ok && controller.IsRetained() && controller.HasPending() &&
                  controller.RecoveryPending() && referenceCalls == 1 && finalCalls == 1 && removeCalls == 0 && fixture.Count(1) == 0,
              "unknown final acknowledgement retains the exact debt and refuses cycle renewal, reacquisition, discard and native replay");
    }
}
int main(int argc, char** argv)
{
    Check(argc == 2 && std::filesystem::path(argv[1]).is_absolute(), "family-cycle evidence stays in its workspace-owned directory");
    RemoveFixture fixture(argv[1]);
    finalFixture = activeRemove = &fixture;
    activeAcquire = &fixture;
    activeReference = &fixture;
    const auto image = fixture.transform.image.image;
    PoolTestHost::image = image;
    ConstructorFamilyDomainChecks(fixture);
    ActualCycleChecks(fixture, image);
    UnclaimedFamilyChecks(fixture, image);
    FaultCannotRenewChecks(fixture, image);
    UnknownReleaseCannotRenewChecks(fixture, image);
    std::puts("Production native model unmodified reference cycle fixture completed");
    std::printf("Passed %u native model unmodified reference cycle checks (64-bit; no native engine code executed)\n", checks);
    return 0;
}
