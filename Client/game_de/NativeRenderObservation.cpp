/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Observe an engine-owned Present scope without worker COM access
 *****************************************************************************/

#include "NativeRenderObservation.h"
#include "NativeD3D12OwnershipProbe.h"
#include "NativeCustomPresentProbe.h"
#include "NativeFrameBoundaryProbe.h"
#include <array>
#include <algorithm>
#include <atomic>
#include <limits>
#include <new>
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

        template <typename T>
        struct VirtualAllocator
        {
            using value_type = T;
            VirtualAllocator() noexcept = default;
            template <typename U>
            VirtualAllocator(const VirtualAllocator<U>&) noexcept
            {
            }
            T* allocate(std::size_t count)
            {
                if (!count || count > (std::numeric_limits<std::size_t>::max)() / sizeof(T))
                    throw std::bad_alloc();
                auto* result = static_cast<T*>(VirtualAlloc(nullptr, count * sizeof(T), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
                if (!result)
                    throw std::bad_alloc();
                return result;
            }
            void deallocate(T* value, std::size_t) noexcept
            {
                if (value)
                    VirtualFree(value, 0, MEM_RELEASE);
            }
            template <typename U>
            bool operator==(const VirtualAllocator<U>&) const noexcept
            {
                return true;
            }
            template <typename U>
            bool operator!=(const VirtualAllocator<U>&) const noexcept
            {
                return false;
            }
        };
        using ThreadHandles = std::vector<HANDLE, VirtualAllocator<HANDLE>>;
        using NextThread = LONG(NTAPI*)(HANDLE, HANDLE, ACCESS_MASK, ULONG, ULONG, PHANDLE);

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
            bool                         classifyCustomPresent = false;
            bool                         queryFrameBoundary = false;
            bool                         nativeDispatch = false;
            ThreadHandles                retainedSuspensions;
            bool                         safetyHeldSuspensions = false;
            NextThread                   nextThread = nullptr;
            std::uintptr_t               nextThreadImage = 0;
            RenderSetupDiagnosticV1      setup;
            bool                         setupTracing = false;
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
            StopFrameBoundaryAdmission();
            std::uint32_t expected = 0;
            runtime.failure.compare_exchange_strong(expected, static_cast<std::uint32_t>(failure));
        }

        void InitializeSetupDiagnostic(std::uint64_t generation) noexcept
        {
            runtime.setup = {};
            runtime.setup.version = RenderSetupDiagnosticVersionV1;
            runtime.setup.generationLow = static_cast<std::uint32_t>(generation);
            runtime.setup.generationHigh = static_cast<std::uint32_t>(generation >> 32);
            runtime.setup.attempted = 1;
            runtime.setup.stage = static_cast<unsigned>(RenderSetupStage::PrepareFrameBoundary);
            runtime.setupTracing = true;
        }
        void SetupStep(RenderSetupStage stage) noexcept
        {
            if (runtime.setupTracing && !runtime.setup.failureStage)
                runtime.setup.stage = static_cast<unsigned>(stage);
        }
        bool SetupRefused(RenderSetupStage stage, RenderSetupErrorDomain domain = RenderSetupErrorDomain::None, std::uint32_t error = 0) noexcept
        {
            // Only the owning worker writes these scalars. Preserve its first
            // install refusal; rollback/detach must not replace it with a later
            // operation or stale GetLastError value. No callback lock is needed.
            if (runtime.setupTracing && !runtime.setup.failureStage)
            {
                runtime.setup.stage = runtime.setup.failureStage = static_cast<unsigned>(stage);
                runtime.setup.errorDomain = static_cast<unsigned>(domain);
                runtime.setup.errorCode = error;
            }
            return false;
        }
        void SetupRetained() noexcept
        {
            if (!runtime.setup.version)
                return;
            runtime.setup.retainedSuspensions = 0;
            for (const auto thread : runtime.retainedSuspensions)
                if (thread)
                    ++runtime.setup.retainedSuspensions;
            runtime.setup.safetyHeldSuspensions = runtime.safetyHeldSuspensions ? 1u : 0u;
        }
        bool FinishSetupChange(bool ready, bool install) noexcept
        {
            if (runtime.setupTracing)
            {
                if (ready && install)
                {
                    runtime.setup.installSucceeded = 1;
                    SetupStep(RenderSetupStage::Complete);
                }
            }
            SetupRetained();
            runtime.setupTracing = false;
            return ready;
        }
        void SetupCleanup(bool complete) noexcept
        {
            if (!runtime.setup.version)
                return;
            runtime.setup.cleanupAttempted = 1;
            runtime.setup.cleanupComplete = complete ? 1u : 0u;
            SetupRetained();
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

        class ImageCustomPresentReads final : public CustomPresentReads
        {
        public:
            bool Object(std::uintptr_t address, void* output, std::size_t bytes) override { return Read(address, output, bytes); }
            bool Image(std::uintptr_t address, void* output, std::size_t bytes, bool executable) override
            {
                // API5 owns one detour inside a known policy function. Recover
                // its pinned bytes only after verifying the entire exact owned
                // patch; historical API4 still checks the ordinary live image.
                if (runtime.queryFrameBoundary && executable && address == runtime.image + 0xc20780 && bytes == 44)
                    return ReadFrameBoundaryOwnedOriginal(address, output, bytes);
                return Read(address, output, bytes, runtime.image, executable);
            }
        };

        void CaptureOwnership(void* viewport, GpuOwnershipProbeState& state, CustomPresentClassificationState* custom = nullptr) noexcept
        {
            using Stage = GpuOwnershipQueryStage;
            using QueryStatus = GpuOwnershipQueryStatus;
            if (custom)
                *custom = {};
            state.queryStage = static_cast<std::uint32_t>(Stage::NativeRoots);
            state.queryStatus = static_cast<std::uint32_t>(QueryStatus::Failed);
            ProbeRoots roots;
            if (!ReadProbeRoots(viewport, roots))
            {
                state.hresult = static_cast<std::uint32_t>(E_POINTER);
                if (custom)
                    custom->status = static_cast<std::uint32_t>(CustomPresentClassificationStatus::ReadFault);
                return;
            }
            state.customPresentPresent = roots.customPresent ? 1u : 0u;
            state.nativeExplicitGPUCount = roots.explicitGpuCount;
            state.nativeAFRCount = roots.afrCount;
            ImageCustomPresentReads customReads;
            CustomPresentProbeRoots customRoots;
            if (custom)
                ClassifyCustomPresent(customReads, runtime.image, roots.customPresent, roots.device, *custom, customRoots);
            bool       queried = false;
            const auto query = [&]() noexcept
            {
                // Indexed multi-GPU routes may choose another native queue. Keep
                // their historical refusal stage rather than query device zero.
                const bool singleNativeGpu = roots.explicitGpuCount == 1 && roots.afrCount == 1;
                state.queryStatus = static_cast<std::uint32_t>(QueryStatus::Unsupported);
                state.queryStage = static_cast<std::uint32_t>(Stage::CustomPresent);
                if (roots.customPresent && !singleNativeGpu)
                    return;
                state.queryStage = static_cast<std::uint32_t>(Stage::SingleNode);
                if (!singleNativeGpu)
                    return;
                if (!BorrowedComRoot(roots.swapchain) || !BorrowedComRoot(roots.queue))
                {
                    state.queryStatus = static_cast<std::uint32_t>(QueryStatus::Failed);
                    state.hresult = static_cast<std::uint32_t>(E_POINTER);
                    return;
                }
                // These are the existing native swapchain and creation queue even
                // when a custom presenter exists; their metadata does not identify
                // its chosen frame sink. The query's support predicate still
                // excludes that route. Release every local ref before the original
                // can present or rotate buffers; issue no GPU work or waits.
                queried = true;
                ProbeD3D12Ownership(reinterpret_cast<void*>(roots.swapchain), reinterpret_cast<void*>(roots.queue), state);
            };
            query();
            // API4's class policy belongs to the same borrowed native roots as
            // its COM metadata, including refused/failed COM attempts. API3
            // retains its historical readback only after an actual COM query.
            if (!queried && !custom)
                return;
            ProbeRoots after;
            const bool changed = !ReadProbeRoots(viewport, after) || !SameProbeRoots(roots, after);
            if (changed && state.queryStatus != static_cast<std::uint32_t>(QueryStatus::CleanupFailed))
            {
                state.queryStage = static_cast<std::uint32_t>(Stage::RootReadback);
                state.queryStatus = static_cast<std::uint32_t>(QueryStatus::RootsChanged);
                state.metadataValid = state.supportPredicate = 0;
                state.hresult = 0;
            }
            if (custom && (changed || !CustomPresentRootsUnchanged(customReads, customRoots)))
            {
                *custom = {};
                custom->status = static_cast<std::uint32_t>(CustomPresentClassificationStatus::RootsChanged);
            }
        }

        bool InvokeOriginal(void* viewport, std::uint32_t syncInterval, FrameBoundaryScope& scope, bool observe)
        {
            __try
            {
                return runtime.original(viewport, syncInterval);
            }
            __finally
            {
                if (observe)
                    LeaveFrameBoundaryScope(scope);
            }
        }

        void BoundaryCleanupFailure() noexcept
        {
            // The original Present may raise before publication. Preserve the
            // fatal release fault now rather than relying on its return path.
            Fail(Failure::GpuOwnershipCleanup);
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
                CaptureOwnership(viewport, capture.gpu, runtime.classifyCustomPresent ? &capture.custom : nullptr);
                fatalCleanup = capture.gpu.queryStatus == static_cast<std::uint32_t>(GpuOwnershipQueryStatus::CleanupFailed);
                // Disarm before either the original or a contended publication
                // can throw/drop this metadata. Unknown Release effects must
                // never be repeated or hidden by a missed snapshot.
                if (fatalCleanup)
                    Fail(Failure::GpuOwnershipCleanup);
            }
            FrameBoundaryScope boundary;
            const bool         observeBoundary = accepted && runtime.queryFrameBoundary && !fatalCleanup && runtime.active.load();
            if (observeBoundary)
                EnterFrameBoundaryScope(boundary, runtime.image, viewport, runtime.callbackThread.load(), capture.custom, &BoundaryCleanupFailure);
            const bool result = InvokeOriginal(viewport, syncInterval, boundary, observeBoundary);
            if (observeBoundary)
            {
                capture.boundary = boundary.state;
                capture.nativeDispatch = boundary.nativeDispatch;
                if (boundary.cleanupFailed)
                {
                    fatalCleanup = true;
                    Fail(Failure::GpuOwnershipCleanup);
                }
            }
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
                    if (runtime.queryFrameBoundary)
                    {
                        if (!observeBoundary && fatalCleanup)
                            capture.boundary = runtime.snapshot.boundary;
                        else
                        {
                            capture.boundary.presentEntries += runtime.snapshot.boundary.presentEntries;
                            capture.boundary.rejectedEntries += runtime.snapshot.boundary.rejectedEntries;
                        }
                    }
                    if (runtime.nativeDispatch)
                    {
                        if (!observeBoundary && fatalCleanup)
                            capture.nativeDispatch = runtime.snapshot.nativeDispatch;
                        else
                        {
                            capture.nativeDispatch.presentEntries += runtime.snapshot.nativeDispatch.presentEntries;
                            capture.nativeDispatch.rejectedEntries += runtime.snapshot.nativeDispatch.rejectedEntries;
                        }
                    }
                    runtime.snapshot = capture;
                    if (observeBoundary)
                        PublishFrameBoundaryEndpointDiagnostic(boundary, runtime.accepted.load() + 1);
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

        bool CollectThreads(ThreadHandles& threads) noexcept
        {
            SetupStep(RenderSetupStage::CollectReserve);
            try
            {
                threads.reserve(1024);
            }
            catch (...)
            {
                return SetupRefused(RenderSetupStage::CollectReserve);
            }
            SetupStep(RenderSetupStage::CollectSnapshot);
            const auto snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
            if (snapshot == INVALID_HANDLE_VALUE)
            {
                const auto error = GetLastError();
                return SetupRefused(RenderSetupStage::CollectSnapshot, RenderSetupErrorDomain::Win32, error);
            }
            THREADENTRY32 entry{};
            entry.dwSize = sizeof(entry);
            SetupStep(RenderSetupStage::CollectFirst);
            bool  enumerated = Thread32First(snapshot, &entry) != FALSE, ready = enumerated;
            DWORD enumerationError = enumerated ? ERROR_SUCCESS : GetLastError();
            if (!enumerated)
                SetupRefused(RenderSetupStage::CollectFirst, RenderSetupErrorDomain::Win32, enumerationError);
            try
            {
                while (enumerated)
                {
                    if (entry.th32OwnerProcessID == GetCurrentProcessId() && entry.th32ThreadID != GetCurrentThreadId())
                    {
                        if (runtime.nativeDispatch && threads.size() == 1024)
                        {
                            ready = SetupRefused(RenderSetupStage::ThreadLimit);
                            break;
                        }
                        SetupStep(RenderSetupStage::CollectOpen);
                        const auto thread =
                            OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_SET_CONTEXT | THREAD_QUERY_INFORMATION, FALSE, entry.th32ThreadID);
                        if (!thread)
                        {
                            const auto error = GetLastError();
                            ready = SetupRefused(RenderSetupStage::CollectOpen, RenderSetupErrorDomain::Win32, error);
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
                        if (runtime.setupTracing)
                            runtime.setup.initialThreads = static_cast<std::uint32_t>(threads.size());
                    }
                    SetupStep(RenderSetupStage::CollectNext);
                    enumerated = Thread32Next(snapshot, &entry) != FALSE;
                    enumerationError = enumerated ? ERROR_SUCCESS : GetLastError();
                }
                if (!enumerated && enumerationError != ERROR_NO_MORE_FILES)
                    ready = SetupRefused(RenderSetupStage::CollectNext, RenderSetupErrorDomain::Win32, enumerationError);
            }
            catch (...)
            {
                ready = SetupRefused(RenderSetupStage::CollectReserve);
            }
            CloseHandle(snapshot);
            return ready;
        }
        bool NativeThreadStubMatches(const std::array<unsigned char, 24>& actual) noexcept
        {
            constexpr std::array<unsigned char, 24> expected{0x4c, 0x8b, 0xd1, 0xb8, 0x01, 0x01, 0,    0,    0xf6, 0x04, 0x25, 0x08,
                                                             0x03, 0xfe, 0x7f, 0x01, 0x75, 0x03, 0x0f, 0x05, 0xc3, 0xcd, 0x2e, 0xc3};
            return actual == expected;
        }
        bool PrepareNativeThreadEnumeration() noexcept
        {
            // Toolhelp may allocate internally. Cache this exact local x64
            // system entry before holds; stable passes then use only its
            // syscall/int2e stub. Modified or foreign entries fail closed.
            if (!runtime.nextThread)
            {
                SetupStep(RenderSetupStage::NativeThreadModule);
                if (!runtime.retainedSuspensions.empty())
                    return SetupRefused(RenderSetupStage::NativeThreadModule);
                const auto module = GetModuleHandleW(L"ntdll.dll");
                if (!module)
                {
                    const auto error = GetLastError();
                    return SetupRefused(RenderSetupStage::NativeThreadModule, RenderSetupErrorDomain::Win32, error);
                }
                SetupStep(RenderSetupStage::NativeThreadExport);
                const auto entry = GetProcAddress(module, "NtGetNextThread");
                if (!entry)
                {
                    const auto error = GetLastError();
                    return SetupRefused(RenderSetupStage::NativeThreadExport, RenderSetupErrorDomain::Win32, error);
                }
                SetupStep(RenderSetupStage::NativeThreadImage);
                MEMORY_BASIC_INFORMATION page{};
                if (VirtualQuery(reinterpret_cast<void*>(entry), &page, sizeof(page)) != sizeof(page))
                {
                    const auto error = GetLastError();
                    return SetupRefused(RenderSetupStage::NativeThreadImage, RenderSetupErrorDomain::Win32, error);
                }
                if (page.Type != MEM_IMAGE || page.AllocationBase != module)
                    return SetupRefused(RenderSetupStage::NativeThreadImage);
                runtime.nextThread = reinterpret_cast<NextThread>(entry);
                runtime.nextThreadImage = reinterpret_cast<std::uintptr_t>(module);
            }
            SetupStep(RenderSetupStage::NativeThreadStubRead);
            std::array<unsigned char, 24> actual{};
            if (!Read(reinterpret_cast<std::uintptr_t>(runtime.nextThread), actual.data(), actual.size(), runtime.nextThreadImage, true))
                return SetupRefused(RenderSetupStage::NativeThreadStubRead);
            SetupStep(RenderSetupStage::NativeThreadStubMatch);
            return NativeThreadStubMatches(actual) || SetupRefused(RenderSetupStage::NativeThreadStubMatch);
        }
        bool ResumeOwnedThread(HANDLE thread, DWORD(WINAPI* resume)(HANDLE) = &ResumeThread, DWORD* resumeError = nullptr) noexcept
        {
            const auto result = resume(thread);
            const auto error = result == static_cast<DWORD>(-1) ? GetLastError() : ERROR_SUCCESS;
            if (resumeError)
                *resumeError = error;
            DWORD exitCode = STILL_ACTIVE;
            return result != static_cast<DWORD>(-1) || (GetExitCodeThread(thread, &exitCode) && exitCode != STILL_ACTIVE);
        }
        bool HoldNativeDispatchThreads(ThreadHandles& threads, ThreadHandles& held) noexcept
        {
            // VM-backed buffers are reserved before suspension. Newly found
            // children never grow a vector or touch a stopped CRT heap.
            constexpr std::size_t maximumThreads = 1024;
            if (threads.size() > maximumThreads)
                return SetupRefused(RenderSetupStage::ThreadLimit);
            SetupStep(RenderSetupStage::HoldReserve);
            try
            {
                threads.reserve(maximumThreads);
                held.reserve(maximumThreads);
                held.resize(threads.size(), nullptr);
            }
            catch (...)
            {
                return SetupRefused(RenderSetupStage::HoldReserve);
            }
            for (std::size_t index = 0; index < threads.size(); ++index)
            {
                SetupStep(RenderSetupStage::HoldSuspend);
                if (SuspendThread(threads[index]) == static_cast<DWORD>(-1))
                {
                    const auto error = GetLastError();
                    return SetupRefused(RenderSetupStage::HoldSuspend, RenderSetupErrorDomain::Win32, error);
                }
                held[index] = threads[index];
                if (runtime.setupTracing)
                    ++runtime.setup.heldThreads;
            }
            unsigned stablePasses = 0;
            for (unsigned pass = 0; pass < 4; ++pass)
            {
                if (runtime.setupTracing)
                    runtime.setup.stablePasses = pass + 1;
                HANDLE   previous = nullptr;
                bool     ready = runtime.nextThread != nullptr, added = false;
                unsigned enumerated = 0;
                if (!ready)
                    SetupRefused(RenderSetupStage::StableEnumerate);
                while (ready)
                {
                    HANDLE next = nullptr;
                    SetupStep(RenderSetupStage::StableEnumerate);
                    const auto status = runtime.nextThread(
                        GetCurrentProcess(), previous, THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_SET_CONTEXT | THREAD_QUERY_INFORMATION, 0, 0, &next);
                    if (previous)
                        CloseHandle(previous);
                    previous = next;
                    if (status == static_cast<LONG>(0x8000001au))
                        break;
                    if (status != 0)
                    {
                        ready = SetupRefused(RenderSetupStage::StableEnumerate, RenderSetupErrorDomain::NtStatus, static_cast<std::uint32_t>(status));
                        break;
                    }
                    if (!next)
                    {
                        ready = SetupRefused(RenderSetupStage::StableEnumerate);
                        break;
                    }
                    if (++enumerated > maximumThreads + 1)
                    {
                        ready = SetupRefused(RenderSetupStage::StableLimit);
                        break;
                    }
                    SetupStep(RenderSetupStage::StableThreadId);
                    const auto id = GetThreadId(next);
                    if (!id)
                    {
                        const auto error = GetLastError();
                        ready = SetupRefused(RenderSetupStage::StableThreadId, RenderSetupErrorDomain::Win32, error);
                        break;
                    }
                    if (id != GetCurrentThreadId() && std::none_of(threads.begin(), threads.end(), [&](HANDLE thread) { return GetThreadId(thread) == id; }))
                    {
                        if (threads.size() == maximumThreads)
                            ready = SetupRefused(RenderSetupStage::ThreadLimit);
                        else
                        {
                            HANDLE thread = nullptr;
                            SetupStep(RenderSetupStage::StableDuplicate);
                            if (!DuplicateHandle(GetCurrentProcess(), next, GetCurrentProcess(), &thread, 0, FALSE, DUPLICATE_SAME_ACCESS))
                            {
                                const auto error = GetLastError();
                                if (thread)
                                    CloseHandle(thread);
                                ready = SetupRefused(RenderSetupStage::StableDuplicate, RenderSetupErrorDomain::Win32, error);
                            }
                            else
                            {
                                SetupStep(RenderSetupStage::StableSuspend);
                                if (SuspendThread(thread) == static_cast<DWORD>(-1))
                                {
                                    const auto error = GetLastError();
                                    CloseHandle(thread);
                                    ready = SetupRefused(RenderSetupStage::StableSuspend, RenderSetupErrorDomain::Win32, error);
                                }
                                else
                                {
                                    threads.push_back(thread);
                                    held.push_back(thread);
                                    if (runtime.setupTracing)
                                        ++runtime.setup.heldThreads;
                                    CONTEXT context{};
                                    context.ContextFlags = CONTEXT_CONTROL;
                                    SetupStep(RenderSetupStage::StableContext);
                                    if (!GetThreadContext(thread, &context))
                                    {
                                        const auto error = GetLastError();
                                        ready = SetupRefused(RenderSetupStage::StableContext, RenderSetupErrorDomain::Win32, error);
                                    }
                                    else
                                    {
                                        SetupStep(RenderSetupStage::StableInstruction);
                                        ready = NativeDispatchInstructionSafe(static_cast<std::uintptr_t>(context.Rip),
                                                                              runtime.hooked.load() ? reinterpret_cast<std::uintptr_t>(runtime.original) : 0) ||
                                                SetupRefused(RenderSetupStage::StableInstruction);
                                    }
                                    added = true;
                                }
                            }
                        }
                    }
                }
                if (previous)
                    CloseHandle(previous);
                if (!ready)
                    return false;
                stablePasses = added ? 0u : stablePasses + 1;
                if (stablePasses == 2)
                    return true;
            }
            return SetupRefused(RenderSetupStage::StableLimit);
        }
        bool ChangeHooks(bool install, bool& pending) noexcept
        {
            runtime.setupTracing = install && runtime.nativeDispatch && runtime.setup.version == RenderSetupDiagnosticVersionV1;
            pending = false;
            if (runtime.safetyHeldSuspensions)
            {
                SetupStep(RenderSetupStage::SafeToResume);
                if (!NativeDispatchSafeToResume(runtime.hooked.load()))
                {
                    SetupRefused(RenderSetupStage::SafeToResume);
                    pending = true;
                    return FinishSetupChange(false, install);
                }
                runtime.safetyHeldSuspensions = false;
            }
            for (auto& retained : runtime.retainedSuspensions)
                if (retained)
                {
                    SetupStep(RenderSetupStage::Resume);
                    DWORD error = 0;
                    if (ResumeOwnedThread(retained, &ResumeThread, &error))
                    {
                        CloseHandle(retained);
                        retained = nullptr;
                    }
                    else
                    {
                        SetupRefused(RenderSetupStage::Resume, RenderSetupErrorDomain::Win32, error);
                        pending = true;
                        return FinishSetupChange(false, install);
                    }
                }
            runtime.retainedSuspensions.clear();
            ThreadHandles threads;
            ThreadHandles ownedSuspended;
            bool          ready = (!runtime.nativeDispatch || PrepareNativeThreadEnumeration()) && CollectThreads(threads);
            if (ready && runtime.nativeDispatch)
                ready = HoldNativeDispatchThreads(threads, ownedSuspended);
            bool       transaction = false;
            bool       callsiteChanged = false;
            bool       committedChange = false;
            const bool entryBefore = runtime.hooked.load();
            if (ready)
            {
                SetupStep(RenderSetupStage::TransactionBegin);
                const auto beginError = DetourTransactionBegin();
                ready = beginError == NO_ERROR ||
                        SetupRefused(RenderSetupStage::TransactionBegin, RenderSetupErrorDomain::Detours, static_cast<std::uint32_t>(beginError));
                if (ready)
                {
                    transaction = true;
                    SetupStep(RenderSetupStage::TransactionCurrent);
                    const auto currentError = DetourUpdateThread(GetCurrentThread());
                    ready = currentError == NO_ERROR ||
                            SetupRefused(RenderSetupStage::TransactionCurrent, RenderSetupErrorDomain::Detours, static_cast<std::uint32_t>(currentError));
                    for (const auto thread : threads)
                        if (ready)
                        {
                            SetupStep(RenderSetupStage::TransactionPeers);
#ifdef DETOURS_MTA_VM_TRANSACTION_NODES
                            const auto peerError = (runtime.nativeDispatch ? DetourUpdateThreadExternallySuspended(thread) : DetourUpdateThread(thread));
                            ready = peerError == NO_ERROR ||
                                    SetupRefused(RenderSetupStage::TransactionPeers, RenderSetupErrorDomain::Detours, static_cast<std::uint32_t>(peerError));
#else
                            if (runtime.nativeDispatch)
                                ready = SetupRefused(RenderSetupStage::TransactionPeers);
                            else
                            {
                                const auto peerError = DetourUpdateThread(thread);
                                ready = peerError == NO_ERROR || SetupRefused(RenderSetupStage::TransactionPeers, RenderSetupErrorDomain::Detours,
                                                                              static_cast<std::uint32_t>(peerError));
                            }
#endif
                        }
                    if (ready && runtime.nativeDispatch)
                        for (const auto thread : threads)
                        {
                            CONTEXT context{};
                            context.ContextFlags = CONTEXT_CONTROL;
                            SetupStep(RenderSetupStage::FinalContext);
                            if (!GetThreadContext(thread, &context))
                            {
                                const auto error = GetLastError();
                                ready = SetupRefused(RenderSetupStage::FinalContext, RenderSetupErrorDomain::Win32, error);
                                pending = !install;
                                break;
                            }
                            SetupStep(RenderSetupStage::FinalInstruction);
                            if (!NativeDispatchInstructionSafe(static_cast<std::uintptr_t>(context.Rip),
                                                               runtime.hooked.load() ? reinterpret_cast<std::uintptr_t>(runtime.original) : 0))
                            {
                                ready = SetupRefused(RenderSetupStage::FinalInstruction);
                                pending = !install;
                                break;
                            }
                        }
                    // No callback-shared lock is held while Detours suspends threads.
                    // A callback can enter after End's optimistic check; abort and
                    // resume it rather than freeing its live trampoline.
                    if (ready && !install && (runtime.inFlight.load() || (runtime.queryFrameBoundary && FrameBoundaryInFlight())))
                    {
                        SetupRefused(RenderSetupStage::InFlight);
                        pending = true;
                        ready = false;
                    }
                    if (ready && runtime.nativeDispatch)
                    {
                        SetupStep(RenderSetupStage::ProtectionBaseline);
                        ready = CaptureNativeDispatchProtectionBaseline() || SetupRefused(RenderSetupStage::ProtectionBaseline);
                    }
                    if (ready && (install || entryBefore))
                    {
                        SetupStep(RenderSetupStage::WrapperChange);
                        const auto wrapperError =
                            (install ? DetourAttach : DetourDetach)(reinterpret_cast<PVOID*>(&runtime.original), reinterpret_cast<PVOID>(&HookPresent));
                        ready = wrapperError == NO_ERROR ||
                                SetupRefused(RenderSetupStage::WrapperChange, RenderSetupErrorDomain::Detours, static_cast<std::uint32_t>(wrapperError));
                    }
                    if (ready && runtime.queryFrameBoundary && (install || NativeDispatchDetoursPresent()))
                    {
                        SetupStep(RenderSetupStage::LifecycleOwnership);
                        ready = install || FrameBoundaryHooksOwned() || SetupRefused(RenderSetupStage::LifecycleOwnership);
                    }
                    if (ready && runtime.queryFrameBoundary && (install || NativeDispatchDetoursPresent()))
                    {
                        SetupStep(RenderSetupStage::LifecycleChange);
                        ready = ChangeFrameBoundaryHooks(install) || SetupRefused(RenderSetupStage::LifecycleChange);
                    }
                    if (ready && runtime.nativeDispatch)
                    {
                        SetupStep(RenderSetupStage::CallsiteChange);
                        const bool before = NativeDispatchPatchPresent();
                        ready = ChangeNativeDispatchCallsite(install, entryBefore) || SetupRefused(RenderSetupStage::CallsiteChange);
                        callsiteChanged = NativeDispatchPatchPresent() != before;
                    }
                    if (ready)
                    {
                        SetupStep(RenderSetupStage::TransactionCommit);
                        const auto commitError = DetourTransactionCommit();
                        ready = commitError == NO_ERROR ||
                                SetupRefused(RenderSetupStage::TransactionCommit, RenderSetupErrorDomain::Detours, static_cast<std::uint32_t>(commitError));
                        transaction = false;
                        committedChange = ready;
                        if (ready && runtime.setupTracing)
                            runtime.setup.transactionCommitted = 1;
                        if (ready && runtime.queryFrameBoundary)
                        {
                            // A committed hook remains owned even if post-attach
                            // verification refuses observation; End must remove it.
                            runtime.hooked.store(install);
                            SetupStep(RenderSetupStage::HooksCommitted);
                            ready = FrameBoundaryHooksCommitted(install) || SetupRefused(RenderSetupStage::HooksCommitted);
                            if (ready && runtime.setupTracing)
                                runtime.setup.hooksCommitted = 1;
                        }
                    }
                }
            }
            // Detours knows only its entry patches. Our additional suspend
            // counts keep every enlisted thread stopped after Commit/Abort so
            // a failed transaction can roll back the CALL before any resume.
            if (!ready && !committedChange && runtime.nativeDispatch && callsiteChanged)
            {
                SetupStep(RenderSetupStage::RollbackCallsite);
                if (!ChangeNativeDispatchCallsite(!install, entryBefore))
                {
                    SetupRefused(RenderSetupStage::RollbackCallsite);
                    pending = true;
                }
            }
            if (transaction)
            {
                SetupStep(RenderSetupStage::TransactionAbort);
                const auto abortError = DetourTransactionAbort();
                if (abortError != NO_ERROR)
                    SetupRefused(RenderSetupStage::TransactionAbort, RenderSetupErrorDomain::Detours, static_cast<std::uint32_t>(abortError));
            }
            SetupStep(RenderSetupStage::SafeToResume);
            const bool safeToResume = !runtime.nativeDispatch || NativeDispatchSafeToResume(runtime.hooked.load());
            if (!safeToResume)
            {
                SetupRefused(RenderSetupStage::SafeToResume);
                pending = true;
                ready = false;
                runtime.safetyHeldSuspensions = true;
            }
            if (ready && !install && runtime.nativeDispatch)
            {
                SetupStep(RenderSetupStage::RelayRelease);
                if (!ReleaseNativeDispatchRelay())
                {
                    SetupRefused(RenderSetupStage::RelayRelease);
                    ready = false;
                    pending = true;
                }
            }
            for (auto& thread : ownedSuspended)
                if (thread && safeToResume)
                {
                    SetupStep(RenderSetupStage::Resume);
                    DWORD error = 0;
                    if (ResumeOwnedThread(thread, &ResumeThread, &error))
                        thread = nullptr;
                    else
                    {
                        SetupRefused(RenderSetupStage::Resume, RenderSetupErrorDomain::Win32, error);
                        ready = false;
                        pending = true;
                    }
                }
            runtime.retainedSuspensions.swap(ownedSuspended);
            for (const auto thread : threads)
                if (std::find(runtime.retainedSuspensions.begin(), runtime.retainedSuspensions.end(), thread) == runtime.retainedSuspensions.end())
                    CloseHandle(thread);
            if (std::all_of(runtime.retainedSuspensions.begin(), runtime.retainedSuspensions.end(), [](HANDLE thread) { return thread == nullptr; }))
                runtime.retainedSuspensions.clear();
            return FinishSetupChange(ready, install);
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

    bool NativeRenderObservation::Begin(RenderObservationRoute route, bool queryGpuOwnership, bool classifyCustomPresent, bool queryFrameBoundary,
                                        bool nativeDispatch) noexcept
    {
        ApiOperation operation;
        if (!operation.acquired || runtime.hooked.load() || NativeDispatchPatchPresent() || !runtime.retainedSuspensions.empty() || runtime.inFlight.load() ||
            FrameBoundaryInFlight() || runtime.active.load())
            return false;
        generation = runtime.generation.fetch_add(1) + 1;
        finalState = {};
        runtime.setup = {};
        runtime.setupTracing = false;
        runtime.bound.store(false);
        runtime.producerThread = GetCurrentThreadId();
        runtime.image = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
        ResetCounters();
        runtime.requestedRoute = static_cast<std::uint32_t>(route);
        runtime.queryGpuOwnership = queryGpuOwnership;
        runtime.classifyCustomPresent = classifyCustomPresent;
        runtime.queryFrameBoundary = queryFrameBoundary;
        runtime.nativeDispatch = nativeDispatch;
        ResetFrameBoundaryObservation();
        runtime.route = SelectRoute(route);
#ifndef DETOURS_MTA_VM_TRANSACTION_NODES
        // The direct CALL path cannot fall back to CRT-backed transaction
        // nodes or vendor-owned suspension counts when this build lacks the
        // explicit DE extension. Refuse before any peer is suspended.
        if (nativeDispatch)
        {
            Fail(Failure::UnsupportedRoute);
            ReadState(finalState);
            return false;
        }
#endif
        if (!runtime.route || ((queryGpuOwnership || classifyCustomPresent) && route != RenderObservationRoute::D3D12) ||
            (classifyCustomPresent && !queryGpuOwnership) || (queryFrameBoundary && (!classifyCustomPresent || route != RenderObservationRoute::D3D12)) ||
            (nativeDispatch && !queryFrameBoundary))
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
        if (nativeDispatch)
            InitializeSetupDiagnostic(generation);
        if (queryFrameBoundary && !PrepareFrameBoundaryHooks(runtime.image, generation, nativeDispatch, reinterpret_cast<void*>(&HookPresent)))
        {
            SetupRefused(RenderSetupStage::PrepareFrameBoundary);
            runtime.setupTracing = false;
            Fail(Failure::FingerprintMismatch);
            ReadState(finalState);
            return false;
        }
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
            return !runtime.hooked.load() && !NativeDispatchPatchPresent() && runtime.retainedSuspensions.empty();
        runtime.active.store(false);
        StopFrameBoundaryAdmission();
        if (GetCurrentThreadId() != runtime.producerThread)
        {
            Fail(Failure::WrongProducerThread);
            return false;
        }
        SetupCleanup(false);
        // A counted callback may be the thread whose resume failed. Cleanup
        // must first retry its safe resume rather than requiring it to finish
        // while it is still stopped; ChangeHooks rechecks counts under holds.
        if ((runtime.inFlight.load() || (runtime.queryFrameBoundary && FrameBoundaryInFlight())) && runtime.retainedSuspensions.empty() &&
            !runtime.safetyHeldSuspensions)
        {
            ReadState(finalState);
            return false;
        }
        bool pending = false;
        if ((runtime.hooked.load() || NativeDispatchPatchPresent() || !runtime.retainedSuspensions.empty()) && !ChangeHooks(false, pending))
        {
            SetupCleanup(false);
            if (!pending)
                Fail(Failure::HookRemove);
            ReadState(finalState);
            return false;
        }
        runtime.hooked.store(false);
        if (!ReleaseNativeDispatchRelay())
        {
            SetupCleanup(false);
            ReadState(finalState);
            return false;
        }
        ReleaseFrameBoundaryDiagnosticModules();
        SetupCleanup(true);
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
        // A failed resume may retain a callback while it owns this lock. The
        // worker must remain able to retry cleanup; keep its last coherent
        // value cache if a nonblocking read cannot obtain a new publication.
        const bool retained = runtime.safetyHeldSuspensions || !runtime.retainedSuspensions.empty();
        const bool locked = retained ? TryAcquireSRWLockShared(&runtime.snapshotLock) != FALSE : (AcquireSRWLockShared(&runtime.snapshotLock), true);
        if (locked)
        {
            state = runtime.snapshot;
            // Count and snapshot are one publication; never attach a newer
            // atomic accepted count to the cached state on a lock miss.
            state.acceptedSnapshots = runtime.accepted.load();
            ReleaseSRWLockShared(&runtime.snapshotLock);
        }
        state.generation = generation;
        state.requestedRoute = runtime.requestedRoute;
        state.modulePinned = runtime.modulePinned.load();
        state.bound = runtime.bound.load();
        state.active = runtime.active.load();
        state.pendingCleanup = (runtime.hooked.load() || NativeDispatchPatchPresent() || !runtime.retainedSuspensions.empty()) && !state.active;
        // Completed effects precede their monotonically increasing prerequisite.
        state.rejectedSnapshots = runtime.rejected.load();
        state.nestedCallbacks = runtime.nested.load();
        state.foreignThreadCallbacks = runtime.foreign.load();
        state.publicationMisses = runtime.missed.load();
        state.nativePresentCallbacks = runtime.callbacks.load();
        state.failureReason = runtime.failure.load();
        finalState = state;
    }
    bool NativeRenderObservation::ReadEndpointDiagnostic(RenderEndpointDiagnosticV1& state) const noexcept
    {
        if (!generation || generation != runtime.generation.load() || GetCurrentThreadId() != runtime.producerThread || !runtime.queryFrameBoundary ||
            runtime.safetyHeldSuspensions || !runtime.retainedSuspensions.empty())
            return false;
        // Use the same lock order as publication so a separate diagnostic read
        // cannot observe next-count metadata before the native snapshot commits.
        AcquireSRWLockShared(&runtime.snapshotLock);
        const bool result = ReadFrameBoundaryEndpointDiagnostic(generation, state);
        ReleaseSRWLockShared(&runtime.snapshotLock);
        return result;
    }
    bool NativeRenderObservation::ReadSetupDiagnostic(RenderSetupDiagnosticV1& state) const noexcept
    {
        // A different live lease may begin a generation after this lease ends.
        // Serialize the scalar copy with those writers without waiting on a
        // worker that might still own suspended peers.
        ApiOperation operation;
        if (!operation.acquired)
            return false;
        if (!generation || generation != runtime.generation.load() || GetCurrentThreadId() != runtime.producerThread || !runtime.nativeDispatch ||
            runtime.safetyHeldSuspensions || !runtime.retainedSuspensions.empty() || runtime.setup.version != RenderSetupDiagnosticVersionV1 ||
            runtime.setup.generationLow != static_cast<std::uint32_t>(generation) ||
            runtime.setup.generationHigh != static_cast<std::uint32_t>(generation >> 32))
            return false;
        // The operation guard protects this worker-owned cache across leases.
        // It needs no callback-shared lock and remains usable after End.
        state = runtime.setup;
        return true;
    }
}
