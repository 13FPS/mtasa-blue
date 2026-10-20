/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Quiescent, retained ownership of native global pad suppression
 *****************************************************************************/
#include "NativePadUpdateControl.h"
#include "NativePadUpdateProfile.h"
#include "NativePeerThreads.h"
#include "NativeFunctionRange.h"
#include "NativeImageAccess.h"
#include "NativeServiceAdmission.h"
#include <detours.h>
#include <array>
#include <cstdio>
#include <cstring>
#include <limits>
#include <new>

namespace mta::game_de
{
    namespace
    {
        namespace Profile = NativePadUpdateProfile;
        namespace Admission = NativeServiceAdmission;
        using UpdateFunction = void (*)();
        using Entry = std::array<unsigned char, 32>;

        struct Runtime
        {
            std::atomic<bool>          apiBusy{false}, bound{false}, hooked{false}, enabled{true}, stopping{false};
            std::atomic<std::uint64_t> abandonedGeneration{0};
            std::atomic<std::uint64_t> generation{0}, calls{0}, forwarded{0}, suppressed{0};
            std::atomic<std::uint32_t> inFlight{0};
            DWORD                      producerThread = 0;
            HMODULE                    module = nullptr;
            Profile::Binding           binding;
            Entry                      ownedEntry{};
            UpdateFunction             original = nullptr;
            NativeFunctionRange        hookRange;
            Admission::Token           admission;
            // Allocated before any suspension and intentionally process-lived.
            // Pending OS resume failure must survive producer/DLL teardown.
            NativePeerThreads* peers = nullptr;
        };
        Runtime runtime;

        struct Operation
        {
            bool acquired = !runtime.apiBusy.exchange(true);
            ~Operation()
            {
                if (acquired)
                    runtime.apiBusy.store(false);
            }
        };

        bool ReadEntry(Entry& output) noexcept
        {
            return NativeImageAccess::ReadImage(runtime.binding.image, runtime.binding.updater, output.data(), output.size(), false, true);
        }

        bool OwnedEntry() noexcept
        {
            Entry observed{};
            return runtime.hooked.load() && ReadEntry(observed) && observed == runtime.ownedEntry;
        }

        void Journal(const char* event) noexcept
        {
            // Diagnostics run after peer release. Reporting an OS failure must
            // never acquire loader/filesystem locks while a retained peer may
            // own them. Recovery remains authoritative if logging is skipped.
            if (!runtime.module || (runtime.peers && (runtime.peers->HasRecovery() || runtime.peers->Read().busy)))
                return;
            std::array<wchar_t, 4096> path{};
            const auto                length = GetModuleFileNameW(runtime.module, path.data(), static_cast<DWORD>(path.size()));
            if (!length || length >= path.size())
                return;
            std::size_t file = length;
            while (file && path[file - 1] != L'\\' && path[file - 1] != L'/')
                --file;
            constexpr wchar_t name[] = L"mta-pad-updater.log";
            if (!file || sizeof(name) / sizeof(wchar_t) > path.size() - file)
                return;
            std::memcpy(path.data() + file, name, sizeof(name));
            std::array<char, 768> line{};
            const auto            bytes = std::snprintf(
                line.data(), line.size(),
                "{\"event\":\"%s\",\"pid\":%lu,\"generation\":%llu,\"bound\":%u,\"hooked\":%u,\"enabled\":%u,"
                           "\"stopping\":%u,\"owned_entry\":%u,\"callbacks\":%u,\"calls\":%llu,\"forwarded\":%llu,\"suppressed\":%llu,\"retained_peers\":%u}\n",
                event, GetCurrentProcessId(), static_cast<unsigned long long>(runtime.generation.load()), runtime.bound.load() ? 1u : 0u,
                runtime.hooked.load() ? 1u : 0u, runtime.enabled.load() ? 1u : 0u, runtime.stopping.load() ? 1u : 0u, OwnedEntry() ? 1u : 0u,
                runtime.inFlight.load(), static_cast<unsigned long long>(runtime.calls.load()), static_cast<unsigned long long>(runtime.forwarded.load()),
                static_cast<unsigned long long>(runtime.suppressed.load()), runtime.peers ? runtime.peers->Read().retained : 0u);
            if (bytes <= 0 || static_cast<std::size_t>(bytes) >= line.size())
                return;
            const auto handle = CreateFileW(path.data(), FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_ALWAYS,
                                            FILE_ATTRIBUTE_NORMAL, nullptr);
            if (handle == INVALID_HANDLE_VALUE)
                return;
            DWORD written = 0;
            WriteFile(handle, line.data(), static_cast<DWORD>(bytes), &written, nullptr);
            CloseHandle(handle);
        }

        bool ActiveOwner(std::uint64_t generation) noexcept
        {
            return generation && generation == runtime.generation.load() && runtime.bound.load() && runtime.hooked.load() && !runtime.stopping.load() &&
                   GetCurrentThreadId() == runtime.producerThread && OwnedEntry() && Profile::Revalidate(runtime.binding, false);
        }

        __declspec(noinline) void HookPadUpdate()
        {
            runtime.inFlight.fetch_add(1);
            __try
            {
                runtime.calls.fetch_add(1);
                // Cleanup immediately releases suppression. A retained hook is
                // passive while its trampoline and peers await safe retirement.
                if (runtime.enabled.load() || runtime.stopping.load())
                {
                    runtime.forwarded.fetch_add(1);
                    runtime.original();
                }
                else
                    runtime.suppressed.fetch_add(1);
            }
            __finally
            {
                runtime.inFlight.fetch_sub(1);
            }
        }

        bool PeerSafe(const CONTEXT& context, void*) noexcept
        {
            const auto pc = static_cast<std::uintptr_t>(context.Rip);
            // The updater and hook both have chained unwind fragments. Check
            // the complete cached family, including entry before inFlight++.
            const auto updaterEnd = runtime.binding.updater + (Profile::EndRva - Profile::UpdaterRva);
            if ((pc >= runtime.binding.updater && pc < updaterEnd) || runtime.hookRange.Contains(pc))
                return false;
            if (runtime.hooked.load())
            {
                const auto trampoline = reinterpret_cast<std::uintptr_t>(runtime.original);
                if (pc >= trampoline && pc - trampoline < 256)
                    return false;
            }
            return true;
        }

        bool ChangeHook(bool install) noexcept
        {
            if (!runtime.peers || !runtime.peers->RetryRecover() || !Profile::Revalidate(runtime.binding, install))
                return false;
            NativePeerThreads::Hold hold(*runtime.peers);
            if (!hold.Prepare() || !hold.Suspend(PeerSafe, nullptr))
            {
                runtime.peers->RetryRecover();
                return false;
            }
            // From here until Release: no heap work, producer mailbox lock or
            // logging. Opt-in Detours uses VM nodes for parked transactions.
            bool  ready = runtime.inFlight.load() == 0;
            bool  transaction = false;
            Entry observed{};
            if (ready)
                ready = ReadEntry(observed) && observed == (install ? Profile::EntryBytes : runtime.ownedEntry);
            // Full PE/unwind validation happened before parking. Calling
            // loader/unwind lookup here could wait for a parked peer's lock.
            if (ready && DetourTransactionBegin() == NO_ERROR)
            {
                transaction = true;
                ready = hold.RegisterDetours();
                if (ready)
                    ready = (install ? DetourAttach : DetourDetach)(reinterpret_cast<PVOID*>(&runtime.original), reinterpret_cast<PVOID>(&HookPadUpdate)) ==
                            NO_ERROR;
                if (ready)
                {
                    ready = DetourTransactionCommit() == NO_ERROR;
                    transaction = false;
                    if (ready)
                    {
                        // Record the mutation before resuming anybody. Resume
                        // failure must retain a real installed hook as owned.
                        runtime.hooked.store(install);
                        if (install)
                            ready = ReadEntry(runtime.ownedEntry) && runtime.ownedEntry != Profile::EntryBytes;
                        else
                        {
                            Entry restored{};
                            ready = ReadEntry(restored) && restored == Profile::EntryBytes;
                        }
                    }
                }
            }
            else
                ready = false;
            if (transaction)
                DetourTransactionAbort();
            const bool released = hold.Release();
            if (!released)
                runtime.peers->RetryRecover();
            return ready && !runtime.peers->HasRecovery();
        }

        bool Retire(std::uint64_t generation) noexcept
        {
            if (!generation || generation != runtime.generation.load())
                return false;
            runtime.stopping.store(true);
            runtime.enabled.store(true);
            if (!runtime.peers || !runtime.peers->RetryRecover() || runtime.inFlight.load())
                return false;
            if (runtime.hooked.load() && !ChangeHook(false))
                return false;
            if (runtime.hooked.load() || runtime.peers->HasRecovery())
                return false;
            // Even a nominal detach needs original-entry readback. Never
            // release admission around an unexplained native code mutation.
            Entry observed{};
            if (!ReadEntry(observed) || observed != Profile::EntryBytes || !Profile::Revalidate(runtime.binding))
                return false;
            if (!Admission::Release(runtime.admission))
                return false;
            runtime.admission = {};
            runtime.bound.store(false);
            runtime.abandonedGeneration.store(0);
            runtime.original = nullptr;
            return true;
        }

        bool Start(const Profile::Binding& binding, std::uint64_t& adopted) noexcept
        {
            if (runtime.bound.load() || runtime.hooked.load() || runtime.inFlight.load() || (runtime.peers && runtime.peers->HasRecovery()))
                return false;
            const auto previous = runtime.generation.load();
            if (previous == (std::numeric_limits<std::uint64_t>::max)())
                return false;
            NativeFunctionRange hookRange;
            if (!FindNativeFunctionRange(reinterpret_cast<const void*>(&HookPadUpdate), hookRange))
                return false;
            HMODULE module = nullptr;
            if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN, reinterpret_cast<const wchar_t*>(&HookPadUpdate),
                                    &module))
                return false;
            runtime.module = module;
            if (!runtime.peers)
                runtime.peers = new (std::nothrow) NativePeerThreads;
            if (!runtime.peers || !Admission::TryAcquire(Admission::Category::PadUpdate, runtime.admission))
                return false;
            runtime.binding = binding;
            runtime.hookRange = hookRange;
            runtime.original = reinterpret_cast<UpdateFunction>(binding.updater);
            runtime.ownedEntry = {};
            runtime.producerThread = GetCurrentThreadId();
            runtime.generation.store(previous + 1);
            runtime.calls.store(0);
            runtime.forwarded.store(0);
            runtime.suppressed.store(0);
            runtime.enabled.store(true);
            runtime.stopping.store(false);
            runtime.abandonedGeneration.store(0);
            runtime.bound.store(true);
            adopted = previous + 1;
            if (ChangeHook(true))
                return true;
            Retire(adopted);
            return false;
        }

        NativePadUpdateState Snapshot(std::uint64_t owned) noexcept
        {
            NativePadUpdateState next;
            if (owned && owned == runtime.generation.load())
            {
                next.generation = owned;
                next.bound = runtime.bound.load();
                next.hooked = runtime.hooked.load();
                next.enabled = runtime.enabled.load();
                next.stopping = runtime.stopping.load();
                next.abandoned = runtime.abandonedGeneration.load() == owned;
                next.ownedEntry = OwnedEntry();
                next.calls = runtime.calls.load();
                next.forwarded = runtime.forwarded.load();
                next.suppressed = runtime.suppressed.load();
                next.callbacks = runtime.inFlight.load();
                next.retainedPeers = runtime.peers ? runtime.peers->Read().retained : 0;
            }
            return next;
        }
    }

    NativePadUpdateControl::~NativePadUpdateControl()
    {
        if (generation.load() && !End())
        {
            // No callback points at this object. Retain process-level recovery
            // for an explicit shutdown helper or a future producer's recovery.
            // Generation tagging cannot mark a later producer abandoned if a
            // concurrent cleanup completed between this destructor's checks.
            runtime.abandonedGeneration.store(generation.load());
        }
    }

    bool NativePadUpdateControl::Begin() noexcept
    {
        Operation operation;
        if (!operation.acquired || generation.load())
            return false;
        Profile::Binding binding;
        if (!Profile::Qualify(binding))
            return false;
        std::uint64_t adopted = 0;
        const bool    ready = Start(binding, adopted);
        if (adopted)
        {
            hasFinalState = false;
            if (runtime.bound.load())
                generation.store(adopted);
            else
            {
                finalState = Snapshot(adopted);
                hasFinalState = true;
            }
        }
        if (adopted)
            Journal(ready ? "begin" : "begin_refused");
        return ready;
    }

    bool NativePadUpdateControl::IsEnabled(bool& output) const noexcept
    {
        Operation operation;
        if (!operation.acquired || !ActiveOwner(generation.load()))
            return false;
        output = runtime.enabled.load();
        return true;
    }

    bool NativePadUpdateControl::SetDisabled(bool disabled) noexcept
    {
        Operation operation;
        if (!operation.acquired || !ActiveOwner(generation.load()))
            return false;
        runtime.enabled.store(!disabled);
        return true;
    }

    bool NativePadUpdateControl::End() noexcept
    {
        Operation operation;
        if (!operation.acquired)
            return false;
        const auto owned = generation.load();
        if (!owned)
            return true;
        if (!runtime.bound.load() && owned == runtime.generation.load())
        {
            finalState = Snapshot(owned);
            hasFinalState = true;
            generation.store(0);
            return true;
        }
        if (!Retire(owned))
            return false;
        // Keep the retired generation's counters for diagnostic readback;
        // observing End must not erase the evidence that it restored ownership.
        finalState = Snapshot(owned);
        hasFinalState = true;
        generation.store(0);
        Journal("end");
        return true;
    }

    void NativePadUpdateControl::Read(NativePadUpdateState& output) const noexcept
    {
        Operation operation;
        if (!operation.acquired)
            return;
        const auto owned = generation.load();
        output = !owned && hasFinalState ? finalState : Snapshot(owned);
    }

    bool NativePadUpdateControl::RecoverAbandoned() noexcept
    {
        Operation operation;
        if (!operation.acquired)
            return false;
        if (!runtime.bound.load())
            return true;
        if (runtime.abandonedGeneration.load() != runtime.generation.load() || !Retire(runtime.generation.load()))
            return false;
        Journal("recover_abandoned");
        return true;
    }
}
