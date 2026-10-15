/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Execute the real owned child/DComp surface on disposable WARP devices
 *****************************************************************************/
#include <windows.h>
#include <d3d12.h>
#include <d3d12sdklayers.h>
#include <dxgi1_4.h>
#include <dwmapi.h>
#include <wrl/client.h>
#include <cstdio>
#include <atomic>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
#include "../../Client/core/DefinitiveGuiSurface.h"
using Microsoft::WRL::ComPtr;
using namespace DefinitiveGuiSurface;
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
    void Hr(HRESULT hr, const char* why)
    {
        if (FAILED(hr))
            std::fprintf(stderr, "%s HRESULT=%08lx\n", why, static_cast<unsigned long>(hr));
        Check(SUCCEEDED(hr), why);
    }
    // Stack lifetime covers the complete helper run. AddRef/Release and QI
    // delegate to the real device, retaining its actual canonical IUnknown.
    class DeviceProxy final : public ID3D12Device
    {
    public:
        DeviceProxy(ID3D12Device* value, ID3D12Fence* gateValue) : real(value), gate(gateValue) {}
        virtual ~DeviceProxy()
        {
            if (gate)
                gate->Signal(1);
            if (created)
                created->Release();
        }
        ID3D12Device*       real;
        ID3D12Fence*        gate;
        ID3D12CommandQueue* created = nullptr;
        unsigned            calls = 0;
        ULONG               proxyReferences = 1;
        bool                faultFinalRelease = false;
        bool                failSecondReadback = false;
        unsigned            readbackCreations = 0;
        unsigned            releaseFaults = 0;
#define DEVICE_FORWARD(type, name, arguments, forwarded) \
    type STDMETHODCALLTYPE name arguments override \
    { \
        ++calls; \
        return real->name forwarded; \
    }
        DEVICE_FORWARD(HRESULT, QueryInterface, (REFIID iid, void** output), (iid, output))
        ULONG STDMETHODCALLTYPE AddRef() override
        {
            ++calls;
            ++proxyReferences;
            return real->AddRef();
        }
        ULONG STDMETHODCALLTYPE Release() override
        {
            ++calls;
            if (faultFinalRelease && proxyReferences == 1)
            {
                ++releaseFaults;
                RaiseException(0xe0421234, 0, 0, nullptr);
                return 0;
            }
            --proxyReferences;
            return real->Release();
        }
        DEVICE_FORWARD(HRESULT, GetPrivateData, (REFGUID id, UINT* size, void* data), (id, size, data))
        DEVICE_FORWARD(HRESULT, SetPrivateData, (REFGUID id, UINT size, const void* data), (id, size, data))
        DEVICE_FORWARD(HRESULT, SetPrivateDataInterface, (REFGUID id, const IUnknown* data), (id, data))
        DEVICE_FORWARD(HRESULT, SetName, (LPCWSTR name), (name))
        DEVICE_FORWARD(UINT, GetNodeCount, (), ())
        HRESULT STDMETHODCALLTYPE CreateCommandQueue(const D3D12_COMMAND_QUEUE_DESC* description, REFIID iid, void** output) override
        {
            ++calls;
            if (!gate)
                return real->CreateCommandQueue(description, iid, output);
            if (iid != __uuidof(ID3D12CommandQueue) || !output || created)
                return E_INVALIDARG;
            ID3D12CommandQueue* queue = nullptr;
            const auto          hr = real->CreateCommandQueue(description, IID_PPV_ARGS(&queue));
            if (FAILED(hr))
                return hr;
            created = queue;
            const auto wait = queue->Wait(gate, 1);
            if (FAILED(wait))
            {
                queue->Release();
                created = nullptr;
                return wait;
            }
            created->AddRef();  // Exact owned observation lease; canonical queue stays intact.
            *output = created;
            return S_OK;
        }
        DEVICE_FORWARD(HRESULT, CreateCommandAllocator, (D3D12_COMMAND_LIST_TYPE type, REFIID iid, void** output), (type, iid, output))
        DEVICE_FORWARD(HRESULT, CreateGraphicsPipelineState, (const D3D12_GRAPHICS_PIPELINE_STATE_DESC* description, REFIID iid, void** output),
                       (description, iid, output))
        DEVICE_FORWARD(HRESULT, CreateComputePipelineState, (const D3D12_COMPUTE_PIPELINE_STATE_DESC* description, REFIID iid, void** output),
                       (description, iid, output))
        DEVICE_FORWARD(HRESULT, CreateCommandList,
                       (UINT node, D3D12_COMMAND_LIST_TYPE type, ID3D12CommandAllocator* allocator, ID3D12PipelineState* initial, REFIID iid, void** output),
                       (node, type, allocator, initial, iid, output))
        DEVICE_FORWARD(HRESULT, CheckFeatureSupport, (D3D12_FEATURE feature, void* data, UINT size), (feature, data, size))
        DEVICE_FORWARD(HRESULT, CreateDescriptorHeap, (const D3D12_DESCRIPTOR_HEAP_DESC* description, REFIID iid, void** output), (description, iid, output))
        DEVICE_FORWARD(UINT, GetDescriptorHandleIncrementSize, (D3D12_DESCRIPTOR_HEAP_TYPE type), (type))
        DEVICE_FORWARD(HRESULT, CreateRootSignature, (UINT node, const void* bytes, SIZE_T size, REFIID iid, void** output), (node, bytes, size, iid, output))
        DEVICE_FORWARD(void, CreateConstantBufferView, (const D3D12_CONSTANT_BUFFER_VIEW_DESC* description, D3D12_CPU_DESCRIPTOR_HANDLE handle),
                       (description, handle))
        DEVICE_FORWARD(void, CreateShaderResourceView,
                       (ID3D12Resource * resource, const D3D12_SHADER_RESOURCE_VIEW_DESC* description, D3D12_CPU_DESCRIPTOR_HANDLE handle),
                       (resource, description, handle))
        DEVICE_FORWARD(void, CreateUnorderedAccessView,
                       (ID3D12Resource * resource, ID3D12Resource* counter, const D3D12_UNORDERED_ACCESS_VIEW_DESC* description,
                        D3D12_CPU_DESCRIPTOR_HANDLE handle),
                       (resource, counter, description, handle))
        DEVICE_FORWARD(void, CreateRenderTargetView,
                       (ID3D12Resource * resource, const D3D12_RENDER_TARGET_VIEW_DESC* description, D3D12_CPU_DESCRIPTOR_HANDLE handle),
                       (resource, description, handle))
        DEVICE_FORWARD(void, CreateDepthStencilView,
                       (ID3D12Resource * resource, const D3D12_DEPTH_STENCIL_VIEW_DESC* description, D3D12_CPU_DESCRIPTOR_HANDLE handle),
                       (resource, description, handle))
        DEVICE_FORWARD(void, CreateSampler, (const D3D12_SAMPLER_DESC* description, D3D12_CPU_DESCRIPTOR_HANDLE handle), (description, handle))
        DEVICE_FORWARD(void, CopyDescriptors,
                       (UINT destinationCount, const D3D12_CPU_DESCRIPTOR_HANDLE* destinations, const UINT* destinationSizes, UINT sourceCount,
                        const D3D12_CPU_DESCRIPTOR_HANDLE* sources, const UINT* sourceSizes, D3D12_DESCRIPTOR_HEAP_TYPE type),
                       (destinationCount, destinations, destinationSizes, sourceCount, sources, sourceSizes, type))
        DEVICE_FORWARD(void, CopyDescriptorsSimple,
                       (UINT count, D3D12_CPU_DESCRIPTOR_HANDLE destination, D3D12_CPU_DESCRIPTOR_HANDLE source, D3D12_DESCRIPTOR_HEAP_TYPE type),
                       (count, destination, source, type))
        DEVICE_FORWARD(D3D12_RESOURCE_ALLOCATION_INFO, GetResourceAllocationInfo, (UINT mask, UINT count, const D3D12_RESOURCE_DESC* descriptions),
                       (mask, count, descriptions))
        DEVICE_FORWARD(D3D12_HEAP_PROPERTIES, GetCustomHeapProperties, (UINT node, D3D12_HEAP_TYPE type), (node, type))
        HRESULT STDMETHODCALLTYPE CreateCommittedResource(const D3D12_HEAP_PROPERTIES* heap, D3D12_HEAP_FLAGS flags, const D3D12_RESOURCE_DESC* description,
                                                          D3D12_RESOURCE_STATES state, const D3D12_CLEAR_VALUE* clear, REFIID iid, void** output) override
        {
            ++calls;
            if (heap && heap->Type == D3D12_HEAP_TYPE_READBACK && ++readbackCreations == 2 && failSecondReadback)
            {
                if (output)
                    *output = nullptr;
                return E_OUTOFMEMORY;
            }
            return real->CreateCommittedResource(heap, flags, description, state, clear, iid, output);
        }
        DEVICE_FORWARD(HRESULT, CreateHeap, (const D3D12_HEAP_DESC* description, REFIID iid, void** output), (description, iid, output))
        DEVICE_FORWARD(HRESULT, CreatePlacedResource,
                       (ID3D12Heap * heap, UINT64 offset, const D3D12_RESOURCE_DESC* description, D3D12_RESOURCE_STATES state, const D3D12_CLEAR_VALUE* clear,
                        REFIID iid, void** output),
                       (heap, offset, description, state, clear, iid, output))
        DEVICE_FORWARD(HRESULT, CreateReservedResource,
                       (const D3D12_RESOURCE_DESC* description, D3D12_RESOURCE_STATES state, const D3D12_CLEAR_VALUE* clear, REFIID iid, void** output),
                       (description, state, clear, iid, output))
        DEVICE_FORWARD(HRESULT, CreateSharedHandle,
                       (ID3D12DeviceChild * object, const SECURITY_ATTRIBUTES* attributes, DWORD access, LPCWSTR name, HANDLE* output),
                       (object, attributes, access, name, output))
        DEVICE_FORWARD(HRESULT, OpenSharedHandle, (HANDLE handle, REFIID iid, void** output), (handle, iid, output))
        DEVICE_FORWARD(HRESULT, OpenSharedHandleByName, (LPCWSTR name, DWORD access, HANDLE* output), (name, access, output))
        DEVICE_FORWARD(HRESULT, MakeResident, (UINT count, ID3D12Pageable* const* objects), (count, objects))
        DEVICE_FORWARD(HRESULT, Evict, (UINT count, ID3D12Pageable* const* objects), (count, objects))
        DEVICE_FORWARD(HRESULT, CreateFence, (UINT64 initial, D3D12_FENCE_FLAGS flags, REFIID iid, void** output), (initial, flags, iid, output))
        DEVICE_FORWARD(HRESULT, GetDeviceRemovedReason, (), ())
        DEVICE_FORWARD(void, GetCopyableFootprints,
                       (const D3D12_RESOURCE_DESC* description, UINT first, UINT count, UINT64 offset, D3D12_PLACED_SUBRESOURCE_FOOTPRINT* layouts, UINT* rows,
                        UINT64* rowBytes, UINT64* total),
                       (description, first, count, offset, layouts, rows, rowBytes, total))
        DEVICE_FORWARD(HRESULT, CreateQueryHeap, (const D3D12_QUERY_HEAP_DESC* description, REFIID iid, void** output), (description, iid, output))
        DEVICE_FORWARD(HRESULT, SetStablePowerState, (BOOL enabled), (enabled))
        DEVICE_FORWARD(HRESULT, CreateCommandSignature, (const D3D12_COMMAND_SIGNATURE_DESC* description, ID3D12RootSignature* root, REFIID iid, void** output),
                       (description, root, iid, output))
        DEVICE_FORWARD(void, GetResourceTiling,
                       (ID3D12Resource * resource, UINT* tiles, D3D12_PACKED_MIP_INFO* packed, D3D12_TILE_SHAPE* shape, UINT* count, UINT first,
                        D3D12_SUBRESOURCE_TILING* tilings),
                       (resource, tiles, packed, shape, count, first, tilings))
        DEVICE_FORWARD(LUID, GetAdapterLuid, (), ())
#undef DEVICE_FORWARD
    };

    LRESULT CALLBACK ParentProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
    {
        if (message == WM_DESTROY)
        {
            PostQuitMessage(0);
            return 0;
        }
        if (message == WM_PAINT)
        {
            PAINTSTRUCT paint{};
            const auto  dc = BeginPaint(window, &paint);
            const auto  brush = CreateSolidBrush(RGB(17, 29, 43));
            FillRect(dc, &paint.rcPaint, brush);
            DeleteObject(brush);
            EndPaint(window, &paint);
            return 0;
        }
        return DefWindowProcW(window, message, wparam, lparam);
    }
    HWND Parent()
    {
        WNDCLASSW cls{};
        cls.hInstance = GetModuleHandleW(nullptr);
        cls.lpfnWndProc = &ParentProc;
        cls.lpszClassName = L"MtaOwnedCompositionParent";
        Check(RegisterClassW(&cls) != 0, "register only disposable parent class");
        RECT rect{0, 0, 640, 480};
        AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);
        const auto window = CreateWindowExW(WS_EX_NOACTIVATE, cls.lpszClassName, L"Owned MTA composition proof", WS_OVERLAPPEDWINDOW, 80, 80,
                                            rect.right - rect.left, rect.bottom - rect.top, nullptr, nullptr, cls.hInstance, nullptr);
        Check(window != nullptr, "create disposable owned parent window");
        Check(SetWindowPos(window, HWND_TOPMOST, 80, 80, rect.right - rect.left, rect.bottom - rect.top, SWP_NOACTIVATE | SWP_SHOWWINDOW) != FALSE,
              "show only owned proof without activation");
        UpdateWindow(window);
        return window;
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
    struct OwnedParent
    {
        std::thread       thread;
        std::atomic<HWND> window{nullptr};
        std::atomic<bool> ready{false}, done{false};
        void              Start()
        {
            // The real game HWND belongs to another thread. Keep the owned
            // parent equally separate, including recursive child destruction.
            thread = std::thread(
                [&]
                {
                    try
                    {
                        window.store(Parent());
                        ready.store(true);
                        MSG message{};
                        while (GetMessageW(&message, nullptr, 0, 0) > 0)
                        {
                            TranslateMessage(&message);
                            DispatchMessageW(&message);
                        }
                    }
                    catch (const std::exception& error)
                    {
                        std::fprintf(stderr, "Owned parent failed: %s\n", error.what());
                        ready.store(true);
                    }
                    UnregisterClassW(L"MtaOwnedCompositionParent", GetModuleHandleW(nullptr));
                    done.store(true);
                });
            const auto deadline = GetTickCount64() + 5000;
            while (!ready.load() && GetTickCount64() < deadline)
                Sleep(1);
            Check(ready.load() && window.load() != nullptr, "independent owned parent thread is ready");
        }
        void Close()
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
        ~OwnedParent() { Close(); }
    };
    void Drain(Session& surface)
    {
        const auto deadline = GetTickCount64() + 5000;
        Status     status;
        do
        {
            Pump();
            status = surface.Shutdown();
            if (status == Status::Busy)
                Sleep(1);
        } while (status == Status::Busy && GetTickCount64() < deadline);
        Check(status == Status::Ok && surface.Snapshot().cleanupComplete, "same owner drains without needing another Present");
    }
    void RenderProof(Session& surface, unsigned minimumFrames)
    {
        const auto deadline = GetTickCount64() + 8000;
        do
        {
            Pump();
            const auto before = GetTickCount64();
            const auto status = surface.Poll();
            Check(GetTickCount64() - before < 1000, "worker Poll does not wait for GPU completion");
            Check(status == Status::Ok || status == Status::Busy, "production composition surface remains running");
            const auto result = surface.Snapshot();
            if (result.readbackComplete && result.presentSucceeded >= minimumFrames)
                return;
            Sleep(10);
        } while (GetTickCount64() < deadline);
        Check(false, "owned composition frames and readback complete before test deadline");
    }
    void Capture(HWND parent, const std::filesystem::path& path)
    {
        // This is a crop of the disposable owned parent, never the game or a
        // desktop-wide capture. It distinguishes actual DWM composition from RT pixels.
        Hr(DwmFlush(), "wait for owned composition commit in fixture only");
        RECT area{};
        GetClientRect(parent, &area);
        POINT origin{};
        ClientToScreen(parent, &origin);
        BITMAPINFO info{};
        info.bmiHeader.biSize = sizeof(info.bmiHeader);
        info.bmiHeader.biWidth = area.right;
        info.bmiHeader.biHeight = -area.bottom;
        info.bmiHeader.biPlanes = 1;
        info.bmiHeader.biBitCount = 32;
        info.bmiHeader.biCompression = BI_RGB;
        void*      pixels = nullptr;
        const auto screen = GetDC(nullptr);
        const auto memory = CreateCompatibleDC(screen);
        const auto bitmap = CreateDIBSection(screen, &info, DIB_RGB_COLORS, &pixels, nullptr, 0);
        Check(bitmap && pixels, "allocate exact owned-parent screenshot");
        const auto previous = SelectObject(memory, bitmap);
        Check(BitBlt(memory, 0, 0, area.right, area.bottom, screen, origin.x, origin.y, SRCCOPY | CAPTUREBLT) != FALSE,
              "capture only owned parent client crop");
        const auto       transparent = GetPixel(memory, 0, 0);
        const auto       red = GetPixel(memory, 532, 132);
        const DWORD      bytes = static_cast<DWORD>(area.right) * static_cast<DWORD>(area.bottom) * 4;
        BITMAPFILEHEADER file{};
        file.bfType = 0x4d42;
        file.bfOffBits = sizeof(file) + sizeof(info.bmiHeader);
        file.bfSize = file.bfOffBits + bytes;
        std::ofstream stream(path, std::ios::binary);
        stream.write(reinterpret_cast<const char*>(&file), sizeof(file));
        stream.write(reinterpret_cast<const char*>(&info.bmiHeader), sizeof(info.bmiHeader));
        stream.write(static_cast<const char*>(pixels), bytes);
        stream.close();
        SelectObject(memory, previous);
        DeleteObject(bitmap);
        DeleteDC(memory);
        ReleaseDC(nullptr, screen);
        Check(transparent == RGB(17, 29, 43), "transparent surface exposes the owned parent background through DWM");
        Check(red == RGB(255, 0, 0), "DWM displays actual MTA texture in the owned parent crop");
        std::printf("Owned composition screenshot transparent=%08lx red=%08lx\n", static_cast<unsigned long>(transparent), static_cast<unsigned long>(red));
    }
}
int wmain(int argc, wchar_t** argv)
{
    setvbuf(stdout, nullptr, _IONBF, 0);
    setvbuf(stderr, nullptr, _IONBF, 0);
    // Only this disposable process changes DPI policy. Native game policy is
    // untouched; owned screenshot coordinates must represent physical pixels.
    SetProcessDPIAware();
    try
    {
        Check(argc == 3 && sizeof(void*) == 8, "owned x64 surface fixture root and case supplied");
        const std::filesystem::path root(argv[1]);
        const std::wstring          mode(argv[2]);
        Check(root.is_absolute() && root.filename() == L"de-online", "use explicit DE-layout assets");
        Check(mode == L"normal" || mode == L"gated" || mode == L"gated-destroy" || mode == L"destroy" || mode == L"resize-failure" || mode == L"release",
              "one explicit normal/gated/parent-destroy/release case");
        std::printf("GUI surface case=%ls\n", mode.c_str());
        Check(!GetModuleHandleW(L"d3dx9_43.dll"), "fixture begins without legacy D3DX9 renderer");
        const auto core = LoadLibraryExW((root / L"core.dll").c_str(), nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
        Check(core != nullptr, "load certified uninitialized Core asset resolver");
        using SetRoot = void (*)(const wchar_t*, std::size_t);
        const auto setRoot = reinterpret_cast<SetRoot>(GetProcAddress(core, "SetMTADirectory"));
        Check(setRoot != nullptr, "resolve existing Core root setter");
        const auto wide = root.wstring();
        setRoot(wide.data(), wide.size());
        {
            Session empty;
            Check(empty.Shutdown() == Status::Ok && empty.Snapshot().cleanupComplete, "empty session owns no cleanup");
        }
        ComPtr<ID3D12Debug> debug;
        const bool          debugAvailable = SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug)));
        if (debugAvailable)
            debug->EnableDebugLayer();
        std::printf("D3D12 debug layer available=%u\n", debugAvailable ? 1u : 0u);
        ComPtr<IDXGIFactory4>   factory;
        ComPtr<IDXGIAdapter>    adapter;
        ComPtr<ID3D12Device>    device;
        ComPtr<ID3D12InfoQueue> info;
        Hr(CreateDXGIFactory1(IID_PPV_ARGS(&factory)), "create owned system factory");
        Hr(factory->EnumWarpAdapter(IID_PPV_ARGS(&adapter)), "select disposable WARP adapter");
        Hr(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device)), "create owned WARP device");
        device.As(&info);
        OwnedParent parentOwner;
        parentOwner.Start();
        auto                parent = parentOwner.window.load();
        const auto          focus = GetForegroundWindow();
        ComPtr<ID3D12Fence> gate;
        if (mode == L"gated" || mode == L"gated-destroy")
            Hr(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&gate)), "create owned queue gate");
        DeviceProxy proxy(device.Get(), gate.Get());
        proxy.faultFinalRelease = mode == L"release";
        proxy.failSecondReadback = mode == L"resize-failure";
        Session surface;
        if (mode == L"normal")
            Check(!std::filesystem::exists(root / L"mta/logs"), "fresh owned package has no generated logs");
        Check(surface.Prepare((root / L"cgui.dll").c_str(), root.c_str()) == Status::Ok, "preload actual cgui and explicit assets");
        Check(std::filesystem::is_directory(root / L"mta/logs"), "surface Prepare creates generated logs before GUI configuration");
        Check(!GetModuleHandleW(L"d3dx9_43.dll"), "Prepare keeps legacy renderer unloaded");
        bool        wrongThread = false;
        std::thread foreign(
            [&]
            {
                wrongThread = surface.AttachDeviceWindow(reinterpret_cast<ID3D12Device*>(UINT_PTR(1)), parent, true) == Status::WrongThread &&
                              surface.Start() == Status::WrongThread && surface.Poll() == Status::WrongThread && surface.Shutdown() == Status::WrongThread &&
                              !surface.Snapshot().cleanupComplete && !surface.Snapshot().ownerThreadId;
            });
        foreign.join();
        Check(wrongThread, "foreign thread cannot attach/create/poll/destroy or obtain owner evidence");
        {
            Session invalid;
            Check(invalid.Prepare((root / L"cgui.dll").c_str(), root.c_str()) == Status::Ok, "prepare separate malformed-window cleanup owner");
            device->AddRef();
            Check(invalid.AttachDeviceWindow(device.Get(), nullptr, false) == Status::Ok,
                  "invalid window metadata still consumes valid owned device without COM");
            Check(invalid.Start() == Status::Failed, "Start refuses malformed window after attachment");
            Drain(invalid);
        }
        const auto    calls = proxy.calls;
        ID3D12Device* transferred = nullptr;
        if (mode == L"normal" || mode == L"destroy")
        {
            device->AddRef();
            transferred = device.Get();
        }
        else
        {
            device->AddRef();
            transferred = &proxy;
        }
        Check(surface.AttachDeviceWindow(transferred, parent, true) == Status::Ok, "attach one device reference and borrowed parent");
        Check(proxy.calls == calls, "AttachDeviceWindow itself invokes no COM");
        Check(surface.Start() == Status::Ok, "start actual GUI on owned composition queue and swapchain");
        if (mode == L"gated" || mode == L"gated-destroy")
        {
            const auto before = GetTickCount64();
            Check(surface.Shutdown() == Status::Busy && GetTickCount64() - before < 1000, "gated stop remains nonblocking without a future Present");
            const auto retained = surface.Snapshot();
            Check(retained.moduleRetained && retained.sessionRetained && retained.deviceRetained && retained.queueRetained && retained.surfaceRetained &&
                      !retained.cleanupComplete,
                  "Busy retains complete owned surface GPU leases");
            if (mode == L"gated-destroy")
            {
                Check(PostMessageW(parent, WM_CLOSE, 0, 0) != FALSE, "request parent destruction while GPU drain remains Busy");
                const auto deadline = GetTickCount64() + 5000;
                while (!surface.Snapshot().parentDestroyed && GetTickCount64() < deadline)
                {
                    Pump();
                    Check(surface.Shutdown() == Status::Busy, "parent-destroyed gated GPU ownership remains retained");
                    Sleep(1);
                }
                Check(surface.Snapshot().parentDestroyed && !surface.Snapshot().parentAlive && !surface.Snapshot().cleanupComplete,
                      "Shutdown refreshes parent destruction after the last Poll");
                parent = nullptr;
            }
            Hr(gate->Signal(1), "open only owned queue gate from CPU");
            Drain(surface);
        }
        else
        {
            RenderProof(surface, 2);
            auto result = surface.Snapshot();
            Check(result.presentCalls == result.presentSucceeded + result.presentBusy && result.presentHresult == static_cast<unsigned>(S_OK),
                  "nonblocking Present attempts preserve success and busy counts separately");
            Check(result.readbackPixels > 10000 && result.readbackRedVerified && result.transparentPixelVerified && result.premultipliedPixelsVerified,
                  "actual MTA widgets have opaque red and transparent premultiplied pixels");
            Check(result.childCreated && result.childShown && result.compositionCreated && result.parentValidated &&
                      result.ownerThreadId == GetCurrentThreadId() && result.parentThreadId != GetCurrentThreadId(),
                  "owned child and DComp parent association recorded");
            Check(GetForegroundWindow() == focus, "surface does not activate or focus its child");
            if (mode == L"normal")
            {
                Capture(parent, std::filesystem::current_path() / L"owned-composition.bmp");
                RECT rect{0, 0, 800, 600};
                AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);
                Check(SetWindowPos(parent, nullptr, 0, 0, rect.right - rect.left, rect.bottom - rect.top, SWP_NOMOVE | SWP_NOACTIVATE | SWP_NOZORDER) != FALSE,
                      "resize only owned parent");
                const auto deadline = GetTickCount64() + 5000;
                while (surface.Snapshot().resizeCount == 0 && GetTickCount64() < deadline)
                {
                    Pump();
                    const auto status = surface.Poll();
                    Check(status == Status::Ok || status == Status::Busy, "resize keeps production surface live");
                    Sleep(5);
                }
                result = surface.Snapshot();
                Check(result.resizeCount == 1 && result.width == 800 && result.height == 600 && !result.resizePending,
                      "drain and resize only owned composition swapchain");
                RenderProof(surface, result.presentSucceeded + 1);
            }
            if (mode == L"resize-failure")
            {
                RECT rectangle{0, 0, 800, 600};
                AdjustWindowRect(&rectangle, WS_OVERLAPPEDWINDOW, FALSE);
                Check(SetWindowPos(parent, nullptr, 0, 0, rectangle.right - rectangle.left, rectangle.bottom - rectangle.top,
                                   SWP_NOMOVE | SWP_NOACTIVATE | SWP_NOZORDER) != FALSE,
                      "resize owned parent before deterministic new-readback refusal");
                Status     status = Status::Ok;
                const auto deadline = GetTickCount64() + 5000;
                do
                {
                    Pump();
                    status = surface.Poll();
                    if (status != Status::Failed)
                        Sleep(5);
                } while (status != Status::Failed && GetTickCount64() < deadline);
                const auto failed = surface.Snapshot();
                Check(status == Status::Failed && proxy.readbackCreations == 2 && failed.hresult == static_cast<unsigned>(E_OUTOFMEMORY) &&
                          failed.resizePending && !failed.cleanupComplete,
                      "post-ResizeBuffers readback allocation failure preserves pending owned lifetime");
                Drain(surface);
                Check(surface.Snapshot().cleanupComplete && !surface.Snapshot().resizePending &&
                          surface.Snapshot().hresult == static_cast<unsigned>(E_OUTOFMEMORY),
                      "successful cleanup clears resizePending without relabeling failed workload");
            }
            if (mode == L"destroy")
            {
                Check(PostMessageW(parent, WM_CLOSE, 0, 0) != FALSE, "request destruction on disposable parent owner");
                const auto deadline = GetTickCount64() + 5000;
                while (!surface.Snapshot().parentDestroyed && GetTickCount64() < deadline)
                {
                    Pump();
                    surface.Poll();
                    Sleep(1);
                }
                parent = nullptr;
                Check(surface.Snapshot().parentDestroyed && surface.Snapshot().stopRequested && !surface.Snapshot().parentAlive,
                      "lost borrowed parent stops own surface without native lifetime hold");
            }
            if (mode == L"release")
            {
                Status     status;
                const auto deadline = GetTickCount64() + 5000;
                do
                {
                    status = surface.Shutdown();
                    if (status == Status::Busy)
                        Sleep(1);
                } while (status == Status::Busy && GetTickCount64() < deadline);
                Check(status == Status::Failed && surface.Snapshot().releaseFault && !surface.Snapshot().cleanupComplete && surface.Snapshot().moduleRetained,
                      "unknown transferred-device Release retains module and denies cleanup");
                const auto faults = proxy.releaseFaults;
                Check(surface.Shutdown() == Status::Failed && proxy.releaseFaults == faults, "unknown Release pointer is never retried");
                std::printf("Owned surface release fault retained=1 cleanupComplete=0 retryCalls=0\n");
            }
            else if (mode != L"resize-failure")
                Drain(surface);
        }
        if (mode != L"release")
        {
            const auto result = surface.Snapshot();
            Check(result.cleanupComplete && result.windowCleanupComplete && result.compositionCleanupComplete && result.classUnregistered &&
                      !result.moduleRetained && !result.sessionRetained && !result.deviceRetained && !result.childRetained && !result.queueRetained &&
                      !result.surfaceRetained,
                  "normal cleanup releases class/window/DComp/GUI/device and all owned GPU references");
            Check(!GetModuleHandleW(L"cgui.dll"), "actual cgui unloads after owner drain");
        }
        Check(!GetModuleHandleW(L"d3dx9_43.dll"), "surface preserves delayed legacy-renderer isolation");
        parentOwner.Close();
        if (proxy.created)
        {
            proxy.created->Release();
            proxy.created = nullptr;
        }
        if (mode != L"release")
        {
            if (info)
            {
                unsigned errors = 0;
                for (UINT64 i = 0; i < info->GetNumStoredMessages(); ++i)
                {
                    SIZE_T size = 0;
                    info->GetMessage(i, nullptr, &size);
                    std::vector<BYTE> bytes(size);
                    auto*             message = reinterpret_cast<D3D12_MESSAGE*>(bytes.data());
                    if (SUCCEEDED(info->GetMessage(i, message, &size)) && message->Severity <= D3D12_MESSAGE_SEVERITY_ERROR)
                    {
                        ++errors;
                        std::fprintf(stderr, "D3D12 error: %s\n", message->pDescription);
                    }
                }
                Check(!errors, "owned surface has no debug-layer lifetime/state errors");
            }
            // The observed queue is already open/drained. Drop the proxy
            // borrow before destroying its real fence, so its destructor cannot
            // signal a freed fixture-only gate after printing the final result.
            proxy.gate = nullptr;
            gate.Reset();
            info.Reset();
            adapter.Reset();
            factory.Reset();
            auto* finalDevice = device.Detach();
            Check(finalDevice->Release() == 0, "owned WARP device references return to zero");
            Check(FreeLibrary(core) != FALSE, "release historical uninitialized Core asset resolver");
        }
        std::printf("Passed %u production GUI surface host checks\n", checks);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "GUI surface host failed: %s\n", error.what());
        return 1;
    }
}
