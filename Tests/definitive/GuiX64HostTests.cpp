/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Exercise the real x64 CGUI DLL on an owned hidden D3D9 device
 *****************************************************************************/

// The host is named SanAndreas.exe only to select Core's existing non-hooking
// DE DllMain path. It never initializes Core, loads a gameplay adapter, or
// borrows a game device/window. Every asset and output belongs to this fixture.
#include <windows.h>
#include <d3d9.h>
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <new>
#include <stdexcept>
#include <string>
#include <vector>
#include <SharedUtil.Defines.h>
#include <SString.h>
#include <gui/CGUI.h>
#include <gui/CGUID3D12.h>

namespace
{
    unsigned           checks = 0, freeClicks = 0;
    constexpr unsigned width = 640, height = 480;
    constexpr D3DCOLOR background = D3DCOLOR_XRGB(17, 29, 43);

    void Check(bool condition, const char* why)
    {
        ++checks;
        if (!condition)
            throw std::runtime_error(why);
    }
    void Hr(HRESULT result, const char* why)
    {
        if (FAILED(result))
            std::fprintf(stderr, "%s HRESULT=%08lx\n", why, static_cast<unsigned long>(result));
        Check(SUCCEEDED(result), why);
    }
    bool FreeClick(CGUIElement* element)
    {
        ++freeClicks;
        return element != nullptr;
    }

    struct LeftBase
    {
        virtual ~LeftBase() = default;
        std::uint64_t left = 0x12345678;
    };
    struct RightBase
    {
        virtual ~RightBase() = default;
        std::uint64_t right = 0x87654321;
    };
    struct VirtualBase
    {
        virtual ~VirtualBase() = default;
        std::uint64_t inherited = 0xabcdef;
    };
    struct Receiver final : LeftBase, RightBase, virtual VirtualBase
    {
        unsigned     clicks = 0, edits = 0;
        CGUIElement* last = nullptr;
        bool         Click(CGUIElement* element)
        {
            ++clicks;
            last = element;
            return element != nullptr && inherited == 0xabcdef && right == 0x87654321;
        }
        bool Edited(CGUIElement* element)
        {
            ++edits;
            last = element;
            return element != nullptr;
        }
    };

    void TestCallbackStorage()
    {
        Receiver receiver;
        struct Guarded
        {
            std::uint64_t                 before = 0x1122334455667788;
            GUI_CALLBACK                  callback;
            std::array<unsigned char, 16> after;
        } guarded;
        guarded.after.fill(0xa5);
        const auto element = reinterpret_cast<CGUIElement*>(std::uintptr_t{1});
        // Construct directly in the guarded object: assignment would first
        // overflow an unrelated temporary, then copy only the truncated bytes.
        new (&guarded.callback) GUI_CALLBACK(&Receiver::Click, &receiver);
        const auto intact = [&]
        { return guarded.before == 0x1122334455667788 && std::all_of(guarded.after.begin(), guarded.after.end(), [](unsigned char b) { return b == 0xa5; }); };
        Check(intact(), "complex method callback preserves adjacent storage");
        Check(guarded.callback && guarded.callback(element) && receiver.clicks == 1 && receiver.last == element,
              "multiple/virtual inheritance callback preserves member pointer and this adjustment");
        GUI_CALLBACK copied(guarded.callback);
        Check(copied && copied(element) && receiver.clicks == 2, "complex callback copy invokes the same receiver");
        guarded.callback = GUI_CALLBACK();
        Check(!guarded.callback && intact(), "clearing callback preserves guards");
        GUI_CALLBACK free(FreeClick);
        Check(free && free(element) && freeClicks == 1, "actual free callback copy layout remains callable");
        freeClicks = 0;
        std::printf("Callback storage=%zu alignment=%zu complexMember=%zu\n", sizeof(GUI_CALLBACK), alignof(GUI_CALLBACK), sizeof(&Receiver::Click));
    }

    std::wstring ReadFontRegistry(const wchar_t* name)
    {
        HKEY key = nullptr;
        Check(RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Fonts", 0, KEY_READ, &key) == ERROR_SUCCESS,
              "native predefined HKEY opens actual font registry without truncation");
        DWORD                bytes = 0, kind = 0;
        const auto           query = RegQueryValueExW(key, name, nullptr, &kind, nullptr, &bytes);
        std::vector<wchar_t> text(bytes / sizeof(wchar_t) + 1);
        const auto result = query == ERROR_SUCCESS ? RegQueryValueExW(key, name, nullptr, &kind, reinterpret_cast<BYTE*>(text.data()), &bytes) : query;
        RegCloseKey(key);
        Check(result == ERROR_SUCCESS && (kind == REG_SZ || kind == REG_EXPAND_SZ) && bytes >= sizeof(wchar_t), "native registered GUI font is available");
        return text.data();
    }

    struct OwnedResources
    {
        HWND               window = nullptr;
        HMODULE            core = nullptr, guiModule = nullptr;
        CGUI*              gui = nullptr;
        IDirect3D9*        d3d = nullptr;
        IDirect3DDevice9*  device = nullptr;
        IDirect3DSurface9 *target = nullptr, *readback = nullptr, *backbuffer = nullptr;
        ~OwnedResources()
        {
            if (guiModule)
                FreeLibrary(guiModule);  // Real DllMain owns CGUI_Impl deletion.
            if (core)
                FreeLibrary(core);
            if (readback)
                readback->Release();
            if (target)
                target->Release();
            if (backbuffer)
                backbuffer->Release();
            if (device)
                device->Release();
            if (d3d)
                d3d->Release();
            if (window)
                DestroyWindow(window);
        }
    };

    void MakeTargets(OwnedResources& owned)
    {
        Hr(owned.device->GetRenderTarget(0, &owned.backbuffer), "retain owned original render target");
        Hr(owned.device->CreateRenderTarget(width, height, D3DFMT_A8R8G8B8, D3DMULTISAMPLE_NONE, 0, FALSE, &owned.target, nullptr),
           "create real offscreen render target");
        Hr(owned.device->CreateOffscreenPlainSurface(width, height, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM, &owned.readback, nullptr),
           "create real readback surface");
        Hr(owned.device->SetRenderTarget(0, owned.target), "select fixture render target");
        D3DVIEWPORT9 viewport{0, 0, width, height, 0.0f, 1.0f};
        Hr(owned.device->SetViewport(&viewport), "select fixture viewport");
    }

    void ReleaseTargets(OwnedResources& owned)
    {
        Hr(owned.device->SetRenderTarget(0, owned.backbuffer), "restore original owned target before device reset");
        owned.readback->Release();
        owned.readback = nullptr;
        owned.target->Release();
        owned.target = nullptr;
        owned.backbuffer->Release();
        owned.backbuffer = nullptr;
    }

    std::vector<std::uint32_t> Render(OwnedResources& owned, bool draw)
    {
        Hr(owned.device->Clear(0, nullptr, D3DCLEAR_TARGET, background, 1.0f, 0), "clear private offscreen device");
        Hr(owned.device->BeginScene(), "begin private GUI scene");
        if (draw)
            owned.gui->Draw();
        Hr(owned.device->EndScene(), "end private GUI scene");
        Hr(owned.device->GetRenderTargetData(owned.target, owned.readback), "read actual GPU-rendered GUI pixels");
        D3DLOCKED_RECT locked{};
        Hr(owned.readback->LockRect(&locked, nullptr, D3DLOCK_READONLY), "lock owned pixel readback");
        std::vector<std::uint32_t> pixels(width * height);
        for (unsigned y = 0; y < height; ++y)
            std::memcpy(pixels.data() + y * width, static_cast<const unsigned char*>(locked.pBits) + y * locked.Pitch, width * sizeof(std::uint32_t));
        Hr(owned.readback->UnlockRect(), "unlock owned readback");
        return pixels;
    }

    void SaveBmp(const std::filesystem::path& path, const std::vector<std::uint32_t>& pixels)
    {
        BITMAPFILEHEADER file{};
        BITMAPINFOHEADER bitmap{};
        file.bfType = 0x4d42;
        file.bfOffBits = sizeof(file) + sizeof(bitmap);
        file.bfSize = file.bfOffBits + static_cast<DWORD>(pixels.size() * sizeof(std::uint32_t));
        bitmap.biSize = sizeof(bitmap);
        bitmap.biWidth = width;
        bitmap.biHeight = -static_cast<LONG>(height);
        bitmap.biPlanes = 1;
        bitmap.biBitCount = 32;
        bitmap.biCompression = BI_RGB;
        std::ofstream output(path, std::ios::binary);
        output.write(reinterpret_cast<const char*>(&file), sizeof(file));
        output.write(reinterpret_cast<const char*>(&bitmap), sizeof(bitmap));
        output.write(reinterpret_cast<const char*>(pixels.data()), static_cast<std::streamsize>(pixels.size() * sizeof(std::uint32_t)));
        Check(output.good(), "save actual offscreen GUI image");
    }

    void Click(CGUI* gui, unsigned x, unsigned y)
    {
        gui->ProcessMouseInput(CGUI_MI_MOUSEPOS, x, y);
        gui->ProcessMouseInput(CGUI_MI_MOUSEDOWN, 0, 0, LeftButton);
        gui->ProcessMouseInput(CGUI_MI_MOUSEUP, 0, 0, LeftButton);
    }
}

int wmain(int argc, wchar_t** argv)
{
    try
    {
        Check(argc == 2 && sizeof(void*) == 8, "owned x64 GUI fixture root supplied");
        const std::filesystem::path root(argv[1]);
        TestCallbackStorage();
        if (std::wcscmp(argv[1], L"--callbacks-only") == 0)
        {
            std::printf("Passed %u actual x64 GUI callback checks\n", checks);
            return 0;
        }
        Check(root.is_absolute() && std::filesystem::is_regular_file(root / L"MTA/core.dll"), "actual staged Core path exists and is absolute");
        for (const auto* font : {L"Tahoma (TrueType)", L"Tahoma Bold (TrueType)", L"Verdana (TrueType)"})
            Check(!ReadFontRegistry(font).empty(), "registered Windows GUI font has a native name");

        OwnedResources owned;
        WNDCLASSW      windowClass{};
        windowClass.lpfnWndProc = DefWindowProcW;
        windowClass.hInstance = GetModuleHandleW(nullptr);
        windowClass.lpszClassName = L"MtaGuiOwnedOfflineFixture";
        Check(RegisterClassW(&windowClass) != 0, "register owned hidden window class");
        owned.window = CreateWindowExW(0, windowClass.lpszClassName, L"MTA offline GUI fixture", WS_OVERLAPPEDWINDOW, 0, 0, width, height, nullptr, nullptr,
                                       windowClass.hInstance, nullptr);
        Check(owned.window && !IsWindowVisible(owned.window), "owned fixture window remains hidden");
        owned.d3d = Direct3DCreate9(D3D_SDK_VERSION);
        Check(owned.d3d != nullptr, "real system D3D9 factory");
        D3DPRESENT_PARAMETERS presentation{};
        presentation.BackBufferWidth = width;
        presentation.BackBufferHeight = height;
        presentation.BackBufferFormat = D3DFMT_X8R8G8B8;
        presentation.BackBufferCount = 1;
        presentation.SwapEffect = D3DSWAPEFFECT_DISCARD;
        presentation.hDeviceWindow = owned.window;
        presentation.Windowed = TRUE;
        presentation.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
        const auto create = owned.d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, owned.window,
                                                    D3DCREATE_SOFTWARE_VERTEXPROCESSING | D3DCREATE_FPU_PRESERVE, &presentation, &owned.device);
        Hr(create, "create actual private D3D9 HAL device");
        D3DADAPTER_IDENTIFIER9 adapter{};
        Hr(owned.d3d->GetAdapterIdentifier(D3DADAPTER_DEFAULT, 0, &adapter), "read owned D3D9 adapter metadata");
        std::printf("D3D9 private HAL adapter: %s\n", adapter.Description);
        MakeTargets(owned);

        const auto corePath = root / L"MTA" / L"core.dll";
        owned.core = LoadLibraryExW(corePath.c_str(), nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
        if (!owned.core)
            std::fwprintf(stderr, L"Core LoadLibrary path=%ls Win32Error=%lu\n", corePath.c_str(), GetLastError());
        Check(owned.core != nullptr, "load actual Core solely for its asset-root identity without initialization");
        Check(!GetModuleHandleW(L"game_de.dll") && !GetModuleHandleW(L"game_sa.dll") && !GetModuleHandleW(L"client.dll"),
              "no gameplay module loaded by fixture Core");
        owned.guiModule = LoadLibraryExW((root / L"MTA" / L"cgui.dll").c_str(), nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
        Check(owned.guiModule != nullptr, "load actual normal x64 cgui DLL");
        using InitGui = CGUI* (*)(IDirect3DDevice9*);
        const auto init = reinterpret_cast<InitGui>(GetProcAddress(owned.guiModule, "InitGUIInterface"));
        Check(init != nullptr, "resolve actual unchanged GUI factory ABI");
        owned.gui = init(owned.device);
        Check(owned.gui && init(owned.device) == owned.gui, "real GUI constructor and existing singleton behavior");
        const auto configureRoot = reinterpret_cast<mta::gui::ConfigureGuiD3D12AssetRoot>(GetProcAddress(owned.guiModule, "ConfigureGUID3D12AssetRoot"));
        Check(configureRoot != nullptr, "optional root export leaves the legacy factory ABI available");
        const auto poisonRoot = root / L"poison-root";
        for (const auto* part : {L"MTA/cgui", L"MTA/logs", L"skins/Default"})
            std::filesystem::create_directories(poisonRoot / part);
        const auto poisonWide = poisonRoot.wstring();
        Check(!configureRoot(mta::gui::GuiD3D12AssetRootApiVersion, poisonWide.data(), static_cast<std::uint32_t>(poisonWide.size())) &&
                  init(owned.device) == owned.gui,
              "live legacy singleton excludes root mutation and retains sequential factory behavior");
        owned.gui->SetSkin("Default");
        owned.gui->SetResolution(static_cast<float>(width), static_cast<float>(height));
        Check(owned.gui->GetResolution().fX == width && owned.gui->GetResolution().fY == height, "actual GUI resolution follows fixture device");
        Check(owned.gui->GetDefaultFont() && owned.gui->GetBoldFont() && owned.gui->GetClearFont() && owned.gui->GetSansFont(),
              "actual MTA and Windows fonts initialized");
        Check(owned.gui->GetDefaultFont()->GetFontHeight() > 0 && owned.gui->GetTextExtent("MTA GUI x64") > 0, "real FreeType font metrics available");

        Receiver    receiver;
        CGUIWindow* window = owned.gui->CreateWnd(nullptr, "Actual MTA GUI / x64");
        Check(window != nullptr, "create real MTA window widget");
        window->SetPosition(CVector2D(40, 40));
        window->SetSize(CVector2D(430, 300));
        window->SetVisible(true);
        CGUIButton* button = owned.gui->CreateButton(static_cast<CGUIElement*>(window), "Real button callback");
        Check(button != nullptr, "create real MTA button widget");
        button->SetPosition(CVector2D(25, 45));
        button->SetSize(CVector2D(220, 45));
        button->SetVisible(true);
        std::uint64_t widgetIdentity = 0x123456789abcdef0;
        button->SetUserData(&widgetIdentity);
        button->SetClickHandler(GUI_CALLBACK(&Receiver::Click, &receiver));
        CGUICheckBox* checkbox = owned.gui->CreateCheckBox(static_cast<CGUIElement*>(window), "Actual checkbox", false);
        Check(checkbox != nullptr, "create real MTA checkbox widget");
        checkbox->SetPosition(CVector2D(25, 110));
        checkbox->SetSize(CVector2D(220, 35));
        checkbox->SetVisible(true);
        CGUIEdit* edit = owned.gui->CreateEdit(static_cast<CGUIElement*>(window), "Input");
        Check(edit != nullptr, "create real MTA edit widget");
        edit->SetPosition(CVector2D(25, 170));
        edit->SetSize(CVector2D(270, 35));
        edit->SetVisible(true);
        edit->SetTextChangedHandler(GUI_CALLBACK(&Receiver::Edited, &receiver));

        const auto blank = Render(owned, false);
        Check(std::all_of(blank.begin(), blank.end(), [](std::uint32_t pixel) { return pixel == background; }),
              "blank readback contains only fixture clear color");
        const auto before = Render(owned, true);
        const auto changed = std::count_if(before.begin(), before.end(), [](std::uint32_t pixel) { return pixel != background; });
        Check(changed > 1000, "real CGUI Draw rasterizes widget geometry into owned target");
        SaveBmp(root / L"gui-before.bmp", before);
        Click(owned.gui, 150, 115);
        std::printf("Button callback count=%u identity=%d\n", receiver.clicks, receiver.last && receiver.last->GetUserData() == &widgetIdentity);
        // Existing widgets have two CGUIElement base subobjects. Verify the
        // callback's actual widget through the public ABI, not pointer equality
        // between independently adjusted bases of the same complete object.
        Check(receiver.clicks == 1 && receiver.last && receiver.last->GetType() == CGUI_BUTTON && receiver.last->GetUserData() == &widgetIdentity,
              "actual GUI mouse dispatch reaches complex inherited method callback");
        button->SetClickHandler(GUI_CALLBACK(FreeClick));
        Click(owned.gui, 150, 115);
        Check(freeClicks == 1 && receiver.clicks == 1, "actual GUI mouse dispatch uses copied free callback");
        button->SetEnabled(false);
        Click(owned.gui, 150, 115);
        Check(freeClicks == 1, "disabled real widget suppresses its click callback");
        button->SetEnabled(true);
        Click(owned.gui, 100, 170);
        Check(checkbox->GetSelected(), "actual mouse event toggles real checkbox");
        const std::string longText = "Actual MTA GUI text crosses the small string storage boundary";
        edit->SetText(longText.c_str());
        Check(edit->GetText() == longText, "actual SDK allocated string roundtrip uses the normal CRT policy");
        edit->Activate();
        edit->SetCaretAtEnd();
        owned.gui->ProcessCharacter('X');
        Check(edit->GetText() == longText + 'X' && receiver.edits > 0, "actual character injection edits text and invokes its callback");
        const auto after = Render(owned, true);
        Check(after != before, "actual widget state changes alter rendered pixels");
        SaveBmp(root / L"gui-after.bmp", after);

        owned.gui->Invalidate();
        ReleaseTargets(owned);
        Hr(owned.device->Reset(&presentation), "reset actual private D3D9 device");
        MakeTargets(owned);
        owned.gui->Restore();
        const auto restored = Render(owned, true);
        Check(std::count_if(restored.begin(), restored.end(), [](std::uint32_t pixel) { return pixel != background; }) > 1000,
              "real GUI restores rendering after device reset");
        SaveBmp(root / L"gui-restored.bmp", restored);

        button->SetClickHandler(GUI_CALLBACK());
        edit->SetTextChangedHandler(GUI_CALLBACK());
        owned.gui->DestroyElementRecursive(window);
        window = nullptr;
        button = nullptr;
        checkbox = nullptr;
        edit = nullptr;
        owned.gui->ClearInputHandlers(INPUT_CORE);
        owned.gui->ClearInputHandlers(INPUT_MOD);
        owned.gui->Cleanup();
        owned.gui = nullptr;
        Check(FreeLibrary(owned.guiModule) != FALSE, "unload real GUI and run actual global CGUI destructor on owner thread");
        owned.guiModule = nullptr;
        Check(GetModuleHandleW(L"cgui.dll") == nullptr, "no CGUI DLL remains loaded after teardown");
        Check(FreeLibrary(owned.core) != FALSE, "unload uninitialized actual Core asset-root module");
        owned.core = nullptr;
        Check(!GetModuleHandleW(L"core.dll") && !GetModuleHandleW(L"game_de.dll"), "Core and gameplay modules remain absent after fixture cleanup");
        ReleaseTargets(owned);
        const auto remainingDeviceReferences = owned.device->Release();
        owned.device = nullptr;
        std::printf("Device references after owned release=%lu\n", remainingDeviceReferences);
        // Do not drain someone else's leaked reference to make cleanup appear
        // successful. A nonzero return preserves a causal ownership failure.
        Check(remainingDeviceReferences == 0, "GUI destructor released its real device references");
        const auto remainingFactoryReferences = owned.d3d->Release();
        owned.d3d = nullptr;
        Check(remainingFactoryReferences == 0, "release actual private D3D9 factory");
        Check(DestroyWindow(owned.window) != FALSE, "destroy owned hidden window");
        owned.window = nullptr;
        Check(UnregisterClassW(windowClass.lpszClassName, windowClass.hInstance) != FALSE, "unregister owned window class");
        std::printf("Rasterized pixels=%td callbacks=%u edits=%u\n", changed, freeClicks, receiver.edits);
        std::printf("Passed %u actual x64 GUI host checks\n", checks);
        return 0;
    }
    catch (const std::exception& exception)
    {
        std::fprintf(stderr, "GUI fixture failed after %u checks: %s\n", checks, exception.what());
        return 1;
    }
}
