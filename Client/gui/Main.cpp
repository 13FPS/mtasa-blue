/*****************************************************************************
 *
 *  PROJECT:     Multi Theft Auto v1.0
 *  LICENSE:     See LICENSE in the top level directory
 *  FILE:        gui/Main.cpp
 *  PURPOSE:     Graphical User Interface entry
 *
 *  Multi Theft Auto is available from https://www.multitheftauto.com/
 *
 *****************************************************************************/

#include "StdInc.h"
#define ALLOC_STATS_MODULE_NAME "cgui"
#include "SharedUtil.hpp"
#if defined(_WIN64) && defined(MTA_WITH_DEFINITIVE_ADAPTER)
    #define MTA_GUI_EXPORTS
    #include <gui/CGUID3D12.h>
    #include "D3D12GuiRenderer.h"
    #include <atomic>
    #include <filesystem>
    #include <string_view>
#endif

CGUI_Impl* g_pGUI = NULL;

#if defined(_WIN64) && defined(MTA_WITH_DEFINITIVE_ADAPTER)
namespace
{
    // CEGUI owns process-local singletons. An explicit session and the legacy
    // factory must never construct two independent GUI systems in this DLL.
    std::atomic<unsigned> guiOwner{0};  // 0 available, 1 legacy access, 2 explicit session, 3 root configuration

    struct TemporaryGuiOwner
    {
        bool acquired;
        explicit TemporaryGuiOwner(unsigned requested)
        {
            unsigned available = 0;
            acquired = guiOwner.compare_exchange_strong(available, requested, std::memory_order_acq_rel);
        }
        ~TemporaryGuiOwner()
        {
            if (acquired)
                guiOwner.store(0, std::memory_order_release);
        }
    };

    using mta::gui::D3D12GuiRenderer;
    using mta::gui::D3D12GuiStatus;
    using mta::gui::GuiD3D12Status;

    GuiD3D12Status SessionStatus(D3D12GuiStatus status) noexcept
    {
        switch (status)
        {
            case D3D12GuiStatus::Ok:
                return GuiD3D12Status::Ok;
            case D3D12GuiStatus::Busy:
                return GuiD3D12Status::Busy;
            case D3D12GuiStatus::InvalidArgument:
                return GuiD3D12Status::InvalidArgument;
            case D3D12GuiStatus::WrongThread:
                return GuiD3D12Status::WrongThread;
            case D3D12GuiStatus::DeviceLost:
                return GuiD3D12Status::DeviceLost;
            case D3D12GuiStatus::Stopped:
                return GuiD3D12Status::Stopped;
            default:
                return GuiD3D12Status::Failed;
        }
    }

    void SetD3D12DisplaySize(CEGUI::Renderer& renderer, float width, float height)
    {
        if (!std::isfinite(width) || !std::isfinite(height) || width <= 0 || height <= 0 || width > 16384 || height > 16384 ||
            static_cast<float>(static_cast<std::uint32_t>(width)) != width || static_cast<float>(static_cast<std::uint32_t>(height)) != height ||
            static_cast<D3D12GuiRenderer&>(renderer).Resize(static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height)) != D3D12GuiStatus::Ok)
            throw std::runtime_error("D3D12 GUI display resize was not accepted");
    }

    void UnsupportedD3D12DeviceReset(CEGUI::Renderer&)
    {
        throw std::runtime_error("Legacy D3D9 GUI device reset is unavailable on D3D12");
    }

    const GuiRendererOperations d3d12Operations{SetD3D12DisplaySize, UnsupportedD3D12DeviceReset, UnsupportedD3D12DeviceReset, false};

    class GuiD3D12Session final : public mta::gui::IGuiD3D12Session
    {
        struct Operation
        {
            bool& active;
            explicit Operation(bool& value) : active(value) { active = true; }
            ~Operation() { active = false; }
        };

    public:
        explicit GuiD3D12Session(const mta::gui::GuiD3D12CreateInfo& info) : ownerThread(GetCurrentThreadId())
        {
            auto renderer = std::make_unique<D3D12GuiRenderer>(info.device, info.queue, static_cast<DXGI_FORMAT>(info.targetFormat), info.width, info.height);
            nativeRenderer = renderer.get();
            gui = std::make_unique<CGUI_Impl>(std::move(renderer), d3d12Operations);
        }

        GuiD3D12Status GetGUI(CGUI*& output) noexcept override
        {
            if (GetCurrentThreadId() != ownerThread)
                return GuiD3D12Status::WrongThread;
            if (operationActive)
                return GuiD3D12Status::Busy;
            if (stopping || !gui)
                return GuiD3D12Status::Stopped;
            output = gui.get();
            return GuiD3D12Status::Ok;
        }

        GuiD3D12Status RenderFrame(const mta::gui::GuiD3D12Frame& frame) noexcept override
        {
            if (GetCurrentThreadId() != ownerThread)
                return GuiD3D12Status::WrongThread;
            if (operationActive)
                return GuiD3D12Status::Busy;
            if (stopping || !gui)
                return GuiD3D12Status::Stopped;
            Operation                           operation(operationActive);
            const mta::gui::D3D12GuiFrameTarget target{frame.resource, static_cast<D3D12_RESOURCE_STATES>(frame.beforeState),
                                                       static_cast<D3D12_RESOURCE_STATES>(frame.afterState), frame.width, frame.height};
            const auto                          status = nativeRenderer->SetFrameTarget(target);
            if (status != D3D12GuiStatus::Ok)
                return SessionStatus(status);
            try
            {
                gui->Draw();
                const auto mainStatus = nativeRenderer->GetLastStatus();
                if (mainStatus != D3D12GuiStatus::Ok)
                    return SessionStatus(mainStatus);
                // MTA draws CEGUI's cursor after its retained widget batch. Use
                // that same path for explicit frames, and preserve either
                // batch's Busy/error instead of reporting a partial frame as Ok.
                if (gui->IsCursorEnabled())
                    gui->DrawMouseCursor();
                return SessionStatus(nativeRenderer->GetLastStatus());
            }
            catch (...)
            {
                return GuiD3D12Status::Failed;
            }
        }

        GuiD3D12Status Resize(std::uint32_t width, std::uint32_t height) noexcept override
        {
            if (GetCurrentThreadId() != ownerThread)
                return GuiD3D12Status::WrongThread;
            if (operationActive)
                return GuiD3D12Status::Busy;
            if (stopping || !gui)
                return GuiD3D12Status::Stopped;
            Operation operation(operationActive);
            return SessionStatus(nativeRenderer->Resize(width, height));
        }

        GuiD3D12Status Shutdown() noexcept override
        {
            if (GetCurrentThreadId() != ownerThread)
                return GuiD3D12Status::WrongThread;
            if (operationActive)
                return GuiD3D12Status::Busy;
            // CEGUI display and destruction callbacks may reenter this API on
            // the same thread. Keep the outer operation's GUI and lease alive.
            Operation operation(operationActive);
            stopping = true;
            if (!gui)
                return GuiD3D12Status::Ok;
            // No widget or renderer destruction is allowed while a submitted
            // frame can still own its textures/target. Busy keeps the real lease.
            const auto status = nativeRenderer->PrepareForShutdown();
            if (status != D3D12GuiStatus::Ok)
                return SessionStatus(status);
            try
            {
                gui->DestroySessionWidgets();
            }
            catch (...)
            {
                return GuiD3D12Status::Failed;
            }
            g_pGUI = nullptr;
            gui.reset();
            nativeRenderer = nullptr;
            guiOwner.store(0, std::memory_order_release);
            return GuiD3D12Status::Ok;
        }

        GuiD3D12Status Release() noexcept override
        {
            if (GetCurrentThreadId() != ownerThread)
                return GuiD3D12Status::WrongThread;
            if (operationActive)
                return GuiD3D12Status::Busy;
            const auto status = Shutdown();
            if (status != GuiD3D12Status::Ok)
                return status;
            delete this;
            return GuiD3D12Status::Ok;
        }

    private:
        DWORD                      ownerThread;
        bool                       stopping = false;
        bool                       operationActive = false;
        D3D12GuiRenderer*          nativeRenderer = nullptr;
        std::unique_ptr<CGUI_Impl> gui;
    };
}
#endif

int WINAPI DllMain(HINSTANCE hModule, DWORD dwReason, PVOID pvNothing)
{
    // Eventually destroy our GUI upon detach
    if (dwReason == DLL_PROCESS_DETACH && g_pGUI)
    {
#if defined(_WIN64) && defined(MTA_WITH_DEFINITIVE_ADAPTER)
        // Explicit D3D12 sessions drain on their owner before DLL unload. Never
        // poll fences or destroy an active GPU session under the loader lock.
        if (guiOwner.load(std::memory_order_acquire) == 2)
            return TRUE;
#endif
        delete g_pGUI;
    }

    return TRUE;
}

MTAEXPORT CGUI* InitGUIInterface(IDirect3DDevice9* pDevice)
{
#if defined(_WIN64) && defined(MTA_WITH_DEFINITIVE_ADAPTER)
    // Inspect the legacy singleton under the same lease as construction and
    // explicit root configuration; a pre-lock pointer read could race creation.
    TemporaryGuiOwner owner(1);
    if (!owner.acquired)
        return nullptr;
    try
    {
#endif
        // Create our GUI interface if not already done
        if (!g_pGUI)
        {
            SetMemoryAllocationFailureHandler();
            g_pGUI = new CGUI_Impl(pDevice);
        }
        return g_pGUI;
#if defined(_WIN64) && defined(MTA_WITH_DEFINITIVE_ADAPTER)
    }
    catch (...)
    {
        // The optional x64 owner must not propagate a C++ initialization
        // failure through the unchanged C factory boundary.
        return nullptr;
    }
#endif
}

#if defined(_WIN64) && defined(MTA_WITH_DEFINITIVE_ADAPTER)
MTAEXPORT std::uint32_t ConfigureGUID3D12AssetRoot(std::uint32_t apiVersion, const wchar_t* root, std::uint32_t length) noexcept
{
    if (apiVersion != mta::gui::GuiD3D12AssetRootApiVersion || !root || !length || length > 32767)
        return 0;
    TemporaryGuiOwner owner(3);
    if (!owner.acquired || g_pGUI)
        return 0;
    try
    {
        const std::wstring_view counted(root, length);
        if (counted.find(L'\0') != std::wstring_view::npos)
            return 0;
        const std::filesystem::path candidate{std::wstring(counted)};
        if (!candidate.is_absolute())
            return 0;
        const auto normalized = candidate.lexically_normal();
        for (const auto* required : {L"MTA/cgui", L"MTA/logs", L"skins/Default"})
            if (!std::filesystem::is_directory(normalized / required))
                return 0;
        const auto wide = normalized.wstring();
        const auto encoded = ToUTF8(wide);
        if (encoded.empty() || FromUTF8(encoded) != wide)
            return 0;
        // Core and cgui compile separate SharedUtil state. DE loads Core beside
        // the bridge rather than under a classic MTA folder, so its root setter
        // cannot configure this DLL's font/skin resolver. Validate the complete
        // directory shape, including CEGUI's log parent, before changing it.
        SharedUtil::SetMTASABaseDirOverride(encoded);
        return 1;
    }
    catch (...)
    {
        return 0;
    }
}

MTAEXPORT mta::gui::IGuiD3D12Session* CreateGUID3D12Session(std::uint32_t apiVersion, const mta::gui::GuiD3D12CreateInfo* info) noexcept
{
    if (apiVersion != mta::gui::GuiD3D12ApiVersion || !info || info->size != sizeof(*info) || !info->device || !info->queue || !info->width || !info->height)
        return nullptr;
    unsigned available = 0;
    if (!guiOwner.compare_exchange_strong(available, 2, std::memory_order_acq_rel))
        return nullptr;
    if (g_pGUI)
    {
        guiOwner.store(0, std::memory_order_release);
        return nullptr;
    }
    try
    {
        auto  session = std::make_unique<GuiD3D12Session>(*info);
        CGUI* borrowed = nullptr;
        if (session->GetGUI(borrowed) != mta::gui::GuiD3D12Status::Ok)
            throw std::runtime_error("D3D12 GUI session did not publish its owner interface");
        g_pGUI = static_cast<CGUI_Impl*>(borrowed);
        return session.release();
    }
    catch (...)
    {
        guiOwner.store(0, std::memory_order_release);
        return nullptr;
    }
}
#endif
