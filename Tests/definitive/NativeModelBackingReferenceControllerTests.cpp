/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Retain real model acquisition receipts in the actual owner journal
 *****************************************************************************/
#pragma push_macro("main")
#undef main
#define main NativeModelBackingReferenceHistoricalAcquireMain
#include "NativeModelReferenceAcquireTests.cpp"
#undef main
#pragma pop_macro("main")
#include "../../Client/game_de/NativeModelBackingReferenceController.h"

namespace
{
    using BackingController = mta::game_de::NativeModelBackingReferenceController;
    using BackingToken = mta::game_de::NativeModelBackingReferenceToken;
    using BackingState = mta::game_de::NativeModelBackingReferenceState;
    using BackingPhase = mta::game_de::NativeModelBackingReferencePhase;
    constexpr std::uint64_t backingGeneration = 17;
    constexpr std::uint32_t backingModel = 17;

    void ResetBackingFixture(AcquireFixture& fixture)
    {
        fixture.Reset();
        reserveCalls = publishCalls = acquireMode = referenceCalls = referenceMode = 0;
        acquireKey = backingModel;
        publishAbiValid = false;
    }

    bool SameBackingToken(const BackingToken& left, const BackingToken& right)
    {
        return left.ownerGeneration == right.ownerGeneration && left.serial == right.serial && left.modelIndex == right.modelIndex;
    }

    bool SameBackingState(const BackingState& left, const BackingState& right)
    {
        const auto& a = left.acquisition;
        const auto& b = right.acquisition;
        return SameBackingToken(left.reference, right.reference) && left.phase == right.phase && left.lastResult == right.lastResult && a.root == b.root &&
               a.map == b.map && a.row == b.row && a.heap == b.heap && a.modelIndex == b.modelIndex && a.before == b.before && a.target == b.target &&
               a.observed == b.observed && a.exceptionCode == b.exceptionCode && a.reservedIndex == b.reservedIndex && a.publishedIndex == b.publishedIndex &&
               a.entered == b.entered && a.reserved == b.reserved && a.published == b.published && a.accepted == b.accepted;
    }

    void ReserveBoundBacking(BackingController& controller, BackingToken& token)
    {
        Check(controller.Reserve(backingGeneration, backingModel, &PoolTestHost::Owner, token) == ClockResult::Ok,
              "actual controller issues an owned CPU record before acquisition");
        Check(controller.Bind(backingGeneration, token, &PoolTestHost::Owner) == ClockResult::Ok,
              "actual controller consumes the reserved construction claim once");
    }

    BackingState ReadBacking(BackingController& controller, const BackingToken& token)
    {
        BackingState state;
        Check(controller.Read(backingGeneration, token, &PoolTestHost::Owner, state) == ClockResult::Ok && SameBackingToken(state.reference, token),
              "actual controller returns the durable state for the exact issued identity");
        return state;
    }

    void BackingIdentityChecks(AcquireFixture& fixture, std::uintptr_t image)
    {
        ResetBackingFixture(fixture);
        BackingController controller;
        Check(!controller.HasPending() && !controller.IsRetained() && !controller.RecoveryPending(), "fresh controller has no records or native debt");
        BackingToken token{91, 92, 93};
        const auto   previous = token;
        Check(controller.Reserve(0, backingModel, &PoolTestHost::Owner, token) == ClockResult::InvalidArgument && SameBackingToken(token, previous),
              "zero generation refuses without replacing the caller's token");
        Check(controller.Reserve(backingGeneration, B::ModelCount, &PoolTestHost::Owner, token) == ClockResult::InvalidArgument &&
                  SameBackingToken(token, previous),
              "out-of-range model refuses before record issuance");
        PoolTestHost::ownerResult = ClockResult::Expired;
        Check(controller.Reserve(backingGeneration, backingModel, &PoolTestHost::Owner, token) == ClockResult::Expired && SameBackingToken(token, previous),
              "revoked owner cannot issue an output token");
        PoolTestHost::ownerResult = ClockResult::Ok;
        Check(controller.Reserve(backingGeneration, backingModel, &PoolTestHost::Owner, token) == ClockResult::Ok &&
                  token.ownerGeneration == backingGeneration && token.modelIndex == backingModel && token.serial != 0,
              "reserve issues generation, serial and selected-model identity");
        auto state = ReadBacking(controller, token);
        Check(state.phase == BackingPhase::Prepared && !state.acquisition.entered && controller.HasPending() && !controller.IsRetained() &&
                  !controller.RecoveryPending() && reserveCalls == 0 && publishCalls == 0 && referenceCalls == 0,
              "Prepared reserve is CPU-only and cannot be mistaken for acquired native debt");
        Check(controller.Acquire(image, backingGeneration, token, &PoolTestHost::Owner) == ClockResult::OwnerConflict &&
                  ReadBacking(controller, token).phase == BackingPhase::Prepared,
              "unbound construction claim cannot enter acquisition");

        BackingState sentinel;
        sentinel.reference = {99, 111, 222};
        sentinel.phase = BackingPhase::FaultDebt;
        sentinel.lastResult = ClockResult::Busy;
        sentinel.acquisition.root = 31;
        sentinel.acquisition.map = 32;
        sentinel.acquisition.row = 33;
        sentinel.acquisition.heap = 34;
        sentinel.acquisition.modelIndex = 35;
        sentinel.acquisition.before = 36;
        sentinel.acquisition.target = 37;
        sentinel.acquisition.observed = 38;
        sentinel.acquisition.exceptionCode = 39;
        sentinel.acquisition.reservedIndex = 40;
        sentinel.acquisition.publishedIndex = 41;
        sentinel.acquisition.entered = sentinel.acquisition.reserved = sentinel.acquisition.published = sentinel.acquisition.accepted = true;
        for (const auto changed :
             {BackingToken{token.ownerGeneration, token.serial + 1, token.modelIndex}, BackingToken{token.ownerGeneration + 1, token.serial, token.modelIndex},
              BackingToken{token.ownerGeneration, token.serial, token.modelIndex + 1}})
        {
            state = sentinel;
            Check(controller.Read(backingGeneration, changed, &PoolTestHost::Owner, state) == ClockResult::BindingChanged && SameBackingState(state, sentinel),
                  "unknown serial, stale generation and model tampering preserve the full previous output state");
            Check(controller.Bind(backingGeneration, changed, &PoolTestHost::Owner) == ClockResult::BindingChanged &&
                      controller.Acquire(image, backingGeneration, changed, &PoolTestHost::Owner) == ClockResult::BindingChanged,
                  "unissued identity cannot bind or acquire a native reference");
        }
        state = sentinel;
        Check(controller.Read(backingGeneration + 1, token, &PoolTestHost::Owner, state) == ClockResult::BindingChanged && SameBackingState(state, sentinel),
              "later generation cannot read a previous generation's record");
        Check(controller.Bind(backingGeneration, token, &PoolTestHost::Owner) == ClockResult::Ok &&
                  controller.Bind(backingGeneration, token, &PoolTestHost::Owner) == ClockResult::OwnerConflict &&
                  ReadBacking(controller, token).phase == BackingPhase::Bound,
              "one-shot Bind accepts exactly one constructor for the issued record");
        Check(reserveCalls == 0 && publishCalls == 0 && referenceCalls == 0, "identity and construction operations never invoke native mutation callbacks");
        Check(controller.RevokeUnacquired(backingGeneration + 1) == ClockResult::Ok && controller.HasPending(),
              "revoking another generation does not erase the issued construction claim");
        Check(controller.RevokeUnacquired(backingGeneration) == ClockResult::Ok && !controller.HasPending() && !controller.IsRetained() &&
                  !controller.RecoveryPending(),
              "explicit revocation erases an unacquired Bound record without native cleanup");
        state = sentinel;
        Check(controller.Read(backingGeneration, token, &PoolTestHost::Owner, state) == ClockResult::BindingChanged && SameBackingState(state, sentinel),
              "revoked identity is unavailable and output remains unchanged");
        BackingToken next;
        Check(controller.Reserve(backingGeneration, backingModel, &PoolTestHost::Owner, next) == ClockResult::Ok && next.serial > token.serial,
              "erasing a CPU-only record never reuses its serial");
        Check(controller.RevokeUnacquired(backingGeneration) == ClockResult::Ok && !controller.HasPending(), "Prepared records can also be explicitly revoked");
    }

    void BackingMissingKeyChecks(AcquireFixture& fixture, std::uintptr_t image)
    {
        ResetBackingFixture(fixture);
        std::int32_t published = -1;
        {
            BackingController controller;
            BackingToken      token;
            ReserveBoundBacking(controller, token);
            Check(controller.Acquire(image, backingGeneration, token, &PoolTestHost::Owner) == ClockResult::Ok,
                  "actual controller delegates missing-key acquisition to the genuine reserve/init/publish transaction");
            const auto state = ReadBacking(controller, token);
            published = state.acquisition.publishedIndex;
            Check(state.phase == BackingPhase::Held && state.lastResult == ClockResult::Ok && state.acquisition.entered && state.acquisition.reserved &&
                      state.acquisition.published && state.acquisition.accepted && state.acquisition.before == 0 && state.acquisition.target == 1 &&
                      state.acquisition.modelIndex == backingModel && fixture.NativeCount(published) == 1 && reserveCalls == 1 && publishCalls == 1 &&
                      referenceCalls == 0 && publishAbiValid && controller.HasPending() && controller.IsRetained() && !controller.RecoveryPending(),
                  "Held keeps the exact accepted publication receipt and durable native reference obligation");
            Check(controller.Acquire(image, backingGeneration, token, &PoolTestHost::Owner) == ClockResult::Ok && fixture.NativeCount(published) == 1 &&
                      reserveCalls == 1 && publishCalls == 1 && referenceCalls == 0,
                  "repeated Held acquire acknowledges the same capability without another native reservation or increment");
            Check(controller.RevokeUnacquired(backingGeneration) == ClockResult::Ok && ReadBacking(controller, token).phase == BackingPhase::Held &&
                      controller.IsRetained(),
                  "unacquired-only revocation cannot erase an accepted missing-key reference");
        }
        Check(fixture.NativeCount(published) == 1 && reserveCalls == 1 && publishCalls == 1 && referenceCalls == 0,
              "controller destruction releases CPU records only and never decrements the accepted published count");
    }

    void BackingExistingKeyChecks(AcquireFixture& fixture, std::uintptr_t image)
    {
        ResetBackingFixture(fixture);
        fixture.Shape(4, 1, false, 4);
        fixture.Row(1, 4, std::uint32_t{2});
        {
            BackingController controller;
            BackingToken      token;
            ReserveBoundBacking(controller, token);
            Check(controller.Acquire(image, backingGeneration, token, &PoolTestHost::Owner) == ClockResult::Ok,
                  "existing-key controller acquisition executes the real shared native adjustment");
            const auto state = ReadBacking(controller, token);
            Check(state.phase == BackingPhase::Held && state.acquisition.entered && state.acquisition.accepted && !state.acquisition.reserved &&
                      !state.acquisition.published && state.acquisition.heap == 0 && state.acquisition.before == 2 && state.acquisition.target == 3 &&
                      state.acquisition.observed == 2 && fixture.Count(1) == 3 && referenceCalls == 1 && reserveCalls == 0 && publishCalls == 0,
                  "existing-key receipt preserves one actual compare/exchange and never reserves or publishes a row");
            Check(controller.Acquire(image, backingGeneration, token, &PoolTestHost::Owner) == ClockResult::Ok && fixture.Count(1) == 3 &&
                      referenceCalls == 1 && reserveCalls == 0 && publishCalls == 0,
                  "Held existing-key acquisition is idempotent and does not increment the native counter again");
        }
        Check(fixture.Count(1) == 3 && referenceCalls == 1, "controller destruction cannot infer a native release for an existing acquired key");
    }

    void BackingPreentryChecks(AcquireFixture& fixture, std::uintptr_t image)
    {
        ResetBackingFixture(fixture);
        fixture.Stream(backingModel, 0x40, 0);
        BackingController controller;
        BackingToken      token;
        ReserveBoundBacking(controller, token);
        Check(controller.Acquire(image, backingGeneration, token, &PoolTestHost::Owner) == ClockResult::NotInitialized,
              "actual unloaded-model admission refuses before native acquisition entry");
        const auto state = ReadBacking(controller, token);
        Check(state.phase == BackingPhase::Bound && state.lastResult == ClockResult::NotInitialized && !state.acquisition.entered &&
                  !state.acquisition.reserved && !state.acquisition.published && !state.acquisition.accepted && !controller.IsRetained() &&
                  !controller.RecoveryPending() && controller.HasPending() && reserveCalls == 0 && publishCalls == 0 && referenceCalls == 0,
              "no-effect refusal retains Bound CPU identity without manufacturing native fault debt");
        Check(controller.RevokeUnacquired(backingGeneration) == ClockResult::Ok && !controller.HasPending(),
              "explicit End-style revocation can erase a preentry refusal without native work");
    }

    void BackingExchangeDebtChecks(AcquireFixture& fixture, std::uintptr_t image, unsigned mode)
    {
        ResetBackingFixture(fixture);
        fixture.Shape(4, 1, false, 4);
        fixture.Row(1, 4, std::uint32_t{2});
        referenceMode = mode;
        {
            BackingController controller;
            BackingToken      token;
            ReserveBoundBacking(controller, token);
            const auto expected = mode == 3 ? ClockResult::Expired : ClockResult::OriginalException;
            Check(controller.Acquire(image, backingGeneration, token, &PoolTestHost::Owner) == expected && referenceCalls == 1 && reserveCalls == 0 &&
                      publishCalls == 0,
                  "controller preserves the original entered exchange failure without another native call");
            // Re-admit observation only so the test can inspect the retained
            // receipt. The FaultDebt phase itself must still refuse replay.
            PoolTestHost::ownerResult = ClockResult::Ok;
            const auto state = ReadBacking(controller, token);
            Check(state.phase == BackingPhase::FaultDebt && state.lastResult == expected && state.acquisition.entered &&
                      state.acquisition.accepted == (mode == 3) && fixture.Count(1) == (mode == 3 ? 3u : 2u) && controller.HasPending() &&
                      controller.IsRetained() && controller.RecoveryPending(),
                  "accepted lost acknowledgement or unknown exchange remains durable FaultDebt with its exact native effect");
            Check(mode != 1 || state.acquisition.exceptionCode == 0xe0580001, "unknown exchange receipt retains the genuine callback exception code");
            Check(controller.Acquire(image, backingGeneration, token, &PoolTestHost::Owner) == ClockResult::OwnerConflict && referenceCalls == 1 &&
                      fixture.Count(1) == (mode == 3 ? 3u : 2u),
                  "FaultDebt refuses repeated acquire even after observation authority is available again");
            Check(controller.RevokeUnacquired(backingGeneration) == ClockResult::Ok && controller.HasPending() && controller.IsRetained() &&
                      controller.RecoveryPending() && SameBackingState(ReadBacking(controller, token), state),
                  "unacquired-only revocation preserves the full entered debt record");
        }
        Check(fixture.Count(1) == (mode == 3 ? 3u : 2u) && referenceCalls == 1,
              "destruction neither rolls back an accepted count nor guesses cleanup for an unknown entered exchange");
    }

    void BackingReserveDebtChecks(AcquireFixture& fixture, std::uintptr_t image, unsigned mode)
    {
        ResetBackingFixture(fixture);
        acquireMode = mode;
        BackingController controller;
        BackingToken      token;
        ReserveBoundBacking(controller, token);
        Check(controller.Acquire(image, backingGeneration, token, &PoolTestHost::Owner) == ClockResult::OriginalException,
              "genuine reserve callback exception propagates through the actual controller");
        const auto state = ReadBacking(controller, token);
        const auto rows = mode == 1 ? 1 : 2;
        Check(state.phase == BackingPhase::FaultDebt && state.acquisition.entered && !state.acquisition.reserved && !state.acquisition.published &&
                  !state.acquisition.accepted && state.acquisition.exceptionCode == (mode == 1 ? 0xe0580101u : 0xe0580102u) && reserveCalls == 1 &&
                  publishCalls == 0 && referenceCalls == 0 && fixture.Integer(8) == rows && controller.HasPending() && controller.IsRetained() &&
                  controller.RecoveryPending(),
              "pre-return reserve failure retains entered unknown debt and any partial owned-row reservation");
        Check(controller.Acquire(image, backingGeneration, token, &PoolTestHost::Owner) == ClockResult::OwnerConflict && reserveCalls == 1 &&
                  publishCalls == 0 && fixture.Integer(8) == rows,
              "unknown reserve debt cannot replay reservation or proceed to publication");
        Check(controller.RevokeUnacquired(backingGeneration) == ClockResult::Ok && SameBackingState(ReadBacking(controller, token), state) &&
                  controller.IsRetained() && controller.RecoveryPending(),
              "explicit CPU-only revocation cannot discard a reserve operation whose native result is unknown");
    }
}

int main(int argc, char** argv)
{
    Check(argc == 2 && std::filesystem::path(argv[1]).is_absolute(), "controller evidence uses an explicit workspace-owned directory");
    // Reuse only the owned-image fixture and final native-call stubs. None of
    // the historical request/reference/acquisition matrix mains is invoked.
    AcquireFixture fixture(argv[1]);
    activeAcquire = &fixture;
    activeReference = &fixture;
    PoolTestHost::image = fixture.transform.image.image;
    const auto image = fixture.transform.image.image;
    BackingIdentityChecks(fixture, image);
    BackingMissingKeyChecks(fixture, image);
    BackingExistingKeyChecks(fixture, image);
    BackingPreentryChecks(fixture, image);
    BackingExchangeDebtChecks(fixture, image, 3);
    BackingExchangeDebtChecks(fixture, image, 1);
    BackingReserveDebtChecks(fixture, image, 1);
    BackingReserveDebtChecks(fixture, image, 2);
    std::puts("Production native model backing reference controller fixture completed");
    std::printf("Passed %u native model backing reference controller checks (64-bit; no native engine code executed)\n", checks);
    return 0;
}
