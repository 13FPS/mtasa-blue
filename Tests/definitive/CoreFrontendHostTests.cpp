/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Exercise the real MTA frontend on a disposable D3D12/WARP target
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

using Microsoft::WRL::ComPtr;
namespace Frontend = DefinitiveFrontend;
using mta::gui::GuiD3D12Status;
static_assert(Frontend::ApiVersion == 1 && Frontend::FieldCount == 32);

namespace
{
    unsigned checks = 0;
    void     Check(bool condition, const char* why)
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
            if (!frontend && !renderer)
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
            Check(snapshot[Frontend::Version] == 1 && !snapshot[Frontend::NetworkStarted] && !snapshot[Frontend::AdapterStarted] &&
                      !snapshot[Frontend::GameplayReady] && !snapshot[Frontend::FullMtaReady] &&
                      std::all_of(snapshot.begin() + Frontend::Reserved29, snapshot.end(), [](auto value) { return value == 0; }),
                  "frontend values never claim network, engine, gameplay or full MTA readiness");
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
    void CheckIsolation()
    {
        Check(!GetModuleHandleW(L"game_de.dll") && !GetModuleHandleW(L"game_sa.dll") && !GetModuleHandleW(L"multiplayer_sa.dll") &&
                  !GetModuleHandleW(L"netc.dll") && !GetModuleHandleW(L"d3dx9_43.dll"),
              "frontend never loads game, multiplayer, networking or legacy D3DX9 modules");
    }
}

int wmain(int argc, wchar_t** argv)
{
    setvbuf(stdout, nullptr, _IONBF, 0);
    setvbuf(stderr, nullptr, _IONBF, 0);
    try
    {
        Check(argc == 3 && sizeof(void*) == 8, "owned x64 frontend root and case supplied");
        const std::filesystem::path root(argv[1]);
        const std::wstring          mode(argv[2]);
        Check(root.is_absolute() && (mode == L"normal" || mode == L"gated" || mode == L"startup-failure"), "explicit owned frontend case selected");
        Check(!std::filesystem::exists(root / L"server"), "owned frontend intentionally has no local-server startup assets");
        Owned owned;
        InitializeWarp(owned);
        owned.core = LoadLibraryExW((root / L"core.dll").c_str(), nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
        Check(owned.core != nullptr, "load current normal Core without InitializeCore or game attachment");
        const auto setRoot = Export<void (*)(const wchar_t*, std::size_t)>(owned.core, "SetMTADirectory");
        const auto wide = root.wstring();
        setRoot(wide.data(), wide.size());
        Check(Export<std::uint32_t (*)()>(owned.core, "MTA_DEFrontendApiVersion")() == Frontend::ApiVersion, "existing Core exposes additive frontend API1");
        owned.create = Export<Frontend::Create>(owned.core, "MTA_CreateDEFrontendSession");
        owned.pulse = Export<Frontend::Pulse>(owned.core, "MTA_PulseDEFrontendSession");
        owned.read = Export<Frontend::Read>(owned.core, "MTA_ReadDEFrontendSession");
        owned.shutdown = Export<Frontend::Shutdown>(owned.core, "MTA_ShutdownDEFrontendSession");
        owned.release = Export<Frontend::Release>(owned.core, "MTA_ReleaseDEFrontendSession");
        owned.cgui = LoadLibraryExW((root / L"cgui.dll").c_str(), nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
        Check(owned.cgui != nullptr, "load current normal cgui");
        const auto configure = Export<mta::gui::ConfigureGuiD3D12AssetRoot>(owned.cgui, "ConfigureGUID3D12AssetRoot");
        Check(configure(mta::gui::GuiD3D12AssetRootApiVersion, wide.data(), static_cast<std::uint32_t>(wide.size())) == 1,
              "configure actual GUI fonts and skin beneath owned root");
        mta::gui::GuiD3D12CreateInfo info;
        info.device = owned.device.Get();
        info.queue = owned.queue.Get();
        info.targetFormat = DXGI_FORMAT_B8G8R8A8_UNORM;
        info.width = Owned::Width;
        info.height = Owned::Height;
        owned.renderer = Export<mta::gui::CreateGuiD3D12Session>(owned.cgui, "CreateGUID3D12Session")(mta::gui::GuiD3D12ApiVersion, &info);
        Check(owned.renderer && owned.renderer->GetGUI(owned.gui) == GuiD3D12Status::Ok && owned.gui, "borrow actual owner-thread MTA GUI");
        owned.gui->SetSkin("Default");
        owned.gui->SetCursorEnabled(false);
        const auto empty = Raster(owned, true);
        Check(std::all_of(empty.begin(), empty.end(), [&](auto value) { return value == empty.front(); }),
              "empty owned GUI raster is uniform before frontend creation");
        Check(!owned.create(0, owned.gui) && !owned.create(2, owned.gui) && !owned.create(Frontend::ApiVersion, nullptr),
              "bad frontend creation arguments return no owner");
        owned.frontend = owned.create(Frontend::ApiVersion, owned.gui);
        Check(owned.frontend != nullptr, "frontend creation publishes ownership even on startup failure");
        auto state = owned.State();
        Check(!owned.create(Frontend::ApiVersion, owned.gui), "second frontend cannot steal process-local Core and GUI ownership");
        Frontend::Snapshot sentinel;
        sentinel.fill(0x12345678);
        const auto unchanged = sentinel;
        Check(owned.read(owned.frontend, sentinel.data(), 31) == Frontend::Status::InvalidArgument && sentinel == unchanged &&
                  owned.read(owned.frontend, nullptr, 32) == Frontend::Status::InvalidArgument,
              "invalid frontend read buffers preserve caller output");
        bool        foreign = false;
        std::thread other(
            [&]
            {
                Frontend::Snapshot output = unchanged;
                foreign = owned.read(owned.frontend, output.data(), 32) == Frontend::Status::WrongThread && output == unchanged &&
                          owned.pulse(owned.frontend, 0, 0, 1) == Frontend::Status::WrongThread &&
                          owned.shutdown(owned.frontend) == Frontend::Status::WrongThread && owned.release(owned.frontend) == Frontend::Status::WrongThread;
            });
        other.join();
        Check(foreign, "foreign thread cannot pulse, read values, stop or release the frontend");
        if (mode == L"startup-failure")
        {
            Check(state[Frontend::Phase] == 5, "missing XML produces an observable failed startup owner");
        }
        else
        {
            Check(state[Frontend::Phase] == 2 && state[Frontend::XmlLoaded] && state[Frontend::ConfigLoaded] && state[Frontend::LocalizationLoaded] &&
                      state[Frontend::UpdaterCreated] && !state[Frontend::UpdaterWorkerStarted] && state[Frontend::GuiBorrowed] &&
                      state[Frontend::ConfigurationReadOnly] && !state[Frontend::ApplicationLocalePersisted] && state[Frontend::UpdaterConfigLoaded],
                  "actual Core configuration, localization, updater and menu start with background work and persistence disabled");
            Check(owned.pulse(owned.frontend, 0, 0, 2) == Frontend::Status::InvalidArgument, "invalid frontend focus scalar is rejected");
            // Complete both the existing menu's fade and its 600ms initial
            // layout movement before using the real menu-item hit regions.
            const auto settleUntil = GetTickCount64() + 750;
            do
            {
                Frame(owned, 0, 0);
                Sleep(15);
            } while (GetTickCount64() < settleUntil);
            state = owned.State();
            Check(state[Frontend::MenuVisible] && !state[Frontend::MenuFading] && state[Frontend::Frames] >= 20 && state[Frontend::InputRequested],
                  "existing MTA main menu completes its actual visible fade");
            owned.gui->SetCursorEnabled(false);
            const auto menu = Raster(owned);
            Check(std::count_if(menu.begin(), menu.end(), [&](auto value) { return value != empty.front(); }) > 10000,
                  "actual existing MTA menu assets produce substantial GPU pixels");
            Save(root / L"frontend-menu.bmp", menu);
            Frame(owned, 230, 760, false);
            Frame(owned, 230, 760, true);
            Click(owned, 230, 760);
            state = owned.State();
            Check(state[Frontend::AboutClicks] == 1 && state[Frontend::CreditsVisible] && !state[Frontend::CloseRequested],
                  "actual menu About hit region invokes existing credits callback");
            const auto credits = Raster(owned);
            Check(credits != menu, "existing credits callback changes actual rendered frontend pixels");
            Save(root / L"frontend-credits.bmp", credits);
            Click(owned, 840, 644);
            Check(!owned.State()[Frontend::CreditsVisible], "existing credits OK button closes the actual credits window");
            CheckIsolation();
            if (mode == L"gated")
            {
                Hr(owned.queue->Wait(owned.gate.Get(), 1), "gate only the owned renderer queue");
                owned.blocked = true;
                Check(owned.renderer->RenderFrame(owned.Frame(D3D12_RESOURCE_STATE_COPY_SOURCE)) == GuiD3D12Status::Ok,
                      "retain actual menu GPU versions behind a deterministic gate");
            }
            else
            {
                Click(owned, 230, 804);
                Check(owned.State()[Frontend::CloseRequested], "actual MTA Quit callback requests host close without exiting the process");
            }
        }
        const auto stoppedAt = GetTickCount64();
        Check(owned.shutdown(owned.frontend) == Frontend::Status::Ok && GetTickCount64() - stoppedAt < 1000,
              "owner stops real CPU frontend services without waiting for GUI GPU fences");
        state = owned.State();
        Check(state[Frontend::Phase] == 4 && state[Frontend::CleanupComplete] && !state[Frontend::CleanupPending] && !state[Frontend::ServicesRetained] &&
                  !state[Frontend::GuiBorrowed] && !state[Frontend::ReleaseFault],
              "frontend teardown releases Core services and borrowed GUI before session release");
        Check(owned.shutdown(owned.frontend) == Frontend::Status::Ok, "completed frontend shutdown is idempotent");
        void* released = owned.frontend;
        Check(owned.release(owned.frontend) == Frontend::Status::Ok, "release fully stopped real frontend owner");
        owned.frontend = nullptr;
        Check(owned.read(released, sentinel.data(), 32) == Frontend::Status::InvalidArgument && sentinel == unchanged,
              "released frontend handle is rejected without touching output");
        if (mode == L"gated")
        {
            Check(owned.renderer->Shutdown() == GuiD3D12Status::Busy && GetModuleHandleW(L"cgui.dll"),
                  "CPU menu teardown does not free queued GUI GPU versions or its module lease");
            Hr(owned.gate->Signal(1), "release only owned GPU gate");
            owned.blocked = false;
            owned.Wait();
        }
        else if (mode == L"normal")
        {
            owned.frontend = owned.create(Frontend::ApiVersion, owned.gui);
            Check(owned.frontend && owned.State()[Frontend::Phase] == 2, "normal teardown permits reacquiring real Core frontend on the same GUI");
            Check(owned.release(owned.frontend) == Frontend::Status::Ok, "release reacquired frontend through its normal shutdown path");
            owned.frontend = nullptr;
        }
        CheckIsolation();
        Check(!GetModuleHandleW(L"xmll.dll"), "real XML module unloads after frontend ownership ends");
        Check(owned.renderer->Shutdown() == GuiD3D12Status::Ok && owned.renderer->Release() == GuiD3D12Status::Ok,
              "actual GUI session drains and releases after its Core frontend");
        owned.renderer = nullptr;
        owned.gui = nullptr;
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
            Check(errors == 0, "frontend creation and teardown produce no actual D3D12 debug errors");
        }
        Check(FreeLibrary(owned.cgui) != FALSE, "unload drained actual cgui");
        owned.cgui = nullptr;
        Check(FreeLibrary(owned.core) != FALSE, "unload stopped actual Core");
        owned.core = nullptr;
        Check(!GetModuleHandleW(L"core.dll") && !GetModuleHandleW(L"cgui.dll"), "no real Core/GUI module survives the owned host lifecycle");
        std::ofstream result(root / L"frontend-result.json");
        result << "{\"schema\":\"mta_owned_frontend_host_v1\",\"success\":true,\"checks\":" << checks + 1 << ",\"terminal\":[";
        for (std::size_t index = 0; index < state.size(); ++index)
            result << (index ? "," : "") << state[index];
        result << "],\"fullMtaReady\":false,\"gameLaunched\":false}\n";
        Check(result.good(), "save actual frontend terminal result");
        std::printf("Passed %u actual MTA frontend host checks\n", checks);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "MTA frontend host failed after %u checks: %s\n", checks, error.what());
        return 1;
    }
}
