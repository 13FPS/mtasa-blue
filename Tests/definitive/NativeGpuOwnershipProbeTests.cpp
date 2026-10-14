/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Own-process COM identity/ref-balance and query fault coverage
 *****************************************************************************/
#include "../../Client/game_de/NativeD3D12OwnershipProbe.h"
#include <windows.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <wrl/client.h>
#include <cstdio>
#include <stdexcept>

using Microsoft::WRL::ComPtr;
using namespace mta::game_de;
namespace
{
    unsigned checks = 0;
    void     Check(bool value, const char* why)
    {
        ++checks;
        if (!value)
            throw std::runtime_error(why);
    }
    std::uint32_t Number(GpuOwnershipQueryStatus value)
    {
        return static_cast<std::uint32_t>(value);
    }
    GpuOwnershipProbeState Fresh()
    {
        GpuOwnershipProbeState state;
        state.nativeExplicitGPUCount = state.nativeAFRCount = 1;
        return state;
    }
    struct FaultQueries final : GpuOwnershipQueries
    {
        unsigned     fail = 0, calls = 0, closes = 0, acquired = 0;
        bool         exception = false, closeFails = false, closeException = false, unsupported = false;
        std::int32_t Execute(GpuOwnershipQueryStage stage, GpuOwnershipProbeState& state) override
        {
            ++calls;
            const auto value = static_cast<unsigned>(stage);
            if (value == fail)
            {
                if (exception)
                    RaiseException(EXCEPTION_ACCESS_VIOLATION, 0, 0, nullptr);
                return E_NOINTERFACE;
            }
            if (value == 4 || value == 5 || value == 7 || value == 8 || value == 9 || value == 10 || value == 13 || value == 15 || value == 16)
                ++acquired;
            state.deviceNodeCount = 1;
            state.canonicalIdentityFlags = unsupported ? 1u : 7u;
            state.swapchainBufferCount = 2;
            state.swapchainWidth = 640;
            state.backbufferWidth = 640;
            state.swapchainHeight = state.backbufferHeight = 480;
            state.swapchainFormat = state.backbufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
            state.backbufferSampleCount = 1;
            return S_OK;
        }
        bool Close() noexcept override
        {
            ++closes;
            acquired = 0;
            if (closeException)
                RaiseException(EXCEPTION_ACCESS_VIOLATION, 0, 0, nullptr);
            return !closeFails;
        }
    };
    ULONG References(IUnknown* value)
    {
        value->AddRef();
        return value->Release();
    }
    struct NullQuery final : IUnknown
    {
        HRESULT STDMETHODCALLTYPE QueryInterface(REFIID, void** output) override
        {
            *output = nullptr;
            return S_OK;
        }
        ULONG STDMETHODCALLTYPE AddRef() override { return 1; }
        ULONG STDMETHODCALLTYPE Release() override { return 1; }
    };
}
int main()
{
    HWND window = nullptr;
    try
    {
        for (unsigned stage = 4; stage <= 16; ++stage)
        {
            for (const bool exception : {false, true})
            {
                FaultQueries query;
                query.fail = stage;
                query.exception = exception;
                auto state = Fresh();
                state.metadataValid = state.supportPredicate = state.canonicalIdentityFlags = 1;
                RunGpuOwnershipQueries(query, state);
                Check(query.closes == 1 && query.acquired == 0, "Every failed stage must release its acquired prefix exactly once");
                Check(
                    state.queryStage == stage && state.queryStatus == Number(exception ? GpuOwnershipQueryStatus::Exception : GpuOwnershipQueryStatus::Failed),
                    "Failure stage and exception provenance must survive cleanup");
                Check(!state.metadataValid && !state.supportPredicate && state.hresult != 0, "Failed queries cannot reuse successful metadata");
            }
        }
        for (unsigned mode = 0; mode < 4; ++mode)
        {
            FaultQueries query;
            query.unsupported = mode == 1;
            query.closeFails = mode == 2;
            query.closeException = mode == 3;
            auto state = Fresh();
            RunGpuOwnershipQueries(query, state);
            Check(query.closes == 1 && !query.acquired, "Close must run once for successful, unsupported and faulty close transactions");
            if (mode < 2)
                Check(state.metadataValid && state.queryStage == 19 &&
                          state.queryStatus == Number(mode ? GpuOwnershipQueryStatus::Unsupported : GpuOwnershipQueryStatus::Accepted),
                      "Fully queried unsupported metadata is distinct from failed COM");
            else
                Check(state.queryStatus == Number(GpuOwnershipQueryStatus::CleanupFailed) && state.queryStage == 18 && !state.metadataValid &&
                          !state.supportPredicate,
                      "Close failures must retain priority over successful metadata");
        }
        WNDCLASSW klass{};
        klass.lpfnWndProc = DefWindowProcW;
        klass.hInstance = GetModuleHandleW(nullptr);
        klass.lpszClassName = L"MtaGpuOwnershipFixture";
        Check(RegisterClassW(&klass) != 0, "Owned hidden window class must register");
        window =
            CreateWindowW(klass.lpszClassName, L"Owned GPU query fixture", WS_OVERLAPPEDWINDOW, 0, 0, 640, 480, nullptr, nullptr, klass.hInstance, nullptr);
        Check(window != nullptr, "Owned hidden window must exist");
        ComPtr<IDXGIFactory4> factory;
        ComPtr<IDXGIAdapter>  warp;
        ComPtr<ID3D12Device>  device;
        Check(SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&factory))), "System DXGI factory must create");
        Check(SUCCEEDED(factory->EnumWarpAdapter(IID_PPV_ARGS(&warp))), "Owned WARP adapter must enumerate");
        Check(SUCCEEDED(D3D12CreateDevice(warp.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device))), "Owned WARP D3D12 device must create");
        D3D12_COMMAND_QUEUE_DESC   queueDescription{};
        ComPtr<ID3D12CommandQueue> queue;
        Check(SUCCEEDED(device->CreateCommandQueue(&queueDescription, IID_PPV_ARGS(&queue))), "Owned DIRECT queue must create");
        DXGI_SWAP_CHAIN_DESC1 description{};
        description.Width = 640;
        description.Height = 480;
        description.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        description.SampleDesc.Count = 1;
        description.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        description.BufferCount = 2;
        description.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        ComPtr<IDXGISwapChain1> initial;
        ComPtr<IDXGISwapChain3> swapchain;
        Check(SUCCEEDED(factory->CreateSwapChainForHwnd(queue.Get(), window, &description, nullptr, nullptr, &initial)),
              "Owned swapchain must create using actual queue");
        Check(SUCCEEDED(initial.As(&swapchain)), "Actual swapchain3 must be available");
        ComPtr<ID3D12Resource> buffer;
        Check(SUCCEEDED(swapchain->GetBuffer(0, IID_PPV_ARGS(&buffer))), "Owned backbuffer must query");
        const auto deviceRefs = References(device.Get()), queueRefs = References(queue.Get()), swapRefs = References(swapchain.Get()),
                   bufferRefs = References(buffer.Get());
        for (unsigned iteration = 0; iteration < 20; ++iteration)
        {
            auto state = Fresh();
            ProbeD3D12Ownership(swapchain.Get(), queue.Get(), state);
            Check(state.queryStatus == Number(GpuOwnershipQueryStatus::Accepted) && state.queryStage == 19 && state.hresult == 0,
                  "Actual D3D12 swapchain GetDevice and complete identity query must succeed");
            Check(state.metadataValid && state.supportPredicate && state.canonicalIdentityFlags == 7 && state.deviceNodeCount == 1 && state.queueType == 0,
                  "Actual SC, queue and backbuffer must report the same canonical device identity");
            Check(state.backbufferWidth == 640 && state.backbufferHeight == 480 && state.backbufferSampleCount == 1 && state.currentBackbufferIndex < 2,
                  "Metadata must describe the actual own-process backbuffer");
            Check(References(device.Get()) == deviceRefs && References(queue.Get()) == queueRefs && References(swapchain.Get()) == swapRefs &&
                      References(buffer.Get()) == bufferRefs,
                  "Repeated production queries must leave all COM reference counts unchanged");
        }
        // A custom presenter prevents renderer admission, but its presence must
        // not hide metadata from independently borrowed swapchain/queue roots.
        // Only the scalar flag is supplied: no custom-presenter interface exists
        // in this fixture for the ownership transaction to call.
        for (unsigned iteration = 0; iteration < 20; ++iteration)
        {
            auto state = Fresh();
            state.customPresentPresent = 1;
            ProbeD3D12Ownership(swapchain.Get(), queue.Get(), state);
            Check(state.queryStatus == Number(GpuOwnershipQueryStatus::Unsupported) && state.queryStage == 19 && state.hresult == 0,
                  "Custom-present metadata must complete all COM queries without admitting ownership");
            Check(state.metadataValid == 1 && state.supportPredicate == 0 && state.canonicalIdentityFlags == 7 && state.deviceNodeCount == 1 &&
                      state.queueType == 0 && state.customPresentPresent == 1 && state.nativeExplicitGPUCount == 1 && state.nativeAFRCount == 1,
                  "A custom-present flag must preserve full identity metadata while keeping the support predicate false");
            Check(state.swapchainWidth == 640 && state.swapchainHeight == 480 && state.backbufferWidth == 640 && state.backbufferHeight == 480 &&
                      state.swapchainBufferCount == 2 && state.backbufferSampleCount == 1 && state.currentBackbufferIndex < 2,
                  "Custom-present diagnostics must describe the actual own-process swapchain and backbuffer");
            Check(References(device.Get()) == deviceRefs && References(queue.Get()) == queueRefs && References(swapchain.Get()) == swapRefs &&
                      References(buffer.Get()) == bufferRefs,
                  "Repeated custom-present diagnostics must release every acquired COM reference before returning");
        }
        // Reusing an output must not carry admission across a change in the
        // custom-present flag or leave a later supported probe unsupported.
        auto reused = Fresh();
        for (const unsigned customPresent : {0u, 1u, 0u})
        {
            reused.customPresentPresent = customPresent;
            ProbeD3D12Ownership(swapchain.Get(), queue.Get(), reused);
            Check(reused.metadataValid == 1 && reused.supportPredicate == (customPresent ? 0u : 1u) && reused.canonicalIdentityFlags == 7 &&
                      reused.queryStage == 19 &&
                      reused.queryStatus == Number(customPresent ? GpuOwnershipQueryStatus::Unsupported : GpuOwnershipQueryStatus::Accepted),
                  "Reused output must derive metadata and support from the current custom-present flag");
            Check(References(device.Get()) == deviceRefs && References(queue.Get()) == queueRefs && References(swapchain.Get()) == swapRefs &&
                      References(buffer.Get()) == bufferRefs,
                  "Changing the custom-present flag cannot leak references from earlier probes");
        }
        NullQuery nullQuery;
        auto      nullSwap = Fresh();
        ProbeD3D12Ownership(&nullQuery, queue.Get(), nullSwap);
        Check(nullSwap.queryStage == 4 && nullSwap.hresult == static_cast<std::uint32_t>(E_POINTER) && !nullSwap.metadataValid,
              "Success with a null swapchain interface must fail at acquisition, not a later vcall");
        auto nullQueue = Fresh();
        ProbeD3D12Ownership(swapchain.Get(), &nullQuery, nullQueue);
        Check(nullQueue.queryStage == 5 && nullQueue.hresult == static_cast<std::uint32_t>(E_POINTER) && References(swapchain.Get()) == swapRefs,
              "Null queue acquisition must release the already acquired actual swapchain");
        // D3D12 may return the same device for repeated creation on one adapter.
        // Use a real COMPUTE queue to prove a fully queried unsupported outcome
        // instead of assuming that two WARP creations have distinct identities.
        ComPtr<ID3D12CommandQueue> computeQueue;
        queueDescription.Type = D3D12_COMMAND_LIST_TYPE_COMPUTE;
        Check(SUCCEEDED(device->CreateCommandQueue(&queueDescription, IID_PPV_ARGS(&computeQueue))), "Owned COMPUTE queue must create");
        auto mismatch = Fresh();
        ProbeD3D12Ownership(swapchain.Get(), computeQueue.Get(), mismatch);
        Check(mismatch.queryStatus == Number(GpuOwnershipQueryStatus::Unsupported) && mismatch.metadataValid && !mismatch.supportPredicate &&
                  mismatch.canonicalIdentityFlags == 7 && mismatch.queueType == 2,
              "A non-DIRECT queue must remain a fully queried unsupported outcome");
        buffer.Reset();
        swapchain.Reset();
        initial.Reset();
        computeQueue.Reset();
        queue.Reset();
        device.Reset();
        warp.Reset();
        factory.Reset();
        DestroyWindow(window);
        window = nullptr;
        UnregisterClassW(klass.lpszClassName, klass.hInstance);
        std::printf(
            "{\"success\":true,\"checks\":%u,\"actual_warp_com_query\":true,\"custom_present_flagged_warp_metadata\":true,"
            "\"gpu_commands_issued\":false,\"game_operations\":false}\n",
            checks);
        return 0;
    }
    catch (const std::exception& error)
    {
        if (window)
            DestroyWindow(window);
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
