/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Exercise the production detach-before-GPU startup lifetime order
 *****************************************************************************/
#include "../../Client/core/DefinitiveGuiStartupReference.h"
#include <cstdio>
#include <stdexcept>
#include <vector>

using namespace DefinitiveGuiStartupReference;
namespace
{
    unsigned checks = 0;
    void     Check(bool value, const char* why)
    {
        ++checks;
        if (!value)
            throw std::runtime_error(why);
    }
    struct Clock
    {
        std::uint64_t now = 1000;
        std::uint64_t Now() const noexcept { return now; }
        void          Wait(std::uint32_t value) noexcept { now += value; }
    };
    struct Owner
    {
        Clock&        clock;
        bool          prepareOk = true, beginOk = true, readOk = true, startOk = true, pollOk = true, cancelled = false;
        bool          prepareThrows = false, beginThrows = false, lateFailure = false, cancelAtEnd = false, metadataOk = true, terminalPublishThrows = false;
        unsigned      readyAfter = 1, endAfter = 1, pollAfter = 1, shutdownAfter = 1, prepareDelay = 0, endThrows = 0, shutdownThrows = 0, publishThrows = 0;
        unsigned      prepared = 0, began = 0, read = 0, took = 0, ended = 0, started = 0, polled = 0, stopped = 0;
        unsigned      pollDelay = 0, cancelAtPoll = 0, pollThrowsAt = 0, retainedShutdownCalls = 0;
        std::uint64_t gpuStartedAt = 0, firstShutdownAt = 0;
        bool          firstShutdownOwned = false;
        bool          detached = false, device = false, retired = false;
        std::vector<Snapshot> reports;
        bool                  Prepare(Snapshot& value)
        {
            ++prepared;
            clock.now += prepareDelay;
            if (prepareThrows)
                throw std::runtime_error("prepare");
            value[GuiPrepared] = prepareOk;
            return prepareOk;
        }
        bool Begin(Snapshot&)
        {
            ++began;
            if (beginThrows)
                throw std::runtime_error("begin");
            return beginOk;
        }
        bool ReadNative(Snapshot& value)
        {
            ++read;
            value[BackendPinnedExpected] = 1;
            return readOk;
        }
        bool TryDevice(Snapshot& value)
        {
            ++took;
            device = took >= readyAfter;
            if (device)
            {
                value[LeaseGenerationLow] = 17;
                value[AcceptedSnapshotsLow] = 2;
                value[DispatchEntriesLow] = 3;
                value[SourceProfile] = 2;
                value[LeaseMetadataValid] = metadataOk;
                value[DeviceRetained] = 1;
            }
            return device;
        }
        bool End(Snapshot& value)
        {
            ++ended;
            if (endThrows)
            {
                --endThrows;
                throw std::runtime_error("end");
            }
            detached = ended >= endAfter;
            value[ObserverPendingCleanup] = detached ? 0u : 1u;
            if (detached && lateFailure)
                value[NativeFailureReason] = 10;
            if (detached && cancelAtEnd)
                cancelled = true;
            return detached;
        }
        bool StartGpu(Snapshot&)
        {
            Check(detached && device && !retired, "GPU startup follows complete detach with retained device");
            ++started;
            gpuStartedAt = clock.Now();
            return startOk;
        }
        Outcome PollGpu(Snapshot& value)
        {
            Check(detached && started && device && !retired, "GPU poll retains owner and transferred device");
            ++polled;
            // Advancing only the owned fake clock makes long frontend and
            // delayed-failure cases deterministic without wall-clock waiting.
            clock.now += pollDelay;
            if (cancelAtPoll && polled >= cancelAtPoll)
                cancelled = true;
            if (pollThrowsAt && polled == pollThrowsAt)
                throw std::runtime_error("late GPU poll");
            if (polled < pollAfter)
                return Outcome::Pending;
            value[WorkloadSucceeded] = pollOk;
            value[ReadbackVerified] = pollOk;
            return pollOk ? Outcome::Passed : Outcome::Failed;
        }
        bool ShutdownGpu(Snapshot& value)
        {
            Check(detached && !retired, "GUI/device cleanup cannot run while native peers may be held");
            ++stopped;
            if (stopped == 1)
            {
                firstShutdownAt = clock.Now();
                firstShutdownOwned = device && detached && !retired;
            }
            if ((shutdownThrows || stopped < shutdownAfter) && device && detached && !retired)
                ++retainedShutdownCalls;
            if (shutdownThrows)
            {
                --shutdownThrows;
                throw std::runtime_error("shutdown");
            }
            if (stopped < shutdownAfter)
                return false;
            device = false;
            retired = true;
            value[DeviceRetained] = 0;
            value[GuiCleanupComplete] = 1;
            return true;
        }
    };
    Owner* active = nullptr;
    bool   Cancelled()
    {
        return active->cancelled;
    }
    void Publish(const std::uint32_t* values, std::uint32_t count)
    {
        Check(count == FieldCount, "GUI protocol keeps its own fixed scalar count");
        if (active->terminalPublishThrows && values[Phase] == 2)
            throw std::runtime_error("terminal publish");
        if (active->publishThrows)
        {
            --active->publishThrows;
            throw std::runtime_error("publish");
        }
        Snapshot copy{};
        for (std::size_t i = 0; i < copy.size(); ++i)
            copy[i] = values[i];
        active->reports.push_back(copy);
    }
    struct Request
    {
        std::uint32_t durationMs = 500;
        bool (*cancelled)() = Cancelled;
        void (*publish)(const std::uint32_t*, std::uint32_t) = Publish;
    };
    Snapshot Execute(Owner& owner, std::uint32_t duration = 500, bool renewableFrontend = false)
    {
        active = &owner;
        const auto value = Run(owner, owner.clock, Request{duration}, renewableFrontend);
        Check(value[Version] == 1 && value[Phase] == 2 && value[CleanupComplete] && !value[CleanupPending] && !value[ObserverPendingCleanup],
              "final report follows actual native and GUI cleanup");
        Check(owner.detached && owner.retired && !owner.device && !value[DeviceRetained], "worker retains ownership until both drains complete");
        Check(!value[NativeQueueCommandsIssued] && !value[NativeBackbufferTouched], "own workload never claims native queue or backbuffer writes");
        Check(owner.reports.back() == value, "final publication is the returned coherent snapshot");
        for (const auto& report : owner.reports)
            Check(report[Sequence] > 0 && report[Version] == 1, "published values have the independent GUI version and sequence");
        return value;
    }
}

int main()
{
    try
    {
        {
            Clock clock;
            Owner owner{clock};
            owner.readyAfter = 3;
            owner.endAfter = 3;
            owner.pollAfter = 3;
            owner.shutdownAfter = 4;
            const auto value = Execute(owner);
            Check(value[Reason] == static_cast<unsigned>(StopReason::Completed) && value[WorkloadSucceeded] && value[ReadbackVerified],
                  "completed own workload succeeds");
            Check(owner.prepared == 1 && owner.began == 1 && owner.started == 1 && owner.polled == 3 && owner.stopped == 4,
                  "one-shot startup and cleanup retries preserve order");
            Check(value[LeaseGenerationLow] == 17 && value[SourceProfile] == 2 && value[BackendPinnedExpected] == 1,
                  "native correlation remains distinct from own GPU result");
        }
        for (unsigned scenario = 0; scenario < 14; ++scenario)
        {
            Clock      clock;
            Owner      owner{clock};
            StopReason expected = StopReason::Running;
            switch (scenario)
            {
                case 0:
                    owner.cancelled = true;
                    expected = StopReason::Cancelled;
                    break;
                case 1:
                    owner.prepareOk = false;
                    expected = StopReason::PrepareError;
                    break;
                case 2:
                    owner.beginOk = false;
                    owner.endAfter = 3;
                    expected = StopReason::BeginError;
                    break;
                case 3:
                    owner.readOk = false;
                    expected = StopReason::NativeError;
                    break;
                case 4:
                    owner.readyAfter = 1000;
                    expected = StopReason::DeviceWaitTimeout;
                    break;
                case 5:
                    owner.lateFailure = true;
                    expected = StopReason::NativeError;
                    break;
                case 6:
                    owner.cancelAtEnd = true;
                    expected = StopReason::Cancelled;
                    break;
                case 7:
                    owner.endAfter = 45;
                    expected = StopReason::CleanupError;
                    break;
                case 8:
                    owner.prepareThrows = true;
                    expected = StopReason::Exception;
                    break;
                case 9:
                    owner.beginThrows = true;
                    expected = StopReason::Exception;
                    break;
                case 10:
                    owner.endThrows = 1;
                    expected = StopReason::Exception;
                    break;
                case 11:
                    owner.startOk = false;
                    expected = StopReason::GpuError;
                    break;
                case 12:
                    owner.pollOk = false;
                    expected = StopReason::GpuError;
                    break;
                case 13:
                    owner.metadataOk = false;
                    expected = StopReason::LeaseError;
                    break;
            }
            const auto value = Execute(owner, 150);
            Check(value[Reason] == static_cast<unsigned>(expected), "refusal/cancellation/failure remains separate from safe cleanup");
            if (scenario < 11 || scenario == 13)
                Check(!owner.started && !owner.polled && !value[WorkloadSucceeded], "refused or late-failed native lease issues no GPU work");
            if (!scenario)
                Check(!owner.prepared && !owner.began, "preparation cancellation runs no loader or native Begin");
            if (scenario == 7)
                Check(value[CleanupDeadlineExceeded] && owner.ended == 45, "native cleanup deadline never abandons retained ownership");
        }
        {
            Clock clock;
            Owner owner{clock};
            owner.prepareDelay = 2500;
            const auto value = Execute(owner);
            Check(value[Reason] == static_cast<unsigned>(StopReason::Completed) && value[ElapsedMs] >= 2500,
                  "loader preparation time is not misclassified as a native pulse gap");
        }
        {
            Clock clock;
            Owner owner{clock};
            owner.shutdownAfter = 45;
            const auto value = Execute(owner);
            Check(value[CleanupDeadlineExceeded] && value[Reason] == static_cast<unsigned>(StopReason::CleanupError) && owner.stopped == 45,
                  "GPU cleanup deadline backs off but retains the worker until actual drain");
            Check(value[WorkloadSucceeded], "workload success and cleanup delay remain separate facts");
        }
        {
            Clock clock;
            Owner owner{clock};
            owner.shutdownThrows = 1;
            const auto value = Execute(owner);
            Check(value[Reason] == static_cast<unsigned>(StopReason::Exception) && owner.stopped == 2, "cleanup exception retries before ownership release");
        }
        {
            Clock clock;
            Owner owner{clock};
            owner.pollAfter = 1000;
            const auto value = Execute(owner, 150);
            Check(value[Reason] == static_cast<unsigned>(StopReason::GpuError) && !value[WorkloadSucceeded] && owner.started == 1,
                  "GPU wait timeout still completes actual cleanup");
        }
        for (bool gpu : {false, true})
        {
            Clock clock;
            Owner owner{clock};
            owner.publishThrows = 1;
            if (gpu)
                owner.pollAfter = 3;
            else
                owner.readyAfter = 3;
            const auto value = Execute(owner);
            Check(value[Reason] == static_cast<unsigned>(StopReason::Exception), "a failed publication cannot be overwritten by later workload success");
            Check(gpu ? owner.polled == 1 : owner.started == 0, "publication failure leaves the polling loop and drains ownership");
        }
        {
            Clock clock;
            Owner owner{clock};
            owner.terminalPublishThrows = true;
            active = &owner;
            const auto value = Run(owner, clock, Request{});
            Check(value[Phase] == 2 && value[CleanupComplete] && value[Reason] == static_cast<unsigned>(StopReason::Exception),
                  "terminal publisher exception is preserved in the returned result");
            Check(owner.retired && !owner.device, "terminal publication failure cannot undo completed ownership cleanup");
        }
        for (bool renewable : {false, true})
        {
            Clock clock;
            Owner owner{clock};
            owner.pollDelay = 500;
            owner.pollAfter = 241;
            const auto value = Execute(owner, 35000, renewable);
            if (renewable)
            {
                Check(value[Reason] == static_cast<unsigned>(StopReason::Completed) && value[WorkloadComplete] && value[WorkloadSucceeded],
                      "renewable frontend completes the same long workload instead of inheriting a diagnostic duration");
                Check(value[ElapsedMs] > 120000 && owner.polled == 241 && !value[CleanupDeadlineExceeded],
                      "renewable execution remains admitted beyond two minutes without weakening cleanup");
            }
            else
            {
                Check(value[Reason] == static_cast<unsigned>(StopReason::GpuError) && !value[WorkloadComplete] && !value[WorkloadSucceeded],
                      "ordinary workloads keep the historical absolute GPU timeout");
                Check(value[ElapsedMs] >= 35000 && value[ElapsedMs] < 36000 && owner.polled < owner.pollAfter,
                      "the original 35000ms diagnostic bound still stops identical pending GPU work");
            }
            Check(owner.firstShutdownOwned && owner.stopped == 1, "long or timed-out work retains its device until actual cleanup");
        }
        for (bool renewable : {false, true})
        {
            Clock clock;
            Owner owner{clock};
            owner.readyAfter = 1000;
            const auto value = Execute(owner, 35000, renewable);
            Check(value[Reason] == static_cast<unsigned>(StopReason::DeviceWaitTimeout) && value[ElapsedMs] == 35000,
                  "renewable opt-in never bypasses the existing device-acquisition deadline");
            Check(owner.took == 700 && !owner.started && !owner.polled && !value[DeviceAcquired],
                  "a timed-out native lease cannot start GPU work in either lifetime mode");
        }
        {
            Clock clock;
            Owner owner{clock};
            owner.endAfter = 45;
            const auto value = Execute(owner, 35000, true);
            Check(value[Reason] == static_cast<unsigned>(StopReason::CleanupError) && value[CleanupDeadlineExceeded] && owner.ended == 45,
                  "renewable frontend preserves native-observer cleanup deadlines and retries");
            Check(!owner.started && !owner.polled && owner.stopped == 1,
                  "native cleanup refusal still prevents renewable GPU startup before releasing the device");
        }
        {
            Clock clock;
            Owner owner{clock};
            owner.pollDelay = 500;
            owner.pollAfter = 1000;
            owner.cancelAtPoll = 82;
            owner.shutdownAfter = 3;
            const auto value = Execute(owner, 35000, true);
            Check(value[Reason] == static_cast<unsigned>(StopReason::Cancelled) && !value[WorkloadComplete] && !value[WorkloadSucceeded],
                  "explicit cancellation remains effective after the old diagnostic duration");
            Check(owner.polled == 82 && owner.firstShutdownAt - owner.gpuStartedAt >= 45000 && owner.firstShutdownOwned && owner.retainedShutdownCalls == 2 &&
                      owner.stopped == 3 && !value[CleanupDeadlineExceeded],
                  "late cancellation drains the same retained owner before release");
        }
        {
            Clock clock;
            Owner owner{clock};
            owner.pollDelay = 500;
            owner.pollAfter = 82;
            owner.pollOk = false;
            owner.shutdownAfter = 3;
            const auto value = Execute(owner, 35000, true);
            Check(value[Reason] == static_cast<unsigned>(StopReason::GpuError) && value[WorkloadComplete] && !value[WorkloadSucceeded],
                  "a late actual GPU failure cannot turn renewable lifetime into workload success");
            Check(owner.firstShutdownAt - owner.gpuStartedAt >= 45000 && owner.firstShutdownOwned && owner.retainedShutdownCalls == 2 && owner.stopped == 3 &&
                      !value[CleanupDeadlineExceeded],
                  "late GPU failure preserves the existing retained-device cleanup order");
        }
        {
            Clock clock;
            Owner owner{clock};
            owner.pollDelay = 500;
            owner.pollAfter = 1000;
            owner.pollThrowsAt = 82;
            owner.shutdownThrows = 1;
            owner.shutdownAfter = 45;
            const auto value = Execute(owner, 35000, true);
            Check(value[Reason] == static_cast<unsigned>(StopReason::CleanupError) && value[CleanupDeadlineExceeded] && !value[WorkloadComplete] &&
                      !value[WorkloadSucceeded],
                  "a late exception and cleanup deadline remain failures in renewable mode");
            Check(owner.firstShutdownAt - owner.gpuStartedAt >= 45000 && owner.firstShutdownOwned && owner.retainedShutdownCalls == 44 && owner.stopped == 45,
                  "exceptions and deadline backoff never abandon the long-running retained owner");
            unsigned retainedReports = 0;
            for (const auto& report : owner.reports)
            {
                if (report[Phase] != 3)
                    continue;
                ++retainedReports;
                Check(report[ElapsedMs] > 35000 && report[CleanupPending] && !report[CleanupComplete] && report[DeviceRetained],
                      "every late cleanup report still owns the unreleased device");
            }
            Check(retainedReports == 44, "all late cleanup retries are observable before final proven retirement");
        }
        std::printf("Passed %u Core GUI startup lifecycle checks\n", checks);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "Failed after %u checks: %s\n", checks, error.what());
        return 1;
    }
}
