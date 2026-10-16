/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Verify completed Core/backend ownership with the real borrowed MTA frontend
 *****************************************************************************/
#include <windows.h>
#include <d3d12.h>
#include <d3d12sdklayers.h>
#include <dxgi1_4.h>
#include <wrl/client.h>
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
#include <SharedUtil.Defines.h>
#include <SString.h>
#include <gui/CGUI.h>
#include <gui/CGUID3D12.h>
#include "../../Client/core/DefinitiveFrontend.h"
#include "../../Client/core/DefinitiveFrontendStartup.h"
#include "../../Client/core/DefinitiveFrontendConsole.h"
#include "../../Client/core/DefinitiveFrontendBindings.h"

using Microsoft::WRL::ComPtr;
namespace Frontend = DefinitiveFrontend;
namespace Startup = DefinitiveFrontendStartup;
namespace Console = DefinitiveFrontendConsole;
namespace Bindings = DefinitiveFrontendBindings;
using mta::gui::GuiD3D12Status;
static_assert(Frontend::ApiVersion == 1 && Frontend::FieldCount == 32);
static_assert(Startup::ApiVersion == 1 && Startup::FieldCount == 32);

namespace
{
    unsigned checks = 0;
    unsigned cancelCalls = 0, publishCalls = 0;
    bool     CancelView()
    {
        ++cancelCalls;
        return false;
    }
    void PublishView(const std::uint32_t*, std::uint32_t)
    {
        ++publishCalls;
    }
    void Check(bool condition, const char* why)
    {
        ++checks;
        if (!condition)
            throw std::runtime_error(why);
        std::printf("Check %u: %s\n", checks, why);
    }
    void Hr(HRESULT result, const char* why)
    {
        if (FAILED(result))
            std::fprintf(stderr, "%s HRESULT=%08lx\n", why, static_cast<unsigned long>(result));
        Check(SUCCEEDED(result), why);
    }
    template <typename Function>
    Function Export(HMODULE module, const char* name)
    {
        const auto function = reinterpret_cast<Function>(GetProcAddress(module, name));
        Check(function != nullptr, name);
        return function;
    }
    struct Owned
    {
        static constexpr UINT              Width = 1280, Height = 1024;
        ComPtr<ID3D12Device>               device;
        ComPtr<ID3D12CommandQueue>         queue;
        ComPtr<ID3D12CommandAllocator>     allocator;
        ComPtr<ID3D12GraphicsCommandList>  list;
        ComPtr<ID3D12Resource>             target, readback;
        ComPtr<ID3D12Fence>                fence, gate;
        ComPtr<ID3D12DescriptorHeap>       rtv;
        ComPtr<ID3D12InfoQueue>            info;
        D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
        HANDLE                             event = nullptr;
        UINT64                             nextFence = 1;
        bool                               blocked = false;
        HMODULE                            core = nullptr, cgui = nullptr;
        mta::gui::IGuiD3D12Session*        renderer = nullptr;
        CGUI*                              gui = nullptr;
        void*                              frontend = nullptr;
        void*                              startup = nullptr;
        Startup::Begin                     beginStartup = nullptr;
        Startup::Pulse                     pulseStartup = nullptr;
        Startup::BindInput                 bindStartup = nullptr;
        Startup::Read                      readStartup = nullptr;
        Startup::Shutdown                  shutdownStartup = nullptr;
        Startup::Release                   releaseStartup = nullptr;
        Startup::RunView                   runView = nullptr;
        Frontend::Create                   create = nullptr;
        Frontend::Pulse                    pulse = nullptr;
        Frontend::Read                     read = nullptr;
        Frontend::Shutdown                 shutdown = nullptr;
        Frontend::Release                  release = nullptr;

        ~Owned()
        {
            if (blocked && gate)
                gate->Signal(UINT64_MAX - 1);
            if (frontend && release && release(frontend) == Frontend::Status::Ok)
                frontend = nullptr;
            if (!frontend && startup && releaseStartup && releaseStartup(startup) == Frontend::Status::Ok)
                startup = nullptr;
            if (!frontend && renderer)
            {
                for (unsigned retry = 0; retry < 500; ++retry)
                {
                    if (renderer->Release() == GuiD3D12Status::Ok)
                    {
                        renderer = nullptr;
                        break;
                    }
                    Sleep(10);
                }
            }
            // An unresolved CPU/GPU owner retains its modules until this exact
            // disposable process exits. Never unload code behind live owners.
            if (!frontend && !startup && !renderer)
            {
                if (cgui)
                    FreeLibrary(cgui);
                if (core)
                    FreeLibrary(core);
            }
            if (event)
                CloseHandle(event);
        }
        void Wait()
        {
            const auto value = nextFence++;
            Hr(queue->Signal(fence.Get(), value), "signal owned queue completion");
            if (fence->GetCompletedValue() < value)
            {
                Hr(fence->SetEventOnCompletion(value, event), "arm owned completion event");
                Check(WaitForSingleObject(event, 5000) == WAIT_OBJECT_0, "owned WARP work drains before deadline");
            }
            Check(fence->GetCompletedValue() != UINT64_MAX, "owned device remains present");
        }
        void Begin()
        {
            Hr(allocator->Reset(), "reset retired host allocator");
            Hr(list->Reset(allocator.Get(), nullptr), "reset host command list");
        }
        void Execute()
        {
            Hr(list->Close(), "close host command list");
            ID3D12CommandList* commands[] = {list.Get()};
            queue->ExecuteCommandLists(1, commands);
            Wait();
        }
        mta::gui::GuiD3D12Frame Frame(D3D12_RESOURCE_STATES before) const
        {
            return {target.Get(), static_cast<std::uint32_t>(before), D3D12_RESOURCE_STATE_COPY_SOURCE, Width, Height};
        }
        Frontend::Snapshot State()
        {
            Frontend::Snapshot snapshot{};
            Check(read(frontend, snapshot.data(), static_cast<std::uint32_t>(snapshot.size())) == Frontend::Status::Ok, "read owner-thread frontend values");
            Check(snapshot[Frontend::Version] == 1 && !snapshot[Frontend::NetworkStarted] && snapshot[Frontend::AdapterStarted] &&
                      !snapshot[Frontend::OwnsCoreShell] && !snapshot[Frontend::GameplayReady] && !snapshot[Frontend::FullMtaReady] &&
                      std::all_of(snapshot.begin() + Frontend::Reserved29, snapshot.end(), [](auto value) { return value == 0; }),
                  "borrowed frontend reports its real backend owner without claiming network or gameplay readiness");
            return snapshot;
        }
    };
    void Transition(ID3D12GraphicsCommandList* list, ID3D12Resource* resource, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after)
    {
        if (before == after)
            return;
        D3D12_RESOURCE_BARRIER barrier{};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition = {resource, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, before, after};
        list->ResourceBarrier(1, &barrier);
    }
    void InitializeWarp(Owned& owned)
    {
        ComPtr<ID3D12Debug> debug;
        const bool          available = SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug)));
        if (available)
            debug->EnableDebugLayer();
        std::printf("D3D12 debug layer available=%u\n", available ? 1u : 0u);
        ComPtr<IDXGIFactory4> factory;
        ComPtr<IDXGIAdapter>  adapter;
        Hr(CreateDXGIFactory1(IID_PPV_ARGS(&factory)), "create system DXGI factory");
        Hr(factory->EnumWarpAdapter(IID_PPV_ARGS(&adapter)), "select Microsoft WARP");
        Hr(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&owned.device)), "create disposable WARP device");
        if (available)
            Hr(owned.device.As(&owned.info), "observe owned device debug messages");
        D3D12_COMMAND_QUEUE_DESC queue{};
        queue.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        Hr(owned.device->CreateCommandQueue(&queue, IID_PPV_ARGS(&owned.queue)), "create private direct queue");
        Hr(owned.device->CreateCommandAllocator(queue.Type, IID_PPV_ARGS(&owned.allocator)), "create private host allocator");
        Hr(owned.device->CreateCommandList(0, queue.Type, owned.allocator.Get(), nullptr, IID_PPV_ARGS(&owned.list)), "create private host list");
        Hr(owned.list->Close(), "close initial private list");
        Hr(owned.device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&owned.fence)), "create completion fence");
        Hr(owned.device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&owned.gate)), "create deterministic queue gate");
        owned.event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        Check(owned.event != nullptr, "create bounded host completion event");
        D3D12_DESCRIPTOR_HEAP_DESC rtv{};
        rtv.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        rtv.NumDescriptors = 1;
        Hr(owned.device->CreateDescriptorHeap(&rtv, IID_PPV_ARGS(&owned.rtv)), "create host RTV");
        D3D12_RESOURCE_DESC target{};
        target.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        target.Width = Owned::Width;
        target.Height = Owned::Height;
        target.DepthOrArraySize = target.MipLevels = 1;
        target.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        target.SampleDesc.Count = 1;
        target.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
        D3D12_HEAP_PROPERTIES heap{};
        heap.Type = D3D12_HEAP_TYPE_DEFAULT;
        heap.CreationNodeMask = heap.VisibleNodeMask = 1;
        Hr(owned.device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &target, D3D12_RESOURCE_STATE_RENDER_TARGET, nullptr,
                                                 IID_PPV_ARGS(&owned.target)),
           "create actual private BGRA target");
        owned.device->CreateRenderTargetView(owned.target.Get(), nullptr, owned.rtv->GetCPUDescriptorHandleForHeapStart());
        UINT64 bytes = 0;
        owned.device->GetCopyableFootprints(&target, 0, 1, 0, &owned.footprint, nullptr, nullptr, &bytes);
        target = {};
        target.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        target.Width = bytes;
        target.Height = target.DepthOrArraySize = target.MipLevels = 1;
        target.SampleDesc.Count = 1;
        target.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        heap.Type = D3D12_HEAP_TYPE_READBACK;
        Hr(owned.device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &target, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&owned.readback)),
           "create private readback buffer");
    }
    std::vector<std::uint32_t> Raster(Owned& owned, bool initial = false)
    {
        owned.Begin();
        Transition(owned.list.Get(), owned.target.Get(), initial ? D3D12_RESOURCE_STATE_RENDER_TARGET : D3D12_RESOURCE_STATE_COPY_SOURCE,
                   D3D12_RESOURCE_STATE_RENDER_TARGET);
        const float color[] = {17.0f / 255, 29.0f / 255, 43.0f / 255, 1};
        owned.list->ClearRenderTargetView(owned.rtv->GetCPUDescriptorHandleForHeapStart(), color, 0, nullptr);
        owned.Execute();
        Check(owned.renderer->RenderFrame(owned.Frame(D3D12_RESOURCE_STATE_RENDER_TARGET)) == GuiD3D12Status::Ok,
              "render existing MTA widgets on actual owned WARP target");
        owned.Wait();
        owned.Begin();
        D3D12_TEXTURE_COPY_LOCATION source{}, destination{};
        source.pResource = owned.target.Get();
        source.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        destination.pResource = owned.readback.Get();
        destination.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        destination.PlacedFootprint = owned.footprint;
        owned.list->CopyTextureRegion(&destination, 0, 0, 0, &source, nullptr);
        owned.Execute();
        void*             bytes = nullptr;
        const D3D12_RANGE read{0, static_cast<SIZE_T>(owned.readback->GetDesc().Width)};
        Hr(owned.readback->Map(0, &read, &bytes), "map actual frontend raster");
        std::vector<std::uint32_t> pixels(SIZE_T(Owned::Width) * Owned::Height);
        for (UINT y = 0; y < Owned::Height; ++y)
            std::memcpy(pixels.data() + SIZE_T(y) * Owned::Width,
                        static_cast<const BYTE*>(bytes) + owned.footprint.Offset + SIZE_T(y) * owned.footprint.Footprint.RowPitch, Owned::Width * 4);
        const D3D12_RANGE noWrite{};
        owned.readback->Unmap(0, &noWrite);
        return pixels;
    }
    void Save(const std::filesystem::path& path, const std::vector<std::uint32_t>& pixels)
    {
        BITMAPFILEHEADER file{};
        BITMAPINFOHEADER info{};
        file.bfType = 0x4d42;
        file.bfOffBits = sizeof(file) + sizeof(info);
        file.bfSize = file.bfOffBits + Owned::Width * Owned::Height * 4;
        info.biSize = sizeof(info);
        info.biWidth = Owned::Width;
        info.biHeight = -static_cast<LONG>(Owned::Height);
        info.biPlanes = 1;
        info.biBitCount = 32;
        std::ofstream stream(path, std::ios::binary);
        stream.write(reinterpret_cast<const char*>(&file), sizeof(file));
        stream.write(reinterpret_cast<const char*>(&info), sizeof(info));
        stream.write(reinterpret_cast<const char*>(pixels.data()), static_cast<std::streamsize>(pixels.size() * sizeof(pixels[0])));
        Check(stream.good(), "save actual BGRA frontend raster evidence");
    }
    void Frame(Owned& owned, int x, int y, bool focused = true)
    {
        owned.gui->ProcessMouseInput(CGUI_MI_MOUSEPOS, x, y);
        Check(owned.pulse(owned.frontend, x, y, focused ? 1u : 0u) == Frontend::Status::Ok, "pulse actual MTA frontend without game or multiplayer facade");
    }
    void Click(Owned& owned, int x, int y)
    {
        Frame(owned, x, y);
        owned.gui->ProcessMouseInput(CGUI_MI_MOUSEDOWN, 0, 0, LeftButton);
        owned.gui->ProcessMouseInput(CGUI_MI_MOUSEUP, 0, 0, LeftButton);
    }
    void CheckIsolation(bool backendExpected)
    {
        Check((GetModuleHandleW(L"game_de.dll") != nullptr) == backendExpected && !GetModuleHandleW(L"game_sa.dll") &&
                  !GetModuleHandleW(L"multiplayer_sa.dll") && !GetModuleHandleW(L"netc.dll") && !GetModuleHandleW(L"d3dx9_43.dll"),
              "only the explicitly owned Game DE backend is admitted; classic game/network modules stay absent");
    }
    Startup::Snapshot StartupState(Owned& owned)
    {
        Startup::Snapshot state{};
        Check(owned.readStartup(owned.startup, state.data(), static_cast<std::uint32_t>(state.size())) == Frontend::Status::Ok,
              "read actual completed Core lifecycle values on its owner thread");
        Check(state[Startup::Version] == Startup::ApiVersion && state[Startup::OwnerThread] == GetCurrentThreadId() && !state[Startup::NetworkStarted] &&
                  !state[Startup::GameplayReady] && !state[Startup::FullMtaReady] &&
                  std::all_of(state.begin() + 24, state.end(), [](auto value) { return value == 0; }),
              "completed frontend Core never upgrades network, gameplay or full-client readiness");
        return state;
    }
    void SaveResult(const std::filesystem::path& root, const Startup::Snapshot& startup, const Frontend::Snapshot& frontend)
    {
        std::ofstream result(root / L"frontend-startup-result.json");
        result << "{\"schema\":\"mta_owned_frontend_startup_host_v1\",\"success\":true,\"checks\":" << checks + 1 << ",\"startupTerminal\":[";
        for (std::size_t index = 0; index < startup.size(); ++index)
            result << (index ? "," : "") << startup[index];
        result << "],\"frontendTerminal\":[";
        for (std::size_t index = 0; index < frontend.size(); ++index)
            result << (index ? "," : "") << frontend[index];
        result << "],\"nativeEngineBindingVerified\":false,\"fullMtaReady\":false,\"gameLaunched\":false}\n";
        Check(result.good(), "save actual completed-Core and frontend terminal values");
    }
    void ConsoleAndBindings(Owned& owned)
    {
        const auto enable = Export<Console::Enable>(owned.core, "MTA_EnableDEFrontendConsole");
        const auto visible = Export<Console::SetVisible>(owned.core, "MTA_SetDEFrontendConsoleVisible");
        const auto read = Export<Console::Read>(owned.core, "MTA_ReadDEFrontendConsole");
        const auto text = Export<Console::ReadText>(owned.core, "MTA_ReadDEFrontendConsoleText");
        const auto enableBindings = Export<Bindings::Enable>(owned.core, "MTA_EnableDEFrontendBindings");
        const auto readBindings = Export<Bindings::Read>(owned.core, "MTA_ReadDEFrontendBindings");
        const auto neutral = Export<Bindings::ProduceNeutral>(owned.core, "MTA_ProduceDEFrontendNeutralInput");
        Check(enable(owned.frontend, Console::ApiVersion) == Frontend::Status::Ok && visible(owned.frontend, 1) == Frontend::Status::Ok,
              "completed Core's borrowed frontend enables and shows its real optional console");
        const auto readText = [&](Console::Text field)
        {
            std::uint32_t required = 0;
            Check(text(owned.frontend, static_cast<std::uint32_t>(field), nullptr, 0, &required) == Frontend::Status::Ok && required > 0 &&
                      required <= Console::MaximumTextCapacity,
                  "actual console text exposes a bounded copied size");
            std::vector<char> buffer(required);
            Check(text(owned.frontend, static_cast<std::uint32_t>(field), buffer.data(), required, &required) == Frontend::Status::Ok,
                  "copy the existing console memo/input without direct widget access");
            return std::string(buffer.data());
        };
        const auto submit = [&](const char* command)
        {
            // This host owns its CGUI directly. Character and Return injection
            // use the same actual edit/parser route as normal MTA input, without
            // an engine keyboard, synthetic CGame or fixture command handler.
            for (const unsigned char* cursor = reinterpret_cast<const unsigned char*>(command); *cursor; ++cursor)
                owned.gui->ProcessCharacter(*cursor);
            Check(readText(Console::Text::Input) == command, "actual console edit retains supplied command text");
            owned.gui->ProcessKeyboardInput(CGUIKeys::Return, true);
            owned.gui->ProcessKeyboardInput(CGUIKeys::Return, false);
            Check(readText(Console::Text::Input).empty(), "existing console Enter callback accepts and clears real edit text");
        };
        submit("help");
        Check(readText(Console::Text::Output).find("COMMAND HELP") != std::string::npos,
              "completed Core's console invokes the existing Help parser and output handler");
        submit("clear");
        Check(readText(Console::Text::Output).find_first_not_of("\r\n") == std::string::npos, "existing Clear command empties the actual console output memo");
        Console::Snapshot state{};
        Check(read(owned.frontend, state.data(), Console::FieldCount) == Frontend::Status::Ok && state[Console::CommandsExecuted] == 2 &&
                  state[Console::TextAccepted] == 2 && state[Console::LastCommand] == static_cast<std::uint32_t>(Console::Command::Clear) &&
                  !state[Console::CommandsRejected] && !state[Console::HistoryPersistent] && !state[Console::NetworkStarted] && !state[Console::FullMtaReady],
              "real Help/Clear work remains local, memory-only and separately counted");
        Check(visible(owned.frontend, 0) == Frontend::Status::Ok && owned.State()[Frontend::InputRequested],
              "hiding real console preserves its completed Core menu's aggregate input request");
        Check(enableBindings(owned.frontend, Bindings::ApiVersion) == Frontend::Status::Ok,
              "completed Core owns actual configured MTA bindings before XML retirement");
        mta::game_de::MtaInputState frame;
        frame.current.ButtonCross = 27;
        Check(neutral(owned.frontend, &frame, sizeof(frame)) == Frontend::Status::Ok, "actual MTA frontend bindings produce a copied neutral frame");
        const mta::game_de::MtaInputState zero;
        Bindings::Snapshot                bindings{};
        Check(std::memcmp(&frame, &zero, sizeof(frame)) == 0 && readBindings(owned.frontend, bindings.data(), Bindings::FieldCount) == Frontend::Status::Ok &&
                  bindings[Bindings::BindingsCreated] && bindings[Bindings::ConfigurationLoaded] && bindings[Bindings::BindingsRetained] &&
                  bindings[Bindings::CurrentZero] && bindings[Bindings::PreviousZero] && !bindings[Bindings::NetworkStarted] &&
                  !bindings[Bindings::FullMtaReady],
              "configured synthesis proves both neutral histories while keeping native engine binding unclaimed");
    }
}

int wmain(int argc, wchar_t** argv)
{
    setvbuf(stdout, nullptr, _IONBF, 0);
    setvbuf(stderr, nullptr, _IONBF, 0);
    try
    {
        Check(argc == 3 && sizeof(void*) == 8, "owned x64 frontend startup root and case supplied");
        const std::filesystem::path root(argv[1]);
        const std::wstring          mode(argv[2]);
        Check(root.is_absolute() && (mode == L"normal" || mode == L"gated" || mode == L"frontend-failure" || mode == L"backend-failure"),
              "explicit completed-Core lifecycle case selected");
        Check(!std::filesystem::exists(root / L"server"), "owned frontend has no local-server startup assets");
        Owned owned;
        InitializeWarp(owned);
        owned.core = LoadLibraryExW((root / L"core.dll").c_str(), nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
        Check(owned.core != nullptr, "load current normal Core in the hashed disposable SanAndreas.exe host");
        const auto setRoot = Export<void (*)(const wchar_t*, std::size_t)>(owned.core, "SetMTADirectory");
        const auto setGameRoot = Export<void (*)(const wchar_t*, std::size_t)>(owned.core, "SetGTADirectory");
        const auto initializeCore = Export<int (*)()>(owned.core, "InitializeCore");
        const auto readLegacy = Export<std::uint32_t (*)(std::uint32_t)>(owned.core, "MTA_ReadDEStartupState");
        // A rejected pre-directory attempt establishes readable owner-thread
        // values. All-Unavailable values could otherwise hide accidental writes
        // to the old startup diagnostics while its ownerThread remains zero.
        Check(initializeCore() == 3, "legacy missing-directory refusal creates no Core or backend but establishes Startup8 sentinel");
        std::array<std::uint32_t, 8> legacy{};
        for (std::uint32_t index = 0; index < legacy.size(); ++index)
            legacy[index] = readLegacy(index);
        Check(legacy[0] == 1 && legacy[1] == 3 && legacy[7] == 1 && !legacy[6], "legacy startup sentinel is a real failed attempt, not unreadable defaults");
        Check(Export<std::uint32_t (*)()>(owned.core, "MTA_DEFrontendStartupApiVersion")() == Startup::ApiVersion,
              "normal Core exposes separate completed frontend startup API1");
        owned.beginStartup = Export<Startup::Begin>(owned.core, "MTA_BeginDEFrontendStartup");
        owned.pulseStartup = Export<Startup::Pulse>(owned.core, "MTA_PulseDEFrontendStartup");
        owned.bindStartup = Export<Startup::BindInput>(owned.core, "MTA_BindDEFrontendStartupInput");
        owned.readStartup = Export<Startup::Read>(owned.core, "MTA_ReadDEFrontendStartup");
        owned.shutdownStartup = Export<Startup::Shutdown>(owned.core, "MTA_ShutdownDEFrontendStartup");
        owned.releaseStartup = Export<Startup::Release>(owned.core, "MTA_ReleaseDEFrontendStartup");
        owned.runView = Export<Startup::RunView>(owned.core, "MTA_RunDEFrontendStartupView");
        Check(!owned.beginStartup(0) && !owned.beginStartup(Startup::ApiVersion + 1) && !owned.beginStartup(Startup::ApiVersion),
              "wrong versions and missing explicit directories cannot construct a frontend Core owner");
        CheckIsolation(false);
        const auto wide = root.wstring();
        const auto gameWide = root.parent_path().parent_path().wstring();
        setRoot(wide.data(), wide.size());
        Check(!owned.beginStartup(Startup::ApiVersion), "missing GTA directory still refuses completed Core creation after MTA root is supplied");
        setGameRoot(gameWide.data(), gameWide.size());
        Check(!owned.beginStartup(0) && !owned.beginStartup(Startup::ApiVersion + 1),
              "wrong frontend startup versions remain rejected with both explicit directories ready");
        owned.startup = owned.beginStartup(Startup::ApiVersion);
        Check(owned.startup != nullptr, "allocated startup publishes completed Core ownership even if backend preparation fails");
        auto startup = StartupState(owned);
        Check(!owned.beginStartup(Startup::ApiVersion), "second startup cannot replace an existing completed Core owner");
        Startup::Snapshot sentinel;
        sentinel.fill(0x13572468);
        const auto unchanged = sentinel;
        Check(owned.readStartup(owned.startup, sentinel.data(), 31) == Frontend::Status::InvalidArgument && sentinel == unchanged &&
                  owned.readStartup(owned.startup, nullptr, 32) == Frontend::Status::InvalidArgument &&
                  owned.readStartup(reinterpret_cast<void*>(std::uintptr_t{1}), sentinel.data(), 32) == Frontend::Status::InvalidArgument &&
                  sentinel == unchanged,
              "wrong count, absent output and unknown lifecycle identity preserve caller storage");
        bool        foreign = false;
        std::thread other(
            [&]
            {
                Startup::Snapshot output = unchanged;
                foreign = owned.readStartup(owned.startup, output.data(), 32) == Frontend::Status::WrongThread && output == unchanged &&
                          owned.pulseStartup(owned.startup) == Frontend::Status::WrongThread &&
                          owned.bindStartup(owned.startup) == Frontend::Status::WrongThread &&
                          owned.shutdownStartup(owned.startup) == Frontend::Status::WrongThread &&
                          owned.releaseStartup(owned.startup) == Frontend::Status::WrongThread &&
                          owned.runView(owned.startup, 100, &CancelView, &PublishView) == Frontend::Status::WrongThread;
            });
        other.join();
        Check(foreign, "foreign thread cannot read, bind, pulse, stop or release the completed Core owner");
        Check(owned.runView(owned.startup, 0, &CancelView, &PublishView) == Frontend::Status::InvalidArgument &&
                  owned.runView(owned.startup, 35001, &CancelView, &PublishView) == Frontend::Status::InvalidArgument &&
                  owned.runView(owned.startup, 100, nullptr, &PublishView) == Frontend::Status::InvalidArgument &&
                  owned.runView(owned.startup, 100, &CancelView, nullptr) == Frontend::Status::InvalidArgument && !cancelCalls && !publishCalls,
              "invalid bounded-view arguments never call user callbacks or enter native work");
        owned.create = Export<Frontend::Create>(owned.core, "MTA_CreateDEFrontendSession");
        owned.pulse = Export<Frontend::Pulse>(owned.core, "MTA_PulseDEFrontendSession");
        owned.read = Export<Frontend::Read>(owned.core, "MTA_ReadDEFrontendSession");
        owned.shutdown = Export<Frontend::Shutdown>(owned.core, "MTA_ShutdownDEFrontendSession");
        owned.release = Export<Frontend::Release>(owned.core, "MTA_ReleaseDEFrontendSession");
        const auto initializeGui = [&]
        {
            owned.cgui = LoadLibraryExW((root / L"cgui.dll").c_str(), nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
            Check(owned.cgui != nullptr, "load current normal cgui after completed backend preparation");
            const auto configure = Export<mta::gui::ConfigureGuiD3D12AssetRoot>(owned.cgui, "ConfigureGUID3D12AssetRoot");
            Check(configure(mta::gui::GuiD3D12AssetRootApiVersion, wide.data(), static_cast<std::uint32_t>(wide.size())) == 1,
                  "configure actual GUI assets beneath the disposable owned root");
            mta::gui::GuiD3D12CreateInfo info;
            info.device = owned.device.Get();
            info.queue = owned.queue.Get();
            info.targetFormat = DXGI_FORMAT_B8G8R8A8_UNORM;
            info.width = Owned::Width;
            info.height = Owned::Height;
            owned.renderer = Export<mta::gui::CreateGuiD3D12Session>(owned.cgui, "CreateGUID3D12Session")(mta::gui::GuiD3D12ApiVersion, &info);
            Check(owned.renderer && owned.renderer->GetGUI(owned.gui) == GuiD3D12Status::Ok && owned.gui, "borrow actual owner-thread MTA D3D12 GUI");
            owned.gui->SetSkin("Default");
            owned.gui->SetCursorEnabled(false);
        };
        Frontend::Snapshot state{};
        if (mode == L"backend-failure")
        {
            Check(startup[Startup::Phase] == 5 && startup[Startup::CoreRetained] && startup[Startup::BackendStartAttempted] &&
                      !startup[Startup::BackendStarted] && startup[Startup::LastFailure],
                  "missing actual Game DE publishes a failed handle retaining its completed Core for explicit cleanup");
            CheckIsolation(false);
            initializeGui();
            Check(!owned.create(Frontend::ApiVersion, owned.gui) && !StartupState(owned)[Startup::FrontendRetained],
                  "a failed backend Core cannot admit a borrowed frontend or leave a partial GUI owner");
        }
        else
        {
            Check(startup[Startup::Phase] == 2 && startup[Startup::CoreRetained] && startup[Startup::BackendStartAttempted] &&
                      startup[Startup::BackendStarted] && !startup[Startup::ExactImageBound] && startup[Startup::Pulses] >= 1,
                  "Begin completes a real Core and initializes its actual backend without native engine binding");
            CheckIsolation(true);
            const auto pulses = startup[Startup::Pulses];
            Check(owned.pulseStartup(owned.startup) == Frontend::Status::Ok && StartupState(owned)[Startup::Pulses] == pulses + 1,
                  "same owner pulses its actual initialized Game DE backend");
            Check(owned.bindStartup(owned.startup) == Frontend::Status::Failed, "explicit native binding refuses this non-game executable's actual bytes");
            startup = StartupState(owned);
            Check(startup[Startup::Phase] == 2 && startup[Startup::CoreRetained] && startup[Startup::BackendStarted] && !startup[Startup::ExactImageBound] &&
                      startup[Startup::LastFailure] && !startup[Startup::CapabilitiesHigh] &&
                      startup[Startup::CapabilitiesLow] == (static_cast<std::uint32_t>(mta::game_de::AdapterCapability::ObservationIngest) |
                                                            static_cast<std::uint32_t>(mta::game_de::AdapterCapability::MtaInputSynthesis)),
                  "wrong-image bind failure preserves the usable completed owner and only CPU storage/synthesis capabilities");
            Check(owned.runView(owned.startup, 100, &CancelView, &PublishView) == Frontend::Status::Failed && !cancelCalls && !publishCalls &&
                      !StartupState(owned)[Startup::ViewRuns] && StartupState(owned)[Startup::LastViewCleanupComplete],
                  "real bounded view refuses wrong-image native binding before callbacks, GUI work or new ownership");
            initializeGui();
            const auto empty = Raster(owned, true);
            Check(std::all_of(empty.begin(), empty.end(), [&](auto value) { return value == empty.front(); }), "empty actual GUI target is uniform");
            owned.frontend = owned.create(Frontend::ApiVersion, owned.gui);
            Check(owned.frontend != nullptr, "actual borrowed frontend publishes a handle even on XML startup failure");
            state = owned.State();
            startup = StartupState(owned);
            Check(startup[Startup::FrontendRetained] && startup[Startup::FrontendPhase] == state[Frontend::Phase] && startup[Startup::CoreRetained] &&
                      !state[Frontend::OwnsCoreShell] && state[Frontend::AdapterStarted],
                  "existing frontend borrows the already completed Core and its initialized backend");
            Check(owned.shutdownStartup(owned.startup) == Frontend::Status::Busy && owned.releaseStartup(owned.startup) == Frontend::Status::Busy,
                  "live or failed borrowed frontend prevents completed Core destruction");
            Check(owned.runView(owned.startup, 100, &CancelView, &PublishView) == Frontend::Status::Busy && !cancelCalls && !publishCalls,
                  "retained frontend prevents a second bounded view from replacing its Core/GUI ownership");
            if (mode == L"frontend-failure")
                Check(state[Frontend::Phase] == 5, "missing XML fails only borrowed frontend startup while the completed Core remains owned");
            else
            {
                Check(state[Frontend::Phase] == 2 && state[Frontend::XmlLoaded] && state[Frontend::ConfigLoaded] && state[Frontend::LocalizationLoaded] &&
                          state[Frontend::UpdaterCreated] && !state[Frontend::UpdaterWorkerStarted] && state[Frontend::GuiBorrowed] &&
                          state[Frontend::ConfigurationReadOnly] && !state[Frontend::ApplicationLocalePersisted],
                      "real read-only Core services and MTA menu start on the completed frontend owner");
                const auto settleUntil = GetTickCount64() + 750;
                do
                {
                    Frame(owned, 0, 0);
                    Sleep(15);
                } while (GetTickCount64() < settleUntil);
                state = owned.State();
                Check(state[Frontend::MenuVisible] && !state[Frontend::MenuFading] && state[Frontend::Frames] >= 20 && state[Frontend::InputRequested],
                      "existing MTA main menu completes its real fade and input ownership");
                const auto menu = Raster(owned);
                Check(std::count_if(menu.begin(), menu.end(), [&](auto value) { return value != empty.front(); }) > 10000,
                      "actual menu assets produce substantial WARP pixels");
                Save(root / L"frontend-menu.bmp", menu);
                Click(owned, 230, 760);
                state = owned.State();
                Check(state[Frontend::AboutClicks] == 1 && state[Frontend::CreditsVisible], "real About callback opens existing credits on the completed Core");
                const auto credits = Raster(owned);
                Check(credits != menu, "existing credits change actual rendered pixels");
                Save(root / L"frontend-credits.bmp", credits);
                Click(owned, 840, 644);
                Check(!owned.State()[Frontend::CreditsVisible], "existing credits OK callback closes its real window");
                ConsoleAndBindings(owned);
                if (mode == L"gated")
                {
                    Hr(owned.queue->Wait(owned.gate.Get(), 1), "gate only the disposable renderer queue");
                    owned.blocked = true;
                    Check(owned.renderer->RenderFrame(owned.Frame(D3D12_RESOURCE_STATE_COPY_SOURCE)) == GuiD3D12Status::Ok,
                          "retain actual GUI GPU versions behind the owned fence gate");
                }
                else
                {
                    Click(owned, 230, 804);
                    Check(owned.State()[Frontend::CloseRequested], "existing Quit callback requests frontend close without terminating the completed Core");
                }
            }
            Check(owned.shutdown(owned.frontend) == Frontend::Status::Ok, "stop real frontend services while retaining their completed Core");
            state = owned.State();
            if (mode == L"normal" || mode == L"gated")
            {
                Console::Snapshot  stoppedConsole{};
                Bindings::Snapshot stoppedBindings{};
                Check(Export<Console::Read>(owned.core, "MTA_ReadDEFrontendConsole")(owned.frontend, stoppedConsole.data(), Console::FieldCount) ==
                              Frontend::Status::Ok &&
                          stoppedConsole[Console::CommandsExecuted] == 2 && stoppedConsole[Console::TextAccepted] == 2 &&
                          stoppedConsole[Console::CleanupComplete] && !stoppedConsole[Console::ConsoleRetained] && !stoppedConsole[Console::CommandsRetained] &&
                          Export<Bindings::Read>(owned.core, "MTA_ReadDEFrontendBindings")(owned.frontend, stoppedBindings.data(), Bindings::FieldCount) ==
                              Frontend::Status::Ok &&
                          stoppedBindings[Bindings::CleanupComplete] && !stoppedBindings[Bindings::BindingsRetained] &&
                          !stoppedBindings[Bindings::InputSuppressed] && stoppedBindings[Bindings::CurrentZero] && stoppedBindings[Bindings::PreviousZero],
                      "real stopped frontend caches command evidence and retires configured input owners before Core release");
            }
            Check(state[Frontend::Phase] == 4 && state[Frontend::CleanupComplete] && !state[Frontend::ServicesRetained] && !state[Frontend::GuiBorrowed],
                  "actual CPU frontend services retire independently of the backend owner");
            Check(owned.shutdownStartup(owned.startup) == Frontend::Status::Busy && owned.releaseStartup(owned.startup) == Frontend::Status::Busy,
                  "stopped but unreleased frontend handle still prevents destroying its borrowed completed Core");
            const auto staleFrontend = owned.frontend;
            Check(owned.release(owned.frontend) == Frontend::Status::Ok, "release the completed actual borrowed frontend");
            owned.frontend = nullptr;
            Frontend::Snapshot frontendSentinel;
            frontendSentinel.fill(0x24681357);
            const auto frontendUnchanged = frontendSentinel;
            Check(owned.read(staleFrontend, frontendSentinel.data(), Frontend::FieldCount) == Frontend::Status::InvalidArgument &&
                      frontendSentinel == frontendUnchanged,
                  "retired frontend identity cannot copy freed Core state");
            startup = StartupState(owned);
            Check(startup[Startup::CoreRetained] && startup[Startup::BackendStarted] && !startup[Startup::FrontendRetained] && !startup[Startup::GuiBorrowed] &&
                      !startup[Startup::ServicesRetained] && owned.pulseStartup(owned.startup) == Frontend::Status::Ok,
                  "completed Core and actual backend persist and pulse after frontend release");
            Check(!GetModuleHandleW(L"xmll.dll"), "frontend XML module unloads while the completed Core remains alive");
            if (mode == L"normal")
            {
                owned.frontend = owned.create(Frontend::ApiVersion, owned.gui);
                Check(owned.frontend && owned.State()[Frontend::Phase] == 2 && !owned.State()[Frontend::OwnsCoreShell],
                      "a second real menu can borrow the same retained completed Core");
                Check(owned.release(owned.frontend) == Frontend::Status::Ok, "release reacquired frontend through normal CPU cleanup");
                owned.frontend = nullptr;
            }
            if (mode == L"gated")
                Check(owned.renderer->Shutdown() == GuiD3D12Status::Busy, "GUI GPU owner remains genuinely Busy after CPU frontend retirement");
        }
        Check(owned.shutdownStartup(owned.startup) == Frontend::Status::Ok,
              "explicit startup shutdown retires the actual backend once no frontend borrows Core");
        startup = StartupState(owned);
        Check(startup[Startup::Phase] == 4 && startup[Startup::CoreRetained] && !startup[Startup::BackendStarted] && startup[Startup::CleanupComplete] &&
                  !startup[Startup::CleanupPending] && !startup[Startup::ReleaseFault],
              "stopped completed Core is retained for final read and explicit release");
        Check(owned.shutdownStartup(owned.startup) == Frontend::Status::Ok && owned.pulseStartup(owned.startup) == Frontend::Status::Stopped &&
                  owned.bindStartup(owned.startup) == Frontend::Status::Stopped,
              "completed shutdown is idempotent and blocks new backend work");
        Check(!owned.create(Frontend::ApiVersion, owned.gui) && !StartupState(owned)[Startup::FrontendRetained],
              "stopped completed Core rejects a new borrowed frontend before final release");
        const auto staleStartup = owned.startup;
        Check(owned.releaseStartup(owned.startup) == Frontend::Status::Ok, "release the completed stopped Core owner");
        owned.startup = nullptr;
        Check(owned.readStartup(staleStartup, sentinel.data(), 32) == Frontend::Status::InvalidArgument && sentinel == unchanged,
              "released completed Core handle cannot access freed singleton state");
        CheckIsolation(false);
        if (mode == L"normal")
        {
            owned.startup = owned.beginStartup(Startup::ApiVersion);
            Check(owned.startup && StartupState(owned)[Startup::Phase] == 2, "explicit lifecycle can reacquire a newly completed Core after full release");
            Check(owned.releaseStartup(owned.startup) == Frontend::Status::Ok, "release reacquired Core through actual adapter shutdown");
            owned.startup = nullptr;
            CheckIsolation(false);
        }
        if (owned.blocked)
        {
            Hr(owned.gate->Signal(1), "release only the owned GUI GPU fence gate");
            owned.blocked = false;
            owned.Wait();
        }
        if (owned.renderer)
        {
            Check(owned.renderer->Shutdown() == GuiD3D12Status::Ok && owned.renderer->Release() == GuiD3D12Status::Ok,
                  "actual cgui renderer drains after CPU and completed Core owners retire");
            owned.renderer = nullptr;
            owned.gui = nullptr;
        }
        if (owned.info)
        {
            unsigned errors = 0;
            for (UINT64 index = 0; index < owned.info->GetNumStoredMessages(); ++index)
            {
                SIZE_T size = 0;
                owned.info->GetMessage(index, nullptr, &size);
                std::vector<BYTE> storage(size);
                auto*             message = reinterpret_cast<D3D12_MESSAGE*>(storage.data());
                if (SUCCEEDED(owned.info->GetMessage(index, message, &size)) && message->Severity <= D3D12_MESSAGE_SEVERITY_ERROR)
                {
                    ++errors;
                    std::fprintf(stderr, "D3D12 debug error: %s\n", message->pDescription);
                }
            }
            Check(errors == 0, "real completed Core/menu/renderer lifecycle emits no D3D12 debug errors");
        }
        if (owned.cgui)
        {
            Check(FreeLibrary(owned.cgui) != FALSE, "unload drained actual cgui");
            owned.cgui = nullptr;
        }
        bool legacyUnchanged = true;
        for (std::uint32_t index = 0; index < legacy.size(); ++index)
            legacyUnchanged = legacyUnchanged && readLegacy(index) == legacy[index];
        Check(legacyUnchanged, "all new Core/frontend lifecycle operations preserve actual legacy Startup8 diagnostics");
        Check(FreeLibrary(owned.core) != FALSE, "unload explicitly released actual Core");
        owned.core = nullptr;
        Check(!GetModuleHandleW(L"core.dll") && !GetModuleHandleW(L"cgui.dll") && !GetModuleHandleW(L"game_de.dll"),
              "no actual Core/GUI/backend module survives the disposable lifecycle");
        SaveResult(root, startup, state);
        std::printf("Passed %u actual MTA frontend startup host checks\n", checks);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "MTA frontend startup host failed after %u checks: %s\n", checks, error.what());
        return 1;
    }
}
