/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Retain a completed Core independently of a bounded frontend view
 *****************************************************************************/

#include "StdInc.h"
#include "DefinitiveFrontendStartup.h"
#include "DefinitiveFrontendClockReference.h"
#include "DefinitivePersistentFrontend.h"

#if defined(_WIN64) && defined(MTA_WITH_DEFINITIVE_ADAPTER)

namespace DefinitiveFrontendStartup
{
    struct Controller
    {
        const DWORD owner = GetCurrentThreadId();
        CCore*      core = nullptr;
        Snapshot    state{};
        bool        clockPending = false;
        bool        lastClockRetired = true;

        Controller()
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
            if (state[ExactImageBound])
                return Status::Ok;
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
            return core->m_frontendResources || !state[LastViewCleanupComplete] ? Status::Busy : Status::Ok;
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
        if (activeStartup->state[Phase] != 2)
            return activeStartup->state[Phase] == 5 ? Status::Failed : Status::Stopped;
        return activeStartup->Pulse();
    }

    Status BindOwner(void* handle) noexcept
    {
        Operation  operation;
        const auto status = operation.Validate(handle);
        return status == Status::Ok ? activeStartup->Bind() : status;
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

    Status ShutdownOwner(void* handle) noexcept
    {
        Operation  operation;
        const auto status = operation.Validate(handle);
        return status == Status::Ok ? activeStartup->Shutdown() : status;
    }

    Status ReleaseOwner(void* handle) noexcept
    {
        Operation  operation;
        const auto status = operation.Validate(handle);
        if (status != Status::Ok)
            return status;
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
        return activeStartup->RunClock({durationMs, cancel, publish});
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
