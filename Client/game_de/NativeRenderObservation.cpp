/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Observe an engine-owned Present scope without worker COM access
 *****************************************************************************/

#include "NativeRenderObservation.h"
#include "NativeD3D12OwnershipProbe.h"
#include <array>
#include <atomic>
#include <limits>
#include <vector>
#include <windows.h>
#include <tlhelp32.h>
#include <bcrypt.h>
#include <detours.h>

namespace mta::game_de
{
    namespace
    {
        using Failure = NativeRenderObservationFailure;
        using PresentFunction = bool (*)(void*, std::uint32_t);
        constexpr ULONGLONG                     maximumObservationMs = 30000;
        constexpr std::array<unsigned char, 32> executableHash = {0x63, 0xe6, 0x33, 0xbb, 0x51, 0xc3, 0x5f, 0xe3, 0x5c, 0xa9, 0x8f,
                                                                  0x50, 0x3b, 0x62, 0xe7, 0xdd, 0xb8, 0x40, 0x3a, 0xbe, 0x4c, 0x9d,
                                                                  0x50, 0x1c, 0xfa, 0x91, 0x62, 0x89, 0x24, 0x53, 0x84, 0x6f};
        struct RouteDescriptor
        {
            RenderObservationRoute        route;
            std::uintptr_t                presentRva;
            std::size_t                   presentBytes;
            std::array<unsigned char, 32> presentHash;
            std::uintptr_t                viewportTable, swapchainOffset;
        };
        constexpr RouteDescriptor d3d11Route = {RenderObservationRoute::D3D11,
                                                0x1f5a430,
                                                421,
                                                {0x60, 0x5f, 0xf6, 0x63, 0x24, 0x56, 0x99, 0x2a, 0xb7, 0x4e, 0x61, 0xcb, 0x14, 0xff, 0x2f, 0xdb,
                                                 0xb5, 0x74, 0xaf, 0x74, 0xf7, 0x49, 0x35, 0x74, 0x0e, 0x66, 0xfd, 0xf6, 0x3b, 0x2a, 0x98, 0x8f},
                                                0x44fbc08,
                                                0x78};
        // These are complete unwind-bounded functions in the same pinned PE,
        // not old SDK hints or a guess based on an inactive D3D11 route.
        constexpr RouteDescriptor d3d12Route = {RenderObservationRoute::D3D12,
                                                0x1fd5da0,
                                                429,
                                                {0x37, 0x6c, 0xfc, 0x00, 0xed, 0x38, 0x24, 0x9e, 0x3e, 0x22, 0x6a, 0x32, 0xde, 0xab, 0xc2, 0x5d,
                                                 0x2d, 0xd1, 0xe5, 0x25, 0x62, 0x1b, 0xab, 0xa6, 0x9f, 0x23, 0x62, 0xc2, 0x9a, 0x99, 0x93, 0x4e},
                                                0x4504060,
                                                0x60};

        const RouteDescriptor* SelectRoute(RenderObservationRoute route) noexcept
        {
            return route == RenderObservationRoute::D3D11 ? &d3d11Route : route == RenderObservationRoute::D3D12 ? &d3d12Route : nullptr;
        }

        struct Runtime
        {
            std::atomic<bool>            apiBusy{false}, bound{false}, active{false}, hooked{false}, modulePinned{false};
            std::atomic<unsigned>        inFlight{0};
            std::atomic<std::uint64_t>   generation{0}, callbacks{0}, accepted{0}, rejected{0}, nested{0}, foreign{0}, missed{0};
            std::atomic<std::uint32_t>   failure{0};
            std::atomic<DWORD>           callbackThread{0};
            SRWLOCK                      snapshotLock = SRWLOCK_INIT;
            NativeRenderObservationState snapshot;
            DWORD                        producerThread = 0;
            std::uintptr_t               image = 0;
            ULONGLONG                    started = 0;
            PresentFunction              original = nullptr;
            const RouteDescriptor*       route = nullptr;
            std::uint32_t                requestedRoute = 0;
            bool                         queryGpuOwnership = false;
        };
        // Failed teardown must retain its trampoline and callback storage.
        // Nothing in this process-lifetime state points at a released owner.
        Runtime               runtime;
        thread_local unsigned callbackDepth = 0;

        struct ApiOperation
        {
            bool acquired;
            ApiOperation() noexcept : acquired(!runtime.apiBusy.exchange(true)) {}
            ~ApiOperation()
            {
                if (acquired)
                    runtime.apiBusy.store(false);
            }
        };

        void Fail(Failure failure) noexcept
        {
            runtime.active.store(false);
            std::uint32_t expected = 0;
            runtime.failure.compare_exchange_strong(expected, static_cast<std::uint32_t>(failure));
        }

        bool Pages(std::uintptr_t address, std::size_t bytes, std::uintptr_t image = 0, bool executable = false) noexcept
        {
            if (!address || !bytes || bytes > (std::numeric_limits<std::uintptr_t>::max)() - address)
                return false;
            const auto end = address + bytes;
            while (address < end)
            {
                MEMORY_BASIC_INFORMATION info{};
                if (VirtualQuery(reinterpret_cast<void*>(address), &info, sizeof(info)) != sizeof(info) || info.State != MEM_COMMIT ||
                    (info.Protect & (PAGE_GUARD | PAGE_NOACCESS)) ||
                    (image && (info.Type != MEM_IMAGE || reinterpret_cast<std::uintptr_t>(info.AllocationBase) != image)))
                    return false;
                const auto protection = info.Protect & 0xff;
                const bool execute = protection == PAGE_EXECUTE_READ || protection == PAGE_EXECUTE_READWRITE || protection == PAGE_EXECUTE_WRITECOPY;
                if (!(execute || protection == PAGE_READONLY || protection == PAGE_READWRITE || protection == PAGE_WRITECOPY) || (executable && !execute))
                    return false;
                const auto start = reinterpret_cast<std::uintptr_t>(info.BaseAddress);
                if (info.RegionSize > (std::numeric_limits<std::uintptr_t>::max)() - start || start + info.RegionSize <= address)
                    return false;
                address = (start + info.RegionSize < end) ? start + info.RegionSize : end;
            }
            return true;
        }

        bool Read(std::uintptr_t address, void* destination, std::size_t bytes, std::uintptr_t image = 0, bool executable = false) noexcept
        {
            SIZE_T copied = 0;
            return Pages(address, bytes, image, executable) &&
                   ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(address), destination, bytes, &copied) && copied == bytes;
        }
        template <typename T>
        bool Read(std::uintptr_t address, T& value) noexcept
        {
            return Read(address, &value, sizeof(value));
        }

        struct HashResources
        {
            HANDLE             file = INVALID_HANDLE_VALUE;
            BCRYPT_ALG_HANDLE  algorithm = nullptr;
            BCRYPT_HASH_HANDLE hash = nullptr;
            ~HashResources()
            {
                if (hash)
                    BCryptDestroyHash(hash);
                if (algorithm)
                    BCryptCloseAlgorithmProvider(algorithm, 0);
                if (file != INVALID_HANDLE_VALUE)
                    CloseHandle(file);
            }
            bool Open() noexcept
            {
                return BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) >= 0 &&
                       BCryptCreateHash(algorithm, &hash, nullptr, 0, nullptr, 0, 0) >= 0;
            }
            bool Finish(const std::array<unsigned char, 32>& expected) noexcept
            {
                std::array<unsigned char, 32> digest{};
                return BCryptFinishHash(hash, digest.data(), static_cast<ULONG>(digest.size()), 0) >= 0 && digest == expected;
            }
        };

        bool ExactExecutable() noexcept
        {
            std::array<wchar_t, 32768> path{};
            const auto                 size = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
            if (!size || size >= path.size() || sizeof(void*) != 8)
                return false;
            HashResources hash;
            hash.file = CreateFileW(path.data(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
            LARGE_INTEGER bytes{};
            if (hash.file == INVALID_HANDLE_VALUE || !GetFileSizeEx(hash.file, &bytes) || bytes.QuadPart != 89497064 || !hash.Open())
                return false;
            std::array<unsigned char, 65536> buffer{};
            for (;;)
            {
                DWORD count = 0;
                if (!ReadFile(hash.file, buffer.data(), static_cast<DWORD>(buffer.size()), &count, nullptr))
                    return false;
                if (!count)
                    break;
                if (BCryptHashData(hash.hash, buffer.data(), count, 0) < 0)
                    return false;
            }
            return hash.Finish(executableHash);
        }

        bool CheckFingerprint(std::uintptr_t image, const RouteDescriptor& route) noexcept
        {
            IMAGE_DOS_HEADER   dos{};
            IMAGE_NT_HEADERS64 nt{};
            if (!Read(image, &dos, sizeof(dos), image) || dos.e_magic != IMAGE_DOS_SIGNATURE || dos.e_lfanew < static_cast<LONG>(sizeof(dos)) ||
                dos.e_lfanew > 1048576 || !Read(image + dos.e_lfanew, &nt, sizeof(nt), image) || nt.Signature != IMAGE_NT_SIGNATURE ||
                nt.FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64 || nt.OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC ||
                nt.OptionalHeader.SizeOfImage != 97141760)
                return false;
            std::array<unsigned char, 429> code{};
            HashResources                  hash;
            // The complete unwind-bounded function has no base relocations;
            // hashing only a prologue could miss a changed member/vcall path.
            return route.presentBytes <= code.size() && Read(image + route.presentRva, code.data(), route.presentBytes, image, true) && hash.Open() &&
                   BCryptHashData(hash.hash, code.data(), static_cast<ULONG>(route.presentBytes), 0) >= 0 && hash.Finish(route.presentHash);
        }

        bool Capture(void* viewport, std::uint32_t syncInterval, NativeRenderObservationState& output) noexcept
        {
            const auto object = reinterpret_cast<std::uintptr_t>(viewport);
            if (!runtime.route)
                return false;
            const auto&                  route = *runtime.route;
            std::uintptr_t               table = 0, owner = 0, window = 0, swapchain = 0;
            NativeRenderObservationState next;
            if (!Read(object, table) || table != runtime.image + route.viewportTable || !Read(object + 0x18, owner) || !owner || !Read(object + 0x40, window) ||
                !window || !Read(object + 0x4c, next.width) || !next.width || next.width > 32768 || !Read(object + 0x50, next.height) || !next.height ||
                next.height > 32768 || !Read(object + route.swapchainOffset, swapchain) || !swapchain)
                return false;
            if (route.route == RenderObservationRoute::D3D11)
            {
                std::uintptr_t primary = 0, secondary = 0, global = 0, device = 0, context = 0;
                if (!Read(owner, primary) || primary != runtime.image + 0x44fb2b8 || !Read(owner + 0x18, secondary) || secondary != runtime.image + 0x44fafa8 ||
                    !Read(runtime.image + 0x576fb60, global) || global != owner || !Read(owner + 0x170, device) || !device || !Read(owner + 0x158, context) ||
                    !context)
                    return false;
                std::uintptr_t globalAgain = 0;
                if (!Read(runtime.image + 0x576fb60, globalAgain) || globalAgain != global)
                    return false;
                next.devicePresent = next.contextPresent = true;
                next.presenceKnownMask = 15;
            }
            else
            {
                // The pinned D3D12 creator explicitly passes pCommandQueue:
                // owner+8A8 -> device object+38 -> manager+28. Neither the
                // device object nor this queue is an immediate D3D11 context.
                // Only non-null queue presence is borrowed within this call.
                std::uintptr_t deviceObject = 0, manager = 0, queue = 0, deviceAgain = 0, managerAgain = 0, queueAgain = 0;
                if (!Read(owner + 0x8a8, deviceObject) || !deviceObject || !Read(deviceObject + 0x38, manager) || !manager || !Read(manager + 0x28, queue) ||
                    !queue || !Read(owner + 0x8a8, deviceAgain) || deviceAgain != deviceObject || !Read(deviceObject + 0x38, managerAgain) ||
                    managerAgain != manager || !Read(manager + 0x28, queueAgain) || queueAgain != queue)
                    return false;
                next.presenceKnownMask = 19;
                next.queuePresent = 1;
            }
            std::uintptr_t tableAgain = 0, ownerAgain = 0, swapchainAgain = 0;
            if (!Read(object, tableAgain) || tableAgain != table || !Read(object + 0x18, ownerAgain) || ownerAgain != owner ||
                !Read(object + route.swapchainOffset, swapchainAgain) || swapchainAgain != swapchain)
                return false;
            // The engine owns these pointers only for its natural call. Copy
            // presence/scalars and transfer no raw pointer to Core. API3's
            // separate callback-local probe releases its COM refs before return.
            next.callbackThreadId = GetCurrentThreadId();
            next.syncInterval = syncInterval;
            next.windowPresent = next.swapchainPresent = true;
            next.observedRoute = static_cast<std::uint32_t>(route.route);
            output = next;
            return true;
        }

        struct ProbeRoots
        {
            std::uintptr_t owner = 0, device = 0, manager = 0, queue = 0, swapchain = 0, customPresent = 0;
            std::uint32_t  explicitGpuCount = 0, afrCount = 0;
        };

        bool ReadProbeRoots(void* viewport, ProbeRoots& roots) noexcept
        {
            const auto object = reinterpret_cast<std::uintptr_t>(viewport);
            return Read(object + 0x18, roots.owner) && roots.owner && Read(roots.owner + 0x8a8, roots.device) && roots.device &&
                   Read(roots.device + 0x38, roots.manager) && roots.manager && Read(roots.manager + 0x28, roots.queue) && roots.queue &&
                   Read(object + 0x60, roots.swapchain) && roots.swapchain && Read(object + 0x1d0, roots.customPresent) &&
                   Read(runtime.image + 0x503e8e0, roots.explicitGpuCount) && Read(runtime.image + 0x503f45c, roots.afrCount);
        }

        bool SameProbeRoots(const ProbeRoots& a, const ProbeRoots& b) noexcept
        {
            return a.owner == b.owner && a.device == b.device && a.manager == b.manager && a.queue == b.queue && a.swapchain == b.swapchain &&
                   a.customPresent == b.customPresent && a.explicitGpuCount == b.explicitGpuCount && a.afrCount == b.afrCount;
        }

        bool BorrowedComRoot(std::uintptr_t object) noexcept
        {
            std::uintptr_t table = 0, queryInterface = 0;
            return Read(object, table) && Read(table, queryInterface) && Pages(queryInterface, 1, 0, true);
        }

        void CaptureOwnership(void* viewport, GpuOwnershipProbeState& state) noexcept
        {
            using Stage = GpuOwnershipQueryStage;
            using QueryStatus = GpuOwnershipQueryStatus;
            state.queryStage = static_cast<std::uint32_t>(Stage::NativeRoots);
            state.queryStatus = static_cast<std::uint32_t>(QueryStatus::Failed);
            ProbeRoots roots;
            if (!ReadProbeRoots(viewport, roots))
            {
                state.hresult = static_cast<std::uint32_t>(E_POINTER);
                return;
            }
            state.customPresentPresent = roots.customPresent ? 1u : 0u;
            state.nativeExplicitGPUCount = roots.explicitGpuCount;
            state.nativeAFRCount = roots.afrCount;
            // A custom presenter or indexed multi-GPU route may choose another
            // queue/resource. Observe those facts without querying a guess.
            state.queryStatus = static_cast<std::uint32_t>(QueryStatus::Unsupported);
            state.queryStage = static_cast<std::uint32_t>(Stage::CustomPresent);
            if (roots.customPresent)
                return;
            state.queryStage = static_cast<std::uint32_t>(Stage::SingleNode);
            if (roots.explicitGpuCount != 1 || roots.afrCount != 1)
                return;
            if (!BorrowedComRoot(roots.swapchain) || !BorrowedComRoot(roots.queue))
            {
                state.queryStatus = static_cast<std::uint32_t>(QueryStatus::Failed);
                state.hresult = static_cast<std::uint32_t>(E_POINTER);
                return;
            }
            // All acquired refs are released before the engine original can
            // rotate its buffers. This probe issues no GPU work or waits.
            ProbeD3D12Ownership(reinterpret_cast<void*>(roots.swapchain), reinterpret_cast<void*>(roots.queue), state);
            ProbeRoots after;
            if ((!ReadProbeRoots(viewport, after) || !SameProbeRoots(roots, after)) &&
                state.queryStatus != static_cast<std::uint32_t>(QueryStatus::CleanupFailed))
            {
                state.queryStage = static_cast<std::uint32_t>(Stage::RootReadback);
                state.queryStatus = static_cast<std::uint32_t>(QueryStatus::RootsChanged);
                state.metadataValid = state.supportPredicate = 0;
                state.hresult = 0;
            }
        }

        bool PresentBody(void* viewport, std::uint32_t syncInterval)
        {
            runtime.callbacks.fetch_add(1);
            if (runtime.active.load())
            {
                if (callbackDepth != 1)
                {
                    runtime.nested.fetch_add(1);
                    Fail(Failure::NestedCallback);
                }
                const auto now = GetTickCount64();
                if (now < runtime.started || now - runtime.started >= maximumObservationMs)
                    Fail(Failure::TimeBudget);
            }
            NativeRenderObservationState capture;
            bool                         accepted = false;
            if (runtime.active.load())
            {
                if (!Capture(viewport, syncInterval, capture))
                {
                    runtime.rejected.fetch_add(1);
                    Fail(Failure::InvalidViewport);
                }
                else
                {
                    DWORD expected = 0;
                    runtime.callbackThread.compare_exchange_strong(expected, GetCurrentThreadId());
                    if (runtime.callbackThread.load() != GetCurrentThreadId())
                        runtime.foreign.fetch_add(1);
                    else
                        accepted = true;
                }
            }
            bool fatalCleanup = false;
            if (accepted && runtime.queryGpuOwnership)
            {
                CaptureOwnership(viewport, capture.gpu);
                fatalCleanup = capture.gpu.queryStatus == static_cast<std::uint32_t>(GpuOwnershipQueryStatus::CleanupFailed);
                // Disarm before either the original or a contended publication
                // can throw/drop this metadata. Unknown Release effects must
                // never be repeated or hidden by a missed snapshot.
                if (fatalCleanup)
                    Fail(Failure::GpuOwnershipCleanup);
            }
            const bool result = runtime.original(viewport, syncInterval);
            if (accepted && (runtime.active.load() || fatalCleanup))
            {
                if (TryAcquireSRWLockExclusive(&runtime.snapshotLock))
                {
                    capture.lastOriginalReturned = true;
                    capture.lastOriginalResult = result;
                    if (runtime.queryGpuOwnership)
                    {
                        // These count published probes. A missed publication
                        // cannot attach new counters to older COM metadata.
                        const auto previous = runtime.snapshot.gpu;
                        capture.gpu.attempts = previous.attempts + 1;
                        capture.gpu.accepted = previous.accepted + (capture.gpu.queryStatus == static_cast<std::uint32_t>(GpuOwnershipQueryStatus::Accepted));
                        capture.gpu.rejected = previous.rejected + (capture.gpu.queryStatus != static_cast<std::uint32_t>(GpuOwnershipQueryStatus::Accepted));
                    }
                    runtime.snapshot = capture;
                    runtime.accepted.fetch_add(1);
                    ReleaseSRWLockExclusive(&runtime.snapshotLock);
                }
                else
                    runtime.missed.fetch_add(1);
            }
            return result;
        }

        bool HookPresent(void* viewport, std::uint32_t syncInterval)
        {
            runtime.inFlight.fetch_add(1);
            ++callbackDepth;
            __try
            {
                return PresentBody(viewport, syncInterval);
            }
            __finally
            {
                --callbackDepth;
                runtime.inFlight.fetch_sub(1);
            }
        }

        bool ChangeHooks(bool install, bool& pending) noexcept
        {
            pending = false;
            std::vector<HANDLE> threads;
            const auto          snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
            if (snapshot == INVALID_HANDLE_VALUE)
                return false;
            THREADENTRY32 entry{};
            entry.dwSize = sizeof(entry);
            bool enumerated = Thread32First(snapshot, &entry) != FALSE, ready = enumerated;
            try
            {
                while (enumerated)
                {
                    if (entry.th32OwnerProcessID == GetCurrentProcessId() && entry.th32ThreadID != GetCurrentThreadId())
                    {
                        const auto thread =
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
                for (const auto thread : threads)
                    ready = ready && DetourUpdateThread(thread) == NO_ERROR;
                // No callback-shared lock is held while Detours suspends threads.
                // A callback can enter after End's optimistic check; abort and
                // resume it rather than freeing its live trampoline.
                if (ready && !install && runtime.inFlight.load())
                {
                    pending = true;
                    ready = false;
                }
                if (ready)
                    ready =
                        (install ? DetourAttach : DetourDetach)(reinterpret_cast<PVOID*>(&runtime.original), reinterpret_cast<PVOID>(&HookPresent)) == NO_ERROR;
                if (ready)
                {
                    ready = DetourTransactionCommit() == NO_ERROR;
                    transaction = false;
                }
            }
            if (transaction)
                DetourTransactionAbort();
            for (const auto thread : threads)
                CloseHandle(thread);
            return ready;
        }

        void ResetCounters() noexcept
        {
            runtime.callbacks.store(0);
            runtime.accepted.store(0);
            runtime.rejected.store(0);
            runtime.nested.store(0);
            runtime.foreign.store(0);
            runtime.missed.store(0);
            runtime.failure.store(0);
            runtime.callbackThread.store(0);
            AcquireSRWLockExclusive(&runtime.snapshotLock);
            runtime.snapshot = {};
            ReleaseSRWLockExclusive(&runtime.snapshotLock);
        }
    }

    NativeRenderObservation::~NativeRenderObservation()
    {
        End();
    }

    bool NativeRenderObservation::Begin(RenderObservationRoute route, bool queryGpuOwnership) noexcept
    {
        ApiOperation operation;
        if (!operation.acquired || runtime.hooked.load() || runtime.inFlight.load() || runtime.active.load())
            return false;
        generation = runtime.generation.fetch_add(1) + 1;
        finalState = {};
        runtime.bound.store(false);
        runtime.producerThread = GetCurrentThreadId();
        runtime.image = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
        ResetCounters();
        runtime.requestedRoute = static_cast<std::uint32_t>(route);
        runtime.queryGpuOwnership = queryGpuOwnership;
        runtime.route = SelectRoute(route);
        if (!runtime.route || (queryGpuOwnership && route != RenderObservationRoute::D3D12))
        {
            Fail(Failure::UnsupportedRoute);
            ReadState(finalState);
            return false;
        }
        if (!ExactExecutable())
        {
            Fail(Failure::UnsupportedBuild);
            ReadState(finalState);
            return false;
        }
        if (!CheckFingerprint(runtime.image, *runtime.route))
        {
            Fail(Failure::FingerprintMismatch);
            ReadState(finalState);
            return false;
        }
        HMODULE pinned = nullptr;
        if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN, reinterpret_cast<LPCWSTR>(&HookPresent), &pinned))
        {
            Fail(Failure::HookInstall);
            ReadState(finalState);
            return false;
        }
        runtime.modulePinned.store(true);
        runtime.original = reinterpret_cast<PresentFunction>(runtime.image + runtime.route->presentRva);
        bool pending = false;
        if (!ChangeHooks(true, pending))
        {
            Fail(Failure::HookInstall);
            ReadState(finalState);
            return false;
        }
        runtime.hooked.store(true);
        runtime.bound.store(true);
        runtime.started = GetTickCount64();
        runtime.active.store(true);
        ReadState(finalState);
        return true;
    }

    bool NativeRenderObservation::End() noexcept
    {
        ApiOperation operation;
        if (!operation.acquired)
            return false;
        if (!generation || generation != runtime.generation.load())
            return !runtime.hooked.load();
        runtime.active.store(false);
        if (GetCurrentThreadId() != runtime.producerThread)
        {
            Fail(Failure::WrongProducerThread);
            return false;
        }
        if (runtime.inFlight.load())
        {
            ReadState(finalState);
            return false;
        }
        bool pending = false;
        if (runtime.hooked.load() && !ChangeHooks(false, pending))
        {
            if (!pending)
                Fail(Failure::HookRemove);
            ReadState(finalState);
            return false;
        }
        runtime.hooked.store(false);
        ReadState(finalState);
        return true;
    }

    void NativeRenderObservation::ReadState(NativeRenderObservationState& state) const noexcept
    {
        state = finalState;
        state.modulePinned = runtime.modulePinned.load();
        if (!generation || generation != runtime.generation.load())
            return;
        if (runtime.active.load())
        {
            const auto now = GetTickCount64();
            if (now < runtime.started || now - runtime.started >= maximumObservationMs)
                Fail(Failure::TimeBudget);
        }
        AcquireSRWLockShared(&runtime.snapshotLock);
        state = runtime.snapshot;
        // A first publication changes route/scalars and accepted together.
        // Sampling the count after unlocking could attach accepted=1 to an
        // older unobserved snapshot, contradicting the value-only contract.
        state.acceptedSnapshots = runtime.accepted.load();
        ReleaseSRWLockShared(&runtime.snapshotLock);
        state.generation = generation;
        state.requestedRoute = runtime.requestedRoute;
        state.modulePinned = runtime.modulePinned.load();
        state.bound = runtime.bound.load();
        state.active = runtime.active.load();
        state.pendingCleanup = runtime.hooked.load() && !state.active;
        // Completed effects precede their monotonically increasing prerequisite.
        state.rejectedSnapshots = runtime.rejected.load();
        state.nestedCallbacks = runtime.nested.load();
        state.foreignThreadCallbacks = runtime.foreign.load();
        state.publicationMisses = runtime.missed.load();
        state.nativePresentCallbacks = runtime.callbacks.load();
        state.failureReason = runtime.failure.load();
    }
}
