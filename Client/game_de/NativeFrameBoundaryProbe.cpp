/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Fail-closed read-only native frame association diagnostics
 *****************************************************************************/
#include "NativeFrameBoundaryProbe.h"
#include "NativeCustomPresentProbe.h"
#include <windows.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <bcrypt.h>
#include <detours.h>
#include <intrin.h>
#include <array>
#include <atomic>
#include <cstring>
#include <limits>

namespace mta::game_de
{
    namespace boundary_detail
    {
        using BoundaryStatus = FrameBoundaryObservationStatus;
        using Stage = FrameBoundaryQueryStage;
        using Present = HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain*, UINT, UINT);
        using Resize = void (*)(void*, UINT, UINT, bool);
        using Notify = void (*)(void*);
        constexpr std::size_t maximumCode = 1216;
        struct Function
        {
            std::uintptr_t                         rva;
            std::size_t                            bytes;
            const char*                            digest;
            std::array<unsigned char, maximumCode> original{}, installed{};
            std::size_t                            stolen = 0;
            std::uintptr_t                         detour = 0;
            bool                                   captured = false;
        };
        struct Runtime
        {
            std::uintptr_t                image = 0, dxgi = 0;
            Present                       present = nullptr;
            Resize                        resize = nullptr;
            Notify                        notify = nullptr, destroy = nullptr;
            Function                      resizeCode{0x1fd58e0, 1216, "c3aa255197bb099e36c0c92a0847839212635182ffee90b82c602a18dec269f2"};
            Function                      notifyCode{0xc20780, 44, "f3b4a18798b293c12b9184eaefa26a2b1aaff29543c79524615fcf6fb4275540"};
            Function                      destroyCode{0x1fd5480, 546, "ceb9c5ed9902ae95e2eeb1b758c8151dd906b3ca514ffb92bc5a5bbc0bf9acf7"};
            Function                      presentCode{0x19000, 1167, "927480275a7ec7ff5fc5b7519b90cbe1565e6a1dde1ad502cea102f8dbc67104"};
            bool                          prepared = false, endpoint = false, committed = false, keyReady = false;
            std::atomic<bool>             armed{false}, destroyed{false};
            std::atomic<unsigned>         inFlight{0}, resizing{0};
            std::atomic<std::uint64_t>    epoch{0}, resizeEntries{0}, notifyEntries{0}, foreignEntries{0};
            std::atomic<DWORD>            resizeThread{0}, notifyThread{0};
            SRWLOCK                       identityLock = SRWLOCK_INIT;
            SRWLOCK                       lifecycleLock = SRWLOCK_INIT;
            std::array<unsigned char, 32> key{}, identity{}, addressIdentity{};
            bool                          identitySet = false;
            struct CachedModule
            {
                std::uintptr_t           base = 0;
                RenderEndpointOwnerClass owner = RenderEndpointOwnerClass::Unknown;
                HMODULE                  retained = nullptr;
            };
            std::array<CachedModule, 7> modules{};
            SRWLOCK                     diagnosticLock = SRWLOCK_INIT;
            RenderEndpointDiagnosticV1  preparation, diagnostic;
        } runtime;
        thread_local FrameBoundaryScope* scope = nullptr;
        thread_local unsigned            presentDepth = 0;
        struct ResizeScope
        {
            std::uintptr_t viewport = 0, presenter = 0, device = 0;
            bool           matching = false;
            ResizeScope*   previous = nullptr;
        };
        thread_local ResizeScope* resizeScope = nullptr;

        bool Read(std::uintptr_t address, void* output, std::size_t bytes, std::uintptr_t image = 0, bool executable = false) noexcept
        {
            if (!address || !bytes || bytes > (std::numeric_limits<std::uintptr_t>::max)() - address)
                return false;
            auto       at = address;
            const auto end = address + bytes;
            while (at < end)
            {
                MEMORY_BASIC_INFORMATION info{};
                if (VirtualQuery(reinterpret_cast<void*>(at), &info, sizeof(info)) != sizeof(info) || info.State != MEM_COMMIT ||
                    (info.Protect & (PAGE_GUARD | PAGE_NOACCESS)) ||
                    (image && (info.Type != MEM_IMAGE || reinterpret_cast<std::uintptr_t>(info.AllocationBase) != image)))
                    return false;
                const auto protection = info.Protect & 0xff;
                const bool execute = protection == PAGE_EXECUTE_READ || protection == PAGE_EXECUTE_READWRITE || protection == PAGE_EXECUTE_WRITECOPY;
                if ((!execute && protection != PAGE_READONLY && protection != PAGE_READWRITE && protection != PAGE_WRITECOPY) || (executable && !execute))
                    return false;
                const auto start = reinterpret_cast<std::uintptr_t>(info.BaseAddress);
                if (info.RegionSize > (std::numeric_limits<std::uintptr_t>::max)() - start || start + info.RegionSize <= at)
                    return false;
                at = (start + info.RegionSize < end) ? start + info.RegionSize : end;
            }
            SIZE_T copied = 0;
            return ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(address), output, bytes, &copied) && copied == bytes;
        }
        template <typename T>
        bool Read(std::uintptr_t address, T& output) noexcept
        {
            return Read(address, &output, sizeof(output));
        }

        bool Hash(const void* bytes, std::size_t count, std::array<unsigned char, 32>& digest, std::int32_t* failure = nullptr) noexcept
        {
            BCRYPT_ALG_HANDLE  algorithm = nullptr;
            BCRYPT_HASH_HANDLE hash = nullptr;
            auto               status = BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0);
            if (status >= 0)
                status = BCryptCreateHash(algorithm, &hash, nullptr, 0, nullptr, 0, 0);
            if (status >= 0 && count > (std::numeric_limits<ULONG>::max)())
                status = static_cast<LONG>(0xc000000du);
            if (status >= 0)
                status = BCryptHashData(hash, reinterpret_cast<PUCHAR>(const_cast<void*>(bytes)), static_cast<ULONG>(count), 0);
            if (status >= 0)
                status = BCryptFinishHash(hash, digest.data(), static_cast<ULONG>(digest.size()), 0);
            if (failure)
                *failure = status;
            if (hash)
                BCryptDestroyHash(hash);
            if (algorithm)
                BCryptCloseAlgorithmProvider(algorithm, 0);
            return status >= 0;
        }
        bool DigestEquals(const std::array<unsigned char, 32>& digest, const char* expected) noexcept
        {
            constexpr char hex[] = "0123456789abcdef";
            for (std::size_t i = 0; i < digest.size(); ++i)
                if (hex[digest[i] >> 4] != expected[2 * i] || hex[digest[i] & 15] != expected[2 * i + 1])
                    return false;
            return true;
        }
        bool DigestMatches(const void* bytes, std::size_t count, const char* expected) noexcept
        {
            std::array<unsigned char, 32> digest{};
            return Hash(bytes, count, digest) && DigestEquals(digest, expected);
        }
        bool CheckFunction(std::uintptr_t image, Function& function) noexcept
        {
            return function.bytes <= function.original.size() && Read(image + function.rva, function.original.data(), function.bytes, image, true) &&
                   DigestMatches(function.original.data(), function.bytes, function.digest);
        }
        bool OwnedBranch(std::uintptr_t image, const Function& function, const unsigned char* code, std::size_t stolen, std::uintptr_t detour) noexcept
        {
            if (code[0] != 0xe9 || stolen < 5 || stolen > function.bytes)
                return false;
            for (std::size_t i = 5; i < stolen; ++i)
                if (code[i] != 0xcc)
                    return false;
            if (std::memcmp(code + stolen, function.original.data() + stolen, function.bytes - stolen))
                return false;
            std::int32_t displacement = 0;
            std::memcpy(&displacement, code + 1, sizeof(displacement));
            const auto                   relay = image + function.rva + 5 + displacement;
            std::array<unsigned char, 6> relayCode{};
            std::uintptr_t               target = 0;
            if (!Read(relay, relayCode.data(), relayCode.size(), 0, true) || relayCode[0] != 0xff || relayCode[1] != 0x25)
                return false;
            std::memcpy(&displacement, relayCode.data() + 2, sizeof(displacement));
            return Read(relay + 6 + displacement, target) && target == detour;
        }
        bool CurrentCode(std::uintptr_t image, const Function& function, bool installed) noexcept
        {
            std::array<unsigned char, maximumCode> code{};
            const auto&                            expected = installed ? function.installed : function.original;
            return Read(image + function.rva, code.data(), function.bytes, image, true) && std::memcmp(code.data(), expected.data(), function.bytes) == 0 &&
                   (!installed || (function.captured && OwnedBranch(image, function, code.data(), function.stolen, function.detour)));
        }
        bool AdditionalSignatures(std::uintptr_t image) noexcept
        {
            // These full source-pinned envelopes establish the member paths
            // and CPU tracker interpretation used below, not GPU completion.
            std::array<Function, 6> functions{{{0x1fd5f50, 1025, "e951e6b18c288915aa8865506180f1b908ed23d034f8cb8b8e17219732c3df69"},
                                               {0x1f848c0, 391, "293a804ad0d470c60154d1cd32abc23738b1a9e6950e648bc2e5299ef3c1dfbd"},
                                               {0x1f7aca0, 856, "e32226d9dad287b258d1fd83b52e90d3ba93904cf0987fff668a8b685f1bbd46"},
                                               {0x1f92160, 775, "72256b0abd5a4b99d82cc6c7bb2848ed7767556f810fefa2528d15f9f38f122c"},
                                               {0x1fb6190, 571, "f72bb94741009f07fcbd8fc60d176b003d24eb02f84249931cc48f6f00bc7e34"},
                                               {0x1fc60b0, 27, "107219b092bba34c8159af02029bd88f7f6664af802cc81a671a09f3c371c8d5"}}};
            for (auto& function : functions)
                if (!CheckFunction(image, function))
                    return false;
            return true;
        }
        bool CaptureOwnedPatch(std::uintptr_t image, Function& function, std::size_t stolen, void* detour) noexcept
        {
            // Detours 4.0.1 AMD64 emits E9 to its FF25 relay and CC padding
            // through the complete stolen instructions. Admit exactly that
            // owned branch and require every untouched function byte original.
            function.stolen = stolen;
            function.detour = reinterpret_cast<std::uintptr_t>(detour);
            function.captured = false;
            if (!Read(image + function.rva, function.installed.data(), function.bytes, image, true))
                return false;
            function.captured = OwnedBranch(image, function, function.installed.data(), stolen, function.detour);
            return function.captured;
        }
        bool OwnedForDetach(std::uintptr_t image, Function& function) noexcept
        {
            if (function.captured)
                return CurrentCode(image, function, true);
            // A transient post-commit read failure may be retried by End. The
            // proof is still the exact committed own branch and original tail;
            // this does not normalize or restore an unfamiliar code change.
            return function.detour && CaptureOwnedPatch(image, function, function.stolen, reinterpret_cast<void*>(function.detour));
        }
        void CacheDiagnosticModules() noexcept
        {
            // Hold only already-loaded module references during this lease.
            // Callback ownership checks then need no loader APIs or path IO,
            // and cannot mistake a reused base after an observed module unload.
            ReleaseFrameBoundaryDiagnosticModules();
            const wchar_t* names[]{L"dxgi.dll", nullptr, L"gameoverlayrenderer64.dll", L"amdihk64.dll", L"SocialClubD3D12Renderer.dll", L"socialclub.dll"};
            for (std::size_t i = 0; i < 6; ++i)
            {
                auto& cached = runtime.modules[i];
                cached.owner = static_cast<RenderEndpointOwnerClass>(i + 1);
                if (!names[i])
                    cached.base = runtime.image;
                else if (GetModuleHandleExW(0, names[i], &cached.retained))
                    cached.base = reinterpret_cast<std::uintptr_t>(cached.retained);
            }
        }
        void DescribeOwner(std::uintptr_t address, std::uint32_t& owner, std::uint32_t& rva) noexcept
        {
            owner = static_cast<unsigned>(RenderEndpointOwnerClass::Unknown);
            rva = 0;
            MEMORY_BASIC_INFORMATION page{};
            if (!address || VirtualQuery(reinterpret_cast<void*>(address), &page, sizeof(page)) != sizeof(page) || page.State != MEM_COMMIT ||
                (page.Protect & (PAGE_GUARD | PAGE_NOACCESS)))
                return;
            const auto protection = page.Protect & 0xff;
            const bool execute =
                protection == PAGE_EXECUTE_READ || protection == PAGE_EXECUTE_READWRITE || protection == PAGE_EXECUTE_WRITECOPY || protection == PAGE_EXECUTE;
            if (!execute)
            {
                owner = static_cast<unsigned>(RenderEndpointOwnerClass::OtherMemory);
                return;
            }
            if (page.Type != MEM_IMAGE)
            {
                owner = static_cast<unsigned>(RenderEndpointOwnerClass::PrivateExecutable);
                return;
            }
            const auto base = reinterpret_cast<std::uintptr_t>(page.AllocationBase);
            owner = static_cast<unsigned>(RenderEndpointOwnerClass::OtherImage);
            for (const auto& cached : runtime.modules)
                if (cached.base && cached.base == base)
                {
                    owner = static_cast<unsigned>(cached.owner);
                    break;
                }
            if (address >= base && address - base <= (std::numeric_limits<std::uint32_t>::max)())
                rva = static_cast<std::uint32_t>(address - base);
        }
        bool RelativeAddress(std::uintptr_t address, std::size_t prefix, std::int32_t displacement, std::uintptr_t& target) noexcept
        {
            if (prefix > (std::numeric_limits<std::uintptr_t>::max)() - address)
                return false;
            address += prefix;
            if (displacement >= 0)
            {
                if (static_cast<std::uintptr_t>(displacement) > (std::numeric_limits<std::uintptr_t>::max)() - address)
                    return false;
                target = address + displacement;
            }
            else
            {
                const auto distance = static_cast<std::uintptr_t>(-static_cast<std::int64_t>(displacement));
                if (distance > address)
                    return false;
                target = address - distance;
            }
            return true;
        }
        void DescribeBranch(std::uintptr_t address, std::uint32_t& kind, std::uint32_t& owner, std::uint32_t& rva) noexcept
        {
            // Classify one bounded entry instruction only. A branch destination
            // is metadata, never a callable replacement or a trusted hook chain.
            using Branch = RenderEndpointBranchKind;
            kind = static_cast<unsigned>(Branch::ReadFault);
            owner = rva = 0;
            std::array<unsigned char, 6> code{};
            if (!Read(address, code.data(), code.size(), 0, true))
                return;
            std::int32_t   displacement = 0;
            std::uintptr_t target = 0;
            if (code[0] == 0xe9)
            {
                std::memcpy(&displacement, code.data() + 1, sizeof(displacement));
                if (!RelativeAddress(address, 5, displacement, target))
                    return;
                kind = static_cast<unsigned>(Branch::RelativeE9);
            }
            else if (code[0] == 0xff && code[1] == 0x25)
            {
                std::memcpy(&displacement, code.data() + 2, sizeof(displacement));
                std::uintptr_t slot = 0;
                if (!RelativeAddress(address, 6, displacement, slot) || !Read(slot, target))
                    return;
                kind = static_cast<unsigned>(Branch::IndirectFF25);
            }
            else
            {
                kind = static_cast<unsigned>(Branch::NoSupportedBranch);
                return;
            }
            DescribeOwner(target, owner, rva);
        }
        bool PreparationFailure(RenderEndpointPrepareGate gate, RenderEndpointPrepareReason reason,
                                RenderEndpointErrorDomain domain = RenderEndpointErrorDomain::None, std::uint32_t code = 0) noexcept
        {
            runtime.preparation.preparationGate = static_cast<unsigned>(gate);
            runtime.preparation.preparationReason = static_cast<unsigned>(reason);
            runtime.preparation.errorDomain = static_cast<unsigned>(domain);
            runtime.preparation.errorCode = code;
            return false;
        }
        bool SystemEndpoint() noexcept
        {
            using Gate = RenderEndpointPrepareGate;
            using Reason = RenderEndpointPrepareReason;
            using Domain = RenderEndpointErrorDomain;
            runtime.preparation.version = RenderEndpointDiagnosticVersionV1;
            runtime.preparation.loadedEntryMatch = 0;
            runtime.preparation.systemBranchKind = runtime.preparation.systemBranchOwnerClass = runtime.preparation.systemBranchRva = 0;
            runtime.dxgi = 0;
            const auto module = GetModuleHandleW(L"dxgi.dll");
            if (!module)
                return PreparationFailure(Gate::ModuleLookup, Reason::OperationFailed, Domain::Win32, GetLastError());
            runtime.dxgi = reinterpret_cast<std::uintptr_t>(module);
            DescribeBranch(runtime.dxgi + runtime.presentCode.rva, runtime.preparation.systemBranchKind, runtime.preparation.systemBranchOwnerClass,
                           runtime.preparation.systemBranchRva);
            std::array<wchar_t, 32768> actual{}, expected{};
            const auto                 actualLength = GetModuleFileNameW(module, actual.data(), static_cast<DWORD>(actual.size()));
            const auto                 actualError = !actualLength ? GetLastError() : actualLength >= actual.size() ? ERROR_INSUFFICIENT_BUFFER : 0;
            const auto                 systemLength = GetSystemDirectoryW(expected.data(), static_cast<UINT>(expected.size()));
            const auto                 systemError = !systemLength ? GetLastError() : systemLength + 10 >= expected.size() ? ERROR_INSUFFICIENT_BUFFER : 0;
            if (!actualLength || actualLength >= actual.size())
                return PreparationFailure(Gate::ModulePath, Reason::OperationFailed, Domain::Win32, actualError);
            if (!systemLength || systemLength + 10 >= expected.size())
                return PreparationFailure(Gate::SystemPath, Reason::OperationFailed, Domain::Win32, systemError);
            const wchar_t suffix[] = L"\\dxgi.dll";
            std::memcpy(expected.data() + systemLength, suffix, sizeof(suffix));
            const auto pathComparison = CompareStringOrdinal(actual.data(), -1, expected.data(), -1, TRUE);
            if (!pathComparison)
                return PreparationFailure(Gate::SystemPath, Reason::OperationFailed, Domain::Win32, GetLastError());
            if (pathComparison != CSTR_EQUAL)
                return PreparationFailure(Gate::SystemPath, Reason::Mismatch);
            HANDLE file = CreateFileW(actual.data(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (file == INVALID_HANDLE_VALUE)
                return PreparationFailure(Gate::FileOpen, Reason::OperationFailed, Domain::Win32, GetLastError());
            LARGE_INTEGER      size{};
            BCRYPT_ALG_HANDLE  algorithm = nullptr;
            BCRYPT_HASH_HANDLE hash = nullptr;
            bool               ok = GetFileSizeEx(file, &size) != FALSE;
            if (!ok)
                PreparationFailure(Gate::FileSize, Reason::OperationFailed, Domain::Win32, GetLastError());
            else if (size.QuadPart != 1316248)
                ok = PreparationFailure(Gate::FileSize, Reason::Mismatch);
            if (ok)
            {
                const auto status = BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0);
                if (status < 0)
                    ok = PreparationFailure(Gate::HashProvider, Reason::OperationFailed, Domain::NtStatus, static_cast<std::uint32_t>(status));
            }
            if (ok)
            {
                const auto status = BCryptCreateHash(algorithm, &hash, nullptr, 0, nullptr, 0, 0);
                if (status < 0)
                    ok = PreparationFailure(Gate::HashCreate, Reason::OperationFailed, Domain::NtStatus, static_cast<std::uint32_t>(status));
            }
            std::array<unsigned char, 65536> block{};
            while (ok)
            {
                DWORD count = 0;
                ok = ReadFile(file, block.data(), static_cast<DWORD>(block.size()), &count, nullptr) != FALSE;
                if (!ok)
                    PreparationFailure(Gate::FileRead, Reason::OperationFailed, Domain::Win32, GetLastError());
                if (!ok || !count)
                    break;
                const auto status = BCryptHashData(hash, block.data(), count, 0);
                if (status < 0)
                    ok = PreparationFailure(Gate::HashUpdate, Reason::OperationFailed, Domain::NtStatus, static_cast<std::uint32_t>(status));
            }
            std::array<unsigned char, 32> digest{};
            if (ok)
            {
                const auto status = BCryptFinishHash(hash, digest.data(), static_cast<ULONG>(digest.size()), 0);
                if (status < 0)
                    ok = PreparationFailure(Gate::FileDigest, Reason::OperationFailed, Domain::NtStatus, static_cast<std::uint32_t>(status));
            }
            if (hash)
                BCryptDestroyHash(hash);
            if (algorithm)
                BCryptCloseAlgorithmProvider(algorithm, 0);
            CloseHandle(file);
            constexpr std::array<unsigned char, 32> expectedHash{0x42, 0x25, 0xe1, 0x3f, 0x27, 0xa4, 0xf3, 0xa4, 0xa4, 0x73, 0x8e,
                                                                 0x64, 0xa5, 0x7f, 0xa0, 0x94, 0x9d, 0x85, 0x89, 0xf3, 0x17, 0xfc,
                                                                 0x6f, 0x8e, 0xc1, 0x94, 0xb3, 0xa4, 0xd3, 0x94, 0xa5, 0x28};
            if (!ok)
                return false;
            if (digest != expectedHash)
                return PreparationFailure(Gate::FileDigest, Reason::Mismatch);
            DWORD64    base = 0;
            const auto unwind = RtlLookupFunctionEntry(runtime.dxgi + runtime.presentCode.rva, &base, nullptr);
            if (!unwind)
                return PreparationFailure(Gate::Unwind, Reason::OperationFailed);
            if (base != runtime.dxgi || unwind->BeginAddress != 0x19000 || unwind->EndAddress != 0x1948f || unwind->UnwindData != 1010704)
                return PreparationFailure(Gate::Unwind, Reason::Mismatch);
            if (!Read(runtime.dxgi + runtime.presentCode.rva, runtime.presentCode.original.data(), runtime.presentCode.bytes, runtime.dxgi, true))
            {
                runtime.preparation.loadedEntryMatch = static_cast<unsigned>(RenderEndpointLoadedMatch::ReadFault);
                return PreparationFailure(Gate::LoadedCodeRead, Reason::OperationFailed);
            }
            std::int32_t hashStatus = 0;
            if (!Hash(runtime.presentCode.original.data(), runtime.presentCode.bytes, digest, &hashStatus))
                return PreparationFailure(Gate::LoadedCodeHash, Reason::OperationFailed, Domain::NtStatus, static_cast<std::uint32_t>(hashStatus));
            const bool codeMatch = DigestEquals(digest, runtime.presentCode.digest);
            runtime.preparation.loadedEntryMatch = static_cast<unsigned>(codeMatch ? RenderEndpointLoadedMatch::Match : RenderEndpointLoadedMatch::Mismatch);
            if (!codeMatch)
                return PreparationFailure(Gate::LoadedCodeHash, Reason::Mismatch);
            HMODULE retained = nullptr;
            if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
                                    reinterpret_cast<LPCWSTR>(runtime.dxgi + runtime.presentCode.rva), &retained))
                return PreparationFailure(Gate::ModulePin, Reason::OperationFailed, Domain::Win32, GetLastError());
            runtime.preparation.preparationGate = static_cast<unsigned>(Gate::Complete);
            runtime.preparation.preparationReason = static_cast<unsigned>(Reason::Admitted);
            runtime.preparation.errorDomain = runtime.preparation.errorCode = 0;
            return true;
        }

        struct Roots
        {
            std::uintptr_t owner = 0, device = 0, manager = 0, queue = 0, swapchain = 0, custom = 0, contextRoot = 0, context = 0;
            std::uintptr_t contextDevice = 0, contextManager = 0, commandHandle = 0, commandData = 0, commandManager = 0;
            std::uintptr_t texture = 0, nativeTexture = 0, view = 0, resource = 0, rawResource = 0;
            std::uint32_t  index = 0, selectorA = 0, selectorB = 0, nodeMask = 0, tracked = 0, gpuCount = 0, afrCount = 0;
            unsigned char  alternate = 0;
        };
        bool ReadRoots(std::uintptr_t viewport, Roots& r) noexcept
        {
            std::uintptr_t table = 0, managerTable = 0, resourceTable = 0;
            if (!Read(viewport, table) || table != runtime.image + 0x4504060 || !Read(viewport + 0x18, r.owner) || !r.owner ||
                !Read(r.owner + 0x8a8, r.device) || !r.device || !Read(r.device + 0x38, r.manager) || !r.manager || !Read(r.manager, managerTable) ||
                managerTable != runtime.image + 0x44fbe88 || !Read(r.manager + 0x28, r.queue) || !r.queue || !Read(viewport + 0x60, r.swapchain) ||
                !r.swapchain || !Read(viewport + 0x1d0, r.custom) || !Read(runtime.image + 0x503e8e0, r.gpuCount) ||
                !Read(runtime.image + 0x503f45c, r.afrCount) || !Read(r.device + 0x670, r.contextRoot) || !r.contextRoot || !Read(r.contextRoot, r.context) ||
                !r.context || !Read(r.context + 0x140, r.contextDevice) || !Read(r.context + 0x13e, r.alternate) || !Read(r.context + 0x138, r.nodeMask) ||
                !Read(r.contextDevice + (r.alternate ? 0x48 : 0x38), r.contextManager) || !Read(r.context + 0x1d0, r.commandHandle) || !r.commandHandle ||
                !Read(r.commandHandle + 8, r.commandData) || !r.commandData || !Read(r.commandData + 0x18, r.commandManager) ||
                !Read(viewport + 0xa0, r.index) || !Read(viewport + 0x58, r.selectorA) || !Read(viewport + 0x118, r.selectorB) ||
                !Read(viewport + (r.selectorA == r.selectorB ? 0xa8 : 0x110), r.texture) || !r.texture || !Read(r.texture + 0x128, r.nativeTexture) ||
                r.nativeTexture < 0x68 || !Read(r.nativeTexture - 0x68 + 0x170, r.view) || !r.view || !Read(r.view + 0x50, r.resource) || !r.resource ||
                !Read(r.resource, resourceTable) || resourceTable != runtime.image + 0x4502080 || !Read(r.resource + 0x20, r.rawResource) || !r.rawResource ||
                !Read(r.resource + 0xc8, r.tracked))
                return false;
            return true;
        }
        bool SameRoots(const Roots& a, const Roots& b) noexcept
        {
            // Roots is value-initialized but padding is not part of its identity.
            return a.owner == b.owner && a.device == b.device && a.manager == b.manager && a.queue == b.queue && a.swapchain == b.swapchain &&
                   a.custom == b.custom && a.contextRoot == b.contextRoot && a.context == b.context && a.contextDevice == b.contextDevice &&
                   a.contextManager == b.contextManager && a.commandHandle == b.commandHandle && a.commandData == b.commandData &&
                   a.commandManager == b.commandManager && a.texture == b.texture && a.nativeTexture == b.nativeTexture && a.view == b.view &&
                   a.resource == b.resource && a.rawResource == b.rawResource && a.index == b.index && a.selectorA == b.selectorA &&
                   a.selectorB == b.selectorB && a.nodeMask == b.nodeMask && a.tracked == b.tracked && a.alternate == b.alternate && a.gpuCount == b.gpuCount &&
                   a.afrCount == b.afrCount;
        }
        enum class IdentityResult
        {
            Unknown,
            Match,
            Foreign
        };
        IdentityResult Identify(std::uintptr_t viewport, bool adopt) noexcept
        {
            std::array<std::uintptr_t, 3> values{viewport, 0, 0};
            if (!Read(viewport + 0x18, values[1]) || !values[1] || !Read(values[1] + 0x8a8, values[2]) || !values[2])
                return IdentityResult::Unknown;
            std::array<unsigned char, 32 + sizeof(values)> input{};
            std::memcpy(input.data(), runtime.key.data(), runtime.key.size());
            std::memcpy(input.data() + runtime.key.size(), values.data(), sizeof(values));
            std::array<unsigned char, 32> digest{}, addressDigest{};
            if (!Hash(input.data(), input.size(), digest) || !Hash(input.data(), runtime.key.size() + sizeof(viewport), addressDigest) ||
                !TryAcquireSRWLockExclusive(&runtime.identityLock))
                return IdentityResult::Unknown;
            if (adopt && !runtime.identitySet)
            {
                runtime.identity = digest;
                runtime.addressIdentity = addressDigest;
                runtime.identitySet = true;
            }
            // A changed tuple at the selected address can be reuse or partial
            // destruction. Only a separately hashed different address proves
            // that this naturally invoked destructor belongs to another view.
            const auto result = !runtime.identitySet                       ? IdentityResult::Unknown
                                : runtime.identity == digest               ? IdentityResult::Match
                                : runtime.addressIdentity != addressDigest ? IdentityResult::Foreign
                                                                           : IdentityResult::Unknown;
            ReleaseSRWLockExclusive(&runtime.identityLock);
            return result;
        }
        bool Identity(std::uintptr_t viewport, bool adopt) noexcept
        {
            return Identify(viewport, adopt) == IdentityResult::Match;
        }

        bool Release(IUnknown*& value) noexcept
        {
            IUnknown* owned = value;
            value = nullptr;
            if (!owned)
                return true;
            __try
            {
                owned->Release();
                return true;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                return false;
            }
        }
        class PresenterReads final : public CustomPresentReads
        {
        public:
            bool Object(std::uintptr_t address, void* output, std::size_t bytes) override { return Read(address, output, bytes); }
            bool Image(std::uintptr_t address, void* output, std::size_t bytes, bool executable) override
            {
                if (executable && address == runtime.image + runtime.notifyCode.rva && bytes == runtime.notifyCode.bytes)
                    return ReadFrameBoundaryOwnedOriginal(address, output, bytes);
                return Read(address, output, bytes, runtime.image, executable);
            }
        };
        class Queries final : public FrameBoundaryQueries
        {
        public:
            explicit Queries(FrameBoundaryScope& selected) noexcept : selected(selected), epoch(runtime.epoch.load()) {}
            std::int32_t Execute(Stage stage, FrameBoundaryObservationState& state) override
            {
                switch (stage)
                {
                    case Stage::NativeRoots:
                    {
                        if (!ReadRoots(selected.viewport, roots))
                            return E_POINTER;
                        PresenterReads                   reads;
                        CustomPresentClassificationState custom;
                        CustomPresentProbeRoots          customRoots;
                        ClassifyCustomPresent(reads, runtime.image, roots.custom, roots.device, custom, customRoots);
                        state.nativeBufferIndex = roots.index;
                        if (!selected.knownPresenter || custom.status != static_cast<unsigned>(CustomPresentClassificationStatus::KnownNativePresenter) ||
                            roots.gpuCount != 1 || roots.afrCount != 1 || roots.swapchain != selected.swapchain || roots.contextDevice != roots.device ||
                            roots.alternate || roots.nodeMask != 1 || roots.contextManager != roots.manager || roots.commandManager != roots.manager)
                            return S_FALSE;
                        state.factsMask |= 8;
                        return S_OK;
                    }
                    case Stage::SwapchainInterface:
                    {
                        auto hr = reinterpret_cast<IUnknown*>(roots.swapchain)->QueryInterface(IID_PPV_ARGS(&chain));
                        if (FAILED(hr) || !chain)
                            return FAILED(hr) ? hr : E_POINTER;
                        hr = chain->QueryInterface(IID_PPV_ARGS(&chainIdentity));
                        if (FAILED(hr) || !chainIdentity)
                            return FAILED(hr) ? hr : E_POINTER;
                        hr = reinterpret_cast<IUnknown*>(roots.swapchain)->QueryInterface(IID_PPV_ARGS(&nativeChainIdentity));
                        if (FAILED(hr) || !nativeChainIdentity)
                            return FAILED(hr) ? hr : E_POINTER;
                        if (chainIdentity != nativeChainIdentity)
                            return S_FALSE;
                        state.factsMask |= 1;
                        state.dxgiBufferIndex = chain->GetCurrentBackBufferIndex();
                        if (state.dxgiBufferIndex != roots.index)
                            return S_FALSE;
                        state.factsMask |= 16;
                        return S_OK;
                    }
                    case Stage::Backbuffer:
                    {
                        const auto hr = chain->GetBuffer(state.dxgiBufferIndex, IID_PPV_ARGS(&buffer));
                        return FAILED(hr) ? hr : buffer ? S_OK : E_POINTER;
                    }
                    case Stage::NativeResourceComparison:
                        // GetBuffer already holds this exact interface alive. A
                        // different raw native pointer is never QI'd or called.
                        if (roots.rawResource != reinterpret_cast<std::uintptr_t>(buffer))
                            return S_FALSE;
                        state.factsMask |= 2;
                        return S_OK;
                    case Stage::QueueInterface:
                    {
                        std::uintptr_t currentQueue = 0;
                        if (!Read(roots.contextManager + 0x28, currentQueue))
                            return E_POINTER;
                        if (currentQueue != roots.queue)
                            return S_FALSE;
                        const auto hr = reinterpret_cast<IUnknown*>(roots.queue)->QueryInterface(IID_PPV_ARGS(&queue));
                        if (FAILED(hr) || !queue)
                            return FAILED(hr) ? hr : E_POINTER;
                        return queue->GetDesc().Type == D3D12_COMMAND_LIST_TYPE_DIRECT ? S_OK : S_FALSE;
                    }
                    case Stage::QueueIdentity:
                    {
                        auto hr = queue->QueryInterface(IID_PPV_ARGS(&queueIdentity));
                        if (FAILED(hr) || !queueIdentity)
                            return FAILED(hr) ? hr : E_POINTER;
                        hr = reinterpret_cast<IUnknown*>(roots.queue)->QueryInterface(IID_PPV_ARGS(&nativeQueueIdentity));
                        if (FAILED(hr) || !nativeQueueIdentity)
                            return FAILED(hr) ? hr : E_POINTER;
                        if (queueIdentity != nativeQueueIdentity)
                            return S_FALSE;
                        hr = queue->GetDevice(IID_PPV_ARGS(&queueDevice));
                        if (FAILED(hr) || !queueDevice)
                            return FAILED(hr) ? hr : E_POINTER;
                        hr = buffer->GetDevice(IID_PPV_ARGS(&bufferDevice));
                        if (FAILED(hr) || !bufferDevice)
                            return FAILED(hr) ? hr : E_POINTER;
                        hr = chain->GetDevice(IID_PPV_ARGS(&chainDevice));
                        if (FAILED(hr) || !chainDevice)
                            return FAILED(hr) ? hr : E_POINTER;
                        hr = queueDevice->QueryInterface(IID_PPV_ARGS(&queueDeviceIdentity));
                        if (FAILED(hr) || !queueDeviceIdentity)
                            return FAILED(hr) ? hr : E_POINTER;
                        hr = bufferDevice->QueryInterface(IID_PPV_ARGS(&bufferDeviceIdentity));
                        if (FAILED(hr) || !bufferDeviceIdentity)
                            return FAILED(hr) ? hr : E_POINTER;
                        hr = chainDevice->QueryInterface(IID_PPV_ARGS(&chainDeviceIdentity));
                        if (FAILED(hr) || !chainDeviceIdentity)
                            return FAILED(hr) ? hr : E_POINTER;
                        if (queueDeviceIdentity != bufferDeviceIdentity || queueDeviceIdentity != chainDeviceIdentity || queueDevice->GetNodeCount() != 1)
                            return S_FALSE;
                        state.factsMask |= 4;
                        return S_OK;
                    }
                    case Stage::NativeTrackedState:
                        state.nativeTrackedMode = (roots.tracked & 0x80000000u) ? 1u : 0u;
                        if (!state.nativeTrackedMode)
                            return S_FALSE;
                        state.nativeTrackedState = roots.tracked & 0x7fffffffu;
                        state.factsMask |= 32;
                        if (state.nativeTrackedState)
                            return S_FALSE;
                        state.factsMask |= 64;
                        return S_OK;
                    case Stage::RootReadback:
                    {
                        state.factsMask &= 127u;
                        Roots again;
                        if (!ReadRoots(selected.viewport, again))
                            return E_POINTER;
                        if (runtime.resizing.load() || runtime.destroyed.load() || runtime.epoch.load() != epoch)
                        {
                            state.status = static_cast<unsigned>(BoundaryStatus::ResizeOverlap);
                            return S_FALSE;
                        }
                        if (!SameRoots(roots, again))
                        {
                            state.status = static_cast<unsigned>(BoundaryStatus::RootsChanged);
                            return S_FALSE;
                        }
                        state.factsMask |= 128;
                        return S_OK;
                    }
                    default:
                        return E_INVALIDARG;
                }
            }
            bool Close() noexcept override
            {
                bool      ok = true;
                IUnknown* refs[]{chainDeviceIdentity,
                                 bufferDeviceIdentity,
                                 queueDeviceIdentity,
                                 chainDevice,
                                 bufferDevice,
                                 queueDevice,
                                 nativeQueueIdentity,
                                 queueIdentity,
                                 queue,
                                 nativeChainIdentity,
                                 chainIdentity,
                                 buffer,
                                 chain};
                chainDeviceIdentity = bufferDeviceIdentity = queueDeviceIdentity = nullptr;
                chainDevice = bufferDevice = queueDevice = nullptr;
                nativeQueueIdentity = queueIdentity = nativeChainIdentity = chainIdentity = nullptr;
                queue = nullptr;
                buffer = nullptr;
                chain = nullptr;
                for (auto& ref : refs)
                    if (!Release(ref))
                        ok = false;
                return ok;
            }

        private:
            FrameBoundaryScope& selected;
            std::uint64_t       epoch;
            Roots               roots;
            IDXGISwapChain3*    chain = nullptr;
            ID3D12Resource*     buffer = nullptr;
            ID3D12CommandQueue* queue = nullptr;
            IUnknown *          chainIdentity = nullptr, *nativeChainIdentity = nullptr, *queueIdentity = nullptr, *nativeQueueIdentity = nullptr;
            ID3D12Device *      queueDevice = nullptr, *bufferDevice = nullptr, *chainDevice = nullptr;
            IUnknown *          queueDeviceIdentity = nullptr, *bufferDeviceIdentity = nullptr, *chainDeviceIdentity = nullptr;
        };
        HRESULT Execute(FrameBoundaryQueries& queries, Stage stage, FrameBoundaryObservationState& state) noexcept
        {
            __try
            {
                return queries.Execute(stage, state);
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                return E_POINTER;
            }
        }
        bool Close(FrameBoundaryQueries& queries) noexcept
        {
            __try
            {
                return queries.Close();
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                return false;
            }
        }
        void CopyLifecycle(FrameBoundaryObservationState& state) noexcept
        {
            AcquireSRWLockShared(&runtime.lifecycleLock);
            state.resizeEpoch = runtime.epoch.load();
            state.resizeEntries = runtime.resizeEntries.load();
            state.preReleaseEntries = runtime.notifyEntries.load();
            state.foreignLifecycleEntries = runtime.foreignEntries.load();
            state.resizeThreadId = runtime.resizeThread.load();
            state.preReleaseThreadId = runtime.notifyThread.load();
            state.lifecycleFlags = (runtime.resizing.load() ? 1u : 0u) | (runtime.destroyed.load() ? 2u : 0u);
            ReleaseSRWLockShared(&runtime.lifecycleLock);
        }
        HRESULT PresentBody(IDXGISwapChain* chain, UINT interval, UINT flags, void* caller)
        {
            FrameBoundaryScope* selected = scope;
            const bool          relevant = selected && runtime.armed.load() && reinterpret_cast<std::uintptr_t>(chain) == selected->swapchain &&
                                  reinterpret_cast<std::uintptr_t>(caller) == runtime.image + 0x1fd5ec2;
            if (relevant)
            {
                selected->entered = true;
                auto&      state = selected->state;
                const auto entries = state.presentEntries, rejected = state.rejectedEntries;
                state = {};
                state.endpointProfileId = 1;
                state.presentEntries = entries;
                state.rejectedEntries = rejected;
                ++state.presentEntries;
                state.presentThreadId = GetCurrentThreadId();
                if (presentDepth != 1 || state.presentThreadId != selected->callbackThread)
                    state.status = static_cast<unsigned>(BoundaryStatus::WrongThreadOrNested);
                else if (!CurrentCode(runtime.dxgi, runtime.presentCode, true))
                    state.status = static_cast<unsigned>(BoundaryStatus::EndpointChanged);
                else if (runtime.resizing.load() || runtime.destroyed.load())
                    state.status = static_cast<unsigned>(BoundaryStatus::ResizeOverlap);
                else
                {
                    Queries queries(*selected);
                    selected->cleanupFailed = !RunFrameBoundaryQueries(queries, state);
                    if (selected->cleanupFailed)
                    {
                        runtime.armed.store(false);
                        if (selected->cleanupFailure)
                            selected->cleanupFailure();
                    }
                    else if (state.status == static_cast<unsigned>(BoundaryStatus::ObservedCoherent))
                    {
                        // Final Release may run arbitrary foreign code. Check
                        // borrowed roots again after every local ref is gone.
                        const auto hr = Execute(queries, Stage::RootReadback, state);
                        if (hr != S_OK)
                        {
                            state.queryStage = static_cast<unsigned>(Stage::RootReadback);
                            state.queryHresult = static_cast<unsigned>(hr);
                            if (FAILED(hr))
                                state.status = static_cast<unsigned>(BoundaryStatus::QueryFailed);
                        }
                    }
                }
            }
            const auto result = runtime.present(chain, interval, flags);
            if (relevant)
            {
                auto& state = selected->state;
                state.presentHresult = static_cast<std::uint32_t>(result);
                if (FAILED(result) && state.status == static_cast<unsigned>(BoundaryStatus::ObservedCoherent))
                    state.status = static_cast<unsigned>(BoundaryStatus::PresentFailed);
                if (state.status != static_cast<unsigned>(BoundaryStatus::ObservedCoherent))
                    ++state.rejectedEntries;
            }
            return result;
        }
        HRESULT STDMETHODCALLTYPE HookPresent(IDXGISwapChain* chain, UINT interval, UINT flags)
        {
            runtime.inFlight.fetch_add(1);
            ++presentDepth;
            void* caller = _ReturnAddress();
            __try
            {
                return PresentBody(chain, interval, flags, caller);
            }
            __finally
            {
                --presentDepth;
                runtime.inFlight.fetch_sub(1);
            }
        }
        void HookResize(void* viewport, UINT width, UINT height, bool fullscreen)
        {
            runtime.inFlight.fetch_add(1);
            ResizeScope local;
            local.viewport = reinterpret_cast<std::uintptr_t>(viewport);
            local.previous = resizeScope;
            const bool counted = runtime.armed.load();
            local.matching = counted && Identity(local.viewport, false);
            if (counted)
            {
                if (local.matching)
                {
                    std::uintptr_t owner = 0;
                    if (!Read(local.viewport + 0x18, owner) || !Read(owner + 0x8a8, local.device) || !Read(local.viewport + 0x1d0, local.presenter))
                        local.matching = false;
                }
                AcquireSRWLockExclusive(&runtime.lifecycleLock);
                runtime.epoch.fetch_add(1);
                runtime.resizing.fetch_add(1);
                if (local.matching)
                {
                    runtime.resizeEntries.fetch_add(1);
                    runtime.resizeThread.store(GetCurrentThreadId());
                }
                if (!local.matching)
                    runtime.foreignEntries.fetch_add(1);
                ReleaseSRWLockExclusive(&runtime.lifecycleLock);
            }
            resizeScope = &local;
            __try
            {
                runtime.resize(viewport, width, height, fullscreen);
            }
            __finally
            {
                resizeScope = local.previous;
                if (counted)
                {
                    AcquireSRWLockExclusive(&runtime.lifecycleLock);
                    runtime.epoch.fetch_add(1);
                    runtime.resizing.fetch_sub(1);
                    ReleaseSRWLockExclusive(&runtime.lifecycleLock);
                }
                runtime.inFlight.fetch_sub(1);
            }
        }
        void HookNotify(void* presenter)
        {
            runtime.inFlight.fetch_add(1);
            if (runtime.armed.load())
            {
                std::uintptr_t device = 0, table = 0;
                const bool     matching = resizeScope && resizeScope->matching && resizeScope->presenter == reinterpret_cast<std::uintptr_t>(presenter) &&
                                      reinterpret_cast<std::uintptr_t>(_ReturnAddress()) == runtime.image + 0x1fd5990 &&
                                      Read(reinterpret_cast<std::uintptr_t>(presenter), table) && table == runtime.image + 0x42296f0 &&
                                      Read(reinterpret_cast<std::uintptr_t>(presenter) + 0x18, device) && device == resizeScope->device;
                AcquireSRWLockExclusive(&runtime.lifecycleLock);
                if (matching)
                {
                    runtime.notifyEntries.fetch_add(1);
                    runtime.notifyThread.store(GetCurrentThreadId());
                }
                else
                {
                    runtime.foreignEntries.fetch_add(1);
                    runtime.epoch.fetch_add(1);
                }
                ReleaseSRWLockExclusive(&runtime.lifecycleLock);
            }
            __try
            {
                runtime.notify(presenter);
            }
            __finally
            {
                runtime.inFlight.fetch_sub(1);
            }
        }
        void HookDestroy(void* viewport)
        {
            runtime.inFlight.fetch_add(1);
            if (runtime.armed.load())
            {
                const auto identity = Identify(reinterpret_cast<std::uintptr_t>(viewport), false);
                AcquireSRWLockExclusive(&runtime.lifecycleLock);
                runtime.epoch.fetch_add(1);
                // Unknown includes read/hash/lock failure. It must poison this
                // lease's lifetime rather than allow a missed destruction and
                // later same-address reuse to inherit a coherent association.
                if (identity != IdentityResult::Foreign)
                    runtime.destroyed.store(true);
                if (identity != IdentityResult::Match)
                    runtime.foreignEntries.fetch_add(1);
                ReleaseSRWLockExclusive(&runtime.lifecycleLock);
            }
            __try
            {
                runtime.destroy(viewport);
            }
            __finally
            {
                runtime.inFlight.fetch_sub(1);
            }
        }
    }

    bool RunFrameBoundaryQueries(FrameBoundaryQueries& queries, FrameBoundaryObservationState& state) noexcept
    {
        using namespace boundary_detail;
        state.status = static_cast<unsigned>(BoundaryStatus::QueryFailed);
        for (unsigned value = static_cast<unsigned>(Stage::NativeRoots); value <= static_cast<unsigned>(Stage::RootReadback); ++value)
        {
            const auto stage = static_cast<Stage>(value);
            state.queryStage = value;
            const auto hr = Execute(queries, stage, state);
            state.queryHresult = static_cast<std::uint32_t>(hr);
            if (hr != S_OK)
            {
                if (hr == S_FALSE && state.status == static_cast<unsigned>(BoundaryStatus::QueryFailed))
                    state.status = static_cast<unsigned>(BoundaryStatus::UnsupportedAssociation);
                break;
            }
            if (stage == Stage::RootReadback)
            {
                state.queryStage = static_cast<unsigned>(Stage::Complete);
                state.status = static_cast<unsigned>(BoundaryStatus::ObservedCoherent);
            }
        }
        if (!Close(queries))
        {
            state.status = static_cast<unsigned>(BoundaryStatus::QueryFailed);
            state.queryHresult = static_cast<unsigned>(E_UNEXPECTED);
            return false;
        }
        return true;
    }
    void ResetFrameBoundaryObservation() noexcept
    {
        using namespace boundary_detail;
        runtime.armed.store(false);
        ReleaseFrameBoundaryDiagnosticModules();
        AcquireSRWLockExclusive(&runtime.diagnosticLock);
        runtime.preparation = runtime.diagnostic = {};
        ReleaseSRWLockExclusive(&runtime.diagnosticLock);
        runtime.prepared = false;
        runtime.endpoint = false;
        runtime.dxgi = 0;
        runtime.committed = false;
        runtime.epoch.store(0);
        runtime.resizeEntries.store(0);
        runtime.notifyEntries.store(0);
        runtime.foreignEntries.store(0);
        runtime.resizeThread.store(0);
        runtime.notifyThread.store(0);
        runtime.resizing.store(0);
        runtime.destroyed.store(false);
        runtime.identitySet = false;
        runtime.identity = {};
        runtime.addressIdentity = {};
        runtime.key = {};
        runtime.keyReady = BCryptGenRandom(nullptr, runtime.key.data(), static_cast<ULONG>(runtime.key.size()), BCRYPT_USE_SYSTEM_PREFERRED_RNG) >= 0;
    }
    bool PrepareFrameBoundaryHooks(std::uintptr_t image, std::uint64_t generation) noexcept
    {
        using namespace boundary_detail;
        runtime.image = image;
        if (!runtime.keyReady)
            return false;
        if (!CheckFunction(image, runtime.resizeCode) || !CheckFunction(image, runtime.notifyCode) || !CheckFunction(image, runtime.destroyCode) ||
            !AdditionalSignatures(image))
            return false;
        runtime.resize = reinterpret_cast<Resize>(image + runtime.resizeCode.rva);
        runtime.notify = reinterpret_cast<Notify>(image + runtime.notifyCode.rva);
        runtime.destroy = reinterpret_cast<Notify>(image + runtime.destroyCode.rva);
        CacheDiagnosticModules();
        runtime.preparation = {};
        runtime.preparation.generationLow = static_cast<std::uint32_t>(generation);
        runtime.preparation.generationHigh = static_cast<std::uint32_t>(generation >> 32);
        runtime.endpoint = SystemEndpoint();
        AcquireSRWLockExclusive(&runtime.diagnosticLock);
        runtime.diagnostic = runtime.preparation;
        ReleaseSRWLockExclusive(&runtime.diagnosticLock);
        runtime.present = runtime.endpoint ? reinterpret_cast<Present>(runtime.dxgi + runtime.presentCode.rva) : nullptr;
        runtime.prepared = true;
        return true;
    }
    bool ChangeFrameBoundaryHooks(bool install) noexcept
    {
        using namespace boundary_detail;
        if (!runtime.prepared)
            return true;
        const auto change = install ? DetourAttach : DetourDetach;
        return change(reinterpret_cast<PVOID*>(&runtime.resize), reinterpret_cast<PVOID>(&HookResize)) == NO_ERROR &&
               change(reinterpret_cast<PVOID*>(&runtime.notify), reinterpret_cast<PVOID>(&HookNotify)) == NO_ERROR &&
               change(reinterpret_cast<PVOID*>(&runtime.destroy), reinterpret_cast<PVOID>(&HookDestroy)) == NO_ERROR &&
               (!runtime.endpoint || change(reinterpret_cast<PVOID*>(&runtime.present), reinterpret_cast<PVOID>(&HookPresent)) == NO_ERROR);
    }
    bool FrameBoundaryHooksCommitted(bool install) noexcept
    {
        using namespace boundary_detail;
        runtime.committed = install;
        if (!install)
        {
            runtime.armed.store(false);
            return true;
        }
        // Inspect every committed hook even if an earlier read fails. No
        // partially initialized byte cache can authorize observation or detach.
        const bool resize = CaptureOwnedPatch(runtime.image, runtime.resizeCode, 5, reinterpret_cast<void*>(&HookResize));
        const bool notify = CaptureOwnedPatch(runtime.image, runtime.notifyCode, 6, reinterpret_cast<void*>(&HookNotify));
        const bool destroy = CaptureOwnedPatch(runtime.image, runtime.destroyCode, 6, reinterpret_cast<void*>(&HookDestroy));
        const bool present = !runtime.endpoint || CaptureOwnedPatch(runtime.dxgi, runtime.presentCode, 5, reinterpret_cast<void*>(&HookPresent));
        const bool ok = resize && notify && destroy && present;
        runtime.armed.store(ok);
        return ok;
    }
    bool FrameBoundaryHooksOwned() noexcept
    {
        using namespace boundary_detail;
        const bool resize = !runtime.committed || OwnedForDetach(runtime.image, runtime.resizeCode);
        const bool notify = !runtime.committed || OwnedForDetach(runtime.image, runtime.notifyCode);
        const bool destroy = !runtime.committed || OwnedForDetach(runtime.image, runtime.destroyCode);
        const bool present = !runtime.committed || !runtime.endpoint || OwnedForDetach(runtime.dxgi, runtime.presentCode);
        return resize && notify && destroy && present;
    }
    unsigned FrameBoundaryInFlight() noexcept
    {
        return boundary_detail::runtime.inFlight.load();
    }
    void StopFrameBoundaryAdmission() noexcept
    {
        boundary_detail::runtime.armed.store(false);
    }
    bool ReadFrameBoundaryOwnedOriginal(std::uintptr_t address, void* output, std::size_t bytes) noexcept
    {
        using namespace boundary_detail;
        if (!runtime.committed || address != runtime.image + runtime.notifyCode.rva || bytes != runtime.notifyCode.bytes ||
            !CurrentCode(runtime.image, runtime.notifyCode, true))
            return false;
        std::memcpy(output, runtime.notifyCode.original.data(), bytes);
        return true;
    }
    void EnterFrameBoundaryScope(FrameBoundaryScope& selected, std::uintptr_t image, void* viewport, std::uint32_t callbackThread,
                                 const CustomPresentClassificationState& custom, void (*cleanupFailure)() noexcept) noexcept
    {
        using namespace boundary_detail;
        selected.image = image;
        selected.viewport = reinterpret_cast<std::uintptr_t>(viewport);
        selected.callbackThread = callbackThread;
        selected.epoch = runtime.epoch.load();
        selected.cleanupFailure = cleanupFailure;
        selected.knownPresenter = custom.status == static_cast<unsigned>(CustomPresentClassificationStatus::KnownNativePresenter);
        selected.diagnostic = runtime.preparation;
        selected.state.endpointProfileId = runtime.endpoint ? 1u : 0u;
        selected.state.status = static_cast<unsigned>(runtime.endpoint ? BoundaryStatus::NoScopedPresent : BoundaryStatus::EndpointUnadmitted);
        std::uintptr_t table = 0, endpoint = 0;
        bool           slotRead = false;
        selected.diagnostic.slotStatus = static_cast<unsigned>(RenderEndpointSlotStatus::ReadFault);
        if (!Identity(selected.viewport, true) || !Read(selected.viewport + 0x60, selected.swapchain) || !Read(selected.swapchain, table) ||
            !(slotRead = Read(table + 0x40, endpoint)))
        {
            selected.state.status = static_cast<unsigned>(BoundaryStatus::QueryFailed);
            selected.state.queryStage = static_cast<unsigned>(Stage::NativeRoots);
            selected.state.queryHresult = static_cast<unsigned>(E_POINTER);
        }
        else if (!runtime.endpoint || endpoint != runtime.dxgi + runtime.presentCode.rva)
        {
            selected.state.status = static_cast<unsigned>(BoundaryStatus::EndpointUnadmitted);
            selected.state.endpointProfileId = 0;
            selected.swapchain = 0;
        }
        else if (!CurrentCode(runtime.dxgi, runtime.presentCode, true))
        {
            selected.state.status = static_cast<unsigned>(BoundaryStatus::EndpointChanged);
            selected.swapchain = 0;
        }
        selected.previous = scope;
        if (slotRead)
        {
            selected.diagnostic.slotStatus = static_cast<unsigned>(RenderEndpointSlotStatus::Read);
            DescribeOwner(endpoint, selected.diagnostic.slotOwnerClass, selected.diagnostic.slotRva);
            selected.diagnostic.slotEqualsPinned = runtime.dxgi && endpoint == runtime.dxgi + runtime.presentCode.rva ? 1u : 0u;
            DescribeBranch(endpoint, selected.diagnostic.slotBranchKind, selected.diagnostic.slotBranchOwnerClass, selected.diagnostic.slotBranchRva);
        }
        scope = &selected;
    }
    void LeaveFrameBoundaryScope(FrameBoundaryScope& selected) noexcept
    {
        using namespace boundary_detail;
        CopyLifecycle(selected.state);
        // A later lifecycle conflict invalidates a completed association, but
        // cannot rewrite an earlier negative result into a different query stage.
        const bool complete = selected.state.status == static_cast<unsigned>(BoundaryStatus::ObservedCoherent) ||
                              selected.state.status == static_cast<unsigned>(BoundaryStatus::PresentFailed);
        if (selected.entered && complete && (selected.epoch != selected.state.resizeEpoch || selected.state.lifecycleFlags))
        {
            if (selected.state.status == static_cast<unsigned>(BoundaryStatus::ObservedCoherent))
                ++selected.state.rejectedEntries;
            selected.state.status = static_cast<unsigned>(BoundaryStatus::ResizeOverlap);
            selected.state.factsMask &= 127u;
        }
        scope = selected.previous;
        selected.viewport = selected.swapchain = selected.image = 0;
        selected.previous = nullptr;
    }
    void PublishFrameBoundaryEndpointDiagnostic(const FrameBoundaryScope& selected, std::uint64_t acceptedSnapshots) noexcept
    {
        using namespace boundary_detail;
        if (selected.diagnostic.version != RenderEndpointDiagnosticVersionV1)
            return;
        auto next = selected.diagnostic;
        next.acceptedSnapshotsLow = static_cast<std::uint32_t>(acceptedSnapshots);
        next.acceptedSnapshotsHigh = static_cast<std::uint32_t>(acceptedSnapshots >> 32);
        AcquireSRWLockExclusive(&runtime.diagnosticLock);
        runtime.diagnostic = next;
        ReleaseSRWLockExclusive(&runtime.diagnosticLock);
    }
    bool ReadFrameBoundaryEndpointDiagnostic(std::uint64_t generation, RenderEndpointDiagnosticV1& state) noexcept
    {
        using namespace boundary_detail;
        AcquireSRWLockShared(&runtime.diagnosticLock);
        const auto next = runtime.diagnostic;
        ReleaseSRWLockShared(&runtime.diagnosticLock);
        if (next.version != RenderEndpointDiagnosticVersionV1 || next.generationLow != static_cast<std::uint32_t>(generation) ||
            next.generationHigh != static_cast<std::uint32_t>(generation >> 32))
            return false;
        state = next;
        return true;
    }
    void ReleaseFrameBoundaryDiagnosticModules() noexcept
    {
        using namespace boundary_detail;
        // Only the owner calls this after hooks/callbacks drain. Failed End
        // keeps these references together with its retained trampoline state.
        for (auto& cached : runtime.modules)
        {
            const auto retained = cached.retained;
            cached = {};
            if (retained)
                FreeLibrary(retained);
        }
    }
}
