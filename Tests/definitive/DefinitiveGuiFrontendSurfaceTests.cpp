/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Exercise the real MTA menu through its owned composition HWND
 *****************************************************************************/
#include <windows.h>
#include <d3d12.h>
#include <d3d12sdklayers.h>
#include <dxgi1_4.h>
#include <wrl/client.h>
#include <algorithm>
#include <atomic>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
#include "../../Client/core/DefinitiveFrontend.h"
#include "../../Client/core/DefinitiveGuiSurface.h"
#include "GuiFrontendSurfaceDevice.h"

using Microsoft::WRL::ComPtr;
namespace Frontend = DefinitiveFrontend;
using namespace DefinitiveGuiSurface;
static_assert(Frontend::ApiVersion == 1 && Frontend::FieldCount == 32);

namespace
{
    unsigned checks = 0;
    void     Check(bool value, const char* why)
    {
        ++checks;
        if (!value)
            throw std::runtime_error(why);
        std::printf("Check %u: %s\n", checks, why);
    }
    void Hr(HRESULT result, const char* why)
    {
        Check(SUCCEEDED(result), why);
    }
    template <typename Function>
    Function Export(HMODULE module, const char* name)
    {
        const auto function = reinterpret_cast<Function>(GetProcAddress(module, name));
        Check(function != nullptr, name);
        return function;
    }
    void Pump()
    {
        MSG message{};
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
    }
    struct Parent
    {
        std::thread             thread;
        std::atomic<HWND>       window{nullptr};
        std::atomic<bool>       ready{false}, done{false};
        static LRESULT CALLBACK Procedure(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
        {
            if (message == WM_DESTROY)
            {
                PostQuitMessage(0);
                return 0;
            }
            return DefWindowProcW(window, message, wparam, lparam);
        }
        void Start()
        {
            // Match the foreign-thread parent relationship without attaching
            // to a game process or intercepting a foreign input procedure.
            thread = std::thread(
                [&]
                {
                    WNDCLASSW cls{};
                    cls.hInstance = GetModuleHandleW(nullptr);
                    cls.lpfnWndProc = &Procedure;
                    cls.lpszClassName = L"MtaFrontendSurfaceOwnedParent";
                    if (RegisterClassW(&cls))
                    {
                        RECT area{0, 0, 1280, 1024};
                        AdjustWindowRect(&area, WS_OVERLAPPEDWINDOW, FALSE);
                        window.store(CreateWindowExW(WS_EX_NOACTIVATE, cls.lpszClassName, L"Owned actual MTA menu", WS_OVERLAPPEDWINDOW, 40, 40,
                                                     area.right - area.left, area.bottom - area.top, nullptr, nullptr, cls.hInstance, nullptr));
                        if (window.load())
                            ShowWindow(window.load(), SW_SHOWNOACTIVATE);
                    }
                    ready.store(true);
                    if (window.load())
                    {
                        MSG message{};
                        while (GetMessageW(&message, nullptr, 0, 0) > 0)
                        {
                            TranslateMessage(&message);
                            DispatchMessageW(&message);
                        }
                    }
                    UnregisterClassW(cls.lpszClassName, cls.hInstance);
                    done.store(true);
                });
            const auto deadline = GetTickCount64() + 5000;
            while (!ready.load() && GetTickCount64() < deadline)
                Sleep(1);
            Check(ready.load() && window.load(), "independent disposable parent thread is ready");
        }
        ~Parent()
        {
            if (!thread.joinable())
                return;
            if (IsWindow(window.load()))
                PostMessageW(window.load(), WM_CLOSE, 0, 0);
            const auto deadline = GetTickCount64() + 5000;
            while (!done.load() && GetTickCount64() < deadline)
            {
                Pump();
                Sleep(1);
            }
            if (!done.load())
                std::terminate();
            thread.join();
        }
    };
    struct Menu
    {
        Frontend::Create   create = nullptr;
        Frontend::Pulse    pulse = nullptr;
        Frontend::Read     read = nullptr;
        Frontend::Shutdown shutdown = nullptr;
        Frontend::Release  release = nullptr;
        void*              handle = nullptr;
        Frontend::Snapshot state{};
        unsigned           starts = 0, pulses = 0, stops = 0, busyStops = 0;
        bool               failStart = false, throwStart = false, focused = false;
        std::int32_t       mouseX = 0, mouseY = 0;
        Session*           surface = nullptr;
        bool               reentrantRejected = false;
        bool               consumeModifierInput = false, consumedModifierReleaseVerified = false;
        unsigned           consumedModifierMessages = 0;
        bool               Read() { return handle && read(handle, state.data(), static_cast<std::uint32_t>(state.size())) == Frontend::Status::Ok; }
        static bool        Start(void* context, CGUI* gui)
        {
            auto& self = *static_cast<Menu*>(context);
            ++self.starts;
            self.handle = self.create(Frontend::ApiVersion, gui);
            if (!self.Read() || self.state[Frontend::Phase] != 2)
                return false;
            // A successful Core factory can still be followed by a failed host
            // start. Surface must retain and invoke its stop obligation.
            if (self.throwStart)
                throw std::runtime_error("deliberate host failure after actual frontend creation");
            return !self.failStart;
        }
        static bool Pulse(void* context, std::int32_t x, std::int32_t y, bool focused)
        {
            auto& self = *static_cast<Menu*>(context);
            ++self.pulses;
            self.mouseX = x;
            self.mouseY = y;
            self.focused = focused;
            self.reentrantRejected = self.surface->Shutdown() == Status::Busy;
            return self.pulse(self.handle, x, y, focused ? 1u : 0u) == Frontend::Status::Ok && self.Read();
        }
        static bool Stop(void* context)
        {
            auto& self = *static_cast<Menu*>(context);
            ++self.stops;
            if (self.busyStops)
            {
                --self.busyStops;
                return false;
            }
            if (self.handle)
            {
                if (self.shutdown(self.handle) != Frontend::Status::Ok || !self.Read() || self.release(self.handle) != Frontend::Status::Ok)
                    return false;
                self.handle = nullptr;
            }
            return true;
        }
        static bool Message(void* context, std::uint32_t message, std::uintptr_t key, std::intptr_t, bool* handled)
        {
            if (!handled)
                return false;
            auto& self = *static_cast<Menu*>(context);
            *handled = self.consumeModifierInput && (key == VK_SHIFT || key == VK_CONTROL) &&
                       (message == WM_KEYDOWN || message == WM_KEYUP || message == WM_SYSKEYDOWN || message == WM_SYSKEYUP);
            if (*handled)
                ++self.consumedModifierMessages;
            return true;
        }
        FrontendCallbacks Callbacks() { return {this, &Start, &Pulse, &Stop, &Message}; }
    };
    void Drain(Session& surface)
    {
        const auto deadline = GetTickCount64() + 5000;
        Status     status = Status::Busy;
        do
        {
            Pump();
            status = surface.Shutdown();
            if (status == Status::Busy)
                Sleep(1);
        } while (status == Status::Busy && GetTickCount64() < deadline);
        Check(status == Status::Ok && surface.Snapshot().cleanupComplete, "frontend surface completes owned nonblocking drain");
    }
    void Settle(Session& surface, Menu& menu)
    {
        const auto deadline = GetTickCount64() + 8000;
        do
        {
            Pump();
            const auto status = surface.Poll();
            Check(status == Status::Ok || status == Status::Busy, "real frontend composition remains running");
            const auto raster = surface.Snapshot();
            if (menu.Read() && menu.state[Frontend::MenuVisible] && !menu.state[Frontend::MenuFading] && raster.readbackComplete &&
                raster.presentSucceeded >= 3 && menu.pulses >= 24)
                return;
            Sleep(10);
        } while (GetTickCount64() < deadline);
        Check(false, "real menu fade and actual composition pixels settle before deadline");
    }
    void Click(HWND child, int x, int y)
    {
        // Deliberately batch move/down/up without a frame between them. The
        // existing menu selection must follow this same client hit position.
        SendMessageW(child, WM_MOUSEMOVE, 0, MAKELPARAM(x, y));
        SendMessageW(child, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(x, y));
        SendMessageW(child, WM_LBUTTONUP, 0, MAKELPARAM(x, y));
    }
    void Isolation()
    {
        Check(!GetModuleHandleW(L"game_de.dll") && !GetModuleHandleW(L"game_sa.dll") && !GetModuleHandleW(L"multiplayer_sa.dll") &&
                  !GetModuleHandleW(L"netc.dll") && !GetModuleHandleW(L"d3dx9_43.dll"),
              "real frontend has no game, multiplayer, networking or legacy D3DX9 module");
    }
}

int wmain(int argc, wchar_t** argv)
{
    setvbuf(stdout, nullptr, _IONBF, 0);
    setvbuf(stderr, nullptr, _IONBF, 0);
    SetProcessDPIAware();
    try
    {
        Check(argc == 3 && sizeof(void*) == 8, "owned frontend surface root and case supplied");
        const std::filesystem::path root(argv[1]);
        const std::wstring          mode(argv[2]);
        Check(root.is_absolute() && root.filename() == L"de-online", "explicit disposable adapter asset layout");
        Check(mode == L"normal" || mode == L"cancel" || mode == L"focus" || mode == L"gated" || mode == L"cpu-busy" || mode == L"partial-start" ||
                  mode == L"throw-start" || mode == L"startup-failure",
              "explicit frontend surface workload");
        std::printf("Frontend surface case=%ls\n", mode.c_str());
        const auto core = LoadLibraryExW((root / L"core.dll").c_str(), nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
        Check(core != nullptr, "load current certified normal Core");
        const auto wide = root.wstring();
        Export<void (*)(const wchar_t*, std::size_t)>(core, "SetMTADirectory")(wide.data(), wide.size());
        Check(Export<std::uint32_t (*)()>(core, "MTA_DEFrontendApiVersion")() == Frontend::ApiVersion, "real Core frontend API matches");
        Menu menu;
        menu.create = Export<Frontend::Create>(core, "MTA_CreateDEFrontendSession");
        menu.pulse = Export<Frontend::Pulse>(core, "MTA_PulseDEFrontendSession");
        menu.read = Export<Frontend::Read>(core, "MTA_ReadDEFrontendSession");
        menu.shutdown = Export<Frontend::Shutdown>(core, "MTA_ShutdownDEFrontendSession");
        menu.release = Export<Frontend::Release>(core, "MTA_ReleaseDEFrontendSession");
        menu.failStart = mode == L"partial-start";
        menu.throwStart = mode == L"throw-start";
        menu.busyStops = mode == L"cpu-busy" ? 1u : 0u;
        ComPtr<ID3D12Debug> debug;
        const bool          debugAvailable = SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug)));
        if (debugAvailable)
            debug->EnableDebugLayer();
        std::printf("D3D12 debug layer available=%u\n", debugAvailable ? 1u : 0u);
        ComPtr<IDXGIFactory4>   factory;
        ComPtr<IDXGIAdapter>    adapter;
        ComPtr<ID3D12Device>    device;
        ComPtr<ID3D12InfoQueue> info;
        Hr(CreateDXGIFactory1(IID_PPV_ARGS(&factory)), "create disposable system factory");
        Hr(factory->EnumWarpAdapter(IID_PPV_ARGS(&adapter)), "select Microsoft WARP");
        Hr(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device)), "create real owned WARP device");
        device.As(&info);
        Parent parent;
        parent.Start();
        ComPtr<ID3D12Fence> gate;
        if (mode == L"gated")
            Hr(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&gate)), "create deterministic owned queue gate");
        GuiFrontendSurfaceTest::DeviceProxy proxy(device.Get(), gate.Get());
        Session                             surface;
        menu.surface = &surface;
        Check(surface.Prepare((root / L"cgui.dll").c_str(), root.c_str()) == Status::Ok, "prepare production surface with normal GUI and actual assets");
        auto invalid = menu.Callbacks();
        invalid.stop = nullptr;
        Check(surface.EnableFrontend(invalid) == Status::InvalidArgument && !surface.InteractionSnapshot().enabled,
              "incomplete frontend callbacks cannot admit input");
        bool        foreignRejected = false;
        std::thread foreign([&] { foreignRejected = surface.EnableFrontend(menu.Callbacks()) == Status::WrongThread; });
        foreign.join();
        Check(foreignRejected && menu.starts == 0, "foreign owner cannot enable or create real frontend");
        Check(surface.EnableFrontend(menu.Callbacks()) == Status::Ok && surface.EnableInteraction() == Status::InvalidArgument,
              "explicit frontend content is separate from the interactive widget fixture");
        device->AddRef();
        Check(surface.AttachDeviceWindow(gate ? static_cast<ID3D12Device*>(&proxy) : device.Get(), parent.window.load(), true) == Status::Ok,
              "transfer only owned WARP lease and independent parent");
        const bool startupFailure = menu.failStart || menu.throwStart || mode == L"startup-failure";
        Check(surface.Start() == (startupFailure ? Status::Failed : Status::Ok), "real frontend start reports the selected actual lifecycle outcome");
        Check(menu.starts == 1 && menu.handle, "Core startup publishes retained frontend ownership");
        if (!startupFailure)
        {
            const auto child = GetWindow(parent.window.load(), GW_CHILD);
            Check(child && GetWindowThreadProcessId(child, nullptr) == GetCurrentThreadId(), "actual composition child belongs to GUI worker");
            SetFocus(child);
            Check(GetFocus() == child && surface.InteractionSnapshot().focused, "owned child acquires exact Win32 input focus");
            if (mode != L"gated")
            {
                Settle(surface, menu);
                const auto pixels = surface.Snapshot();
                Check(pixels.readbackPixels > 10000 && pixels.premultipliedPixelsVerified && !pixels.readbackRedVerified && !pixels.transparentPixelVerified,
                      "real menu produces substantial premultiplied pixels without fixture color claims");
                Check(menu.reentrantRejected && menu.focused, "frontend callback cannot reenter destructive Surface operations");
            }
            if (mode == L"normal")
            {
                // A modal frontend can start between a modifier's physical
                // press and release. Exercise the real Surface HWND so this
                // fails if its consumed-message branch bypasses the ledger.
                for (const auto key : {WPARAM{VK_SHIFT}, WPARAM{VK_CONTROL}})
                {
                    const auto down = key == VK_SHIFT ? WM_KEYDOWN : WM_SYSKEYDOWN;
                    const auto up = key == VK_SHIFT ? WM_KEYUP : WM_SYSKEYUP;
                    const auto before = surface.InteractionSnapshot();
                    menu.consumeModifierInput = false;
                    SendMessageW(child, down, key, 0);
                    const auto held = surface.InteractionSnapshot();
                    Check(held.pressedKeys == 1 && held.events == before.events + 1,
                          "actual owned HWND records a modifier press before modal frontend capture");
                    menu.consumeModifierInput = true;
                    SendMessageW(child, up, key, 0);
                    const auto released = surface.InteractionSnapshot();
                    Check(!released.pressedKeys && released.events == held.events + 1 && !released.inputFailed,
                          "frontend-consumed key-up still releases the exact existing GUI hold through production Surface dispatch");
                    SendMessageW(child, up, key, 0);
                    Check(!surface.InteractionSnapshot().pressedKeys && surface.InteractionSnapshot().events == released.events,
                          "orphan consumed key-up injects no additional GUI event");
                    SendMessageW(child, down, key, 0);
                    SendMessageW(child, up, key, 0);
                    Check(!surface.InteractionSnapshot().pressedKeys && surface.InteractionSnapshot().events == released.events,
                          "a new frontend-consumed modifier press cannot create a GUI hold or an unmatched release");
                }
                Check(menu.consumedModifierMessages == 8, "the actual Surface message callback consumed both normal and system modifier messages");
                menu.consumeModifierInput = false;
                menu.consumedModifierReleaseVerified = true;
                Click(child, 230, 760);
                Check(menu.Read() && menu.state[Frontend::AboutClicks] == 1 && menu.state[Frontend::CreditsVisible],
                      "actual HWND messages invoke real menu About and Credits");
                Check(menu.mouseX == 230 && menu.mouseY == 760, "frame callbacks receive translator client mouse coordinates");
                Click(child, 840, 644);
                Check(menu.Read() && !menu.state[Frontend::CreditsVisible], "actual HWND messages close real Credits with its existing OK callback");
                Click(child, 230, 804);
                Check(menu.Read() && menu.state[Frontend::CloseRequested], "actual MTA Quit requests host close without terminating owned process");
            }
            else
            {
                if (mode == L"cancel")
                {
                    RECT rectangle{0, 0, 1024, 800};
                    AdjustWindowRect(&rectangle, WS_OVERLAPPEDWINDOW, FALSE);
                    Check(SetWindowPos(parent.window.load(), nullptr, 0, 0, rectangle.right - rectangle.left, rectangle.bottom - rectangle.top,
                                       SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE) != FALSE,
                          "resize only owned real-menu parent");
                    const auto deadline = GetTickCount64() + 5000;
                    while (!surface.Snapshot().resizeCount && GetTickCount64() < deadline)
                    {
                        const auto status = surface.Poll();
                        Check(status == Status::Ok || status == Status::Busy, "frontend resize does not require a fixture image");
                        Sleep(10);
                    }
                    Check(surface.Snapshot().resizeCount == 1, "actual real-menu Surface resize retires and recreates buffers");
                }
                SendMessageW(child, WM_KEYDOWN, VK_SHIFT, 0);
                SendMessageW(child, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(230, 760));
                Check(surface.InteractionSnapshot().pressedKeys && surface.InteractionSnapshot().pressedButtons,
                      "hold actual translator input before interrupted cleanup");
                if (mode == L"focus")
                {
                    SetFocus(nullptr);
                    const auto deadline = GetTickCount64() + 1000;
                    while (menu.focused && GetTickCount64() < deadline)
                    {
                        surface.Poll();
                        Sleep(10);
                    }
                    const auto input = surface.InteractionSnapshot();
                    Check(!input.focused && !input.pressedKeys && !input.pressedButtons && input.syntheticReleases >= 2 && !menu.focused,
                          "focus loss releases holds and reaches explicit frontend frame policy");
                    Check(menu.Read() && !menu.state[Frontend::AboutClicks], "focus-loss cancellation never activates a held About action");
                }
                if (mode == L"cancel")
                {
                    SendMessageW(child, WM_KEYDOWN, VK_ESCAPE, 0);
                    Check(surface.InteractionSnapshot().closeRequested && surface.Snapshot().stopRequested, "Escape requests owned frontend cancellation");
                }
            }
        }
        const auto beforeStop = GetTickCount64();
        const auto firstStop = surface.Shutdown();
        Check(GetTickCount64() - beforeStop < 1000, "frontend CPU stop never blocks for a GPU fence");
        const auto input = surface.InteractionSnapshot();
        Check(input.inputDetached && !input.pressedKeys && !input.pressedButtons && !input.focused,
              "input detaches before CPU or GPU ownership can remain pending");
        if (mode == L"cpu-busy")
        {
            Check(firstStop == Status::Busy && menu.handle && surface.Snapshot().sessionRetained && !surface.Snapshot().cleanupComplete,
                  "Busy frontend stop retains actual callbacks, context and borrowed CGUI");
            Check(menu.Read() && menu.state[Frontend::Phase] == 2, "pending CPU owner still has a valid real frontend handle");
        }
        else
        {
            Check((firstStop == Status::Ok || firstStop == Status::Busy) && !menu.handle && menu.state[Frontend::CleanupComplete],
                  "real frontend CPU services stop before surface session release");
        }
        if (mode == L"gated")
        {
            const auto retained = surface.Snapshot();
            Check(firstStop == Status::Busy && retained.moduleRetained && retained.sessionRetained && retained.queueRetained && retained.surfaceRetained &&
                      !retained.cleanupComplete && !menu.handle,
                  "gated GPU retains Surface owners after real frontend CPU teardown");
            Hr(gate->Signal(1), "release only disposable owned GPU gate");
        }
        Drain(surface);
        Check(menu.stops >= 1 && !menu.handle && menu.state[Frontend::CleanupComplete] && !menu.state[Frontend::FullMtaReady] &&
                  !menu.state[Frontend::GameplayReady] && !menu.state[Frontend::NetworkStarted],
              "completed cleanup never grants full/gameplay/network readiness");
        Check(!GetModuleHandleW(L"cgui.dll") && !GetModuleHandleW(L"xmll.dll"), "real GUI and XML modules retire after owned frontend and GPU");
        Isolation();
        if (info)
        {
            unsigned errors = 0;
            for (UINT64 index = 0; index < info->GetNumStoredMessages(); ++index)
            {
                SIZE_T size = 0;
                info->GetMessage(index, nullptr, &size);
                std::vector<BYTE> storage(size);
                auto*             message = reinterpret_cast<D3D12_MESSAGE*>(storage.data());
                if (SUCCEEDED(info->GetMessage(index, message, &size)) && message->Severity <= D3D12_MESSAGE_SEVERITY_ERROR)
                {
                    ++errors;
                    std::fprintf(stderr, "D3D12 debug error: %s\n", message->pDescription);
                }
            }
            Check(errors == 0, "real menu composition/cleanup produces no D3D12 debug errors");
        }
        Check(FreeLibrary(core) != FALSE && !GetModuleHandleW(L"core.dll"), "normal Core unload follows complete CPU/GPU ownership retirement");
        std::ofstream result(root / L"frontend-surface-result.json");
        result << "{\"schema\":\"mta_owned_frontend_surface_v1\",\"success\":true,\"checks\":" << checks + 1 << ",\"starts\":" << menu.starts
               << ",\"pulses\":" << menu.pulses << ",\"stops\":" << menu.stops << ",\"aboutClicks\":" << menu.state[Frontend::AboutClicks]
               << ",\"consumedModifierReleaseVerified\":" << (menu.consumedModifierReleaseVerified ? "true" : "false")
               << ",\"cleanupComplete\":true,\"gameLaunched\":false,\"fullMtaReady\":false}";
        Check(result.good(), "save actual owned frontend composition lifecycle evidence");
        std::printf("Passed %u actual MTA frontend surface host checks\n", checks);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "Owned frontend surface failed: %s\n", error.what());
        // This disposable process retains modules/owners on failed assertions;
        // it never frees live callbacks merely to claim successful cleanup.
        return 1;
    }
}
