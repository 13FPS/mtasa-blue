/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the top level directory
 * PURPOSE: Render the existing CEGUI widget and font batches on an owned D3D12 queue.
 *****************************************************************************/
#pragma once

#if defined(_WIN64)
    #include <CEGUIRenderer.h>
    #include <d3d12.h>
    #include <cstdint>
    #include <memory>

namespace mta::gui
{
    enum class D3D12GuiStatus
    {
        Ok,
        Busy,
        InvalidArgument,
        WrongThread,
        DeviceLost,
        Failed,
        Stopped
    };

    struct D3D12GuiFrameTarget
    {
        ID3D12Resource*       resource = nullptr;
        D3D12_RESOURCE_STATES before = D3D12_RESOURCE_STATE_RENDER_TARGET;
        D3D12_RESOURCE_STATES after = D3D12_RESOURCE_STATE_RENDER_TARGET;
        std::uint32_t         width = 0;
        std::uint32_t         height = 0;
    };

    // The caller owns the queue ordering and knows the target's real resource state.
    // No method waits for the GPU. A Busy shutdown retains this renderer and the
    // CEGUI System that borrows it until every submitted frame has completed.
    class D3D12GuiRenderer final : public CEGUI::Renderer
    {
    public:
        D3D12GuiRenderer(ID3D12Device* device, ID3D12CommandQueue* queue, DXGI_FORMAT targetFormat, std::uint32_t width, std::uint32_t height);
        ~D3D12GuiRenderer() override;

        D3D12GuiStatus SetFrameTarget(const D3D12GuiFrameTarget& target) noexcept;
        D3D12GuiStatus Resize(std::uint32_t width, std::uint32_t height) noexcept;
        D3D12GuiStatus PrepareForShutdown() noexcept;
        D3D12GuiStatus GetLastStatus() const noexcept;

        void            addQuad(const CEGUI::Rect&, float, const CEGUI::Texture*, const CEGUI::Rect&, const CEGUI::ColourRect&, CEGUI::QuadSplitMode,
                                const CEGUI::Image* = nullptr) override;
        bool            doRender() override;
        void            clearRenderList() override;
        void            setQueueingEnabled(bool setting) override;
        bool            isQueueingEnabled() const override;
        CEGUI::Texture* createTexture() override;
        CEGUI::Texture* createTexture(const CEGUI::String&, const CEGUI::String&) override;
        CEGUI::Texture* createTexture(float size) override;
        void            destroyTexture(CEGUI::Texture*) override;
        void            destroyAllTextures() override;
        float           getWidth() const override;
        float           getHeight() const override;
        CEGUI::Size     getSize() const override;
        CEGUI::Rect     getRect() const override;
        CEGUI::uint     getMaxTextureSize() const override;
        CEGUI::uint     getHorzScreenDPI() const override;
        CEGUI::uint     getVertScreenDPI() const override;
        void            NotifyImageInvalid(CEGUI::Image* const) override;

    private:
        struct Impl;
        std::unique_ptr<Impl> m_impl;
        friend class D3D12GuiTexture;
    };
}  // namespace mta::gui
#endif
