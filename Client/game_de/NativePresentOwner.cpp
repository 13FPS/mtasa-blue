/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Own the actual native present hooks, relay and retained recovery
 *****************************************************************************/

#include "NativePresentOwner.h"
#include "NativeFrameBoundaryProbe.h"
#include "NativeServiceAdmission.h"
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
    namespace PresentOwnerDetail
    {
        namespace Admission = NativeServiceAdmission;
        using PresentFunction = NativePresentOwner::PresentFunction;
        using RouteDescriptor = NativePresentOwner::RouteDescriptor;
        constexpr std::array<unsigned char, 32> executableHash = {0x63, 0xe6, 0x33, 0xbb, 0x51, 0xc3, 0x5f, 0xe3, 0x5c, 0xa9, 0x8f,
                                                                  0x50, 0x3b, 0x62, 0xe7, 0xdd, 0xb8, 0x40, 0x3a, 0xbe, 0x4c, 0x9d,
                                                                  0x50, 0x1c, 0xfa, 0x91, 0x62, 0x89, 0x24, 0x53, 0x84, 0x6f};
        constexpr RouteDescriptor               d3d11Route = {RenderObservationRoute::D3D11,
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
        using CompareThreadObjects = LONG(NTAPI*)(HANDLE, HANDLE);
        enum class Failure : std::uint32_t
        {
            None = 0,
            UnsupportedBuild = 1,
            FingerprintMismatch = 2,
            HookInstall = 3,
            HookRemove = 4,
            WrongProducerThread = 5,
            NestedCallback = 6,
            InvalidViewport = 7,
            TimeBudget = 8,
            UnsupportedRoute = 9,
            GpuOwnershipCleanup = 10
        };
        struct Runtime
        {
            std::atomic<bool>                  apiBusy{false}, bound{false}, hooked{false}, modulePinned{false};
            std::atomic<unsigned>              inFlight{0};
            std::atomic<std::uint64_t>         generation{0}, admissionGeneration{0}, admissionSerial{0};
            DWORD                              producerThread = 0;
            std::uintptr_t                     image = 0;
            PresentFunction                    original = nullptr;
            const RouteDescriptor*             route = nullptr;
            NativePresentOwner::Config         config;
            bool                               queryFrameBoundary = false, nativeDispatch = false;
            NativePresentOwner::Consumer       consumer = nullptr;
            NativePresentOwner::FailureHandler failureHandler = nullptr;
            bool                               detached = false, consumerCleanupComplete = false, releaseFailureAfterSnapshot = false;
            ThreadHandles                      retainedSuspensions;
            bool                               safetyHeldSuspensions = false;
            NextThread                         nextThread = nullptr;
            CompareThreadObjects               compareThreadObjects = nullptr;
            std::uintptr_t                     nextThreadImage = 0;
            RenderSetupDiagnosticV1            setup;
            bool                               setupTracing = false;
        };
        // The actual hook, trampoline and recovery state outlive every consumer
        // object. Failed detach never leaves callable storage in a destructor.
        Runtime               runtime;
        thread_local unsigned callbackDepth = 0;
        void                  Fail(Failure failure) noexcept
        {
            if (runtime.failureHandler)
                runtime.failureHandler(static_cast<std::uint32_t>(failure));
            else
                StopFrameBoundaryAdmission();
        }
        bool ReleaseAdmission(std::uint64_t generation) noexcept
        {
            const auto owner = runtime.admissionGeneration.load();
            if (!owner)
                return true;
            if (!generation || generation != runtime.generation.load() || generation != owner)
                return false;
            if (!Admission::Release({Admission::Category::Render, runtime.admissionSerial.load()}))
                return false;
            runtime.admissionSerial.store(0);
            runtime.admissionGeneration.store(0);
            return true;
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

        bool HookPresent(void* viewport, std::uint32_t syncInterval)
        {
            runtime.inFlight.fetch_add(1);
            ++callbackDepth;
            __try
            {
                const NativePresentOwner::CallbackContext context{runtime.original, runtime.generation.load(), callbackDepth};
                // The consumer is static pinned-module code, registered before
                // attach and retained through detach/drain. It holds no owner
                // object pointer and receives the exact real original target.
                return runtime.consumer ? runtime.consumer(context, viewport, syncInterval) : runtime.original(viewport, syncInterval);
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
        bool NativeThreadComparisonStubMatches(const std::array<unsigned char, 24>& actual) noexcept
        {
            constexpr std::array<unsigned char, 24> expected{0x4c, 0x8b, 0xd1, 0xb8, 0x9f, 0,    0,    0,    0xf6, 0x04, 0x25, 0x08,
                                                             0x03, 0xfe, 0x7f, 0x01, 0x75, 0x03, 0x0f, 0x05, 0xc3, 0xcd, 0x2e, 0xc3};
            return actual == expected;
        }
        bool PrepareNativeThreadEnumeration() noexcept
        {
            // Toolhelp may allocate internally. Cache this exact local x64
            // system entry before holds; stable passes then use only its
            // syscall/int2e stub. Modified or foreign entries fail closed.
            if (!runtime.nextThread || !runtime.compareThreadObjects)
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
                const auto comparison = GetProcAddress(module, "NtCompareObjects");
                if (!comparison)
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
                if (VirtualQuery(reinterpret_cast<void*>(comparison), &page, sizeof(page)) != sizeof(page))
                {
                    const auto error = GetLastError();
                    return SetupRefused(RenderSetupStage::NativeThreadImage, RenderSetupErrorDomain::Win32, error);
                }
                if (page.Type != MEM_IMAGE || page.AllocationBase != module)
                    return SetupRefused(RenderSetupStage::NativeThreadImage);
                runtime.nextThread = reinterpret_cast<NextThread>(entry);
                runtime.compareThreadObjects = reinterpret_cast<CompareThreadObjects>(comparison);
                runtime.nextThreadImage = reinterpret_cast<std::uintptr_t>(module);
            }
            SetupStep(RenderSetupStage::NativeThreadStubRead);
            std::array<unsigned char, 24> actual{};
            if (!Read(reinterpret_cast<std::uintptr_t>(runtime.nextThread), actual.data(), actual.size(), runtime.nextThreadImage, true))
                return SetupRefused(RenderSetupStage::NativeThreadStubRead);
            SetupStep(RenderSetupStage::NativeThreadStubMatch);
            if (!NativeThreadStubMatches(actual))
                return SetupRefused(RenderSetupStage::NativeThreadStubMatch);
            // The documented Win32 comparator converts negative NT statuses.
            // Use only the pinned local syscall body while peers are stopped,
            // avoiding that wrapper's additional error-conversion calls.
            SetupStep(RenderSetupStage::NativeThreadStubRead);
            if (!Read(reinterpret_cast<std::uintptr_t>(runtime.compareThreadObjects), actual.data(), actual.size(), runtime.nextThreadImage, true))
                return SetupRefused(RenderSetupStage::NativeThreadStubRead);
            SetupStep(RenderSetupStage::NativeThreadStubMatch);
            return NativeThreadComparisonStubMatches(actual) || SetupRefused(RenderSetupStage::NativeThreadStubMatch);
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
            struct ExitedObjects
            {
                ThreadHandles handles;
                ~ExitedObjects()
                {
                    for (const auto handle : handles)
                        CloseHandle(handle);
                }
            } exitedObjects;
            if (threads.size() > maximumThreads)
                return SetupRefused(RenderSetupStage::ThreadLimit);
            SetupStep(RenderSetupStage::HoldReserve);
            try
            {
                threads.reserve(maximumThreads);
                held.reserve(maximumThreads);
                exitedObjects.handles.reserve(maximumThreads);
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
                bool     ready = runtime.nextThread && runtime.compareThreadObjects, added = false;
                unsigned enumerated = 0;
                if (!ready)
                    SetupRefused(RenderSetupStage::StableEnumerate);
                while (ready)
                {
                    HANDLE next = nullptr;
                    SetupStep(RenderSetupStage::StableEnumerate);
                    const auto status = runtime.nextThread(
                        GetCurrentProcess(), previous, THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_SET_CONTEXT | THREAD_QUERY_INFORMATION | SYNCHRONIZE,
                        0, 0, &next);
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
                                    // A newly enumerated thread can finish before
                                    // suspension. Error 5 alone also describes a
                                    // live access refusal; discard only this exact
                                    // owned object after a nonblocking exit proof.
                                    const bool exited = error == ERROR_ACCESS_DENIED && WaitForSingleObject(thread, 0) == WAIT_OBJECT_0;
                                    if (exited)
                                    {
                                        // Retained exited objects remain enumerable.
                                        // Keep the iterator valid through its next
                                        // native call, and remember exact kernel
                                        // objects with separate unsuspended handles.
                                        // First sightings reset stability because a
                                        // dead creator could have spawned a live
                                        // child behind this scan; only repeats with
                                        // this fresh exit proof are neutral.
                                        bool seen = false;
                                        for (const auto known : exitedObjects.handles)
                                        {
                                            const auto comparisonStatus = runtime.compareThreadObjects(thread, known);
                                            if (comparisonStatus == 0)
                                            {
                                                seen = true;
                                                break;
                                            }
                                            if (comparisonStatus != static_cast<LONG>(0xc00001acu))
                                            {
                                                ready = SetupRefused(RenderSetupStage::StableSuspend, RenderSetupErrorDomain::NtStatus,
                                                                     static_cast<std::uint32_t>(comparisonStatus));
                                                break;
                                            }
                                        }
                                        if (ready && !seen)
                                        {
                                            if (exitedObjects.handles.size() == maximumThreads)
                                                ready = SetupRefused(RenderSetupStage::ThreadLimit);
                                            else
                                            {
                                                exitedObjects.handles.push_back(thread);
                                                thread = nullptr;
                                                added = true;
                                            }
                                        }
                                        if (thread)
                                            CloseHandle(thread);
                                        continue;
                                    }
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

    }

    namespace NativePresentOwner
    {
        using namespace PresentOwnerDetail;
        Operation::Operation() noexcept : acquired(!runtime.apiBusy.exchange(true))
        {
        }
        Operation::~Operation()
        {
            if (acquired)
                runtime.apiBusy.store(false);
        }
        std::uint64_t CurrentGeneration() noexcept
        {
            return runtime.generation.load();
        }
        bool ModulePinned() noexcept
        {
            return runtime.modulePinned.load();
        }
        State ReadOwnerState() noexcept
        {
            State state;
            state.generation = runtime.generation.load();
            state.bound = runtime.bound.load();
            state.hooked = runtime.hooked.load();
            state.modulePinned = runtime.modulePinned.load();
            state.retained = runtime.safetyHeldSuspensions || !runtime.retainedSuspensions.empty();
            state.pendingHookWork = state.hooked || NativeDispatchPatchPresent() || state.retained;
            state.producerThread = runtime.producerThread;
            state.image = runtime.image;
            state.route = runtime.route;
            return state;
        }
        bool ValidatePages(std::uintptr_t address, std::size_t bytes, std::uintptr_t image, bool executable) noexcept
        {
            return PresentOwnerDetail::Pages(address, bytes, image, executable);
        }
        bool ReadBytes(std::uintptr_t address, void* destination, std::size_t bytes, std::uintptr_t image, bool executable) noexcept
        {
            return Read(address, destination, bytes, image, executable);
        }
        bool CurrentOperation(const Operation& operation, std::uint64_t generation) noexcept
        {
            return operation.acquired && generation && generation == runtime.generation.load();
        }
        bool Reserve(const Operation& operation, const Config& config, Consumer consumer, FailureHandler failure, std::uint64_t& generation) noexcept
        {
            if (!operation.acquired || !consumer || !failure ||
                (config.captureDeviceWindow && (config.captureDevice || !config.nativeDispatch || !config.queryFrameBoundary)) || runtime.hooked.load() ||
                NativeDispatchPatchPresent() || !runtime.retainedSuspensions.empty() || runtime.inFlight.load() || FrameBoundaryInFlight() ||
                runtime.admissionSerial.load() || runtime.generation.load() == (std::numeric_limits<std::uint64_t>::max)())
                return false;
            Admission::Token claim;
            if (!Admission::TryAcquire(Admission::Category::Render, claim))
                return false;
            generation = runtime.generation.fetch_add(1) + 1;
            runtime.admissionSerial.store(claim.serial);
            runtime.admissionGeneration.store(generation);
            runtime.setup = {};
            runtime.setupTracing = false;
            runtime.bound.store(false);
            runtime.producerThread = GetCurrentThreadId();
            runtime.image = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
            runtime.config = config;
            runtime.queryFrameBoundary = config.queryFrameBoundary;
            runtime.nativeDispatch = config.nativeDispatch;
            runtime.route = SelectRoute(config.route);
            runtime.consumer = consumer;
            runtime.failureHandler = failure;
            runtime.detached = false;
            runtime.consumerCleanupComplete = false;
            runtime.releaseFailureAfterSnapshot = false;
            return true;
        }
        bool Install(const Operation& operation, std::uint64_t generation) noexcept
        {
            if (!CurrentOperation(operation, generation) || GetCurrentThreadId() != runtime.producerThread)
                return false;
            const auto& config = runtime.config;
            const auto  route = config.route;
            const bool  queryGpuOwnership = config.queryGpuOwnership, classifyCustomPresent = config.classifyCustomPresent;
            const bool  queryFrameBoundary = config.queryFrameBoundary, nativeDispatch = config.nativeDispatch, captureDevice = config.captureDevice;
#ifndef DETOURS_MTA_VM_TRANSACTION_NODES
            // The direct CALL path cannot fall back to CRT-backed transaction
            // nodes or vendor-owned suspension counts when this build lacks the
            // explicit DE extension. Refuse before any peer is suspended.
            if (nativeDispatch)
            {
                Fail(Failure::UnsupportedRoute);
                runtime.releaseFailureAfterSnapshot = true;
                return false;
            }
#endif
            if (!runtime.route || ((queryGpuOwnership || classifyCustomPresent) && route != RenderObservationRoute::D3D12) ||
                (classifyCustomPresent && !queryGpuOwnership) || (queryFrameBoundary && (!classifyCustomPresent || route != RenderObservationRoute::D3D12)) ||
                (nativeDispatch && !queryFrameBoundary) || (captureDevice && !nativeDispatch))
            {
                Fail(Failure::UnsupportedRoute);
                runtime.releaseFailureAfterSnapshot = true;
                return false;
            }
            if (!ExactExecutable())
            {
                Fail(Failure::UnsupportedBuild);
                runtime.releaseFailureAfterSnapshot = true;
                return false;
            }
            if (!CheckFingerprint(runtime.image, *runtime.route))
            {
                Fail(Failure::FingerprintMismatch);
                runtime.releaseFailureAfterSnapshot = true;
                return false;
            }
            HMODULE pinned = nullptr;
            if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN, reinterpret_cast<LPCWSTR>(&HookPresent), &pinned))
            {
                Fail(Failure::HookInstall);
                runtime.releaseFailureAfterSnapshot = true;
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
                return false;
            }
            runtime.original = reinterpret_cast<PresentFunction>(runtime.image + runtime.route->presentRva);
            bool pending = false;
            if (!ChangeHooks(true, pending))
            {
                Fail(Failure::HookInstall);
                return false;
            }
            runtime.hooked.store(true);
            runtime.bound.store(true);
            return true;
        }
        void FinishFailedInstall(const Operation& operation, std::uint64_t generation) noexcept
        {
            // Preserve the old failure order: publish the diagnostic's final
            // scalar snapshot before dropping its pre-hook Render claim.
            if (CurrentOperation(operation, generation) && runtime.releaseFailureAfterSnapshot)
            {
                ReleaseAdmission(generation);
                runtime.releaseFailureAfterSnapshot = false;
            }
        }
        void StopBoundaryAdmission(const Operation& operation, std::uint64_t generation) noexcept
        {
            if (CurrentOperation(operation, generation))
                StopFrameBoundaryAdmission();
        }
        bool Detach(const Operation& operation, std::uint64_t generation) noexcept
        {
            if (!CurrentOperation(operation, generation))
                return false;
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
                return false;
            }
            bool pending = false;
            if ((runtime.hooked.load() || NativeDispatchPatchPresent() || !runtime.retainedSuspensions.empty()) && !ChangeHooks(false, pending))
            {
                SetupCleanup(false);
                if (!pending)
                    Fail(Failure::HookRemove);
                return false;
            }
            runtime.hooked.store(false);
            if (!ReleaseNativeDispatchRelay())
            {
                SetupCleanup(false);
                return false;
            }
            runtime.detached = true;
            return true;
        }
        void CleanupIncomplete(const Operation& operation, std::uint64_t generation) noexcept
        {
            if (CurrentOperation(operation, generation))
                SetupCleanup(false);
        }
        bool CompleteCleanup(const Operation& operation, std::uint64_t generation) noexcept
        {
            if (!CurrentOperation(operation, generation) || GetCurrentThreadId() != runtime.producerThread || !runtime.detached || runtime.hooked.load() ||
                NativeDispatchPatchPresent() || runtime.inFlight.load() || FrameBoundaryInFlight() || runtime.safetyHeldSuspensions ||
                !runtime.retainedSuspensions.empty())
                return false;
            // Diagnostic COM/device ownership is retired by the consumer first.
            // Until then the manager retains module and Render admission.
            ReleaseFrameBoundaryDiagnosticModules();
            SetupCleanup(true);
            runtime.consumerCleanupComplete = true;
            return true;
        }
        bool Release(const Operation& operation, std::uint64_t generation) noexcept
        {
            return CurrentOperation(operation, generation) && runtime.detached && runtime.consumerCleanupComplete && !runtime.hooked.load() &&
                   !NativeDispatchPatchPresent() && !runtime.inFlight.load() && !FrameBoundaryInFlight() && !runtime.safetyHeldSuspensions &&
                   runtime.retainedSuspensions.empty() && ReleaseAdmission(generation);
        }
        bool ReadSetupDiagnostic(std::uint64_t generation, RenderSetupDiagnosticV1& state) noexcept
        {
            // A different live lease may begin a generation after this lease ends.
            // Serialize the scalar copy with those writers without waiting on a
            // worker that might still own suspended peers.
            Operation operation;
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
}
