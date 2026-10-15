/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Exercise actual MTA widgets on a private real D3D12/WARP swapchain and its lifetime seam.
 *****************************************************************************/
#include <windows.h>
#include <d3d12.h>
#include <d3d12sdklayers.h>
#include <dxgi1_4.h>
#include <wrl/client.h>
#include <algorithm>
#include <array>
#include <cstdint>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>
#include <SharedUtil.Defines.h>
#include <SString.h>
#include <gui/CGUI.h>
#include <gui/CGUID3D12.h>

using Microsoft::WRL::ComPtr;
using namespace mta::gui;
static_assert(GuiD3D12ApiVersion == 1 && GuiD3D12AssetRootApiVersion == 1);
static_assert(sizeof(GuiD3D12CreateInfo) == 40 && sizeof(GuiD3D12Frame) == 24);
namespace
{
    unsigned checks = 0, clicks = 0;
    struct Channels
    {
        unsigned red, green, blue, alpha, maximumColor, maximumAlpha;
    };

    Channels Decode(DXGI_FORMAT format, std::uint32_t value)
    {
        if (format == DXGI_FORMAT_R10G10B10A2_UNORM)
            return {value & 1023, (value >> 10) & 1023, (value >> 20) & 1023, value >> 30, 1023, 3};
        if (format == DXGI_FORMAT_R8G8B8A8_UNORM)
            return {value & 255, (value >> 8) & 255, (value >> 16) & 255, value >> 24, 255, 255};
        return {(value >> 16) & 255, (value >> 8) & 255, value & 255, value >> 24, 255, 255};
    }

    bool Near(unsigned actual, double expected)
    {
        return std::abs(static_cast<double>(actual) - expected) <= 2;
    }
    void Check(bool value, const char* why)
    {
        ++checks;
        if (!value)
            throw std::runtime_error(why);
        std::printf("Check %u: %s\n", checks, why);
    }
    void Hr(HRESULT value, const char* why)
    {
        if (FAILED(value))
            std::fprintf(stderr, "%s HRESULT=%08lx\n", why, static_cast<unsigned long>(value));
        Check(SUCCEEDED(value), why);
    }
    bool Click(CGUIElement* element)
    {
        ++clicks;
        return element != nullptr;
    }
    void InputClick(CGUI* gui, unsigned x, unsigned y)
    {
        gui->ProcessMouseInput(CGUI_MI_MOUSEPOS, x, y);
        gui->ProcessMouseInput(CGUI_MI_MOUSEDOWN, 0, 0, LeftButton);
        gui->ProcessMouseInput(CGUI_MI_MOUSEUP, 0, 0, LeftButton);
    }
    struct Owned
    {
        ComPtr<IDXGIFactory4>                 factory;
        ComPtr<IDXGIAdapter>                  adapter;
        ComPtr<IDXGISwapChain3>               swapchain;
        std::array<ComPtr<ID3D12Resource>, 3> buffers;
        ComPtr<ID3D12Device>                  device;
        ComPtr<ID3D12CommandQueue>            queue;
        ComPtr<ID3D12CommandAllocator>        allocator;
        ComPtr<ID3D12GraphicsCommandList>     list;
        ComPtr<ID3D12Fence>                   fence, blocker;
        ComPtr<ID3D12Resource>                target, readback;
        ComPtr<ID3D12DescriptorHeap>          rtv;
        ComPtr<ID3D12InfoQueue>               info;
        D3D12_PLACED_SUBRESOURCE_FOOTPRINT    footprint{};
        HANDLE                                event = nullptr;
        UINT64                                nextFence = 1;
        UINT                                  width = 640, height = 480;
        DXGI_FORMAT                           format = DXGI_FORMAT_UNKNOWN;
        HMODULE                               core = nullptr, guiModule = nullptr;
        IGuiD3D12Session*                     session = nullptr;
        CGUI*                                 gui = nullptr;
        bool                                  blocked = false;
        HWND                                  window = nullptr;
        unsigned                              nativeResizeCalls = 0, presentCalls = 0;
        HRESULT                               lastPresent = E_UNEXPECTED;
        std::array<bool, 3>                   renderedBuffers{};
        ~Owned()
        {
            if (blocked && blocker)
                blocker->Signal(UINT64_MAX - 1);
            if (session)
            {
                // Failure cleanup stays bounded in the disposable owned host. A
                // failed drain retains the module/session rather than unloading code
                // while the GPU can still reference its allocations.
                for (unsigned i = 0; i < 500; ++i)
                {
                    if (session->Release() == GuiD3D12Status::Ok)
                    {
                        session = nullptr;
                        break;
                    }
                    Sleep(10);
                }
            }
            if (!session && guiModule)
                FreeLibrary(guiModule);
            if (!session && core)
                FreeLibrary(core);
            if (event)
                CloseHandle(event);
            swapchain.Reset();
            if (window)
                DestroyWindow(window);
            UnregisterClassW(L"MtaOwnedGuiSwapchain", GetModuleHandleW(nullptr));
        }
        void Wait()
        {
            const UINT64 value = nextFence++;
            Hr(queue->Signal(fence.Get(), value), "signal owned host completion fence");
            if (fence->GetCompletedValue() < value)
            {
                Hr(fence->SetEventOnCompletion(value, event), "arm owned host completion event");
                Check(WaitForSingleObject(event, 5000) == WAIT_OBJECT_0, "owned host GPU wait finishes within five seconds");
            }
            Check(fence->GetCompletedValue() != UINT64_MAX, "owned WARP device remains present");
        }
        void Begin()
        {
            Hr(allocator->Reset(), "reset completed owned host allocator");
            Hr(list->Reset(allocator.Get(), nullptr), "reset owned host command list");
        }
        void Execute()
        {
            Hr(list->Close(), "close owned host command list");
            ID3D12CommandList* lists[] = {list.Get()};
            queue->ExecuteCommandLists(1, lists);
            Wait();
        }
    };

    void Barrier(ID3D12GraphicsCommandList* list, ID3D12Resource* resource, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after)
    {
        if (before == after)
            return;
        D3D12_RESOURCE_BARRIER barrier{};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = resource;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barrier.Transition.StateBefore = before;
        barrier.Transition.StateAfter = after;
        list->ResourceBarrier(1, &barrier);
    }

    void MakeReadback(Owned& owned)
    {
        owned.readback.Reset();
        auto   desc = owned.target->GetDesc();
        UINT64 bytes = 0;
        owned.device->GetCopyableFootprints(&desc, 0, 1, 0, &owned.footprint, nullptr, nullptr, &bytes);
        Check(bytes && bytes != UINT64_MAX, "real backbuffer readback footprint is valid");
        desc = {};
        desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        desc.Width = bytes;
        desc.Height = desc.DepthOrArraySize = desc.MipLevels = 1;
        desc.SampleDesc.Count = 1;
        desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        D3D12_HEAP_PROPERTIES heap{};
        heap.Type = D3D12_HEAP_TYPE_READBACK;
        heap.CreationNodeMask = heap.VisibleNodeMask = 1;
        Hr(owned.device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&owned.readback)),
           "create owned swapchain readback buffer");
    }

    void SelectCurrent(Owned& owned)
    {
        const UINT index = owned.swapchain->GetCurrentBackBufferIndex();
        Check(index < owned.buffers.size(), "DXGI current index is within the real three-buffer chain");
        owned.target = owned.buffers[index];
        owned.device->CreateRenderTargetView(owned.target.Get(), nullptr, owned.rtv->GetCPUDescriptorHandleForHeapStart());
    }

    void AcquireBuffers(Owned& owned)
    {
        DXGI_SWAP_CHAIN_DESC1 description{};
        Hr(owned.swapchain->GetDesc1(&description), "read real HWND swapchain description");
        Check(description.BufferCount == 3 && description.Width == owned.width && description.Height == owned.height && description.Format == owned.format &&
                  description.SwapEffect == DXGI_SWAP_EFFECT_FLIP_DISCARD,
              "real swapchain retains the requested three-buffer normalized layout");
        ComPtr<IUnknown> expected;
        Hr(owned.device.As(&expected), "canonicalize creation device identity");
        for (UINT index = 0; index < owned.buffers.size(); ++index)
        {
            Hr(owned.swapchain->GetBuffer(index, IID_PPV_ARGS(&owned.buffers[index])), "acquire an actual DXGI backbuffer");
            const auto desc = owned.buffers[index]->GetDesc();
            Check(desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D && desc.Width == owned.width && desc.Height == owned.height &&
                      desc.Format == owned.format && desc.SampleDesc.Count == 1 && (desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET),
                  "actual backbuffer dimensions, format and render-target capability agree");
            ComPtr<ID3D12Device> device;
            ComPtr<IUnknown>     identity;
            Hr(owned.buffers[index]->GetDevice(IID_PPV_ARGS(&device)), "read actual backbuffer device");
            Hr(device.As(&identity), "canonicalize actual backbuffer device");
            Check(identity.Get() == expected.Get(), "all actual backbuffers belong to the canonical creation device");
            for (UINT earlier = 0; earlier < index; ++earlier)
                Check(owned.buffers[earlier].Get() != owned.buffers[index].Get(), "three backbuffers are distinct resources");
        }
        SelectCurrent(owned);
        MakeReadback(owned);
    }

    void ReleaseBuffers(Owned& owned)
    {
        // Native ResizeBuffers requires every external backbuffer reference to
        // retire, including the real GUI renderer's fence-protected frame slots.
        owned.target.Reset();
        for (auto& buffer : owned.buffers)
            buffer.Reset();
        owned.readback.Reset();
        owned.Begin();
        Hr(owned.list->Close(), "discard completed host command references before native resize");
    }

    void NativeResizeAfterGuiDrain(Owned& owned, UINT width, UINT height)
    {
        ReleaseBuffers(owned);
        ++owned.nativeResizeCalls;
        Hr(owned.swapchain->ResizeBuffers(3, width, height, owned.format, 0), "real ResizeBuffers succeeds after GUI drain and external backbuffer release");
        owned.width = width;
        owned.height = height;
        Check(
            SetWindowPos(owned.window, nullptr, 0, 0, static_cast<int>(width), static_cast<int>(height), SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOZORDER) != FALSE &&
                !IsWindowVisible(owned.window),
            "resize only the owned unshown HWND without activation");
        AcquireBuffers(owned);
    }

    GuiD3D12Status ResizeOwned(Owned& owned, UINT width, UINT height)
    {
        const auto status = owned.session->Resize(width, height);
        // This owner-side seam deliberately forbids touching native buffers
        // while the shipping GUI still reports an outstanding GPU lease.
        if (status == GuiD3D12Status::Ok)
            NativeResizeAfterGuiDrain(owned, width, height);
        return status;
    }

    HRESULT PresentOwned(Owned& owned)
    {
        // One natural call is forwarded after the actual GUI's PRESENT-to-RT-
        // to-PRESENT submission. Successful occlusion is preserved for hidden HWNDs.
        ++owned.presentCalls;
        owned.lastPresent = owned.swapchain->Present(0, 0);
        return owned.lastPresent;
    }

    void Clear(Owned& owned, D3D12_RESOURCE_STATES before)
    {
        owned.Begin();
        Barrier(owned.list.Get(), owned.target.Get(), before, D3D12_RESOURCE_STATE_RENDER_TARGET);
        constexpr float color[] = {17.0f / 255, 29.0f / 255, 43.0f / 255, 1};
        owned.list->ClearRenderTargetView(owned.rtv->GetCPUDescriptorHandleForHeapStart(), color, 0, nullptr);
        Barrier(owned.list.Get(), owned.target.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
        owned.Execute();
    }

    GuiD3D12Frame Frame(const Owned& owned)
    {
        return {owned.target.Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_PRESENT, owned.width, owned.height};
    }

    std::vector<std::uint32_t> Read(Owned& owned)
    {
        owned.Wait();
        owned.Begin();
        Barrier(owned.list.Get(), owned.target.Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_COPY_SOURCE);
        D3D12_TEXTURE_COPY_LOCATION source{}, destination{};
        source.pResource = owned.target.Get();
        source.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        destination.pResource = owned.readback.Get();
        destination.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        destination.PlacedFootprint = owned.footprint;
        owned.list->CopyTextureRegion(&destination, 0, 0, 0, &source, nullptr);
        Barrier(owned.list.Get(), owned.target.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_PRESENT);
        owned.Execute();
        void*             mapped = nullptr;
        const D3D12_RANGE range{0, static_cast<SIZE_T>(owned.readback->GetDesc().Width)};
        Hr(owned.readback->Map(0, &range, &mapped), "map actual WARP-rendered GUI pixels");
        std::vector<std::uint32_t> pixels(SIZE_T(owned.width) * owned.height);
        for (UINT row = 0; row < owned.height; ++row)
            memcpy(pixels.data() + SIZE_T(row) * owned.width,
                   static_cast<const BYTE*>(mapped) + owned.footprint.Offset + SIZE_T(row) * owned.footprint.Footprint.RowPitch, SIZE_T(owned.width) * 4);
        const D3D12_RANGE noWrite{};
        owned.readback->Unmap(0, &noWrite);
        return pixels;
    }

    void Save(const std::filesystem::path& path, DXGI_FORMAT format, UINT width, UINT height, const std::vector<std::uint32_t>& rawPixels)
    {
        // A BI_RGB bitmap requires BGRA bytes. Preserve the native packed
        // readback independently so artifact normalization cannot hide errors
        // in the 10-bit channels or the target's two-bit alpha.
        auto rawPath = path;
        rawPath.replace_extension(L".raw");
        std::ofstream raw(rawPath, std::ios::binary);
        raw.write(reinterpret_cast<const char*>(rawPixels.data()), static_cast<std::streamsize>(rawPixels.size() * 4));
        Check(raw.good(), "save untouched tightly packed native-format GPU readback");
        std::vector<std::uint32_t> pixels;
        pixels.reserve(rawPixels.size());
        for (const auto value : rawPixels)
        {
            const auto channels = Decode(format, value);
            const auto byte = [](unsigned channel, unsigned maximum) { return (channel * 255 + maximum / 2) / maximum; };
            pixels.push_back((byte(channels.alpha, channels.maximumAlpha) << 24) | (byte(channels.red, channels.maximumColor) << 16) |
                             (byte(channels.green, channels.maximumColor) << 8) | byte(channels.blue, channels.maximumColor));
        }
        BITMAPFILEHEADER file{};
        BITMAPINFOHEADER info{};
        file.bfType = 0x4d42;
        file.bfOffBits = sizeof(file) + sizeof(info);
        file.bfSize = file.bfOffBits + width * height * 4;
        info.biSize = sizeof(info);
        info.biWidth = static_cast<LONG>(width);
        info.biHeight = -static_cast<LONG>(height);
        info.biPlanes = 1;
        info.biBitCount = 32;
        info.biCompression = BI_RGB;
        std::ofstream output(path, std::ios::binary);
        output.write(reinterpret_cast<const char*>(&file), sizeof(file));
        output.write(reinterpret_cast<const char*>(&info), sizeof(info));
        output.write(reinterpret_cast<const char*>(pixels.data()), static_cast<std::streamsize>(pixels.size() * 4));
        Check(output.good(), "save normalized BGRA8 GUI artifact separately from native readback");
    }

    void CheckDebugErrors(Owned& owned)
    {
        if (!owned.info)
            return;
        unsigned errors = 0;
        for (UINT64 i = 0; i < owned.info->GetNumStoredMessages(); ++i)
        {
            SIZE_T bytes = 0;
            owned.info->GetMessage(i, nullptr, &bytes);
            std::vector<BYTE> storage(bytes);
            auto*             message = reinterpret_cast<D3D12_MESSAGE*>(storage.data());
            if (SUCCEEDED(owned.info->GetMessage(i, message, &bytes)) && message->Severity <= D3D12_MESSAGE_SEVERITY_ERROR)
            {
                ++errors;
                std::fprintf(stderr, "D3D12 debug error: %s\n", message->pDescription);
            }
        }
        Check(errors == 0, "actual D3D12 debug layer reports no resource or lifetime errors");
    }
}

int wmain(int argc, wchar_t** argv)
{
    setvbuf(stdout, nullptr, _IONBF, 0);
    setvbuf(stderr, nullptr, _IONBF, 0);
    try
    {
        Check(argc == 3 && sizeof(void*) == 8, "owned x64 swapchain fixture root and format supplied");
        const std::filesystem::path root(argv[1]);
        Check(root.is_absolute() && root.filename() == L"de-online" && root.parent_path().filename() == L"CLEO",
              "actual assets and DLLs use the explicit adapter-root layout");
        Check(!std::filesystem::exists(root / L"MTA/core.dll"), "no classic Core fallback can hide the explicit asset root");
        Owned              owned;
        const std::wstring format(argv[2]);
        owned.format = format == L"24"   ? DXGI_FORMAT_R10G10B10A2_UNORM
                       : format == L"28" ? DXGI_FORMAT_R8G8B8A8_UNORM
                       : format == L"87" ? DXGI_FORMAT_B8G8R8A8_UNORM
                                         : DXGI_FORMAT_UNKNOWN;
        Check(owned.format != DXGI_FORMAT_UNKNOWN, "fixture uses one explicit supported normalized target format");
        std::printf("Swapchain GUI target format=%u\n", static_cast<unsigned>(owned.format));
        ComPtr<ID3D12Debug> debug;
        const bool          debugAvailable = SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug)));
        if (debugAvailable)
            debug->EnableDebugLayer();
        std::printf("D3D12 debug layer available=%u\n", debugAvailable ? 1u : 0u);
        Hr(CreateDXGIFactory1(IID_PPV_ARGS(&owned.factory)), "create system DXGI factory for owned swapchain");
        Hr(owned.factory->EnumWarpAdapter(IID_PPV_ARGS(&owned.adapter)), "select actual owned WARP adapter");
        Hr(D3D12CreateDevice(owned.adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&owned.device)), "create private WARP device");
        if (debugAvailable)
            Hr(owned.device.As(&owned.info), "obtain owned D3D12 debug message queue");
        D3D12_COMMAND_QUEUE_DESC queue{};
        queue.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        Hr(owned.device->CreateCommandQueue(&queue, IID_PPV_ARGS(&owned.queue)), "create canonical direct creation queue");
        Hr(owned.device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&owned.allocator)), "create owned host allocator");
        Hr(owned.device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, owned.allocator.Get(), nullptr, IID_PPV_ARGS(&owned.list)),
           "create host copy/clear list");
        Hr(owned.list->Close(), "close initial owned list");
        Hr(owned.device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&owned.fence)), "create owned completion fence");
        Hr(owned.device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&owned.blocker)), "create CPU-released private queue gate");
        owned.event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        Check(owned.event != nullptr, "create bounded owned completion event");
        D3D12_DESCRIPTOR_HEAP_DESC rtv{};
        rtv.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        rtv.NumDescriptors = 1;
        Hr(owned.device->CreateDescriptorHeap(&rtv, IID_PPV_ARGS(&owned.rtv)), "create host RTV descriptor heap");
        WNDCLASSW windowClass{};
        windowClass.lpfnWndProc = DefWindowProcW;
        windowClass.hInstance = GetModuleHandleW(nullptr);
        windowClass.lpszClassName = L"MtaOwnedGuiSwapchain";
        Check(RegisterClassW(&windowClass) != 0, "register private fixture window class");
        owned.window = CreateWindowExW(0, windowClass.lpszClassName, L"Owned MTA GUI swapchain", WS_POPUP, 0, 0, static_cast<int>(owned.width),
                                       static_cast<int>(owned.height), nullptr, nullptr, windowClass.hInstance, nullptr);
        Check(owned.window != nullptr && !IsWindowVisible(owned.window), "create actual unshown HWND without game or desktop interaction");
        DXGI_SWAP_CHAIN_DESC1 desc{};
        desc.Width = owned.width;
        desc.Height = owned.height;
        desc.Format = owned.format;
        desc.SampleDesc.Count = 1;
        desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        desc.BufferCount = 3;
        desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        desc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
        {
            ComPtr<IDXGISwapChain1> chain;
            Hr(owned.factory->CreateSwapChainForHwnd(owned.queue.Get(), owned.window, &desc, nullptr, nullptr, &chain),
               "create real three-buffer swapchain using the exact owned direct queue");
            Hr(chain.As(&owned.swapchain), "query actual IDXGISwapChain3");
        }
        Hr(owned.factory->MakeWindowAssociation(owned.window, DXGI_MWA_NO_ALT_ENTER), "disable fullscreen switching for hidden fixture HWND");
        {
            ComPtr<ID3D12Device> queueDevice, chainDevice;
            ComPtr<IUnknown>     queueIdentity, chainIdentity, deviceIdentity;
            Hr(owned.queue->GetDevice(IID_PPV_ARGS(&queueDevice)), "get actual creation queue device");
            Hr(owned.swapchain->GetDevice(IID_PPV_ARGS(&chainDevice)), "get actual swapchain device");
            Hr(queueDevice.As(&queueIdentity), "canonicalize queue device");
            Hr(chainDevice.As(&chainIdentity), "canonicalize swapchain device");
            Hr(owned.device.As(&deviceIdentity), "canonicalize owner device");
            Check(queueIdentity.Get() == chainIdentity.Get() && chainIdentity.Get() == deviceIdentity.Get(),
                  "queue/swapchain/device canonical identities agree");
        }
        AcquireBuffers(owned);
        owned.core = LoadLibraryExW((root / L"core.dll").c_str(), nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
        Check(owned.core != nullptr, "load certified Core asset resolver without initialization");
        using SetCoreRoot = void (*)(const wchar_t*, std::size_t);
        const auto setCoreRoot = reinterpret_cast<SetCoreRoot>(GetProcAddress(owned.core, "SetMTADirectory"));
        Check(setCoreRoot != nullptr, "resolve separate Core asset-root setter");
        const auto rootWide = root.wstring();
        setCoreRoot(rootWide.data(), rootWide.size());
        owned.guiModule = LoadLibraryExW((root / L"cgui.dll").c_str(), nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
        Check(owned.guiModule != nullptr, "load certified actual MTA GUI DLL");
        const auto configure = reinterpret_cast<ConfigureGuiD3D12AssetRoot>(GetProcAddress(owned.guiModule, "ConfigureGUID3D12AssetRoot"));
        const auto create = reinterpret_cast<CreateGuiD3D12Session>(GetProcAddress(owned.guiModule, "CreateGUID3D12Session"));
        Check(configure && create && configure(GuiD3D12AssetRootApiVersion, rootWide.data(), static_cast<std::uint32_t>(rootWide.size())),
              "configure explicit real MTA assets through the existing unchanged API");
        const auto makeSession = [&]
        {
            GuiD3D12CreateInfo info;
            info.device = owned.device.Get();
            info.queue = owned.queue.Get();
            info.targetFormat = owned.format;
            info.width = owned.width;
            info.height = owned.height;
            owned.session = create(GuiD3D12ApiVersion, &info);
            Check(owned.session && owned.session->GetGUI(owned.gui) == GuiD3D12Status::Ok && owned.gui,
                  "create and borrow actual MTA/CEGUI owner-thread session");
            owned.gui->SetSkin("Default");
            Check(owned.gui->GetDefaultFont() && owned.gui->GetTextExtent("Owned swapchain") > 0, "real staged MTA fonts produce glyphs");
        };
        makeSession();
        bool        foreignCorrect = false;
        std::thread foreign(
            [&]
            {
                CGUI* sentinel = reinterpret_cast<CGUI*>(UINT_PTR(1));
                foreignCorrect = owned.session->GetGUI(sentinel) == GuiD3D12Status::WrongThread && sentinel == reinterpret_cast<CGUI*>(UINT_PTR(1)) &&
                                 owned.session->RenderFrame(Frame(owned)) == GuiD3D12Status::WrongThread &&
                                 owned.session->Resize(800, 600) == GuiD3D12Status::WrongThread && owned.session->Shutdown() == GuiD3D12Status::WrongThread &&
                                 owned.session->Release() == GuiD3D12Status::WrongThread;
            });
        foreign.join();
        Check(foreignCorrect && owned.nativeResizeCalls == 0, "foreign session owner cannot render, stop, release or reach native resize");
        const auto makeWidgets = [&](std::uint32_t color)
        {
            auto* window = owned.gui->CreateWnd(nullptr, "Actual MTA / owned swapchain");
            window->SetPosition(CVector2D(40, 40));
            window->SetSize(CVector2D(430, 300));
            window->SetVisible(true);
            auto* button = owned.gui->CreateButton(static_cast<CGUIElement*>(window), "Existing widget callback");
            button->SetPosition(CVector2D(25, 45));
            button->SetSize(CVector2D(220, 45));
            button->SetVisible(true);
            button->SetClickHandler(GUI_CALLBACK(&Click));
            auto*                         texture = owned.gui->CreateTexture();
            std::array<std::uint32_t, 16> pixels{};
            pixels.fill(color);
            texture->LoadFromMemory(pixels.data(), 4, 4);
            auto* image = owned.gui->CreateStaticImage();
            image->SetPosition(CVector2D(500, 100));
            image->SetSize(CVector2D(64, 64));
            image->SetFrameEnabled(false);
            image->SetVisible(true);
            Check(image->LoadFromTexture(texture), "actual static image uses existing MTA memory texture");
            return std::make_pair(texture, image);
        };
        auto       widgets = makeWidgets(0x80ff0000);
        const auto render = [&](const wchar_t* artifact, bool green)
        {
            SelectCurrent(owned);
            const UINT beforeIndex = owned.swapchain->GetCurrentBackBufferIndex();
            Clear(owned, D3D12_RESOURCE_STATE_PRESENT);
            Check(owned.session->RenderFrame(Frame(owned)) == GuiD3D12Status::Ok, "actual GUI records PRESENT to RT to PRESENT on current real backbuffer");
            const auto pixels = Read(owned);
            const auto clear = Decode(owned.format, pixels.front());
            Check(Near(clear.red, 17.0 / 255 * clear.maximumColor) && Near(clear.green, 29.0 / 255 * clear.maximumColor) &&
                      Near(clear.blue, 43.0 / 255 * clear.maximumColor) && clear.alpha == clear.maximumAlpha,
                  "real backbuffer preserves untouched clear channels and opaque native alpha");
            const auto changed = std::count_if(pixels.begin(), pixels.end(), [&](auto value) { return value != pixels.front(); });
            Check(changed > 10000, "actual skin, text and widgets rasterize into true DXGI backbuffer");
            const auto   sample = Decode(owned.format, pixels[132 * owned.width + 532]);
            const double alpha = 128.0 / 255;
            Check(green ? sample.red == 0 && sample.green == sample.maximumColor && sample.blue == 0 && sample.alpha == sample.maximumAlpha
                        : Near(sample.red, alpha * sample.maximumColor + clear.red * (1 - alpha)) && Near(sample.green, clear.green * (1 - alpha)) &&
                              Near(sample.blue, clear.blue * (1 - alpha)) && sample.alpha == sample.maximumAlpha,
                  "native format channel/blend checks use raw backbuffer precision");
            Save(root / artifact, owned.format, owned.width, owned.height, pixels);
            const unsigned beforeCalls = owned.presentCalls;
            const HRESULT  result = PresentOwned(owned);
            Check(result == owned.lastPresent && owned.presentCalls == beforeCalls + 1, "forward exactly one actual Present and preserve its HRESULT");
            Hr(result, "owned hidden swapchain Present succeeds or reports successful occlusion");
            owned.renderedBuffers[beforeIndex] = true;
            std::printf("Owned Present HRESULT=%08lx beforeIndex=%u afterIndex=%u\n", static_cast<unsigned long>(result), beforeIndex,
                        owned.swapchain->GetCurrentBackBufferIndex());
            owned.Wait();
        };
        render(L"swapchain-before.bmp", false);
        InputClick(owned.gui, 170, 105);
        Check(clicks == 1, "actual existing GUI callback works on swapchain-rendered widgets");
        render(L"swapchain-after.bmp", false);
        // A blocked private queue makes in-flight leases deterministic. The
        // owner keeps DLL and target references and never calls ResizeBuffers on Busy.
        SelectCurrent(owned);
        Clear(owned, D3D12_RESOURCE_STATE_PRESENT);
        Hr(owned.queue->Wait(owned.blocker.Get(), 1), "block only owned queue before GUI frame");
        owned.blocked = true;
        Check(owned.session->RenderFrame(Frame(owned)) == GuiD3D12Status::Ok, "submit real backbuffer frame behind owned gate");
        auto*           retainedTarget = owned.target.Get();
        const auto      retainedModule = owned.guiModule;
        const ULONGLONG start = GetTickCount64();
        Check(ResizeOwned(owned, 800, 600) == GuiD3D12Status::Busy, "GUI resize refuses pending GPU target ownership");
        Check(GetTickCount64() - start < 1000 && owned.nativeResizeCalls == 0 && owned.target.Get() == retainedTarget &&
                  GetModuleHandleW(L"cgui.dll") == retainedModule && owned.gui->GetResolution().fX == 640,
              "Busy retains module/target and leaves native ResizeBuffers uncalled");
        Hr(owned.blocker->Signal(1), "release only owned gate from CPU");
        owned.blocked = false;
        owned.Wait();
        Check(ResizeOwned(owned, 800, 600) == GuiD3D12Status::Ok && owned.nativeResizeCalls == 1, "GUI retirement precedes first real native resize");
        Check(owned.gui->GetResolution().fX == 800 && owned.gui->GetResolution().fY == 600, "same real widgets observe resized display dimensions");
        widgets.second->Clear();
        delete widgets.first;
        widgets.first = owned.gui->CreateTexture();
        std::array<std::uint32_t, 16> green{};
        green.fill(0xff00ff00);
        widgets.first->LoadFromMemory(green.data(), 4, 4);
        Check(widgets.second->LoadFromTexture(widgets.first), "resized actual widget adopts new opaque texture");
        render(L"swapchain-resized.bmp", true);
        SelectCurrent(owned);
        Clear(owned, D3D12_RESOURCE_STATE_PRESENT);
        Hr(owned.queue->Wait(owned.blocker.Get(), 2), "block final owned frame for no-next-frame stop");
        owned.blocked = true;
        Check(owned.session->RenderFrame(Frame(owned)) == GuiD3D12Status::Ok, "submit final actual widget batch without a future Present");
        widgets.second->Clear();
        delete widgets.first;
        widgets.first = nullptr;
        auto* retainedSession = owned.session;
        Check(owned.session->Shutdown() == GuiD3D12Status::Busy && owned.session->Release() == GuiD3D12Status::Busy,
              "shutdown/release Busy preserves real renderer GPU leases");
        CGUI* sentinel = reinterpret_cast<CGUI*>(UINT_PTR(1));
        Check(owned.session == retainedSession && owned.target && GetModuleHandleW(L"cgui.dll") == retainedModule && owned.nativeResizeCalls == 1,
              "pending stop keeps owner session/module/targets and never resizes natively");
        Check(owned.session->GetGUI(sentinel) == GuiD3D12Status::Stopped && sentinel == reinterpret_cast<CGUI*>(UINT_PTR(1)) &&
                  owned.session->RenderFrame(Frame(owned)) == GuiD3D12Status::Stopped,
              "no-next-frame stop denies new widget borrows and GPU submissions");
        Hr(owned.blocker->Signal(2), "release final owned gate independently of another render callback");
        owned.blocked = false;
        owned.Wait();
        Check(owned.session->Shutdown() == GuiD3D12Status::Ok, "drained GUI shutdown completes without next frame");
        owned.gui = nullptr;
        Check(owned.session->Release() == GuiD3D12Status::Ok, "release stopped GUI before native buffer recreation");
        owned.session = nullptr;
        NativeResizeAfterGuiDrain(owned, 640, 480);
        Check(owned.nativeResizeCalls == 2, "second native resize occurs only after complete GUI shutdown");
        makeSession();
        widgets = makeWidgets(0xff00ff00);
        render(L"swapchain-recreated.bmp", true);
        widgets.second->Clear();
        delete widgets.first;
        widgets.first = nullptr;
        Check(owned.session->Shutdown() == GuiD3D12Status::Ok && owned.session->Release() == GuiD3D12Status::Ok, "recreated GUI drains and releases on owner");
        owned.session = nullptr;
        owned.gui = nullptr;
        CheckDebugErrors(owned);
        Check(FreeLibrary(owned.guiModule) != FALSE, "unload GUI only after all actual targets and GPU work retire");
        owned.guiModule = nullptr;
        Check(!GetModuleHandleW(L"cgui.dll"), "GUI module absent after explicit owner teardown");
        Check(FreeLibrary(owned.core) != FALSE, "unload uninitialized certified Core asset resolver");
        owned.core = nullptr;
        Check(!GetModuleHandleW(L"core.dll") && !GetModuleHandleW(L"game_de.dll"), "no Core/gameplay module survives owned fixture");
        ReleaseBuffers(owned);
        owned.swapchain.Reset();
        Check(DestroyWindow(owned.window) != FALSE, "destroy only private unshown HWND");
        owned.window = nullptr;
        owned.rtv.Reset();
        owned.list.Reset();
        owned.allocator.Reset();
        owned.info.Reset();
        debug.Reset();
        owned.blocker.Reset();
        owned.fence.Reset();
        owned.queue.Reset();
        auto*       device = owned.device.Detach();
        const ULONG references = device->Release();
        Check(references == 0, "real GUI/swapchain/host leave zero owned D3D12 device references");
        const auto distinct = std::count(owned.renderedBuffers.begin(), owned.renderedBuffers.end(), true);
        std::printf("Owned swapchain buffers=3 observedCurrentBuffers=%td presents=%u nativeResizes=%u deviceRefs=%lu\n", distinct, owned.presentCalls,
                    owned.nativeResizeCalls, static_cast<unsigned long>(references));
        std::printf("Passed %u actual D3D12 swapchain GUI host checks\n", checks);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "Owned swapchain GUI failure after %u checks: %s\n", checks, error.what());
        return 1;
    }
}
