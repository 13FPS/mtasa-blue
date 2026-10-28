// Private test support, included only after the real Probe implementation.
// Owned executable-image bytes satisfy its unchanged MEM_IMAGE/AllocationBase
// guards. They model installed owned profiles, never the game's admission hash.
#include <array>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <limits>
#pragma section(".mtapol", read, execute)
__declspec(allocate(".mtapol")) __declspec(align(4096)) unsigned char ownedPolicyImageCode[4096] = {0x90};

namespace NativePresentPolicyFixture
{
    namespace Detail = mta::game_de::boundary_detail;
    using mta::game_de::RenderEndpointOwnerClass;
    using mta::game_de::RenderEndpointPrepareGate;

    class OwnedPolicyImage
    {
    public:
        alignas(16) std::array<unsigned char, 0x220> viewport{};
        alignas(16) std::array<unsigned char, 0x900> owner{};
        alignas(16) std::array<unsigned char, 0x100> device{}, manager{}, queue{};
        std::array<std::uintptr_t, 10>  chainTable{};
        std::uintptr_t                  chain = 0;
        std::uintptr_t                  image = 0;
        HMODULE                         dxgi = nullptr;
        DWORD                           previousProtection = 0;
        bool                            imageWritable = false;
        std::array<unsigned char, 4096> originalImageBytes{};
        bool                            valid = false;

        template <typename T>
        static void Put(void* destination, std::size_t offset, T value)
        {
            std::memcpy(static_cast<unsigned char*>(destination) + offset, &value, sizeof(value));
        }
        static void Noop(void*) {}
        static void ResizeNoop(void*, UINT, UINT, bool) {}

        OwnedPolicyImage()
        {
            image = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
            MEMORY_BASIC_INFORMATION page{};
            if (!image || VirtualQuery(ownedPolicyImageCode, &page, sizeof(page)) != sizeof(page) || page.Type != MEM_IMAGE ||
                reinterpret_cast<std::uintptr_t>(page.AllocationBase) != image)
                return;
            std::memcpy(originalImageBytes.data(), ownedPolicyImageCode, originalImageBytes.size());
            if (!VirtualProtect(ownedPolicyImageCode, sizeof(ownedPolicyImageCode), PAGE_EXECUTE_READWRITE, &previousProtection))
                return;
            imageWritable = true;
            dxgi = LoadLibraryExW(L"dxgi.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
            valid = dxgi != nullptr;
        }
        OwnedPolicyImage(const OwnedPolicyImage&) = delete;
        OwnedPolicyImage& operator=(const OwnedPolicyImage&) = delete;
        ~OwnedPolicyImage()
        {
            // Test-owned arrays are never handed to VirtualFree. Clear their
            // cache only after the test has retired all actual policy scopes.
            Detail::runtime.dispatchRelay = nullptr;
            Detail::runtime.dispatchPatched = false;
            mta::game_de::ReleaseFrameBoundaryDiagnosticModules();
            if (dxgi)
                FreeLibrary(dxgi);
            if (imageWritable)
            {
                std::memcpy(ownedPolicyImageCode, originalImageBytes.data(), originalImageBytes.size());
                FlushInstructionCache(GetCurrentProcess(), ownedPolicyImageCode, originalImageBytes.size());
                DWORD ignored = 0;
                VirtualProtect(ownedPolicyImageCode, sizeof(ownedPolicyImageCode), previousProtection, &ignored);
            }
        }
        void Profile(Detail::Function& function, std::size_t offset, std::size_t bytes, std::size_t stolen, std::uintptr_t detour)
        {
            function = {};
            function.rva = reinterpret_cast<std::uintptr_t>(ownedPolicyImageCode + offset) - image;
            function.bytes = bytes;
            function.stolen = stolen;
            function.detour = detour;
            function.captured = true;
            // Keep the NOP value byte-typed under the real /W4 /WX fixture build.
            std::fill_n(function.original.begin(), bytes, static_cast<unsigned char>(0x90));
            function.installed = function.original;
            auto*      code = ownedPolicyImageCode + offset;
            const auto relayOffset = 2304 + offset / 128 * 64;
            auto*      relay = ownedPolicyImageCode + relayOffset;
            std::memcpy(code, function.original.data(), bytes);
            code[0] = 0xe9;
            const auto displacement = static_cast<std::int32_t>(relay - (code + 5));
            std::memcpy(code + 1, &displacement, sizeof(displacement));
            for (std::size_t i = 5; i < stolen; ++i)
                code[i] = 0xcc;
            relay[0] = 0xff;
            relay[1] = 0x25;
            std::memset(relay + 2, 0, 4);
            std::memcpy(relay + 6, &detour, sizeof(detour));
            std::memcpy(function.installed.data(), code, bytes);
        }
        bool ConfigureOwnedPolicyImage(Detail::Present target)
        {
            if (!valid || !target || Detail::runtime.policySelector.load() || Detail::runtime.inFlight.load())
                return false;
            std::memset(ownedPolicyImageCode, 0x90, sizeof(ownedPolicyImageCode));
            Detail::runtime.image = image;
            Detail::runtime.dxgi = reinterpret_cast<std::uintptr_t>(dxgi);
            Detail::CacheDiagnosticModules();
            Detail::runtime.dxgi = reinterpret_cast<std::uintptr_t>(dxgi);
            Detail::runtime.prepared = Detail::runtime.committed = Detail::runtime.nativeDispatch = true;
            Detail::runtime.endpoint = false;
            Detail::runtime.preparation = {};
            Detail::runtime.preparation.preparationGate = static_cast<unsigned>(RenderEndpointPrepareGate::Complete);
            Detail::runtime.wrapperHook = reinterpret_cast<std::uintptr_t>(&Noop);
            Profile(Detail::runtime.wrapperCode, 0, 429, 7, Detail::runtime.wrapperHook);
            Profile(Detail::runtime.resizeCode, 512, 32, 5, reinterpret_cast<std::uintptr_t>(&ResizeNoop));
            Profile(Detail::runtime.notifyCode, 640, 32, 6, reinterpret_cast<std::uintptr_t>(&Noop));
            Profile(Detail::runtime.destroyCode, 768, 32, 6, reinterpret_cast<std::uintptr_t>(&Noop));
            Detail::runtime.resize = &ResizeNoop;
            Detail::runtime.notify = Detail::runtime.destroy = &Noop;
            Detail::runtime.presentCode.rva = reinterpret_cast<std::uintptr_t>(target) - Detail::runtime.dxgi;
            Detail::runtime.dispatchPatched = true;
            Detail::runtime.dispatchProtectionPending = Detail::runtime.dispatchFlushPending = Detail::runtime.dispatchUnsafeWrite = false;
            Detail::runtime.dispatchFinalizePending = Detail::runtime.dispatchReleasePending = false;
            Detail::runtime.dispatchPatch = {0xe8, 1, 2, 3, 4};
            std::memcpy(ownedPolicyImageCode + Detail::dispatchOffset, Detail::runtime.dispatchPatch.data(), Detail::runtime.dispatchPatch.size());
            Detail::runtime.dispatchRelay = ownedPolicyImageCode + 2048;
            Detail::runtime.relayCode.fill(0x90);
            std::memcpy(Detail::runtime.dispatchRelay, Detail::runtime.relayCode.data(), Detail::runtime.relayCode.size());
            chainTable[8] = reinterpret_cast<std::uintptr_t>(target);
            chain = reinterpret_cast<std::uintptr_t>(chainTable.data());
            Put(viewport.data(), 0, image + 0x4504060);
            Put(viewport.data(), 0x18, reinterpret_cast<std::uintptr_t>(owner.data()));
            Put(viewport.data(), 0x40, reinterpret_cast<std::uintptr_t>(viewport.data()));
            Put(viewport.data(), 0x4c, std::uint32_t{1280});
            Put(viewport.data(), 0x50, std::uint32_t{720});
            Put(viewport.data(), 0x60, reinterpret_cast<std::uintptr_t>(&chain));
            Put(viewport.data(), 0x1d0, std::uintptr_t{0});
            Put(owner.data(), 0x8a8, reinterpret_cast<std::uintptr_t>(device.data()));
            Put(device.data(), 0x38, reinterpret_cast<std::uintptr_t>(manager.data()));
            Put(manager.data(), 0, image + 0x44fbe88);
            Put(manager.data(), 0x28, reinterpret_cast<std::uintptr_t>(queue.data()));
            mta::game_de::StopFrameBoundaryAdmission();
            return FlushInstructionCache(GetCurrentProcess(), ownedPolicyImageCode, sizeof(ownedPolicyImageCode)) != FALSE;
        }
        IDXGISwapChain* Chain() { return reinterpret_cast<IDXGISwapChain*>(&chain); }
        void*           Caller() { return reinterpret_cast<void*>(image + 0x1fd5ec2); }
        void            RestoreViewportTarget(Detail::Present target)
        {
            chainTable[8] = reinterpret_cast<std::uintptr_t>(target);
            Put(viewport.data(), 0x60, reinterpret_cast<std::uintptr_t>(&chain));
            Put(owner.data(), 0x8a8, reinterpret_cast<std::uintptr_t>(device.data()));
        }
    };
}
