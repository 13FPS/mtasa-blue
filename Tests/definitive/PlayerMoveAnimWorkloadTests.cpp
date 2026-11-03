/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Explicit workload policy without executing mapped native originals
 ****************************************************************************/
#include <windows.h>
#include <SharedUtil.Defines.h>
#include "../../Client/game_de/NativeMoveAnimWorkload.h"
#include "../../Client/core/DefinitiveClockReference.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <iterator>
#include <stdexcept>
#include <string>
#include <thread>
#include <type_traits>
#include <vector>

namespace
{
    using namespace mta::game_de;
    using Phase = MoveAnimWorkloadPhaseV1;
    using State = MoveAnimWorkloadStateV1;
    unsigned checks = 0;
    void     Check(bool value, const char* reason)
    {
        ++checks;
        if (!value)
            throw std::runtime_error(reason);
    }
    void Require(bool value, const char* reason)
    {
        if (!value)
            throw std::runtime_error(reason);
    }
    bool Same(const NativePedReferenceToken& first, const NativePedReferenceToken& second) noexcept
    {
        return first.version == second.version && first.ownerGeneration == second.ownerGeneration && first.slot == second.slot;
    }
    struct LegacyOwner : NativeGameTimingAccess
    {
        ClockResult SetTimeScale(float) noexcept override { return ClockResult::NotInitialized; }
        ClockResult SetMinuteDuration(std::uint32_t) noexcept override { return ClockResult::NotInitialized; }
        ClockResult Pause(bool) noexcept override { return ClockResult::NotInitialized; }
    };
    struct Owner final : LegacyOwner
    {
        const DWORD                   engineThread = GetCurrentThreadId();
        const NativePedReferenceToken issued{UINT64_C(0xfedcba9876543210), UINT64_C(0x1234567887654321), 3};
        std::uint32_t                 handle = 0x301u, selector = 0;
        std::int32_t                  group = 56, custom = 0;
        bool                          afterApplication = false, registered = false, tokensMatch = true, oneEffectPerTick = true;
        bool                          enteredWriteFailure = true;
        unsigned                      unexpected = 0;
        std::string                   fail;
        ClockResult                   failure = ClockResult::BindingChanged;
        std::vector<std::string>      calls, effects;
        ClockResult Guard() const noexcept { return GetCurrentThreadId() == engineThread && afterApplication ? ClockResult::Ok : ClockResult::WrongThread; }
        ClockResult Call(const char* name, bool effect = false) noexcept
        {
            const auto guard = Guard();
            if (guard != ClockResult::Ok)
                return guard;
            calls.emplace_back(name);
            if (effect)
                effects.emplace_back(name);
            return fail == name ? failure : ClockResult::Ok;
        }
        bool Token(const NativePedReferenceToken& token) noexcept
        {
            tokensMatch = tokensMatch && Same(token, issued);
            return tokensMatch && registered;
        }
        ClockResult ReadCurrentPlayerMoveAnimCandidate(std::uint32_t& outHandle, std::uint32_t& outSelector, std::int32_t& outGroup) noexcept override
        {
            const auto result = Call("candidate");
            if (result == ClockResult::Ok)
            {
                outHandle = handle;
                outSelector = selector;
                outGroup = group;
            }
            return result;
        }
        ClockResult CheckPlayerMoveAnimWorkloadGroup(std::int32_t selected) noexcept override
        {
            const auto result = Call("group");
            return selected >= 54 && selected <= 56 ? result : ClockResult::InvalidArgument;
        }
        ClockResult AcquirePedReference(std::uint32_t selected, NativePedReferenceToken& output) noexcept override
        {
            const auto result = Call("acquire", true);
            if (result == ClockResult::Ok && selected == handle)
            {
                output = issued;
                registered = true;
                return result;
            }
            return result == ClockResult::Ok ? ClockResult::BindingChanged : result;
        }
        ClockResult EnablePlayerMoveAnimHooks() noexcept override { return Call("enable", true); }
        ClockResult PublishPlayerMoveAnimOverride(const NativePedReferenceToken& token, std::int32_t selected) noexcept override
        {
            if (!Token(token))
                return ClockResult::BindingChanged;
            const auto result = Call("publish", true);
            if (result == ClockResult::Ok)
                custom = selected;
            return result;
        }
        ClockResult WriteCurrentPlayerMoveAnimGroup(const NativePedReferenceToken& token, std::int32_t selected) noexcept override
        {
            if (!Token(token) || custom != selected)
                return ClockResult::BindingChanged;
            const auto result = Call("write", true);
            if (result == ClockResult::Ok || enteredWriteFailure)
                group = selected;
            return result;
        }
        ClockResult ReapplyCurrentPlayerMoveAnim(const NativePedReferenceToken& token) noexcept override
        {
            if (!Token(token) || custom != group)
                return ClockResult::BindingChanged;
            return Call("reapply", true);
        }
        ClockResult WithdrawPlayerMoveAnimOverride(const NativePedReferenceToken& token) noexcept override
        {
            if (!Token(token))
                return ClockResult::BindingChanged;
            const auto result = Call("withdraw", true);
            if (result == ClockResult::Ok)
                custom = 0;
            return result;
        }
        ClockResult ReleasePedReference(const NativePedReferenceToken& token) noexcept override
        {
            if (!Token(token))
                return ClockResult::BindingChanged;
            const auto result = Call("release", true);
            if (result == ClockResult::Ok)
                registered = false;
            return result;
        }
        ClockResult RequestStreamingModel(std::uint32_t, std::uint32_t) noexcept override
        {
            ++unexpected;
            return ClockResult::OwnerConflict;
        }
        ClockResult LoadStreamingModels(bool) noexcept override
        {
            ++unexpected;
            return ClockResult::OwnerConflict;
        }
        ClockResult AddAnimationBlockRef(const NativeAnimationBlockIdentity&) noexcept override
        {
            ++unexpected;
            return ClockResult::OwnerConflict;
        }
    };
    State Read(const NativeMoveAnimWorkload& workload)
    {
        State state;
        Require(workload.Read(state), "Expected a coherent published workload snapshot");
        return state;
    }
    void Tick(NativeMoveAnimWorkload& workload, Owner& owner, std::uint64_t now)
    {
        const auto before = owner.effects.size();
        owner.afterApplication = true;
        workload.Tick(owner, now);
        owner.afterApplication = false;
        owner.oneEffectPerTick = owner.oneEffectPerTick && owner.effects.size() - before <= 1;
    }
    void Drive(NativeMoveAnimWorkload& workload, Owner& owner, unsigned count = 20, std::uint64_t first = 1000)
    {
        for (unsigned index = 0; index != count; ++index)
            Tick(workload, owner, first + index * 100);
    }
    MoveAnimWorkloadRequestV1 Request(std::int32_t selected = 54, std::uint32_t duration = 1)
    {
        MoveAnimWorkloadRequestV1 request;
        request.preferredGroup = selected;
        request.durationMs = duration;
        return request;
    }
    ClockServiceStateV1 EndAcknowledgement()
    {
        // This is explicit double evidence supplied only after the fixture's
        // simulated End. Actual native owner cleanup is a separate owner test.
        ClockServiceStateV1 state;
        state.generation = UINT64_C(0x1234567887654321);
        state.producerThread = GetCurrentThreadId();
        state.restored = 1;
        return state;
    }
    void Contract()
    {
        const MoveAnimWorkloadRequestV1 request;
        const State                     defaults;
        Check(NativeMoveAnimWorkload::ValidRequest(request), "default explicit request is canonical");
        Check(request.size == sizeof(request) && request.version == 1 && request.durationMs == 8000 && request.preferredGroup == 54 &&
                  std::all_of(std::begin(request.reserved), std::end(request.reserved), [](auto value) { return value == 0; }),
              "request defaults carry no implicit alternate mode");
        Check(defaults.phase == Phase::Idle && defaults.result == ClockResult::NotInitialized && defaults.physicalConfirmationRequired == 1 &&
                  !defaults.fullMtaReady && !defaults.tokenRegistered && !defaults.writeAcknowledged,
              "native evidence never claims physical or full-MTA completion");
        Check(sizeof(ClockServiceStateV1) == 232 && sizeof(ClockServiceRequestV1) == 24 && DefinitiveClockReference::FieldCount == 96,
              "the existing clock API and read-only96 report are unchanged");
        NativeMoveAnimWorkload workload;
        State                  output;
        std::memset(&output, 0x6a, sizeof(output));
        std::array<unsigned char, sizeof(output)> poison{};
        std::memcpy(poison.data(), &output, sizeof(output));
        Check(!workload.Read(output) && !std::memcmp(&output, poison.data(), sizeof(output)), "unpublished optional state preserves caller output");
        for (const auto size : {0u, static_cast<unsigned>(sizeof(request) - 1), static_cast<unsigned>(sizeof(request) + 4)})
        {
            auto invalid = request;
            invalid.size = size;
            Check(!NativeMoveAnimWorkload::ValidRequest(invalid) && !workload.Reset(invalid), "wrong request size refuses before publication");
        }
        for (const auto version : {0u, 2u})
        {
            auto invalid = request;
            invalid.version = version;
            Check(!NativeMoveAnimWorkload::ValidRequest(invalid) && !workload.Reset(invalid), "unknown workload version refuses");
        }
        for (const auto duration : {0u, MoveAnimWorkloadMaximumDurationMsV1 + 1})
        {
            auto invalid = request;
            invalid.durationMs = duration;
            Check(!NativeMoveAnimWorkload::ValidRequest(invalid) && !workload.Reset(invalid), "out-of-range observation duration refuses");
        }
        for (const auto group : {53, 57})
        {
            auto invalid = request;
            invalid.preferredGroup = group;
            Check(!NativeMoveAnimWorkload::ValidRequest(invalid) && !workload.Reset(invalid), "only explicit already-loaded candidate IDs are admitted");
        }
        for (unsigned index = 0; index != 4; ++index)
        {
            auto invalid = request;
            invalid.reserved[index] = 1;
            Check(!NativeMoveAnimWorkload::ValidRequest(invalid) && !workload.Reset(invalid), "every reserved request word remains zero");
        }
        Check(!workload.Read(output) && !std::memcmp(&output, poison.data(), sizeof(output)), "invalid requests never replace the unpublished output");
        for (const auto group : {54, 55, 56})
            Check(NativeMoveAnimWorkload::ValidRequest(Request(group)), "each allowed preferred group is independently valid");
        Check(workload.Reset(request), "producer explicitly resets the new workload");
        const auto state = Read(workload);
        Check(state.phase == Phase::Preflight && state.sequence && state.cleanupPending == 1 && !state.endAcknowledged && state.selectedGroup == 54,
              "explicit reset publishes preflight without native effects or end acknowledgement");
    }
    void Success()
    {
        for (const auto selected : {54, 55, 56})
        {
            NativeMoveAnimWorkload workload;
            Owner                  owner;
            Check(workload.Reset(Request(selected)), "explicit loaded-group workload resets");
            Drive(workload, owner);
            const auto state = Read(workload);
            Check(state.phase == Phase::Completed && state.result == ClockResult::Ok && state.cleanupPending == 1 && !state.endAcknowledged,
                  "callback completion still waits for the real native owner End");
            Check(owner.effects == std::vector<std::string>{"acquire", "enable", "publish", "write", "reapply", "withdraw", "release"},
                  "one registered token follows the exact whole-operation effect itinerary");
            Check(owner.tokensMatch && !owner.registered && !owner.custom && owner.group == selected,
                  "withdraw/release preserve the acknowledged native group");
            Check(state.hooksEnabled && state.writeAcknowledged && state.reapplyAcknowledged && state.withdrawAcknowledged && state.releaseAcknowledged &&
                      !state.overridePublished && !state.tokenRegistered && state.physicalConfirmationRequired == 1 && !state.fullMtaReady,
                  "success records actual acknowledgements without claiming visual validation");
            Check(owner.oneEffectPerTick && state.sequence > 1 && state.afterGroup == selected,
                  "each callback performs at most one effect and publishes the actual observed group");
            Check(owner.calls == std::vector<std::string>{"candidate", "group", "candidate", "acquire", "candidate", "enable", "candidate", "publish",
                                                          "candidate", "write", "candidate", "reapply", "candidate", "withdraw", "release"},
                  "fresh candidate reads immediately precede every admitted mutation and observation");
            Check(!owner.unexpected, "the live validation path never requests, loads or adds a speculative block reference");
            workload.Retired(EndAcknowledgement());
            const auto retired = Read(workload);
            Check(retired.phase == Phase::Retired && retired.endAcknowledged == 1 && !retired.cleanupPending && !retired.cleanupViaOwnerEnd &&
                      retired.owner.restored == 1 && retired.owner.generation == owner.issued.ownerGeneration,
                  "only external native End acknowledgement retires the producer context");
            Check(owner.group == selected && retired.writeAcknowledged && retired.reapplyAcknowledged && retired.physicalConfirmationRequired &&
                      !retired.fullMtaReady,
                  "owner retirement does not roll back the intentional scalar or forge physical confirmation");
        }
    }
    void CompletionAdmission()
    {
        NativeMoveAnimWorkload workload;
        Owner                  owner;
        Require(workload.Reset(Request()), "setup");
        const auto pending = Read(workload);
        Check(!workload.Reset(Request(55)), "a new request cannot erase a pending native owner attempt");
        const auto unchanged = Read(workload);
        Check(pending.sequence == unchanged.sequence && pending.phase == unchanged.phase && pending.selectedGroup == unchanged.selectedGroup &&
                  pending.cleanupPending == unchanged.cleanupPending && pending.result == unchanged.result && pending.notEntered == unchanged.notEntered,
              "refused reset preserves the complete published attempt");
        // The export layer supplies NotEntered only after proving absence of
        // a generation and retained owner; this test invokes that explicit seam.
        workload.NotEntered(ClockResult::NotInitialized);
        const auto noEntry = Read(workload);
        Check(noEntry.phase == Phase::Refused && noEntry.notEntered == 1 && noEntry.result == ClockResult::NotInitialized && !noEntry.cleanupPending,
              "proven no-entry is a separate retired refusal route");
        Check(!noEntry.endAcknowledged && !noEntry.withdrawAcknowledged && !noEntry.releaseAcknowledged && !noEntry.tokenRegistered &&
                  !noEntry.overridePublished && noEntry.physicalConfirmationRequired && !noEntry.fullMtaReady,
              "no-generation refusal does not forge native End or cleanup receipts");
        Check(workload.Reset(Request()), "new explicit request may follow the completed no-entry attempt");
        Check(!workload.Reset(Request(56)), "that new live attempt cannot be replaced");
        Drive(workload, owner, 3);
        workload.Retired(EndAcknowledgement());
        const auto stopped = Read(workload);
        Check(stopped.result == ClockResult::Stopped && stopped.phase == Phase::Retired && stopped.endAcknowledged && stopped.cleanupViaOwnerEnd &&
                  !stopped.cleanupPending && !stopped.withdrawAcknowledged && !stopped.releaseAcknowledged && !stopped.writeAcknowledged,
              "owner End of an incomplete prefix is cancellation, with honest bulk cleanup instead of fabricated explicit release");
        Check(workload.Reset(Request()), "producer may reuse the controller only after the prior owner is retired");
    }
    void FailurePrefixes()
    {
        for (const std::string phase : {"candidate", "group", "acquire", "enable", "publish", "write", "reapply", "withdraw", "release"})
        {
            NativeMoveAnimWorkload workload;
            Owner                  owner;
            owner.fail = phase;
            const bool unknown = phase == "write" || phase == "reapply" || phase == "withdraw" || phase == "release";
            owner.failure = unknown ? ClockResult::OriginalException : ClockResult::BindingChanged;
            Check(workload.Reset(Request()), "failure case resets only from producer-owned idle");
            Drive(workload, owner);
            const auto state = Read(workload);
            Check(state.result == owner.failure && state.phase == (unknown ? Phase::Unknown : Phase::Refused),
                  "first refused/unknown prefix is reported truthfully");
            std::vector<std::string> expected;
            if (phase != "candidate" && phase != "group")
                expected.push_back("acquire");
            if (phase != "candidate" && phase != "group" && phase != "acquire")
                expected.push_back("enable");
            if (phase == "publish" || unknown)
                expected.push_back("publish");
            if (unknown)
                expected.push_back("write");
            if (phase == "reapply" || phase == "withdraw" || phase == "release")
                expected.push_back("reapply");
            if (unknown)
                expected.push_back("withdraw");
            if (phase == "enable" || phase == "publish" || phase == "write" || phase == "reapply" || phase == "release")
                expected.push_back("release");
            Check(owner.effects == expected, "failure retains the exact attempt prefix and only admitted explicit cleanup");
            Check(state.cleanupPending == 1 && !state.endAcknowledged && state.physicalConfirmationRequired && !state.fullMtaReady,
                  "even explicit cleanup acknowledgements cannot dispose of native owner debt");
            const auto calls = owner.calls.size();
            Drive(workload, owner, 4, 5000);
            Check(owner.calls.size() == calls, "finished refused/unknown operations and failed cleanup are never replayed");
            const bool writeAck = phase == "reapply" || phase == "withdraw" || phase == "release";
            const bool reapplyAck = phase == "withdraw" || phase == "release";
            Check(state.writeAcknowledged == static_cast<std::uint32_t>(writeAck) && state.reapplyAcknowledged == static_cast<std::uint32_t>(reapplyAck) &&
                      (phase != "withdraw" || (state.withdrawAttempted && !state.withdrawAcknowledged)) &&
                      (phase != "release" || (state.releaseAttempted && !state.releaseAcknowledged)),
                  "only successful prefixes produce acknowledgement flags");
            Check(owner.tokensMatch && owner.oneEffectPerTick && !owner.unexpected, "failure cleanup retains exact token, thread phase and no-load scope");
            workload.Retired(EndAcknowledgement());
            const auto retired = Read(workload);
            Check(retired.phase == Phase::Retired && !retired.cleanupPending && retired.endAcknowledged && retired.result == state.result &&
                      retired.withdrawAcknowledged == state.withdrawAcknowledged && retired.releaseAcknowledged == state.releaseAcknowledged &&
                      retired.cleanupViaOwnerEnd == static_cast<unsigned>(state.overridePublished || state.tokenRegistered),
                  "external owner-End retirement reports its distinct route without forging explicit receipts or erasing first failure");
            Check(owner.group == (unknown ? 54 : 56), "possibly-entered writes retain their actual scalar effect without a compensating write");
        }
    }
    void Stops()
    {
        for (const unsigned priorTicks : {0u, 3u, 4u, 5u, 6u, 8u})
        {
            NativeMoveAnimWorkload workload;
            Owner                  owner;
            Check(workload.Reset(Request(54, 8000)), "explicit stop case begins with a valid producer request");
            Drive(workload, owner, priorTicks);
            // Stop is the intentional cross-producer signal. Native calls
            // continue only through the next admitted after-application Tick.
            std::thread stopper([&] { workload.RequestStop(); });
            stopper.join();
            Drive(workload, owner, 20, 3000);
            const auto state = Read(workload);
            Check(state.phase == Phase::Refused && state.result == ClockResult::Stopped && state.cleanupPending && !state.endAcknowledged,
                  "explicit cancellation never masquerades as completed validation");
            std::vector<std::string> expected;
            if (priorTicks >= 3)
                expected.push_back("acquire");
            if (priorTicks >= 4)
                expected.push_back("enable");
            if (priorTicks >= 5)
                expected.push_back("publish");
            if (priorTicks >= 6)
                expected.push_back("write");
            if (priorTicks >= 8)
                expected.push_back("reapply");
            if (priorTicks >= 5)
                expected.push_back("withdraw");
            if (priorTicks >= 3)
                expected.push_back("release");
            Check(owner.effects == expected, "stop performs only cleanup appropriate to its accepted prefix");
            Check(owner.tokensMatch && owner.oneEffectPerTick && !owner.registered && !owner.custom && !owner.unexpected,
                  "stop retains callback authority and exact token through cleanup");
            Check(state.writeAcknowledged == static_cast<std::uint32_t>(priorTicks >= 6) &&
                      state.reapplyAcknowledged == static_cast<std::uint32_t>(priorTicks >= 8),
                  "stop does not invent later write/reapply acknowledgements");
            const auto calls = owner.calls.size();
            Drive(workload, owner, 4, 6000);
            Check(owner.calls.size() == calls, "repeated stop frames cannot replay retired operations");
            workload.Retired(EndAcknowledgement());
            const auto retired = Read(workload);
            Check(retired.endAcknowledged && !retired.cleanupPending && retired.result == ClockResult::Stopped &&
                      retired.withdrawAcknowledged == state.withdrawAcknowledged && retired.releaseAcknowledged == state.releaseAcknowledged,
                  "producer observes only explicit or real owner-End retirement receipts");
            Check(owner.group == (priorTicks >= 6 ? 54 : 56), "cancel preserves every acknowledged intentional group write");
        }
    }
    void Ownership()
    {
        NativeMoveAnimWorkload legacy;
        LegacyOwner            legacyOwner;
        Check(legacy.Reset(Request()), "optional workload may validate its request independently of an older owner");
        legacy.Tick(legacyOwner, 1000);
        const auto old = Read(legacy);
        Check(old.phase == Phase::Refused && old.result == ClockResult::NotInitialized && !old.tokenRegistered && !old.hooksEnabled,
              "missing legacy channels refuse without fabricating an engine operation");
        NativeMoveAnimWorkload missingGroup;
        Owner                  groupOwner;
        groupOwner.fail = "group";
        groupOwner.failure = ClockResult::NotInitialized;
        Require(missingGroup.Reset(Request()), "setup");
        Drive(missingGroup, groupOwner);
        Check(Read(missingGroup).result == ClockResult::NotInitialized && groupOwner.calls == std::vector<std::string>{"candidate", "group"} &&
                  groupOwner.effects.empty(),
              "ambiguous missing group provider refuses the selected group without falling back or acquiring a token");
        NativeMoveAnimWorkload wrongPhase;
        Owner                  phaseOwner;
        Require(wrongPhase.Reset(Request()), "setup");
        wrongPhase.Tick(phaseOwner, 1000);
        const auto phase = Read(wrongPhase);
        Check(phase.result == ClockResult::WrongThread && phaseOwner.calls.empty() && phaseOwner.effects.empty(),
              "general producer context cannot substitute for after-application authority");
        NativeMoveAnimWorkload wrongThread;
        Owner                  threadOwner;
        Require(wrongThread.Reset(Request()), "setup");
        std::thread foreign([&] { Tick(wrongThread, threadOwner, 1000); });
        foreign.join();
        Check(Read(wrongThread).result == ClockResult::WrongThread && threadOwner.calls.empty(),
              "foreign thread cannot acquire authority by setting a callback-phase flag");
        NativeMoveAnimWorkload handleDrift;
        Owner                  handleOwner;
        Require(handleDrift.Reset(Request()), "setup");
        Drive(handleDrift, handleOwner, 4);
        ++handleOwner.handle;
        Drive(handleDrift, handleOwner, 20, 3000);
        Check(Read(handleDrift).result == ClockResult::BindingChanged && handleOwner.effects == std::vector<std::string>{"acquire", "enable", "release"} &&
                  handleOwner.tokensMatch,
              "fresh handle drift refuses publication while releasing the original full token");
        NativeMoveAnimWorkload selectorDrift;
        Owner                  selectorOwner;
        Require(selectorDrift.Reset(Request()), "setup");
        Drive(selectorDrift, selectorOwner, 5);
        selectorOwner.selector = 1;
        Drive(selectorDrift, selectorOwner, 20, 3000);
        Check(Read(selectorDrift).result == ClockResult::BindingChanged &&
                  selectorOwner.effects == std::vector<std::string>{"acquire", "enable", "publish", "withdraw", "release"} && selectorOwner.tokensMatch,
              "fresh player selector drift prevents the native group write and retires the original publication");
        NativeMoveAnimWorkload observed;
        Owner                  observedOwner;
        Require(observed.Reset(Request(54, 8000)), "setup");
        Drive(observed, observedOwner, 7);
        observedOwner.group = 55;
        Tick(observed, observedOwner, 1800);
        Check(Read(observed).afterGroup == 55 && Read(observed).selectedGroup == 54 && Read(observed).phase == Phase::Observing,
              "observer reports the fresh native group rather than substituting selected-group cache");
        observed.RequestStop();
        Drive(observed, observedOwner, 4, 3000);
        NativeMoveAnimWorkload concurrent;
        Owner                  concurrentOwner;
        Require(concurrent.Reset(Request()), "setup");
        std::atomic<bool>     ready{false}, coherent{true};
        std::atomic<unsigned> reads{0};
        std::thread           reader(
            [&]
            {
                State first;
                if (concurrent.Read(first))
                    ++reads;
                else
                    coherent.store(false);
                // Establish one read before the writer starts. Later bounded
                // refusals remain valid even if the writer is preempted mid-publish.
                ready.store(true);
                for (unsigned index = 0; index != 10000; ++index)
                {
                    State value;
                    std::memset(&value, 0x35, sizeof(value));
                    std::array<unsigned char, sizeof(value)> unchanged{};
                    std::memcpy(unchanged.data(), &value, sizeof(value));
                    if (concurrent.Read(value))
                    {
                        ++reads;
                        if (value.size != sizeof(value) || value.version != 1 || !value.sequence || !value.physicalConfirmationRequired || value.fullMtaReady)
                            coherent.store(false);
                    }
                    else if (std::memcmp(&value, unchanged.data(), sizeof(value)))
                        coherent.store(false);
                }
            });
        while (!ready.load())
            std::this_thread::yield();
        Drive(concurrent, concurrentOwner);
        reader.join();
        Check(reads.load() && coherent.load(), "concurrent report reads are coherent or preserve output on bounded seqlock refusal");
    }
}
static_assert(!std::is_copy_constructible_v<NativeMoveAnimWorkload> && !std::is_move_constructible_v<NativeMoveAnimWorkload>);
static_assert(std::is_standard_layout_v<MoveAnimWorkloadRequestV1> && std::is_trivially_copyable_v<MoveAnimWorkloadStateV1>);
static_assert(sizeof(MoveAnimWorkloadRequestV1) == 32 && MoveAnimWorkloadVersionV1 == 1);
static_assert(std::is_same_v<decltype(&ReadGameMoveAnimWorkloadV1), Status (*)(std::uint32_t, IGameAdapter*, MoveAnimWorkloadStateV1*) noexcept>);

int main()
{
    try
    {
        Contract();
        Success();
        CompletionAdmission();
        FailurePrefixes();
        Stops();
        Ownership();
        std::printf("Actual move animation workload API fixture completed\n");
        std::printf("Actual move animation workload operation order fixture completed\n");
        std::printf("Actual move animation workload unknown retention fixture completed\n");
        std::printf("Actual move animation workload thread phase fixture completed\n");
        std::printf("Historical clock96 remains read-only and separate\n");
        std::printf("Passed %u player move animation workload checks (%zu-bit; no native engine code executed)\n", checks, sizeof(void*) * 8);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "Workload fixture failed after %u checks: %s\n", checks, error.what());
        return 1;
    }
}
