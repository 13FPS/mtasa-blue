/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Optional owner-thread session for the existing GUI on D3D12
 *****************************************************************************/
#pragma once

#include <cstdint>

class CGUI;
struct ID3D12Device;
struct ID3D12CommandQueue;
struct ID3D12Resource;

namespace mta::gui
{
    constexpr std::uint32_t GuiD3D12ApiVersion = 1;
    constexpr std::uint32_t GuiD3D12AssetRootApiVersion = 1;

    enum class GuiD3D12Status : std::uint32_t
    {
        Ok,
        Busy,
        InvalidArgument,
        WrongThread,
        DeviceLost,
        Failed,
        Stopped
    };

    struct GuiD3D12CreateInfo
    {
        std::uint32_t       size = sizeof(GuiD3D12CreateInfo);
        ID3D12Device*       device = nullptr;
        ID3D12CommandQueue* queue = nullptr;
        std::uint32_t       targetFormat = 0;
        std::uint32_t       width = 0;
        std::uint32_t       height = 0;
    };

    struct GuiD3D12Frame
    {
        ID3D12Resource* resource = nullptr;
        std::uint32_t   beforeState = 0;
        std::uint32_t   afterState = 0;
        std::uint32_t   width = 0;
        std::uint32_t   height = 0;
    };

    class IGuiD3D12Session
    {
    public:
        // The GUI and all returned widgets are borrowed on the creation thread.
        // RenderFrame supplies the real target and its caller-owned state policy;
        // CGUI::Draw alone cannot supply a D3D12 frame target.
        virtual GuiD3D12Status GetGUI(CGUI*& output) noexcept = 0;
        virtual GuiD3D12Status RenderFrame(const GuiD3D12Frame& frame) noexcept = 0;
        virtual GuiD3D12Status Resize(std::uint32_t width, std::uint32_t height) noexcept = 0;
        // Busy/error retains the GUI, renderer and module lease. The owner must
        // retry before releasing the session or unloading its DLL.
        virtual GuiD3D12Status Shutdown() noexcept = 0;
        virtual GuiD3D12Status Release() noexcept = 0;

    protected:
        virtual ~IGuiD3D12Session() = default;
    };

    using CreateGuiD3D12Session = IGuiD3D12Session* (*)(std::uint32_t apiVersion, const GuiD3D12CreateInfo* info) noexcept;
    // Configure this DLL's existing asset resolver before creating any GUI.
    // The counted path need not be terminated, must be absolute, and is bounded
    // to 32767 wide characters. Failure preserves the previous root. An active
    // legacy GUI or explicit session excludes configuration on every thread.
    using ConfigureGuiD3D12AssetRoot = std::uint32_t (*)(std::uint32_t apiVersion, const wchar_t* root, std::uint32_t length) noexcept;
}

// Optional export; the original InitGUIInterface and CGUI vtable stay unchanged.
#if defined(_WIN32) && defined(MTA_GUI_EXPORTS)
extern "C" __declspec(dllexport) mta::gui::IGuiD3D12Session* CreateGUID3D12Session(std::uint32_t apiVersion, const mta::gui::GuiD3D12CreateInfo* info) noexcept;
extern "C" __declspec(dllexport) std::uint32_t ConfigureGUID3D12AssetRoot(std::uint32_t apiVersion, const wchar_t* root, std::uint32_t length) noexcept;
#else
extern "C" mta::gui::IGuiD3D12Session* CreateGUID3D12Session(std::uint32_t apiVersion, const mta::gui::GuiD3D12CreateInfo* info) noexcept;
extern "C" std::uint32_t               ConfigureGUID3D12AssetRoot(std::uint32_t apiVersion, const wchar_t* root, std::uint32_t length) noexcept;
#endif
