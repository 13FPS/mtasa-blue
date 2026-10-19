/*****************************************************************************
 * PROJECT: Multi Theft Auto - experimental Definitive Edition client
 * LICENSE: See LICENSE in the repository root
 *****************************************************************************/

#include "CoreStartupReference.h"
#include "../core/DefinitiveFrontend.h"
#include <algorithm>
#include <filesystem>
#include <process.h>

namespace deonline
{
    namespace
    {
        thread_local CoreStartupReference* inputOwner = nullptr;
        std::uint32_t                      LegacyRendererModules()
        {
            return (GetModuleHandleW(L"d3dx9_43.dll") ? 1u : 0u) | (GetModuleHandleW(L"d3d9.dll") ? 2u : 0u);
        }
        bool FrontendInputRetired(const DefinitiveGuiFrontendInputReference::Snapshot& sample, bool available)
        {
            using namespace DefinitiveGuiStartupReference;
            using namespace DefinitiveGuiFrontendInputReference;
            const auto zeroPair = [&](std::uint32_t first) { return !sample[first] && !sample[first + 1]; };
            const bool frontendRetired = !sample[Frontend]
                                             ? std::all_of(sample.begin() + Frontend, sample.begin() + Frontend + 32, [](auto value) { return value == 0; })
                                             : sample[Frontend] == 1 && sample[Frontend + 1] == 4 && sample[Frontend + 17] == 1 && !sample[Frontend + 6] &&
                                                   !sample[Frontend + 16] && !sample[Frontend + 19] && !sample[Frontend + 20] && !sample[Frontend + 21];
            const bool consoleRetired = !sample[Console]
                                            ? std::all_of(sample.begin() + Console, sample.begin() + Console + 32, [](auto value) { return value == 0; })
                                            : sample[Console] == 1 && (sample[Console + 1] == 0 || sample[Console + 1] == 4) && sample[Console + 21] == 1 &&
                                                  !sample[Console + 4] && !sample[Console + 5] && !sample[Console + 8] && !sample[Console + 9] &&
                                                  !sample[Console + 10] && !sample[Console + 22] && !sample[Console + 23] && !sample[Console + 28];
            const bool bindingsRetired = !sample[Bindings]
                                             ? std::all_of(sample.begin() + Bindings, sample.begin() + Bindings + 32, [](auto value) { return value == 0; })
                                             : sample[Bindings] == 1 && (sample[Bindings + 1] == 0 || sample[Bindings + 1] == 4) &&
                                                   sample[Bindings + 14] == 1 && !sample[Bindings + 10] && !sample[Bindings + 15] && !sample[Bindings + 21];
            // Native scope retirement is independent from script
            // progress, rendering success, and historical timeouts.
            // Core still cannot unload a live callback or its sole
            // pad restoration snapshot after a failed workload.
            return available && sample[DefinitiveGuiStartupReference::Version] == DefinitiveGuiFrontendInputReference::ApiVersion && sample[Phase] == 2 &&
                   sample[CleanupComplete] == 1 && !sample[CleanupPending] && sample[ObserverEnded] == 1 && sample[GuiCleanupComplete] == 1 &&
                   !sample[GuiShutdownStatus] && !sample[ObserverPendingCleanup] && !sample[ModuleRetained] && !sample[SessionRetained] &&
                   !sample[DeviceRetained] && !sample[DeviceReleaseFault] && !sample[DefinitiveGuiSurfaceReference::ChildRetained] &&
                   !sample[DefinitiveGuiSurfaceReference::QueueRetained] && !sample[DefinitiveGuiSurfaceReference::SurfaceRetained] &&
                   !sample[DefinitiveGuiSurfaceReference::ResizePending] && sample[DefinitiveGuiSurfaceReference::WindowCleanupComplete] == 1 &&
                   sample[DefinitiveGuiSurfaceReference::CompositionCleanupComplete] == 1 && sample[DefinitiveGuiSurfaceReference::ClassUnregistered] == 1 &&
                   frontendRetired && consoleRetired && bindingsRetired && !sample[PressedKeys] && !sample[PressedButtons] && !sample[InteractionFocused] &&
                   (!sample[InteractionEnabled] || sample[InputDetached] == 1) && sample[NativeEnded] == 1 && !sample[NativeCleanupPending] &&
                   !sample[NativeOwnerRetained] && zeroPair(NativeMetrics) && zeroPair(NativeMetrics + 2) &&
                   sample[NativeMetrics + 6] == sample[NativeMetrics + 12] && sample[NativeMetrics + 7] == sample[NativeMetrics + 13] &&
                   zeroPair(NativeMetrics + 14) && !sample[Frontend + 24] && !sample[Frontend + 26] && !sample[Frontend + 27] && !sample[Console + 25] &&
                   !sample[Console + 26] && !sample[Console + 27] && !sample[Bindings + 18] && !sample[Bindings + 19] && !sample[Bindings + 20] &&
                   !sample[CameraCaptureVerified] && !sample[FullEngineInputReady];
        }
    }

    bool CoreStartupReference::Begin(const void* moduleAddress) noexcept
    {
        return Start(moduleAddress, false);
    }

    bool CoreStartupReference::BeginInputReference(const void* moduleAddress) noexcept
    {
        return Start(moduleAddress, true);
    }

    bool CoreStartupReference::BeginMovementReference(const void* moduleAddress) noexcept
    {
        return Start(moduleAddress, true, true);
    }

    bool CoreStartupReference::BeginClockReference(const void* moduleAddress) noexcept
    {
        // Clock ownership excludes the input/render workloads. The first live
        // trial submits read commands only through the same actual Core owner.
        return Start(moduleAddress, true, false, false, 0, 1, false, false, false, false, false, false, true);
    }

    bool CoreStartupReference::BeginClockSetReference(const void* moduleAddress) noexcept
    {
        // A write reference is independently selected and cached. It borrows
        // only the existing worker's ownership and retained-cleanup lifetime.
        return Start(moduleAddress, true, false, false, 0, 1, false, false, false, false, false, false, false, true);
    }

    bool CoreStartupReference::BeginApplicationFrameReference(const void* moduleAddress) noexcept
    {
        // This observes the native application boundary without admitting GUI,
        // input, clock commands or lifecycle writes through the old protocols.
        return Start(moduleAddress, true, false, false, 0, 1, false, false, false, false, false, false, false, false, false, false, false, true);
    }

    bool CoreStartupReference::BeginRenderReference(const void* moduleAddress) noexcept
    {
        return Start(moduleAddress, true, false, true);
    }

    bool CoreStartupReference::BeginRenderReferenceV2(const void* moduleAddress, std::uint32_t route) noexcept
    {
        return (route == 1 || route == 2) && Start(moduleAddress, true, false, true, route, 2);
    }

    bool CoreStartupReference::BeginRenderReferenceV3(const void* moduleAddress, std::uint32_t route) noexcept
    {
        return route == 2 && Start(moduleAddress, true, false, true, route, 3);
    }

    bool CoreStartupReference::BeginRenderReferenceV4(const void* moduleAddress, std::uint32_t route) noexcept
    {
        // The classification is additive: requesting it must not change the
        // old ownership snapshots or silently select another native route.
        return route == 2 && Start(moduleAddress, true, false, true, route, 4);
    }

    bool CoreStartupReference::BeginRenderReferenceV5(const void* moduleAddress, std::uint32_t route) noexcept
    {
        // A scoped Present association needs an independent lease and cache;
        // earlier protocols must never expose or silently request its hooks.
        return route == 2 && Start(moduleAddress, true, false, true, route, 5);
    }

    bool CoreStartupReference::BeginRenderReferenceV6(const void* moduleAddress, std::uint32_t route) noexcept
    {
        // Native dispatch is independently selected and cached; its caller hook
        // must never be enabled or exposed by historical Present protocols.
        return route == 2 && Start(moduleAddress, true, false, true, route, 6);
    }

    bool CoreStartupReference::BeginGuiStartupReference(const void* moduleAddress) noexcept
    {
        // GUI startup owns a device reference and its own workload. It must not
        // silently enable or reuse the historical read-only render protocol.
        return Start(moduleAddress, true, false, false, 0, 1, true);
    }

    bool CoreStartupReference::Start(const void* moduleAddress, bool configuredInput, bool configuredMovement, bool configuredRender,
                                     std::uint32_t configuredRenderRoute, std::uint32_t configuredRenderApi, bool configuredGuiStartup,
                                     bool configuredGuiSurface, bool configuredGuiFrontend, bool configuredGuiFrontendInput, bool configuredFrontendStartup,
                                     bool configuredPersistentFrontend, bool configuredClock, bool configuredClockSet, bool configuredClockBeforeFrontend,
                                     bool configuredFrontendRuntime, bool configuredLocalInputRuntime, bool configuredApplicationFrame) noexcept
    {
        DWORD      expected = 0;
        const auto current = GetCurrentThreadId();
        if ((configuredLocalInputRuntime && !configuredFrontendRuntime) || (configuredClockBeforeFrontend && !configuredFrontendStartup) ||
            (static_cast<unsigned>(configuredMovement) + static_cast<unsigned>(configuredRender) + static_cast<unsigned>(configuredGuiStartup) +
                 static_cast<unsigned>(configuredGuiSurface) + static_cast<unsigned>(configuredGuiFrontend) +
                 static_cast<unsigned>(configuredGuiFrontendInput) + static_cast<unsigned>(configuredFrontendStartup) +
                 static_cast<unsigned>(configuredPersistentFrontend) + static_cast<unsigned>(configuredClock) + static_cast<unsigned>(configuredClockSet) +
                 static_cast<unsigned>(configuredFrontendRuntime) + static_cast<unsigned>(configuredApplicationFrame) >
             1) ||
            (!ownerThread.compare_exchange_strong(expected, current) && expected != current) || !moduleAddress || IsPending() || thread ||
            frontendQuarantined || clockQuarantined ||
            (!configuredClock && !configuredClockSet && !configuredApplicationFrame && !configuredGuiFrontendInput && !configuredFrontendStartup &&
             !configuredPersistentFrontend && !configuredFrontendRuntime && !frontendControl.PrepareAttempt(current)))
            return false;
        if (configuredPersistentFrontend && !persistentHostLease.Begin(current, GetTickCount64()))
            return false;
        if (configuredFrontendRuntime)
        {
            const auto token = runtimeMailbox.Begin(current);
            if (!token || runtimeHostLease.Begin(current, GetTickCount64()) != token)
                return false;
        }
        frontendRuntimeRequested = configuredFrontendRuntime;
        localInputRuntimeRequested = configuredLocalInputRuntime;
        localInputRuntimeCache.Reset();
        capturedLocalInputRuntime = {};
        runtimeCache.Reset();
        runtimeReport = {};
        capturedRuntime = {};
        runtimePublicationFault.store(false);
        runtimeViewAvailable = false;
        runtimeViewRequest = {};
        runtimeStartedAt = GetTickCount64();
        runtimePublicationSequence = 0;
        persistentFrontendRequested = configuredPersistentFrontend;
        clockRequested = configuredClock;
        clockSetRequested = configuredClockSet;
        clockBeforeFrontendRequested = configuredClockBeforeFrontend;
        applicationFrameRequested = configuredApplicationFrame;
        clockInitializationInvoked = false;
        clockAvailable.store(false);
        clockSequence.store(0);
        for (auto& field : publishedClock)
            field.store(0);
        capturedClock = {};
        clockOwnerAvailable.store(false);
        clockOwnerSequence.store(0);
        for (auto& field : publishedClockOwner)
            field.store(0);
        capturedClockOwner = {};
        clockSetAvailable.store(false);
        clockSetSequence.store(0);
        for (auto& field : publishedClockSet)
            field.store(0);
        capturedClockSet = {};
        applicationFrameAvailable.store(false);
        applicationFrameSequence.store(0);
        for (auto& field : publishedApplicationFrame)
            field.store(0);
        capturedApplicationFrame = {};
        persistentFrontendAvailable.store(false);
        persistentFrontendSequence.store(0);
        for (auto& field : publishedPersistentFrontend)
            field.store(0);
        capturedPersistentFrontend = {};
        address = moduleAddress;
        inputRequested = configuredInput;
        movementRequested = configuredMovement;
        renderRequested = configuredRender;
        guiRequested = configuredGuiStartup;
        guiSurfaceRequested = configuredGuiSurface;
        guiFrontendRequested = configuredGuiFrontend;
        guiFrontendInputRequested = configuredGuiFrontendInput || configuredFrontendStartup;
        frontendStartupRequested = configuredFrontendStartup;
        frontendStartupAvailable.store(false);
        frontendStartupSequence.store(0);
        for (auto& field : publishedFrontendStartup)
            field.store(0);
        capturedFrontendStartup = {};
        renderRoute = configuredRenderRoute;
        renderApi = configuredRenderApi;
        AcquireSRWLockExclusive(&inputLock);
        guiAvailable = false;
        publishedGui = {};
        guiSurfaceAvailable = false;
        publishedGuiSurface = {};
        guiFrontendAvailable = false;
        publishedGuiFrontend = {};
        guiFrontendInputAvailable = false;
        publishedGuiFrontendInput = {};
        inputAvailable = false;
        publishedInput = {};
        movementAvailable = false;
        publishedMovement = {};
        renderAvailable = false;
        publishedRender = {};
        renderV3Available = false;
        publishedRenderV3 = {};
        renderV4Available = false;
        publishedRenderV4 = {};
        renderV5Available = false;
        publishedRenderV5 = {};
        renderV6Available = false;
        publishedRenderV6 = {};
        renderV2Available = false;
        publishedRenderV2 = {};
        ReleaseSRWLockExclusive(&inputLock);
        capturedInput = {};
        capturedMovement = {};
        capturedRender = {};
        capturedRenderV2 = {};
        capturedRenderV3 = {};
        capturedRenderV4 = {};
        capturedRenderV5 = {};
        capturedRenderV6 = {};
        capturedGui = {};
        capturedGuiSurface = {};
        capturedGuiFrontend = {};
        capturedGuiFrontendInput = {};
        capturedFrontendControl = {};
        cancellation.store(false);
        state.store(1, std::memory_order_release);
        // Use the existing CRT worker convention instead of a joinable global
        // std::thread whose destructor could run under the Windows loader lock.
        const auto handle = _beginthreadex(nullptr, 0, ThreadMain, this, 0, nullptr);
        if (!handle)
        {
            if (persistentFrontendRequested)
                persistentHostLease.Revoke();
            if (frontendRuntimeRequested)
                runtimeHostLease.Revoke();
            state.store(0, std::memory_order_release);
            return false;
        }
        thread = reinterpret_cast<HANDLE>(handle);
        return true;
    }

    int CoreStartupReference::Poll() noexcept
    {
        if (ownerThread.load() != GetCurrentThreadId())
            return -1;
        const auto phase = state.load(std::memory_order_acquire);
        if (phase == 3)
            return 1;
        if (phase == 0)
            return -1;
        if (phase != 2 || WaitForSingleObject(thread, 0) != WAIT_OBJECT_0)
            return 0;
        // No loader API or blocking join is allowed on the script's polling
        // path, even when the worker is inside a slow DLL initializer.
        CloseHandle(thread);
        thread = nullptr;
        state.store(3, std::memory_order_release);
        return 1;
    }

    std::uint32_t CoreStartupReference::Read(std::uint32_t field) const noexcept
    {
        return !frontendRuntimeRequested && ownerThread.load() == GetCurrentThreadId() && state.load(std::memory_order_acquire) == 3 && field < values.size()
                   ? values[field]
                   : 0;
    }

    bool CoreStartupReference::CaptureInput() noexcept
    {
        if (ownerThread.load() != GetCurrentThreadId() || !TryAcquireSRWLockShared(&inputLock))
            return false;
        const bool available = inputAvailable;
        if (available)
            capturedInput = publishedInput;
        ReleaseSRWLockShared(&inputLock);
        return available;
    }

    std::uint32_t CoreStartupReference::ReadInput(std::uint32_t field) const noexcept
    {
        // Reads use the one captured owner-thread copy, never independently
        // sampled atomics or a Core export that the worker may be unloading.
        return ownerThread.load() == GetCurrentThreadId() && field < capturedInput.size() ? capturedInput[field] : 0;
    }

    bool CoreStartupReference::CaptureMovement() noexcept
    {
        if (ownerThread.load() != GetCurrentThreadId() || !TryAcquireSRWLockShared(&inputLock))
            return false;
        const bool available = movementAvailable;
        if (available)
            capturedMovement = publishedMovement;
        ReleaseSRWLockShared(&inputLock);
        return available;
    }

    std::uint32_t CoreStartupReference::ReadMovement(std::uint32_t field) const noexcept
    {
        return ownerThread.load() == GetCurrentThreadId() && field < capturedMovement.size() ? capturedMovement[field] : 0;
    }

    bool CoreStartupReference::CopyClockSnapshot(DefinitiveClockReference::Snapshot& output) const noexcept
    {
        if ((!clockRequested && !clockBeforeFrontendRequested) || !clockAvailable.load())
            return false;
        for (unsigned attempt = 0; attempt != 2; ++attempt)
        {
            const auto before = clockSequence.load();
            if (before & 1)
                continue;
            DefinitiveClockReference::Snapshot next{};
            for (std::size_t index = 0; index != next.size(); ++index)
                next[index] = publishedClock[index].load();
            if (clockSequence.load() == before)
            {
                output = next;
                return true;
            }
        }
        return false;
    }

    bool CoreStartupReference::CaptureClock() noexcept
    {
        return ownerThread.load() == GetCurrentThreadId() && clockRequested && CopyClockSnapshot(capturedClock);
    }

    std::uint32_t CoreStartupReference::ReadClock(std::uint32_t field) const noexcept
    {
        return ownerThread.load() == GetCurrentThreadId() && clockRequested && field < capturedClock.size() ? capturedClock[field] : 0;
    }

    void CoreStartupReference::PublishClock(const std::uint32_t* snapshot, std::uint32_t count)
    {
        if (!inputOwner || (!inputOwner->clockRequested && !inputOwner->clockBeforeFrontendRequested) || !snapshot ||
            count != DefinitiveClockReference::FieldCount)
            return;
        // A retained clock transaction can own a script peer's heap or cache
        // lock. Publication is value-only and takes neither kind of lock.
        inputOwner->clockSequence.fetch_add(1);
        for (std::size_t index = 0; index != count; ++index)
            inputOwner->publishedClock[index].store(snapshot[index]);
        inputOwner->clockAvailable.store(true);
        inputOwner->clockSequence.fetch_add(1);
    }

    bool CoreStartupReference::CopyClockSetSnapshot(DefinitiveClockSetReference::Snapshot& output) const noexcept
    {
        if (!clockSetRequested || !clockSetAvailable.load())
            return false;
        for (unsigned attempt = 0; attempt != 2; ++attempt)
        {
            const auto before = clockSetSequence.load();
            if (before & 1)
                continue;
            DefinitiveClockSetReference::Snapshot next{};
            for (std::size_t index = 0; index != next.size(); ++index)
                next[index] = publishedClockSet[index].load();
            if (clockSetSequence.load() == before)
            {
                output = next;
                return true;
            }
        }
        return false;
    }

    bool CoreStartupReference::CaptureClockSet() noexcept
    {
        return ownerThread.load() == GetCurrentThreadId() && CopyClockSetSnapshot(capturedClockSet);
    }

    std::uint32_t CoreStartupReference::ReadClockSet(std::uint32_t field) const noexcept
    {
        return ownerThread.load() == GetCurrentThreadId() && clockSetRequested && field < capturedClockSet.size() ? capturedClockSet[field] : 0;
    }

    void CoreStartupReference::PublishClockSet(const std::uint32_t* snapshot, std::uint32_t count)
    {
        if (!inputOwner || !inputOwner->clockSetRequested || !snapshot || count != DefinitiveClockSetReference::FieldCount)
            return;
        // Native recovery may have parked a peer inside the legacy cache or
        // heap. This independent atomic path must remain callable throughout.
        inputOwner->clockSetSequence.fetch_add(1);
        for (std::size_t index = 0; index != count; ++index)
            inputOwner->publishedClockSet[index].store(snapshot[index]);
        inputOwner->clockSetAvailable.store(true);
        inputOwner->clockSetSequence.fetch_add(1);
    }

    bool CoreStartupReference::CopyApplicationFrameSnapshot(DefinitiveApplicationFrameReference::Snapshot& output) const noexcept
    {
        if (!applicationFrameRequested || !applicationFrameAvailable.load())
            return false;
        for (unsigned attempt = 0; attempt != 2; ++attempt)
        {
            const auto before = applicationFrameSequence.load();
            if (before & 1)
                continue;
            DefinitiveApplicationFrameReference::Snapshot next{};
            for (std::size_t index = 0; index != next.size(); ++index)
                next[index] = publishedApplicationFrame[index].load();
            if (applicationFrameSequence.load() == before)
            {
                output = next;
                return true;
            }
        }
        return false;
    }

    bool CoreStartupReference::CaptureApplicationFrame() noexcept
    {
        return ownerThread.load() == GetCurrentThreadId() && CopyApplicationFrameSnapshot(capturedApplicationFrame);
    }

    std::uint32_t CoreStartupReference::ReadApplicationFrame(std::uint32_t field) const noexcept
    {
        return ownerThread.load() == GetCurrentThreadId() && applicationFrameRequested && field < capturedApplicationFrame.size()
                   ? capturedApplicationFrame[field]
                   : 0;
    }

    void CoreStartupReference::PublishApplicationFrame(const std::uint32_t* snapshot, std::uint32_t count)
    {
        if (!inputOwner || !inputOwner->applicationFrameRequested || !snapshot || count != DefinitiveApplicationFrameReference::FieldCount)
            return;
        // Cleanup publication cannot wait for the script's historical cache
        // lock: native detachment may already have parked that script peer.
        inputOwner->applicationFrameSequence.fetch_add(1);
        for (std::size_t index = 0; index != count; ++index)
            inputOwner->publishedApplicationFrame[index].store(snapshot[index]);
        inputOwner->applicationFrameAvailable.store(true);
        inputOwner->applicationFrameSequence.fetch_add(1);
    }

    void CoreStartupReference::WaitForClockRetirement() noexcept
    {
        if ((!clockRequested && !clockSetRequested && !clockBeforeFrontendRequested && !applicationFrameRequested) || !clockInitializationInvoked)
            return;
        clockQuarantined = true;
        const auto started = GetTickCount64();
        for (;;)
        {
            DefinitiveClockReference::Snapshot            snapshot{};
            DefinitiveClockSetReference::Snapshot         setSnapshot{};
            DefinitiveApplicationFrameReference::Snapshot applicationSnapshot{};
            if ((clockRequested && CopyClockSnapshot(snapshot) && DefinitiveClockReference::Retired(snapshot)) ||
                (clockBeforeFrontendRequested && CopyClockSnapshot(snapshot) && DefinitiveFrontendClockReference::RetiredForCompletedOwner(snapshot)) ||
                (clockSetRequested && CopyClockSetSnapshot(setSnapshot) && DefinitiveClockSetReference::Retired(setSnapshot)) ||
                (applicationFrameRequested && CopyApplicationFrameSnapshot(applicationSnapshot) &&
                 DefinitiveApplicationFrameReference::Retired(applicationSnapshot)))
            {
                clockQuarantined = false;
                return;
            }
            // Production Run performs its same-owner cleanup retries before
            // returning. A faulty return/exception with pending evidence must
            // instead retain this worker, its callback context and Core code.
            // Cancellation or a diagnostic deadline cannot turn missing proof
            // into permission to destroy the native owner's only snapshot.
            if (GetTickCount64() - started >= DefinitiveClockReference::CleanupDeadlineMs && !values[16])
                values[16] = 15;
            CheckCancellation();
            Sleep(DefinitiveClockReference::CleanupBackoffMs);
        }
    }

    bool CoreStartupReference::CaptureRender() noexcept
    {
        if (ownerThread.load() != GetCurrentThreadId() || renderRoute || !TryAcquireSRWLockShared(&inputLock))
            return false;
        const bool available = renderAvailable;
        if (available)
            capturedRender = publishedRender;
        ReleaseSRWLockShared(&inputLock);
        return available;
    }

    std::uint32_t CoreStartupReference::ReadRender(std::uint32_t field) const noexcept
    {
        return ownerThread.load() == GetCurrentThreadId() && !renderRoute && field < capturedRender.size() ? capturedRender[field] : 0;
    }

    bool CoreStartupReference::CaptureRenderV2() noexcept
    {
        if (ownerThread.load() != GetCurrentThreadId() || renderApi != 2 || !renderRoute || !TryAcquireSRWLockShared(&inputLock))
            return false;
        const bool available = renderV2Available;
        if (available)
            capturedRenderV2 = publishedRenderV2;
        ReleaseSRWLockShared(&inputLock);
        return available;
    }

    std::uint32_t CoreStartupReference::ReadRenderV2(std::uint32_t field) const noexcept
    {
        return ownerThread.load() == GetCurrentThreadId() && renderApi == 2 && renderRoute && field < capturedRenderV2.size() ? capturedRenderV2[field] : 0;
    }

    bool CoreStartupReference::CaptureRenderV3() noexcept
    {
        if (ownerThread.load() != GetCurrentThreadId() || renderApi != 3 || !renderRoute || !TryAcquireSRWLockShared(&inputLock))
            return false;
        const bool available = renderV3Available;
        if (available)
            capturedRenderV3 = publishedRenderV3;
        ReleaseSRWLockShared(&inputLock);
        return available;
    }

    std::uint32_t CoreStartupReference::ReadRenderV3(std::uint32_t field) const noexcept
    {
        return ownerThread.load() == GetCurrentThreadId() && renderApi == 3 && renderRoute && field < capturedRenderV3.size() ? capturedRenderV3[field] : 0;
    }

    bool CoreStartupReference::CaptureRenderV4() noexcept
    {
        if (ownerThread.load() != GetCurrentThreadId() || renderApi != 4 || !renderRoute || !TryAcquireSRWLockShared(&inputLock))
            return false;
        const bool available = renderV4Available;
        if (available)
            capturedRenderV4 = publishedRenderV4;
        ReleaseSRWLockShared(&inputLock);
        return available;
    }

    std::uint32_t CoreStartupReference::ReadRenderV4(std::uint32_t field) const noexcept
    {
        return ownerThread.load() == GetCurrentThreadId() && renderApi == 4 && renderRoute && field < capturedRenderV4.size() ? capturedRenderV4[field] : 0;
    }

    bool CoreStartupReference::CaptureRenderV5() noexcept
    {
        if (ownerThread.load() != GetCurrentThreadId() || renderApi != 5 || !renderRoute || !TryAcquireSRWLockShared(&inputLock))
            return false;
        const bool available = renderV5Available;
        if (available)
            capturedRenderV5 = publishedRenderV5;
        ReleaseSRWLockShared(&inputLock);
        return available;
    }

    std::uint32_t CoreStartupReference::ReadRenderV5(std::uint32_t field) const noexcept
    {
        return ownerThread.load() == GetCurrentThreadId() && renderApi == 5 && renderRoute && field < capturedRenderV5.size() ? capturedRenderV5[field] : 0;
    }

    bool CoreStartupReference::CaptureRenderV6() noexcept
    {
        if (ownerThread.load() != GetCurrentThreadId() || renderApi != 6 || !renderRoute || !TryAcquireSRWLockShared(&inputLock))
            return false;
        const bool available = renderV6Available;
        if (available)
            capturedRenderV6 = publishedRenderV6;
        ReleaseSRWLockShared(&inputLock);
        return available;
    }

    std::uint32_t CoreStartupReference::ReadRenderV6(std::uint32_t field) const noexcept
    {
        return ownerThread.load() == GetCurrentThreadId() && renderApi == 6 && renderRoute && field < capturedRenderV6.size() ? capturedRenderV6[field] : 0;
    }

    bool CoreStartupReference::CaptureGuiStartup() noexcept
    {
        if (ownerThread.load() != GetCurrentThreadId() || !guiRequested || !TryAcquireSRWLockShared(&inputLock))
            return false;
        const bool available = guiAvailable;
        if (available)
            capturedGui = publishedGui;
        ReleaseSRWLockShared(&inputLock);
        return available;
    }

    std::uint32_t CoreStartupReference::ReadGuiStartup(std::uint32_t field) const noexcept
    {
        return ownerThread.load() == GetCurrentThreadId() && guiRequested && field < capturedGui.size() ? capturedGui[field] : 0;
    }
    bool CoreStartupReference::BeginGuiSurfaceReference(const void* moduleAddress) noexcept
    {
        // A child surface owns a separate queue/window and cache. It neither
        // replaces offscreen startup evidence nor enables native-frame writes.
        return Start(moduleAddress, true, false, false, 0, 1, false, true);
    }
    bool CoreStartupReference::CaptureGuiSurface() noexcept
    {
        if (ownerThread.load() != GetCurrentThreadId() || !guiSurfaceRequested || !TryAcquireSRWLockShared(&inputLock))
            return false;
        const bool available = guiSurfaceAvailable;
        if (available)
            capturedGuiSurface = publishedGuiSurface;
        ReleaseSRWLockShared(&inputLock);
        return available;
    }
    std::uint32_t CoreStartupReference::ReadGuiSurface(std::uint32_t field) const noexcept
    {
        return ownerThread.load() == GetCurrentThreadId() && guiSurfaceRequested && field < capturedGuiSurface.size() ? capturedGuiSurface[field] : 0;
    }

    bool CoreStartupReference::BeginGuiFrontendReference(const void* moduleAddress) noexcept
    {
        // This owns the actual MTA menu and a script-confirmed engine control
        // lease. Its cache and protocol remain separate from passive Surface96.
        return Start(moduleAddress, true, false, false, 0, 1, false, false, true);
    }

    bool CoreStartupReference::BeginGuiFrontendInputReference(const void* moduleAddress) noexcept
    {
        // The owning Core worker publishes native neutral frames and ends its
        // own scopes. This protocol never borrows the script control mailbox.
        return Start(moduleAddress, true, false, false, 0, 1, false, false, false, true);
    }

    bool CoreStartupReference::BeginGuiFrontendStartupReference(const void* moduleAddress) noexcept
    {
        // This owns a completed Core through the separate frontend lifecycle;
        // the historical constructor-rejection diagnostic remains untouched.
        return Start(moduleAddress, true, false, false, 0, 1, false, false, false, false, true);
    }

    bool CoreStartupReference::BeginGuiFrontendStartupReferenceV2(const void* moduleAddress) noexcept
    {
        // V1 remains the historical GUI-only owner. This explicit upgrade
        // composes the existing read-only clock on the same completed Core.
        return Start(moduleAddress, true, false, false, 0, 1, false, false, false, false, true, false, false, false, true);
    }

    bool CoreStartupReference::CaptureGuiFrontendStartupClockV2() noexcept
    {
        return ownerThread.load() == GetCurrentThreadId() && clockBeforeFrontendRequested && CopyClockSnapshot(capturedClock);
    }

    std::uint32_t CoreStartupReference::ReadGuiFrontendStartupClockV2(std::uint32_t field) const noexcept
    {
        return ownerThread.load() == GetCurrentThreadId() && clockBeforeFrontendRequested && field < capturedClock.size() ? capturedClock[field] : 0;
    }

    void CoreStartupReference::PublishClockOwner(const DefinitiveFrontendStartup::Snapshot& report) noexcept
    {
        clockOwnerSequence.fetch_add(1);
        for (std::size_t index = 0; index != report.size(); ++index)
            publishedClockOwner[index].store(report[index]);
        clockOwnerAvailable.store(true);
        clockOwnerSequence.fetch_add(1);
    }

    bool CoreStartupReference::CaptureGuiFrontendStartupClockOwnerV2() noexcept
    {
        if (ownerThread.load() != GetCurrentThreadId() || !clockBeforeFrontendRequested || !clockOwnerAvailable.load())
            return false;
        for (unsigned attempt = 0; attempt != 2; ++attempt)
        {
            const auto before = clockOwnerSequence.load();
            if (before & 1)
                continue;
            DefinitiveFrontendStartup::Snapshot next{};
            for (std::size_t index = 0; index != next.size(); ++index)
                next[index] = publishedClockOwner[index].load();
            if (clockOwnerSequence.load() == before)
            {
                capturedClockOwner = next;
                return true;
            }
        }
        return false;
    }

    std::uint32_t CoreStartupReference::ReadGuiFrontendStartupClockOwnerV2(std::uint32_t field) const noexcept
    {
        return ownerThread.load() == GetCurrentThreadId() && clockBeforeFrontendRequested && field < capturedClockOwner.size() ? capturedClockOwner[field] : 0;
    }

    void CoreStartupReference::PublishFrontendStartup(const FrontendStartupReport::Snapshot& report) noexcept
    {
        // Use sequentially consistent atomic scalars so a bounded seqlock read
        // cannot accept fields from a later publication with an older sequence.
        // No script-owned lock may block cleanup of retained native peers.
        frontendStartupSequence.fetch_add(1);
        for (std::size_t index = 0; index < report.size(); ++index)
            publishedFrontendStartup[index].store(report[index]);
        frontendStartupAvailable.store(true);
        frontendStartupSequence.fetch_add(1);
    }

    bool CoreStartupReference::CaptureGuiFrontendStartup() noexcept
    {
        if (!frontendStartupRequested)
            return false;
        return CaptureFrontendLifecycle();
    }

    bool CoreStartupReference::CapturePersistentFrontendLifecycle() noexcept
    {
        return persistentFrontendRequested && CaptureFrontendLifecycle();
    }

    bool CoreStartupReference::CaptureFrontendLifecycle() noexcept
    {
        if (ownerThread.load() != GetCurrentThreadId() || (!frontendStartupRequested && !persistentFrontendRequested) || !frontendStartupAvailable.load())
            return false;
        for (unsigned attempt = 0; attempt < 2; ++attempt)
        {
            const auto before = frontendStartupSequence.load();
            if (before & 1)
                continue;
            FrontendStartupReport::Snapshot next{};
            for (std::size_t index = 0; index < next.size(); ++index)
                next[index] = publishedFrontendStartup[index].load();
            if (frontendStartupSequence.load() == before)
            {
                capturedFrontendStartup = next;
                return true;
            }
        }
        return false;
    }

    std::uint32_t CoreStartupReference::ReadGuiFrontendStartup(std::uint32_t field) const noexcept
    {
        return ownerThread.load() == GetCurrentThreadId() && frontendStartupRequested && field < capturedFrontendStartup.size() ? capturedFrontendStartup[field]
                                                                                                                                : 0;
    }

    std::uint32_t CoreStartupReference::BeginPersistentFrontend(const void* moduleAddress) noexcept
    {
        return Start(moduleAddress, true, false, false, 0, 1, false, false, false, false, false, true) ? persistentHostLease.Generation() : 0;
    }

    std::uint32_t CoreStartupReference::BeginFrontendRuntime(const void* moduleAddress, bool localInput) noexcept
    {
        return Start(moduleAddress, true, false, false, 0, 1, false, false, false, false, false, false, false, false, false, true, localInput)
                   ? runtimeMailbox.Generation()
                   : 0;
    }

    bool CoreStartupReference::CaptureLocalInputRuntime() noexcept
    {
        return localInputRuntimeRequested && ownerThread.load() == GetCurrentThreadId() && localInputRuntimeCache.Capture(capturedLocalInputRuntime);
    }

    std::uint32_t CoreStartupReference::ReadLocalInputRuntime(std::uint32_t field) const noexcept
    {
        return localInputRuntimeRequested && ownerThread.load() == GetCurrentThreadId() && field < capturedLocalInputRuntime.size()
                   ? capturedLocalInputRuntime[field]
                   : 0;
    }

    bool CoreStartupReference::CommandFrontendRuntime(std::uint32_t generation, std::uint32_t sequence, std::uint32_t command) noexcept
    {
        if (!frontendRuntimeRequested || ownerThread.load() != GetCurrentThreadId() || !IsPending())
            return false;
        if (command != static_cast<std::uint32_t>(FrontendRuntimeReport::Command::Stop))
        {
            FrontendRuntimeReport::Snapshot sample{};
            if (!runtimeCache.Capture(sample) ||
                (command == 1 && sample[FrontendRuntimeReport::WorkerPhase] != static_cast<std::uint32_t>(FrontendRuntimeReport::Phase::Hidden)) ||
                (command == 2 && sample[FrontendRuntimeReport::WorkerPhase] != static_cast<std::uint32_t>(FrontendRuntimeReport::Phase::Visible)))
                return false;
        }
        return runtimeMailbox.Submit(GetCurrentThreadId(), generation, sequence, static_cast<FrontendRuntimeReport::Command>(command));
    }

    bool CoreStartupReference::HeartbeatFrontendRuntime(std::uint32_t generation) noexcept
    {
        return frontendRuntimeRequested && ownerThread.load() == GetCurrentThreadId() && IsPending() &&
               runtimeHostLease.Heartbeat(GetCurrentThreadId(), generation, GetTickCount64());
    }

    int CoreStartupReference::PollFrontendRuntime() noexcept
    {
        return frontendRuntimeRequested ? Poll() : -1;
    }

    bool CoreStartupReference::CaptureFrontendRuntime() noexcept
    {
        return frontendRuntimeRequested && ownerThread.load() == GetCurrentThreadId() && runtimeCache.Capture(capturedRuntime);
    }

    std::uint32_t CoreStartupReference::ReadFrontendRuntime(std::uint32_t field) const noexcept
    {
        return frontendRuntimeRequested && ownerThread.load() == GetCurrentThreadId() && field < capturedRuntime.size() ? capturedRuntime[field] : 0;
    }

    void CoreStartupReference::PublishRuntime() noexcept
    {
        namespace R = FrontendRuntimeReport;
        const auto superseded = runtimeMailbox.TakeSuperseded();
        if (superseded.sequence)
        {
            ++runtimeReport[R::SupersededRequests];
            if (superseded.sequence > runtimeReport[R::AcknowledgedSequence])
            {
                runtimeReport[R::AcknowledgedSequence] = superseded.sequence;
                runtimeReport[R::AcknowledgedCommand] = static_cast<std::uint32_t>(superseded.command);
                runtimeReport[R::AcknowledgementCode] = static_cast<std::uint32_t>(R::Acknowledgement::Superseded);
            }
        }
        const auto accepted = runtimeMailbox.Accepted();
        const auto pending = runtimeMailbox.Pending();
        runtimeReport[R::Version] = R::ApiVersion;
        ++runtimePublicationSequence;
        runtimeReport[R::SequenceLow] = static_cast<std::uint32_t>(runtimePublicationSequence);
        runtimeReport[R::SequenceHigh] = static_cast<std::uint32_t>(runtimePublicationSequence >> 32);
        const auto now = GetTickCount64();
        runtimeReport[R::ElapsedMs] = static_cast<std::uint32_t>((std::min)(now - runtimeStartedAt, std::uint64_t{UINT32_MAX}));
        runtimeReport[R::SessionGenerationLow] = runtimeMailbox.Generation();
        runtimeReport[R::ScriptThread] = ownerThread.load();
        runtimeReport[R::WorkerThread] = GetCurrentThreadId();
        runtimeReport[R::AcceptedSequence] = accepted.sequence;
        runtimeReport[R::AcceptedCommand] = static_cast<std::uint32_t>(accepted.command);
        runtimeReport[R::OutstandingSequence] = pending.sequence;
        runtimeReport[R::OutstandingCommand] = static_cast<std::uint32_t>(pending.command);
        runtimeReport[R::StopRequested] = runtimeMailbox.Stopping() || cancellation.load() || runtimeReport[R::HostExpiryObserved];
        runtimeReport[R::StopSequence] = accepted.command == R::Command::Stop ? accepted.sequence : 0;
        runtimeReport[R::HostLeaseFresh] = runtimeHostLease.Fresh(runtimeMailbox.Generation(), now) ? 1u : 0u;
        runtimeReport[R::HostHeartbeatBudgetMs] = 1000;
        runtimeReport[R::NativeHeartbeatBudgetMs] = runtimeReport[R::CursorHeartbeatBudgetMs] = 500;
        runtimeCache.Publish(runtimeReport);
    }

    bool CoreStartupReference::RuntimeHostFresh()
    {
        return inputOwner && inputOwner->frontendRuntimeRequested &&
               inputOwner->runtimeHostLease.Fresh(inputOwner->runtimeMailbox.Generation(), GetTickCount64());
    }

    bool CoreStartupReference::RuntimeViewCancelled()
    {
        if (!inputOwner || !inputOwner->frontendRuntimeRequested)
            return true;
        const auto command = inputOwner->runtimeMailbox.Pending().command;
        return inputOwner->cancellation.load() || inputOwner->runtimePublicationFault.load() || command == FrontendRuntimeReport::Command::Hide ||
               command == FrontendRuntimeReport::Command::Stop;
    }

    void CoreStartupReference::PublishRuntimeView(const std::uint32_t* snapshot, std::uint32_t count)
    {
        namespace R = FrontendRuntimeReport;
        auto* owner = inputOwner;
        if (!owner || !owner->frontendRuntimeRequested)
            return;
        if (!snapshot || count != DefinitivePersistentFrontend::FieldCount || GetCurrentThreadId() != owner->runtimeReport[R::WorkerThread] ||
            snapshot[0] != 1 || snapshot[256] != 1 || snapshot[257] != owner->runtimeReport[R::ViewGenerationLow] || snapshot[258] || snapshot[244] ||
            snapshot[245] || std::any_of(snapshot + 270, snapshot + count, [](auto value) { return value != 0; }))
        {
            owner->runtimePublicationFault.store(true);
            return;
        }
        std::copy(snapshot, snapshot + count, owner->runtimeReport.begin() + R::ViewOffset);
        owner->runtimeViewAvailable = true;
        ++owner->runtimeReport[R::ViewSamples];
        owner->runtimeReport[R::BackendPinnedExpected] |= snapshot[57] | snapshot[232];
        // NativeVerified is set before cursor admission. Focused actual GUI
        // input and a successful present prove that the later cursor admission
        // and GUI Poll also completed; a partial Begin cannot acknowledge Show.
        // The shared GUI lifecycle keeps CleanupPending set while its active
        // view still owns resources. Only final retirement clears that debt;
        // requiring zero here would reject a healthy visibly running menu.
        const bool admitted = snapshot[1] == 1 && snapshot[5] == 1 && !snapshot[6] && snapshot[68] && snapshot[70] && snapshot[96] == 1 && snapshot[97] == 2 &&
                              snapshot[99] && snapshot[100] == 1 && snapshot[161] == 1 && snapshot[162] == 1 && !snapshot[171] && !snapshot[172] &&
                              snapshot[208] == 1 && snapshot[210] == 1 && snapshot[232] == 1 && snapshot[236] == 1 && snapshot[237] == 1 && !snapshot[238] &&
                              snapshot[259] == 1 && snapshot[262] == 1;
        if (admitted && owner->runtimeViewRequest.command == R::Command::Show && owner->runtimeMailbox.Complete(owner->runtimeViewRequest))
        {
            owner->runtimeReport[R::AcknowledgedSequence] = owner->runtimeViewRequest.sequence;
            owner->runtimeReport[R::AcknowledgedCommand] = static_cast<std::uint32_t>(R::Command::Show);
            owner->runtimeReport[R::AcknowledgementCode] = static_cast<std::uint32_t>(R::Acknowledgement::Completed);
            ++owner->runtimeReport[R::ShowAcknowledgements];
            owner->runtimeReport[R::WorkerPhase] = static_cast<std::uint32_t>(R::Phase::Visible);
        }
        if (snapshot[1] == 3)
        {
            owner->runtimeReport[R::WorkerPhase] = static_cast<std::uint32_t>(R::Phase::RetainedCleanup);
            owner->runtimeReport[R::CleanupPending] = 1;
        }
        else if (RuntimeViewCancelled())
            owner->runtimeReport[R::WorkerPhase] = static_cast<std::uint32_t>(owner->runtimeMailbox.Stopping() ? R::Phase::Stopping : R::Phase::Hiding);
        owner->PublishRuntime();
    }

    bool CoreStartupReference::HeartbeatPersistentFrontend(std::uint32_t generation) noexcept
    {
        return ownerThread.load() == GetCurrentThreadId() && persistentFrontendRequested && IsPending() &&
               persistentHostLease.Heartbeat(GetCurrentThreadId(), generation, GetTickCount64());
    }

    std::uint32_t CoreStartupReference::ReadPersistentFrontendLifecycle(std::uint32_t field) const noexcept
    {
        return ownerThread.load() == GetCurrentThreadId() && persistentFrontendRequested && field < capturedFrontendStartup.size()
                   ? capturedFrontendStartup[field]
                   : 0;
    }

    bool CoreStartupReference::CapturePersistentFrontend() noexcept
    {
        if (ownerThread.load() != GetCurrentThreadId() || !persistentFrontendRequested || !persistentFrontendAvailable.load())
            return false;
        for (unsigned attempt = 0; attempt < 2; ++attempt)
        {
            const auto before = persistentFrontendSequence.load();
            if (before & 1)
                continue;
            DefinitivePersistentFrontend::Snapshot next{};
            for (std::size_t index = 0; index < next.size(); ++index)
                next[index] = publishedPersistentFrontend[index].load();
            if (persistentFrontendSequence.load() == before)
            {
                capturedPersistentFrontend = next;
                return true;
            }
        }
        return false;
    }

    std::uint32_t CoreStartupReference::ReadPersistentFrontend(std::uint32_t field) const noexcept
    {
        return ownerThread.load() == GetCurrentThreadId() && persistentFrontendRequested && field < capturedPersistentFrontend.size()
                   ? capturedPersistentFrontend[field]
                   : 0;
    }

    bool CoreStartupReference::PersistentHostFresh()
    {
        return inputOwner && inputOwner->persistentFrontendRequested &&
               inputOwner->persistentHostLease.Fresh(inputOwner->persistentHostLease.Generation(), GetTickCount64());
    }

    void CoreStartupReference::PublishPersistentFrontend(const std::uint32_t* snapshot, std::uint32_t count)
    {
        if (!inputOwner || !inputOwner->persistentFrontendRequested || !snapshot || count != DefinitivePersistentFrontend::FieldCount)
            return;
        // Atomic value publication cannot wait behind the script while native
        // cleanup still owns held peers. This is a distinct persistent cache.
        inputOwner->persistentFrontendSequence.fetch_add(1);
        for (std::size_t index = 0; index < count; ++index)
            inputOwner->publishedPersistentFrontend[index].store(snapshot[index]);
        inputOwner->persistentFrontendAvailable.store(true);
        inputOwner->persistentFrontendSequence.fetch_add(1);
    }

    bool CoreStartupReference::CaptureGuiFrontendInput() noexcept
    {
        if (ownerThread.load() != GetCurrentThreadId() || !guiFrontendInputRequested || !TryAcquireSRWLockShared(&inputLock))
            return false;
        const bool available = guiFrontendInputAvailable;
        if (available)
            capturedGuiFrontendInput = publishedGuiFrontendInput;
        ReleaseSRWLockShared(&inputLock);
        return available;
    }

    std::uint32_t CoreStartupReference::ReadGuiFrontendInput(std::uint32_t field) const noexcept
    {
        return ownerThread.load() == GetCurrentThreadId() && guiFrontendInputRequested && field < capturedGuiFrontendInput.size()
                   ? capturedGuiFrontendInput[field]
                   : 0;
    }

    bool CoreStartupReference::CaptureGuiFrontend() noexcept
    {
        if (ownerThread.load() != GetCurrentThreadId() || !guiFrontendRequested || !TryAcquireSRWLockShared(&inputLock))
            return false;
        const bool available = guiFrontendAvailable;
        if (available)
            capturedGuiFrontend = publishedGuiFrontend;
        ReleaseSRWLockShared(&inputLock);
        return available;
    }

    std::uint32_t CoreStartupReference::ReadGuiFrontend(std::uint32_t field) const noexcept
    {
        return ownerThread.load() == GetCurrentThreadId() && guiFrontendRequested && field < capturedGuiFrontend.size() ? capturedGuiFrontend[field] : 0;
    }

    bool CoreStartupReference::CaptureGuiFrontendControl() noexcept
    {
        return ownerThread.load() == GetCurrentThreadId() && guiFrontendRequested &&
               frontendControl.Capture(GetCurrentThreadId(), GetTickCount64(), capturedFrontendControl);
    }

    std::uint32_t CoreStartupReference::ReadGuiFrontendControl(std::uint32_t field) const noexcept
    {
        return ownerThread.load() == GetCurrentThreadId() && guiFrontendRequested && field < capturedFrontendControl.size() ? capturedFrontendControl[field]
                                                                                                                            : 0;
    }

    bool CoreStartupReference::AcknowledgeGuiFrontendControl(std::uint32_t generation) noexcept
    {
        return ownerThread.load() == GetCurrentThreadId() && guiFrontendRequested &&
               frontendControl.Script(FrontendControlMailbox::ScriptAction::Acknowledge, GetCurrentThreadId(), generation, GetTickCount64());
    }

    bool CoreStartupReference::HeartbeatGuiFrontendControl(std::uint32_t generation) noexcept
    {
        return ownerThread.load() == GetCurrentThreadId() && guiFrontendRequested &&
               frontendControl.Script(FrontendControlMailbox::ScriptAction::Heartbeat, GetCurrentThreadId(), generation, GetTickCount64());
    }

    bool CoreStartupReference::RevokeGuiFrontendControl(std::uint32_t generation) noexcept
    {
        return ownerThread.load() == GetCurrentThreadId() && guiFrontendRequested &&
               frontendControl.Script(FrontendControlMailbox::ScriptAction::Revoke, GetCurrentThreadId(), generation, GetTickCount64());
    }

    bool CoreStartupReference::RestoreGuiFrontendControl(std::uint32_t generation) noexcept
    {
        return ownerThread.load() == GetCurrentThreadId() && guiFrontendRequested &&
               frontendControl.Script(FrontendControlMailbox::ScriptAction::Restore, GetCurrentThreadId(), generation, GetTickCount64());
    }

    bool CoreStartupReference::ControlInput(std::uint32_t action, std::uint32_t* snapshot, std::uint32_t count)
    {
        return inputOwner && inputOwner->guiFrontendRequested &&
               inputOwner->frontendControl.Worker(action, GetCurrentThreadId(), GetTickCount64(), snapshot, count);
    }

    bool CoreStartupReference::InputCancelled()
    {
        return !inputOwner || inputOwner->cancellation.load(std::memory_order_acquire);
    }

    void CoreStartupReference::PublishInput(const std::uint32_t* snapshot, std::uint32_t count)
    {
        if (!inputOwner || !snapshot ||
            count != (inputOwner->guiFrontendInputRequested ? DefinitiveGuiFrontendInputReference::FieldCount
                      : inputOwner->guiFrontendRequested    ? DefinitiveGuiFrontendReference::FieldCount
                      : inputOwner->guiSurfaceRequested     ? DefinitiveGuiSurfaceReference::FieldCount
                      : inputOwner->guiRequested            ? DefinitiveGuiStartupReference::FieldCount
                      : inputOwner->renderRequested         ? (inputOwner->renderApi == 6   ? DefinitiveRenderReference::FieldCountV6
                                                               : inputOwner->renderApi == 5 ? DefinitiveRenderReference::FieldCountV5
                                                               : inputOwner->renderApi == 4 ? DefinitiveRenderReference::FieldCountV4
                                                               : inputOwner->renderApi == 3 ? DefinitiveRenderReference::FieldCountV3
                                                               : inputOwner->renderRoute    ? DefinitiveRenderReference::FieldCountV2
                                                                                            : DefinitiveRenderReference::FieldCount)
                      : inputOwner->movementRequested       ? DefinitiveMovementReference::FieldCount
                                                            : DefinitiveInputReference::FieldCount))
            return;
        const bool pendingDispatchCleanup = inputOwner->renderRequested && inputOwner->renderApi == 6 && count == DefinitiveRenderReference::FieldCountV6 &&
                                            snapshot[DefinitiveRenderReference::Phase] == 3 && snapshot[DefinitiveRenderReference::CleanupPending] == 1;
        const bool pendingGuiCleanup = inputOwner->guiRequested && count == DefinitiveGuiStartupReference::FieldCount &&
                                       snapshot[DefinitiveGuiStartupReference::Phase] == 3 && snapshot[DefinitiveGuiStartupReference::CleanupPending] == 1;
        const bool pendingSurfaceCleanup = inputOwner->guiSurfaceRequested && count == DefinitiveGuiSurfaceReference::FieldCount &&
                                           snapshot[DefinitiveGuiStartupReference::Phase] == 3 && snapshot[DefinitiveGuiStartupReference::CleanupPending] == 1;
        const bool pendingFrontendCleanup = inputOwner->guiFrontendRequested && count == DefinitiveGuiFrontendReference::FieldCount &&
                                            snapshot[DefinitiveGuiStartupReference::Phase] == 3 && snapshot[DefinitiveGuiStartupReference::CleanupPending] == 1;
        const bool pendingFrontendInputCleanup = inputOwner->guiFrontendInputRequested && count == DefinitiveGuiFrontendInputReference::FieldCount &&
                                                 snapshot[DefinitiveGuiStartupReference::Phase] == 3 &&
                                                 snapshot[DefinitiveGuiStartupReference::CleanupPending] == 1;
        // A failed resume may retain the script peer inside a shared capture.
        // Drop only intermediate API6/GUI cleanup reports instead of blocking the
        // worker that must retry that resume; final publication remains reliable.
        if (pendingDispatchCleanup || pendingGuiCleanup || pendingSurfaceCleanup || pendingFrontendCleanup || pendingFrontendInputCleanup)
        {
            if (!TryAcquireSRWLockExclusive(&inputOwner->inputLock))
                return;
        }
        else
            AcquireSRWLockExclusive(&inputOwner->inputLock);
        if (inputOwner->guiFrontendInputRequested)
        {
            std::copy_n(snapshot, count, inputOwner->publishedGuiFrontendInput.begin());
            inputOwner->guiFrontendInputAvailable = true;
        }
        else if (inputOwner->guiFrontendRequested)
        {
            std::copy_n(snapshot, count, inputOwner->publishedGuiFrontend.begin());
            inputOwner->guiFrontendAvailable = true;
        }
        else if (inputOwner->guiSurfaceRequested)
        {
            std::copy_n(snapshot, count, inputOwner->publishedGuiSurface.begin());
            inputOwner->guiSurfaceAvailable = true;
        }
        else if (inputOwner->guiRequested)
        {
            std::copy_n(snapshot, count, inputOwner->publishedGui.begin());
            inputOwner->guiAvailable = true;
        }
        else if (inputOwner->renderRequested && inputOwner->renderApi == 6)
        {
            std::copy_n(snapshot, count, inputOwner->publishedRenderV6.begin());
            inputOwner->renderV6Available = true;
        }
        else if (inputOwner->renderRequested && inputOwner->renderApi == 5)
        {
            std::copy_n(snapshot, count, inputOwner->publishedRenderV5.begin());
            inputOwner->renderV5Available = true;
        }
        else if (inputOwner->renderRequested && inputOwner->renderApi == 4)
        {
            std::copy_n(snapshot, count, inputOwner->publishedRenderV4.begin());
            inputOwner->renderV4Available = true;
        }
        else if (inputOwner->renderRequested && inputOwner->renderApi == 3)
        {
            std::copy_n(snapshot, count, inputOwner->publishedRenderV3.begin());
            inputOwner->renderV3Available = true;
        }
        else if (inputOwner->renderRequested && inputOwner->renderRoute)
        {
            std::copy_n(snapshot, count, inputOwner->publishedRenderV2.begin());
            inputOwner->renderV2Available = true;
        }
        else if (inputOwner->renderRequested)
        {
            std::copy_n(snapshot, count, inputOwner->publishedRender.begin());
            inputOwner->renderAvailable = true;
        }
        else if (inputOwner->movementRequested)
        {
            std::copy_n(snapshot, count, inputOwner->publishedMovement.begin());
            inputOwner->movementAvailable = true;
        }
        else
        {
            std::copy_n(snapshot, count, inputOwner->publishedInput.begin());
            inputOwner->inputAvailable = true;
        }
        ReleaseSRWLockExclusive(&inputOwner->inputLock);
    }

    bool CoreStartupReference::Cancel() noexcept
    {
        if (state.load(std::memory_order_acquire) == 0)
            return false;
        cancellation.store(true, std::memory_order_release);
        return true;
    }

    bool CoreStartupReference::IsPending() const noexcept
    {
        const auto phase = state.load(std::memory_order_acquire);
        return phase == 1 || phase == 2;
    }

    unsigned __stdcall CoreStartupReference::ThreadMain(void* context) noexcept
    {
        const auto self = static_cast<CoreStartupReference*>(context);
        self->Run();
        self->state.store(2, std::memory_order_release);
        return 0;
    }

    bool CoreStartupReference::CheckCancellation() noexcept
    {
        if (!cancellation.load(std::memory_order_acquire))
            return false;
        if (!values[16])
            values[16] = 12;
        return true;
    }

    void CoreStartupReference::RunFrontendRuntime(HMODULE core, const std::wstring& mtaDirectory, const std::wstring& gtaDirectory) noexcept
    {
        namespace R = FrontendRuntimeReport;
        namespace C = DefinitiveFrontendRuntime;
        namespace L = DefinitiveLocalInputRuntime;
        namespace S = DefinitiveFrontendStartup;
        namespace P = DefinitivePersistentFrontend;
        using Status = C::Status;
        using Version = std::uint32_t (*)();
        using SetDirectory = void (*)(const wchar_t*, std::size_t);
        const auto version = reinterpret_cast<Version>(GetProcAddress(core, "MTA_DEFrontendRuntimeApiVersion"));
        const auto begin = reinterpret_cast<C::Begin>(GetProcAddress(core, "MTA_BeginDEFrontendRuntime"));
        const auto read = reinterpret_cast<C::Read>(GetProcAddress(core, "MTA_ReadDEFrontendRuntime"));
        const auto pulse = reinterpret_cast<C::Pulse>(GetProcAddress(core, "MTA_PulseDEFrontendRuntime"));
        const auto view = reinterpret_cast<C::RunView>(GetProcAddress(core, "MTA_RunDEFrontendRuntimeView"));
        const auto shutdown = reinterpret_cast<C::Shutdown>(GetProcAddress(core, "MTA_ShutdownDEFrontendRuntime"));
        const auto release = reinterpret_cast<C::Release>(GetProcAddress(core, "MTA_ReleaseDEFrontendRuntime"));
        const auto localVersion = reinterpret_cast<Version>(GetProcAddress(core, "MTA_DELocalInputRuntimeApiVersion"));
        const auto localConfigure = reinterpret_cast<L::Configure>(GetProcAddress(core, "MTA_ConfigureDELocalInputRuntime"));
        const auto localPulse = reinterpret_cast<L::Pulse>(GetProcAddress(core, "MTA_PulseDELocalInputRuntime"));
        const auto localRead = reinterpret_cast<L::Read>(GetProcAddress(core, "MTA_ReadDELocalInputRuntime"));
        const auto localSuspend = reinterpret_cast<L::Suspend>(GetProcAddress(core, "MTA_SuspendDELocalInputRuntime"));
        const auto localDisable = reinterpret_cast<L::Disable>(GetProcAddress(core, "MTA_DisableDELocalInputRuntime"));
        const auto setMta = reinterpret_cast<SetDirectory>(GetProcAddress(core, "SetMTADirectory"));
        const auto setGta = reinterpret_cast<SetDirectory>(GetProcAddress(core, "SetGTADirectory"));
        runtimeReport[R::CoreLoaded] = 1;
        inputOwner = this;
        void*         handle = nullptr;
        C::Snapshot   lastCore{};
        L::Snapshot   lastLocal{};
        HWND          localWindow = nullptr;
        bool          localConfigured = false;
        std::uint64_t coreGeneration = 0;
        const auto    fail = [&](R::Failure failure, R::StopReason reason = R::StopReason::ViewFault) noexcept
        {
            if (!runtimeReport[R::LastFailure])
                runtimeReport[R::LastFailure] = static_cast<std::uint32_t>(failure);
            if (!runtimeReport[R::SessionStopReason])
                runtimeReport[R::SessionStopReason] = static_cast<std::uint32_t>(reason);
        };
        const auto retain = [&]() noexcept
        {
            runtimeReport[R::WorkerPhase] = static_cast<std::uint32_t>(R::Phase::RetainedCleanup);
            runtimeReport[R::CleanupPending] = 1;
            const auto retainedAt = GetTickCount64();
            // A returned Busy/throw without a qualified final publication may
            // still own native peers. Keep code/TLS/owner indefinitely; neither
            // the script nor elapsed time can certify this scope's retirement.
            for (;;)
            {
                if (GetTickCount64() - retainedAt >= 2000)
                    runtimeReport[R::CleanupDeadlineExceeded] = 1;
                PublishRuntime();
                Sleep(250);
            }
        };
        const auto coreShape = [&](const C::Snapshot& sample) noexcept
        {
            return sample[S::Version] == 1 && sample[C::RuntimeVersion] == 1 && sample[S::OwnerThread] == GetCurrentThreadId() &&
                   sample[C::OwnerThread] == GetCurrentThreadId() && sample[S::Phase] >= 1 && sample[S::Phase] <= 5 && sample[C::RuntimePhase] >= 1 &&
                   sample[C::RuntimePhase] <= 6 && (sample[C::SessionGenerationLow] || sample[C::SessionGenerationHigh]) && !sample[S::NetworkStarted] &&
                   !sample[S::GameplayReady] && !sample[S::FullMtaReady] && !sample[C::NetworkStarted] && !sample[C::GameplayReady] &&
                   !sample[C::FullMtaReady] && !sample[C::Reserved59] && !sample[C::Reserved63] &&
                   std::all_of(sample.begin() + S::Reserved24, sample.begin() + 32, [](auto value) { return value == 0; });
        };
        const auto hidden = [&](const C::Snapshot& sample) noexcept
        {
            return coreShape(sample) && sample[S::Phase] == 2 && sample[S::CoreRetained] == 1 && sample[S::BackendStarted] == 1 &&
                   std::all_of(sample.begin() + S::FrontendRetained, sample.begin() + S::CleanupComplete + 1, [](auto value) { return value == 0; }) &&
                   !sample[S::ReleaseFault] && sample[S::LastViewCleanupComplete] == 1 &&
                   sample[C::RuntimePhase] == static_cast<std::uint32_t>(C::Phase::Hidden) && sample[C::ServicesInitialized] == 1 &&
                   sample[C::ServicesRetained] == 1 && sample[C::XmlRetained] == 1 && sample[C::ConfigRetained] == 1 && sample[C::CVarsLoaded] == 1 &&
                   sample[C::BindingsRetained] == 1 && sample[C::BindingManagerCreations] == 1 && sample[C::ConfigurationLoads] == 1 &&
                   sample[C::BindingLoads] == 1 && sample[C::ViewsAttached] == sample[C::ViewsRetired] && !sample[C::ViewBorrowed] &&
                   sample[C::HiddenNeutralVerified] == 1 && sample[C::ConfigurationReadOnly] == 1 && !sample[C::CleanupPending] && !sample[C::CleanupComplete];
        };
        const auto stopped = [&](const C::Snapshot& sample) noexcept
        {
            return coreShape(sample) && sample[S::Phase] == 4 && sample[S::CoreRetained] == 1 && sample[S::CleanupComplete] == 1 &&
                   !sample[S::BackendStarted] && !sample[S::ExactImageBound] && !sample[S::CapabilitiesLow] && !sample[S::CapabilitiesHigh] &&
                   std::all_of(sample.begin() + S::FrontendRetained, sample.begin() + S::CleanupPending + 1, [](auto value) { return value == 0; }) &&
                   !sample[S::ReleaseFault] && sample[S::LastViewCleanupComplete] == 1 &&
                   sample[C::RuntimePhase] == static_cast<std::uint32_t>(C::Phase::Stopped) && !sample[C::ServicesRetained] && !sample[C::XmlRetained] &&
                   !sample[C::ConfigRetained] && !sample[C::CVarsLoaded] && !sample[C::BindingsRetained] && !sample[C::ViewBorrowed] &&
                   !sample[C::CleanupPending] && sample[C::CleanupComplete] == 1;
        };
        const auto sampleCore = [&](R::CoreStage stage) noexcept
        {
            C::Snapshot sample{};
            try
            {
                runtimeReport[R::LastCoreStatus] = static_cast<std::uint32_t>(read(handle, sample.data(), C::FieldCount));
            }
            catch (...)
            {
                fail(R::Failure::CoreRead);
                return false;
            }
            if (runtimeReport[R::LastCoreStatus] != static_cast<std::uint32_t>(Status::Ok) || !coreShape(sample))
                return false;
            const auto generation =
                static_cast<std::uint64_t>(sample[C::SessionGenerationLow]) | (static_cast<std::uint64_t>(sample[C::SessionGenerationHigh]) << 32);
            if (coreGeneration && coreGeneration != generation)
                return false;
            coreGeneration = generation;
            lastCore = sample;
            std::copy(sample.begin(), sample.end(), runtimeReport.begin() + R::CoreOffset);
            runtimeReport[R::CoreSampleStage] = static_cast<std::uint32_t>(stage);
            ++runtimeReport[R::CoreSampleCount];
            return true;
        };
        const auto viewRetired = [&]() noexcept
        {
            if (!runtimeViewAvailable)
                return false;
            DefinitiveGuiFrontendInputReference::Snapshot first{};
            std::copy_n(runtimeReport.begin() + R::ViewOffset, first.size(), first.begin());
            const auto* sample = runtimeReport.data() + R::ViewOffset;
            return FrontendInputRetired(first, true) && sample[P::PersistentVersion] == 1 &&
                   sample[P::SessionGenerationLow] == runtimeReport[R::ViewGenerationLow] && !sample[P::SessionGenerationHigh] &&
                   !sample[P::NativeLeaseActive] && sample[P::HostLeaseFresh] <= 1 && sample[P::NativeLeaseRevoked] <= 1 && sample[P::NativeLeaseReason] <= 6 &&
                   (!(sample[P::NativeLeaseGenerationLow] || sample[P::NativeLeaseGenerationHigh]) ||
                    (sample[P::NativeLeaseRevoked] == 1 && sample[P::NativeLeaseReason])) &&
                   sample[P::HostHeartbeatBudgetMs] == 1000 && sample[P::NativeHeartbeatBudgetMs] == 500 && sample[P::StopReason] >= 1 &&
                   sample[P::StopReason] <= 5 && std::all_of(sample + P::Reserved270, sample + P::FieldCount, [](auto value) { return value == 0; });
        };
        const auto acknowledge = [&](const FrontendRuntimeMailbox::Request& request, R::Acknowledgement code) noexcept
        {
            if (!runtimeMailbox.Complete(request))
                return false;
            runtimeReport[R::AcknowledgedSequence] = request.sequence;
            runtimeReport[R::AcknowledgedCommand] = static_cast<std::uint32_t>(request.command);
            runtimeReport[R::AcknowledgementCode] = static_cast<std::uint32_t>(code);
            return true;
        };
        const auto sampleLocal = [&]() noexcept
        {
            if (!localInputRuntimeRequested)
                return true;
            L::Snapshot sample{};
            Status      status = Status::Failed;
            try
            {
                status = localRead(handle, sample.data(), L::FieldCount);
            }
            catch (...)
            {
                return false;
            }
            if (status != Status::Ok || sample[L::Version] != L::ApiVersion || sample[L::OwnerThread] != GetCurrentThreadId() ||
                sample[L::Phase] < static_cast<std::uint32_t>(L::State::Disabled) || sample[L::Phase] > static_cast<std::uint32_t>(L::State::Failed) ||
                sample[L::LastFailure] > static_cast<std::uint32_t>(L::Failure::Exception))
                return false;
            for (const auto field : {L::Configured, L::Enabled, L::NativeAttempted, L::NativeActive, L::NativeBound, L::NativeRevoked, L::PhysicalRearmReady,
                                     L::CanonicalNeutral, L::BindingsBorrowed, L::CleanupPending, L::CleanupComplete, L::Focused, L::ContextEligible})
                if (sample[field] > 1)
                    return false;
            if (sample[L::NativeReason] > 7 || (sample[L::NativeLifetimeMs] && sample[L::NativeLifetimeMs] != 500) ||
                (sample[L::NativeActive] && (!sample[L::NativeAttempted] || !sample[L::NativeBound] || sample[L::NativeRevoked] ||
                                             !(sample[L::NativeGenerationLow] || sample[L::NativeGenerationHigh]))))
                return false;
            if (sample[L::CleanupComplete] && (sample[L::NativeActive] || sample[L::NativeBound] || sample[L::CleanupPending] ||
                                               (sample[L::NativeAttempted] && (!sample[L::CanonicalNeutral] || !sample[L::NativeEndAttempts] ||
                                                                               ((sample[L::NativeGenerationLow] || sample[L::NativeGenerationHigh]) &&
                                                                                (!sample[L::NativeRevoked] || !sample[L::NativeReason]))))))
                return false;
            lastLocal = sample;
            localInputRuntimeCache.Publish(sample);
            return true;
        };
        const auto retireLocal = [&](bool final) noexcept
        {
            if (!localInputRuntimeRequested)
                return true;
            // A fresh menu may not race the old movement callback. Busy keeps
            // this exact owner/module alive; elapsed time is never retirement.
            for (;;)
            {
                Status status = Status::Failed;
                try
                {
                    status = final ? localDisable(handle) : localSuspend(handle);
                }
                catch (...)
                {
                    return false;
                }
                if (!sampleLocal())
                    return false;
                if (status != Status::Busy)
                    return status == Status::Ok && !lastLocal[L::NativeActive] && !lastLocal[L::NativeBound] && !lastLocal[L::CleanupPending] &&
                           lastLocal[L::CleanupComplete] &&
                           (!lastLocal[L::NativeAttempted] ||
                            (lastLocal[L::NativeEndAttempts] && (!(lastLocal[L::NativeGenerationLow] || lastLocal[L::NativeGenerationHigh]) ||
                                                                 (lastLocal[L::NativeRevoked] && lastLocal[L::NativeReason])))) &&
                           (lastLocal[L::CanonicalNeutral] ||
                            (!lastLocal[L::Configured] && lastLocal[L::Phase] == static_cast<std::uint32_t>(L::State::Disabled)));
                runtimeReport[R::WorkerPhase] = static_cast<std::uint32_t>(R::Phase::RetainedCleanup);
                runtimeReport[R::CleanupPending] = 1;
                PublishRuntime();
                Sleep(10);
            }
        };
        const auto configureLocal = [&]() noexcept
        {
            if (!localInputRuntimeRequested)
                return true;
            L::RequestV1 request;
            request.window = localWindow;
            Status configured = Status::Failed;
            try
            {
                configured = localConfigure(handle, &request);
            }
            catch (...)
            {
                return false;
            }
            localConfigured = configured == Status::Ok;
            return localConfigured && sampleLocal();
        };
        try
        {
            if (!version || !begin || !read || !pulse || !view || !shutdown || !release || !setMta || !setGta || version() != C::ApiVersion)
            {
                fail(R::Failure::Exports, R::StopReason::LoaderFault);
                inputOwner = nullptr;
                return;
            }
            if (localInputRuntimeRequested &&
                (!localVersion || !localConfigure || !localPulse || !localRead || !localSuspend || !localDisable || localVersion() != L::ApiVersion))
            {
                fail(R::Failure::Exports, R::StopReason::LoaderFault);
                inputOwner = nullptr;
                return;
            }
            setMta(mtaDirectory.c_str(), mtaDirectory.size());
            setGta(gtaDirectory.c_str(), gtaDirectory.size());
            try
            {
                handle = begin(C::ApiVersion);
            }
            catch (...)
            {
                fail(R::Failure::Begin);
                retain();
            }
            if (!handle)
            {
                fail(R::Failure::Begin, R::StopReason::LoaderFault);
                inputOwner = nullptr;
                return;
            }
            runtimeReport[R::CoreOwnerRetained] = 1;
            if (!sampleCore(R::CoreStage::Initial))
            {
                fail(R::Failure::CoreRead);
                retain();
            }
            if (!hidden(lastCore) || !sampleLocal())
                fail(R::Failure::CoreState);
            runtimeReport[R::LastViewCleanupComplete] = 1;
            runtimeReport[R::WorkerPhase] = static_cast<std::uint32_t>(R::Phase::Hidden);
            PublishRuntime();
            while (!runtimeReport[R::LastFailure])
            {
                const auto pending = runtimeMailbox.Pending();
                if (pending.command == R::Command::Stop || cancellation.load())
                    break;
                if (!RuntimeHostFresh())
                {
                    runtimeReport[R::HostExpiryObserved] = 1;
                    fail(R::Failure::HostExpired, R::StopReason::HostExpired);
                    break;
                }
                if (pending.command == R::Command::Hide)
                {
                    if (acknowledge(pending, R::Acknowledgement::Completed))
                        ++runtimeReport[R::HideAcknowledgements];
                }
                else if (pending.command == R::Command::Show)
                {
                    if (!retireLocal(false))
                    {
                        fail(R::Failure::CoreState);
                        break;
                    }
                    runtimeReport[R::CleanupPending] = 0;
                    runtimeReport[R::WorkerPhase] = static_cast<std::uint32_t>(R::Phase::Hidden);
                    // Stop may supersede Show while the callback retires.
                    if (runtimeMailbox.Pending().command == R::Command::Stop || cancellation.load() || !RuntimeHostFresh())
                        continue;
                    if (localInputRuntimeRequested && !localWindow)
                        localWindow = GetAncestor(GetForegroundWindow(), GA_ROOT);
                    if (lastCore[C::ViewGenerationHigh] || lastCore[C::ViewGenerationLow] == UINT32_MAX)
                    {
                        fail(R::Failure::ViewGenerationExhausted);
                        break;
                    }
                    if (pulse(handle) != Status::Ok || !sampleCore(R::CoreStage::BeforeShow) || !hidden(lastCore))
                    {
                        fail(R::Failure::CoreState);
                        break;
                    }
                    runtimeReport[R::ViewGenerationLow] = lastCore[C::ViewGenerationLow] + 1;
                    ++runtimeReport[R::ViewAttempts];
                    runtimeReport[R::LastViewCleanupComplete] = 0;
                    runtimeReport[R::WorkerPhase] = static_cast<std::uint32_t>(R::Phase::Showing);
                    std::fill(runtimeReport.begin() + R::ViewOffset, runtimeReport.end(), 0);
                    runtimeViewAvailable = false;
                    runtimePublicationFault.store(false);
                    runtimeViewRequest = pending;
                    PublishRuntime();
                    P::RequestV1 request;
                    request.generation = runtimeReport[R::ViewGenerationLow];
                    request.cancelled = RuntimeViewCancelled;
                    request.publish = PublishRuntimeView;
                    request.heartbeat = RuntimeHostFresh;
                    Status viewStatus = Status::Busy;
                    bool   threw = false;
                    try
                    {
                        viewStatus = view(handle, &request);
                    }
                    catch (...)
                    {
                        threw = true;
                    }
                    runtimeReport[R::LastViewStatus] = static_cast<std::uint32_t>(viewStatus);
                    // Failed/no-publication is admitted only for the concrete
                    // unchanged hidden InputBind refusal. Every other missing
                    // or nonretired report retains the native owner first.
                    if (!runtimeViewAvailable && !runtimePublicationFault.load() && !threw && viewStatus == Status::Failed)
                    {
                        const auto before = lastCore;
                        if (!sampleCore(R::CoreStage::AfterHide) || !hidden(lastCore) || lastCore[S::ExactImageBound] ||
                            lastCore[S::LastFailure] != static_cast<std::uint32_t>(S::Failure::InputBind) || lastCore[C::LastFailure] ||
                            lastCore[C::ViewsAttached] != before[C::ViewsAttached] || lastCore[C::ViewsRetired] != before[C::ViewsRetired] ||
                            lastCore[C::ViewGenerationLow] != before[C::ViewGenerationLow] || lastCore[C::ViewGenerationHigh] != before[C::ViewGenerationHigh])
                            retain();
                        fail(R::Failure::View);
                        runtimeReport[R::LastViewCleanupComplete] = 1;
                        break;
                    }
                    if (!viewRetired())
                        retain();
                    ++runtimeReport[R::ViewsRetired];
                    runtimeReport[R::LastRetiredViewGeneration] = runtimeReport[R::ViewGenerationLow];
                    runtimeReport[R::LastViewCleanupComplete] = 1;
                    runtimeReport[R::CleanupPending] = 0;
                    if (!sampleCore(R::CoreStage::AfterHide) || !hidden(lastCore) || lastCore[C::ViewGenerationLow] != runtimeReport[R::ViewGenerationLow])
                    {
                        fail(R::Failure::CoreState);
                        break;
                    }
                    const auto stop = runtimeReport[R::ViewOffset + P::StopReason];
                    if (stop == static_cast<std::uint32_t>(P::Stop::GuiClosed))
                        ++runtimeReport[R::GuiHideCount];
                    if (stop == static_cast<std::uint32_t>(P::Stop::FocusLost))
                        ++runtimeReport[R::FocusHideCount];
                    if (threw || runtimePublicationFault.load() || (viewStatus != Status::Ok && viewStatus != Status::Stopped) ||
                        stop == static_cast<std::uint32_t>(P::Stop::Fault))
                    {
                        fail(R::Failure::View);
                        break;
                    }
                    if (!RuntimeHostFresh() || stop == static_cast<std::uint32_t>(P::Stop::HostLeaseExpired))
                    {
                        runtimeReport[R::HostExpiryObserved] = 1;
                        fail(R::Failure::HostExpired, R::StopReason::HostExpired);
                        break;
                    }
                    const auto next = runtimeMailbox.Pending();
                    if (next.command == R::Command::Hide && acknowledge(next, R::Acknowledgement::Completed))
                        ++runtimeReport[R::HideAcknowledgements];
                    else if (next.command == R::Command::Show)
                        acknowledge(next, R::Acknowledgement::Failed);
                    runtimeReport[R::WorkerPhase] = static_cast<std::uint32_t>(R::Phase::Hidden);
                    if (localInputRuntimeRequested && runtimeMailbox.Pending().command != R::Command::Stop && !configureLocal())
                    {
                        fail(R::Failure::CoreState);
                        break;
                    }
                }
                else if (pulse(handle) != Status::Ok)
                {
                    fail(R::Failure::CoreState);
                    break;
                }
                if (localInputRuntimeRequested && localConfigured && runtimeReport[R::WorkerPhase] == static_cast<std::uint32_t>(R::Phase::Hidden))
                {
                    const auto status = localPulse(handle);
                    if (!sampleLocal() || (status != Status::Ok && status != Status::Busy))
                    {
                        fail(R::Failure::CoreState);
                        break;
                    }
                    if (status == Status::Busy)
                    {
                        if (!retireLocal(false))
                        {
                            fail(R::Failure::CoreState);
                            break;
                        }
                        runtimeReport[R::WorkerPhase] = static_cast<std::uint32_t>(R::Phase::Hidden);
                        runtimeReport[R::CleanupPending] = 0;
                    }
                }
                PublishRuntime();
                Sleep(50);
            }
            runtimeReport[R::WorkerPhase] = static_cast<std::uint32_t>(R::Phase::Stopping);
            runtimeReport[R::CleanupPending] = 1;
            if (!runtimeReport[R::SessionStopReason])
                runtimeReport[R::SessionStopReason] = static_cast<std::uint32_t>(cancellation.load() ? R::StopReason::Cancelled : R::StopReason::Requested);
            const auto cleanupStarted = GetTickCount64();
            const auto wait = [&]() noexcept
            {
                if (GetTickCount64() - cleanupStarted >= 2000)
                    runtimeReport[R::CleanupDeadlineExceeded] = 1;
                runtimeReport[R::WorkerPhase] = static_cast<std::uint32_t>(R::Phase::RetainedCleanup);
                PublishRuntime();
                Sleep(250);
            };
            Status status = Status::Busy;
            if (!retireLocal(true))
            {
                fail(R::Failure::Shutdown);
                retain();
            }
            do
            {
                try
                {
                    status = shutdown(handle);
                }
                catch (...)
                {
                    fail(R::Failure::Shutdown);
                    retain();
                }
                if (status == Status::Busy)
                    wait();
            } while (status == Status::Busy);
            runtimeReport[R::LastCoreStatus] = static_cast<std::uint32_t>(status);
            if (status != Status::Ok || !sampleCore(R::CoreStage::Stopped) || !stopped(lastCore))
            {
                fail(R::Failure::Shutdown);
                retain();
            }
            runtimeReport[R::CoreStoppedVerified] = 1;
            do
            {
                try
                {
                    status = release(handle);
                }
                catch (...)
                {
                    fail(R::Failure::Release);
                    retain();
                }
                if (status == Status::Busy)
                    wait();
            } while (status == Status::Busy);
            if (status != Status::Ok)
            {
                fail(R::Failure::Release);
                retain();
            }
            handle = nullptr;
            runtimeReport[R::CoreReleasedVerified] = 1;
            runtimeReport[R::CoreOwnerRetained] = 0;
            runtimeHostLease.Revoke();
        }
        catch (...)
        {
            fail(R::Failure::Exception);
            if (handle)
                retain();
        }
        inputOwner = nullptr;
    }

    void CoreStartupReference::FinishRuntime(HMODULE core) noexcept
    {
        namespace R = FrontendRuntimeReport;
        if (core && runtimeReport[R::CoreOwnerRetained])
            return;
        if (core && !FreeLibrary(core))
            runtimeReport[R::LastFailure] = static_cast<std::uint32_t>(R::Failure::Unload);
        runtimeReport[R::CoreLoaded] = GetModuleHandleW(L"core.dll") ? 1u : 0u;
        runtimeReport[R::CoreUnloadedVerified] = runtimeReport[R::CoreLoaded] ? 0u : 1u;
        runtimeReport[R::LegacyModulesAfter] = LegacyRendererModules();
        if (runtimeReport[R::CoreLoaded] || (runtimeReport[R::LegacyModulesAfter] & ~runtimeReport[R::LegacyModulesBefore]))
            runtimeReport[R::LastFailure] = static_cast<std::uint32_t>(R::Failure::Unload);
        const auto expected = runtimeReport[R::BackendPinnedExpected];
        if ((GetModuleHandleW(L"game_de.dll") ? 1u : 0u) != expected)
            runtimeReport[R::LastFailure] = static_cast<std::uint32_t>(R::Failure::Unload);
        if (values[16] && !runtimeReport[R::LastFailure])
            runtimeReport[R::LastFailure] = static_cast<std::uint32_t>(R::Failure::Begin);
        runtimeReport[R::CleanupPending] = 0;
        runtimeReport[R::CleanupComplete] = 1;
        runtimeReport[R::WorkerPhase] = static_cast<std::uint32_t>(runtimeReport[R::LastFailure] ? R::Phase::Failed : R::Phase::Stopped);
        const auto pending = runtimeMailbox.Pending();
        if (pending.sequence && runtimeMailbox.Complete(pending))
        {
            runtimeReport[R::AcknowledgedSequence] = pending.sequence;
            runtimeReport[R::AcknowledgedCommand] = static_cast<std::uint32_t>(pending.command);
            runtimeReport[R::AcknowledgementCode] = static_cast<std::uint32_t>(
                pending.command == R::Command::Stop && !runtimeReport[R::LastFailure] ? R::Acknowledgement::Completed : R::Acknowledgement::Failed);
        }
        runtimeHostLease.Revoke();
        PublishRuntime();
    }

    void CoreStartupReference::RunFrontendStartup(HMODULE core, const std::wstring& mtaDirectory, const std::wstring& gtaDirectory) noexcept
    {
        namespace Startup = DefinitiveFrontendStartup;
        using Status = Startup::Status;
        using Version = std::uint32_t (*)();
        using SetDirectory = void (*)(const wchar_t*, std::size_t);
        const auto version = reinterpret_cast<Version>(GetProcAddress(core, "MTA_DEFrontendStartupApiVersion"));
        const auto begin = reinterpret_cast<Startup::Begin>(GetProcAddress(core, "MTA_BeginDEFrontendStartup"));
        const auto read = reinterpret_cast<Startup::Read>(GetProcAddress(core, "MTA_ReadDEFrontendStartup"));
        const auto view = reinterpret_cast<Startup::RunView>(GetProcAddress(core, "MTA_RunDEFrontendStartupView"));
        const auto clockVersion = reinterpret_cast<Version>(GetProcAddress(core, "MTA_DEFrontendClockReferenceApiVersion"));
        const auto clockRun = reinterpret_cast<DefinitiveFrontendClockReference::Run>(GetProcAddress(core, "MTA_RunDEFrontendClockReference"));
        const auto persistentVersion = reinterpret_cast<Version>(GetProcAddress(core, "MTA_DEPersistentFrontendApiVersion"));
        const auto persistentView = reinterpret_cast<DefinitivePersistentFrontend::Run>(GetProcAddress(core, "MTA_RunDEPersistentFrontendV1"));
        const auto shutdown = reinterpret_cast<Startup::Shutdown>(GetProcAddress(core, "MTA_ShutdownDEFrontendStartup"));
        const auto release = reinterpret_cast<Startup::Release>(GetProcAddress(core, "MTA_ReleaseDEFrontendStartup"));
        const auto setMta = reinterpret_cast<SetDirectory>(GetProcAddress(core, "SetMTADirectory"));
        const auto setGta = reinterpret_cast<SetDirectory>(GetProcAddress(core, "SetGTADirectory"));
        void*      handle = nullptr;
        FrontendStartupReport::Snapshot report{};
        Startup::Snapshot               initial{}, after{}, stopped{};
        bool                            initialValid = false, afterValid = false, viewCalled = false;
        bool                            clockStageSucceeded = !clockBeforeFrontendRequested;
        std::uint64_t                   clockOwnerCapabilities = 0;
        Status                          viewStatus = Status::Stopped;
        const auto                      error = [&](std::uint32_t code, bool ownershipFailure = false)
        {
            if (!values[16] || ownershipFailure)
                values[16] = code;
        };
        const auto valid = [](const Startup::Snapshot& sample)
        {
            const std::array<std::uint32_t, 14> booleans = {
                Startup::CoreRetained, Startup::BackendStartAttempted, Startup::BackendStarted, Startup::ExactImageBound,        Startup::FrontendRetained,
                Startup::GuiBorrowed,  Startup::ServicesRetained,      Startup::InputRequested, Startup::CleanupPending,         Startup::CleanupComplete,
                Startup::ReleaseFault, Startup::NetworkStarted,        Startup::GameplayReady,  Startup::LastViewCleanupComplete};
            return sample[Startup::Version] == Startup::ApiVersion && sample[Startup::OwnerThread] == GetCurrentThreadId() && sample[Startup::Phase] >= 1 &&
                   sample[Startup::Phase] <= 5 && !sample[Startup::NetworkStarted] && !sample[Startup::GameplayReady] && !sample[Startup::FullMtaReady] &&
                   sample[Startup::LastFailure] <= static_cast<std::uint32_t>(Startup::Failure::View) &&
                   std::all_of(booleans.begin(), booleans.end(), [&](auto field) { return sample[field] <= 1; }) &&
                   std::all_of(sample.begin() + Startup::Reserved24, sample.end(), [](auto value) { return value == 0; });
        };
        const auto noFrontend = [](const Startup::Snapshot& sample)
        { return std::all_of(sample.begin() + Startup::FrontendRetained, sample.begin() + Startup::CleanupPending, [](auto value) { return value == 0; }); };
        const auto capture = [&](Startup::Snapshot& output, std::uint32_t offset)
        {
            Startup::Snapshot sample{};
            if (read(handle, sample.data(), Startup::FieldCount) != Status::Ok)
                return false;
            output = sample;
            std::copy(sample.begin(), sample.end(), report.begin() + offset);
            PublishFrontendStartup(report);
            return valid(sample);
        };
        try
        {
            if (!version || !begin || !read || !view || !shutdown || !release || !setMta || !setGta || version() != Startup::ApiVersion ||
                (persistentFrontendRequested && (!persistentVersion || !persistentView || persistentVersion() != DefinitivePersistentFrontend::ApiVersion)))
            {
                error(20);
                return;
            }
            if (clockBeforeFrontendRequested && (!clockVersion || !clockRun || clockVersion() != DefinitiveFrontendClockReference::ApiVersion))
            {
                error(26);
                return;
            }
            setMta(mtaDirectory.c_str(), mtaDirectory.size());
            setGta(gtaDirectory.c_str(), gtaDirectory.size());
            if (CheckCancellation())
                return;
            // A thrown Begin may have created an owner without returning its
            // handle. Retain Core until the successful return proves otherwise.
            frontendQuarantined = true;
            handle = begin(Startup::ApiVersion);
            frontendQuarantined = false;
            if (!handle)
            {
                error(21);
                return;
            }
            initialValid = capture(initial, FrontendStartupReport::Begin);
            if (!initialValid)
                error(22);
            else
            {
                values[8] = initial[Startup::Phase];
                const bool running = initial[Startup::Phase] == 2 && initial[Startup::CoreRetained] == 1 && initial[Startup::BackendStarted] == 1 &&
                                     initial[Startup::BackendStartAttempted] == 1 && initial[Startup::Pulses] && !initial[Startup::ExactImageBound] &&
                                     noFrontend(initial) && !initial[Startup::CleanupPending] && !initial[Startup::CleanupComplete] &&
                                     !initial[Startup::ReleaseFault] && !initial[Startup::ViewRuns] && initial[Startup::LastViewCleanupComplete] == 1;
                if (!running)
                    error(21);
                else if (!CheckCancellation())
                {
                    inputOwner = this;
                    bool admitView = true;
                    if (clockBeforeFrontendRequested)
                    {
                        namespace Clock = DefinitiveClockReference;
                        constexpr std::uint32_t clockDurationMs = 2000;
                        Status                  clockStatus = Status::Busy;
                        bool                    clockThrew = false;
                        clockInitializationInvoked = true;
                        try
                        {
                            clockStatus = clockRun(handle, clockDurationMs, InputCancelled, PublishClock);
                        }
                        catch (...)
                        {
                            // A bad clock return can retain a peer inside its
                            // allocator. Keep all enclosing path objects and
                            // TLS alive before attempting any owner read/unwind.
                            WaitForClockRetirement();
                            clockThrew = true;
                        }
                        Clock::Snapshot   clockSnapshot{};
                        const bool        published = CopyClockSnapshot(clockSnapshot);
                        Startup::Snapshot clockOwner{};
                        bool              clockOwnerValid = false;
                        if (!published && clockStatus == Status::Failed && !clockThrew)
                        {
                            // The completed Core rejects wrong-image Bind
                            // before native Begin or callbacks. Qualify only
                            // that narrow refusal through its actual owner;
                            // missing evidence after Busy/throw never permits it.
                            clockOwnerValid = read(handle, clockOwner.data(), Startup::FieldCount) == Status::Ok && valid(clockOwner) &&
                                              clockOwner[Startup::Phase] == 2 && clockOwner[Startup::CoreRetained] == 1 &&
                                              clockOwner[Startup::BackendStarted] == 1 && clockOwner[Startup::BackendStartAttempted] == 1 &&
                                              clockOwner[Startup::Pulses] == initial[Startup::Pulses] && !clockOwner[Startup::ExactImageBound] &&
                                              noFrontend(clockOwner) && !clockOwner[Startup::CleanupPending] && !clockOwner[Startup::CleanupComplete] &&
                                              !clockOwner[Startup::ReleaseFault] && !clockOwner[Startup::ViewRuns] &&
                                              clockOwner[Startup::LastViewCleanupComplete] == 1 &&
                                              clockOwner[Startup::LastFailure] == static_cast<std::uint32_t>(Startup::Failure::InputBind) &&
                                              clockOwner[Startup::CapabilitiesLow] == initial[Startup::CapabilitiesLow] &&
                                              clockOwner[Startup::CapabilitiesHigh] == initial[Startup::CapabilitiesHigh];
                            if (!clockOwnerValid)
                                WaitForClockRetirement();
                            // No clock scope entered, so there is no native
                            // report to synthesize and no adapter-stop claim.
                            clockInitializationInvoked = false;
                        }
                        else
                        {
                            // Any publication must independently prove this
                            // clock scope retired before CoreRead or GUI locks.
                            do
                            {
                                WaitForClockRetirement();
                            } while (!CopyClockSnapshot(clockSnapshot) || !DefinitiveFrontendClockReference::RetiredForCompletedOwner(clockSnapshot));
                            clockOwnerValid = read(handle, clockOwner.data(), Startup::FieldCount) == Status::Ok && valid(clockOwner);
                            clockStageSucceeded =
                                !clockThrew && clockStatus == Status::Ok && DefinitiveFrontendClockReference::WorkloadSucceeded(clockSnapshot) &&
                                clockSnapshot[Clock::WorkloadElapsedMs] >= clockDurationMs &&
                                clockSnapshot[Clock::ProducerThread] == initial[Startup::OwnerThread] && clockOwnerValid && clockOwner[Startup::Phase] == 2 &&
                                clockOwner[Startup::CoreRetained] == 1 && clockOwner[Startup::BackendStartAttempted] == 1 &&
                                clockOwner[Startup::BackendStarted] == 1 && clockOwner[Startup::ExactImageBound] == 1 &&
                                clockOwner[Startup::Pulses] > initial[Startup::Pulses] && noFrontend(clockOwner) && !clockOwner[Startup::CleanupPending] &&
                                !clockOwner[Startup::CleanupComplete] && !clockOwner[Startup::ReleaseFault] && !clockOwner[Startup::ViewRuns] &&
                                clockOwner[Startup::LastViewCleanupComplete] == 1 && !clockOwner[Startup::LastFailure] &&
                                // Bind legitimately adds only its build-pinned native-pad
                                // capability to the completed backend's initial services.
                                clockOwner[Startup::CapabilitiesLow] ==
                                    (initial[Startup::CapabilitiesLow] | static_cast<std::uint32_t>(mta::game_de::NativePadBindingCapabilities)) &&
                                clockOwner[Startup::CapabilitiesHigh] ==
                                    (initial[Startup::CapabilitiesHigh] | static_cast<std::uint32_t>(mta::game_de::NativePadBindingCapabilities >> 32));
                        }
                        PublishClockOwner(clockOwner);
                        if (clockStageSucceeded)
                            clockOwnerCapabilities = static_cast<std::uint64_t>(clockOwner[Startup::CapabilitiesLow]) |
                                                     (static_cast<std::uint64_t>(clockOwner[Startup::CapabilitiesHigh]) << 32);
                        // A retired cancelled scope is not a failed workload.
                        // Preserve the existing worker cancellation code before
                        // recording the separate clock-stage failure outcome.
                        CheckCancellation();
                        if (!clockStageSucceeded)
                        {
                            error(27);
                            admitView = false;
                        }
                        if (!admitView || CheckCancellation())
                        {
                            after = clockOwner;
                            afterValid = clockOwnerValid;
                            std::copy(after.begin(), after.end(), report.begin() + FrontendStartupReport::AfterView);
                            PublishFrontendStartup(report);
                            admitView = false;
                        }
                    }
                    if (admitView)
                    {
                        viewCalled = true;
                        try
                        {
                            if (persistentFrontendRequested)
                            {
                                DefinitivePersistentFrontend::RequestV1 request;
                                request.generation = persistentHostLease.Generation();
                                request.cancelled = InputCancelled;
                                request.publish = PublishPersistentFrontend;
                                request.heartbeat = PersistentHostFresh;
                                viewStatus = persistentView(handle, &request);
                                persistentHostLease.Revoke();
                            }
                            else
                                viewStatus = view(handle, DefinitiveGuiFrontendInputReference::MaximumDurationMs, InputCancelled, PublishInput);
                            values[9] = static_cast<std::uint32_t>(viewStatus);
                        }
                        catch (...)
                        {
                            error(23);
                        }
                        afterValid = capture(after, FrontendStartupReport::AfterView);
                        if (!afterValid)
                            error(22);
                        if (viewStatus != Status::Ok && !(persistentFrontendRequested && viewStatus == Status::Stopped))
                        {
                            if (!CheckCancellation())
                                error(23);
                        }
                    }
                }
            }
        }
        catch (...)
        {
            if (clockBeforeFrontendRequested && clockInitializationInvoked)
                WaitForClockRetirement();
            error(handle ? 22u : 21u);
        }

        // Cancellation and failed workloads never bypass owner-thread teardown.
        // A failed/Busy cleanup retains both its opaque owner and Core's code.
        if (!handle)
            return;
        CheckCancellation();
        try
        {
            const auto cleanupStarted = GetTickCount64();
            const auto waitForOwner = [&]
            {
                CheckCancellation();
                const auto elapsed = GetTickCount64() - cleanupStarted;
                if (elapsed >= DefinitiveGuiStartupReference::CleanupDeadlineMs)
                    error(24, true);
                // Busy owns a live same-thread retry, even after cancellation or
                // a workload deadline. Ending this worker would strand that API.
                Sleep(elapsed >= DefinitiveGuiStartupReference::CleanupDeadlineMs ? DefinitiveGuiStartupReference::CleanupBackoffMs : 50);
            };
            Status shutdownStatus = Status::Busy;
            bool   stoppedValid = false;
            do
            {
                shutdownStatus = shutdown(handle);
                values[10] = static_cast<std::uint32_t>(shutdownStatus);
                stoppedValid = capture(stopped, FrontendStartupReport::Stopped);
                if (!stoppedValid)
                {
                    error(24, true);
                    frontendQuarantined = true;
                    return;
                }
                if (shutdownStatus == Status::Busy)
                    waitForOwner();
            } while (shutdownStatus == Status::Busy);
            const auto stoppedProof = [&]
            {
                return stoppedValid && shutdownStatus == Status::Ok && stopped[Startup::Phase] == 4 && stopped[Startup::CoreRetained] == 1 &&
                       !stopped[Startup::BackendStarted] && !stopped[Startup::ExactImageBound] && !stopped[Startup::CapabilitiesLow] &&
                       !stopped[Startup::CapabilitiesHigh] && noFrontend(stopped) && !stopped[Startup::CleanupPending] &&
                       stopped[Startup::CleanupComplete] == 1 && !stopped[Startup::ReleaseFault] && stopped[Startup::LastViewCleanupComplete] == 1 &&
                       stopped[Startup::ViewRuns] <= 1 && (!afterValid || stopped[Startup::ViewRuns] == after[Startup::ViewRuns]) &&
                       (!initialValid || stopped[Startup::Pulses] >= initial[Startup::Pulses]) &&
                       (!afterValid || stopped[Startup::Pulses] >= after[Startup::Pulses]);
            };
            const bool stoppedClean = stoppedProof();
            const bool noView = initialValid && !initial[Startup::ViewRuns] && (!viewCalled || (afterValid && !after[Startup::ViewRuns])) &&
                                !stopped[Startup::ViewRuns] && initial[Startup::LastViewCleanupComplete] == 1 &&
                                (!viewCalled || after[Startup::LastViewCleanupComplete] == 1) && noFrontend(initial) && (!viewCalled || noFrontend(after));
            bool inputClean = noView;
            bool workload = false;
            if (!noView && stoppedClean)
            {
                DefinitiveGuiFrontendInputReference::Snapshot sample{};
                bool                                          available = false;
                if (persistentFrontendRequested)
                {
                    available = persistentFrontendAvailable.load();
                    // The sole publishing worker has returned from Run here;
                    // its atomic values are final and cannot race another writer.
                    for (std::size_t index = 0; index < sample.size(); ++index)
                        sample[index] = publishedPersistentFrontend[index].load();
                }
                else
                {
                    AcquireSRWLockShared(&inputLock);
                    sample = publishedGuiFrontendInput;
                    available = guiFrontendInputAvailable;
                    ReleaseSRWLockShared(&inputLock);
                }
                inputClean = FrontendInputRetired(sample, available);
                if (persistentFrontendRequested)
                {
                    namespace Persistent = DefinitivePersistentFrontend;
                    const auto scalar = [&](std::uint32_t field) { return publishedPersistentFrontend[field].load(); };
                    inputClean = inputClean && !sample[DefinitiveGuiSurfaceReference::DisplayDuration] && scalar(Persistent::PersistentVersion) == 1 &&
                                 scalar(Persistent::SessionGenerationLow) == persistentHostLease.Generation() && !scalar(Persistent::SessionGenerationHigh) &&
                                 scalar(Persistent::HostLeaseFresh) <= 1 && !scalar(Persistent::NativeLeaseActive) &&
                                 scalar(Persistent::NativeLeaseRevoked) <= 1 && scalar(Persistent::NativeLeaseReason) <= 6 &&
                                 (!(scalar(Persistent::NativeLeaseGenerationLow) || scalar(Persistent::NativeLeaseGenerationHigh)) ||
                                  (scalar(Persistent::NativeLeaseRevoked) == 1 && scalar(Persistent::NativeLeaseReason) != 0)) &&
                                 scalar(Persistent::HostHeartbeatBudgetMs) == Persistent::HostHeartbeatMs &&
                                 scalar(Persistent::NativeHeartbeatBudgetMs) == 500 && scalar(Persistent::StopReason) >= 1 &&
                                 scalar(Persistent::StopReason) <= 5;
                    for (std::uint32_t field = Persistent::Reserved270; field < Persistent::FieldCount; ++field)
                        inputClean = inputClean && !scalar(field);
                }
                // Retirement alone also succeeds after expiry or an early close.
                // Certify a persistent workload only when a fresh host reaches
                // an actual GUI close beyond the historical bounded duration.
                workload =
                    inputClean && sample[DefinitiveGuiStartupReference::WorkloadSucceeded] == 1 &&
                    sample[DefinitiveGuiStartupReference::WorkloadComplete] == 1 && !sample[DefinitiveGuiStartupReference::CleanupDeadlineExceeded] &&
                    (!persistentFrontendRequested || (sample[DefinitiveGuiFrontendInputReference::NativeActiveMs] > 35000 &&
                                                      publishedPersistentFrontend[DefinitivePersistentFrontend::HostLeaseFresh].load() == 1 &&
                                                      (sample[DefinitiveGuiFrontendInputReference::GuiCloseRequested] == 1 ||
                                                       sample[DefinitiveGuiFrontendInputReference::Frontend + DefinitiveFrontend::CloseRequested] == 1) &&
                                                      publishedPersistentFrontend[DefinitivePersistentFrontend::StopReason].load() == 1 &&
                                                      publishedPersistentFrontend[DefinitivePersistentFrontend::NativeLeaseReason].load() == 5 &&
                                                      (publishedPersistentFrontend[DefinitivePersistentFrontend::NativeLeaseGenerationLow].load() ||
                                                       publishedPersistentFrontend[DefinitivePersistentFrontend::NativeLeaseGenerationHigh].load()) &&
                                                      (publishedPersistentFrontend[DefinitivePersistentFrontend::NativeLeaseRenewalsLow].load() ||
                                                       publishedPersistentFrontend[DefinitivePersistentFrontend::NativeLeaseRenewalsHigh].load())));
            }
            if (!stoppedClean || !inputClean)
            {
                error(24, true);
                frontendQuarantined = true;
                return;
            }
            // The GUI borrows the same completed backend; it cannot silently
            // replace the capability set proven at clock retirement.
            const bool retainedAfterView =
                afterValid && after[Startup::Phase] == 2 && after[Startup::CoreRetained] == 1 && after[Startup::BackendStarted] == 1 &&
                after[Startup::ExactImageBound] == 1 && noFrontend(after) && !after[Startup::CleanupPending] && !after[Startup::CleanupComplete] &&
                !after[Startup::ReleaseFault] && after[Startup::ViewRuns] == 1 && after[Startup::LastViewCleanupComplete] == 1 &&
                (!clockBeforeFrontendRequested ||
                 (clockStageSucceeded && (static_cast<std::uint64_t>(after[Startup::CapabilitiesLow]) |
                                          (static_cast<std::uint64_t>(after[Startup::CapabilitiesHigh]) << 32)) == clockOwnerCapabilities));
            if (viewStatus == Status::Ok && (!workload || !retainedAfterView))
                error(23);
            Status releaseStatus = Status::Busy;
            do
            {
                try
                {
                    releaseStatus = release(handle);
                }
                catch (...)
                {
                    error(25, true);
                    frontendQuarantined = true;
                    return;
                }
                if (releaseStatus == Status::Busy)
                {
                    stoppedValid = capture(stopped, FrontendStartupReport::Stopped);
                    if (!stoppedProof())
                    {
                        error(24, true);
                        frontendQuarantined = true;
                        return;
                    }
                    waitForOwner();
                }
            } while (releaseStatus == Status::Busy);
            if (releaseStatus != Status::Ok)
            {
                error(25, true);
                frontendQuarantined = true;
                return;
            }
            handle = nullptr;
            CheckCancellation();
            if (!values[16] && clockStageSucceeded && viewStatus == Status::Ok && workload && retainedAfterView)
                values[17] = 1;
        }
        catch (...)
        {
            error(24, true);
            frontendQuarantined = true;
        }
    }

    void CoreStartupReference::Run() noexcept
    {
        values = {};
        if (frontendRuntimeRequested)
        {
            runtimeReport[FrontendRuntimeReport::WorkerPhase] = static_cast<std::uint32_t>(FrontendRuntimeReport::Phase::Starting);
            runtimeReport[FrontendRuntimeReport::LegacyModulesBefore] = LegacyRendererModules();
            PublishRuntime();
        }
        values[8] = values[9] = values[10] = 0xffffffffu;
        if (frontendStartupRequested || persistentFrontendRequested)
        {
            values[8] = 0;
            values[9] = values[10] = static_cast<std::uint32_t>(DefinitiveFrontendStartup::Status::Stopped);
        }
        else
        {
            values[1] = 0xffffffffu;
            values[7] = 1;
        }  // No adapter has been started by this attempt yet.
        values[11] = LegacyRendererModules();
        values[14] = GetModuleHandleW(L"game_de.dll") ? 1u : 0u;
        HMODULE core = nullptr;
        try
        {
            const auto run = [&]()
            {
                if (CheckCancellation())
                    return;
                if (GetModuleHandleW(L"core.dll") || values[14])
                {
                    values[16] = 2;
                    return;
                }
                HMODULE                    self = nullptr;
                std::array<wchar_t, 32768> ownPath{}, gamePath{};
                if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                        reinterpret_cast<LPCWSTR>(address), &self))
                {
                    values[16] = 3;
                    return;
                }
                const auto ownLength = GetModuleFileNameW(self, ownPath.data(), static_cast<DWORD>(ownPath.size()));
                const auto gameLength = GetModuleFileNameW(nullptr, gamePath.data(), static_cast<DWORD>(gamePath.size()));
                if (!ownLength || ownLength >= ownPath.size() || !gameLength || gameLength >= gamePath.size())
                {
                    values[16] = 3;
                    return;
                }
                const auto mtaDirectory = std::filesystem::path(std::wstring(ownPath.data(), ownLength)).parent_path();
                const auto gtaDirectory = std::filesystem::path(std::wstring(gamePath.data(), gameLength)).parent_path();
                if (CheckCancellation())
                    return;
                core = LoadLibraryExW((mtaDirectory / L"core.dll").c_str(), nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
                if (!core)
                {
                    // Preserve the loader error immediately; cleanup APIs may
                    // replace it before the script receives this snapshot.
                    values[18] = GetLastError();
                    values[16] = 4;
                    return;
                }
                if (CheckCancellation())
                    return;
                if (frontendRuntimeRequested)
                {
                    RunFrontendRuntime(core, mtaDirectory.wstring(), gtaDirectory.wstring());
                    return;
                }
                if (frontendStartupRequested || persistentFrontendRequested)
                {
                    RunFrontendStartup(core, mtaDirectory.wstring(), gtaDirectory.wstring());
                    return;
                }
                using Version = std::uint32_t (*)();
                using Read = std::uint32_t (*)(std::uint32_t);
                using SetDirectory = void (*)(const wchar_t*, std::size_t);
                using Initialize = int (*)();
                const auto version = reinterpret_cast<Version>(GetProcAddress(core, "MTA_DEStartupApiVersion"));
                const auto read = reinterpret_cast<Read>(GetProcAddress(core, "MTA_ReadDEStartupState"));
                const auto setMta = reinterpret_cast<SetDirectory>(GetProcAddress(core, "SetMTADirectory"));
                const auto setGta = reinterpret_cast<SetDirectory>(GetProcAddress(core, "SetGTADirectory"));
                const auto initialize = reinterpret_cast<Initialize>(GetProcAddress(core, "InitializeCore"));
                if (!version || !read || !setMta || !setGta || !initialize)
                {
                    values[16] = 5;
                    return;
                }
                if (version() != 1)
                {
                    values[16] = 6;
                    return;
                }
                values[8] = static_cast<std::uint32_t>(initialize());
                const auto mta = mtaDirectory.wstring(), gta = gtaDirectory.wstring();
                if (CheckCancellation())
                    return;
                setMta(mta.c_str(), mta.size());
                values[9] = static_cast<std::uint32_t>(initialize());
                if (CheckCancellation())
                    return;
                setGta(gta.c_str(), gta.size());
                if (inputRequested)
                {
                    using Configure = std::uint32_t (*)(std::uint32_t, DefinitiveInputReference::CancelCallback, DefinitiveInputReference::PublishCallback);
                    using ConfigureRenderV2 =
                        std::uint32_t (*)(std::uint32_t, std::uint32_t, DefinitiveInputReference::CancelCallback, DefinitiveInputReference::PublishCallback);
                    using ConfigureFrontend = std::uint32_t (*)(std::uint32_t, std::uint32_t, DefinitiveInputReference::CancelCallback,
                                                                DefinitiveInputReference::PublishCallback, DefinitiveGuiFrontendReference::ControlCallback);
                    const auto inputVersion =
                        reinterpret_cast<Version>(GetProcAddress(core, applicationFrameRequested           ? "MTA_DEApplicationFrameReferenceApiVersion"
                                                                       : clockSetRequested                 ? "MTA_DEClockSetReferenceApiVersion"
                                                                       : clockRequested                    ? "MTA_DEClockReferenceApiVersion"
                                                                       : guiFrontendInputRequested         ? "MTA_DEGuiFrontendInputReferenceApiVersion"
                                                                       : guiFrontendRequested              ? "MTA_DEGuiFrontendReferenceApiVersion"
                                                                       : guiSurfaceRequested               ? "MTA_DEGuiSurfaceReferenceApiVersion"
                                                                       : guiRequested                      ? "MTA_DEGuiStartupReferenceApiVersion"
                                                                       : renderRequested && renderApi == 6 ? "MTA_DERenderReferenceApiVersionV6"
                                                                       : renderRequested && renderApi == 5 ? "MTA_DERenderReferenceApiVersionV5"
                                                                       : renderRequested && renderApi == 4 ? "MTA_DERenderReferenceApiVersionV4"
                                                                       : renderRequested && renderApi == 3 ? "MTA_DERenderReferenceApiVersionV3"
                                                                       : renderRoute                       ? "MTA_DERenderReferenceApiVersionV2"
                                                                       : renderRequested                   ? "MTA_DERenderReferenceApiVersion"
                                                                       : movementRequested                 ? "MTA_DEMovementReferenceApiVersion"
                                                                                                           : "MTA_DEInputReferenceApiVersion"));
                    const auto configure =
                        reinterpret_cast<Configure>(GetProcAddress(core, applicationFrameRequested   ? "MTA_ConfigureDEApplicationFrameReference"
                                                                         : clockSetRequested         ? "MTA_ConfigureDEClockSetReference"
                                                                         : clockRequested            ? "MTA_ConfigureDEClockReference"
                                                                         : guiFrontendInputRequested ? "MTA_ConfigureDEGuiFrontendInputReference"
                                                                         : guiSurfaceRequested       ? "MTA_ConfigureDEGuiSurfaceReference"
                                                                         : guiRequested              ? "MTA_ConfigureDEGuiStartupReference"
                                                                         : renderRequested           ? "MTA_ConfigureDERenderReference"
                                                                         : movementRequested         ? "MTA_ConfigureDEMovementReference"
                                                                                                     : "MTA_ConfigureDEInputReference"));
                    const auto configureV2 = reinterpret_cast<ConfigureRenderV2>(GetProcAddress(core, renderApi == 6   ? "MTA_ConfigureDERenderReferenceV6"
                                                                                                      : renderApi == 5 ? "MTA_ConfigureDERenderReferenceV5"
                                                                                                      : renderApi == 4 ? "MTA_ConfigureDERenderReferenceV4"
                                                                                                      : renderApi == 3 ? "MTA_ConfigureDERenderReferenceV3"
                                                                                                                       : "MTA_ConfigureDERenderReferenceV2"));
                    const auto configureFrontend = reinterpret_cast<ConfigureFrontend>(GetProcAddress(core, "MTA_ConfigureDEGuiFrontendReference"));
                    const auto expectedVersion = applicationFrameRequested           ? DefinitiveApplicationFrameReference::ApiVersion
                                                 : clockSetRequested                 ? DefinitiveClockSetReference::ApiVersion
                                                 : clockRequested                    ? DefinitiveClockReference::ApiVersion
                                                 : guiFrontendInputRequested         ? DefinitiveGuiFrontendInputReference::ApiVersion
                                                 : guiFrontendRequested              ? DefinitiveGuiFrontendReference::ApiVersion
                                                 : guiSurfaceRequested               ? DefinitiveGuiSurfaceReference::ApiVersion
                                                 : guiRequested                      ? DefinitiveGuiStartupReference::ApiVersion
                                                 : renderRequested && renderApi == 6 ? DefinitiveRenderReference::ApiVersionV6
                                                 : renderRequested && renderApi == 5 ? DefinitiveRenderReference::ApiVersionV5
                                                 : renderRequested && renderApi == 4 ? DefinitiveRenderReference::ApiVersionV4
                                                 : renderRequested && renderApi == 3 ? DefinitiveRenderReference::ApiVersionV3
                                                 : renderRoute                       ? DefinitiveRenderReference::ApiVersionV2
                                                 : renderRequested                   ? DefinitiveRenderReference::ApiVersion
                                                                                     : DefinitiveInputReference::ApiVersion;
                    if (!inputVersion ||
                        (guiFrontendRequested ? !configureFrontend
                         : renderRoute        ? !configureV2
                                              : !configure) ||
                        inputVersion() != expectedVersion)
                    {
                        values[16] = 13;
                        return;
                    }
                    inputOwner = this;
                    const auto duration = applicationFrameRequested   ? DefinitiveApplicationFrameReference::MaximumDurationMs
                                          : clockSetRequested         ? DefinitiveClockSetReference::MaximumDurationMs
                                          : clockRequested            ? DefinitiveClockReference::MaximumDurationMs
                                          : guiFrontendInputRequested ? DefinitiveGuiFrontendInputReference::MaximumDurationMs
                                          : guiFrontendRequested      ? DefinitiveGuiFrontendReference::MaximumDurationMs
                                          : guiSurfaceRequested       ? DefinitiveGuiSurfaceReference::MaximumDurationMs
                                          : guiRequested              ? DefinitiveGuiStartupReference::MaximumDurationMs
                                          : renderRequested           ? DefinitiveRenderReference::MaximumDurationMs
                                          : movementRequested         ? DefinitiveMovementReference::MaximumDurationMs
                                                                      : DefinitiveInputReference::MaximumDurationMs;
                    const auto publish = applicationFrameRequested ? PublishApplicationFrame
                                         : clockSetRequested       ? PublishClockSet
                                         : clockRequested          ? PublishClock
                                                                   : PublishInput;
                    if ((guiFrontendRequested ? configureFrontend(DefinitiveGuiFrontendReference::ApiVersion, duration, InputCancelled, publish, ControlInput)
                         : renderRoute        ? configureV2(duration, renderRoute, InputCancelled, publish)
                                              : configure(duration, InputCancelled, publish)) != 1)
                    {
                        values[16] = 14;
                        return;
                    }
                }
                clockInitializationInvoked = clockRequested || clockSetRequested || applicationFrameRequested;
                try
                {
                    values[10] = static_cast<std::uint32_t>(initialize());
                    // The real diagnostic export rejects foreign threads. All
                    // Core calls, including readback and unload, stay on this worker.
                    for (std::uint32_t field = 0; field < 8; ++field)
                        values[field] = read(field);
                }
                catch (...)
                {
                    // A faulty Core can return by exception while native
                    // cleanup still retains a peer inside its allocator. Keep
                    // the enclosing directory/path objects alive as well as
                    // Core and TLS; unwinding them before retirement may free
                    // heap blocks through a lock that stopped peer owns.
                    if (clockRequested || clockSetRequested || applicationFrameRequested)
                        WaitForClockRetirement();
                    throw;
                }
                if (clockRequested || clockSetRequested || applicationFrameRequested)
                    WaitForClockRetirement();
                if (inputRequested && !clockRequested && !clockSetRequested && !applicationFrameRequested)
                {
                    // Native peer holds and GUI callbacks have ended. Wait for
                    // a final independent control read before taking the cache
                    // lock, so script capture contention cannot force quarantine.
                    DefinitiveGuiFrontendReference::ControlSnapshot finalControl{};
                    const bool                                      finalControlAvailable =
                        !guiFrontendRequested || frontendControl.CaptureFinalOnWorker(GetCurrentThreadId(), GetTickCount64(), finalControl);
                    AcquireSRWLockShared(&inputLock);
                    const auto complete = [](const auto& sample)
                    {
                        return sample[DefinitiveInputReference::Version] == DefinitiveInputReference::ApiVersion &&
                               sample[DefinitiveInputReference::Phase] == 2 && sample[DefinitiveInputReference::InputCleanupComplete] == 1;
                    };
                    const auto renderEnded = [](const auto& sample, std::uint32_t expected)
                    {
                        using namespace DefinitiveRenderReference;
                        return sample[DefinitiveRenderReference::Version] == expected && sample[Phase] == 2 && sample[CleanupComplete] == 1 &&
                               !sample[CleanupPending] && !sample[NativeCleanupPending] && !sample[CleanupDeadlineExceeded] &&
                               (sample[Reason] == static_cast<std::uint32_t>(StopReason::TimeBudget) ||
                                sample[Reason] == static_cast<std::uint32_t>(StopReason::Cancelled));
                    };
                    // Completing a diagnostic lease is separate from observing
                    // a callback. Even a zero-callback run must end and unload
                    // safely, with the requested protocol/route still intact.
                    const auto explicitRouteEnded = [&](const auto& sample, bool available, std::uint32_t version)
                    {
                        using namespace DefinitiveRenderReference;
                        return available && renderEnded(sample, version) && sample[RequestedRoute] == renderRoute &&
                               (!sample[ObservedRoute] || sample[ObservedRoute] == renderRoute) &&
                               sample[PresenceKnownMask] == (sample[ObservedRoute] ? (renderRoute == 2 ? 19u : 15u) : 0u);
                    };
                    const bool renderComplete =
                        renderApi == 6   ? explicitRouteEnded(publishedRenderV6, renderV6Available, DefinitiveRenderReference::ApiVersionV6)
                        : renderApi == 5 ? explicitRouteEnded(publishedRenderV5, renderV5Available, DefinitiveRenderReference::ApiVersionV5)
                        : renderApi == 4 ? explicitRouteEnded(publishedRenderV4, renderV4Available, DefinitiveRenderReference::ApiVersionV4)
                        : renderApi == 3 ? explicitRouteEnded(publishedRenderV3, renderV3Available, DefinitiveRenderReference::ApiVersionV3)
                        : renderRoute    ? renderV2Available && renderEnded(publishedRenderV2, DefinitiveRenderReference::ApiVersionV2) &&
                                            publishedRenderV2[DefinitiveRenderReference::RequestedRoute] == renderRoute &&
                                            (publishedRenderV2[DefinitiveRenderReference::ObservedRoute] == 0 ||
                                             publishedRenderV2[DefinitiveRenderReference::ObservedRoute] == renderRoute) &&
                                            publishedRenderV2[DefinitiveRenderReference::PresenceKnownMask] ==
                                                (publishedRenderV2[DefinitiveRenderReference::ObservedRoute] ? (renderRoute == 2 ? 19u : 15u) : 0u)
                                      : renderAvailable && renderEnded(publishedRender, DefinitiveRenderReference::ApiVersion);
                    const bool guiComplete =
                        guiAvailable && publishedGui[DefinitiveGuiStartupReference::Version] == DefinitiveGuiStartupReference::ApiVersion &&
                        publishedGui[DefinitiveGuiStartupReference::Phase] == 2 && publishedGui[DefinitiveGuiStartupReference::CleanupComplete] == 1 &&
                        !publishedGui[DefinitiveGuiStartupReference::CleanupPending] && !publishedGui[DefinitiveGuiStartupReference::CleanupDeadlineExceeded] &&
                        !publishedGui[DefinitiveGuiStartupReference::ObserverPendingCleanup] && !publishedGui[DefinitiveGuiStartupReference::ModuleRetained] &&
                        !publishedGui[DefinitiveGuiStartupReference::SessionRetained] && !publishedGui[DefinitiveGuiStartupReference::DeviceRetained] &&
                        !publishedGui[DefinitiveGuiStartupReference::DeviceReleaseFault];
                    // Core cannot unload while a child class, composition tree
                    // or pending resize still belongs to its surface owner.
                    const bool surfaceComplete = guiSurfaceAvailable &&
                                                 publishedGuiSurface[DefinitiveGuiStartupReference::Version] == DefinitiveGuiSurfaceReference::ApiVersion &&
                                                 publishedGuiSurface[DefinitiveGuiStartupReference::Phase] == 2 &&
                                                 publishedGuiSurface[DefinitiveGuiStartupReference::CleanupComplete] == 1 &&
                                                 publishedGuiSurface[DefinitiveGuiStartupReference::ObserverEnded] == 1 &&
                                                 publishedGuiSurface[DefinitiveGuiStartupReference::GuiCleanupComplete] == 1 &&
                                                 !publishedGuiSurface[DefinitiveGuiStartupReference::GuiShutdownStatus] &&
                                                 !publishedGuiSurface[DefinitiveGuiStartupReference::CleanupPending] &&
                                                 !publishedGuiSurface[DefinitiveGuiStartupReference::CleanupDeadlineExceeded] &&
                                                 !publishedGuiSurface[DefinitiveGuiStartupReference::ObserverPendingCleanup] &&
                                                 !publishedGuiSurface[DefinitiveGuiStartupReference::ModuleRetained] &&
                                                 !publishedGuiSurface[DefinitiveGuiStartupReference::SessionRetained] &&
                                                 !publishedGuiSurface[DefinitiveGuiStartupReference::DeviceRetained] &&
                                                 !publishedGuiSurface[DefinitiveGuiStartupReference::DeviceReleaseFault] &&
                                                 !publishedGuiSurface[DefinitiveGuiSurfaceReference::ChildRetained] &&
                                                 !publishedGuiSurface[DefinitiveGuiSurfaceReference::QueueRetained] &&
                                                 !publishedGuiSurface[DefinitiveGuiSurfaceReference::SurfaceRetained] &&
                                                 !publishedGuiSurface[DefinitiveGuiSurfaceReference::ResizePending] &&
                                                 publishedGuiSurface[DefinitiveGuiSurfaceReference::WindowCleanupComplete] == 1 &&
                                                 publishedGuiSurface[DefinitiveGuiSurfaceReference::CompositionCleanupComplete] == 1 &&
                                                 publishedGuiSurface[DefinitiveGuiSurfaceReference::ClassUnregistered] == 1;
                    const auto frontendComplete = [&]
                    {
                        using namespace DefinitiveGuiFrontendReference;
                        using namespace DefinitiveGuiStartupReference;
                        const auto& sample = publishedGuiFrontend;
                        const auto& control = finalControl;
                        // Independently read the mailbox: Core cannot turn an
                        // unrestored script obligation into an unload claim.
                        if (!guiFrontendAvailable || !finalControlAvailable)
                            return false;
                        const bool controlComplete = control[ControlField::Requested]
                                                         ? control[ControlField::Revoked] == 1 && control[ControlField::Detached] == 1 &&
                                                               control[ControlField::Restored] == 1 && sample[ControlRequested] == 1 &&
                                                               sample[ControlGeneration] == control[ControlField::Generation] && sample[ControlRestored] == 1 &&
                                                               sample[ControlInputDetached] == 1 && sample[ControlRevoked] == 1 && !sample[ControlFresh] &&
                                                               sample[ControlAcknowledged] == control[ControlField::Acknowledged] &&
                                                               sample[ControlFault] == control[ControlField::Fault]
                                                         : std::all_of(sample.begin() + ControlGeneration, sample.begin() + ControlFault + 1,
                                                                       [](std::uint32_t value) { return value == 0; });
                        const bool frontendRetired =
                            sample[Frontend + DefinitiveFrontend::Version] == 0
                                ? !sample[FrontendStarted] && !sample[FrontendStopped] &&
                                      std::all_of(sample.begin() + Frontend, sample.begin() + Frontend + DefinitiveFrontend::FieldCount,
                                                  [](std::uint32_t value) { return value == 0; })
                                : sample[Frontend + DefinitiveFrontend::Version] == DefinitiveFrontend::ApiVersion &&
                                      sample[Frontend + DefinitiveFrontend::Phase] == 4 && !sample[Frontend + DefinitiveFrontend::InputRequested] &&
                                      !sample[Frontend + DefinitiveFrontend::ReleaseFault] && sample[Frontend + DefinitiveFrontend::CleanupComplete] == 1 &&
                                      !sample[Frontend + DefinitiveFrontend::CleanupPending] && !sample[Frontend + DefinitiveFrontend::ServicesRetained] &&
                                      !sample[Frontend + DefinitiveFrontend::GuiBorrowed] && sample[FrontendStopped] == 1;
                        const bool inputRetired =
                            sample[InputDetached] == 1 || (!sample[DefinitiveGuiSurfaceReference::ChildCreated] && !sample[FrontendStarted] &&
                                                           !sample[InputEventsLow] && !sample[InputEventsHigh]);
                        // A missed cleanup deadline remains workload evidence;
                        // eventual proven retirement must still release Core.
                        return sample[DefinitiveGuiStartupReference::Version] == DefinitiveGuiFrontendReference::ApiVersion && sample[Phase] == 2 &&
                               sample[CleanupComplete] == 1 && sample[ObserverEnded] == 1 && sample[GuiCleanupComplete] == 1 && !sample[GuiShutdownStatus] &&
                               !sample[CleanupPending] && !sample[ObserverPendingCleanup] && !sample[ModuleRetained] && !sample[SessionRetained] &&
                               !sample[DeviceRetained] && !sample[DeviceReleaseFault] && !sample[DefinitiveGuiSurfaceReference::ChildRetained] &&
                               !sample[DefinitiveGuiSurfaceReference::QueueRetained] && !sample[DefinitiveGuiSurfaceReference::SurfaceRetained] &&
                               !sample[DefinitiveGuiSurfaceReference::ResizePending] && sample[DefinitiveGuiSurfaceReference::WindowCleanupComplete] == 1 &&
                               sample[DefinitiveGuiSurfaceReference::CompositionCleanupComplete] == 1 &&
                               sample[DefinitiveGuiSurfaceReference::ClassUnregistered] == 1 && frontendRetired && inputRetired && !sample[PressedKeys] &&
                               !sample[PressedButtons] && !sample[Frontend + DefinitiveFrontend::NetworkStarted] &&
                               !sample[Frontend + DefinitiveFrontend::GameplayReady] && !sample[Frontend + DefinitiveFrontend::FullMtaReady] &&
                               !sample[CameraCaptureVerified] && !sample[FullEngineInputReady] && controlComplete;
                    };
                    // Workload failure is separate from completing ownership
                    // cleanup. The GUI snapshot retains its own outcome facts.
                    const bool inputComplete = guiFrontendInputRequested ? FrontendInputRetired(publishedGuiFrontendInput, guiFrontendInputAvailable)
                                               : guiFrontendRequested    ? frontendComplete()
                                               : guiSurfaceRequested     ? surfaceComplete
                                               : guiRequested            ? guiComplete
                                               : renderRequested         ? renderComplete
                                               : movementRequested       ? movementAvailable && complete(publishedMovement) &&
                                                                         !publishedMovement[DefinitiveMovementReference::CleanupPending]
                                                                   : inputAvailable && complete(publishedInput);
                    ReleaseSRWLockShared(&inputLock);
                    if (!inputComplete)
                    {
                        values[16] = 15;
                        frontendQuarantined = guiFrontendRequested || guiFrontendInputRequested;
                        return;
                    }
                }
                if (CheckCancellation())
                    return;
                if (values[8] != 3 || values[9] != 4 || values[10] != 5)
                {
                    values[16] = 7;
                    return;
                }
                if (values[0] != 1 || values[1] != 5 || values[2] != 6 || values[3] != 97 || values[4] || values[5] || values[6] || values[7] != 1)
                {
                    values[16] = 8;
                    return;
                }
                if (applicationFrameRequested)
                {
                    DefinitiveApplicationFrameReference::Snapshot sample{};
                    // Retirement grants unload permission; successful real
                    // application/dictionary observations prove the workload.
                    values[17] = CopyApplicationFrameSnapshot(sample) && DefinitiveApplicationFrameReference::Completed(sample) ? 1u : 0u;
                }
                else if (clockSetRequested)
                {
                    DefinitiveClockSetReference::Snapshot sample{};
                    // Workload completion and unload permission remain
                    // independent: safe preflight refusal never proves Set.
                    values[17] = CopyClockSetSnapshot(sample) && DefinitiveClockSetReference::Completed(sample) ? 1u : 0u;
                }
                else if (clockRequested)
                {
                    DefinitiveClockReference::Snapshot sample{};
                    using namespace DefinitiveClockReference;
                    // Safe retirement grants unload permission independently
                    // from proof that the requested read-only workload ran.
                    values[17] = CopyClockSnapshot(sample) && Retired(sample) && sample[Reason] == static_cast<std::uint32_t>(StopReason::Completed) &&
                                         sample[WorkloadComplete] == 1 && sample[ReadOnlyVerified] == 1 && !sample[NotEntered] &&
                                         (sample[QualifiedCallbacksLow] || sample[QualifiedCallbacksHigh]) && sample[ReadAcknowledged] &&
                                         sample[NativeTimerProgressSamples] && !sample[SetsLow] && !sample[SetsHigh] && !sample[FreezesLow] &&
                                         !sample[FreezesHigh] && !sample[UnfreezesLow] && !sample[UnfreezesHigh] && !sample[ResetsLow] && !sample[ResetsHigh]
                                     ? 1u
                                     : 0u;
                }
                else
                    values[17] = 1;
            };
            run();
        }
        catch (...)
        {
            values[16] = 11;
        }
        // Initialize's numeric/exception result is not evidence that native
        // callbacks, admission or suspended peers have retired. Keep both Core
        // and this live publication context until an independent proof arrives.
        if (core && (clockRequested || clockSetRequested || clockBeforeFrontendRequested || applicationFrameRequested) && clockInitializationInvoked)
            WaitForClockRetirement();
        inputOwner = nullptr;
        // Cancellation never interrupts the loader or unloads Core from a
        // second thread. Completion is published only after this owned cleanup.
        persistentHostLease.Revoke();
        if (frontendRuntimeRequested)
        {
            FinishRuntime(core);
            return;
        }
        if ((guiFrontendRequested || guiFrontendInputRequested || persistentFrontendRequested) && core && values[16] == 11)
            frontendQuarantined = true;
        if (core && !frontendQuarantined && !clockQuarantined && !FreeLibrary(core))
            values[16] = 9;
        values[12] = LegacyRendererModules();
        values[13] = GetModuleHandleW(L"core.dll") ? 0u : 1u;
        values[15] = GetModuleHandleW(L"game_de.dll") ? 1u : 0u;
        if ((values[12] & ~values[11]) && !values[16])
            values[16] = 10;
        const auto expectedBackend =
            clockBeforeFrontendRequested ? ((clockAvailable.load() ? publishedClock[DefinitiveClockReference::BackendPinnedExpected].load() : 0u) |
                                            (guiFrontendInputAvailable ? publishedGuiFrontendInput[DefinitiveGuiStartupReference::BackendPinnedExpected] : 0u))
            : applicationFrameRequested && applicationFrameAvailable.load()
                ? publishedApplicationFrame[DefinitiveApplicationFrameReference::BackendPinnedExpected].load()
            : clockSetRequested && clockSetAvailable.load() ? publishedClockSet[DefinitiveClockSetReference::BackendPinnedExpected].load()
            : clockRequested && clockAvailable.load()       ? publishedClock[DefinitiveClockReference::BackendPinnedExpected].load()
            : persistentFrontendRequested && persistentFrontendAvailable.load()
                ? publishedPersistentFrontend[DefinitiveGuiStartupReference::BackendPinnedExpected].load()
            : guiFrontendInputRequested && guiFrontendInputAvailable ? publishedGuiFrontendInput[DefinitiveGuiStartupReference::BackendPinnedExpected]
            : guiFrontendRequested && guiFrontendAvailable           ? publishedGuiFrontend[DefinitiveGuiStartupReference::BackendPinnedExpected]
            : guiSurfaceRequested && guiSurfaceAvailable             ? publishedGuiSurface[DefinitiveGuiStartupReference::BackendPinnedExpected]
            : guiRequested && guiAvailable                           ? publishedGui[DefinitiveGuiStartupReference::BackendPinnedExpected]
            : renderRequested && renderApi == 6 && renderV6Available ? publishedRenderV6[DefinitiveRenderReference::BackendPinnedExpected]
            : renderRequested && renderApi == 5 && renderV5Available ? publishedRenderV5[DefinitiveRenderReference::BackendPinnedExpected]
            : renderRequested && renderApi == 4 && renderV4Available ? publishedRenderV4[DefinitiveRenderReference::BackendPinnedExpected]
            : renderRequested && renderApi == 3 && renderV3Available ? publishedRenderV3[DefinitiveRenderReference::BackendPinnedExpected]
            : renderRequested && renderRoute && renderV2Available    ? publishedRenderV2[DefinitiveRenderReference::BackendPinnedExpected]
            : renderRequested && renderAvailable                     ? publishedRender[DefinitiveRenderReference::BackendPinnedExpected]
            : movementRequested && movementAvailable                 ? publishedMovement[DefinitiveMovementReference::BackendPinnedExpected]
                                                                     : 0u;
        if ((!values[13] || values[15] != expectedBackend) && !values[16])
            values[16] = 9;
    }
}
