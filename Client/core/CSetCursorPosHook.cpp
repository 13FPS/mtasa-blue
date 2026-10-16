/*****************************************************************************
 *
 *  PROJECT:     Multi Theft Auto v1.0
 *  LICENSE:     See LICENSE in the top level directory
 *  FILE:        core/CSetCursorPosHook.cpp
 *  PURPOSE:     Cursor position setting hook
 *
 *  Multi Theft Auto is available from https://www.multitheftauto.com/
 *
 *****************************************************************************/

#include "StdInc.h"
#include <SharedUtil.Detours.h>
#if defined(_WIN64) && defined(MTA_WITH_DEFINITIVE_ADAPTER)
    #include <tlhelp32.h>
    #include <intrin.h>
    #include <atomic>
    #include <array>
    #include <vector>

struct CSetCursorPosHook::FrontendImpl
{
    using Clock = ULONGLONG(WINAPI*)();
    FrontendCursorLease::Policy policy;
    Clock                       clock = nullptr;
    HWND                        window = nullptr;
    std::uintptr_t              imageBegin = 0, imageEnd = 0, hookImageBegin = 0, hookImageEnd = 0;
    std::uint64_t               generation = 0;
    std::atomic<bool>           hooked{false};
    std::atomic_flag            operation = ATOMIC_FLAG_INIT;
    std::atomic<std::uint32_t>  lastFailure{0}, attachStatus{0}, detachStatus{0};
    std::atomic<LONG>           clipLeft{0}, clipTop{0}, clipRight{0}, clipBottom{0};
    std::atomic<bool>           clipObserved{false};

    std::uint64_t Now() const noexcept { return clock ? clock() : UINT64_MAX; }

    bool Focused() const noexcept
    {
        DWORD      process = 0;
        const auto foreground = GetForegroundWindow();
        return window && IsWindow(window) && GetWindowThreadProcessId(window, &process) && process == GetCurrentProcessId() &&
               GetAncestor(window, GA_ROOT) == window && IsWindowVisible(window) && IsWindowEnabled(window) && !IsIconic(window) && foreground &&
               GetAncestor(foreground, GA_ROOT) == window;
    }
    struct Operation
    {
        std::atomic_flag& flag;
        bool              acquired;
        explicit Operation(std::atomic_flag& value) noexcept : flag(value), acquired(!flag.test_and_set()) {}
        ~Operation()
        {
            if (acquired)
                flag.clear();
        }
    };
};

namespace
{
    DWORD CursorLastError() noexcept
    {
        const auto error = GetLastError();
        return error ? error : ERROR_GEN_FAILURE;
    }
    bool CursorImageBounds(HMODULE module, std::uintptr_t& first, std::uintptr_t& end) noexcept
    {
        IMAGE_DOS_HEADER dos{};
        SIZE_T           count = 0;
        first = reinterpret_cast<std::uintptr_t>(module);
        if (!first || !ReadProcessMemory(GetCurrentProcess(), module, &dos, sizeof(dos), &count) || count != sizeof(dos) ||
            dos.e_magic != IMAGE_DOS_SIGNATURE || dos.e_lfanew < 64 || dos.e_lfanew > 4096)
            return false;
        IMAGE_NT_HEADERS64 nt{};
        if (!ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(first + dos.e_lfanew), &nt, sizeof(nt), &count) || count != sizeof(nt) ||
            nt.Signature != IMAGE_NT_SIGNATURE || nt.FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64 ||
            nt.OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC || !nt.OptionalHeader.SizeOfImage || first > UINTPTR_MAX - nt.OptionalHeader.SizeOfImage)
            return false;
        end = first + nt.OptionalHeader.SizeOfImage;
        return true;
    }
}
#endif

template <>
CSetCursorPosHook* CSingleton<CSetCursorPosHook>::m_pSingleton = NULL;

CSetCursorPosHook::CSetCursorPosHook()
{
    WriteDebugEvent("CSetCursorPosHook::CSetCursorPosHook");

    m_bCanCall = true;
    m_pfnSetCursorPos = NULL;
}

CSetCursorPosHook::~CSetCursorPosHook()
{
    WriteDebugEvent("CSetCursorPosHook::~CSetCursorPosHook");

#if defined(_WIN64) && defined(MTA_WITH_DEFINITIVE_ADAPTER)
    // A failed frontend detach is still a live code owner. Its caller must
    // retain this instance and Core; the destructor is not a cleanup retry.
    assert(!m_frontend || (!m_frontend->hooked.load() && m_frontend->policy.Read().ended));
#endif
    if (m_pfnSetCursorPos != NULL)
    {
        RemoveHook();
    }
}

void CSetCursorPosHook::ApplyHook()
{
    DetourLibraryFunction("user32.dll", "SetCursorPos", m_pfnSetCursorPos, API_SetCursorPos);
}

void CSetCursorPosHook::RemoveHook()
{
    // Remove hook
    if (m_pfnSetCursorPos)
    {
        UndoFunctionDetour(m_pfnSetCursorPos, API_SetCursorPos);
    }

    // Reset variables
    m_pfnSetCursorPos = nullptr;
    m_bCanCall = true;
}

void CSetCursorPosHook::DisableSetCursorPos()
{
    m_bCanCall = false;
}

void CSetCursorPosHook::EnableSetCursorPos()
{
    m_bCanCall = true;
}

bool CSetCursorPosHook::IsSetCursorPosEnabled()
{
    return m_bCanCall;
}

BOOL CSetCursorPosHook::CallSetCursorPos(int X, int Y)
{
#if defined(_WIN64) && defined(MTA_WITH_DEFINITIVE_ADAPTER)
    if (m_frontend)
    {
        auto& lease = *m_frontend;
        lease.policy.EnterCallback();
        __try
        {
            // Explicit MTA positioning bypasses suppression, but its original
            // may still be executing user32 while another thread requests End.
            // Retain Core until that call returns through this counted frame.
            lease.policy.RecordPassthrough();
            return m_pfnSetCursorPos ? m_pfnSetCursorPos(X, Y) : ::SetCursorPos(X, Y);
        }
        __finally
        {
            lease.policy.LeaveCallback();
        }
    }
#endif
    if (m_pfnSetCursorPos == NULL)
    {
        // We should never get here, but if we do, attempt to call
        // an imported SetCursorPos.
        return SetCursorPos(X, Y);
    }
    else
    {
        return m_pfnSetCursorPos(X, Y);
    }
}

BOOL WINAPI CSetCursorPosHook::API_SetCursorPos(int X, int Y)
{
    CSetCursorPosHook* pThis = CSetCursorPosHook::GetSingletonPtr();
#if defined(_WIN64) && defined(MTA_WITH_DEFINITIVE_ADAPTER)
    if (pThis && pThis->m_frontend)
    {
        auto& lease = *pThis->m_frontend;
        lease.policy.EnterCallback();
        __try
        {
            const auto caller = reinterpret_cast<std::uintptr_t>(_ReturnAddress());
            if (lease.policy.Suppress(lease.Now(), lease.Focused(), caller >= lease.imageBegin && caller < lease.imageEnd))
                // GUI ownership deliberately completes the engine request as a
                // no-op. Unlike the classic denial, success prevents UE cursor
                // fallback/error handling from attempting another recenter.
                return TRUE;
            // Count the entire original call, including pass-through and
            // failure paths. A suspended original may still return into Core.
            return pThis->m_pfnSetCursorPos ? pThis->m_pfnSetCursorPos(X, Y) : FALSE;
        }
        __finally
        {
            lease.policy.LeaveCallback();
        }
    }
#endif
    if (pThis && pThis->m_bCanCall && pThis->m_pfnSetCursorPos)
        return pThis->m_pfnSetCursorPos(X, Y);
    return FALSE;
}

#if defined(_WIN64) && defined(MTA_WITH_DEFINITIVE_ADAPTER)
bool CSetCursorPosHook::ChangeFrontendHook(bool install) noexcept
{
    auto&               lease = *m_frontend;
    std::vector<HANDLE> threads;
    const HANDLE        snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    DWORD               status = snapshot == INVALID_HANDLE_VALUE ? CursorLastError() : NO_ERROR;
    if (snapshot != INVALID_HANDLE_VALUE)
    {
        THREADENTRY32 entry{};
        entry.dwSize = sizeof(entry);
        bool enumerated = Thread32First(snapshot, &entry) != FALSE;
        if (!enumerated)
            status = CursorLastError();
        try
        {
            while (enumerated && !status)
            {
                if (entry.th32OwnerProcessID == GetCurrentProcessId() && entry.th32ThreadID != GetCurrentThreadId())
                {
                    HANDLE thread =
                        OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_SET_CONTEXT | THREAD_QUERY_INFORMATION, FALSE, entry.th32ThreadID);
                    if (!thread)
                    {
                        status = CursorLastError();
                        break;
                    }
                    try
                    {
                        threads.push_back(thread);
                    }
                    catch (...)
                    {
                        CloseHandle(thread);
                        throw;
                    }
                }
                enumerated = Thread32Next(snapshot, &entry) != FALSE;
                if (!enumerated && GetLastError() != ERROR_NO_MORE_FILES)
                    status = CursorLastError();
            }
        }
        catch (...)
        {
            status = ERROR_NOT_ENOUGH_MEMORY;
        }
        CloseHandle(snapshot);
    }
    bool transaction = false;
    if (!status)
    {
        status = DetourTransactionBegin();
        transaction = !status;
    }
    if (!status)
        status = DetourUpdateThread(GetCurrentThread());
    for (HANDLE thread : threads)
    {
        if (!status)
            status = DetourUpdateThread(thread);
    }
    if (!status && !install)
    {
        if (lease.policy.Read().inFlight)
            status = ERROR_BUSY;
        // Also cover the tiny callback prologue/epilogue outside its counter.
        // A peer executing Core could resume through the hook after teardown;
        // retain and retry rather than unload that code beneath its RIP.
        for (HANDLE thread : threads)
        {
            if (status)
                break;
            CONTEXT context{};
            context.ContextFlags = CONTEXT_CONTROL;
            if (!GetThreadContext(thread, &context))
                status = CursorLastError();
            else if (context.Rip >= lease.hookImageBegin && context.Rip < lease.hookImageEnd)
                status = ERROR_BUSY;
        }
    }
    if (!status)
        status = (install ? DetourAttach : DetourDetach)(reinterpret_cast<PVOID*>(&m_pfnSetCursorPos), reinterpret_cast<PVOID>(&API_SetCursorPos));
    if (!status)
    {
        status = DetourTransactionCommit();
        transaction = false;
    }
    if (transaction)
        DetourTransactionAbort();
    for (HANDLE thread : threads)
        CloseHandle(thread);
    (install ? lease.attachStatus : lease.detachStatus).store(status);
    if (status)
        lease.lastFailure.store(status);
    return status == NO_ERROR;
}

bool CSetCursorPosHook::BeginFrontendLease(HWND rootWindow) noexcept
{
    try
    {
        if (!m_frontend)
            m_frontend = std::make_unique<FrontendImpl>();
        auto&                   lease = *m_frontend;
        FrontendImpl::Operation operation(lease.operation);
        if (!operation.acquired || lease.hooked.load() || m_pfnSetCursorPos ||
            (lease.policy.Read().ownerThread && lease.policy.Read().ownerThread != GetCurrentThreadId()))
            return false;
        lease.window = rootWindow;
        RECT clip{};
        lease.clipObserved.store(GetClipCursor(&clip) != FALSE);
        lease.clipLeft.store(clip.left);
        lease.clipTop.store(clip.top);
        lease.clipRight.store(clip.right);
        lease.clipBottom.store(clip.bottom);
        HMODULE    ownModule = nullptr;
        const bool module = GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                               reinterpret_cast<LPCWSTR>(&API_SetCursorPos), &ownModule) != FALSE;
        // The actual adapter already verifies this executable before admitting
        // GUI input. Pin its reviewed UE cursor wrapper as a separate guard;
        // its tail jump does not have a fixed hook return-address callsite.
        constexpr std::uintptr_t                  wrapperRva = 0x1cf3c40;
        constexpr std::array<unsigned char, 12>   wrapper{0x8b, 0xca, 0x41, 0x8b, 0xd0, 0x48, 0xff, 0x25, 0xfc, 0xe5, 0x24, 0x02};
        std::array<unsigned char, wrapper.size()> actual{};
        SIZE_T                                    copied = 0;
        if (!lease.Focused() || !CursorImageBounds(GetModuleHandleW(nullptr), lease.imageBegin, lease.imageEnd) || !module ||
            !CursorImageBounds(ownModule, lease.hookImageBegin, lease.hookImageEnd) || lease.imageEnd - lease.imageBegin < wrapperRva + wrapper.size() ||
            !ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(lease.imageBegin + wrapperRva), actual.data(), actual.size(), &copied) ||
            copied != actual.size() || actual != wrapper)
        {
            lease.lastFailure.store(ERROR_INVALID_DATA);
            return false;
        }
        const auto user = GetModuleHandleW(L"user32.dll");
        m_pfnSetCursorPos = user ? reinterpret_cast<pSetCursorPos>(GetProcAddress(user, "SetCursorPos")) : nullptr;
        // The general MTA clock wrapper takes a shared critical section. A
        // cursor callback must not wait for a Core lock while Detours can
        // suspend its owner. DE already requires modern Windows, so resolve
        // its clock once before hooks; callbacks never use the loader.
        const auto kernel = GetModuleHandleW(L"kernel32.dll");
        lease.clock = kernel ? reinterpret_cast<FrontendImpl::Clock>(GetProcAddress(kernel, "GetTickCount64")) : nullptr;
        if (!m_pfnSetCursorPos || !lease.clock || !lease.policy.Begin(GetCurrentThreadId(), lease.Now()))
        {
            m_pfnSetCursorPos = nullptr;
            lease.lastFailure.store(ERROR_INVALID_STATE);
            return false;
        }
        lease.generation = lease.policy.Read().generation;
        if (!ChangeFrontendHook(true))
        {
            // Detours aborts a failed transaction without publishing its patch.
            // The same owner still performs explicit End, even for this failure.
            lease.policy.Disarm();
            m_pfnSetCursorPos = nullptr;
            return false;
        }
        lease.hooked.store(true);
        return true;
    }
    catch (...)
    {
        if (m_frontend)
            m_frontend->lastFailure.store(ERROR_NOT_ENOUGH_MEMORY);
        return false;
    }
}

bool CSetCursorPosHook::RenewFrontendLease() noexcept
{
    if (!m_frontend)
        return false;
    auto&                   lease = *m_frontend;
    FrontendImpl::Operation operation(lease.operation);
    return operation.acquired && lease.hooked.load() && lease.policy.Renew(GetCurrentThreadId(), lease.generation, lease.Now(), lease.Focused());
}

void CSetCursorPosHook::DisarmFrontendLease() noexcept
{
    if (m_frontend)
        m_frontend->policy.Disarm();
}

bool CSetCursorPosHook::EndFrontendLease() noexcept
{
    if (!m_frontend)
        return true;
    auto&                   lease = *m_frontend;
    FrontendImpl::Operation operation(lease.operation);
    if (!operation.acquired)
        return false;
    const auto state = lease.policy.Read();
    if (state.ownerThread && state.ownerThread != GetCurrentThreadId())
        return false;
    lease.policy.Disarm();
    if (state.inFlight)
        return false;
    if (state.ended && !lease.hooked.load())
        return true;
    if (lease.hooked.load())
    {
        if (!ChangeFrontendHook(false))
            return false;
        lease.hooked.store(false);
        m_pfnSetCursorPos = nullptr;
    }
    return lease.policy.Complete(GetCurrentThreadId());
}

CSetCursorPosHook::FrontendLeaseState CSetCursorPosHook::ReadFrontendLease() const noexcept
{
    FrontendLeaseState state;
    if (!m_frontend)
        return state;
    const auto& lease = *m_frontend;
    static_cast<FrontendCursorLease::State&>(state) = lease.policy.Read();
    state.hooked = lease.hooked.load();
    state.lastFailure = lease.lastFailure.load();
    state.attachStatus = lease.attachStatus.load();
    state.detachStatus = lease.detachStatus.load();
    state.clipObserved = lease.clipObserved.load();
    state.clip = {lease.clipLeft.load(), lease.clipTop.load(), lease.clipRight.load(), lease.clipBottom.load()};
    return state;
}
#endif
