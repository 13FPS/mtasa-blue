/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Shared clock-derived peer suspension without detached cleanup
 *****************************************************************************/
#include "NativePeerThreads.h"
#include <tlhelp32.h>
#include <intrin.h>
#include <detours.h>

#if !defined(DETOURS_MTA_VM_TRANSACTION_NODES)
    #error Native peer registration requires externally suspended Detours VM nodes.
#endif

namespace mta::game_de
{
    namespace
    {
        DWORD Suspend(HANDLE handle) noexcept
        {
            return SuspendThread(handle);
        }
        BOOL Context(HANDLE handle, CONTEXT* context) noexcept
        {
            return GetThreadContext(handle, context);
        }
        DWORD Resume(HANDLE handle) noexcept
        {
            return ResumeThread(handle);
        }
        DWORD Wait(HANDLE handle, DWORD milliseconds) noexcept
        {
            return WaitForSingleObject(handle, milliseconds);
        }
        BOOL Close(HANDLE handle) noexcept
        {
            return CloseHandle(handle);
        }
        LONG Register(HANDLE handle) noexcept
        {
            return DetourUpdateThreadExternallySuspended(handle);
        }
    }

    const NativePeerThreads::Operations& NativePeerThreads::DefaultOperations() noexcept
    {
        static const Operations defaults{Suspend, Context, Resume, Wait, Close, Register};
        return defaults;
    }

    NativePeerThreads::NativePeerThreads(const Operations& selected) noexcept : operations(selected)
    {
    }

    NativePeerThreads::~NativePeerThreads()
    {
        // A destructor cannot report successful cleanup or throw away the only
        // retry handles. Services with retained recovery use process-lifetime
        // pinned storage; reaching this destructor busy/retained is misuse.
        if (busy.load() || prepared.load() || parked.load() || retained.load() || !recovery.empty())
            __fastfail(FAST_FAIL_FATAL_APP_EXIT);
    }

    bool NativePeerThreads::ValidOperations() const noexcept
    {
        return operations.suspend && operations.context && operations.resume && operations.wait && operations.close && operations.registerDetours;
    }

    bool NativePeerThreads::ResumePeer(Hold::Peer& peer) const noexcept
    {
        if (peer.suspended)
        {
            if (operations.resume(peer.handle) == static_cast<DWORD>(-1) && operations.wait(peer.handle, 0) != WAIT_OBJECT_0)
                return false;
            peer.suspended = false;
        }
        // Retain even an unparked handle if closing it fails. A retry must not
        // add another resume to a thread whose owned suspend count is gone.
        if (peer.handle && !operations.close(peer.handle))
            return false;
        peer.handle = nullptr;
        return true;
    }

    NativePeerThreads::Hold::~Hold()
    {
        Release();
    }

    bool NativePeerThreads::Hold::Prepare() noexcept
    {
        if (ownsGate || owner.busy.exchange(true))
            return false;
        ownsGate = true;
        if (owner.HasRecovery() || !owner.recovery.empty())
        {
            // This hold owns no recovery handles. Releasing its empty vector
            // must not clear counts belonging to an earlier failed resume.
            owner.busy.store(false);
            ownsGate = false;
            return false;
        }
        if (!owner.ValidOperations())
        {
            Release();
            return false;
        }
        // Retire old empty capacity and reserve every record BEFORE parking.
        // Failed release later transfers ownership with a no-allocation swap.
        std::vector<Peer>{}.swap(owner.recovery);
        try
        {
            peers.reserve(MaximumPeers);
        }
        catch (...)
        {
            Release();
            return false;
        }
        const auto snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
        if (snapshot == INVALID_HANDLE_VALUE)
        {
            Release();
            return false;
        }
        THREADENTRY32 entry{};
        entry.dwSize = sizeof(entry);
        bool enumerated = Thread32First(snapshot, &entry) != FALSE;
        bool ready = enumerated;
        while (enumerated)
        {
            if (entry.th32OwnerProcessID == GetCurrentProcessId() && entry.th32ThreadID != GetCurrentThreadId())
            {
                if (peers.size() == MaximumPeers)
                {
                    ready = false;
                    break;
                }
                const auto handle = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_SET_CONTEXT | THREAD_QUERY_INFORMATION | SYNCHRONIZE, FALSE,
                                               entry.th32ThreadID);
                if (!handle)
                {
                    ready = false;
                    break;
                }
                peers.push_back({handle, false});
            }
            SetLastError(ERROR_SUCCESS);
            enumerated = Thread32Next(snapshot, &entry) != FALSE;
            if (!enumerated && GetLastError() != ERROR_NO_MORE_FILES)
                ready = false;
        }
        CloseHandle(snapshot);
        owner.prepared.store(static_cast<std::uint32_t>(peers.size()));
        if (!ready)
        {
            Release();
            return false;
        }
        prepared = true;
        return true;
    }

    bool NativePeerThreads::Hold::Suspend(ContextPolicy policy, void* context) noexcept
    {
        if (!ownsGate || !prepared || parked || !policy)
        {
            Release();
            return false;
        }
        for (auto& peer : peers)
        {
            if (owner.operations.suspend(peer.handle) == static_cast<DWORD>(-1))
            {
                Release();
                return false;
            }
            peer.suspended = true;
            owner.parked.fetch_add(1);
        }
        for (const auto& peer : peers)
        {
            CONTEXT captured{};
            captured.ContextFlags = CONTEXT_CONTROL;
            if (!owner.operations.context(peer.handle, &captured) || !policy(captured, context))
            {
                Release();
                return false;
            }
        }
        parked = true;
        return true;
    }

    bool NativePeerThreads::Hold::RegisterDetours() const noexcept
    {
        if (!ownsGate || !prepared || !parked)
            return false;
        for (const auto& peer : peers)
            if (!peer.suspended || owner.operations.registerDetours(peer.handle) != NO_ERROR)
                return false;
        return true;
    }

    bool NativePeerThreads::Hold::Release() noexcept
    {
        if (!ownsGate)
            return true;
        std::uint32_t remaining = 0, stillParked = 0;
        for (auto& peer : peers)
            if (peer.handle && !owner.ResumePeer(peer))
            {
                ++remaining;
                stillParked += peer.suspended ? 1 : 0;
            }
        if (remaining)
            owner.recovery.swap(peers);
        else
            peers.clear();
        owner.prepared.store(0);
        owner.parked.store(stillParked);
        owner.retained.store(remaining);
        owner.busy.store(false);
        ownsGate = false;
        prepared = false;
        parked = false;
        return remaining == 0;
    }

    bool NativePeerThreads::RetryRecover(unsigned attempts) noexcept
    {
        if (attempts > MaximumRecoveryAttempts)
            attempts = MaximumRecoveryAttempts;
        for (unsigned attempt = 0; attempt < attempts; ++attempt)
        {
            if (busy.exchange(true))
                return false;
            std::uint32_t remaining = 0, stillParked = 0;
            for (auto& peer : recovery)
                if (peer.handle && !ResumePeer(peer))
                {
                    ++remaining;
                    stillParked += peer.suspended ? 1 : 0;
                }
            if (!remaining)
                recovery.clear();
            retained.store(remaining);
            parked.store(stillParked);
            busy.store(false);
            if (!remaining)
                return true;
        }
        return !HasRecovery() && !busy.load();
    }

    NativePeerThreads::State NativePeerThreads::Read() const noexcept
    {
        return {prepared.load(), parked.load(), retained.load(), busy.load()};
    }
}
