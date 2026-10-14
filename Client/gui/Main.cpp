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
#endif

CGUI_Impl* g_pGUI = NULL;

#if defined(_WIN64) && defined(MTA_WITH_DEFINITIVE_ADAPTER)
namespace
{
    // CEGUI owns process-local singletons. An explicit session and the legacy
    // factory must never construct two independent GUI systems in this DLL.
    std::atomic<unsigned> guiOwner{0};  // 0 available, 1 legacy construction, 2 explicit session

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
    if (guiOwner.load(std::memory_order_acquire) == 2)
        return nullptr;
#endif
    // Create our GUI interface if not already done
    if (!g_pGUI)
    {
#if defined(_WIN64) && defined(MTA_WITH_DEFINITIVE_ADAPTER)
        unsigned available = 0;
        if (!guiOwner.compare_exchange_strong(available, 1, std::memory_order_acq_rel))
            return nullptr;
        try
        {
#endif
            SetMemoryAllocationFailureHandler();
            g_pGUI = new CGUI_Impl(pDevice);
#if defined(_WIN64) && defined(MTA_WITH_DEFINITIVE_ADAPTER)
        }
        catch (...)
        {
            guiOwner.store(0, std::memory_order_release);
            // The optional x64 owner must not propagate a C++ initialization
            // failure through the unchanged C factory boundary.
            return nullptr;
        }
        guiOwner.store(0, std::memory_order_release);
#endif
    }

    // Return it
    return g_pGUI;
}

#if defined(_WIN64) && defined(MTA_WITH_DEFINITIVE_ADAPTER)
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
