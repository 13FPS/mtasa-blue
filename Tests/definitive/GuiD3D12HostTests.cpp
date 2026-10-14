/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Exercise actual MTA widgets on a private offscreen D3D12/WARP queue.
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
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <thread>
#include <vector>
#include <SharedUtil.Defines.h>
#include <SString.h>
#include <gui/CGUI.h>
#include <gui/CGUID3D12.h>

using Microsoft::WRL::ComPtr;
using namespace mta::gui;
namespace
{
    unsigned                checks = 0, clicks = 0;
    constexpr std::uint32_t background = 0xff111d2b;
    void                    Check(bool value, const char* why)
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
    struct ReentrantResize
    {
        IGuiD3D12Session* session = nullptr;
        unsigned          calls = 0;
        bool              correct = true;
        bool              Sized(CGUIElement*)
        {
            ++calls;
            CGUI* sentinel = reinterpret_cast<CGUI*>(UINT_PTR(1));
            correct = correct && session->GetGUI(sentinel) == GuiD3D12Status::Busy && sentinel == reinterpret_cast<CGUI*>(UINT_PTR(1)) &&
                      session->Resize(320, 240) == GuiD3D12Status::Busy && session->Shutdown() == GuiD3D12Status::Busy &&
                      session->Release() == GuiD3D12Status::Busy;
            return true;
        }
    };

    struct Owned
    {
        ComPtr<IDXGIFactory4>              factory;
        ComPtr<IDXGIAdapter>               adapter;
        ComPtr<ID3D12Device>               device;
        ComPtr<ID3D12CommandQueue>         queue;
        ComPtr<ID3D12CommandAllocator>     allocator;
        ComPtr<ID3D12GraphicsCommandList>  list;
        ComPtr<ID3D12Fence>                fence, blocker;
        ComPtr<ID3D12Resource>             target, readback;
        ComPtr<ID3D12DescriptorHeap>       rtv;
        ComPtr<ID3D12InfoQueue>            info;
        D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
        HANDLE                             event = nullptr;
        UINT64                             nextFence = 1;
        UINT                               width = 640, height = 480;
        HMODULE                            core = nullptr, guiModule = nullptr;
        IGuiD3D12Session*                  session = nullptr;
        CGUI*                              gui = nullptr;
        bool                               blocked = false;
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

    void MakeTarget(Owned& owned)
    {
        owned.target.Reset();
        owned.readback.Reset();
        D3D12_RESOURCE_DESC desc{};
        desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        desc.Width = owned.width;
        desc.Height = owned.height;
        desc.DepthOrArraySize = desc.MipLevels = 1;
        desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
        D3D12_HEAP_PROPERTIES heap{};
        heap.Type = D3D12_HEAP_TYPE_DEFAULT;
        heap.CreationNodeMask = heap.VisibleNodeMask = 1;
        Hr(owned.device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_RENDER_TARGET, nullptr, IID_PPV_ARGS(&owned.target)),
           "create owned offscreen BGRA render target");
        owned.device->CreateRenderTargetView(owned.target.Get(), nullptr, owned.rtv->GetCPUDescriptorHandleForHeapStart());
        UINT64 bytes = 0;
        owned.device->GetCopyableFootprints(&desc, 0, 1, 0, &owned.footprint, nullptr, nullptr, &bytes);
        Check(bytes && bytes != UINT64_MAX, "owned readback footprint is valid");
        desc = {};
        desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        desc.Width = bytes;
        desc.Height = desc.DepthOrArraySize = desc.MipLevels = 1;
        desc.SampleDesc.Count = 1;
        desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        heap.Type = D3D12_HEAP_TYPE_READBACK;
        Hr(owned.device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&owned.readback)),
           "create owned GPU readback buffer");
    }

    void Clear(Owned& owned, D3D12_RESOURCE_STATES before)
    {
        owned.Begin();
        Barrier(owned.list.Get(), owned.target.Get(), before, D3D12_RESOURCE_STATE_RENDER_TARGET);
        constexpr float color[] = {17.0f / 255, 29.0f / 255, 43.0f / 255, 1};
        owned.list->ClearRenderTargetView(owned.rtv->GetCPUDescriptorHandleForHeapStart(), color, 0, nullptr);
        owned.Execute();
    }

    GuiD3D12Frame Frame(const Owned& owned, D3D12_RESOURCE_STATES before = D3D12_RESOURCE_STATE_RENDER_TARGET)
    {
        return {owned.target.Get(), static_cast<std::uint32_t>(before), static_cast<std::uint32_t>(D3D12_RESOURCE_STATE_COPY_SOURCE), owned.width,
                owned.height};
    }

    std::vector<std::uint32_t> Read(Owned& owned)
    {
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

    void Save(const std::filesystem::path& path, UINT width, UINT height, const std::vector<std::uint32_t>& pixels)
    {
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
        Check(output.good(), "save actual offscreen GUI pixel artifact");
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
    // Preserve the last completed phase even if a real dependency aborts before
    // C++ exception handling can write the terminal fixture result.
    setvbuf(stdout, nullptr, _IONBF, 0);
    setvbuf(stderr, nullptr, _IONBF, 0);
    try
    {
        Check(argc == 2 && sizeof(void*) == 8, "owned x64 D3D12 fixture root supplied");
        const std::filesystem::path root(argv[1]);
        Check(root.is_absolute(), "fixture root is an absolute owned path");
        Owned               owned;
        ComPtr<ID3D12Debug> debug;
        const bool          debugAvailable = SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug)));
        if (debugAvailable)
            debug->EnableDebugLayer();
        std::printf("D3D12 debug layer available=%u\n", debugAvailable ? 1u : 0u);
        Hr(CreateDXGIFactory1(IID_PPV_ARGS(&owned.factory)), "create actual system DXGI factory");
        Hr(owned.factory->EnumWarpAdapter(IID_PPV_ARGS(&owned.adapter)), "select actual Microsoft WARP adapter");
        Hr(D3D12CreateDevice(owned.adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&owned.device)), "create actual owned D3D12 WARP device");
        if (debugAvailable)
            Hr(owned.device.As(&owned.info), "read actual D3D12 debug queue");
        D3D12_COMMAND_QUEUE_DESC queue{};
        queue.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        Hr(owned.device->CreateCommandQueue(&queue, IID_PPV_ARGS(&owned.queue)), "create actual private direct queue");
        Hr(owned.device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&owned.allocator)), "create owned host allocator");
        Hr(owned.device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, owned.allocator.Get(), nullptr, IID_PPV_ARGS(&owned.list)),
           "create owned host command list");
        Hr(owned.list->Close(), "close initial host list");
        Hr(owned.device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&owned.fence)), "create host readback fence");
        Hr(owned.device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&owned.blocker)), "create deterministic owned queue gate");
        owned.event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        Check(owned.event != nullptr, "create bounded host GPU completion event");
        D3D12_DESCRIPTOR_HEAP_DESC rtv{};
        rtv.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        rtv.NumDescriptors = 1;
        Hr(owned.device->CreateDescriptorHeap(&rtv, IID_PPV_ARGS(&owned.rtv)), "create owned host RTV heap");
        MakeTarget(owned);
        owned.core = LoadLibraryExW((root / L"MTA" / L"core.dll").c_str(), nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
        Check(owned.core != nullptr, "load actual Core only for its existing asset-root identity");
        Check(!GetModuleHandleW(L"game_de.dll") && !GetModuleHandleW(L"game_sa.dll"), "no gameplay adapter loaded or initialized");
        owned.guiModule = LoadLibraryExW((root / L"MTA" / L"cgui.dll").c_str(), nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
        Check(owned.guiModule != nullptr, "load actual cgui with existing widgets and new renderer");
        const auto create = reinterpret_cast<CreateGuiD3D12Session>(GetProcAddress(owned.guiModule, "CreateGUID3D12Session"));
        Check(create != nullptr, "resolve additive optional D3D12 GUI factory");
        GuiD3D12CreateInfo info;
        info.device = owned.device.Get();
        info.queue = owned.queue.Get();
        info.targetFormat = DXGI_FORMAT_B8G8R8A8_UNORM;
        info.width = owned.width;
        info.height = owned.height;
        Check(!create(0, &info) && !create(GuiD3D12ApiVersion, nullptr), "reject unknown ABI and missing create information");
        auto invalid = info;
        invalid.size = 0;
        Check(!create(GuiD3D12ApiVersion, &invalid), "reject wrong create structure size without taking ownership");
        invalid = info;
        invalid.targetFormat = DXGI_FORMAT_UNKNOWN;
        Check(!create(GuiD3D12ApiVersion, &invalid), "reject unsupported target format and release failed factory ownership");
        invalid = info;
        invalid.width = 0;
        Check(!create(GuiD3D12ApiVersion, &invalid), "reject zero dimensions without constructing CEGUI");
        invalid = info;
        invalid.height = 16385;
        Check(!create(GuiD3D12ApiVersion, &invalid), "reject oversized dimensions and release failed factory ownership");
        ComPtr<ID3D12CommandQueue> copyQueue;
        queue.Type = D3D12_COMMAND_LIST_TYPE_COPY;
        Hr(owned.device->CreateCommandQueue(&queue, IID_PPV_ARGS(&copyQueue)), "create owned copy queue for negative direct-queue policy");
        invalid = info;
        invalid.queue = copyQueue.Get();
        Check(!create(GuiD3D12ApiVersion, &invalid), "reject a non-direct queue without retaining factory ownership");
        copyQueue.Reset();
        bool distinctDeviceTested = false;
        for (UINT index = 0; index < 16; ++index)
        {
            ComPtr<IDXGIAdapter1> candidate;
            if (owned.factory->EnumAdapters1(index, &candidate) == DXGI_ERROR_NOT_FOUND)
                break;
            if (!candidate)
                continue;
            DXGI_ADAPTER_DESC1 description{};
            candidate->GetDesc1(&description);
            if (description.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
                continue;
            ComPtr<ID3D12Device> anotherDevice;
            if (FAILED(D3D12CreateDevice(candidate.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&anotherDevice))) || anotherDevice.Get() == owned.device.Get())
                continue;
            ComPtr<ID3D12CommandQueue> anotherQueue;
            queue.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
            Hr(anotherDevice->CreateCommandQueue(&queue, IID_PPV_ARGS(&anotherQueue)), "create private distinct-device queue for identity negative");
            invalid = info;
            invalid.queue = anotherQueue.Get();
            Check(!create(GuiD3D12ApiVersion, &invalid), "reject a direct queue owned by another actual device");
            distinctDeviceTested = true;
            break;
        }
        std::printf("Distinct-device queue rejection tested=%u\n", distinctDeviceTested ? 1u : 0u);
        const auto fontPath = root / L"MTA" / L"cgui" / L"unifont.ttf";
        const auto heldFont = root / L"MTA" / L"cgui" / L"unifont.fixture-disabled";
        std::filesystem::rename(fontPath, heldFont);
        const bool missingFontRejected = create(GuiD3D12ApiVersion, &info) == nullptr;
        std::filesystem::rename(heldFont, fontPath);
        Check(missingFontRejected, "actual missing font unwinds CEGUI singletons and GUI ownership without an interactive dialog");
        owned.session = create(GuiD3D12ApiVersion, &info);
        Check(owned.session != nullptr, "construct actual MTA GUI/font/CEGUI D3D12 session after failed initialization");
        Check(!create(GuiD3D12ApiVersion, &info), "reject concurrent actual GUI session ownership");
        using InitGui = CGUI* (*)(IDirect3DDevice9*);
        const auto legacy = reinterpret_cast<InitGui>(GetProcAddress(owned.guiModule, "InitGUIInterface"));
        Check(legacy && legacy(nullptr) == nullptr, "legacy GUI factory cannot steal an owned D3D12 session");
        Check(owned.session->GetGUI(owned.gui) == GuiD3D12Status::Ok && owned.gui, "borrow actual unchanged CGUI interface on its owner thread");
        bool        foreignCorrect = false;
        std::thread foreign(
            [&]
            {
                CGUI* sentinel = reinterpret_cast<CGUI*>(UINT_PTR(1));
                foreignCorrect = owned.session->GetGUI(sentinel) == GuiD3D12Status::WrongThread && sentinel == reinterpret_cast<CGUI*>(UINT_PTR(1)) &&
                                 owned.session->Resize(800, 600) == GuiD3D12Status::WrongThread &&
                                 owned.session->RenderFrame(Frame(owned)) == GuiD3D12Status::WrongThread &&
                                 owned.session->Shutdown() == GuiD3D12Status::WrongThread && owned.session->Release() == GuiD3D12Status::WrongThread;
            });
        foreign.join();
        Check(foreignCorrect, "foreign owner calls preserve output and do not render, resize, destroy, or release");
        owned.gui->SetSkin("Default");
        Check(owned.gui->GetDefaultFont() && owned.gui->GetTextExtent("Actual MTA D3D12") > 0, "actual MTA FreeType font glyphs available");
        auto* window = owned.gui->CreateWnd(nullptr, "Actual MTA GUI / D3D12 WARP");
        window->SetPosition(CVector2D(40, 40));
        window->SetSize(CVector2D(430, 300));
        window->SetVisible(true);
        auto* button = owned.gui->CreateButton(static_cast<CGUIElement*>(window), "Actual widget callback");
        button->SetPosition(CVector2D(25, 45));
        button->SetSize(CVector2D(220, 45));
        button->SetVisible(true);
        button->SetClickHandler(GUI_CALLBACK(&Click));
        auto* checkbox = owned.gui->CreateCheckBox(static_cast<CGUIElement*>(window), "Actual checkbox", false);
        checkbox->SetPosition(CVector2D(25, 110));
        checkbox->SetSize(CVector2D(220, 35));
        checkbox->SetVisible(true);
        auto* edit = owned.gui->CreateEdit(static_cast<CGUIElement*>(window), "D3D12 input");
        edit->SetPosition(CVector2D(25, 170));
        edit->SetSize(CVector2D(270, 35));
        edit->SetVisible(true);
        auto* resizeProbe = owned.gui->CreateWnd(nullptr, "resize reentry guard");
        resizeProbe->SetSize(CVector2D(0.25f, 0.25f), true);
        resizeProbe->SetVisible(false);
        ReentrantResize reentry{owned.session};
        resizeProbe->SetSizedHandler(GUI_CALLBACK(&ReentrantResize::Sized, &reentry));
        auto*                               texture = owned.gui->CreateTexture();
        const std::array<std::uint32_t, 16> red = {0x80ff0000, 0x80ff0000, 0x80ff0000, 0x80ff0000, 0x80ff0000, 0x80ff0000, 0x80ff0000, 0x80ff0000,
                                                   0x80ff0000, 0x80ff0000, 0x80ff0000, 0x80ff0000, 0x80ff0000, 0x80ff0000, 0x80ff0000, 0x80ff0000};
        texture->LoadFromMemory(red.data(), 4, 4);
        Check(texture->GetD3DTexture() == nullptr, "D3D12 texture never masquerades as a D3D9 texture");
        bool unsupported = false;
        try
        {
            texture->CreateTexture(32, 32);
        }
        catch (const std::exception&)
        {
            unsupported = true;
        }
        Check(unsupported, "legacy D3D9 render-target texture creation is explicitly unavailable");
        auto* image = owned.gui->CreateStaticImage();
        image->SetPosition(CVector2D(500, 100));
        image->SetSize(CVector2D(64, 64));
        image->SetFrameEnabled(false);
        image->SetVisible(true);
        Check(image->LoadFromTexture(texture), "actual static image references actual CEGUI D3D12 memory texture");
        auto invalidFrame = Frame(owned);
        invalidFrame.resource = nullptr;
        Check(owned.session->RenderFrame(invalidFrame) == GuiD3D12Status::InvalidArgument, "reject missing target before recording GPU commands");
        invalidFrame = Frame(owned);
        invalidFrame.beforeState = UINT32_MAX;
        Check(owned.session->RenderFrame(invalidFrame) == GuiD3D12Status::InvalidArgument, "reject unsupported target state instead of guessing barriers");
        Clear(owned, D3D12_RESOURCE_STATE_RENDER_TARGET);
        Check(owned.session->RenderFrame(Frame(owned)) == GuiD3D12Status::Ok, "submit actual CEGUI skin/fonts/widget quads on WARP");
        const auto before = Read(owned);
        const auto rasterized = std::count_if(before.begin(), before.end(), [](auto pixel) { return pixel != background; });
        Check(rasterized > 10000, "actual GPU readback contains substantial widget pixels");
        const auto sample = before[132 * owned.width + 532];
        const int  r = (sample >> 16) & 255, g = (sample >> 8) & 255, b = sample & 255;
        std::printf("BGRA/straight-alpha sample=%08x red=%d green=%d blue=%d\n", sample, r, g, b);
        Check(r >= 134 && r <= 138 && g >= 13 && g <= 15 && b >= 20 && b <= 22, "CEGUI ARGB memory channels and straight alpha match actual BGRA GPU pixels");
        Save(root / L"d3d12-before.bmp", owned.width, owned.height, before);
        InputClick(owned.gui, 170, 105);
        Check(clicks == 1, "actual GUI callback fires through the D3D12-backed widget");
        InputClick(owned.gui, 80, 167);
        Check(checkbox->GetSelected(), "actual checkbox changes state through existing MTA input");
        edit->Activate();
        edit->SetCaretAtEnd();
        owned.gui->ProcessCharacter('X');
        Check(edit->GetText().find('X') != std::string::npos, "actual edit/font input works through existing widget code");
        Clear(owned, D3D12_RESOURCE_STATE_COPY_SOURCE);
        Check(owned.session->RenderFrame(Frame(owned)) == GuiD3D12Status::Ok, "submit changed actual widgets");
        const auto after = Read(owned);
        Check(after != before, "actual widget input changes rasterized GPU output");
        Save(root / L"d3d12-after.bmp", owned.width, owned.height, after);

        // Block only our private queue with a CPU-signaled fence. This makes
        // pending GPU ownership deterministic without sleeps or driver timing.
        Hr(owned.queue->Wait(owned.blocker.Get(), 1), "gate the owned queue for deterministic resource retention");
        owned.blocked = true;
        for (unsigned i = 0; i < 3; ++i)
            Check(owned.session->RenderFrame(Frame(owned, D3D12_RESOURCE_STATE_COPY_SOURCE)) == GuiD3D12Status::Ok,
                  "submit one of three fenced renderer slots");
        Check(owned.session->Resize(800, 600) == GuiD3D12Status::Busy && owned.gui->GetResolution().fX == 640,
              "pending target references prevent resize until every GPU slot retires");
        const ULONGLONG busyStarted = GetTickCount64();
        Check(owned.session->RenderFrame(Frame(owned, D3D12_RESOURCE_STATE_COPY_SOURCE)) == GuiD3D12Status::Busy,
              "a fourth pending frame returns Busy without allocator reuse");
        Check(GetTickCount64() - busyStarted < 1000, "renderer never waits on its blocked GPU queue");
        image->Clear();
        delete texture;
        texture = nullptr;
        Check(true, "destroy submitted CPU texture/image while fenced GPU versions remain owned");
        Hr(owned.blocker->Signal(1), "release the private queue gate from the CPU");
        owned.blocked = false;
        owned.Wait();
        texture = owned.gui->CreateTexture();
        std::array<std::uint32_t, 16> green{};
        green.fill(0xff00ff00);
        texture->LoadFromMemory(green.data(), 4, 4);
        Check(image->LoadFromTexture(texture), "load a replacement texture after queued old texture destruction");
        Clear(owned, D3D12_RESOURCE_STATE_COPY_SOURCE);
        Check(owned.session->RenderFrame(Frame(owned)) == GuiD3D12Status::Ok, "completed fence slots can be reused safely");
        const auto replacement = Read(owned);
        Check(replacement[132 * owned.width + 532] == 0xff00ff00, "new descriptor/texture version produces exact green without stale submitted red");
        owned.width = 800;
        owned.height = 600;
        Check(owned.session->Resize(owned.width, owned.height) == GuiD3D12Status::Ok, "resize renderer and notify the actual CEGUI display event");
        Check(reentry.calls > 0 && reentry.correct, "actual display resize callbacks cannot reenter, destroy, or borrow the session mid-operation");
        Check(owned.gui->GetResolution().fX == 800 && owned.gui->GetResolution().fY == 600, "existing GUI observes actual new dimensions");
        MakeTarget(owned);
        Clear(owned, D3D12_RESOURCE_STATE_RENDER_TARGET);
        Check(owned.session->RenderFrame(Frame(owned)) == GuiD3D12Status::Ok, "render same actual MTA widgets into a new sized target");
        const auto resized = Read(owned);
        Check(resized[132 * owned.width + 532] == 0xff00ff00, "pixel-to-clip transform preserves absolute widget coordinates after resize");
        Save(root / L"d3d12-resized.bmp", owned.width, owned.height, resized);
        image->Clear();
        delete texture;
        texture = nullptr;
        button->SetClickHandler(GUI_CALLBACK());
        resizeProbe->SetSizedHandler(GUI_CALLBACK());
        owned.gui->DestroyElementRecursive(resizeProbe);
        resizeProbe = nullptr;
        owned.gui->DestroyElementRecursive(window);
        window = nullptr;
        button = nullptr;
        checkbox = nullptr;
        edit = nullptr;
        owned.gui->DestroyElementRecursive(image);
        image = nullptr;
        owned.gui->ClearInputHandlers(INPUT_CORE);
        owned.gui->ClearInputHandlers(INPUT_MOD);
        owned.gui->Cleanup();
        Hr(owned.queue->Wait(owned.blocker.Get(), 2), "gate one final renderer frame for shutdown retention");
        owned.blocked = true;
        Check(owned.session->RenderFrame(Frame(owned, D3D12_RESOURCE_STATE_COPY_SOURCE)) == GuiD3D12Status::Ok,
              "submit final naturally empty widget batch before shutdown");
        Check(owned.session->Shutdown() == GuiD3D12Status::Busy && owned.session->Release() == GuiD3D12Status::Busy,
              "pending shutdown/release retain the real GUI renderer and module lease");
        CGUI* sentinel = reinterpret_cast<CGUI*>(UINT_PTR(1));
        Check(owned.session->GetGUI(sentinel) == GuiD3D12Status::Stopped && sentinel == reinterpret_cast<CGUI*>(UINT_PTR(1)),
              "stopping session does not lend widgets while cleanup is pending");
        Check(owned.session->RenderFrame(Frame(owned)) == GuiD3D12Status::Stopped, "stopping session cannot submit new GPU work");
        Hr(owned.blocker->Signal(2), "release final private queue gate");
        owned.blocked = false;
        owned.Wait();
        Check(owned.session->Shutdown() == GuiD3D12Status::Ok && owned.session->Shutdown() == GuiD3D12Status::Ok,
              "completed GPU work permits idempotent actual GUI destruction");
        owned.gui = nullptr;
        Check(owned.session->Release() == GuiD3D12Status::Ok, "release fully drained actual GUI session");
        owned.session = nullptr;
        CheckDebugErrors(owned);
        Check(FreeLibrary(owned.guiModule) != FALSE, "unload real cgui after explicit renderer teardown");
        owned.guiModule = nullptr;
        Check(!GetModuleHandleW(L"cgui.dll"), "actual GUI module is absent after drain and unload");
        Check(FreeLibrary(owned.core) != FALSE, "unload actual uninitialized Core asset-root owner");
        owned.core = nullptr;
        Check(!GetModuleHandleW(L"core.dll") && !GetModuleHandleW(L"game_de.dll"), "no Core/game module survives the fixture");
        owned.target.Reset();
        owned.readback.Reset();
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
        std::printf("D3D12 device references after owned release=%lu\n", static_cast<unsigned long>(references));
        Check(references == 0, "no actual GUI device references survive full renderer/texture cleanup");
        std::printf("WARP rasterized pixels=%td callbacks=%u\nPassed %u actual D3D12 GUI host checks\n", rasterized, clicks, checks);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "D3D12 GUI fixture failed after %u checks: %s\n", checks, error.what());
        return 1;
    }
}
