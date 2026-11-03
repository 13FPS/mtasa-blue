/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Never replay a movement effect after a refused or unknown prefix
 ****************************************************************************/
#include "NativeMoveAnimWorkload.h"
#include <cstring>

namespace mta::game_de
{
    bool NativeMoveAnimWorkload::ValidRequest(const MoveAnimWorkloadRequestV1& value) noexcept
    {
        if (value.size != sizeof(value) || value.version != MoveAnimWorkloadVersionV1 || !value.durationMs ||
            value.durationMs > MoveAnimWorkloadMaximumDurationMsV1 || value.preferredGroup < 54 || value.preferredGroup > 56)
            return false;
        for (const auto reserved : value.reserved)
            if (reserved)
                return false;
        return true;
    }
    bool NativeMoveAnimWorkload::Reset(const MoveAnimWorkloadRequestV1& value) noexcept
    {
        if (!ValidRequest(value) || step != Step::Finished || state.cleanupPending)
            return false;
        request = value;
        state = MoveAnimWorkloadStateV1{};
        state.phase = MoveAnimWorkloadPhaseV1::Preflight;
        state.result = ClockResult::Ok;
        state.selectedGroup = value.preferredGroup;
        state.cleanupPending = 1;
        token = NativePedReferenceToken{};
        step = Step::Candidate;
        observedSince = 0;
        stopRequested.store(false);
        Publish();
        return true;
    }
    void NativeMoveAnimWorkload::Publish() noexcept
    {
        ++state.sequence;
        publication.fetch_add(1);
        std::array<std::uint32_t, sizeof(state) / sizeof(std::uint32_t)> next{};
        static_assert(sizeof(state) % sizeof(std::uint32_t) == 0);
        std::memcpy(next.data(), &state, sizeof(state));
        for (std::size_t index = 0; index != next.size(); ++index)
            words[index].store(next[index]);
        publication.fetch_add(1);
    }
    bool NativeMoveAnimWorkload::Read(MoveAnimWorkloadStateV1& output) const noexcept
    {
        for (unsigned attempt = 0; attempt != 8; ++attempt)
        {
            const auto before = publication.load();
            if (!before || (before & 1))
                continue;
            std::array<std::uint32_t, sizeof(state) / sizeof(std::uint32_t)> next{};
            for (std::size_t index = 0; index != next.size(); ++index)
                next[index] = words[index].load();
            if (publication.load() != before)
                continue;
            MoveAnimWorkloadStateV1 value;
            std::memcpy(&value, next.data(), sizeof(value));
            output = value;
            return true;
        }
        return false;
    }
    void NativeMoveAnimWorkload::RequestStop() noexcept
    {
        stopRequested.store(true);
    }
    void NativeMoveAnimWorkload::Fail(ClockResult result, bool uncertain) noexcept
    {
        // Keep the first failed prefix. Cleanup acknowledgements cannot turn
        // an unknown write/reapply into a completed workload or invite replay.
        if (state.result == ClockResult::Ok)
            state.result = result;
        if (uncertain || state.phase == MoveAnimWorkloadPhaseV1::Unknown)
            state.phase = MoveAnimWorkloadPhaseV1::Unknown;
        else
            state.phase = MoveAnimWorkloadPhaseV1::Refused;
        step = state.overridePublished ? Step::Withdraw : state.tokenRegistered ? Step::Release : Step::Finished;
    }
    bool NativeMoveAnimWorkload::SameCurrentPlayer(NativeGameTimingAccess& access) noexcept
    {
        std::uint32_t handle = 0, selector = 0;
        std::int32_t  group = 0;
        const auto    result = access.ReadCurrentPlayerMoveAnimCandidate(handle, selector, group);
        if (result != ClockResult::Ok || handle != state.handle || selector != state.selector)
        {
            Fail(result == ClockResult::Ok ? ClockResult::BindingChanged : result);
            return false;
        }
        state.afterGroup = group;
        return true;
    }
    void NativeMoveAnimWorkload::Tick(NativeGameTimingAccess& access, std::uint64_t nowMs) noexcept
    {
        if (step == Step::Finished)
            return;
        if (stopRequested.load() && step != Step::Withdraw && step != Step::Release)
            Fail(ClockResult::Stopped);
        ClockResult result = ClockResult::Ok;
        switch (step)
        {
            case Step::Candidate:
                result = access.ReadCurrentPlayerMoveAnimCandidate(state.handle, state.selector, state.beforeGroup);
                if (result == ClockResult::Ok)
                {
                    state.afterGroup = state.beforeGroup;
                    step = Step::Group;
                }
                else
                    Fail(result);
                break;
            case Step::Group:
                // A refused observation is not a missing-group boolean. The
                // explicitly requested already-loaded group either qualifies
                // or ends this attempt; this path never loads another block.
                result = access.CheckPlayerMoveAnimWorkloadGroup(state.selectedGroup);
                if (result == ClockResult::Ok)
                    step = Step::Acquire;
                else
                    Fail(result);
                break;
            case Step::Acquire:
                if (!SameCurrentPlayer(access))
                    break;
                result = access.AcquirePedReference(state.handle, token);
                if (result == ClockResult::Ok)
                {
                    state.tokenRegistered = 1;
                    state.phase = MoveAnimWorkloadPhaseV1::Installing;
                    step = Step::Enable;
                }
                else
                    Fail(result);
                break;
            case Step::Enable:
                if (!SameCurrentPlayer(access))
                    break;
                result = access.EnablePlayerMoveAnimHooks();
                if (result == ClockResult::Ok)
                {
                    state.hooksEnabled = 1;
                    step = Step::Publish;
                }
                else
                    Fail(result);
                break;
            case Step::Publish:
                if (!SameCurrentPlayer(access))
                    break;
                result = access.PublishPlayerMoveAnimOverride(token, state.selectedGroup);
                if (result == ClockResult::Ok)
                {
                    state.overridePublished = 1;
                    state.phase = MoveAnimWorkloadPhaseV1::Published;
                    step = Step::Write;
                }
                else
                    Fail(result);
                break;
            case Step::Write:
                if (!SameCurrentPlayer(access))
                    break;
                result = access.WriteCurrentPlayerMoveAnimGroup(token, state.selectedGroup);
                if (result == ClockResult::Ok)
                {
                    state.writeAcknowledged = 1;
                    state.phase = MoveAnimWorkloadPhaseV1::Written;
                    step = Step::Reapply;
                }
                else
                    Fail(result, true);
                break;
            case Step::Reapply:
                if (!SameCurrentPlayer(access))
                    break;
                result = access.ReapplyCurrentPlayerMoveAnim(token);
                if (result == ClockResult::Ok)
                {
                    state.reapplyAcknowledged = 1;
                    state.phase = MoveAnimWorkloadPhaseV1::Reapplied;
                    observedSince = nowMs;
                    step = Step::Observe;
                }
                else
                    Fail(result, true);
                break;
            case Step::Observe:
                if (!SameCurrentPlayer(access))
                    break;
                state.phase = MoveAnimWorkloadPhaseV1::Observing;
                if (nowMs < observedSince)
                    Fail(ClockResult::BindingChanged);
                else if (nowMs - observedSince >= request.durationMs)
                    step = Step::Withdraw;
                break;
            case Step::Withdraw:
            {
                state.withdrawAttempted = 1;
                const auto previousPhase = state.phase;
                state.phase = MoveAnimWorkloadPhaseV1::Withdrawing;
                result = access.WithdrawPlayerMoveAnimOverride(token);
                if (result == ClockResult::Ok)
                {
                    state.overridePublished = 0;
                    state.withdrawAcknowledged = 1;
                    state.phase = previousPhase;
                    step = Step::Release;
                }
                else
                {
                    Fail(result, true);
                    // Native Stop/End retains and drains the actual owner;
                    // never loop a failed cleanup call on another callback.
                    step = Step::Finished;
                }
                break;
            }
            case Step::Release:
                state.releaseAttempted = 1;
                result = access.ReleasePedReference(token);
                if (result == ClockResult::Ok)
                {
                    state.tokenRegistered = 0;
                    state.releaseAcknowledged = 1;
                    token = NativePedReferenceToken{};
                    if (state.result == ClockResult::Ok)
                        state.phase = MoveAnimWorkloadPhaseV1::Completed;
                    step = Step::Finished;
                }
                else
                {
                    Fail(result, true);
                    step = Step::Finished;
                }
                break;
            case Step::Finished:
                break;
        }
        Publish();
    }
    void NativeMoveAnimWorkload::NotEntered(ClockResult result) noexcept
    {
        // Begin returned no generation and retained no callback/patch. Keep
        // that fact separate from a native End acknowledgement or old receipt.
        state.owner = ClockServiceStateV1{};
        state.notEntered = 1;
        state.result = result;
        state.phase = MoveAnimWorkloadPhaseV1::Refused;
        state.cleanupPending = 0;
        step = Step::Finished;
        Publish();
    }
    void NativeMoveAnimWorkload::Retired(const ClockServiceStateV1& owner) noexcept
    {
        // Called only after the existing native End acknowledgement. Its real
        // bulk retirement can discharge a token when callbacks stopped before
        // our explicit cleanup; report that distinct route instead of forging
        // a successful Withdraw/Release receipt. Known scalar intent stays put.
        state.owner = owner;
        if (state.result == ClockResult::Ok && (!state.writeAcknowledged || !state.reapplyAcknowledged))
            state.result = owner.failure == ClockResult::Ok ? ClockResult::Stopped : owner.failure;
        state.cleanupViaOwnerEnd = state.overridePublished || state.tokenRegistered;
        state.overridePublished = 0;
        state.tokenRegistered = 0;
        state.endAcknowledged = 1;
        state.cleanupPending = 0;
        state.phase = MoveAnimWorkloadPhaseV1::Retired;
        token = NativePedReferenceToken{};
        step = Step::Finished;
        Publish();
    }
}
