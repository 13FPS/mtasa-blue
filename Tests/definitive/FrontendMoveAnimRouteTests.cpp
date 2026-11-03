/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual Core route policy with explicit adapter-call doubles
 ****************************************************************************/
#include <SharedUtil.Defines.h>
#include "../../Client/core/DefinitiveMoveAnimWorkload.h"
#include "../../Client/core/DefinitiveFrontendClockReference.h"
#include <cstdio>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

namespace
{
    namespace Core = DefinitiveMoveAnimWorkload;
    using namespace mta::game_de;
    using NativePhase = MoveAnimWorkloadPhaseV1;
    unsigned checks = 0;
    void     Check(bool value, const char* reason)
    {
        ++checks;
        if (!value)
            throw std::runtime_error(reason);
    }
    struct Owner
    {
        bool                     available = true, pending = false, pendingAfterBegin = true, pendingAfterEnd = false;
        bool                     hidden = true, inputRetired = true, beginSafe = true, acknowledge = true, noEntry = false;
        bool                     poisonRead = false, poisonEnd = false;
        Status                   beginStatus = Status::Ok, readStatus = Status::Ok, renewStatus = Status::Ok, endStatus = Status::Ok;
        std::string              throws;
        std::vector<std::string> calls;
        MoveAnimWorkloadStateV1  state;
        std::uint64_t            renewed = 0, ended = 0;
        Owner()
        {
            state.owner.generation = UINT64_C(0x1234567887654321);
            state.owner.active = state.owner.hooked = state.owner.modulePinned = 1;
            state.cleanupPending = 1;
            state.phase = NativePhase::Observing;
        }
        bool HasMoveAnimWorkloadService() const noexcept { return available; }
        bool MoveAnimWorkloadPending() const noexcept { return pending; }
        void Call(const char* name)
        {
            calls.emplace_back(name);
            if (throws == name)
                throw std::runtime_error(name);
        }
        Status BeginMoveAnimWorkload(const MoveAnimWorkloadRequestV1& request, MoveAnimWorkloadStateV1& output)
        {
            beginSafe = beginSafe && hidden && inputRetired && request.preferredGroup == 54 && request.durationMs == 8000;
            pending = pendingAfterBegin;
            Call("begin");
            output = state;
            return beginStatus;
        }
        Status ReadMoveAnimWorkload(MoveAnimWorkloadStateV1& output)
        {
            Call("read");
            if (readStatus == Status::Ok)
                output = state;
            else if (poisonRead)
                output.owner.generation = 999;
            return readStatus;
        }
        Status RenewMoveAnimWorkload(std::uint64_t generation)
        {
            renewed = generation;
            Call("renew");
            return renewStatus;
        }
        Status EndMoveAnimWorkload(std::uint64_t generation, MoveAnimWorkloadStateV1& output)
        {
            ended = generation;
            Call("end");
            if (endStatus == Status::Ok)
            {
                pending = pendingAfterEnd;
                output = state;
                // Native GetModuleHandleEx(PIN) is process lifetime. End retires
                // active/hooked ownership without inventing an unloadable module.
                output.owner.active = output.owner.hooked = 0;
                output.cleanupPending = 0;
                output.endAcknowledged = noEntry ? 0u : 1u;
                output.notEntered = noEntry ? 1u : 0u;
                if (!acknowledge)
                    output.cleanupPending = 1;
            }
            else if (poisonEnd)
                output.owner.generation = 998;
            return endStatus;
        }
        void Pulse(Core::Route& route, bool hiddenValue = true, bool retiredValue = true)
        {
            hidden = hiddenValue;
            inputRetired = retiredValue;
            route.Pulse(*this, hiddenValue, retiredValue);
        }
        unsigned Count(const std::string& name) const
        {
            unsigned count = 0;
            for (const auto& value : calls)
                count += value == name ? 1u : 0u;
            return count;
        }
    };
    void QueueAndAdmission()  // 12
    {
        Core::Route route;
        Owner       owner;
        const auto  initial = route.Read();
        Check(initial.size == sizeof(initial) && initial.version == 1 && initial.phase == Core::Phase::Idle && !route.BlocksAdmission(),
              "route defaults contain no queued command or native attempt");
        owner.Pulse(route);
        Check(owner.calls.empty() && !route.BlocksAdmission(), "readiness polling never starts native movement");
        Check(route.Queue(), "only an explicit command queues the physical trial");
        Check(route.BlocksAdmission() && route.Read().queued == 1 && owner.calls.empty(), "queue closes frontend borrowing without invoking the adapter");
        Check(!route.Queue(), "duplicate command cannot replace a queued request");
        owner.Pulse(route, false, false);
        Check(owner.calls.empty() && route.Read().phase == Core::Phase::Queued, "visible view and active input defer Begin");
        owner.Pulse(route, true, false);
        Check(owner.calls.empty() && route.Read().queued, "hidden view alone does not retire the input borrower");
        owner.Pulse(route, false, true);
        Check(owner.calls.empty() && route.Read().queued, "retired input alone does not hide the view");
        (void)route.Read();
        (void)route.Read();
        Check(owner.calls.empty(), "cached reporting cannot call native workload exports");
        owner.Pulse(route);
        Check(owner.calls == std::vector<std::string>{"begin", "read", "renew"} && owner.beginSafe,
              "first safe pulse admits exactly one Begin then reads and renews that owner");
        Check(route.Read().phase == Core::Phase::Running && route.Read().attemptPending && !route.Read().queued && route.BlocksAdmission(),
              "running workload retains admission ownership after queue consumption");
        Check(owner.renewed == owner.state.owner.generation && !route.Queue(), "renew and duplicate refusal preserve the exact retained generation");
    }
    void AvailabilityAndCancellation()  // 10
    {
        Core::Route absent;
        Owner       legacy;
        legacy.available = false;
        Check(absent.Queue(), "unavailable legacy attempt is still explicitly requested");
        legacy.Pulse(absent);
        Check(absent.Read().phase == Core::Phase::Unavailable && absent.Read().lastStatus == Status::Unsupported && !absent.BlocksAdmission() &&
                  legacy.calls.empty(),
              "missing optional legacy exports refuse without a fabricated Begin");
        Core::Route beforeBegin;
        Owner       owner;
        Check(beforeBegin.Queue(), "queued cancellation has an explicit request");
        beforeBegin.Cancel();
        owner.Pulse(beforeBegin);
        Check(!beforeBegin.BlocksAdmission() && beforeBegin.Read().cancelRequested && owner.calls.empty(),
              "cancelled queued command never begins when a later pulse becomes eligible");
        Check(beforeBegin.Queue(), "a new explicit command may follow a completed cancellation");
        owner.Pulse(beforeBegin);
        beforeBegin.Cancel();
        owner.Pulse(beforeBegin);
        Check(owner.Count("begin") == 1 && owner.Count("end") == 1 && beforeBegin.Read().phase == Core::Phase::Finished &&
                  beforeBegin.Read().native.owner.modulePinned == 1 && beforeBegin.Read().native.endAcknowledged == 1 &&
                  !beforeBegin.Read().native.cleanupPending,
              "running cancellation drains the existing owner without another Begin");
        Core::Route stopped;
        Owner       stopOwner;
        Check(stopped.Queue(), "global stop has a queued request to cancel");
        Check(stopped.Stop(stopOwner) && !stopped.BlocksAdmission() && stopOwner.calls.empty(), "stop before Begin cancels CPU work without a native End");
        Check(!stopped.Queue(), "global stop never rearms from another console command");
        stopOwner.Pulse(stopped);
        Check(stopOwner.calls.empty() && stopped.Read().stopRequested, "later pulses cannot reactivate a stopped route");
    }
    void EndAuthority()  // 12
    {
        Core::Route route;
        Owner       owner;
        owner.endStatus = Status::Unsupported;
        Check(route.Queue(), "retention case queues explicitly");
        owner.Pulse(route);
        owner.state.phase = NativePhase::Unknown;
        owner.state.result = ClockResult::OriginalException;
        owner.Pulse(route);
        Check(route.Read().phase == Core::Phase::Retiring && route.BlocksAdmission() && route.Read().attemptPending,
              "unknown native effects retain the actual adapter attempt");
        Check(route.Read().native.result == ClockResult::OriginalException && owner.Count("begin") == 1,
              "cleanup cannot erase first unknown result or submit another Begin");
        Check(!route.Stop(owner) && route.BlocksAdmission(), "failed owner End blocks Core shutdown");
        owner.endStatus = Status::Ok;
        owner.pendingAfterEnd = true;
        Check(!route.Stop(owner) && route.BlocksAdmission(), "generic End Ok does not override the adapter's retained pending bit");
        owner.pendingAfterEnd = false;
        owner.acknowledge = false;
        Check(!route.Stop(owner) && route.BlocksAdmission(), "generic End Ok with a nonretired typed snapshot is insufficient");
        owner.acknowledge = true;
        Check(route.Stop(owner), "only exact acknowledged retirement releases the retained Core route");
        const auto retired = route.Read();
        Check(retired.phase == Core::Phase::Finished && !retired.attemptPending && !route.BlocksAdmission() && retired.native.endAcknowledged &&
                  !retired.native.cleanupPending,
              "finished route contains the native End acknowledgement rather than a guessed local timeout");
        Check(retired.native.result == ClockResult::OriginalException && retired.native.physicalConfirmationRequired && !retired.native.fullMtaReady,
              "route completion never turns unknown native evidence into physical or full-MTA success");
        Check(owner.ended == owner.state.owner.generation && owner.Count("begin") == 1, "every End retry belongs to the original full generation");
        const auto count = owner.calls.size();
        owner.Pulse(route);
        owner.Pulse(route);
        Check(owner.calls.size() == count, "retired stopped route makes no further workload calls");
        Check(!route.Queue(), "a stopped owner cannot rearm after cleanup eventually completes");
    }
    void RefusedReports()  // 10
    {
        Core::Route route;
        Owner       owner;
        owner.readStatus = Status::Unsupported;
        owner.poisonRead = owner.poisonEnd = true;
        owner.endStatus = Status::Unsupported;
        Check(route.Queue(), "poisoned-output case queues explicitly");
        owner.Pulse(route);
        Check(route.Read().native.owner.generation == owner.state.owner.generation && owner.ended == owner.state.owner.generation,
              "failed Read and End cannot poison the generation of the retained cached attempt");
        Check(route.BlocksAdmission() && route.Read().phase == Core::Phase::Retiring && owner.Count("renew") == 0,
              "failed read goes directly to retirement instead of renewal");
        Check(!route.Stop(owner) && owner.Count("begin") == 1, "failed-read retirement retries only End authority");
        Core::Route renewal;
        Owner       renewalOwner;
        renewalOwner.renewStatus = Status::Unsupported;
        renewalOwner.endStatus = Status::Unsupported;
        Check(renewal.Queue(), "renewal refusal has an explicit request");
        renewalOwner.Pulse(renewal);
        Check(renewal.Read().phase == Core::Phase::Retiring && renewal.BlocksAdmission() && renewalOwner.Count("renew") == 1 && renewalOwner.Count("end") == 1,
              "renewal refusal immediately retains and drains the same owner");
        Core::Route rejected;
        Owner       rejectedOwner;
        rejectedOwner.beginStatus = Status::Unsupported;
        rejectedOwner.pendingAfterBegin = false;
        Check(rejected.Queue(), "pre-entry refusal still requires an explicit request");
        rejectedOwner.Pulse(rejected);
        Check(rejected.Read().phase == Core::Phase::Finished && !rejected.BlocksAdmission() && rejectedOwner.calls == std::vector<std::string>{"begin"},
              "a proven absent backend attempt does not invent Read, Renew or End work");
        Check(!rejected.Read().native.endAcknowledged && !rejected.Read().native.writeAcknowledged,
              "pre-entry refusal cannot fabricate native acknowledgements");
        Check(rejected.Read().beginAttempted && rejected.Read().beginStatus == Status::Unsupported,
              "report distinguishes an attempted refused Begin from an unavailable optional API");
    }
    void TerminalReports()  // 12
    {
        for (const auto phase : {NativePhase::Completed, NativePhase::Refused, NativePhase::Unknown, NativePhase::Retired})
        {
            Core::Route route;
            Owner       owner;
            owner.state.phase = phase;
            owner.endStatus = Status::Unsupported;
            Check(route.Queue(), "terminal native sample is observed only for an explicit request");
            owner.Pulse(route);
            Check(route.Read().phase == Core::Phase::Retiring && route.BlocksAdmission() && owner.Count("renew") == 0 && owner.Count("end") == 1,
                  "every native terminal phase requires actual End rather than a successful renewal");
            Check(owner.Count("begin") == 1 && route.Read().native.physicalConfirmationRequired && !route.Read().native.fullMtaReady,
                  "terminal values neither replay Begin nor prove physical appearance or a complete product");
        }
    }
    void Exceptions()  // 16
    {
        for (const std::string phase : {"begin", "read", "renew", "end"})
        {
            Core::Route route;
            Owner       owner;
            owner.throws = phase;
            if (phase == "end")
                owner.state.phase = NativePhase::Completed;
            Check(route.Queue(), "throwing provider case queues explicitly");
            owner.Pulse(route);
            Check(route.Read().exceptionObserved && route.Read().attemptPending && route.BlocksAdmission() && route.Read().phase == Core::Phase::Retiring,
                  "exceptions retain provider-reported attempts even when no generation reached the route");
            Check(owner.Count("begin") == 1, "throwing calls never cause implicit Begin replay");
            owner.throws.clear();
            Check(route.Stop(owner) && !route.BlocksAdmission(), "subsequent explicit stop recovers the same owner report and authoritative End");
        }
    }
    void NoEntryGeneration()  // 6
    {
        Core::Route route;
        Owner       owner;
        owner.noEntry = true;
        owner.state.owner = {};
        owner.state.notEntered = 1;
        owner.state.cleanupPending = 0;
        owner.state.phase = NativePhase::Refused;
        owner.beginStatus = Status::Unsupported;
        Check(route.Queue(), "generation-zero no-entry route starts explicitly");
        owner.Pulse(route);
        Check(owner.ended == 0 && route.Read().phase == Core::Phase::Finished && !route.BlocksAdmission(),
              "generation zero retires only this recorded no-entry attempt");
        Check(route.Read().native.notEntered == 1 && !route.Read().native.endAcknowledged && !route.Read().native.cleanupPending,
              "no-entry retirement cannot forge a native End acknowledgement");
        Core::Route malformed;
        Owner       wrong;
        wrong.state.owner = {};
        wrong.state.phase = NativePhase::Refused;
        wrong.beginStatus = Status::Unsupported;
        Check(malformed.Queue(), "malformed zero-generation route still begins explicitly");
        wrong.Pulse(malformed);
        Check(malformed.BlocksAdmission() && malformed.Read().phase == Core::Phase::Retiring,
              "zero generation is not a wildcard for an unrelated End acknowledgement");
        wrong.noEntry = true;
        Check(malformed.Stop(wrong) && malformed.Read().native.notEntered && !malformed.Read().native.endAcknowledged,
              "only corrected explicit no-entry acknowledgement permits retirement");
    }
    void VisibilityLoss()  // 8
    {
        for (const bool hidden : {false, true})
        {
            Core::Route route;
            Owner       owner;
            Check(route.Queue(), "admission-loss case starts explicitly");
            owner.Pulse(route);
            owner.Pulse(route, hidden, !hidden);
            Check(owner.Count("begin") == 1 && owner.Count("end") == 1 && route.Read().phase == Core::Phase::Finished,
                  "loss of hidden view or retired input drains an existing workload instead of acquiring another owner");
            Check(!route.BlocksAdmission() && route.Read().native.endAcknowledged && !owner.pending, "admission can reopen only after provider retirement");
            const auto count = owner.calls.size();
            owner.Pulse(route);
            Check(owner.calls.size() == count, "returning to an eligible view cannot rearm a consumed command");
        }
    }
}
static_assert(Core::ApiVersion == 1 && std::is_standard_layout_v<Core::ReportV1> && std::is_trivially_copyable_v<Core::ReportV1>);
static_assert(DefinitiveFrontendClockReference::FieldCount == 96 && MoveAnimWorkloadVersionV1 == 1);

int main()
{
    try
    {
        QueueAndAdmission();
        AvailabilityAndCancellation();
        EndAuthority();
        RefusedReports();
        TerminalReports();
        Exceptions();
        NoEntryGeneration();
        VisibilityLoss();
        std::printf("Actual retained Core movement route fixture completed\n");
        std::printf("Actual retained Core movement route admission fixture completed\n");
        std::printf("Actual retained Core movement route End authority fixture completed\n");
        std::printf("Passed %u frontend movement route checks (%zu-bit; explicit adapter doubles)\n", checks, sizeof(void*) * 8);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "Route fixture failed after %u checks: %s\n", checks, error.what());
        return 1;
    }
}
