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
#include <iterator>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
#include <SharedUtil.Defines.h>
#include <SString.h>
#include <gui/CGUI.h>
#include <gui/CGUID3D12.h>
#include <core/CKeyBindsInterface.h>
#include "ObservedFrontendGui.h"
#include "../../Client/core/DefinitiveFrontend.h"
#include "../../Client/core/DefinitiveFrontendStartup.h"
#include "../../Client/core/DefinitivePersistentFrontend.h"
#include "../../Client/core/DefinitiveFrontendConsole.h"
#include "../../Client/core/DefinitiveFrontendBindings.h"
#include "../../Client/core/DefinitiveClockReference.h"
#include "../../Client/core/DefinitiveFrontendClockReference.h"
#include "../../Client/core/DefinitiveFrontendRuntime.h"
#include "../../Client/core/DefinitiveLocalInputRuntime.h"
#include "../../Client/core/DefinitiveStartupGameTime.h"
#include <game/CGameTimeService.h>

using Microsoft::WRL::ComPtr;
namespace Frontend = DefinitiveFrontend;
namespace Startup = DefinitiveFrontendStartup;
namespace Persistent = DefinitivePersistentFrontend;
namespace Console = DefinitiveFrontendConsole;
namespace Bindings = DefinitiveFrontendBindings;
namespace ClockReference = DefinitiveClockReference;
namespace FrontendClock = DefinitiveFrontendClockReference;
namespace Runtime = DefinitiveFrontendRuntime;
namespace LocalInput = DefinitiveLocalInputRuntime;
namespace GameTime = DefinitiveStartupGameTime;
using mta::gui::GuiD3D12Status;
static_assert(Frontend::ApiVersion == 1 && Frontend::FieldCount == 32);
static_assert(Startup::ApiVersion == 1 && Startup::FieldCount == 32);
static_assert(Persistent::ApiVersion == 1 && Persistent::FieldCount == 288);
static_assert(Console::ApiVersion == 1 && Console::FieldCount == 32 && Bindings::ApiVersion == 1 && Bindings::FieldCount == 32);
static_assert(ClockReference::ApiVersion == 1 && ClockReference::FieldCount == 96);
static_assert(FrontendClock::ApiVersion == 1 && Startup::FieldCount == 32);
static_assert(Runtime::ApiVersion == 1 && Runtime::FieldCount == 64 && Runtime::StartupFieldCount == 32);
static_assert(LocalInput::ApiVersion == 1 && LocalInput::FieldCount == 32 && sizeof(LocalInput::RequestV1) == 24);
static_assert(GameTime::ApiVersion == 1 && GameTime::FieldCount == 16);

namespace
{
    unsigned                 checks = 0;
    bool                     settingsVerified = false, settingsCaptureRetainedForCleanup = false;
    unsigned                 cancelCalls = 0, publishCalls = 0, heartbeatCalls = 0;
    unsigned                 clockCancelCalls = 0, clockPublishCalls = 0;
    bool                     clockPublicationShapeValid = true, clockAdmissionVerified = false, clockWrongImageRefusalVerified = false;
    bool                     runtimeCase = false, runtimeRetentionVerified = false, runtimeFailureCleanupVerified = false;
    bool                     localInputAdmissionVerified = false, localInputWrongImageVerified = false;
    Runtime::Snapshot        runtimeTerminal{};
    ClockReference::Snapshot clockSnapshot{};
    bool                     CancelClock()
    {
        ++clockCancelCalls;
        return false;
    }
    void PublishClock(const std::uint32_t* values, std::uint32_t count)
    {
        ++clockPublishCalls;
        if (!values || count != clockSnapshot.size())
        {
            clockPublicationShapeValid = false;
            return;
        }
        std::copy_n(values, clockSnapshot.size(), clockSnapshot.begin());
    }
    bool CancelView()
    {
        ++cancelCalls;
        return false;
    }
    void PublishView(const std::uint32_t*, std::uint32_t)
    {
        ++publishCalls;
    }
    bool HostHeartbeat()
    {
        ++heartbeatCalls;
        return true;
    }
    Persistent::RequestV1 PersistentRequest()
    {
        Persistent::RequestV1 request;
        // High bits must survive the real C ABI even though the script owner
        // currently issues only uint32 generation tokens through the bridge.
        request.generation = UINT64_C(0x100000002);
        request.cancelled = &CancelView;
        request.publish = &PublishView;
        request.heartbeat = &HostHeartbeat;
        return request;
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
        static constexpr UINT                Width = 1280, Height = 1024;
        ComPtr<ID3D12Device>                 device;
        ComPtr<ID3D12CommandQueue>           queue;
        ComPtr<ID3D12CommandAllocator>       allocator;
        ComPtr<ID3D12GraphicsCommandList>    list;
        ComPtr<ID3D12Resource>               target, readback;
        ComPtr<ID3D12Fence>                  fence, gate;
        ComPtr<ID3D12DescriptorHeap>         rtv;
        ComPtr<ID3D12InfoQueue>              info;
        D3D12_PLACED_SUBRESOURCE_FOOTPRINT   footprint{};
        HANDLE                               event = nullptr;
        UINT64                               nextFence = 1;
        bool                                 blocked = false;
        HMODULE                              core = nullptr, cgui = nullptr;
        mta::gui::IGuiD3D12Session*          renderer = nullptr;
        CGUI*                                gui = nullptr;
        std::unique_ptr<ObservedFrontendGui> observed;
        void*                                frontend = nullptr;
        void*                                startup = nullptr;
        Startup::Begin                       beginStartup = nullptr;
        Startup::Pulse                       pulseStartup = nullptr;
        Startup::BindInput                   bindStartup = nullptr;
        Startup::Read                        readStartup = nullptr;
        Startup::Shutdown                    shutdownStartup = nullptr;
        Startup::Release                     releaseStartup = nullptr;
        Startup::RunView                     runView = nullptr;
        FrontendClock::Run                   runClock = nullptr;
        Persistent::Run                      runPersistent = nullptr;
        Runtime::Begin                       beginRuntime = nullptr;
        Runtime::Read                        readRuntime = nullptr;
        Runtime::Pulse                       pulseRuntime = nullptr;
        Runtime::RunView                     runRuntimeView = nullptr;
        Runtime::Shutdown                    shutdownRuntime = nullptr;
        Runtime::Release                     releaseRuntime = nullptr;
        LocalInput::Configure                configureLocal = nullptr;
        LocalInput::Pulse                    pulseLocal = nullptr;
        LocalInput::Read                     readLocal = nullptr;
        LocalInput::Suspend                  suspendLocal = nullptr;
        LocalInput::Disable                  disableLocal = nullptr;
        GameTime::Read                       readGameTime = nullptr;
        HWND                                 localWindow = nullptr;
        bool                                 runtimeOwner = false;
        Frontend::Create                     create = nullptr;
        Frontend::Pulse                      pulse = nullptr;
        Frontend::Read                       read = nullptr;
        Frontend::Shutdown                   shutdown = nullptr;
        Frontend::Release                    release = nullptr;

        ~Owned()
        {
            if (blocked && gate)
                gate->Signal(UINT64_MAX - 1);
            if (frontend && release && release(frontend) == Frontend::Status::Ok)
                frontend = nullptr;
            if (!frontend && startup)
            {
                const auto releaseOwner = runtimeOwner ? releaseRuntime : releaseStartup;
                if (releaseOwner && releaseOwner(startup) == Frontend::Status::Ok)
                    startup = nullptr;
            }
            if (!startup && localWindow)
                DestroyWindow(localWindow);
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
    void PersistentArgumentChecks(Owned& owned)
    {
        const auto request = PersistentRequest();
        const auto before = StartupState(owned);
        Check(owned.runPersistent(nullptr, &request) == Frontend::Status::InvalidArgument &&
                  owned.runPersistent(reinterpret_cast<void*>(std::uintptr_t{1}), &request) == Frontend::Status::InvalidArgument &&
                  owned.runPersistent(owned.startup, nullptr) == Frontend::Status::InvalidArgument,
              "persistent API refuses missing request and unknown owner identities before invoking callbacks");
        const auto invalid = [&](const Persistent::RequestV1& value, const char* why)
        { Check(owned.runPersistent(owned.startup, &value) == Frontend::Status::InvalidArgument, why); };
        auto changed = request;
        changed.size = sizeof(request) - 1;
        invalid(changed, "persistent API refuses a truncated request size");
        changed = request;
        changed.size = sizeof(request) + 1;
        invalid(changed, "persistent API refuses an extended request size without a matching version");
        changed = request;
        changed.version = 0;
        invalid(changed, "persistent API refuses a missing request version");
        changed.version = Persistent::ApiVersion + 1;
        invalid(changed, "persistent API refuses an unsupported request version");
        changed = request;
        changed.generation = 0;
        invalid(changed, "persistent API refuses a zero generation token");
        changed = request;
        changed.reserved[0] = 1;
        invalid(changed, "persistent API refuses first reserved request value");
        changed = request;
        changed.reserved[1] = 1;
        invalid(changed, "persistent API refuses second reserved request value");
        changed = request;
        changed.cancelled = nullptr;
        invalid(changed, "persistent API requires an explicit cancellation callback");
        changed = request;
        changed.publish = nullptr;
        invalid(changed, "persistent API requires independent value publication");
        changed = request;
        changed.heartbeat = nullptr;
        invalid(changed, "persistent API requires the independently renewed host lifetime callback");
        Check(StartupState(owned) == before && !cancelCalls && !publishCalls && !heartbeatCalls,
              "malformed persistent requests leave real Core/backend lifecycle and every caller callback untouched");
    }
    void ClockArgumentChecks(Owned& owned)
    {
        const auto before = StartupState(owned);
        const auto cancelled = clockCancelCalls, published = clockPublishCalls;
        const auto values = clockSnapshot;
        Check(owned.runClock(nullptr, 1000, &CancelClock, &PublishClock) == Frontend::Status::InvalidArgument &&
                  owned.runClock(reinterpret_cast<void*>(std::uintptr_t{1}), 1000, &CancelClock, &PublishClock) == Frontend::Status::InvalidArgument,
              "completed-owner clock rejects absent and unknown handles before callbacks or backend work");
        Check(owned.runClock(owned.startup, 0, &CancelClock, &PublishClock) == Frontend::Status::InvalidArgument &&
                  owned.runClock(owned.startup, ClockReference::MaximumDurationMs + 1, &CancelClock, &PublishClock) == Frontend::Status::InvalidArgument &&
                  owned.runClock(owned.startup, 1000, nullptr, &PublishClock) == Frontend::Status::InvalidArgument &&
                  owned.runClock(owned.startup, 1000, &CancelClock, nullptr) == Frontend::Status::InvalidArgument,
              "completed-owner clock refuses zero or excessive duration and absent cancellation/publication callbacks");
        Check(StartupState(owned) == before && clockCancelCalls == cancelled && clockPublishCalls == published && clockSnapshot == values,
              "invalid clock requests preserve actual Startup32 values, reserved fields and callback-owned storage");
        clockAdmissionVerified = true;
    }
    void ClockRejected(Owned& owned, Frontend::Status expected, const char* why)
    {
        const auto before = StartupState(owned);
        const auto cancelled = clockCancelCalls, published = clockPublishCalls;
        const auto values = clockSnapshot;
        Check(owned.runClock(owned.startup, 1000, &CancelClock, &PublishClock) == expected, why);
        // Rejected admission must not borrow the retained Core or replace its
        // existing GUI owner, including a stopped but unreleased frontend.
        Check(StartupState(owned) == before && clockCancelCalls == cancelled && clockPublishCalls == published && clockSnapshot == values,
              "clock admission refusal preserves completed Core/backend ownership and invokes no caller callback");
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
        result << "],\"persistentApiVersion\":1,\"persistentFields\":288,\"persistentRequestAdmissionVerified\":" << (!runtimeCase ? "true" : "false")
               << ",\"persistentWrongImageRefusalVerified\":"
               << (startup[Startup::LastFailure] == static_cast<std::uint32_t>(Startup::Failure::InputBind) ? "true" : "false")
               << ",\"completedOwnerClockApiVersion\":1,\"completedOwnerClockFields\":96,\"completedOwnerClockAdmissionVerified\":"
               << (clockAdmissionVerified ? "true" : "false")
               << ",\"completedOwnerClockWrongImageRefusalVerified\":" << (clockWrongImageRefusalVerified ? "true" : "false")
               << ",\"completedOwnerClockNativeWorkloadVerified\":false,\"completedOwnerClockPublishedSamples\":" << clockPublishCalls
               << ",\"actualSessionBindingsSettingsVerified\":" << (settingsVerified ? "true" : "false")
               << ",\"settingsCaptureRetainedForCleanup\":" << (settingsCaptureRetainedForCleanup ? "true" : "false")
               << ",\"runtimeApiVersion\":1,\"runtimeFields\":64,\"runtimeCase\":" << (runtimeCase ? "true" : "false")
               << ",\"runtimeRetentionVerified\":" << (runtimeRetentionVerified ? "true" : "false")
               << ",\"runtimeFailureCleanupVerified\":" << (runtimeFailureCleanupVerified ? "true" : "false")
               << ",\"localInputApiVersion\":1,\"localInputFields\":32,\"localInputAdmissionVerified\":" << (localInputAdmissionVerified ? "true" : "false")
               << ",\"localInputWrongImageVerified\":" << (localInputWrongImageVerified ? "true" : "false") << ",\"localInputNativeMovementVerified\":false"
               << ",\"runtimeNativeViewVerified\":false,\"runtimeTerminal\":[";
        for (std::size_t index = 0; index < runtimeTerminal.size(); ++index)
            result << (index ? "," : "") << runtimeTerminal[index];
        result << "]"
               << ",\"persistentNativeLeaseStarted\":false,\"persistentVisibleRenderingVerified\":false,\"nativeEngineBindingVerified\":false,\"fullMtaReady\":"
                  "false,\"gameLaunched\":false}\n";
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

    void SessionBindingsSettings(Owned& owned, const std::filesystem::path& root, bool retainCapture)
    {
        auto& observed = *owned.observed;
        auto* window = observed.settingsWindow;
        auto* grid = observed.settingsGrid;
        auto* tabs = observed.settingsTabs;
        std::printf("Settings observer root=%p grid=%p tabs=%p apply=%p cancel=%p buttons=%zu\n", static_cast<void*>(window), static_cast<void*>(grid),
                    static_cast<void*>(tabs), static_cast<void*>(observed.settingsApply), static_cast<void*>(observed.settingsCancel),
                    observed.settingsButtons.size());
        Check(window != nullptr, "actual configured bindings constructed the existing SETTINGS window");
        Check(grid && tabs && observed.settingsApply && observed.settingsCancel,
              "actual configured bindings own a complete existing grid, tab and session buttons");
        Check(!window->IsVisible(), "actual configured bindings settings window remains initially hidden");
        Check(tabs->GetTabCount() == 1 && tabs->GetSelectedTab()->GetText() == "Binds" && grid->GetColumnCount() == 5 &&
                  observed.settingsButtons == std::vector<std::string>{"Apply (session only)", "Cancel"},
              "only the existing Binds grid and session Apply/Cancel are constructed; unavailable tabs and defaults stay absent");
        const auto open = [&]
        {
            Click(owned, 230, 716);
            Check(window->IsVisible() && owned.State()[Frontend::InputRequested] && !owned.State()[Frontend::CloseRequested],
                  "real Settings menu image opens the existing bindings window and retains menu input ownership");
        };
        const auto clickButton = [&](CGUIButton* button)
        {
            CVector2D point = button->GetSize() / 2;
            for (auto* element = static_cast<CGUIElement*>(button); element; element = element->GetParent())
                point += element->GetPosition();
            Click(owned, static_cast<int>(point.fX), static_cast<int>(point.fY));
        };
        const auto process = Export<Console::ProcessMessage>(owned.core, "MTA_ProcessDEFrontendConsoleMessage");
        const auto message = [&](UINT type, WPARAM key, LPARAM detail = 0)
        {
            std::uint32_t handled = UINT32_MAX;
            Check(process(owned.frontend, type, key, detail, &handled) == Frontend::Status::Ok && handled <= 1,
                  "actual frontend routes owned keyboard/focus input through the existing Core message API");
            return handled != 0;
        };
        const auto consoleHidden = [&]
        {
            Console::Snapshot state{};
            Check(Export<Console::Read>(owned.core, "MTA_ReadDEFrontendConsole")(owned.frontend, state.data(), Console::FieldCount) == Frontend::Status::Ok &&
                      !state[Console::ConsoleVisible] && state[Console::CommandsRegistered] == 2 && state[Console::CommandsExecuted] == 2,
                  "settings capture preserves the existing hidden Help/Clear console without adding commands");
        };
        constexpr int                             descriptionColumn = 1, primaryColumn = 2;
        const std::array<std::string, 6>          controls{"forwards", "backwards", "left", "right", "sprint", "walk"};
        const std::array<std::string, 6>          replacement{"i", "k", "j", "l", "u", "o"};
        const std::array<WPARAM, 6>               keys{'I', 'K', 'J', 'L', 'U', 'O'};
        std::array<int, 6>                        rows{};
        std::array<CGTAControlBind*, 6>           realBinds{};
        std::array<std::string, 6>                original{};
        std::array<std::array<std::string, 3>, 6> originalAlternates{};
        const auto                                findRows = [&]
        {
            rows.fill(-1);
            unsigned count = 0;
            for (int row = 0; row < grid->GetRowCount(); ++row)
            {
                const auto type = static_cast<KeyBindType>(reinterpret_cast<std::intptr_t>(grid->GetItemData(row, descriptionColumn)));
                if (type != KeyBindType::GTA_CONTROL)
                    continue;
                auto* bind = static_cast<CGTAControlBind*>(grid->GetItemData(row, primaryColumn));
                Check(bind && bind->type == KeyBindType::GTA_CONTROL && bind->boundKey && bind->control,
                      "existing grid borrows a real configured MTA control bind rather than fixture data");
                const auto action = std::find(controls.begin(), controls.end(), std::string(bind->control->szControl));
                Check(action != controls.end(), "settings lists only one of the six admitted movement controls");
                const auto index = static_cast<std::size_t>(action - controls.begin());
                Check(rows[index] == -1, "each admitted movement control occupies exactly one existing grid row");
                rows[index] = row;
                realBinds[index] = bind;
                ++count;
            }
            Check(count == controls.size() && grid->GetRowCount() == 7,
                  "actual session settings include six movement rows and their single noneditable section header");
        };
        const auto beginCapture = [&](std::size_t index, int column = 2)
        {
            grid->SetSelectedItem(rows[index], column, true);
            Check(grid->GetSelectedItemRow() == rows[index] && grid->GetSelectedItemColumn() == column - 1,
                  "actual existing grid selects the requested movement binding cell");
            Check(grid->Event_OnDoubleClick(), "existing grid dispatches its real production key-capture callback");
        };
        open();
        findRows();
        Save(root / L"frontend-settings.bmp", Raster(owned));
        for (std::size_t index = 0; index < controls.size(); ++index)
        {
            original[index] = realBinds[index]->boundKey->szKey;
            for (int alternate = 0; alternate < 3; ++alternate)
                originalAlternates[index][alternate] = grid->GetItemText(rows[index], 3 + alternate);
            Check(grid->GetItemText(rows[index], primaryColumn) == original[index], "initial grid text matches its actual CKeyBinds record");
            beginCapture(index);
            if (index == 0)
                Check(!message(WM_LBUTTONUP, 0), "the initiating mouse release reaches the surface translator while settings capture remains armed");
            Check(message(WM_KEYDOWN, keys[index]) && grid->GetItemText(rows[index], primaryColumn) == replacement[index] &&
                      std::string(realBinds[index]->boundKey->szKey) == original[index],
                  "real key capture changes only the staged cell until session Apply");
            message(WM_KEYUP, keys[index]);
        }
        clickButton(observed.settingsApply);
        Check(!window->IsVisible(), "actual session Apply callback closes the existing settings window");
        for (std::size_t index = 0; index < controls.size(); ++index)
            Check(std::string(realBinds[index]->boundKey->szKey) == replacement[index],
                  "session Apply changes the real MTA manager's configured movement binding");
        open();
        findRows();
        for (std::size_t index = 0; index < controls.size(); ++index)
            Check(grid->GetItemText(rows[index], primaryColumn) == replacement[index],
                  "reopening reloads applied movement keys from the actual retained MTA binding manager");
        beginCapture(0);
        Check(message(WM_KEYDOWN, 'P') && grid->GetItemText(rows[0], primaryColumn) == std::string("p"),
              "another real key can be staged before testing Cancel");
        message(WM_KEYUP, 'P');
        clickButton(observed.settingsCancel);
        Check(!window->IsVisible() && std::string(realBinds[0]->boundKey->szKey) == replacement[0],
              "actual Cancel closes the dialog without applying staged edits to real MTA bindings");
        open();
        findRows();
        Check(grid->GetItemText(rows[0], primaryColumn) == replacement[0], "Cancel discards its staged key when the grid reloads");
        beginCapture(0);
        Check(message(WM_KEYDOWN, VK_ESCAPE) && window->IsVisible() && grid->GetItemText(rows[0], primaryColumn) != replacement[0] &&
                  std::string(realBinds[0]->boundKey->szKey) == replacement[0] && !owned.State()[Frontend::CloseRequested],
              "Escape clears only the staged captured cell and cannot close the trial or mutate the manager");
        message(WM_KEYUP, VK_ESCAPE);
        clickButton(observed.settingsCancel);
        open();
        findRows();
        beginCapture(0);
        Check(message(WM_KEYDOWN, VK_F8), "a settings capture consumes F8 before the existing console shortcut");
        message(WM_KEYUP, VK_F8);
        consoleHidden();
        Check(grid->GetItemText(rows[0], primaryColumn) == replacement[0] && message(WM_KEYDOWN, 'I'),
              "reserved F8 leaves capture active so a subsequent bindable key completes the real capture");
        message(WM_KEYUP, 'I');
        clickButton(observed.settingsCancel);
        open();
        findRows();
        beginCapture(0);
        message(WM_KILLFOCUS, 0);
        Check(!message(WM_KEYDOWN, 'P') && grid->GetItemText(rows[0], primaryColumn) == replacement[0] &&
                  std::string(realBinds[0]->boundKey->szKey) == replacement[0],
              "focus loss cancels modal capture without changing either the staged cell or actual configured key");
        message(WM_KEYUP, 'P');
        consoleHidden();
        beginCapture(0);
        open();
        findRows();
        Check(!message(WM_KEYDOWN, 'P') && grid->GetItemText(rows[0], primaryColumn) == replacement[0] &&
                  std::string(realBinds[0]->boundKey->szKey) == replacement[0],
              "opening settings again retires capture before rebuilding rows and cannot write through its old selected cell");
        message(WM_KEYUP, 'P');
        beginCapture(0);
        Check(message(WM_KEYDOWN, VK_ESCAPE), "real capture stages removal of the configured primary movement key");
        message(WM_KEYUP, VK_ESCAPE);
        const std::string cleared = grid->GetItemText(rows[0], primaryColumn);
        for (int alternate = 0; alternate < 3; ++alternate)
            grid->SetItemText(rows[0], 3 + alternate, cleared.c_str());
        beginCapture(0, 5);
        Check(message(WM_KEYDOWN, 'P') && grid->GetItemText(rows[0], 5) == std::string("p"),
              "the same staged edit captures a new alternate key after staging removal of the old configured keys");
        message(WM_KEYUP, 'P');
        clickButton(observed.settingsApply);
        Check(!window->IsVisible(), "primary removal and alternate replacement finish through the real session Apply callback");
        // Apply may retire the old primary CKeyBind. Reopen and obtain the new
        // borrowed grid records before observing manager ownership again.
        open();
        findRows();
        Check(grid->GetItemText(rows[0], primaryColumn) == std::string("p") && std::string(realBinds[0]->boundKey->szKey) == "p",
              "clearing primary and adding an alternate retains a valid real control record after manager removal");
        // Restore the disposable session through the actual Apply callback. No
        // configuration file or native control is needed to observe ownership.
        for (std::size_t index = 0; index < controls.size(); ++index)
        {
            grid->SetItemText(rows[index], primaryColumn, original[index].c_str());
            for (int alternate = 0; alternate < 3; ++alternate)
                grid->SetItemText(rows[index], 3 + alternate, originalAlternates[index][alternate].c_str());
        }
        clickButton(observed.settingsApply);
        for (std::size_t index = 0; index < controls.size(); ++index)
            Check(std::string(realBinds[index]->boundKey->szKey) == original[index],
                  "session Apply restores the disposable host's real original movement bindings");
        if (retainCapture)
        {
            open();
            findRows();
            beginCapture(0);
            Check(message(WM_CHAR, 'P'), "a live capture owns text immediately before actual frontend teardown");
            settingsCaptureRetainedForCleanup = true;
        }
        CheckIsolation(true);
        settingsVerified = true;
    }

    Runtime::Snapshot RuntimeState(Owned& owned)
    {
        Runtime::Snapshot state{};
        Check(owned.readRuntime(owned.startup, state.data(), Runtime::FieldCount) == Frontend::Status::Ok,
              "read actual frontend Runtime64 without native admission");
        const auto startup = StartupState(owned);
        Check(std::equal(startup.begin(), startup.end(), state.begin()) && state[Runtime::RuntimeVersion] == 1 &&
                  state[Runtime::OwnerThread] == GetCurrentThreadId() && (state[Runtime::SessionGenerationLow] || state[Runtime::SessionGenerationHigh]) &&
                  !state[Runtime::Reserved59] && !state[Runtime::Reserved63] && !state[Runtime::NetworkStarted] && !state[Runtime::GameplayReady] &&
                  !state[Runtime::FullMtaReady],
              "Runtime64 preserves exact Startup32 and exposes a separate frontend-only session identity");
        return state;
    }
    void GameTimeArguments(Owned& owned)
    {
        GameTime::Snapshot sentinel;
        sentinel.fill(0x73614825);
        const auto unchanged = sentinel;
        for (const auto count : {0u, 15u, 17u})
            Check(owned.readGameTime(owned.startup, sentinel.data(), count) == Frontend::Status::InvalidArgument && sentinel == unchanged,
                  "time16 read rejects a foreign field count without modifying caller bytes");
        Check(owned.readGameTime(owned.startup, nullptr, GameTime::FieldCount) == Frontend::Status::InvalidArgument &&
                  owned.readGameTime(nullptr, sentinel.data(), GameTime::FieldCount) == Frontend::Status::InvalidArgument && sentinel == unchanged &&
                  owned.readGameTime(reinterpret_cast<void*>(std::uintptr_t{1}), sentinel.data(), GameTime::FieldCount) == Frontend::Status::InvalidArgument &&
                  sentinel == unchanged,
              "time16 read rejects missing output and unknown completed owner identities");
        bool        foreign = false;
        std::thread other(
            [&]
            {
                auto output = unchanged;
                foreign = owned.readGameTime(owned.startup, output.data(), GameTime::FieldCount) == Frontend::Status::WrongThread && output == unchanged;
            });
        other.join();
        Check(foreign, "time16 foreign thread cannot read native time or replace caller output");
    }

    void GameTimeUnavailable(Owned& owned, mta::game_de::Status nativeStatus)
    {
        const auto           before = StartupState(owned);
        const auto           beforeRuntime = owned.runtimeOwner ? RuntimeState(owned) : Runtime::Snapshot{};
        const auto           beforeView = owned.frontend ? owned.State() : Frontend::Snapshot{};
        LocalInput::Snapshot beforeLocal{};
        if (owned.runtimeOwner)
            Check(owned.readLocal(owned.startup, beforeLocal.data(), LocalInput::FieldCount) == Frontend::Status::Ok,
                  "capture local32 before optional time read");
        GameTime::Snapshot output;
        // Poison every word on each call: unavailable native reads must write
        // an explicit empty sample rather than leave caller bytes or stale time.
        output.fill(0x61247853);
        Check(owned.readGameTime(owned.startup, output.data(), GameTime::FieldCount) == Frontend::Status::Ok && output[GameTime::Version] == 1 &&
                  output[GameTime::OwnerThread] == GetCurrentThreadId() && output[GameTime::CoreRetained] == 1 &&
                  output[GameTime::BackendStarted] == before[Startup::BackendStarted] && !output[GameTime::SampleAvailable] &&
                  output[GameTime::NativeStatus] == static_cast<std::uint32_t>(nativeStatus) &&
                  std::all_of(output.begin() + GameTime::NativeProducerThread, output.end(), [](auto value) { return value == 0; }),
              "time16 uses the retained Core owner and reports unavailable native evidence with all sample and reserved fields zero");
        Check(
            StartupState(owned) == before && (!owned.runtimeOwner || RuntimeState(owned) == beforeRuntime) && (!owned.frontend || owned.State() == beforeView),
            "explicit native time read leaves Startup32 Runtime64 GUI32 ownership and gameplay capabilities unchanged");
        if (owned.runtimeOwner)
        {
            LocalInput::Snapshot afterLocal{};
            Check(owned.readLocal(owned.startup, afterLocal.data(), LocalInput::FieldCount) == Frontend::Status::Ok && afterLocal == beforeLocal,
                  "read-only native time neither admits nor retires the same configured local-input owner");
        }
    }

    void GameTimeStale(Owned& owned, void* stale)
    {
        GameTime::Snapshot output;
        output.fill(0x52813746);
        const auto unchanged = output;
        Check(owned.readGameTime(stale, output.data(), GameTime::FieldCount) == Frontend::Status::InvalidArgument && output == unchanged,
              "released completed Core identity cannot publish a cached native time sample");
    }

    std::uint64_t RuntimePair(const Runtime::Snapshot& state, Runtime::Field field)
    {
        return std::uint64_t{state[field]} | (std::uint64_t{state[field + 1]} << 32);
    }
    void RuntimeHidden(const Runtime::Snapshot& state, unsigned views)
    {
        Check(state[Startup::Phase] == 2 && state[Startup::CoreRetained] && state[Startup::BackendStarted] && !state[Startup::FrontendRetained] &&
                  !state[Startup::GuiBorrowed] && !state[Startup::ServicesRetained] && state[Startup::LastViewCleanupComplete] &&
                  state[Runtime::RuntimePhase] == static_cast<std::uint32_t>(Runtime::Phase::Hidden) && state[Runtime::ServicesInitialized] &&
                  state[Runtime::ServicesRetained] && state[Runtime::XmlRetained] && state[Runtime::ConfigRetained] && state[Runtime::CVarsLoaded] &&
                  state[Runtime::BindingsRetained] && state[Runtime::BindingManagerCreations] == 1 && state[Runtime::ConfigurationLoads] == 1 &&
                  state[Runtime::BindingLoads] == 1 && state[Runtime::ViewsAttached] == views && state[Runtime::ViewsRetired] == views &&
                  !state[Runtime::ViewBorrowed] && state[Runtime::HiddenNeutralVerified] && state[Runtime::ConfigurationReadOnly] &&
                  !state[Runtime::CleanupPending] && !state[Runtime::CleanupComplete],
              "hidden runtime retains exactly one real XML/CVars/bindings owner with neutral view aliases");
    }

    std::uintptr_t RuntimeSettings(Owned& owned, bool secondView, std::uintptr_t previousIdentity)
    {
        auto& observed = *owned.observed;
        auto* grid = observed.settingsGrid;
        auto* window = observed.settingsWindow;
        Check(window && grid && observed.settingsApply && observed.settingsCancel,
              "fresh runtime view constructs actual Settings widgets over retained MTA bindings");
        const auto process = Export<Console::ProcessMessage>(owned.core, "MTA_ProcessDEFrontendConsoleMessage");
        const auto message = [&](UINT type, WPARAM key)
        {
            std::uint32_t handled = UINT32_MAX;
            Check(process(owned.frontend, type, key, 0, &handled) == Frontend::Status::Ok && handled <= 1,
                  "runtime Settings uses the real frontend message and key-capture route");
            return handled != 0;
        };
        const auto button = [&](CGUIButton* value)
        {
            CVector2D point = value->GetSize() / 2;
            for (auto* element = static_cast<CGUIElement*>(value); element; element = element->GetParent())
                point += element->GetPosition();
            Click(owned, static_cast<int>(point.fX), static_cast<int>(point.fY));
        };
        const auto open = [&]
        {
            Click(owned, 230, 716);
            Check(window->IsVisible(), "runtime menu opens its genuine Settings window");
        };
        const auto forwards = [&]()
        {
            int row = -1;
            for (int index = 0; index < grid->GetRowCount(); ++index)
            {
                const auto type = static_cast<KeyBindType>(reinterpret_cast<std::intptr_t>(grid->GetItemData(index, 1)));
                if (type != KeyBindType::GTA_CONTROL)
                    continue;
                auto* bind = static_cast<CGTAControlBind*>(grid->GetItemData(index, 2));
                Check(bind && bind->control && bind->boundKey, "runtime grid borrows an actual configured control record");
                if (std::strcmp(bind->control->szControl, "forwards") == 0)
                {
                    Check(row == -1, "one retained Forwards record appears in runtime Settings");
                    row = index;
                }
            }
            Check(row >= 0, "runtime view exposes configured Forwards from its real manager");
            return row;
        };
        const auto capture = [&](int row, WPARAM key)
        {
            grid->SetSelectedItem(row, 2, true);
            Check(grid->Event_OnDoubleClick() && message(WM_KEYDOWN, key), "real runtime capture consumes the staged binding key");
            message(WM_KEYUP, key);
        };
        open();
        int row = forwards();
        if (!secondView)
        {
            capture(row, 'I');
            Check(grid->GetItemText(row, 2) == std::string("i"), "first runtime view stages Forwards I");
            button(observed.settingsApply);
            Check(!window->IsVisible(), "first runtime view applies through the actual Settings callback");
            open();
            row = forwards();
        }
        auto* bind = static_cast<CGTAControlBind*>(grid->GetItemData(row, 2));
        Check(grid->GetItemText(row, 2) == std::string("i") && std::string(bind->boundKey->szKey) == "i",
              "reopened runtime Settings reads applied I from the retained actual manager");
        const auto identity = reinterpret_cast<std::uintptr_t>(bind);
        if (secondView)
        {
            // An opaque comparison is safe while the first widget is gone.
            // Dereference only the freshly borrowed second-view grid record.
            Check(identity == previousIdentity, "fresh second GUI view borrows the identical retained MTA control record");
            capture(row, 'O');
            Check(grid->GetItemText(row, 2) == std::string("o") && std::string(bind->boundKey->szKey) == "i",
                  "second runtime view stages O without changing actual retained I");
            button(observed.settingsCancel);
            open();
            row = forwards();
            bind = static_cast<CGTAControlBind*>(grid->GetItemData(row, 2));
            Check(grid->GetItemText(row, 2) == std::string("i") && std::string(bind->boundKey->szKey) == "i",
                  "second-view Cancel discards O across an actual grid rebuild");
            grid->SetSelectedItem(row, 2, true);
            Check(grid->Event_OnDoubleClick() && message(WM_CHAR, 'P'), "runtime hide retains a real modal capture for teardown coverage");
            settingsCaptureRetainedForCleanup = true;
        }
        else
            button(observed.settingsCancel);
        return identity;
    }

    void RuntimeSessionCase(Owned& owned, const std::filesystem::path& root, const std::wstring& mode, Startup::Snapshot& startup, Frontend::Snapshot& frontend)
    {
        runtimeCase = true;
        owned.runtimeOwner = true;
        Check(Export<std::uint32_t (*)()>(owned.core, "MTA_DEFrontendRuntimeApiVersion")() == 1,
              "actual Core exposes optional runtime API1 without changing old field counts");
        owned.beginRuntime = Export<Runtime::Begin>(owned.core, "MTA_BeginDEFrontendRuntime");
        owned.readRuntime = Export<Runtime::Read>(owned.core, "MTA_ReadDEFrontendRuntime");
        owned.pulseRuntime = Export<Runtime::Pulse>(owned.core, "MTA_PulseDEFrontendRuntime");
        owned.runRuntimeView = Export<Runtime::RunView>(owned.core, "MTA_RunDEFrontendRuntimeView");
        owned.shutdownRuntime = Export<Runtime::Shutdown>(owned.core, "MTA_ShutdownDEFrontendRuntime");
        owned.releaseRuntime = Export<Runtime::Release>(owned.core, "MTA_ReleaseDEFrontendRuntime");
        Check(Export<std::uint32_t (*)()>(owned.core, "MTA_DELocalInputRuntimeApiVersion")() == 1, "actual Core exposes separate local-input API1/32");
        owned.configureLocal = Export<LocalInput::Configure>(owned.core, "MTA_ConfigureDELocalInputRuntime");
        owned.pulseLocal = Export<LocalInput::Pulse>(owned.core, "MTA_PulseDELocalInputRuntime");
        owned.readLocal = Export<LocalInput::Read>(owned.core, "MTA_ReadDELocalInputRuntime");
        owned.suspendLocal = Export<LocalInput::Suspend>(owned.core, "MTA_SuspendDELocalInputRuntime");
        owned.disableLocal = Export<LocalInput::Disable>(owned.core, "MTA_DisableDELocalInputRuntime");
        Check(!owned.beginRuntime(0) && !owned.beginRuntime(2), "runtime rejects foreign API versions before creating Core");
        owned.startup = owned.beginRuntime(1);
        Check(owned.startup != nullptr, "runtime publishes the same completed Core owner even on partial preparation failure");
        const auto initial = RuntimeState(owned);
        GameTimeArguments(owned);
        GameTimeUnavailable(owned, initial[Startup::Phase] == 2 ? mta::game_de::Status::Unsupported : mta::game_de::Status::NotInitialized);
        const auto localState = [&]
        {
            LocalInput::Snapshot values{};
            Check(owned.readLocal(owned.startup, values.data(), LocalInput::FieldCount) == Frontend::Status::Ok,
                  "read separate actual Core local-input32 values");
            return values;
        };
        const auto absentLocal = localState();
        Check(absentLocal[LocalInput::Version] == 1 && absentLocal[LocalInput::Phase] == static_cast<std::uint32_t>(LocalInput::State::Disabled) &&
                  absentLocal[LocalInput::OwnerThread] == GetCurrentThreadId() && absentLocal[LocalInput::CleanupComplete] &&
                  !absentLocal[LocalInput::Configured] && !absentLocal[LocalInput::Enabled] && !absentLocal[LocalInput::NativeAttempted] &&
                  !absentLocal[LocalInput::NativeActive] && !absentLocal[LocalInput::NativeBound] && !absentLocal[LocalInput::CleanupPending] &&
                  !absentLocal[LocalInput::CanonicalNeutral],
              "unconfigured local service reports absence without inventing a canonical neutral readback or native ownership");
        Check(owned.suspendLocal(owned.startup) == Frontend::Status::Ok && owned.disableLocal(owned.startup) == Frontend::Status::Ok &&
                  localState() == absentLocal,
              "absent-service Suspend and Disable are safe no-ops on a retained or failed runtime owner");
        LocalInput::Snapshot localSentinel;
        localSentinel.fill(0x37516824);
        const auto            localUnchanged = localSentinel;
        const auto            unknownOwner = reinterpret_cast<void*>(std::uintptr_t{1});
        LocalInput::RequestV1 localRequest;
        localRequest.window = reinterpret_cast<void*>(std::uintptr_t{1});
        Check(owned.readLocal(owned.startup, localSentinel.data(), 31) == Frontend::Status::InvalidArgument && localSentinel == localUnchanged &&
                  owned.readLocal(owned.startup, nullptr, 32) == Frontend::Status::InvalidArgument &&
                  owned.readLocal(unknownOwner, localSentinel.data(), 32) == Frontend::Status::InvalidArgument && localSentinel == localUnchanged &&
                  owned.configureLocal(unknownOwner, &localRequest) == Frontend::Status::InvalidArgument &&
                  owned.pulseLocal(unknownOwner) == Frontend::Status::InvalidArgument &&
                  owned.suspendLocal(unknownOwner) == Frontend::Status::InvalidArgument &&
                  owned.disableLocal(unknownOwner) == Frontend::Status::InvalidArgument &&
                  owned.configureLocal(owned.startup, nullptr) == Frontend::Status::InvalidArgument,
              "malformed local reads and unknown owner operations preserve caller bytes and cannot adopt another Core");
        for (unsigned field = 0; field != 5; ++field)
        {
            auto malformedLocal = localRequest;
            if (field == 0)
                malformedLocal.size = 0;
            if (field == 1)
                malformedLocal.version = 0;
            if (field == 2)
                malformedLocal.window = nullptr;
            if (field == 3)
                malformedLocal.reserved[0] = 1;
            if (field == 4)
                malformedLocal.reserved[1] = 1;
            Check(owned.configureLocal(owned.startup, &malformedLocal) == Frontend::Status::InvalidArgument && localState() == absentLocal,
                  "malformed local configuration never allocates a producer or changes its absent snapshot");
        }
        bool        foreignLocal = false;
        std::thread localOther(
            [&]
            {
                auto output = localUnchanged;
                foreignLocal = owned.readLocal(owned.startup, output.data(), LocalInput::FieldCount) == Frontend::Status::WrongThread &&
                               output == localUnchanged && owned.configureLocal(owned.startup, &localRequest) == Frontend::Status::WrongThread &&
                               owned.pulseLocal(owned.startup) == Frontend::Status::WrongThread &&
                               owned.suspendLocal(owned.startup) == Frontend::Status::WrongThread &&
                               owned.disableLocal(owned.startup) == Frontend::Status::WrongThread;
            });
        localOther.join();
        Check(foreignLocal && localState() == absentLocal && RuntimeState(owned) == initial,
              "foreign local service calls cannot read mutate admit or retire the same real Core and binding manager");
        localInputAdmissionVerified = true;
        Check(!owned.beginRuntime(1) && !owned.beginStartup(1), "runtime and historical startup cannot construct competing Core singletons");
        Runtime::Snapshot sentinel;
        sentinel.fill(0x75316824);
        const auto unchanged = sentinel;
        Check(owned.readRuntime(owned.startup, sentinel.data(), 63) == Frontend::Status::InvalidArgument && sentinel == unchanged &&
                  owned.readRuntime(owned.startup, nullptr, 64) == Frontend::Status::InvalidArgument &&
                  owned.readRuntime(reinterpret_cast<void*>(std::uintptr_t{1}), sentinel.data(), 64) == Frontend::Status::InvalidArgument &&
                  sentinel == unchanged,
              "runtime malformed reads preserve caller storage and reject unknown owners");
        auto request = PersistentRequest();
        request.generation = 1;
        bool        foreign = false;
        std::thread other(
            [&]
            {
                auto output = unchanged;
                foreign = owned.readRuntime(owned.startup, output.data(), 64) == Frontend::Status::WrongThread && output == unchanged &&
                          owned.pulseRuntime(owned.startup) == Frontend::Status::WrongThread &&
                          owned.runRuntimeView(owned.startup, &request) == Frontend::Status::WrongThread &&
                          owned.shutdownRuntime(owned.startup) == Frontend::Status::WrongThread &&
                          owned.releaseRuntime(owned.startup) == Frontend::Status::WrongThread;
            });
        other.join();
        Check(foreign && !cancelCalls && !publishCalls && !heartbeatCalls, "runtime foreign calls cannot read mutate start or retire the real owner");
        Check(owned.pulseStartup(owned.startup) == Frontend::Status::InvalidArgument && owned.bindStartup(owned.startup) == Frontend::Status::InvalidArgument &&
                  owned.runView(owned.startup, 100, &CancelView, &PublishView) == Frontend::Status::InvalidArgument &&
                  owned.runPersistent(owned.startup, &request) == Frontend::Status::InvalidArgument &&
                  owned.runClock(owned.startup, 1000, &CancelClock, &PublishClock) == Frontend::Status::InvalidArgument &&
                  owned.shutdownStartup(owned.startup) == Frontend::Status::InvalidArgument &&
                  owned.releaseStartup(owned.startup) == Frontend::Status::InvalidArgument,
              "old mutating owner APIs cannot bypass runtime service ownership or inject a clock prelude");
        Check(!cancelCalls && !publishCalls && !heartbeatCalls && !clockCancelCalls && !clockPublishCalls,
              "runtime mode refusals invoke no native or clock callbacks");
        if (mode != L"runtime")
        {
            Check(initial[Startup::Phase] == 5 && initial[Startup::CoreRetained] &&
                      initial[Runtime::RuntimePhase] == static_cast<std::uint32_t>(Runtime::Phase::Failed) && !initial[Runtime::ViewBorrowed] &&
                      !initial[Runtime::ViewsAttached],
                  "runtime preparation failure preserves an inspectable completed owner without a view");
            Check(owned.runRuntimeView(owned.startup, &request) == Frontend::Status::Failed && !cancelCalls && !publishCalls && !heartbeatCalls,
                  "failed runtime cannot admit native work or GUI ownership");
            runtimeFailureCleanupVerified = true;
        }
        else
        {
            RuntimeHidden(initial, 0);
            const auto session = RuntimePair(initial, Runtime::SessionGenerationLow);
            const auto malformed = [&](Persistent::RequestV1 value)
            {
                const auto before = RuntimeState(owned);
                Check(owned.runRuntimeView(owned.startup, &value) == Frontend::Status::InvalidArgument && RuntimeState(owned) == before && !cancelCalls &&
                          !publishCalls && !heartbeatCalls,
                      "malformed runtime request leaves service owner and callbacks unchanged");
            };
            Check(owned.runRuntimeView(owned.startup, nullptr) == Frontend::Status::InvalidArgument, "runtime view requires an explicit request");
            auto changed = request;
            changed.size = 0;
            malformed(changed);
            changed = request;
            changed.version = 0;
            malformed(changed);
            changed = request;
            changed.generation = 0;
            malformed(changed);
            changed = request;
            changed.generation = 2;
            malformed(changed);
            changed = request;
            changed.reserved[0] = 1;
            malformed(changed);
            changed = request;
            changed.reserved[1] = 1;
            malformed(changed);
            changed = request;
            changed.cancelled = nullptr;
            malformed(changed);
            changed = request;
            changed.publish = nullptr;
            malformed(changed);
            changed = request;
            changed.heartbeat = nullptr;
            malformed(changed);
            Check(owned.runRuntimeView(owned.startup, &request) == Frontend::Status::Failed && !cancelCalls && !publishCalls && !heartbeatCalls,
                  "runtime native view rejects actual wrong image before publication or any borrowed view");
            const auto refused = RuntimeState(owned);
            RuntimeHidden(refused, 0);
            Check(refused[Startup::LastFailure] == static_cast<std::uint32_t>(Startup::Failure::InputBind) && !refused[Startup::ExactImageBound] &&
                      RuntimePair(refused, Runtime::SessionGenerationLow) == session && !refused[Runtime::LastFailure],
                  "safe runtime native preflight refusal preserves the same hidden configured session");
            const auto configBytes = [&]
            {
                std::ifstream stream(root / L"mta/config/coreconfig.xml", std::ios::binary);
                Check(stream.good(), "read disposable configuration for before/after byte proof");
                return std::string(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
            };
            const auto originalConfig = configBytes();
            owned.create = Export<Frontend::Create>(owned.core, "MTA_CreateDEFrontendSession");
            owned.pulse = Export<Frontend::Pulse>(owned.core, "MTA_PulseDEFrontendSession");
            owned.read = Export<Frontend::Read>(owned.core, "MTA_ReadDEFrontendSession");
            owned.shutdown = Export<Frontend::Shutdown>(owned.core, "MTA_ShutdownDEFrontendSession");
            owned.release = Export<Frontend::Release>(owned.core, "MTA_ReleaseDEFrontendSession");
            owned.cgui = LoadLibraryExW((root / L"cgui.dll").c_str(), nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
            Check(owned.cgui != nullptr, "load actual normal cgui for two runtime CPU views");
            const auto wide = root.wstring();
            Check(Export<mta::gui::ConfigureGuiD3D12AssetRoot>(owned.cgui, "ConfigureGUID3D12AssetRoot")(mta::gui::GuiD3D12AssetRootApiVersion, wide.data(),
                                                                                                         static_cast<std::uint32_t>(wide.size())) == 1,
                  "configure runtime fixture assets without touching an installation");
            mta::gui::GuiD3D12CreateInfo info;
            info.device = owned.device.Get();
            info.queue = owned.queue.Get();
            info.targetFormat = DXGI_FORMAT_B8G8R8A8_UNORM;
            info.width = Owned::Width;
            info.height = Owned::Height;
            owned.renderer = Export<mta::gui::CreateGuiD3D12Session>(owned.cgui, "CreateGUID3D12Session")(mta::gui::GuiD3D12ApiVersion, &info);
            Check(owned.renderer && owned.renderer->GetGUI(owned.gui) == GuiD3D12Status::Ok && owned.gui, "runtime borrows an actual owner-thread WARP GUI");
            owned.observed = std::make_unique<ObservedFrontendGui>(*owned.gui);
            owned.gui = owned.observed.get();
            const auto  beforeForeignBorrow = RuntimeState(owned);
            bool        foreignBorrowRejected = false;
            std::thread foreignBorrow([&] { foreignBorrowRejected = owned.create(Frontend::ApiVersion, owned.gui) == nullptr; });
            foreignBorrow.join();
            Check(foreignBorrowRejected && RuntimeState(owned) == beforeForeignBorrow,
                  "foreign direct GUI creation cannot allocate or borrow retained runtime services");
            // A hidden real owned HWND satisfies configuration admission. The
            // actual executable remains the synthetic host, so Pulse must
            // refuse native binding before any engine hook or input lease.
            owned.localWindow = CreateWindowExW(0, L"STATIC", L"MTA local input owned host", WS_OVERLAPPEDWINDOW, 0, 0, 320, 240, nullptr, nullptr,
                                                GetModuleHandleW(nullptr), nullptr);
            Check(owned.localWindow != nullptr, "create an undisplayed real owner-process root window for local configuration");
            localRequest.window = owned.localWindow;
            Check(owned.configureLocal(owned.startup, &localRequest) == Frontend::Status::Ok, "configure real local producer on the same parked MTA bindings");
            const auto arming = localState();
            Check(arming[LocalInput::Phase] == static_cast<std::uint32_t>(LocalInput::State::Arming) && arming[LocalInput::Configured] &&
                      arming[LocalInput::Enabled] && arming[LocalInput::BindingsBorrowed] && arming[LocalInput::CanonicalNeutral] &&
                      arming[LocalInput::CleanupComplete] && !arming[LocalInput::NativeAttempted] && !arming[LocalInput::NativeActive] &&
                      !arming[LocalInput::NativeBound] && arming[LocalInput::NativeLifetimeMs] == 500,
                  "configuration verifies actual canonical neutral history without native Begin or a replacement binding manager");
            const auto beforeLocalBorrow = RuntimeState(owned);
            Check(!owned.create(Frontend::ApiVersion, owned.gui) && RuntimeState(owned) == beforeLocalBorrow && localState() == arming,
                  "enabled local owner excludes a direct GUI borrower before allocating or changing either owner");
            Check(owned.pulseLocal(owned.startup) == Frontend::Status::Failed, "local producer refuses the actual host's wrong game image before native Begin");
            const auto refusedLocal = localState();
            Check(refusedLocal[LocalInput::Phase] == static_cast<std::uint32_t>(LocalInput::State::Failed) &&
                      refusedLocal[LocalInput::LastFailure] == static_cast<std::uint32_t>(LocalInput::Failure::InputBind) &&
                      refusedLocal[LocalInput::Configured] && !refusedLocal[LocalInput::Enabled] && refusedLocal[LocalInput::BindingsBorrowed] &&
                      !refusedLocal[LocalInput::NativeAttempted] && !refusedLocal[LocalInput::NativeGenerationLow] &&
                      !refusedLocal[LocalInput::NativeGenerationHigh] && !refusedLocal[LocalInput::NativeActive] && !refusedLocal[LocalInput::NativeBound] &&
                      !refusedLocal[LocalInput::CleanupPending] && refusedLocal[LocalInput::CleanupComplete] && refusedLocal[LocalInput::CanonicalNeutral] &&
                      RuntimePair(RuntimeState(owned), Runtime::SessionGenerationLow) == session && !RuntimeState(owned)[Startup::ExactImageBound] &&
                      RuntimeState(owned)[Startup::CapabilitiesLow] == beforeLocalBorrow[Startup::CapabilitiesLow],
                  "wrong-image local refusal preserves the same Core and actual neutral history with no fabricated native lease");
            Check(owned.suspendLocal(owned.startup) == Frontend::Status::Ok && localState()[LocalInput::CanonicalNeutral],
                  "qualified local suspension permits subsequent CPU GUI borrowing while preserving historical refusal");
            localInputWrongImageVerified = true;
            std::uintptr_t identity = 0;
            for (unsigned view = 1; view <= 2; ++view)
            {
                owned.observed->ResetSettingsObservation();
                owned.frontend = owned.create(Frontend::ApiVersion, owned.gui);
                Check(owned.frontend && owned.State()[Frontend::Phase] == 2, "runtime creates a fresh actual GUI view on its existing Core");
                GameTimeUnavailable(owned, mta::game_de::Status::Unsupported);
                auto visible = RuntimeState(owned);
                Check(visible[Runtime::RuntimePhase] == static_cast<std::uint32_t>(Runtime::Phase::Visible) && visible[Runtime::ViewBorrowed] &&
                          visible[Runtime::ViewsAttached] == view && visible[Runtime::ViewsRetired] == view - 1 &&
                          RuntimePair(visible, Runtime::ViewGenerationLow) == view && RuntimePair(visible, Runtime::SessionGenerationLow) == session,
                      "fresh CPU view borrows the same runtime with one actual view generation");
                Check(owned.shutdownRuntime(owned.startup) == Frontend::Status::Busy && owned.releaseRuntime(owned.startup) == Frontend::Status::Busy,
                      "runtime refuses service and backend destruction behind a live GUI borrower");
                const auto beforeLocalBusy = localState();
                Check(owned.configureLocal(owned.startup, &localRequest) == Frontend::Status::Busy &&
                          owned.pulseLocal(owned.startup) == Frontend::Status::Busy && owned.suspendLocal(owned.startup) == Frontend::Status::Busy &&
                          owned.disableLocal(owned.startup) == Frontend::Status::Busy && localState() == beforeLocalBusy,
                      "live actual GUI borrower excludes local configuration polling and binding-history cleanup");
                // Exercise the real fade to completion before clicking the
                // existing menu; a fixed short frame count is scheduler-dependent.
                const auto settleUntil = GetTickCount64() + 750;
                do
                {
                    Frame(owned, 4, 4);
                    Sleep(15);
                } while (GetTickCount64() < settleUntil);
                Check(owned.State()[Frontend::MenuVisible] && !owned.State()[Frontend::MenuFading],
                      "fresh runtime menu completes its actual fade before Settings interaction");
                ConsoleAndBindings(owned);
                identity = RuntimeSettings(owned, view == 2, identity);
                Save(root / (view == 1 ? L"runtime-first-view.bmp" : L"runtime-second-view.bmp"), Raster(owned));
                Check(owned.shutdown(owned.frontend) == Frontend::Status::Ok, "runtime hide retires actual widgets and their active settings capture");
                frontend = owned.State();
                Bindings::Snapshot stopped{};
                Check(frontend[Frontend::CleanupComplete] && !frontend[Frontend::ServicesRetained] && !frontend[Frontend::GuiBorrowed] &&
                          Export<Bindings::Read>(owned.core, "MTA_ReadDEFrontendBindings")(owned.frontend, stopped.data(), 32) == Frontend::Status::Ok &&
                          stopped[Bindings::CleanupComplete] && !stopped[Bindings::BindingsRetained] && stopped[Bindings::CurrentZero] &&
                          stopped[Bindings::PreviousZero],
                      "old stopped-view fields retain their zero-alias meaning while session objects remain parked");
                Check(owned.shutdownRuntime(owned.startup) == Frontend::Status::Busy && owned.releaseRuntime(owned.startup) == Frontend::Status::Busy,
                      "stopped but unreleased GUI still excludes runtime final retirement");
                const auto stale = owned.frontend;
                Check(owned.release(owned.frontend) == Frontend::Status::Ok, "actual GUI Release completes a runtime view without retiring session services");
                owned.frontend = nullptr;
                Frontend::Snapshot output{};
                output.fill(0x58135724);
                const auto previous = output;
                Check(owned.read(stale, output.data(), 32) == Frontend::Status::InvalidArgument && output == previous,
                      "retired runtime view cannot expose freed widgets or service aliases");
                const auto hidden = RuntimeState(owned);
                RuntimeHidden(hidden, view);
                Check(RuntimePair(hidden, Runtime::SessionGenerationLow) == session && GetModuleHandleW(L"xmll.dll") && configBytes() == originalConfig,
                      "hidden runtime preserves same XML owner, configuration bytes and Core session identity");
                const auto pulses = hidden[Startup::Pulses];
                Check(owned.pulseRuntime(owned.startup) == Frontend::Status::Ok && RuntimeState(owned)[Startup::Pulses] == pulses + 1,
                      "same actual backend remains usable between runtime views");
                if (view == 1)
                {
                    Check(owned.configureLocal(owned.startup, &localRequest) == Frontend::Status::Ok && !owned.create(Frontend::ApiVersion, owned.gui),
                          "explicit local resume after view retirement borrows the same applied binding manager and excludes another GUI");
                    Check(owned.disableLocal(owned.startup) == Frontend::Status::Ok, "Disable retires armed local ownership before the second real GUI view");
                    const auto disabled = localState();
                    Check(disabled[LocalInput::Phase] == static_cast<std::uint32_t>(LocalInput::State::Disabled) && !disabled[LocalInput::Configured] &&
                              !disabled[LocalInput::Enabled] && !disabled[LocalInput::BindingsBorrowed] && !disabled[LocalInput::NativeAttempted] &&
                              !disabled[LocalInput::CleanupPending] && disabled[LocalInput::CleanupComplete] && disabled[LocalInput::CanonicalNeutral] &&
                              RuntimeState(owned)[Runtime::BindingManagerCreations] == 1 && RuntimeState(owned)[Runtime::ConfigurationLoads] == 1 &&
                              RuntimeState(owned)[Runtime::BindingLoads] == 1,
                          "disabled local producer leaves no native debt and retains one actual manager/configuration for the second I/CancelO view");
                    Check(owned.suspendLocal(owned.startup) == Frontend::Status::Ok && localState() == disabled,
                          "Suspend after Disable remains idempotently Disabled with unchanged qualified neutral values");
                }
            }
            request.generation = 2;
            Check(owned.runRuntimeView(owned.startup, &request) == Frontend::Status::InvalidArgument && !cancelCalls && !publishCalls && !heartbeatCalls,
                  "retired runtime view generation cannot be reused for native input");
            Check(configBytes() == originalConfig, "both runtime views leave actual configuration file bytes unchanged");
            runtimeRetentionVerified = settingsVerified = true;
        }
        Check(owned.shutdownRuntime(owned.startup) == Frontend::Status::Ok && owned.shutdownRuntime(owned.startup) == Frontend::Status::Ok,
              "runtime final shutdown retires actual manager then CVars/XML before backend, idempotently");
        runtimeTerminal = RuntimeState(owned);
        GameTimeUnavailable(owned, mta::game_de::Status::NotInitialized);
        startup = StartupState(owned);
        Check(runtimeTerminal[Runtime::RuntimePhase] == static_cast<std::uint32_t>(Runtime::Phase::Stopped) && runtimeTerminal[Runtime::CleanupComplete] &&
                  !runtimeTerminal[Runtime::CleanupPending] && !runtimeTerminal[Runtime::ServicesRetained] && !runtimeTerminal[Runtime::XmlRetained] &&
                  !runtimeTerminal[Runtime::ConfigRetained] && !runtimeTerminal[Runtime::CVarsLoaded] && !runtimeTerminal[Runtime::BindingsRetained] &&
                  !runtimeTerminal[Runtime::ViewBorrowed] && startup[Startup::Phase] == 4 && startup[Startup::CoreRetained] &&
                  !startup[Startup::BackendStarted] && !GetModuleHandleW(L"xmll.dll"),
              "stopped runtime proves real session service and backend retirement while keeping its final value handle");
        Check(owned.pulseRuntime(owned.startup) == Frontend::Status::Stopped && owned.runRuntimeView(owned.startup, &request) == Frontend::Status::Stopped &&
                  !cancelCalls && !publishCalls && !heartbeatCalls,
              "stopped runtime cannot reopen a native view or renew a host lease");
        const auto stoppedLocal = localState();
        Check(!stoppedLocal[LocalInput::Configured] && !stoppedLocal[LocalInput::Enabled] && !stoppedLocal[LocalInput::BindingsBorrowed] &&
                  !stoppedLocal[LocalInput::NativeActive] && !stoppedLocal[LocalInput::NativeBound] && !stoppedLocal[LocalInput::CleanupPending] &&
                  stoppedLocal[LocalInput::CleanupComplete],
              "runtime final shutdown leaves the separate local producer retired before its real bindings and backend disappear");
        if (owned.gui)
            Check(!owned.create(Frontend::ApiVersion, owned.gui) && RuntimeState(owned) == runtimeTerminal,
                  "stopped runtime closes direct GUI borrowing before releasing its completed Core");
        const auto stale = owned.startup;
        Check(owned.releaseRuntime(owned.startup) == Frontend::Status::Ok, "release stopped actual runtime Core once all borrowed owners retired");
        owned.startup = nullptr;
        GameTimeStale(owned, stale);
        Check(owned.readRuntime(stale, sentinel.data(), 64) == Frontend::Status::InvalidArgument && sentinel == unchanged &&
                  owned.releaseRuntime(stale) == Frontend::Status::InvalidArgument,
              "stale runtime identity preserves output and cannot release another Core");
        Check(owned.readLocal(stale, localSentinel.data(), 32) == Frontend::Status::InvalidArgument && localSentinel == localUnchanged &&
                  owned.suspendLocal(stale) == Frontend::Status::InvalidArgument && owned.disableLocal(stale) == Frontend::Status::InvalidArgument,
              "stale local input identity preserves output and cannot retire another completed Core");
        CheckIsolation(false);
        if (owned.renderer)
        {
            Check(owned.renderer->Shutdown() == GuiD3D12Status::Ok && owned.renderer->Release() == GuiD3D12Status::Ok,
                  "actual runtime fixture renderer drains after both CPU views");
            owned.renderer = nullptr;
            owned.gui = nullptr;
        }
        if (owned.cgui)
        {
            Check(FreeLibrary(owned.cgui) != FALSE, "unload drained runtime fixture cgui");
            owned.cgui = nullptr;
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
            Check(errors == 0, "retained Core and two actual runtime GUI views emit no D3D12 debug errors");
        }
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
        Check(root.is_absolute() && (mode == L"normal" || mode == L"gated" || mode == L"frontend-failure" || mode == L"backend-failure" || mode == L"runtime" ||
                                     mode == L"runtime-frontend-failure" || mode == L"runtime-backend-failure"),
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
        Check(Export<std::uint32_t (*)()>(owned.core, "MTA_DEStartupGameTimeApiVersion")() == GameTime::ApiVersion,
              "normal Core exposes additive native time16 without changing startup runtime or GUI protocols");
        owned.readGameTime = Export<GameTime::Read>(owned.core, "MTA_ReadDEStartupGameTimeV1");
        Check(Export<std::uint32_t (*)()>(owned.core, "MTA_DEFrontendClockReferenceApiVersion")() == FrontendClock::ApiVersion,
              "normal Core exposes additive completed-owner clock API1 without widening Startup32 or Read96");
        owned.runClock = Export<FrontendClock::Run>(owned.core, "MTA_RunDEFrontendClockReference");
        Check(Export<std::uint32_t (*)()>(owned.core, "MTA_DEPersistentFrontendApiVersion")() == Persistent::ApiVersion,
              "normal Core exposes separate renewable frontend API1 without changing completed startup or bounded trials");
        owned.runPersistent = Export<Persistent::Run>(owned.core, "MTA_RunDEPersistentFrontendV1");
        Check(!owned.beginStartup(0) && !owned.beginStartup(Startup::ApiVersion + 1) && !owned.beginStartup(Startup::ApiVersion),
              "wrong versions and missing explicit directories cannot construct a frontend Core owner");
        Check(!Export<Runtime::Begin>(owned.core, "MTA_BeginDEFrontendRuntime")(Runtime::ApiVersion),
              "runtime also requires explicit directories before constructing any Core or configuration owner");
        CheckIsolation(false);
        const auto wide = root.wstring();
        const auto gameWide = root.parent_path().parent_path().wstring();
        setRoot(wide.data(), wide.size());
        Check(!owned.beginStartup(Startup::ApiVersion), "missing GTA directory still refuses completed Core creation after MTA root is supplied");
        setGameRoot(gameWide.data(), gameWide.size());
        Check(!owned.beginStartup(0) && !owned.beginStartup(Startup::ApiVersion + 1),
              "wrong frontend startup versions remain rejected with both explicit directories ready");
        if (mode.rfind(L"runtime", 0) == 0)
        {
            Startup::Snapshot  startup{};
            Frontend::Snapshot frontend{};
            RuntimeSessionCase(owned, root, mode, startup, frontend);
            bool legacyUnchanged = true;
            for (std::uint32_t index = 0; index < legacy.size(); ++index)
                legacyUnchanged = legacyUnchanged && readLegacy(index) == legacy[index];
            Check(legacyUnchanged, "runtime services and both actual views preserve historical Startup8 diagnostics");
            Check(FreeLibrary(owned.core) != FALSE, "unload explicitly released actual runtime Core");
            owned.core = nullptr;
            Check(!GetModuleHandleW(L"core.dll") && !GetModuleHandleW(L"cgui.dll") && !GetModuleHandleW(L"game_de.dll") && !GetModuleHandleW(L"xmll.dll"),
                  "no actual runtime Core GUI XML or backend module survives final retirement");
            SaveResult(root, startup, frontend);
            std::printf("Passed %u actual MTA frontend startup host checks\n", checks);
            return 0;
        }
        owned.startup = owned.beginStartup(Startup::ApiVersion);
        Check(owned.startup != nullptr, "allocated startup publishes completed Core ownership even if backend preparation fails");
        auto startup = StartupState(owned);
        GameTimeArguments(owned);
        GameTimeUnavailable(owned, startup[Startup::Phase] == 2 ? mta::game_de::Status::Unsupported : mta::game_de::Status::NotInitialized);
        PersistentArgumentChecks(owned);
        ClockArgumentChecks(owned);
        const auto persistentRequest = PersistentRequest();
        Check(Export<std::uint32_t (*)()>(owned.core, "MTA_DEFrontendRuntimeApiVersion")() == 1,
              "normal historical owner retains the independently versioned runtime export");
        Runtime::Snapshot runtimeSentinel;
        runtimeSentinel.fill(0x24687531);
        const auto runtimeUnchanged = runtimeSentinel;
        Check(
            !Export<Runtime::Begin>(owned.core, "MTA_BeginDEFrontendRuntime")(1) &&
                Export<Runtime::Read>(owned.core, "MTA_ReadDEFrontendRuntime")(owned.startup, runtimeSentinel.data(), 64) ==
                    Frontend::Status::InvalidArgument &&
                runtimeSentinel == runtimeUnchanged &&
                Export<Runtime::Pulse>(owned.core, "MTA_PulseDEFrontendRuntime")(owned.startup) == Frontend::Status::InvalidArgument &&
                Export<Runtime::RunView>(owned.core, "MTA_RunDEFrontendRuntimeView")(owned.startup, &persistentRequest) == Frontend::Status::InvalidArgument &&
                Export<Runtime::Shutdown>(owned.core, "MTA_ShutdownDEFrontendRuntime")(owned.startup) == Frontend::Status::InvalidArgument &&
                Export<Runtime::Release>(owned.core, "MTA_ReleaseDEFrontendRuntime")(owned.startup) == Frontend::Status::InvalidArgument,
            "runtime API refuses historical startup handles without adoption replacement or output mutation");
        LocalInput::Snapshot historicalLocal;
        historicalLocal.fill(0x35724681);
        const auto            historicalLocalUnchanged = historicalLocal;
        LocalInput::RequestV1 historicalLocalRequest;
        historicalLocalRequest.window = reinterpret_cast<void*>(std::uintptr_t{1});
        Check(Export<LocalInput::Read>(owned.core, "MTA_ReadDELocalInputRuntime")(owned.startup, historicalLocal.data(), LocalInput::FieldCount) ==
                      Frontend::Status::InvalidArgument &&
                  historicalLocal == historicalLocalUnchanged &&
                  Export<LocalInput::Configure>(owned.core, "MTA_ConfigureDELocalInputRuntime")(owned.startup, &historicalLocalRequest) ==
                      Frontend::Status::InvalidArgument &&
                  Export<LocalInput::Pulse>(owned.core, "MTA_PulseDELocalInputRuntime")(owned.startup) == Frontend::Status::InvalidArgument &&
                  Export<LocalInput::Suspend>(owned.core, "MTA_SuspendDELocalInputRuntime")(owned.startup) == Frontend::Status::InvalidArgument &&
                  Export<LocalInput::Disable>(owned.core, "MTA_DisableDELocalInputRuntime")(owned.startup) == Frontend::Status::InvalidArgument,
              "local input runtime cannot adopt historical startup handles or change their output and ownership");
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
                          owned.runView(owned.startup, 100, &CancelView, &PublishView) == Frontend::Status::WrongThread &&
                          owned.runClock(owned.startup, 1000, &CancelClock, &PublishClock) == Frontend::Status::WrongThread &&
                          owned.runPersistent(owned.startup, &persistentRequest) == Frontend::Status::WrongThread;
            });
        other.join();
        Check(foreign && !clockCancelCalls && !clockPublishCalls,
              "foreign thread cannot read, bind, pulse, stop, release or start clock/persistent work on the completed Core owner");
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
            owned.observed = std::make_unique<ObservedFrontendGui>(*owned.gui);
            owned.gui = owned.observed.get();
        };
        Frontend::Snapshot state{};
        if (mode == L"backend-failure")
        {
            ClockRejected(owned, Frontend::Status::Failed, "failed backend owner rejects completed-owner clock before invoking callbacks");
            Check(startup[Startup::Phase] == 5 && startup[Startup::CoreRetained] && startup[Startup::BackendStartAttempted] &&
                      !startup[Startup::BackendStarted] && startup[Startup::LastFailure],
                  "missing actual Game DE publishes a failed handle retaining its completed Core for explicit cleanup");
            CheckIsolation(false);
            Check(owned.runPersistent(owned.startup, &persistentRequest) == Frontend::Status::Failed && !cancelCalls && !publishCalls && !heartbeatCalls &&
                      !StartupState(owned)[Startup::ViewRuns],
                  "failed backend owner refuses persistent native work without invoking callbacks or inventing a view");
            initializeGui();
            owned.observed->ResetSettingsObservation();
            Check(!owned.create(Frontend::ApiVersion, owned.gui) && !StartupState(owned)[Startup::FrontendRetained],
                  "a failed backend Core cannot admit a borrowed frontend or leave a partial GUI owner");
        }
        else
        {
            Check(startup[Startup::Phase] == 2 && startup[Startup::CoreRetained] && startup[Startup::BackendStartAttempted] &&
                      startup[Startup::BackendStarted] && !startup[Startup::ExactImageBound] && startup[Startup::Pulses] >= 1,
                  "Begin completes a real Core and initializes its actual backend without native engine binding");
            CheckIsolation(true);
            const auto beforeClock = StartupState(owned);
            Check(owned.runClock(owned.startup, 1000, &CancelClock, &PublishClock) == Frontend::Status::Failed && !clockCancelCalls && !clockPublishCalls,
                  "completed-owner clock refuses actual wrong-image backend binding before native Begin or publication");
            const auto afterClock = StartupState(owned);
            Check(afterClock[Startup::Phase] == 2 && afterClock[Startup::CoreRetained] && afterClock[Startup::BackendStarted] &&
                      !afterClock[Startup::ExactImageBound] && afterClock[Startup::Pulses] == beforeClock[Startup::Pulses] &&
                      afterClock[Startup::CapabilitiesLow] == beforeClock[Startup::CapabilitiesLow] &&
                      afterClock[Startup::CapabilitiesHigh] == beforeClock[Startup::CapabilitiesHigh] &&
                      afterClock[Startup::ViewRuns] == beforeClock[Startup::ViewRuns] && afterClock[Startup::LastViewCleanupComplete] &&
                      afterClock[Startup::LastFailure] == static_cast<std::uint32_t>(Startup::Failure::InputBind),
                  "safe clock preflight refusal keeps actual completed Core phase2, initialized backend and existing Startup32 semantics");
            clockWrongImageRefusalVerified = true;
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
            const auto beforePersistent = StartupState(owned);
            Check(owned.runPersistent(owned.startup, &persistentRequest) == Frontend::Status::Failed && !cancelCalls && !publishCalls && !heartbeatCalls,
                  "real persistent API refuses the owned host's wrong image before callbacks, native lease or GUI work");
            startup = StartupState(owned);
            Check(startup == beforePersistent && startup[Startup::Phase] == 2 && startup[Startup::CoreRetained] && startup[Startup::BackendStarted] &&
                      !startup[Startup::ExactImageBound] && !startup[Startup::ViewRuns] && startup[Startup::LastViewCleanupComplete] &&
                      startup[Startup::LastFailure] == static_cast<std::uint32_t>(Startup::Failure::InputBind),
                  "persistent wrong-image refusal retains exact completed Core/backend capabilities and admits no view ownership");
            CheckIsolation(true);
            initializeGui();
            const auto  beforeForeignBorrow = StartupState(owned);
            bool        foreignBorrowRejected = false;
            std::thread foreignBorrow([&] { foreignBorrowRejected = owned.create(Frontend::ApiVersion, owned.gui) == nullptr; });
            foreignBorrow.join();
            Check(foreignBorrowRejected && StartupState(owned) == beforeForeignBorrow,
                  "foreign-thread direct frontend creation cannot allocate or alter the completed Core before owner admission");
            const auto empty = Raster(owned, true);
            Check(std::all_of(empty.begin(), empty.end(), [&](auto value) { return value == empty.front(); }), "empty actual GUI target is uniform");
            owned.observed->ResetSettingsObservation();
            owned.frontend = owned.create(Frontend::ApiVersion, owned.gui);
            Check(owned.frontend != nullptr, "actual borrowed frontend publishes a handle even on XML startup failure");
            GameTimeUnavailable(owned, mta::game_de::Status::Unsupported);
            state = owned.State();
            startup = StartupState(owned);
            Check(startup[Startup::FrontendRetained] && startup[Startup::FrontendPhase] == state[Frontend::Phase] && startup[Startup::CoreRetained] &&
                      !state[Frontend::OwnsCoreShell] && state[Frontend::AdapterStarted],
                  "existing frontend borrows the already completed Core and its initialized backend");
            Check(owned.shutdownStartup(owned.startup) == Frontend::Status::Busy && owned.releaseStartup(owned.startup) == Frontend::Status::Busy,
                  "live or failed borrowed frontend prevents completed Core destruction");
            Check(owned.runView(owned.startup, 100, &CancelView, &PublishView) == Frontend::Status::Busy && !cancelCalls && !publishCalls,
                  "retained frontend prevents a second bounded view from replacing its Core/GUI ownership");
            Check(owned.runPersistent(owned.startup, &persistentRequest) == Frontend::Status::Busy && !cancelCalls && !publishCalls && !heartbeatCalls,
                  "retained actual frontend prevents persistent input from replacing existing Core/GUI ownership");
            ClockRejected(owned, Frontend::Status::Busy, "retained actual frontend prevents completed-owner clock from borrowing Core concurrently");
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
                SessionBindingsSettings(owned, root, mode == L"gated");
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
            ClockRejected(owned, Frontend::Status::Busy, "stopped but unreleased frontend still excludes completed-owner clock admission");
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
                owned.observed->ResetSettingsObservation();
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
        GameTimeUnavailable(owned, mta::game_de::Status::NotInitialized);
        Check(startup[Startup::Phase] == 4 && startup[Startup::CoreRetained] && !startup[Startup::BackendStarted] && startup[Startup::CleanupComplete] &&
                  !startup[Startup::CleanupPending] && !startup[Startup::ReleaseFault],
              "stopped completed Core is retained for final read and explicit release");
        Check(owned.shutdownStartup(owned.startup) == Frontend::Status::Ok && owned.pulseStartup(owned.startup) == Frontend::Status::Stopped &&
                  owned.bindStartup(owned.startup) == Frontend::Status::Stopped,
              "completed shutdown is idempotent and blocks new backend work");
        Check(owned.runPersistent(owned.startup, &persistentRequest) == Frontend::Status::Stopped && !cancelCalls && !publishCalls && !heartbeatCalls,
              "stopped completed Core refuses persistent work without renewing a host lifetime or native lease");
        ClockRejected(owned, Frontend::Status::Stopped, "stopped completed Core rejects new clock work without callbacks");
        owned.observed->ResetSettingsObservation();
        Check(!owned.create(Frontend::ApiVersion, owned.gui) && !StartupState(owned)[Startup::FrontendRetained],
              "stopped completed Core rejects a new borrowed frontend before final release");
        const auto staleStartup = owned.startup;
        Check(owned.releaseStartup(owned.startup) == Frontend::Status::Ok, "release the completed stopped Core owner");
        owned.startup = nullptr;
        GameTimeStale(owned, staleStartup);
        Check(owned.readStartup(staleStartup, sentinel.data(), 32) == Frontend::Status::InvalidArgument && sentinel == unchanged,
              "released completed Core handle cannot access freed singleton state");
        Check(owned.runPersistent(staleStartup, &persistentRequest) == Frontend::Status::InvalidArgument && !cancelCalls && !publishCalls && !heartbeatCalls,
              "retired completed Core identity cannot start persistent work or access freed state");
        Check(owned.runClock(staleStartup, 1000, &CancelClock, &PublishClock) == Frontend::Status::InvalidArgument && !clockCancelCalls && !clockPublishCalls &&
                  clockPublicationShapeValid,
              "retired completed Core identity cannot start clock work or publish through stale callbacks");
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
