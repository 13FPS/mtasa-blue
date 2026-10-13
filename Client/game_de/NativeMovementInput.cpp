/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Scoped native local movement routing for the build-pinned DE trial
 *****************************************************************************/

#include "NativeMovementInput.h"
#include "NativeInputRouting.h"
#include "NativePadReader.h"

#include <array>
#include <atomic>
#include <cstring>
#include <limits>
#include <vector>
#include <windows.h>
#include <tlhelp32.h>
#include <detours.h>

namespace mta::game_de
{
    namespace
    {
        using Failure = NativeMovementTrialFailure;
        using ControlFunction = void (*)(void*);
        using MembershipFunction = bool (*)(void*, std::int32_t);
        using QueryFunction = std::int32_t (*)(void*, std::int32_t, float*);

        constexpr std::uintptr_t controlRva = 0x124eaf0;
        constexpr std::uintptr_t membershipRva = 0xc09750;
        constexpr std::uintptr_t availabilityRva = 0xc097d0;
        constexpr std::uintptr_t queryRva = 0xc09990;
        constexpr std::uintptr_t padRva = 0x53e8810;
        constexpr ULONGLONG      maximumTrialMs = 35000;
        constexpr ULONGLONG      maximumInputAgeMs = 2000;

        struct Counters
        {
            std::atomic<std::uint64_t> callbacks{0}, steps{0}, queries{0}, neutral{0}, restored{0}, restoreFailures{0}, foreign{0}, nested{0};
            std::atomic<std::uint32_t> reason{0};
            std::atomic<std::uint32_t> lastMoveState{0};
        };

        struct Runtime
        {
            SRWLOCK                    lock = SRWLOCK_INIT;
            NativePadReader            reader;
            std::atomic<bool>          bound{false}, active{false}, hooked{false};
            std::atomic<unsigned>      scopes{0};
            std::atomic<std::uint64_t> generation{0};
            DWORD                      thread = 0;
            std::uintptr_t             image = 0, ped = 0, provider = 0;
            ULONGLONG                  started = 0, refreshed = 0;
            MtaInputState              frame;
            Counters                   counters;
            ControlFunction            originalControl = nullptr;
            MembershipFunction         originalMembership = nullptr, originalAvailability = nullptr;
            QueryFunction              originalQuery = nullptr;
        };

        // Hooks must never retain an adapter instance. A failed detachment may
        // outlive Core's owner, so this process lifetime state and the pinned
        // module retain every trampoline and restoration snapshot they need.
        Runtime runtime;

        struct Scope
        {
            NativeInputRouting::PadTransaction transaction;
            MtaInputState                      frame;
            std::uint64_t                      generation = 0;
            std::int32_t                       providerIndex = 0;
            std::uintptr_t                     alternate = 0;
            bool                               routed = false;
            unsigned                           callbackDepth = 0;
        };
        thread_local Scope scope;

        void Fail(Failure reason) noexcept
        {
            runtime.active.store(false);
            std::uint32_t expected = 0;
            runtime.counters.reason.compare_exchange_strong(expected, static_cast<std::uint32_t>(reason));
        }

        bool Pages(std::uintptr_t address, std::size_t count, bool writable = false, bool executable = false) noexcept
        {
            if (!address || !count || count > (std::numeric_limits<std::uintptr_t>::max)() - address)
                return false;
            const auto end = address + count;
            while (address < end)
            {
                MEMORY_BASIC_INFORMATION info{};
                if (VirtualQuery(reinterpret_cast<const void*>(address), &info, sizeof(info)) != sizeof(info) || info.State != MEM_COMMIT ||
                    (info.Protect & (PAGE_GUARD | PAGE_NOACCESS)))
                    return false;
                const DWORD protection = info.Protect & 0xff;
                const bool  read = protection == PAGE_READONLY || protection == PAGE_READWRITE || protection == PAGE_WRITECOPY ||
                                  protection == PAGE_EXECUTE_READ || protection == PAGE_EXECUTE_READWRITE || protection == PAGE_EXECUTE_WRITECOPY;
                const bool write = protection == PAGE_READWRITE || protection == PAGE_WRITECOPY;
                const bool execute = protection == PAGE_EXECUTE_READ || protection == PAGE_EXECUTE_READWRITE || protection == PAGE_EXECUTE_WRITECOPY;
                if (!read || (writable && !write) || (executable && !execute))
                    return false;
                const auto region = reinterpret_cast<std::uintptr_t>(info.BaseAddress);
                if (info.RegionSize > (std::numeric_limits<std::uintptr_t>::max)() - region || region + info.RegionSize <= address)
                    return false;
                address = (region + info.RegionSize < end) ? region + info.RegionSize : end;
            }
            return true;
        }

        bool Read(std::uintptr_t address, void* result, std::size_t count) noexcept
        {
            SIZE_T copied = 0;
            return Pages(address, count) && ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<const void*>(address), result, count, &copied) &&
                   copied == count;
        }

        template <typename T>
        bool Read(std::uintptr_t address, T& result) noexcept
        {
            return Read(address, &result, sizeof(result));
        }

        bool WritePad(const NativeInputRouting::PadStateBytes& bytes) noexcept
        {
            const auto address = runtime.image + padRva + NativePadCurrentOffset;
            SIZE_T     copied = 0;
            // Only these two adjacent controller blocks are owned. Mouse data,
            // native physical sources and their pending edge queues stay outside
            // the write range, even when a trial or restoration fails.
            return Pages(address, bytes.size(), true) &&
                   WriteProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(address), bytes.data(), bytes.size(), &copied) && copied == bytes.size();
        }

        bool ReadContext(std::int32_t& index, std::uintptr_t& alternate) noexcept
        {
            return Read(runtime.image + 0x5726120, index) && Read(runtime.image + 0x5067940, alternate);
        }

        bool CheckBinding(void* candidate = nullptr) noexcept
        {
            std::uintptr_t ped = 0, vtable = 0, callback = 0, info = 0, provider = 0, providerVtable = 0, alternate = 0;
            std::int32_t   selector = -1, index = -1;
            std::uint32_t  pedFlags = 0;
            unsigned char  playerIndex = 0;
            if (!Read(runtime.image + 0x53eb720, ped) || !ped || (runtime.ped && ped != runtime.ped) ||
                (candidate && reinterpret_cast<std::uintptr_t>(candidate) != ped) || !Read(ped, vtable) || vtable != runtime.image + 0x4319730 ||
                !Read(vtable + 0x50, callback) || callback != runtime.image + controlRva || !Read(ped + 0x5f0, info) || info != runtime.image + 0x53eb728 ||
                !Read(ped + 0x7e0, selector) || selector != 0 || !Read(runtime.image + 0x522bcf6, playerIndex) || playerIndex != 0 ||
                !ReadContext(index, alternate) || index != 0 || alternate != runtime.image + padRva || !Read(runtime.image + 0x5726100, provider) ||
                !provider || (runtime.provider && provider != runtime.provider) || !Read(provider, providerVtable) ||
                providerVtable != runtime.image + 0x4226b20 || !Read(ped + 0x634, pedFlags))
                return false;
            // A cached vehicle pointer can outlive the on-foot transition.
            // The native InVehicle flag decides whether this trial owns input.
            if (pedFlags & 0x100)
            {
                Fail(Failure::Vehicle);
                return false;
            }
            const std::array<std::uintptr_t, 7> expected = {0xc09750, 0xc097d0, 0xc09810, 0xc09950, 0xc09970, 0xc09ae0, 0xae3c00};
            std::array<std::uintptr_t, 7>       actual{};
            if (!Read(providerVtable, actual.data(), sizeof(actual)))
                return false;
            for (std::size_t i = 0; i < actual.size(); ++i)
                if (actual[i] != runtime.image + expected[i])
                    return false;
            // Type2 is checked as code, rather than invoking an unowned native
            // virtual method merely to decide whether the trial can start.
            constexpr std::array<unsigned char, 6> type2 = {0xb8, 0x02, 0, 0, 0, 0xc3};
            std::array<unsigned char, 6>           typeCode{};
            if (!Read(actual[6], typeCode) || typeCode != type2)
                return false;
            runtime.ped = ped;
            runtime.provider = provider;
            return Pages(runtime.image + padRva + NativePadCurrentOffset, sizeof(CControllerState) * 2, true);
        }

        template <std::size_t N>
        bool Pin(std::uintptr_t rva, const std::array<unsigned char, N>& expected) noexcept
        {
            std::array<unsigned char, N> actual{};
            return Pages(runtime.image + rva, N, false, true) && Read(runtime.image + rva, actual) && actual == expected;
        }

        bool CheckEntryPins() noexcept
        {
            constexpr std::array<unsigned char, 16> control = {0x48, 0x8b, 0xc4, 0x48, 0x89, 0x58, 0x20, 0x55, 0x56, 0x57, 0x41, 0x54, 0x41, 0x55, 0x48, 0x8d};
            constexpr std::array<unsigned char, 16> membership = {0x48, 0x89, 0x5c, 0x24, 0x08, 0x57, 0x48, 0x83,
                                                                  0xec, 0x20, 0x48, 0x63, 0xda, 0x48, 0x8b, 0xf9};
            constexpr std::array<unsigned char, 14> availability = {0x8d, 0x42, 0xdb, 0x83, 0xf8, 0x01, 0x76, 0x2f, 0x80, 0x3d, 0x55, 0xc9, 0xb1, 0x04};
            constexpr std::array<unsigned char, 24> query = {0x48, 0x89, 0x5c, 0x24, 0x08, 0x48, 0x89, 0x6c, 0x24, 0x10, 0x48, 0x89,
                                                             0x74, 0x24, 0x18, 0x48, 0x89, 0x7c, 0x24, 0x20, 0x41, 0x56, 0x41, 0x57};
            return Pin(controlRva, control) && Pin(membershipRva, membership) && Pin(availabilityRva, availability) && Pin(queryRva, query);
        }

        bool Fresh() noexcept
        {
            const auto now = GetTickCount64();
            if (now - runtime.started >= maximumTrialMs)
            {
                Fail(Failure::Expired);
                return false;
            }
            if (now - runtime.refreshed > maximumInputAgeMs)
            {
                Fail(Failure::StaleInput);
                return false;
            }
            return true;
        }

        bool RestoreScope() noexcept
        {
            scope.routed = false;
            if (!scope.transaction.Active())
                return true;
            NativeInputRouting::PadStateBytes original{}, observed{};
            if (!scope.transaction.RestoreBytes(original) || !WritePad(original) ||
                !Read(runtime.image + padRva + NativePadCurrentOffset, observed.data(), observed.size()) || !scope.transaction.ConfirmRestored(observed))
            {
                runtime.counters.restoreFailures.fetch_add(1);
                Fail(Failure::RestoreFailure);
                // The original snapshot remains owned for an explicit retry.
                // Keeping the module/hook state alive is safer than discarding
                // the only bytes which can put local input back.
                return false;
            }
            runtime.counters.restored.fetch_add(1);
            runtime.scopes.store(0);
            std::int32_t   index = -1;
            std::uintptr_t alternate = 0;
            if (!ReadContext(index, alternate) || index != scope.providerIndex || alternate != scope.alternate)
                Fail(Failure::ContextChanged);
            return true;
        }

        bool PrepareScope(void* ped) noexcept
        {
            if (!runtime.active.load() || !Fresh())
                return false;
            if (!CheckBinding(ped))
            {
                Fail(Failure::BindingChanged);
                return false;
            }
            unsigned expected = 0;
            if (!runtime.scopes.compare_exchange_strong(expected, 1))
            {
                runtime.counters.nested.fetch_add(1);
                Fail(Failure::NestedCallback);
                return false;
            }
            NativeInputRouting::PadStateBytes physical{}, replacement{};
            scope.frame = runtime.frame;
            scope.generation = runtime.generation.load();
            if (!ReadContext(scope.providerIndex, scope.alternate) ||
                !Read(runtime.image + padRva + NativePadCurrentOffset, physical.data(), physical.size()) ||
                !scope.transaction.Prepare(physical, scope.frame, replacement))
            {
                runtime.scopes.store(0);
                Fail(Failure::ReadFailure);
                return false;
            }
            // Count ownership before applying: even a partial failed write has
            // a restoration obligation, which terminal metrics must include.
            runtime.counters.steps.fetch_add(1);
            if (!WritePad(replacement))
            {
                Fail(Failure::WriteFailure);
                RestoreScope();
                return false;
            }
            scope.routed = true;
            return true;
        }

        bool OwnsQuery(void* provider) noexcept
        {
            if (!scope.routed || runtime.generation.load() != scope.generation || GetCurrentThreadId() != runtime.thread)
                return false;
            std::int32_t   index = -1;
            std::uintptr_t alternate = 0;
            if (!ReadContext(index, alternate) || index != scope.providerIndex || alternate != scope.alternate)
            {
                Fail(Failure::ContextChanged);
                RestoreScope();
                return false;
            }
            // Disarming prevents the next scope. An existing native call still
            // owns its replacement bytes until restoration, so queries must
            // remain authoritative throughout that call even after a fault.
            if (reinterpret_cast<std::uintptr_t>(provider) == runtime.provider)
                return true;
            Fail(Failure::BindingChanged);
            RestoreScope();
            return false;
        }

        bool HookMembership(void* provider, std::int32_t action)
        {
            if (OwnsQuery(provider))
                return true;
            if (GetCurrentThreadId() == runtime.thread && scope.transaction.Active())
                return true;
            return runtime.originalMembership(provider, action);
        }

        bool HookAvailability(void* provider, std::int32_t action)
        {
            if (OwnsQuery(provider))
                return true;
            if (GetCurrentThreadId() == runtime.thread && scope.transaction.Active())
                return true;
            return runtime.originalAvailability(provider, action);
        }

        std::int32_t HookQuery(void* provider, std::int32_t action, float* value)
        {
            if (!OwnsQuery(provider))
            {
                if (GetCurrentThreadId() == runtime.thread && scope.transaction.Active())
                {
                    if (value)
                        *value = 0.0f;
                    return 1;
                }
                return runtime.originalQuery(provider, action, value);
            }
            NativeInputRouting::ActionSample sample{};
            if (!NativeInputRouting::QueryAction(scope.frame, static_cast<std::uint32_t>(action), sample))
            {
                Fail(Failure::InvalidState);
                sample = {0.0f, 1};
            }
            runtime.counters.queries.fetch_add(1);
            if (action != 2 && action != 0x1e && action != 0x1f)
                runtime.counters.neutral.fetch_add(1);
            if (value)
                *value = sample.value;
            return sample.state;
        }

        void ControlBody(void* ped)
        {
            runtime.counters.callbacks.fetch_add(1);
            if (GetCurrentThreadId() != runtime.thread)
            {
                runtime.counters.foreign.fetch_add(1);
                runtime.originalControl(ped);
                return;
            }
            if (scope.transaction.Active())
            {
                runtime.counters.nested.fetch_add(1);
                Fail(Failure::NestedCallback);
                if (!RestoreScope())
                    return;
                runtime.originalControl(ped);
                return;
            }
            if (!runtime.active.load() || reinterpret_cast<std::uintptr_t>(ped) != runtime.ped || !PrepareScope(ped))
            {
                // A partial apply followed by failed restoration must not run
                // another native consumer against stranded replacement bytes.
                if (scope.transaction.Active())
                    return;
                runtime.originalControl(ped);
                return;
            }
            // Keep structured cleanup around the real native call: early
            // returns and SEH unwinding both restore local bytes, while the
            // original exception continues to the game's own handler.
            __try
            {
                runtime.originalControl(ped);
                std::uint32_t moveState = 0;
                if (CheckBinding(ped) && Read(runtime.ped + 0x600, moveState))
                    runtime.counters.lastMoveState.store(moveState);
                else
                    Fail(Failure::BindingChanged);
            }
            __finally
            {
                if (AbnormalTermination())
                    Fail(Failure::OriginalException);
                if (RestoreScope() && runtime.active.load())
                    runtime.frame.previous = scope.frame.current;
            }
        }

        void HookControl(void* ped)
        {
            if (GetCurrentThreadId() != runtime.thread)
            {
                ControlBody(ped);
                return;
            }
            ++scope.callbackDepth;
            __try
            {
                ControlBody(ped);
            }
            __finally
            {
                --scope.callbackDepth;
            }
        }

        bool ChangeHooks(bool install) noexcept
        {
            std::vector<HANDLE> threads;
            const HANDLE        snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
            if (snapshot == INVALID_HANDLE_VALUE)
                return false;
            THREADENTRY32 entry{};
            entry.dwSize = sizeof(entry);
            bool enumerated = Thread32First(snapshot, &entry) != FALSE;
            bool ready = enumerated;
            try
            {
                while (enumerated)
                {
                    if (entry.th32OwnerProcessID == GetCurrentProcessId() && entry.th32ThreadID != GetCurrentThreadId())
                    {
                        HANDLE thread =
                            OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_SET_CONTEXT | THREAD_QUERY_INFORMATION, FALSE, entry.th32ThreadID);
                        if (!thread)
                        {
                            ready = false;
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
                }
            }
            catch (...)
            {
                ready = false;
            }
            CloseHandle(snapshot);
            bool transaction = false;
            if (ready && DetourTransactionBegin() == NO_ERROR)
            {
                transaction = true;
                ready = DetourUpdateThread(GetCurrentThread()) == NO_ERROR;
                for (HANDLE thread : threads)
                    ready = ready && DetourUpdateThread(thread) == NO_ERROR;
                const auto change = install ? DetourAttach : DetourDetach;
                if (ready)
                    ready = change(reinterpret_cast<PVOID*>(&runtime.originalControl), reinterpret_cast<PVOID>(&HookControl)) == NO_ERROR &&
                            change(reinterpret_cast<PVOID*>(&runtime.originalMembership), reinterpret_cast<PVOID>(&HookMembership)) == NO_ERROR &&
                            change(reinterpret_cast<PVOID*>(&runtime.originalAvailability), reinterpret_cast<PVOID>(&HookAvailability)) == NO_ERROR &&
                            change(reinterpret_cast<PVOID*>(&runtime.originalQuery), reinterpret_cast<PVOID>(&HookQuery)) == NO_ERROR;
                if (ready)
                {
                    ready = DetourTransactionCommit() == NO_ERROR;
                    transaction = false;
                }
            }
            if (transaction)
                DetourTransactionAbort();
            for (HANDLE thread : threads)
                CloseHandle(thread);
            return ready;
        }

        void ClearCounters() noexcept
        {
            runtime.counters.callbacks.store(0);
            runtime.counters.steps.store(0);
            runtime.counters.queries.store(0);
            runtime.counters.neutral.store(0);
            runtime.counters.restored.store(0);
            runtime.counters.restoreFailures.store(0);
            runtime.counters.foreign.store(0);
            runtime.counters.nested.store(0);
            runtime.counters.reason.store(0);
            runtime.counters.lastMoveState.store(0);
        }
    }

    bool NativeMovementInput::Begin(const MtaInputState& state) noexcept
    {
        AcquireSRWLockExclusive(&runtime.lock);
        if (runtime.hooked.load() || runtime.scopes.load() || runtime.active.load())
        {
            ReleaseSRWLockExclusive(&runtime.lock);
            return false;
        }
        ClearCounters();
        generation = runtime.generation.fetch_add(1) + 1;
        runtime.bound.store(false);
        runtime.thread = GetCurrentThreadId();
        runtime.ped = runtime.provider = 0;
        bool ready = NativeInputRouting::ValidateMovementState(state);
        if (!ready)
            Fail(Failure::InvalidState);
        // Wrong executables fail before pinning the module or creating hooks.
        NativeDeviceState physicalSources;
        if (ready && (!runtime.reader.Bind() || !runtime.reader.ReadDeviceState(physicalSources)))
        {
            Fail(Failure::UnsupportedBuild);
            ready = false;
        }
        runtime.image = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
        if (ready && (!CheckEntryPins() || !CheckBinding()))
        {
            Fail(Failure::BindingChanged);
            ready = false;
        }
        HMODULE retained = nullptr;
        if (ready &&
            !GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN, reinterpret_cast<LPCWSTR>(&HookControl), &retained))
        {
            Fail(Failure::HookInstall);
            ready = false;
        }
        if (ready)
        {
            runtime.originalControl = reinterpret_cast<ControlFunction>(runtime.image + controlRva);
            runtime.originalMembership = reinterpret_cast<MembershipFunction>(runtime.image + membershipRva);
            runtime.originalAvailability = reinterpret_cast<MembershipFunction>(runtime.image + availabilityRva);
            runtime.originalQuery = reinterpret_cast<QueryFunction>(runtime.image + queryRva);
            ready = ChangeHooks(true);
            if (!ready)
                Fail(Failure::HookInstall);
        }
        if (ready)
        {
            runtime.hooked.store(true);
            runtime.bound.store(true);
            runtime.frame = state;
            runtime.started = runtime.refreshed = GetTickCount64();
            runtime.active.store(true);
        }
        else
            runtime.reader.Reset();
        ReadState(finalState);
        hasFinalState = true;
        ReleaseSRWLockExclusive(&runtime.lock);
        return ready;
    }

    bool NativeMovementInput::Update(const MtaInputState& state) noexcept
    {
        AcquireSRWLockExclusive(&runtime.lock);
        bool ready = generation && generation == runtime.generation.load() && runtime.active.load() && !runtime.scopes.load();
        if (ready && GetCurrentThreadId() != runtime.thread)
        {
            Fail(Failure::WrongThread);
            ready = false;
        }
        if (ready && !NativeInputRouting::ValidateMovementState(state))
        {
            Fail(Failure::InvalidState);
            ready = false;
        }
        if (ready && (!Fresh() || !CheckBinding()))
        {
            Fail(Failure::BindingChanged);
            ready = false;
        }
        if (ready)
        {
            // Previous is consumed once by the native simulation, not replayed
            // as a fresh press on every script refresh of a held MTA frame.
            runtime.frame.current = state.current;
            runtime.refreshed = GetTickCount64();
        }
        ReleaseSRWLockExclusive(&runtime.lock);
        return ready;
    }

    bool NativeMovementInput::End() noexcept
    {
        AcquireSRWLockExclusive(&runtime.lock);
        if (!generation || generation != runtime.generation.load())
        {
            const bool harmless = !runtime.hooked.load();
            ReleaseSRWLockExclusive(&runtime.lock);
            return harmless;
        }
        runtime.active.store(false);
        if (GetCurrentThreadId() != runtime.thread || scope.callbackDepth)
        {
            Fail(GetCurrentThreadId() != runtime.thread ? Failure::WrongThread : Failure::ScopeActive);
            ReleaseSRWLockExclusive(&runtime.lock);
            return false;
        }
        // Failed finally cleanup retains a snapshot after the native call has
        // returned. Its owner can retry directly; only an in-flight callback
        // blocks End from restoring and then detaching the hooks.
        if (runtime.scopes.load() && (!scope.transaction.Active() || scope.generation != generation || !RestoreScope()))
        {
            Fail(Failure::RestoreFailure);
            ReleaseSRWLockExclusive(&runtime.lock);
            return false;
        }
        const bool clean = !runtime.hooked.load() || ChangeHooks(false);
        if (clean)
        {
            runtime.hooked.store(false);
            runtime.bound.store(false);
            runtime.reader.Reset();
        }
        else
            Fail(Failure::HookRemove);
        ReadState(finalState);
        hasFinalState = true;
        ReleaseSRWLockExclusive(&runtime.lock);
        return clean;
    }

    bool NativeMovementInput::IsBound() const noexcept
    {
        return generation && generation == runtime.generation.load() && runtime.bound.load();
    }

    bool NativeMovementInput::IsActive() const noexcept
    {
        return IsBound() && runtime.active.load();
    }

    void NativeMovementInput::ReadState(NativeMovementTrialState& state) const noexcept
    {
        state = hasFinalState ? finalState : NativeMovementTrialState{};
        if (!generation || generation != runtime.generation.load())
            return;
        state.bound = IsBound();
        state.active = IsActive();
        state.callbackCount = runtime.counters.callbacks.load();
        state.scopedSteps = runtime.counters.steps.load();
        state.routedQueries = runtime.counters.queries.load();
        state.neutralQueries = runtime.counters.neutral.load();
        state.restoredSteps = runtime.counters.restored.load();
        state.restoreFailures = runtime.counters.restoreFailures.load();
        state.foreignThreadCallbacks = runtime.counters.foreign.load();
        state.nestedCallbacks = runtime.counters.nested.load();
        state.failureReason = runtime.counters.reason.load();
        state.lastMoveState = runtime.counters.lastMoveState.load();
    }
}
