/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 *
 * PURPOSE: Retain the real WARP device while gating only the owned surface queue

 * *****************************************************************************/
#pragma once
#include <windows.h>
#include <d3d12.h>

// This test-only forwarding device preserves the real canonical IUnknown and
// queue identity. It provides deterministic GPU retirement without a game stub.
namespace GuiFrontendSurfaceTest
{
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

}
