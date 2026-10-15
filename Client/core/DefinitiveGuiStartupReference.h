/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Separate value-only protocol for Core-owned D3D12 GUI startup
 *****************************************************************************/
#pragma once

#include <array>
#include <algorithm>
#include <cstdint>

namespace DefinitiveGuiStartupReference
{
    // This protocol describes an MTA-owned offscreen workload. API6 remains
    // read-only and never acquires persistent device ownership implicitly.
    inline constexpr std::uint32_t ApiVersion = 1;
    inline constexpr std::uint32_t FieldCount = 64;
    inline constexpr std::uint32_t MaximumDurationMs = 10000;
    inline constexpr std::uint32_t SampleIntervalMs = 50;
    inline constexpr std::uint32_t CleanupDeadlineMs = 2000;
    inline constexpr std::uint32_t CleanupBackoffMs = 250;

    enum Field : std::uint32_t
    {
        Version,
        Phase,
        Sequence,
        ElapsedMs,
        Reason,
        CleanupPending,
        CleanupComplete,
        CleanupDeadlineExceeded,
        ObserverStarted,
        ObserverEnded,
        DeviceAcquired,
        GuiPrepared,
        LeaseGenerationLow,
        LeaseGenerationHigh,
        AcceptedSnapshotsLow,
        AcceptedSnapshotsHigh,
        DispatchEntriesLow,
        DispatchEntriesHigh,
        SourceProfile,
        GuiStarted,
        WorkloadComplete,
        WorkloadSucceeded,
        WorkloadHresult,
        WorkloadGuiStatus,
        NodeCount,
        AdapterLuidLow,
        AdapterLuidHigh,
        TargetFormat,
        Width,
        Height,
        QueueCreated,
        TargetCreated,
        RenderSubmitted,
        FenceSignaledLow,
        FenceSignaledHigh,
        FenceCompletedLow,
        FenceCompletedHigh,
        ReadbackVerified,
        ReadbackChangedPixels,
        GuiCleanupComplete,
        CleanupAttempts,
        GuiShutdownStatus,
        NativeFailureReason,
        ObserverPendingCleanup,
        NativeQueueCommandsIssued,
        NativeBackbufferTouched,
        OwnQueueSubmissions,
        GuiStage,
        GuiFailureStage,
        ModuleRetained,
        SessionRetained,
        DeviceRetained,
        DeviceRemoved,
        BackgroundVerified,
        TextureVerified,
        OpaqueAlphaVerified,
        WidgetsCreated,
        BackendPinnedExpected,
        LeaseMetadataValid,
        DeviceReleaseFault,
        // Remaining entries stay zero. Queue and target facts above refer only
        // to MTA's allocations and never grant native frame admission.
    };

    enum class StopReason : std::uint32_t
    {
        Running,
        Completed,
        Cancelled,
        PrepareError,
        BeginError,
        NativeError,
        DeviceWaitTimeout,
        GpuError,
        PulseGap,
        Exception,
        CleanupError,
        LeaseError,
    };

    using Snapshot = std::array<std::uint32_t, FieldCount>;

    enum class Outcome
    {
        Pending,
        Passed,
        Failed,
    };

    inline void SetPair(Snapshot& values, Field field, std::uint64_t value) noexcept
    {
        values[field] = static_cast<std::uint32_t>(value);
        values[field + 1] = static_cast<std::uint32_t>(value >> 32);
    }

    // Production and owned tests execute the same lifetime order. Preparing
    // the module precedes hook installation; all GPU/GUI work and COM release
    // follow successful End, even when Begin failed while retaining peers.
    template <typename Owner, typename Clock, typename Request>
    Snapshot Run(Owner& owner, Clock& clock, const Request& request) noexcept
    {
        static_assert(noexcept(clock.Now()) && noexcept(clock.Wait(std::uint32_t{})));
        Snapshot snapshot{};
        snapshot[Version] = ApiVersion;
        auto       reason = StopReason::Running;
        const auto started = clock.Now();
        auto       lastPulse = started;
        const auto elapsed = [&]() noexcept
        {
            const auto now = clock.Now();
            snapshot[ElapsedMs] = now >= started ? static_cast<std::uint32_t>((std::min)(now - started, std::uint64_t{0xffffffffu})) : 0;
            return now;
        };
        const auto publish = [&]() noexcept
        {
            elapsed();
            snapshot[Reason] = static_cast<std::uint32_t>(reason);
            ++snapshot[Sequence];
            try
            {
                request.publish(snapshot.data(), FieldCount);
            }
            catch (...)
            {
                reason = StopReason::Exception;
                snapshot[Reason] = static_cast<std::uint32_t>(reason);
            }
        };
        try
        {
            if (request.cancelled())
                reason = StopReason::Cancelled;
            else if (!owner.Prepare(snapshot))
                reason = StopReason::PrepareError;
            else if (!owner.Begin(snapshot))
                reason = StopReason::BeginError;
            else
            {
                snapshot[ObserverStarted] = 1;
                const auto deviceStarted = clock.Now();
                lastPulse = deviceStarted;
                for (;;)
                {
                    const auto now = elapsed();
                    if (request.cancelled())
                    {
                        reason = StopReason::Cancelled;
                        break;
                    }
                    if (now < started || now < lastPulse || now - lastPulse > 1000)
                    {
                        reason = StopReason::PulseGap;
                        break;
                    }
                    if (now < deviceStarted || now - deviceStarted >= request.durationMs)
                    {
                        reason = StopReason::DeviceWaitTimeout;
                        break;
                    }
                    lastPulse = now;
                    if (!owner.ReadNative(snapshot) || snapshot[NativeFailureReason])
                    {
                        reason = StopReason::NativeError;
                        break;
                    }
                    if (owner.TryDevice(snapshot))
                    {
                        snapshot[DeviceAcquired] = 1;
                        break;
                    }
                    snapshot[Phase] = 1;
                    publish();
                    if (reason != StopReason::Running)
                        break;
                    clock.Wait(SampleIntervalMs);
                }
            }
        }
        catch (...)
        {
            reason = StopReason::Exception;
        }

        auto cleanupStarted = clock.Now();
        snapshot[CleanupPending] = 1;
        for (;;)
        {
            if (snapshot[CleanupAttempts] != 0xffffffffu)
                ++snapshot[CleanupAttempts];
            bool ended = false;
            try
            {
                ended = owner.End(snapshot);
            }
            catch (...)
            {
                reason = StopReason::Exception;
            }
            const auto now = elapsed();
            if (now < cleanupStarted || now - cleanupStarted >= CleanupDeadlineMs)
            {
                snapshot[CleanupDeadlineExceeded] = 1;
                reason = StopReason::CleanupError;
            }
            if (ended)
                break;
            snapshot[Phase] = 3;
            publish();
            // No loader, GUI, COM release or diagnostic I/O is allowed while
            // this retry can still own stopped peers. Publication is POD-only.
            clock.Wait(snapshot[CleanupDeadlineExceeded] ? CleanupBackoffMs : SampleIntervalMs);
        }
        snapshot[ObserverEnded] = 1;
        snapshot[ObserverPendingCleanup] = 0;
        // Taking the device does not freeze the remaining native callback.
        // Refuse work if its final quiesced state acquired a later failure.
        if (reason == StopReason::Running && snapshot[NativeFailureReason])
            reason = StopReason::NativeError;
        if (reason == StopReason::Running && snapshot[DeviceAcquired] && !snapshot[LeaseMetadataValid])
            reason = StopReason::LeaseError;

        try
        {
            if (reason == StopReason::Running && snapshot[DeviceAcquired])
            {
                if (request.cancelled())
                    reason = StopReason::Cancelled;
                else if (!owner.StartGpu(snapshot))
                    reason = StopReason::GpuError;
                else
                {
                    snapshot[GuiStarted] = 1;
                    const auto gpuStarted = clock.Now();
                    for (;;)
                    {
                        if (request.cancelled())
                        {
                            reason = StopReason::Cancelled;
                            break;
                        }
                        const auto outcome = owner.PollGpu(snapshot);
                        if (outcome != Outcome::Pending)
                        {
                            snapshot[WorkloadComplete] = 1;
                            snapshot[WorkloadSucceeded] = outcome == Outcome::Passed ? 1u : 0u;
                            reason = outcome == Outcome::Passed ? StopReason::Completed : StopReason::GpuError;
                            break;
                        }
                        const auto now = elapsed();
                        if (now < gpuStarted || now - gpuStarted >= request.durationMs)
                        {
                            reason = StopReason::GpuError;
                            break;
                        }
                        snapshot[Phase] = 1;
                        publish();
                        if (reason != StopReason::Running)
                            break;
                        clock.Wait(SampleIntervalMs);
                    }
                }
            }
        }
        catch (...)
        {
            reason = StopReason::Exception;
        }

        cleanupStarted = clock.Now();
        for (;;)
        {
            if (snapshot[CleanupAttempts] != 0xffffffffu)
                ++snapshot[CleanupAttempts];
            bool stopped = false;
            try
            {
                stopped = owner.ShutdownGpu(snapshot);
            }
            catch (...)
            {
                reason = StopReason::Exception;
            }
            const auto now = elapsed();
            if (now < cleanupStarted || now - cleanupStarted >= CleanupDeadlineMs)
            {
                snapshot[CleanupDeadlineExceeded] = 1;
                reason = StopReason::CleanupError;
            }
            if (stopped)
                break;
            snapshot[Phase] = 3;
            publish();
            // A timeout changes the report and backoff, never ownership. The
            // worker and its loaded modules remain alive until actual drain.
            clock.Wait(snapshot[CleanupDeadlineExceeded] ? CleanupBackoffMs : SampleIntervalMs);
        }
        snapshot[CleanupPending] = 0;
        snapshot[CleanupComplete] = 1;
        snapshot[Phase] = 2;
        publish();
        return snapshot;
    }
}
