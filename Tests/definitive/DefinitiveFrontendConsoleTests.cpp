/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Exercise the optional real MTA console through its owned composition HWND
 *****************************************************************************/
#include <windows.h>
#include <d3d12.h>
#include <d3d12sdklayers.h>
#include <dxgi1_4.h>
#include <wrl/client.h>
#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
#include "../../Client/core/DefinitiveFrontend.h"
#include "../../Client/core/DefinitiveFrontendConsole.h"
#include "../../Client/core/DefinitiveGuiSurface.h"
#include "GuiFrontendSurfaceDevice.h"

using Microsoft::WRL::ComPtr;
namespace Frontend = DefinitiveFrontend;
namespace Console = DefinitiveFrontendConsole;
using namespace DefinitiveGuiSurface;
static_assert(Frontend::ApiVersion == 1 && Frontend::FieldCount == 32);
static_assert(Console::ApiVersion == 1 && Console::FieldCount == 32);

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
                    cls.lpszClassName = L"MtaFrontendConsoleOwnedParent";
                    if (RegisterClassW(&cls))
                    {
                        RECT area{0, 0, 1280, 1024};
                        AdjustWindowRect(&area, WS_OVERLAPPEDWINDOW, FALSE);
                        window.store(CreateWindowExW(WS_EX_NOACTIVATE, cls.lpszClassName, L"Owned actual MTA console", WS_OVERLAPPEDWINDOW, 40, 40,
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
        using EnableConsole = Frontend::Status (*)(void*, std::uint32_t);
        using ReadConsole = Frontend::Status (*)(void*, std::uint32_t*, std::uint32_t);
        using Visible = Frontend::Status (*)(void*, std::uint32_t);
        using Message = Frontend::Status (*)(void*, std::uint32_t, std::uintptr_t, std::intptr_t, std::uint32_t*);
        using ReadText = Frontend::Status (*)(void*, std::uint32_t, char*, std::uint32_t, std::uint32_t*);
        EnableConsole      enableConsole = nullptr;
        ReadConsole        readConsole = nullptr;
        Visible            consoleVisible = nullptr, menuVisible = nullptr;
        Message            processMessage = nullptr;
        ReadText           readText = nullptr;
        Console::Snapshot  initialConsole{};
        Console::Snapshot  finalConsole{};
        unsigned           messageCalls = 0, lifetimeMessages = 0;
        bool               failMessage = false, throwMessage = false, consumeLifetime = false;
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
        bool               Read() { return handle && read(handle, state.data(), static_cast<std::uint32_t>(state.size())) == Frontend::Status::Ok; }
        static bool        Start(void* context, CGUI* gui)
        {
            auto& self = *static_cast<Menu*>(context);
            ++self.starts;
            self.handle = self.create(Frontend::ApiVersion, gui);
            if (!self.Read() || self.state[Frontend::Phase] != 2)
                return false;
            if (self.readConsole(self.handle, self.initialConsole.data(), Console::FieldCount) != Frontend::Status::Ok ||
                self.enableConsole(self.handle, Console::ApiVersion) != Frontend::Status::Ok)
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
                if (self.shutdown(self.handle) != Frontend::Status::Ok || !self.Read() ||
                    self.readConsole(self.handle, self.finalConsole.data(), Console::FieldCount) != Frontend::Status::Ok ||
                    self.release(self.handle) != Frontend::Status::Ok)
                    return false;
                self.handle = nullptr;
            }
            return true;
        }
        static bool Process(void* context, UINT message, WPARAM wparam, LPARAM lparam, bool* handled)
        {
            auto& self = *static_cast<Menu*>(context);
            ++self.messageCalls;
            if (message == WM_NCDESTROY)
            {
                ++self.lifetimeMessages;
                *handled = self.consumeLifetime;
                return true;
            }
            if (message == WM_KEYDOWN && wparam == VK_F8)
            {
                if (self.throwMessage)
                    throw std::runtime_error("deliberate optional console shortcut failure");
                if (self.failMessage)
                    return false;
            }
            std::uint32_t consumed = 0;
            const auto    result = self.processMessage(self.handle, message, wparam, lparam, &consumed);
            *handled = consumed != 0;
            return result == Frontend::Status::Ok;
        }
        FrontendCallbacks Callbacks()
        {
            FrontendCallbacks callbacks{this, &Start, &Pulse, &Stop};
            callbacks.message = &Process;
            return callbacks;
        }
        Console::Snapshot ConsoleState()
        {
            Console::Snapshot result{};
            Check(readConsole(handle, result.data(), Console::FieldCount) == Frontend::Status::Ok, "read real optional console owner state");
            Check(result[Console::Version] == Console::ApiVersion && !result[Console::HistoryPersistent] && !result[Console::NetworkStarted] &&
                      !result[Console::GameplayReady] && !result[Console::FullMtaReady] && !result[Console::Reserved31],
                  "console metadata keeps history, network and game readiness gates explicit");
            return result;
        }
        std::string Text(std::uint32_t field)
        {
            std::uint32_t required = 0;
            Check(readText(handle, field, nullptr, 0, &required) == Frontend::Status::Ok && required > 0 && required <= 8192,
                  "query bounded actual console widget UTF8 storage");
            std::vector<char> storage(required, '\x7f');
            Check(readText(handle, field, storage.data(), required, &required) == Frontend::Status::Ok && storage.back() == '\0',
                  "read copied widget UTF8 with a bounded NUL terminator");
            return storage.data();
        }
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
    void Key(HWND child, WPARAM key)
    {
        SendMessageW(child, WM_KEYDOWN, key, 0);
        SendMessageW(child, WM_KEYUP, key, 0);
    }
    void Type(HWND child, const std::wstring& text)
    {
        for (const auto unit : text)
            SendMessageW(child, WM_CHAR, static_cast<WPARAM>(unit), 0);
    }
    void ClearEdit(HWND child)
    {
        // Use the existing edit selection and Delete behavior, rather than a
        // fixture-only text setter that could bypass real keyboard handling.
        Key(child, VK_HOME);
        SendMessageW(child, WM_KEYDOWN, VK_SHIFT, 0);
        Key(child, VK_END);
        SendMessageW(child, WM_KEYUP, VK_SHIFT, 0);
        Key(child, VK_DELETE);
    }
    void ConsoleGuards(Menu& menu)
    {
        Check(!menu.initialConsole[Console::ConsoleCreated] && menu.initialConsole[Console::Phase] == 0,
              "existing frontend startup does not automatically create the optional console");
        const auto initial = menu.ConsoleState();
        Check(initial[Console::ConsoleCreated] && !initial[Console::ConsoleVisible] && initial[Console::CommandPolicyActive] &&
                  initial[Console::CommandsRegistered] == 2 && initial[Console::ConfigurationReadOnly] && initial[Console::MaxInputCharacters] > 0,
              "explicit enable owns actual console with only existing Help and Clear handlers");
        Check(menu.enableConsole(menu.handle, Console::ApiVersion + 1) == Frontend::Status::InvalidArgument &&
                  menu.consoleVisible(menu.handle, 2) == Frontend::Status::InvalidArgument &&
                  menu.menuVisible(menu.handle, 2) == Frontend::Status::InvalidArgument,
              "wrong version and nonboolean visibility never change real console admission");
        Console::Snapshot untouched;
        untouched.fill(0x12345678);
        Check(menu.readConsole(menu.handle, untouched.data(), Console::FieldCount - 1) == Frontend::Status::InvalidArgument &&
                  std::all_of(untouched.begin(), untouched.end(), [](auto value) { return value == 0x12345678; }),
              "wrong console snapshot count preserves caller storage");
        bool        foreignRejected = false;
        std::thread foreign(
            [&]
            {
                std::uint32_t required = 123, handled = 123;
                char          storage[8] = "guard";
                foreignRejected = menu.consoleVisible(menu.handle, 1) == Frontend::Status::WrongThread &&
                                  menu.readConsole(menu.handle, untouched.data(), Console::FieldCount) == Frontend::Status::WrongThread &&
                                  menu.readText(menu.handle, 0, storage, sizeof(storage), &required) == Frontend::Status::WrongThread &&
                                  menu.processMessage(menu.handle, WM_KEYDOWN, VK_F8, 0, &handled) == Frontend::Status::WrongThread && required == 123 &&
                                  handled == 123 && std::strcmp(storage, "guard") == 0;
            });
        foreign.join();
        Check(foreignRejected && !menu.ConsoleState()[Console::ConsoleVisible], "foreign observers cannot dispatch console input, copy text or show widgets");
        std::uint32_t required = 123;
        char          guard[8] = "guard";
        Check(menu.readText(menu.handle, 99, guard, sizeof(guard), &required) == Frontend::Status::InvalidArgument && std::strcmp(guard, "guard") == 0,
              "unknown copied-text field never accesses another Core object");
    }
    void Ownership(Menu& menu)
    {
        Check(menu.consoleVisible(menu.handle, 1) == Frontend::Status::Ok, "show real optional console explicitly");
        auto state = menu.ConsoleState();
        Check(state[Console::MenuInputRequested] && state[Console::ConsoleInputRequested] && state[Console::AggregateInputRequested],
              "menu and console independently contribute to aggregate input ownership");
        Check(menu.menuVisible(menu.handle, 0) == Frontend::Status::Ok && menu.Read() && menu.state[Frontend::InputRequested] &&
                  menu.ConsoleState()[Console::AggregateInputRequested],
              "hiding menu preserves the console's real input request");
        Check(menu.consoleVisible(menu.handle, 0) == Frontend::Status::Ok && menu.Read() && !menu.state[Frontend::InputRequested],
              "hiding both views releases aggregate frontend input ownership");
        Check(menu.menuVisible(menu.handle, 1) == Frontend::Status::Ok && menu.consoleVisible(menu.handle, 1) == Frontend::Status::Ok,
              "restore both real views before single-owner release");
        Check(menu.consoleVisible(menu.handle, 0) == Frontend::Status::Ok && menu.Read() && menu.state[Frontend::InputRequested] &&
                  menu.ConsoleState()[Console::MenuInputRequested],
              "closing console preserves the visible menu input request");
    }
    void Commands(HWND child, Menu& menu)
    {
        Key(child, VK_F8);
        Check(menu.ConsoleState()[Console::ConsoleVisible] && menu.ConsoleState()[Console::InputActive],
              "actual Win32 F8 activates the existing console edit widget");
        Key(child, VK_UP);
        Check(menu.Text(0).empty(), "in-memory history does not load the staged classic history log");
        const auto revision = menu.ConsoleState()[Console::CVarRevision];
        Type(child, L"help");
        Check(menu.Text(0) == "help", "real Win32 character messages enter the actual console text widget");
        Key(child, VK_RETURN);
        auto output = menu.Text(1);
        auto state = menu.ConsoleState();
        Check(output.find("COMMAND HELP") != std::string::npos && output.find("help") != std::string::npos && output.find("clear") != std::string::npos &&
                  state[Console::CommandsExecuted] == 1 && state[Console::TextAccepted] == 1 && state[Console::LastCommand] == 1 && menu.Text(0).empty(),
              "real edit Enter invokes the existing parser and Help handler output");
        Type(child, L"clear");
        Key(child, VK_RETURN);
        output = menu.Text(1);
        Check(output.find_first_not_of("\r\n") == std::string::npos && menu.ConsoleState()[Console::LastCommand] == 2,
              "existing Clear handler clears the actual output memo");
        Type(child, L"help parser argument");
        Key(child, VK_RETURN);
        Check(menu.Text(1).find("COMMAND HELP") != std::string::npos && menu.ConsoleState()[Console::CommandsExecuted] == 3,
              "existing command parser separates handler name from its argument string");
        const std::wstring refused[] = {L"unknown_console_fixture", L"maxfps 1", L"nick OwnedConsoleMutation", L"connect 127.0.0.1 22003", L"bind F1 help"};
        for (const auto& command : refused)
        {
            Type(child, command);
            Key(child, VK_RETURN);
        }
        state = menu.ConsoleState();
        Check(state[Console::CommandsRejected] == 5 && state[Console::CommandsExecuted] == 3 && state[Console::LastCommand] == 3 &&
                  state[Console::CVarRevision] == revision && !state[Console::NetworkStarted],
              "unknown, CVar, nickname, connect and bind commands are refused before mutation or network startup");
        Check(state[Console::HistoryEntries] >= 5 && !state[Console::HistoryPersistent], "accepted command text stays in bounded in-memory history");
        Key(child, VK_UP);
        Check(menu.Text(0) == "bind F1 help", "actual Up key recalls latest in-memory entry");
        Key(child, VK_DOWN);
        Check(menu.Text(0).empty(), "actual Down key restores the current empty draft");
        Type(child, L"h");
        Key(child, VK_TAB);
        SendMessageW(child, WM_CHAR, '\t', 0);
        state = menu.ConsoleState();
        Check(menu.Text(0) == "help parser argument" && state[Console::AutoCompleteUses] > 0 && state[Console::TextAccepted] == 8 &&
                  state[Console::CommandsExecuted] == 3,
              "actual Tab completes history without the editbox Tab-as-Enter side effect");
        ClearEdit(child);
        Check(menu.Text(0).empty(), "existing keyboard selection/Delete clears draft without submitting it");
        Type(child, L"caf\u00e9 \u0416 \U0001f642");
        Check(menu.Text(0) == u8"caf\u00e9 \u0416 \U0001f642", "real edit preserves BMP and surrogate-pair text as UTF8");
        std::uint32_t required = 0;
        char          smallBuffer[2] = {'x', 'y'};
        Check(menu.readText(menu.handle, 0, smallBuffer, sizeof(smallBuffer), &required) != Frontend::Status::Ok && required > sizeof(smallBuffer) &&
                  smallBuffer[0] == 'x' && smallBuffer[1] == 'y',
              "insufficient text capacity reports required storage without partial UTF8 or overwrite");
        // The real close button is an 8-pixel skin child, aligned against the
        // console's resolution-scaled 550x360 window; invoke its actual hitbox.
        Click(child, 934, 396);
        Check(!menu.ConsoleState()[Console::ConsoleVisible] && menu.Read() && menu.state[Frontend::InputRequested],
              "actual console close callback hides console while menu retains input ownership");
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
                  mode == L"throw-start" || mode == L"startup-failure" || mode == L"message-failure" || mode == L"message-throw" || mode == L"child-destroy",
              "explicit frontend surface workload");
        std::printf("Frontend console case=%ls\n", mode.c_str());
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
        Check(Export<std::uint32_t (*)()>(core, "MTA_DEFrontendConsoleApiVersion")() == Console::ApiVersion,
              "optional console has a distinct version without changing the existing frontend ABI");
        menu.enableConsole = Export<Menu::EnableConsole>(core, "MTA_EnableDEFrontendConsole");
        menu.readConsole = Export<Menu::ReadConsole>(core, "MTA_ReadDEFrontendConsole");
        menu.consoleVisible = Export<Menu::Visible>(core, "MTA_SetDEFrontendConsoleVisible");
        menu.menuVisible = Export<Menu::Visible>(core, "MTA_SetDEFrontendMenuVisible");
        menu.processMessage = Export<Menu::Message>(core, "MTA_ProcessDEFrontendConsoleMessage");
        menu.readText = Export<Menu::ReadText>(core, "MTA_ReadDEFrontendConsoleText");
        menu.failStart = mode == L"partial-start";
        menu.throwStart = mode == L"throw-start";
        menu.busyStops = mode == L"cpu-busy" ? 1u : 0u;
        menu.failMessage = mode == L"message-failure";
        menu.throwMessage = mode == L"message-throw";
        menu.consumeLifetime = mode == L"child-destroy";
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
            ConsoleGuards(menu);
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
                Ownership(menu);
                Commands(child, menu);
                Settle(surface, menu);
                Click(child, 230, 760);
                Check(menu.Read() && menu.state[Frontend::AboutClicks] == 1 && menu.state[Frontend::CreditsVisible],
                      "actual HWND messages invoke real menu About and Credits");
                Check(menu.mouseX == 230 && menu.mouseY == 760, "frame callbacks receive translator client mouse coordinates");
                Click(child, 840, 644);
                Check(menu.Read() && !menu.state[Frontend::CreditsVisible], "actual HWND messages close real Credits with its existing OK callback");
                Click(child, 230, 804);
                Check(menu.Read() && menu.state[Frontend::CloseRequested], "actual MTA Quit requests host close without terminating owned process");
            }
            else if (mode == L"message-failure" || mode == L"message-throw")
            {
                Key(child, VK_F8);
                const auto failed = surface.InteractionSnapshot();
                Check(failed.inputFailed && surface.Snapshot().stopRequested && !menu.ConsoleState()[Console::ConsoleVisible],
                      "failed or throwing optional message callback stops admission and preserves normal teardown");
            }
            else if (mode == L"child-destroy")
            {
                Check(DestroyWindow(child) != FALSE && !IsWindow(child) && !surface.Snapshot().childVisible && menu.lifetimeMessages == 0,
                      "optional input policy cannot consume NCDESTROY or bypass real child lifetime bookkeeping");
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
                std::string textBeforeFocus;
                if (mode == L"focus")
                {
                    Check(menu.consoleVisible(menu.handle, 1) == Frontend::Status::Ok, "show actual console before interrupted Unicode input");
                    Type(child, L"focus\u00e9");
                    textBeforeFocus = menu.Text(0);
                    SendMessageW(child, WM_CHAR, 0xd83d, 0);
                }
                SendMessageW(child, WM_KEYDOWN, VK_SHIFT, 0);
                SendMessageW(child, WM_LBUTTONDOWN, MK_LBUTTON, mode == L"focus" ? MAKELPARAM(450, 790) : MAKELPARAM(230, 760));
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
                    SendMessageW(child, WM_CHAR, 'x', 0);
                    Check(menu.Text(0) == textBeforeFocus, "text queued while the owned child lacks focus cannot edit the console");
                    SetFocus(child);
                    Check(menu.consoleVisible(menu.handle, 1) == Frontend::Status::Ok, "same owner reactivates real console edit after focus return");
                    Key(child, VK_END);
                    SendMessageW(child, WM_CHAR, 0xde42, 0);
                    Check(menu.Text(0) == textBeforeFocus + "\xef\xbf\xbd",
                          "focus cancellation drops pending surrogate state instead of pairing text across focus owners");
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
        if (mode == L"cpu-busy" || mode == L"gated")
        {
            const auto child = GetWindow(parent.window.load(), GW_CHILD);
            const auto callbacks = menu.messageCalls;
            Check(child && IsWindow(child), "pending owned drain still has its disposable child window");
            SetFocus(child);
            Key(child, VK_F8);
            Type(child, L"help");
            Key(child, VK_RETURN);
            Check(menu.messageCalls == callbacks && surface.InteractionSnapshot().inputDetached,
                  "detached child cannot reenter hotkeys or console callbacks while CPU/GPU owners remain retained");
            if (menu.handle)
                Check(!menu.ConsoleState()[Console::ConsoleVisible] && !menu.ConsoleState()[Console::TextAccepted],
                      "retained console owner receives no text or commands after input detachment");
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
        if (mode != L"startup-failure")
            Check(menu.finalConsole[Console::Phase] == 4 && menu.finalConsole[Console::CleanupComplete] && !menu.finalConsole[Console::ConsoleRetained] &&
                      !menu.finalConsole[Console::CommandsRetained] && !menu.finalConsole[Console::AggregateInputRequested] &&
                      !menu.finalConsole[Console::ConsoleVisible] && !menu.finalConsole[Console::HistoryPersistent] &&
                      !menu.finalConsole[Console::NetworkStarted],
                  "real console widgets, commands and aggregate input retire before GUI/Core unload");
        if (mode == L"normal")
            Check(menu.finalConsole[Console::CommandsExecuted] == 3 && menu.finalConsole[Console::CommandsRejected] == 5 &&
                      menu.finalConsole[Console::TextAccepted] == 8 && menu.finalConsole[Console::HistoryEntries] >= 5,
                  "teardown retains actual parser/history evidence without retaining callbacks or command owners");
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
        std::ofstream result(root / L"frontend-console-result.json");
        result << "{\"schema\":\"mta_owned_frontend_console_v1\",\"success\":true,\"checks\":" << checks + 1 << ",\"starts\":" << menu.starts
               << ",\"pulses\":" << menu.pulses << ",\"stops\":" << menu.stops << ",\"commandsExecuted\":" << menu.finalConsole[Console::CommandsExecuted]
               << ",\"commandsRejected\":" << menu.finalConsole[Console::CommandsRejected] << ",\"textAccepted\":" << menu.finalConsole[Console::TextAccepted]
               << ",\"cleanupComplete\":true,\"gameLaunched\":false,\"fullMtaReady\":false}";
        Check(result.good(), "save actual owned frontend composition lifecycle evidence");
        std::printf("Passed %u actual MTA frontend console host checks\n", checks);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "Owned frontend console failed: %s\n", error.what());
        // This disposable process retains modules/owners on failed assertions;
        // it never frees live callbacks merely to claim successful cleanup.
        return 1;
    }
}
