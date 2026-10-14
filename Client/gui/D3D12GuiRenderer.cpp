/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the top level directory
 *****************************************************************************/
#include "StdInc.h"

#if defined(_WIN64) && defined(MTA_WITH_DEFINITIVE_ADAPTER)
    #include "D3D12GuiRenderer.h"
    #include <d3dcompiler.h>
    #include <wincodec.h>
    #include <wrl/client.h>
    #include <array>
    #include <algorithm>
    #include <cmath>
    #include <limits>
    #include <memory>
    #include <unordered_map>
    #include <stdexcept>

namespace mta::gui
{
    using Microsoft::WRL::ComPtr;
    namespace
    {
        constexpr UINT FrameCount = 3;
        constexpr UINT MaximumQuads = 65536;
        constexpr UINT MaximumTexturesPerFrame = 2048;
        constexpr UINT MaximumTextureSize = 16384;

        bool SupportedTargetState(D3D12_RESOURCE_STATES state)
        {
            return state == D3D12_RESOURCE_STATE_COMMON || state == D3D12_RESOURCE_STATE_RENDER_TARGET || state == D3D12_RESOURCE_STATE_COPY_SOURCE ||
                   state == D3D12_RESOURCE_STATE_COPY_DEST || state == D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE ||
                   state == D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE ||
                   state == (D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        }

        [[noreturn]] void RendererError(const char* operation)
        {
            // CEGUI exceptions log through its System-owned Logger singleton.
            // Renderer construction precedes System construction, so admission
            // and device failures must also work before that logger exists.
            if (CEGUI::Logger::getSingletonPtr())
                throw CEGUI::RendererException(operation);
            throw std::runtime_error(operation);
        }

        void Check(HRESULT result, const char* operation)
        {
            if (FAILED(result))
                RendererError(operation);
        }

        D3D12_RESOURCE_DESC BufferDesc(UINT64 bytes)
        {
            D3D12_RESOURCE_DESC desc{};
            desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
            desc.Width = bytes;
            desc.Height = desc.DepthOrArraySize = desc.MipLevels = 1;
            desc.SampleDesc.Count = 1;
            desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
            return desc;
        }

        ComPtr<ID3D12Resource> MakeBuffer(ID3D12Device* device, UINT64 bytes)
        {
            D3D12_HEAP_PROPERTIES heap{};
            heap.Type = D3D12_HEAP_TYPE_UPLOAD;
            heap.CreationNodeMask = heap.VisibleNodeMask = 1;
            const auto             desc = BufferDesc(bytes);
            ComPtr<ID3D12Resource> resource;
            Check(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&resource)),
                  "D3D12 GUI upload buffer creation failed");
            return resource;
        }

        void Transition(ID3D12GraphicsCommandList* list, ID3D12Resource* resource, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after)
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

        struct GpuTexture
        {
            ComPtr<ID3D12Resource>             texture;
            ComPtr<ID3D12Resource>             upload;
            D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
            bool                               submitted = false;
        };

        struct Vertex
        {
            float x, y, u, v;
            float r, g, b, a;
        };

        const char Shaders[] = R"(
cbuffer Display : register(b0) { float2 size; };
struct Input { float2 position:POSITION; float2 uv:TEXCOORD0; float4 color:COLOR0; };
struct Output { float4 position:SV_POSITION; float2 uv:TEXCOORD0; float4 color:COLOR0; };
Output VS(Input input) {
    Output output;
    output.position=float4(input.position.x*2.0/size.x-1.0,1.0-input.position.y*2.0/size.y,0,1);
    output.uv=input.uv; output.color=input.color; return output;
}
Texture2D image:register(t0);
SamplerState imageSampler:register(s0);
float4 PS(Output input):SV_TARGET { return image.Sample(imageSampler,input.uv)*input.color; }
)";
    }

    class D3D12GuiTexture final : public CEGUI::Texture
    {
    public:
        explicit D3D12GuiTexture(D3D12GuiRenderer* owner) : Texture(owner) {}
        CEGUI::ushort getWidth() const override { return static_cast<CEGUI::ushort>(width); }
        CEGUI::ushort getHeight() const override { return static_cast<CEGUI::ushort>(height); }
        void          loadFromFile(const CEGUI::String&, const CEGUI::String&) override;
        void          loadFromMemory(const void*, CEGUI::uint, CEGUI::uint) override;

        UINT                        width = 0, height = 0;
        std::vector<std::uint32_t>  pixels;
        std::shared_ptr<GpuTexture> gpu;
    };

    struct D3D12GuiRenderer::Impl
    {
        struct Quad
        {
            CEGUI::Rect            destination, uv;
            CEGUI::ColourRect      colours;
            const D3D12GuiTexture* texture;
            const CEGUI::Image*    image;
            float                  z;
            CEGUI::QuadSplitMode   split;
        };
        struct Frame
        {
            ComPtr<ID3D12CommandAllocator>           allocator;
            ComPtr<ID3D12GraphicsCommandList>        list;
            ComPtr<ID3D12DescriptorHeap>             srv, rtv;
            ComPtr<ID3D12Resource>                   vertices, target;
            std::vector<std::shared_ptr<GpuTexture>> textures;
            UINT64                                   vertexCapacity = 0, fenceValue = 0;
            bool                                     untrackedSubmission = false;
        };

        DWORD                         ownerThread = GetCurrentThreadId();
        ComPtr<ID3D12Device>          device;
        ComPtr<IUnknown>              deviceIdentity;
        ComPtr<ID3D12CommandQueue>    queue;
        ComPtr<ID3D12Fence>           fence;
        ComPtr<ID3D12RootSignature>   root;
        ComPtr<ID3D12PipelineState>   pipeline;
        ComPtr<IWICImagingFactory>    wic;
        std::array<Frame, FrameCount> frames;
        std::vector<Quad>             quads;
        std::vector<D3D12GuiTexture*> textures;
        ComPtr<ID3D12Resource>        pendingTarget;
        ComPtr<ID3D12Resource>        directTarget;
        D3D12GuiFrameTarget           target;
        DXGI_FORMAT                   format;
        UINT                          width, height, descriptorSize = 0;
        UINT64                        nextFence = 1;
        D3D12GuiStatus                status = D3D12GuiStatus::Ok;
        bool                          queueing = true, stopped = false, comInitialized = false;

        Impl(ID3D12Device* d, ID3D12CommandQueue* q, DXGI_FORMAT f, UINT w, UINT h) : device(d), queue(q), format(f), width(w), height(h) {}
        ~Impl()
        {
            // COM apartment ownership belongs to the same thread as this renderer.
            wic.Reset();
            if (comInitialized)
                CoUninitialize();
        }
        D3D12GuiStatus SetStatus(D3D12GuiStatus value) noexcept { return status = value; }
        D3D12GuiStatus Poll() noexcept
        {
            const UINT64 completed = fence->GetCompletedValue();
            if (completed == UINT64_MAX)
                return SetStatus(D3D12GuiStatus::DeviceLost);
            bool busy = false;
            for (auto& frame : frames)
            {
                // A failed queue Signal must not free commands already submitted.
                // Retry tracking them on the same ordered queue without waiting.
                if (frame.untrackedSubmission)
                {
                    const UINT64 value = nextFence++;
                    if (FAILED(queue->Signal(fence.Get(), value)))
                    {
                        busy = true;
                        continue;
                    }
                    frame.fenceValue = value;
                    frame.untrackedSubmission = false;
                }
                if (frame.fenceValue && completed < frame.fenceValue)
                    busy = true;
                else
                {
                    if (frame.fenceValue)
                        for (const auto& texture : frame.textures)
                            texture->upload.Reset();
                    frame.fenceValue = 0;
                    frame.target.Reset();
                    frame.textures.clear();
                }
            }
            return busy ? D3D12GuiStatus::Busy : D3D12GuiStatus::Ok;
        }
        std::shared_ptr<GpuTexture> CreateGpu(D3D12GuiTexture& source)
        {
            auto                result = std::make_shared<GpuTexture>();
            D3D12_RESOURCE_DESC desc{};
            desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
            desc.Width = source.width;
            desc.Height = source.height;
            desc.DepthOrArraySize = desc.MipLevels = 1;
            // CEGUI's 0xAARRGGBB memory contract is BGRA on little-endian Windows.
            desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
            desc.SampleDesc.Count = 1;
            D3D12_HEAP_PROPERTIES heap{};
            heap.Type = D3D12_HEAP_TYPE_DEFAULT;
            heap.CreationNodeMask = heap.VisibleNodeMask = 1;
            Check(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&result->texture)),
                  "D3D12 GUI texture creation failed");
            UINT   rows = 0;
            UINT64 rowBytes = 0, total = 0;
            device->GetCopyableFootprints(&desc, 0, 1, 0, &result->footprint, &rows, &rowBytes, &total);
            if (!total || total == UINT64_MAX || rows != source.height || rowBytes != UINT64(source.width) * 4)
                RendererError("D3D12 GUI texture footprint is invalid");
            result->upload = MakeBuffer(device.Get(), total);
            void*             mapped = nullptr;
            const D3D12_RANGE noRead{};
            Check(result->upload->Map(0, &noRead, &mapped), "D3D12 GUI texture mapping failed");
            auto* bytes = static_cast<std::uint8_t*>(mapped) + result->footprint.Offset;
            for (UINT row = 0; row < rows; ++row)
                memcpy(bytes + SIZE_T(row) * result->footprint.Footprint.RowPitch, source.pixels.data() + SIZE_T(row) * source.width,
                       static_cast<SIZE_T>(rowBytes));
            result->upload->Unmap(0, nullptr);
            return result;
        }
    };

    D3D12GuiRenderer::D3D12GuiRenderer(ID3D12Device* device, ID3D12CommandQueue* queue, DXGI_FORMAT format, std::uint32_t width, std::uint32_t height)
    {
        if (!device || !queue || !width || !height || width > 16384 || height > 16384 ||
            (format != DXGI_FORMAT_R8G8B8A8_UNORM && format != DXGI_FORMAT_B8G8R8A8_UNORM) || queue->GetDesc().Type != D3D12_COMMAND_LIST_TYPE_DIRECT)
            RendererError("D3D12 GUI requires an owned device, direct queue, and supported target");
        ComPtr<ID3D12Device> queueDevice;
        Check(queue->GetDevice(IID_PPV_ARGS(&queueDevice)), "D3D12 GUI queue device lookup failed");
        ComPtr<IUnknown> queueIdentity, deviceIdentity;
        Check(queueDevice.As(&queueIdentity), "D3D12 GUI queue device identity lookup failed");
        Check(device->QueryInterface(IID_PPV_ARGS(&deviceIdentity)), "D3D12 GUI device identity lookup failed");
        if (queueIdentity.Get() != deviceIdentity.Get())
            RendererError("D3D12 GUI queue belongs to another device");
        auto impl = std::make_unique<Impl>(device, queue, format, width, height);
        impl->deviceIdentity = std::move(deviceIdentity);
        const HRESULT apartment = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        if (FAILED(apartment) && apartment != RPC_E_CHANGED_MODE)
            Check(apartment, "D3D12 GUI COM apartment initialization failed");
        impl->comInitialized = SUCCEEDED(apartment);
        Check(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&impl->wic)), "D3D12 GUI WIC initialization failed");
        Check(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&impl->fence)), "D3D12 GUI fence creation failed");
        impl->descriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

        D3D12_DESCRIPTOR_RANGE range{};
        range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        range.NumDescriptors = 1;
        D3D12_ROOT_PARAMETER parameters[2]{};
        parameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
        parameters[0].Constants.Num32BitValues = 2;
        parameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
        parameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        parameters[1].DescriptorTable.NumDescriptorRanges = 1;
        parameters[1].DescriptorTable.pDescriptorRanges = &range;
        parameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
        D3D12_STATIC_SAMPLER_DESC sampler{};
        sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
        sampler.AddressU = sampler.AddressV = sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        // Descriptor enums/ranges remain valid even when the chosen filter or
        // disabled tests do not use them; strict API validation still sees them.
        sampler.MaxAnisotropy = 1;
        sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
        sampler.MaxLOD = D3D12_FLOAT32_MAX;
        sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
        D3D12_ROOT_SIGNATURE_DESC rootDesc{};
        rootDesc.NumParameters = 2;
        rootDesc.pParameters = parameters;
        rootDesc.NumStaticSamplers = 1;
        rootDesc.pStaticSamplers = &sampler;
        rootDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
        ComPtr<ID3DBlob> rootBlob, errors, vs, ps;
        Check(D3D12SerializeRootSignature(&rootDesc, D3D_ROOT_SIGNATURE_VERSION_1, &rootBlob, &errors), "D3D12 GUI root signature serialization failed");
        Check(device->CreateRootSignature(0, rootBlob->GetBufferPointer(), rootBlob->GetBufferSize(), IID_PPV_ARGS(&impl->root)),
              "D3D12 GUI root signature creation failed");
        Check(D3DCompile(Shaders, sizeof(Shaders), "MTA CEGUI D3D12", nullptr, nullptr, "VS", "vs_5_0", D3DCOMPILE_ENABLE_STRICTNESS, 0, &vs, &errors),
              "D3D12 GUI vertex shader compilation failed");
        Check(D3DCompile(Shaders, sizeof(Shaders), "MTA CEGUI D3D12", nullptr, nullptr, "PS", "ps_5_0", D3DCOMPILE_ENABLE_STRICTNESS, 0, &ps, &errors),
              "D3D12 GUI pixel shader compilation failed");
        const D3D12_INPUT_ELEMENT_DESC     elements[] = {{"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
                                                         {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 8, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
                                                         {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 16, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}};
        D3D12_GRAPHICS_PIPELINE_STATE_DESC pso{};
        pso.pRootSignature = impl->root.Get();
        pso.VS = {vs->GetBufferPointer(), vs->GetBufferSize()};
        pso.PS = {ps->GetBufferPointer(), ps->GetBufferSize()};
        pso.InputLayout = {elements, 3};
        pso.SampleMask = UINT_MAX;
        pso.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
        pso.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
        pso.RasterizerState.DepthClipEnable = TRUE;
        auto& blend = pso.BlendState.RenderTarget[0];
        blend.BlendEnable = TRUE;
        blend.SrcBlend = D3D12_BLEND_SRC_ALPHA;
        blend.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
        blend.BlendOp = D3D12_BLEND_OP_ADD;
        blend.SrcBlendAlpha = D3D12_BLEND_ONE;
        blend.DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
        blend.BlendOpAlpha = D3D12_BLEND_OP_ADD;
        blend.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
        pso.DepthStencilState.DepthEnable = FALSE;
        pso.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
        pso.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
        pso.DepthStencilState.StencilEnable = FALSE;
        pso.DepthStencilState.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
        pso.DepthStencilState.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;
        pso.DepthStencilState.FrontFace = {D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_COMPARISON_FUNC_ALWAYS};
        pso.DepthStencilState.BackFace = pso.DepthStencilState.FrontFace;
        pso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        pso.NumRenderTargets = 1;
        pso.RTVFormats[0] = format;
        pso.SampleDesc.Count = 1;
        Check(device->CreateGraphicsPipelineState(&pso, IID_PPV_ARGS(&impl->pipeline)), "D3D12 GUI pipeline creation failed");
        for (auto& frame : impl->frames)
        {
            Check(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&frame.allocator)), "D3D12 GUI allocator creation failed");
            Check(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, frame.allocator.Get(), impl->pipeline.Get(), IID_PPV_ARGS(&frame.list)),
                  "D3D12 GUI command list creation failed");
            Check(frame.list->Close(), "D3D12 GUI initial command list close failed");
            D3D12_DESCRIPTOR_HEAP_DESC heap{};
            heap.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
            heap.NumDescriptors = MaximumTexturesPerFrame;
            heap.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
            Check(device->CreateDescriptorHeap(&heap, IID_PPV_ARGS(&frame.srv)), "D3D12 GUI SRV heap creation failed");
            heap.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
            heap.NumDescriptors = 1;
            heap.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
            Check(device->CreateDescriptorHeap(&heap, IID_PPV_ARGS(&frame.rtv)), "D3D12 GUI RTV heap creation failed");
        }
        d_identifierString = "MTA CEGUI Direct3D12 renderer";
        m_impl = std::move(impl);
    }

    D3D12GuiRenderer::~D3D12GuiRenderer()
    {
        // The session must retain ownership on Busy. Silently destroying an active
        // allocator or descriptor heap would turn a cleanup error into GPU use-after-free.
        if (m_impl && m_impl->Poll() == D3D12GuiStatus::Busy)
            std::terminate();
        destroyAllTextures();
    }

    D3D12GuiStatus D3D12GuiRenderer::GetLastStatus() const noexcept
    {
        return m_impl->status;
    }

    D3D12GuiStatus D3D12GuiRenderer::SetFrameTarget(const D3D12GuiFrameTarget& target) noexcept
    {
        auto& impl = *m_impl;
        if (GetCurrentThreadId() != impl.ownerThread)
            return D3D12GuiStatus::WrongThread;
        if (impl.stopped)
            return impl.SetStatus(D3D12GuiStatus::Stopped);
        if (!target.resource || !target.width || !target.height || target.width != impl.width || target.height != impl.height ||
            !SupportedTargetState(target.before) || !SupportedTargetState(target.after))
            return impl.SetStatus(D3D12GuiStatus::InvalidArgument);
        const auto           desc = target.resource->GetDesc();
        ComPtr<ID3D12Device> device;
        ComPtr<IUnknown>     identity;
        if (FAILED(target.resource->GetDevice(IID_PPV_ARGS(&device))) || FAILED(device.As(&identity)) || identity.Get() != impl.deviceIdentity.Get() ||
            desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D || desc.Width != target.width || desc.Height != target.height || desc.Format != impl.format ||
            desc.SampleDesc.Count != 1 || desc.DepthOrArraySize != 1 || desc.MipLevels != 1 || !(desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET))
            return impl.SetStatus(D3D12GuiStatus::InvalidArgument);
        impl.target = target;
        impl.pendingTarget = target.resource;
        impl.directTarget.Reset();
        return impl.SetStatus(D3D12GuiStatus::Ok);
    }

    D3D12GuiStatus D3D12GuiRenderer::Resize(std::uint32_t width, std::uint32_t height) noexcept
    {
        auto& impl = *m_impl;
        if (GetCurrentThreadId() != impl.ownerThread)
            return D3D12GuiStatus::WrongThread;
        if (impl.stopped)
            return impl.SetStatus(D3D12GuiStatus::Stopped);
        if (!width || !height || width > 16384 || height > 16384)
            return impl.SetStatus(D3D12GuiStatus::InvalidArgument);
        const auto retirement = impl.Poll();
        if (retirement != D3D12GuiStatus::Ok)
            return impl.SetStatus(retirement);
        try
        {
            impl.width = width;
            impl.height = height;
            impl.pendingTarget.Reset();
            impl.directTarget.Reset();
            impl.quads.clear();
            CEGUI::EventArgs args;
            fireEvent(EventDisplaySizeChanged, args, EventNamespace);
            return impl.SetStatus(D3D12GuiStatus::Ok);
        }
        catch (...)
        {
            return impl.SetStatus(D3D12GuiStatus::Failed);
        }
    }

    D3D12GuiStatus D3D12GuiRenderer::PrepareForShutdown() noexcept
    {
        auto& impl = *m_impl;
        if (GetCurrentThreadId() != impl.ownerThread)
            return D3D12GuiStatus::WrongThread;
        impl.stopped = true;
        impl.pendingTarget.Reset();
        impl.directTarget.Reset();
        const auto result = impl.Poll();
        // Device removal completes resource retirement without pretending rendering succeeded.
        return impl.SetStatus(result == D3D12GuiStatus::DeviceLost ? D3D12GuiStatus::Ok : result);
    }

    void D3D12GuiRenderer::addQuad(const CEGUI::Rect& destination, float z, const CEGUI::Texture* texture, const CEGUI::Rect& uv,
                                   const CEGUI::ColourRect& colours, CEGUI::QuadSplitMode split, const CEGUI::Image* image)
    {
        auto& impl = *m_impl;
        if (GetCurrentThreadId() != impl.ownerThread || impl.stopped || !texture || texture->getRenderer() != this || !std::isfinite(z) ||
            impl.quads.size() >= MaximumQuads)
            RendererError("D3D12 GUI quad ownership or capacity is invalid");
        const Impl::Quad quad{destination, uv, colours, static_cast<const D3D12GuiTexture*>(texture), image, z, split};
        // A cursor drawn after a failed main batch cannot repair that frame. Keep
        // its first failure so callers observe Busy instead of a missing target.
        if (!impl.queueing && impl.status != D3D12GuiStatus::Ok)
            return;
        if (impl.queueing)
            impl.quads.push_back(quad);
        else
        {
            // CEGUI draws the cursor outside its retained main batch. Do not append
            // that transient quad to the cached widget list or replay the widgets.
            auto retained = std::move(impl.quads);
            try
            {
                impl.quads.push_back(quad);
                if (!impl.pendingTarget)
                    impl.pendingTarget = impl.directTarget;
                doRender();
            }
            catch (...)
            {
                impl.quads = std::move(retained);
                throw;
            }
            impl.quads = std::move(retained);
        }
    }

    bool D3D12GuiRenderer::doRender()
    {
        auto& impl = *m_impl;
        if (GetCurrentThreadId() != impl.ownerThread)
            return false;
        if (impl.stopped || !impl.pendingTarget)
        {
            impl.SetStatus(impl.stopped ? D3D12GuiStatus::Stopped : D3D12GuiStatus::InvalidArgument);
            return false;
        }
        const auto poll = impl.Poll();
        if (poll == D3D12GuiStatus::DeviceLost)
            return false;
        auto slot = std::find_if(impl.frames.begin(), impl.frames.end(), [](const Impl::Frame& f) { return !f.fenceValue && !f.untrackedSubmission; });
        if (slot == impl.frames.end())
        {
            impl.pendingTarget.Reset();
            impl.SetStatus(D3D12GuiStatus::Busy);
            return false;
        }
        auto& frame = *slot;
        bool  recording = false;
        try
        {
            // All descriptors, vertices, and texture versions belong to this slot
            // until its fence completes; CPU texture mutation never edits live GPU data.
            std::stable_sort(impl.quads.begin(), impl.quads.end(), [](const Impl::Quad& a, const Impl::Quad& b) { return a.z > b.z; });
            std::unordered_map<const D3D12GuiTexture*, UINT> indices;
            for (const auto& quad : impl.quads)
            {
                if (!quad.texture->width || !quad.texture->height || quad.texture->pixels.empty())
                    RendererError("D3D12 GUI quad references an empty texture");
                if (indices.find(quad.texture) != indices.end())
                    continue;
                if (indices.size() >= MaximumTexturesPerFrame)
                    RendererError("D3D12 GUI frame exceeds texture capacity");
                auto& mutableTexture = *const_cast<D3D12GuiTexture*>(quad.texture);
                if (!mutableTexture.gpu)
                    mutableTexture.gpu = impl.CreateGpu(mutableTexture);
                indices.emplace(quad.texture, static_cast<UINT>(frame.textures.size()));
                frame.textures.push_back(mutableTexture.gpu);
            }
            const UINT64 bytes = std::max<UINT64>(sizeof(Vertex), UINT64(impl.quads.size()) * 6 * sizeof(Vertex));
            if (frame.vertexCapacity < bytes)
            {
                frame.vertices = MakeBuffer(impl.device.Get(), bytes);
                frame.vertexCapacity = bytes;
            }
            Vertex*           vertices = nullptr;
            const D3D12_RANGE noRead{};
            Check(frame.vertices->Map(0, &noRead, reinterpret_cast<void**>(&vertices)), "D3D12 GUI vertex mapping failed");
            UINT vertexIndex = 0;
            for (const auto& quad : impl.quads)
            {
                const auto make = [](float x, float y, float u, float v, const CEGUI::colour& colour)
                { return Vertex{x, y, u, v, colour.getRed(), colour.getGreen(), colour.getBlue(), colour.getAlpha()}; };
                const auto&  p = quad.destination;
                const auto&  t = quad.uv;
                const Vertex corners[] = {make(p.d_left, p.d_top, t.d_left, t.d_top, quad.colours.d_top_left),
                                          make(p.d_right, p.d_top, t.d_right, t.d_top, quad.colours.d_top_right),
                                          make(p.d_left, p.d_bottom, t.d_left, t.d_bottom, quad.colours.d_bottom_left),
                                          make(p.d_right, p.d_bottom, t.d_right, t.d_bottom, quad.colours.d_bottom_right)};
                const UINT   diagonalA[] = {0, 3, 2, 1, 3, 0}, diagonalB[] = {0, 1, 2, 1, 3, 2};
                const UINT*  order = quad.split == CEGUI::TopLeftToBottomRight ? diagonalA : diagonalB;
                for (UINT i = 0; i < 6; ++i)
                    vertices[vertexIndex++] = corners[order[i]];
                if (quad.image && quad.image->getCodepoint() > 127 && quad.image->getCodepoint() < 65534 && quad.image->getFont())
                    quad.image->getFont()->refreshCachedGlyph(quad.image->getCodepoint());
            }
            frame.vertices->Unmap(0, nullptr);
            Check(frame.allocator->Reset(), "D3D12 GUI allocator reset failed");
            Check(frame.list->Reset(frame.allocator.Get(), impl.pipeline.Get()), "D3D12 GUI command reset failed");
            recording = true;
            auto cpu = frame.srv->GetCPUDescriptorHandleForHeapStart();
            for (const auto& texture : frame.textures)
            {
                if (!texture->submitted)
                {
                    D3D12_TEXTURE_COPY_LOCATION destination{};
                    destination.pResource = texture->texture.Get();
                    destination.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
                    D3D12_TEXTURE_COPY_LOCATION source{};
                    source.pResource = texture->upload.Get();
                    source.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
                    source.PlacedFootprint = texture->footprint;
                    frame.list->CopyTextureRegion(&destination, 0, 0, 0, &source, nullptr);
                    Transition(frame.list.Get(), texture->texture.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
                }
                D3D12_SHADER_RESOURCE_VIEW_DESC srv{};
                srv.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
                srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
                srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
                srv.Texture2D.MipLevels = 1;
                impl.device->CreateShaderResourceView(texture->texture.Get(), &srv, cpu);
                cpu.ptr += impl.descriptorSize;
            }
            frame.target = impl.pendingTarget;
            impl.pendingTarget.Reset();
            const auto rtv = frame.rtv->GetCPUDescriptorHandleForHeapStart();
            impl.device->CreateRenderTargetView(frame.target.Get(), nullptr, rtv);
            Transition(frame.list.Get(), frame.target.Get(), impl.target.before, D3D12_RESOURCE_STATE_RENDER_TARGET);
            frame.list->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
            frame.list->SetGraphicsRootSignature(impl.root.Get());
            ID3D12DescriptorHeap* heaps[] = {frame.srv.Get()};
            frame.list->SetDescriptorHeaps(1, heaps);
            const float size[] = {static_cast<float>(impl.width), static_cast<float>(impl.height)};
            frame.list->SetGraphicsRoot32BitConstants(0, 2, size, 0);
            const D3D12_VIEWPORT viewport{0, 0, size[0], size[1], 0, 1};
            const D3D12_RECT     scissor{0, 0, static_cast<LONG>(impl.width), static_cast<LONG>(impl.height)};
            frame.list->RSSetViewports(1, &viewport);
            frame.list->RSSetScissorRects(1, &scissor);
            frame.list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            const D3D12_VERTEX_BUFFER_VIEW vb{frame.vertices->GetGPUVirtualAddress(), static_cast<UINT>(bytes), sizeof(Vertex)};
            frame.list->IASetVertexBuffers(0, 1, &vb);
            const auto gpuBase = frame.srv->GetGPUDescriptorHandleForHeapStart();
            UINT       startVertex = 0;
            for (const auto& quad : impl.quads)
            {
                auto gpu = gpuBase;
                gpu.ptr += UINT64(indices.at(quad.texture)) * impl.descriptorSize;
                frame.list->SetGraphicsRootDescriptorTable(1, gpu);
                frame.list->DrawInstanced(6, 1, startVertex, 0);
                startVertex += 6;
            }
            Transition(frame.list.Get(), frame.target.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, impl.target.after);
            Check(frame.list->Close(), "D3D12 GUI command close failed");
            recording = false;
            ID3D12CommandList* lists[] = {frame.list.Get()};
            frame.untrackedSubmission = true;
            impl.queue->ExecuteCommandLists(1, lists);
            for (const auto& texture : frame.textures)
                texture->submitted = true;
            const UINT64 value = impl.nextFence++;
            if (FAILED(impl.queue->Signal(impl.fence.Get(), value)))
            {
                impl.stopped = true;
                impl.SetStatus(D3D12GuiStatus::Failed);
                return false;
            }
            frame.fenceValue = value;
            frame.untrackedSubmission = false;
            impl.directTarget = frame.target;
            impl.target.before = impl.target.after;
            impl.SetStatus(D3D12GuiStatus::Ok);
            return true;
        }
        catch (...)
        {
            if (recording)
                frame.list->Close();
            impl.pendingTarget.Reset();
            frame.target.Reset();
            frame.textures.clear();
            impl.SetStatus(D3D12GuiStatus::Failed);
            return false;
        }
    }

    void D3D12GuiTexture::loadFromMemory(const void* data, CEGUI::uint bufferWidth, CEGUI::uint bufferHeight)
    {
        if (!data || !bufferWidth || !bufferHeight || bufferWidth > MaximumTextureSize || bufferHeight > MaximumTextureSize)
            RendererError("D3D12 GUI memory texture dimensions are invalid");
        auto* owner = static_cast<D3D12GuiRenderer*>(getRenderer());
        if (GetCurrentThreadId() != owner->m_impl->ownerThread || owner->m_impl->stopped)
            RendererError("D3D12 GUI texture mutation requires its active owner thread");
        UINT size = 1;
        while (size < std::max(bufferWidth, bufferHeight))
            size *= 2;
        std::vector<std::uint32_t> replacement(SIZE_T(size) * size, 0);
        for (UINT row = 0; row < bufferHeight; ++row)
            memcpy(replacement.data() + SIZE_T(row) * size, static_cast<const std::uint32_t*>(data) + SIZE_T(row) * bufferWidth, SIZE_T(bufferWidth) * 4);
        pixels.swap(replacement);
        width = height = size;
        gpu.reset();
    }

    void D3D12GuiTexture::loadFromFile(const CEGUI::String& filename, const CEGUI::String& group)
    {
        auto* owner = static_cast<D3D12GuiRenderer*>(getRenderer());
        if (GetCurrentThreadId() != owner->m_impl->ownerThread || owner->m_impl->stopped)
            RendererError("D3D12 GUI file texture load requires its active owner thread");
        CEGUI::RawDataContainer data;
        auto*                   provider = CEGUI::System::getSingleton().getResourceProvider();
        provider->loadRawDataContainer(filename, data, group);
        try
        {
            if (!data.getSize() || data.getSize() > UINT_MAX)
                RendererError("D3D12 GUI image file exceeds WIC stream limits");
            ComPtr<IWICStream>            stream;
            ComPtr<IWICBitmapDecoder>     decoder;
            ComPtr<IWICBitmapFrameDecode> frame;
            ComPtr<IWICFormatConverter>   converter;
            auto*                         factory = owner->m_impl->wic.Get();
            Check(factory->CreateStream(&stream), "D3D12 GUI WIC stream creation failed");
            Check(stream->InitializeFromMemory(data.getDataPtr(), static_cast<DWORD>(data.getSize())), "D3D12 GUI image stream initialization failed");
            Check(factory->CreateDecoderFromStream(stream.Get(), nullptr, WICDecodeMetadataCacheOnLoad, &decoder), "D3D12 GUI image decoding failed");
            Check(decoder->GetFrame(0, &frame), "D3D12 GUI image frame lookup failed");
            UINT w = 0, h = 0;
            Check(frame->GetSize(&w, &h), "D3D12 GUI image dimensions lookup failed");
            if (!w || !h || w > MaximumTextureSize || h > MaximumTextureSize)
                RendererError("D3D12 GUI image dimensions exceed texture limits");
            Check(factory->CreateFormatConverter(&converter), "D3D12 GUI image converter creation failed");
            Check(converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppBGRA, WICBitmapDitherTypeNone, nullptr, 0, WICBitmapPaletteTypeCustom),
                  "D3D12 GUI BGRA image conversion failed");
            std::vector<std::uint32_t> replacement(SIZE_T(w) * h);
            Check(converter->CopyPixels(nullptr, w * 4, w * h * 4, reinterpret_cast<BYTE*>(replacement.data())), "D3D12 GUI image pixel copy failed");
            pixels.swap(replacement);
            width = w;
            height = h;
            gpu.reset();
        }
        catch (...)
        {
            provider->unloadRawDataContainer(data);
            throw;
        }
        provider->unloadRawDataContainer(data);
    }

    CEGUI::Texture* D3D12GuiRenderer::createTexture()
    {
        if (GetCurrentThreadId() != m_impl->ownerThread || m_impl->stopped)
            RendererError("D3D12 GUI texture creation requires its active owner thread");
        auto texture = std::make_unique<D3D12GuiTexture>(this);
        m_impl->textures.push_back(texture.get());
        return texture.release();
    }
    CEGUI::Texture* D3D12GuiRenderer::createTexture(const CEGUI::String& filename, const CEGUI::String& group)
    {
        auto* texture = createTexture();
        try
        {
            texture->loadFromFile(filename, group);
        }
        catch (...)
        {
            destroyTexture(texture);
            throw;
        }
        return texture;
    }
    CEGUI::Texture* D3D12GuiRenderer::createTexture(float size)
    {
        if (!std::isfinite(size) || size < 1 || size > MaximumTextureSize)
            RendererError("D3D12 GUI requested texture size is invalid");
        const UINT                       extent = static_cast<UINT>(std::ceil(size));
        const std::vector<std::uint32_t> empty(SIZE_T(extent) * extent, 0);
        auto*                            texture = createTexture();
        try
        {
            texture->loadFromMemory(empty.data(), extent, extent);
        }
        catch (...)
        {
            destroyTexture(texture);
            throw;
        }
        return texture;
    }
    void D3D12GuiRenderer::destroyTexture(CEGUI::Texture* texture)
    {
        const auto found = std::find(m_impl->textures.begin(), m_impl->textures.end(), texture);
        if (found == m_impl->textures.end())
            return;
        // Retained batches use image/texture pointers, whereas submitted frames use
        // shared immutable GPU versions. Invalidate only the former before deletion.
        m_impl->quads.clear();
        delete *found;
        m_impl->textures.erase(found);
    }
    void D3D12GuiRenderer::destroyAllTextures()
    {
        m_impl->quads.clear();
        for (auto* texture : m_impl->textures)
            delete texture;
        m_impl->textures.clear();
    }
    void D3D12GuiRenderer::clearRenderList()
    {
        m_impl->quads.clear();
    }
    void D3D12GuiRenderer::NotifyImageInvalid(CEGUI::Image* const)
    {
        clearRenderList();
    }
    void D3D12GuiRenderer::setQueueingEnabled(bool setting)
    {
        m_impl->queueing = setting;
    }
    bool D3D12GuiRenderer::isQueueingEnabled() const
    {
        return m_impl->queueing;
    }
    float D3D12GuiRenderer::getWidth() const
    {
        return static_cast<float>(m_impl->width);
    }
    float D3D12GuiRenderer::getHeight() const
    {
        return static_cast<float>(m_impl->height);
    }
    CEGUI::Size D3D12GuiRenderer::getSize() const
    {
        return CEGUI::Size(getWidth(), getHeight());
    }
    CEGUI::Rect D3D12GuiRenderer::getRect() const
    {
        return CEGUI::Rect(0, 0, getWidth(), getHeight());
    }
    CEGUI::uint D3D12GuiRenderer::getMaxTextureSize() const
    {
        return MaximumTextureSize;
    }
    CEGUI::uint D3D12GuiRenderer::getHorzScreenDPI() const
    {
        return 96;
    }
    CEGUI::uint D3D12GuiRenderer::getVertScreenDPI() const
    {
        return 96;
    }
}  // namespace mta::gui
#endif
