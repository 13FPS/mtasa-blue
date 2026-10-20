/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Retain a completed Core independently of a bounded frontend view
 *****************************************************************************/

#include "StdInc.h"
#include "DefinitiveFrontendStartup.h"
#include "DefinitiveFrontendClockReference.h"
#include "DefinitivePersistentFrontend.h"
#include "DefinitiveFrontendRuntime.h"
#include "DefinitiveFrontendBindingPersistence.h"
#include "DefinitiveLocalInputRuntime.h"
#include "DefinitiveStartupGameTime.h"

#if defined(_WIN64) && defined(MTA_WITH_DEFINITIVE_ADAPTER)

namespace DefinitiveFrontendStartup
{
    struct Controller
    {
        const DWORD                        owner = GetCurrentThreadId();
        CCore*                             core = nullptr;
        Snapshot                           state{};
        bool                               clockPending = false;
        bool                               lastClockRetired = true;
        const bool                         runtimeMode;
        const std::uint64_t                runtimeGeneration;
        bool                               runtimeStopping = false, runtimeStopped = false;
        DefinitiveFrontendRuntime::Failure runtimeFailure = DefinitiveFrontendRuntime::Failure::None;

        explicit Controller(bool runtime = false, std::uint64_t generation = 0) : runtimeMode(runtime), runtimeGeneration(generation)
        {
            state[Version] = ApiVersion;
            state[Phase] = 1;
            state[OwnerThread] = owner;
            state[LastViewCleanupComplete] = 1;
        }

        void Begin() noexcept
        {
            try
            {
                core = new CCore(CCore::FrontendStartupTag{});
                state[BackendStartAttempted] = 1;
                if (!core->m_DefinitiveAdapter.StartAdjacent(core))
                {
                    Fail(Failure::BackendStart);
                    return;
                }
                if (Pulse() != Status::Ok)
                    return;
                if (runtimeMode && core->EnableFrontendRuntimeServices() != Status::Ok)
                {
                    runtimeFailure = DefinitiveFrontendRuntime::Failure::ServicesStart;
                    Fail(Failure::RuntimeServices);
                    return;
                }
                state[Phase] = 2;
                core->m_frontendStartupAccepting = true;
            }
            catch (...)
            {
                // An allocated Core is deliberately retained on failure: its
                // backend may still require an owner-thread shutdown retry.
                Fail(core ? Failure::BackendStart : Failure::CoreConstruction);
            }
        }

        void Fail(Failure failure) noexcept
        {
            state[LastFailure] = static_cast<std::uint32_t>(failure);
            state[Phase] = 5;
            if (core)
                core->m_frontendStartupAccepting = false;
        }

        void RetainFailedView() noexcept
        {
            // A callback failure cannot transfer the completed Core out of its
            // controller. Keep the same admission and cleanup state as Busy.
            Fail(Failure::View);
            state[Phase] = 3;
            state[CleanupPending] = 1;
        }

        Status Pulse() noexcept
        {
            if (clockPending || !lastClockRetired)
                return Status::Busy;
            if (!core || !core->m_DefinitiveAdapter.IsStarted())
                return Status::Failed;
            try
            {
                if (core->m_DefinitiveAdapter.Pulse())
                {
                    if (state[Pulses] != UINT32_MAX)
                        ++state[Pulses];
                    return Status::Ok;
                }
            }
            catch (...)
            {
                // Consumer diagnostics can allocate after an adapter failure;
                // no C++ exception may escape the exported owner lifecycle.
            }
            Fail(Failure::BackendPulse);
            return Status::Failed;
        }

        Status Bind() noexcept
        {
            if (clockPending || !lastClockRetired)
                return Status::Busy;
            if (state[Phase] != 2)
                return state[Phase] == 5 ? Status::Failed : Status::Stopped;
            // A retired optional service can Reset the adapter and discard its
            // native reader. Never let the cached owner bit skip rebinding on
            // the next same-Core view or producer admission.
            if (state[ExactImageBound] &&
                (core->m_DefinitiveAdapter.GetCapabilities() & mta::game_de::NativePadBindingCapabilities) == mta::game_de::NativePadBindingCapabilities)
                return Status::Ok;
            state[ExactImageBound] = 0;
            try
            {
                if (core->m_DefinitiveAdapter.BindNativeInput())
                {
                    state[ExactImageBound] = 1;
                    return Status::Ok;
                }
            }
            catch (...)
            {
            }
            // Wrong-image binding does not invalidate a completed Core's
            // backend or its existing CPU/GUI services. No engine-facing
            // operation is admitted until an explicit bind succeeds.
            state[LastFailure] = static_cast<std::uint32_t>(Failure::InputBind);
            return Status::Failed;
        }

        Snapshot Read() const
        {
            auto result = state;
            result[CleanupPending] = result[CleanupPending] || clockPending || !lastClockRetired;
            result[CoreRetained] = core != nullptr;
            if (core)
            {
                const bool started = core->m_DefinitiveAdapter.IsStarted();
                result[BackendStarted] = started;
                const auto capabilities = core->m_DefinitiveAdapter.GetCapabilities();
                result[CapabilitiesLow] = static_cast<std::uint32_t>(capabilities);
                result[CapabilitiesHigh] = static_cast<std::uint32_t>(capabilities >> 32);
                result[NetworkStarted] = core->m_pNet != nullptr;
                result[FrontendRetained] = core->m_frontendResources != nullptr;
                if (core->m_frontendResources)
                {
                    const auto frontend = core->ReadFrontend();
                    result[FrontendPhase] = frontend[DefinitiveFrontend::Phase];
                    result[GuiBorrowed] = frontend[DefinitiveFrontend::GuiBorrowed];
                    result[ServicesRetained] = frontend[DefinitiveFrontend::ServicesRetained];
                    result[InputRequested] = frontend[DefinitiveFrontend::InputRequested];
                    result[CleanupPending] = result[CleanupPending] || frontend[DefinitiveFrontend::CleanupPending];
                }
            }
            // Adapter capabilities and a running frontend are diagnostic facts;
            // neither authorizes the missing CGame/network/multiplayer startup.
            return result;
        }

        DefinitiveStartupGameTime::Snapshot ReadGameTime() noexcept
        {
            namespace Time = DefinitiveStartupGameTime;
            Time::Snapshot result{};
            result[Time::Version] = Time::ApiVersion;
            result[Time::OwnerThread] = owner;
            result[Time::CoreRetained] = core != nullptr;
            result[Time::BackendStarted] = core && core->m_DefinitiveAdapter.IsStarted();
            result[Time::NativeStatus] = static_cast<std::uint32_t>(mta::game_de::Status::NotInitialized);
            // Use the same completed Core and adapter as GUI/input startup.
            // Reads neither reset those services nor acquire a clock-write
            // lease. A stopping owner must not reach back into its backend.
            if (state[Phase] != 2 || runtimeStopping || runtimeStopped || !result[Time::BackendStarted])
                return result;
            mta::game_de::GameTimeStateV1 native;
            const auto                    status = core->m_DefinitiveAdapter.ReadGameTimeState(native);
            result[Time::NativeStatus] = static_cast<std::uint32_t>(status);
            if (status == mta::game_de::Status::Ok)
            {
                result[Time::SampleAvailable] = 1;
                result[Time::NativeProducerThread] = native.producerThread;
                result[Time::Hour] = native.hour;
                result[Time::Minute] = native.minute;
                result[Time::SecondsRaw] = native.seconds;
                result[Time::NativeTimer] = native.nativeTimer;
                result[Time::LastMinuteChange] = native.lastMinuteChange;
                result[Time::MillisecondsPerGameMinute] = native.millisecondsPerGameMinute;
            }
            return result;
        }

        Status Shutdown() noexcept
        {
            // Even a stopped GUI controller still holds a borrowed Core until
            // its Release. Its opaque handle must never become a dangling view.
            if (clockPending || !lastClockRetired || (core && core->m_frontendResources) || !state[LastViewCleanupComplete])
                return Status::Busy;
            if (state[Phase] == 4)
                return Status::Ok;
            if (core)
                core->m_frontendStartupAccepting = false;
            state[Phase] = 3;
            state[CleanupPending] = 1;
            if (core)
            {
                core->m_DefinitiveAdapter.Stop();
                if (core->m_DefinitiveAdapter.IsStarted())
                {
                    state[LastFailure] = static_cast<std::uint32_t>(Failure::BackendStop);
                    return Status::Busy;
                }
            }
            state[ExactImageBound] = 0;
            state[CleanupPending] = 0;
            state[CleanupComplete] = 1;
            state[Phase] = 4;
            return Status::Ok;
        }

        Status Retire() noexcept
        {
            const auto stopped = Shutdown();
            if (stopped != Status::Ok)
                return stopped;
            // The completed Core owns no borrowed GUI or live adapter now. Its
            // existing frontend-only destructor clears the genuine singleton.
            delete core;
            core = nullptr;
            return Status::Ok;
        }

        DefinitiveFrontendRuntime::Snapshot ReadRuntime() const
        {
            namespace Runtime = DefinitiveFrontendRuntime;
            auto       result = core ? core->ReadFrontendRuntimeServices() : Runtime::Snapshot{};
            const auto startup = Read();
            std::copy(startup.begin(), startup.end(), result.begin());
            result[Runtime::RuntimeVersion] = Runtime::ApiVersion;
            result[Runtime::OwnerThread] = owner;
            result[Runtime::SessionGenerationLow] = static_cast<std::uint32_t>(runtimeGeneration);
            result[Runtime::SessionGenerationHigh] = static_cast<std::uint32_t>(runtimeGeneration >> 32);
            if (!core || state[Phase] == 5)
                result[Runtime::RuntimePhase] = static_cast<std::uint32_t>(Runtime::Phase::Failed);
            if (runtimeStopping)
            {
                result[Runtime::RuntimePhase] = static_cast<std::uint32_t>(Runtime::Phase::Stopping);
                result[Runtime::CleanupPending] = 1;
                result[Runtime::CleanupComplete] = 0;
            }
            if (runtimeStopped)
            {
                result[Runtime::RuntimePhase] = static_cast<std::uint32_t>(Runtime::Phase::Stopped);
                result[Runtime::CleanupPending] = 0;
                result[Runtime::CleanupComplete] = 1;
            }
            if (runtimeFailure != Runtime::Failure::None)
                result[Runtime::LastFailure] = static_cast<std::uint32_t>(runtimeFailure);
            return result;
        }

        Status ShutdownRuntime() noexcept
        {
            namespace Runtime = DefinitiveFrontendRuntime;
            // Even a stopped CPU view still borrows Core until Release. Parked
            // session services cannot be destroyed behind that opaque handle.
            if (clockPending || !lastClockRetired || (core && core->m_frontendResources) || !state[LastViewCleanupComplete])
                return Status::Busy;
            if (runtimeStopped)
                return Status::Ok;
            runtimeStopping = true;
            if (core)
            {
                core->m_frontendStartupAccepting = false;
                const auto status = core->StopFrontendRuntimeServices();
                if (status != Status::Ok)
                {
                    if (status != Status::Busy)
                        runtimeFailure = Runtime::Failure::ServicesStop;
                    state[Phase] = 3;
                    state[CleanupPending] = 1;
                    state[LastFailure] = static_cast<std::uint32_t>(Failure::RuntimeServices);
                    return status;
                }
            }
            // The actual CKeyBinds/CVars/XML owner retires before the backend.
            // A failed service or adapter stop keeps this same completed Core.
            const auto stopped = Shutdown();
            if (stopped != Status::Ok)
            {
                runtimeFailure = Runtime::Failure::BackendStop;
                return stopped;
            }
            runtimeStopping = false;
            runtimeStopped = true;
            return Status::Ok;
        }

        Status RetireRuntime() noexcept
        {
            const auto stopped = ShutdownRuntime();
            if (stopped != Status::Ok)
                return stopped;
            delete core;
            core = nullptr;
            return Status::Ok;
        }

        Status RunView(std::uint32_t durationMs, DefinitiveInputReference::CancelCallback cancel, DefinitiveInputReference::PublishCallback publish)
        {
            if (clockPending || !lastClockRetired)
                return Status::Busy;
            if (state[Phase] != 2)
                return state[Phase] == 5 ? Status::Failed : Status::Stopped;
            if (core->m_frontendResources)
                return Status::Busy;
            const auto bound = Bind();
            if (bound != Status::Ok)
                return bound;
            const auto pulsed = Pulse();
            if (pulsed != Status::Ok)
                return pulsed;
            DefinitiveInputReference::Request request;
            request.durationMs = durationMs;
            request.cancelled = cancel;
            request.publish = publish;
            request.guiFrontendInput = true;
            DefinitiveGuiFrontendInputReference::Snapshot result{};
            if (state[ViewRuns] != UINT32_MAX)
                ++state[ViewRuns];
            state[LastViewCleanupComplete] = 0;
            const auto status = core->RunFrontendStartupView(request, result);
            state[LastViewCleanupComplete] = result[DefinitiveGuiStartupReference::CleanupComplete];
            if (status != Status::Ok)
                state[LastFailure] = static_cast<std::uint32_t>(Failure::View);
            if (status == Status::Busy)
            {
                state[Phase] = 3;
                state[CleanupPending] = 1;
                core->m_frontendStartupAccepting = false;
            }
            // A normal view retirement leaves the completed Core and backend
            // alive. Only the separate Shutdown/Release owns their retirement.
            return status;
        }

        Status RunPersistent(const DefinitivePersistentFrontend::RequestV1& request)
        {
            if (clockPending || !lastClockRetired)
                return Status::Busy;
            if (state[Phase] != 2)
                return state[Phase] == 5 ? Status::Failed : Status::Stopped;
            if (core->m_frontendResources)
                return Status::Busy;
            if (Bind() != Status::Ok || Pulse() != Status::Ok)
                return Status::Failed;
            if (state[ViewRuns] != UINT32_MAX)
                ++state[ViewRuns];
            state[LastViewCleanupComplete] = 0;
            DefinitivePersistentFrontend::Snapshot result{};
            const auto                             status = core->RunPersistentFrontend(request, result);
            state[LastViewCleanupComplete] = result[DefinitiveGuiStartupReference::CleanupComplete];
            if (status != Status::Ok && status != Status::Stopped)
                state[LastFailure] = static_cast<std::uint32_t>(Failure::View);
            if (status == Status::Busy)
            {
                state[Phase] = 3;
                state[CleanupPending] = 1;
                core->m_frontendStartupAccepting = false;
            }
            return status;
        }

        Status RunRuntimeView(const DefinitivePersistentFrontend::RequestV1& request)
        {
            namespace Runtime = DefinitiveFrontendRuntime;
            if (runtimeStopping || runtimeStopped)
                return runtimeStopped ? Status::Stopped : Status::Busy;
            if (state[Phase] != 2)
                return state[Phase] == 5 ? Status::Failed : Status::Stopped;
            if (clockPending || !lastClockRetired || core->m_frontendResources || !state[LastViewCleanupComplete])
                return Status::Busy;
            const auto service = core->ReadFrontendRuntimeServices();
            const auto generation = std::uint64_t{service[Runtime::ViewGenerationLow]} | (std::uint64_t{service[Runtime::ViewGenerationHigh]} << 32);
            // A fresh view consumes one actual Core counter, including views
            // created through the existing direct CPU/GUI borrowing API.
            if (generation == UINT64_MAX || request.generation != generation + 1)
                return Status::InvalidArgument;
            core->m_frontendStartupAccepting = false;
            const auto localStopped = core->StopLocalInputRuntime(false);
            if (localStopped != Status::Ok)
                return localStopped;
            core->m_frontendStartupAccepting = core->LocalInputAllowsFrontend();
            const auto status = RunPersistent(request);
            if (status == Status::Busy)
                runtimeStopping = true;
            else if (status == Status::Failed && state[LastFailure] == static_cast<std::uint32_t>(Failure::View))
                runtimeFailure = Runtime::Failure::View;
            return status;
        }

        Status CheckLocalAdmission() const noexcept
        {
            if (!runtimeMode)
                return Status::InvalidArgument;
            if (runtimeStopped)
                return Status::Stopped;
            if (runtimeStopping || clockPending || !lastClockRetired)
                return Status::Busy;
            if (state[Phase] != 2 || !core || !core->m_DefinitiveAdapter.IsStarted())
                return state[Phase] == 5 ? Status::Failed : Status::Stopped;
            return core->m_frontendResources || !state[LastViewCleanupComplete] ? Status::Busy : Status::Ok;
        }

        void RefreshLocalBorrowGate() noexcept
        {
            if (core)
            {
                // A no-token native refusal can retire through Reset, which
                // clears the reader binding. The next same-Core service must
                // rebind instead of trusting its previous ExactImageBound bit.
                state[ExactImageBound] =
                    (core->m_DefinitiveAdapter.GetCapabilities() & mta::game_de::NativePadBindingCapabilities) == mta::game_de::NativePadBindingCapabilities;
                core->m_frontendStartupAccepting =
                    state[Phase] == 2 && !runtimeStopping && !runtimeStopped && !clockPending && lastClockRetired && core->LocalInputAllowsFrontend();
            }
        }

        Status ConfigureLocal(const DefinitiveLocalInputRuntime::RequestV1& request) noexcept
        {
            const auto admitted = CheckLocalAdmission();
            if (admitted != Status::Ok)
                return admitted;
            // The same operation lock pins Core through native admission and
            // cleanup. A direct GUI factory must refuse before allocating its
            // borrower while this distinct input owner is enabled or retained.
            core->m_frontendStartupAccepting = false;
            const auto configured = core->ConfigureLocalInputRuntime(request);
            RefreshLocalBorrowGate();
            return configured;
        }

        Status EnableBindingPersistence(const wchar_t* path, std::uint32_t chars) noexcept
        {
            const auto admitted = CheckLocalAdmission();
            if (admitted != Status::Ok)
                return admitted;
            if (!core->LocalInputAllowsFrontend())
                return Status::Busy;
            // Reuse the existing owner operation lock and borrowing gate. XML
            // loading can replace the retained manager only before a GUI/input
            // borrower is admitted; it does not create another service owner.
            core->m_frontendStartupAccepting = false;
            Status result = Status::Failed;
            try
            {
                result = core->EnableFrontendRuntimeBindingPersistence(path, chars);
            }
            catch (...)
            {
            }
            RefreshLocalBorrowGate();
            return result;
        }

        Status PulseLocal() noexcept
        {
            namespace Local = DefinitiveLocalInputRuntime;
            const auto admitted = CheckLocalAdmission();
            if (admitted != Status::Ok)
                return admitted;
            core->m_frontendStartupAccepting = false;
            const auto local = core->ReadLocalInputRuntime();
            if (local[Local::Enabled] && (Bind() != Status::Ok || Pulse() != Status::Ok))
            {
                const auto retired = core->StopLocalInputRuntime(false, Local::Failure::InputBind, true);
                RefreshLocalBorrowGate();
                return retired == Status::Ok ? Status::Failed : retired;
            }
            const auto result = core->PulseLocalInputRuntime();
            RefreshLocalBorrowGate();
            return result;
        }

        DefinitiveLocalInputRuntime::Snapshot ReadLocal() noexcept
        {
            namespace Local = DefinitiveLocalInputRuntime;
            if (core)
                return core->ReadLocalInputRuntime();
            Local::Snapshot absent{};
            absent[Local::Version] = Local::ApiVersion;
            absent[Local::Phase] = static_cast<std::uint32_t>(Local::State::Disabled);
            absent[Local::OwnerThread] = owner;
            absent[Local::CleanupComplete] = 1;
            return absent;
        }

        Status StopLocal(bool disable) noexcept
        {
            // Retirement remains callable after a failed backend pulse. A
            // historical workload error cannot strand its still-owned lease.
            if (!runtimeMode)
                return Status::InvalidArgument;
            if (!core)
                return Status::Stopped;
            if (clockPending || !lastClockRetired || core->m_frontendResources || !state[LastViewCleanupComplete])
                return Status::Busy;
            core->m_frontendStartupAccepting = false;
            const auto result = core->StopLocalInputRuntime(disable);
            RefreshLocalBorrowGate();
            return result;
        }

        Status CheckClockAdmission() const noexcept
        {
            if (clockPending || !lastClockRetired)
                return Status::Busy;
            if (state[Phase] != 2)
                return state[Phase] == 5 ? Status::Failed : Status::Stopped;
            if (!core || !core->m_DefinitiveAdapter.IsStarted())
                return Status::Failed;
            // A stopped frontend still borrows Core until its Release. Clock
            // ownership cannot overlap that GUI owner or an unretired view.
            return core->m_frontendResources || !state[LastViewCleanupComplete] || !core->LocalInputAllowsFrontend() ? Status::Busy : Status::Ok;
        }

        Status PrepareClock() noexcept
        {
            // Bind/Pulse can invoke a host callback before native Begin. Close
            // the separate frontend API first, while the startup lock pins Core.
            core->m_frontendStartupAccepting = false;
            auto prepared = Bind();
            if (prepared == Status::Ok)
                prepared = Pulse();
            if (prepared != Status::Ok && lastClockRetired && state[Phase] == 2)
                core->m_frontendStartupAccepting = true;
            return prepared;
        }

        void EnterClock() noexcept
        {
            clockPending = true;
            lastClockRetired = false;
        }

        void ClockRetired() noexcept
        {
            // Only the existing lifecycle's qualified native retirement can
            // reopen borrowing. A nonretired return retains Core, adapter and
            // its native callback context, without poisoning GUI view state.
            lastClockRetired = true;
            clockPending = false;
            // A refused Begin may have reset the adapter's native binding.
            // Keep the next same-Core view's Bind admission tied to the actual
            // backend rather than a stale successful pre-clock preparation.
            state[ExactImageBound] =
                (core->m_DefinitiveAdapter.GetCapabilities() & mta::game_de::NativePadBindingCapabilities) == mta::game_de::NativePadBindingCapabilities;
            core->m_frontendStartupAccepting = state[Phase] == 2;
        }

        Status RunClock(const DefinitiveFrontendClockReference::Request& request) noexcept
        {
            DefinitiveInputReference::Request nativeRequest;
            nativeRequest.durationMs = request.durationMs;
            nativeRequest.cancelled = request.cancelled;
            nativeRequest.publish = request.publish;
            nativeRequest.clockReference = true;
            // Reuse Read96 unchanged. Preparation completes all adapter work
            // before native Begin; the entered scope uses fixed values only.
            return DefinitiveFrontendClockReference::RunTransaction(
                *this, request, [&]() noexcept { return DefinitiveClockReference::Run(core->m_DefinitiveAdapter, nativeRequest); });
        }
    };

    namespace
    {
        SRWLOCK           startupLock = SRWLOCK_INIT;
        Controller*       activeStartup = nullptr;
        std::uint64_t     runtimeGeneration = 0;
        thread_local bool frontendBorrowAuthorized = false;

        struct FrontendBorrowAuthorization
        {
            const bool previous = frontendBorrowAuthorized;
            FrontendBorrowAuthorization() noexcept { frontendBorrowAuthorized = true; }
            ~FrontendBorrowAuthorization() { frontendBorrowAuthorized = previous; }
        };

        struct Operation
        {
            bool acquired = TryAcquireSRWLockExclusive(&startupLock) != FALSE;
            ~Operation()
            {
                if (acquired)
                    ReleaseSRWLockExclusive(&startupLock);
            }
            Status Validate(void* handle) const noexcept
            {
                if (!acquired)
                    return Status::Busy;
                if (!handle || handle != activeStartup)
                    return Status::InvalidArgument;
                return activeStartup->owner == GetCurrentThreadId() ? Status::Ok : Status::WrongThread;
            }
        };
    }

    bool AcquireFrontendBorrow(bool& ownsLock) noexcept
    {
        ownsLock = false;
        // RunView/RunPersistent alone authorize a synchronous borrow while
        // already holding startupLock. A clock callback gets no such bypass.
        if (frontendBorrowAuthorized)
            return true;
        if (!TryAcquireSRWLockExclusive(&startupLock))
            return false;
        ownsLock = true;
        return true;
    }

    void ReleaseFrontendBorrow(bool ownsLock) noexcept
    {
        if (ownsLock)
            ReleaseSRWLockExclusive(&startupLock);
    }

    void* BeginOwner() noexcept
    {
        Operation operation;
        if (!operation.acquired || activeStartup || g_pCore || CCore::GetSingletonPtr())
            return nullptr;
        try
        {
            auto controller = std::make_unique<Controller>();
            controller->Begin();
            activeStartup = controller.release();
            return activeStartup;
        }
        catch (...)
        {
            return nullptr;
        }
    }

    Status PulseOwner(void* handle) noexcept
    {
        Operation  operation;
        const auto status = operation.Validate(handle);
        if (status != Status::Ok)
            return status;
        if (activeStartup->runtimeMode)
            return Status::InvalidArgument;
        if (activeStartup->state[Phase] != 2)
            return activeStartup->state[Phase] == 5 ? Status::Failed : Status::Stopped;
        return activeStartup->Pulse();
    }

    Status BindOwner(void* handle) noexcept
    {
        Operation  operation;
        const auto status = operation.Validate(handle);
        return status == Status::Ok ? (activeStartup->runtimeMode ? Status::InvalidArgument : activeStartup->Bind()) : status;
    }

    Status ReadOwner(void* handle, std::uint32_t* output, std::uint32_t count) noexcept
    {
        Operation  operation;
        const auto status = operation.Validate(handle);
        if (status != Status::Ok)
            return status;
        if (!output || count != FieldCount)
            return Status::InvalidArgument;
        try
        {
            const auto snapshot = activeStartup->Read();
            std::copy(snapshot.begin(), snapshot.end(), output);
            return Status::Ok;
        }
        catch (...)
        {
            return Status::Failed;
        }
    }

    Status ReadGameTimeOwner(void* handle, std::uint32_t* output, std::uint32_t count) noexcept
    {
        Operation  operation;
        const auto status = operation.Validate(handle);
        if (status != Status::Ok)
            return status;
        if (!output || count != DefinitiveStartupGameTime::FieldCount)
            return Status::InvalidArgument;
        const auto snapshot = activeStartup->ReadGameTime();
        std::copy(snapshot.begin(), snapshot.end(), output);
        return Status::Ok;
    }

    Status ShutdownOwner(void* handle) noexcept
    {
        Operation  operation;
        const auto status = operation.Validate(handle);
        return status == Status::Ok ? (activeStartup->runtimeMode ? Status::InvalidArgument : activeStartup->Shutdown()) : status;
    }

    Status ReleaseOwner(void* handle) noexcept
    {
        Operation  operation;
        const auto status = operation.Validate(handle);
        if (status != Status::Ok)
            return status;
        if (activeStartup->runtimeMode)
            return Status::InvalidArgument;
        const auto retired = activeStartup->Retire();
        if (retired != Status::Ok)
            return retired;
        delete activeStartup;
        activeStartup = nullptr;
        return Status::Ok;
    }

    Status RunViewOwner(void* handle, std::uint32_t durationMs, DefinitiveInputReference::CancelCallback cancel,
                        DefinitiveInputReference::PublishCallback publish) noexcept
    {
        Operation  operation;
        const auto status = operation.Validate(handle);
        if (status != Status::Ok)
            return status;
        if (activeStartup->runtimeMode)
            return Status::InvalidArgument;
        if (!durationMs || durationMs > DefinitiveGuiFrontendInputReference::MaximumDurationMs || !cancel || !publish)
            return Status::InvalidArgument;
        try
        {
            FrontendBorrowAuthorization borrow;
            return activeStartup->RunView(durationMs, cancel, publish);
        }
        catch (...)
        {
            activeStartup->RetainFailedView();
            return Status::Failed;
        }
    }

    Status RunPersistentOwner(void* handle, const DefinitivePersistentFrontend::RequestV1* request) noexcept
    {
        Operation  operation;
        const auto status = operation.Validate(handle);
        if (status != Status::Ok)
            return status;
        if (activeStartup->runtimeMode)
            return Status::InvalidArgument;
        if (!request || request->size != sizeof(*request) || request->version != DefinitivePersistentFrontend::ApiVersion || !request->generation ||
            request->reserved[0] || request->reserved[1] || !request->cancelled || !request->publish || !request->heartbeat)
            return Status::InvalidArgument;
        try
        {
            FrontendBorrowAuthorization borrow;
            return activeStartup->RunPersistent(*request);
        }
        catch (...)
        {
            activeStartup->RetainFailedView();
            return Status::Failed;
        }
    }

    Status RunClockOwner(void* handle, std::uint32_t durationMs, DefinitiveInputReference::CancelCallback cancel,
                         DefinitiveInputReference::PublishCallback publish) noexcept
    {
        Operation  operation;
        const auto status = operation.Validate(handle);
        if (status != Status::Ok)
            return status;
        if (activeStartup->runtimeMode)
            return Status::InvalidArgument;
        return activeStartup->RunClock({durationMs, cancel, publish});
    }

    void* BeginRuntimeOwner() noexcept
    {
        Operation operation;
        if (!operation.acquired || activeStartup || g_pCore || CCore::GetSingletonPtr() || runtimeGeneration == UINT64_MAX)
            return nullptr;
        try
        {
            auto controller = std::make_unique<Controller>(true, ++runtimeGeneration);
            controller->Begin();
            activeStartup = controller.release();
            return activeStartup;
        }
        catch (...)
        {
            return nullptr;
        }
    }

    Status ReadRuntimeOwner(void* handle, std::uint32_t* output, std::uint32_t count) noexcept
    {
        Operation  operation;
        const auto status = operation.Validate(handle);
        if (status != Status::Ok)
            return status;
        if (!activeStartup->runtimeMode || !output || count != DefinitiveFrontendRuntime::FieldCount)
            return Status::InvalidArgument;
        try
        {
            const auto snapshot = activeStartup->ReadRuntime();
            std::copy(snapshot.begin(), snapshot.end(), output);
            return Status::Ok;
        }
        catch (...)
        {
            return Status::Failed;
        }
    }

    Status PulseRuntimeOwner(void* handle) noexcept
    {
        Operation  operation;
        const auto status = operation.Validate(handle);
        if (status != Status::Ok)
            return status;
        if (!activeStartup->runtimeMode)
            return Status::InvalidArgument;
        if (activeStartup->runtimeStopping || activeStartup->runtimeStopped || activeStartup->state[Phase] != 2)
            return activeStartup->runtimeStopped ? Status::Stopped : activeStartup->state[Phase] == 5 ? Status::Failed : Status::Busy;
        return activeStartup->Pulse();
    }

    Status RunRuntimeViewOwner(void* handle, const DefinitivePersistentFrontend::RequestV1* request) noexcept
    {
        Operation  operation;
        const auto status = operation.Validate(handle);
        if (status != Status::Ok)
            return status;
        if (!activeStartup->runtimeMode || !request || request->size != sizeof(*request) || request->version != DefinitivePersistentFrontend::ApiVersion ||
            !request->generation || request->reserved[0] || request->reserved[1] || !request->cancelled || !request->publish || !request->heartbeat)
            return Status::InvalidArgument;
        try
        {
            FrontendBorrowAuthorization borrow;
            return activeStartup->RunRuntimeView(*request);
        }
        catch (...)
        {
            activeStartup->RetainFailedView();
            activeStartup->runtimeStopping = true;
            activeStartup->runtimeFailure = DefinitiveFrontendRuntime::Failure::View;
            return Status::Failed;
        }
    }

    Status ShutdownRuntimeOwner(void* handle) noexcept
    {
        Operation  operation;
        const auto status = operation.Validate(handle);
        if (status != Status::Ok)
            return status;
        return activeStartup->runtimeMode ? activeStartup->ShutdownRuntime() : Status::InvalidArgument;
    }

    Status ReleaseRuntimeOwner(void* handle) noexcept
    {
        Operation  operation;
        const auto status = operation.Validate(handle);
        if (status != Status::Ok)
            return status;
        if (!activeStartup->runtimeMode)
            return Status::InvalidArgument;
        const auto retired = activeStartup->RetireRuntime();
        if (retired != Status::Ok)
            return retired;
        delete activeStartup;
        activeStartup = nullptr;
        return Status::Ok;
    }

    Status ConfigureLocalOwner(void* handle, const DefinitiveLocalInputRuntime::RequestV1* request) noexcept
    {
        Operation  operation;
        const auto status = operation.Validate(handle);
        if (status != Status::Ok)
            return status;
        if (!request || request->size != sizeof(*request) || request->version != DefinitiveLocalInputRuntime::ApiVersion || !request->window ||
            request->reserved[0] || request->reserved[1])
            return Status::InvalidArgument;
        return activeStartup->ConfigureLocal(*request);
    }

    Status EnableRuntimeBindingPersistenceOwner(void* handle, std::uint32_t version, const wchar_t* path, std::uint32_t chars, bool explicitPath) noexcept
    {
        Operation  operation;
        const auto status = operation.Validate(handle);
        if (status != Status::Ok)
            return status;
        if (version != DefinitiveFrontendBindingPersistence::ApiVersion ||
            (explicitPath && (!path || !chars || chars > 32767 || std::find(path, path + chars, L'\0') != path + chars)))
            return Status::InvalidArgument;
        return activeStartup->EnableBindingPersistence(path, chars);
    }

    Status PulseLocalOwner(void* handle) noexcept
    {
        Operation  operation;
        const auto status = operation.Validate(handle);
        return status == Status::Ok ? activeStartup->PulseLocal() : status;
    }

    Status ReadLocalOwner(void* handle, std::uint32_t* output, std::uint32_t count) noexcept
    {
        Operation  operation;
        const auto status = operation.Validate(handle);
        if (status != Status::Ok)
            return status;
        if (!activeStartup->runtimeMode || !output || count != DefinitiveLocalInputRuntime::FieldCount)
            return Status::InvalidArgument;
        if (activeStartup->clockPending || !activeStartup->lastClockRetired)
            return Status::Busy;
        const auto sample = activeStartup->ReadLocal();
        std::copy(sample.begin(), sample.end(), output);
        return Status::Ok;
    }

    Status StopLocalOwner(void* handle, bool disable) noexcept
    {
        Operation  operation;
        const auto status = operation.Validate(handle);
        return status == Status::Ok ? activeStartup->StopLocal(disable) : status;
    }
}

namespace DefinitiveLocalInputRuntime
{
    Status ConfigureOwner(void* handle, const RequestV1* request) noexcept
    {
        return DefinitiveFrontendStartup::ConfigureLocalOwner(handle, request);
    }
    Status PulseOwner(void* handle) noexcept
    {
        return DefinitiveFrontendStartup::PulseLocalOwner(handle);
    }
    Status ReadOwner(void* handle, std::uint32_t* output, std::uint32_t count) noexcept
    {
        return DefinitiveFrontendStartup::ReadLocalOwner(handle, output, count);
    }
    Status SuspendOwner(void* handle) noexcept
    {
        return DefinitiveFrontendStartup::StopLocalOwner(handle, false);
    }
    Status DisableOwner(void* handle) noexcept
    {
        return DefinitiveFrontendStartup::StopLocalOwner(handle, true);
    }
}

namespace DefinitiveStartupGameTime
{
    Status ReadOwner(void* handle, std::uint32_t* output, std::uint32_t count) noexcept
    {
        return DefinitiveFrontendStartup::ReadGameTimeOwner(handle, output, count);
    }
}

namespace DefinitiveFrontendRuntime
{
    void* BeginOwner() noexcept
    {
        return DefinitiveFrontendStartup::BeginRuntimeOwner();
    }
    Status ReadOwner(void* handle, std::uint32_t* output, std::uint32_t count) noexcept
    {
        return DefinitiveFrontendStartup::ReadRuntimeOwner(handle, output, count);
    }
    Status PulseOwner(void* handle) noexcept
    {
        return DefinitiveFrontendStartup::PulseRuntimeOwner(handle);
    }
    Status RunViewOwner(void* handle, const DefinitivePersistentFrontend::RequestV1* request) noexcept
    {
        return DefinitiveFrontendStartup::RunRuntimeViewOwner(handle, request);
    }
    Status ShutdownOwner(void* handle) noexcept
    {
        return DefinitiveFrontendStartup::ShutdownRuntimeOwner(handle);
    }
    Status ReleaseOwner(void* handle) noexcept
    {
        return DefinitiveFrontendStartup::ReleaseRuntimeOwner(handle);
    }
    Status EnableBindingPersistenceOwner(void* handle, std::uint32_t version) noexcept
    {
        return DefinitiveFrontendStartup::EnableRuntimeBindingPersistenceOwner(handle, version, nullptr, 0, false);
    }
    Status EnableBindingPersistenceAtPathOwner(void* handle, std::uint32_t version, const wchar_t* path, std::uint32_t chars) noexcept
    {
        return DefinitiveFrontendStartup::EnableRuntimeBindingPersistenceOwner(handle, version, path, chars, true);
    }
}

namespace DefinitiveFrontendClockReference
{
    Status RunOwner(void* handle, std::uint32_t durationMs, DefinitiveInputReference::CancelCallback cancel,
                    DefinitiveInputReference::PublishCallback publish) noexcept
    {
        return DefinitiveFrontendStartup::RunClockOwner(handle, durationMs, cancel, publish);
    }
}

namespace DefinitivePersistentFrontend
{
    Status RunOwner(void* handle, const RequestV1* request) noexcept
    {
        return DefinitiveFrontendStartup::RunPersistentOwner(handle, request);
    }
}
#endif
