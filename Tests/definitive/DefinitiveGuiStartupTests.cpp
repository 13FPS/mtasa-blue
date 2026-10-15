/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Exercise production Core GUI startup on disposable owned WARP devices
 *****************************************************************************/
#include <windows.h>
#include <d3d12.h>
#include <d3d12sdklayers.h>
#include <dxgi1_4.h>
#include <wrl/client.h>
#include <atomic>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
#include "../../Client/core/DefinitiveGuiStartup.h"

using Microsoft::WRL::ComPtr;
using namespace DefinitiveGuiStartup;
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

    // The proxy exists only in this owned fixture. It forwards actual WARP
    // operations, blocks its private queue, and drops one helper completion
    // Signal after submission. No shipping helper seam or engine hook is used.
    class QueueProxy final : public ID3D12CommandQueue
    {
    public:
        explicit QueueProxy(ID3D12CommandQueue* value) { real.Attach(value); }
        virtual ~QueueProxy() = default;
        ComPtr<ID3D12CommandQueue> real;
        std::atomic<ULONG>         references{1};
        unsigned                   executions = 0, failedSignals = 0;
        bool                       failSignal = true;
        HRESULT STDMETHODCALLTYPE  QueryInterface(REFIID iid, void** output) override
        {
            if (!output)
                return E_POINTER;
            if (iid == __uuidof(IUnknown) || iid == __uuidof(ID3D12Object) || iid == __uuidof(ID3D12DeviceChild) || iid == __uuidof(ID3D12Pageable) ||
                iid == __uuidof(ID3D12CommandQueue))
            {
                *output = static_cast<ID3D12CommandQueue*>(this);
                AddRef();
                return S_OK;
            }
            *output = nullptr;
            return E_NOINTERFACE;
        }
        ULONG STDMETHODCALLTYPE AddRef() override { return ++references; }
        ULONG STDMETHODCALLTYPE Release() override
        {
            const auto left = --references;
            if (!left)
                delete this;
            return left;
        }
#define QUEUE_FORWARD(type, name, arguments, forwarded) \
    type STDMETHODCALLTYPE name arguments override \
    { \
        return real->name forwarded; \
    }
        QUEUE_FORWARD(HRESULT, GetPrivateData, (REFGUID id, UINT* size, void* data), (id, size, data))
        QUEUE_FORWARD(HRESULT, SetPrivateData, (REFGUID id, UINT size, const void* data), (id, size, data))
        QUEUE_FORWARD(HRESULT, SetPrivateDataInterface, (REFGUID id, const IUnknown* data), (id, data))
        QUEUE_FORWARD(HRESULT, SetName, (LPCWSTR name), (name))
        QUEUE_FORWARD(HRESULT, GetDevice, (REFIID iid, void** output), (iid, output))
        QUEUE_FORWARD(void, UpdateTileMappings,
                      (ID3D12Resource * resource, UINT regions, const D3D12_TILED_RESOURCE_COORDINATE* starts, const D3D12_TILE_REGION_SIZE* sizes,
                       ID3D12Heap* heap, UINT ranges, const D3D12_TILE_RANGE_FLAGS* flags, const UINT* offsets, const UINT* counts,
                       D3D12_TILE_MAPPING_FLAGS options),
                      (resource, regions, starts, sizes, heap, ranges, flags, offsets, counts, options))
        QUEUE_FORWARD(void, CopyTileMappings,
                      (ID3D12Resource * destination, const D3D12_TILED_RESOURCE_COORDINATE* destinationStart, ID3D12Resource* source,
                       const D3D12_TILED_RESOURCE_COORDINATE* sourceStart, const D3D12_TILE_REGION_SIZE* size, D3D12_TILE_MAPPING_FLAGS flags),
                      (destination, destinationStart, source, sourceStart, size, flags))
        void STDMETHODCALLTYPE ExecuteCommandLists(UINT count, ID3D12CommandList* const* lists) override
        {
            ++executions;
            real->ExecuteCommandLists(count, lists);
        }
        QUEUE_FORWARD(void, SetMarker, (UINT metadata, const void* data, UINT size), (metadata, data, size))
        QUEUE_FORWARD(void, BeginEvent, (UINT metadata, const void* data, UINT size), (metadata, data, size))
        QUEUE_FORWARD(void, EndEvent, (), ())
        HRESULT STDMETHODCALLTYPE Signal(ID3D12Fence* fence, UINT64 value) override
        {
            // The GUI's internal Signal follows the second Execute. The
            // helper's completion Signal follows its third (copy) Execute.
            if (failSignal && executions >= 3)
            {
                failSignal = false;
                ++failedSignals;
                return E_FAIL;
            }
            return real->Signal(fence, value);
        }
        QUEUE_FORWARD(HRESULT, Wait, (ID3D12Fence * fence, UINT64 value), (fence, value))
        QUEUE_FORWARD(HRESULT, GetTimestampFrequency, (UINT64 * frequency), (frequency))
        QUEUE_FORWARD(HRESULT, GetClockCalibration, (UINT64 * gpu, UINT64* cpu), (gpu, cpu))
        QUEUE_FORWARD(D3D12_COMMAND_QUEUE_DESC, GetDesc, (), ())
#undef QUEUE_FORWARD
    };

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
        ID3D12Device* real;
        ID3D12Fence*  gate;
        QueueProxy*   created = nullptr;
        unsigned      calls = 0;
        ULONG         proxyReferences = 1;
        bool          faultFinalRelease = false;
        unsigned      releaseFaults = 0;
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
            created = new QueueProxy(queue);
            const auto wait = queue->Wait(gate, 1);
            if (FAILED(wait))
            {
                created->Release();
                created = nullptr;
                return wait;
            }
            created->AddRef();  // A fixture-only observation lease, released below.
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
        DEVICE_FORWARD(HRESULT, CreateCommittedResource,
                       (const D3D12_HEAP_PROPERTIES* heap, D3D12_HEAP_FLAGS flags, const D3D12_RESOURCE_DESC* description, D3D12_RESOURCE_STATES state,
                        const D3D12_CLEAR_VALUE* clear, REFIID iid, void** output),
                       (heap, flags, description, state, clear, iid, output))
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

    void DebugErrors(ID3D12InfoQueue* info)
    {
        if (!info)
            return;
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
                std::fprintf(stderr, "D3D12 debug error: %s\n", message->pDescription);
            }
        }
        Check(!errors, "actual owned device reports no D3D12 resource/lifetime debug errors");
    }

    void Drain(Session& session)
    {
        const auto start = GetTickCount64();
        Status     status;
        do
        {
            const auto before = GetTickCount64();
            status = session.Shutdown();
            Check(GetTickCount64() - before < 1000, "production Shutdown remains nonblocking");
            if (status == Status::Busy)
                Sleep(1);
        } while (status == Status::Busy && GetTickCount64() - start < 5000);
        Check(status == Status::Ok && session.Snapshot().cleanupComplete, "production helper completes explicit owner cleanup within five seconds");
    }
}

int wmain(int argc, wchar_t** argv)
{
    setvbuf(stdout, nullptr, _IONBF, 0);
    setvbuf(stderr, nullptr, _IONBF, 0);
    try
    {
        Check(argc == 3 && sizeof(void*) == 8, "owned x64 startup fixture root and case supplied");
        const std::filesystem::path root(argv[1]);
        const std::wstring          mode(argv[2]);
        Check(root.is_absolute() && root.filename() == L"de-online" && root.parent_path().filename() == L"CLEO", "use actual explicit DE asset/DLL layout");
        Check(mode == L"normal" || mode == L"signal" || mode == L"release", "run one explicit real-device, signal-fault or release-fault case");
        std::printf("GUI startup case=%ls\n", mode.c_str());
        Check(!GetModuleHandleW(L"d3dx9_43.dll"), "owned fixture begins without legacy D3DX9 renderer dependency");
        const auto core = LoadLibraryExW((root / L"core.dll").c_str(), nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
        Check(core != nullptr, "load certified Core only as uninitialized asset resolver");
        using SetRoot = void (*)(const wchar_t*, std::size_t);
        const auto setRoot = reinterpret_cast<SetRoot>(GetProcAddress(core, "SetMTADirectory"));
        Check(setRoot != nullptr, "resolve Core asset-root setter");
        const auto rootWide = root.wstring();
        setRoot(rootWide.data(), rootWide.size());
        {
            Session invalid;
            Check(invalid.Shutdown() == Status::Ok && invalid.Snapshot().cleanupComplete && !invalid.Snapshot().moduleRetained &&
                      !invalid.Snapshot().sessionRetained && !invalid.Snapshot().deviceRetained,
                  "empty helper cleanup is complete for cancellation before Prepare or allocation failure");
            Check(invalid.AttachDevice(reinterpret_cast<ID3D12Device*>(UINT_PTR(1))) == Status::InvalidArgument,
                  "unprepared Attach rejects without touching a sentinel device");
            Check(invalid.Prepare(L"relative/cgui.dll", root.c_str()) == Status::Failed, "relative module path fails closed before loader/GPU work");
            Check(invalid.Snapshot().hresult == static_cast<std::uint32_t>(E_INVALIDARG) && !invalid.Snapshot().moduleRetained,
                  "failed preparation records its own refusal without a module lease");
            Drain(invalid);
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
        Hr(CreateDXGIFactory1(IID_PPV_ARGS(&factory)), "create owned system DXGI factory");
        Hr(factory->EnumWarpAdapter(IID_PPV_ARGS(&adapter)), "select private actual WARP adapter");
        Hr(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device)), "create private actual WARP device");
        if (debugAvailable)
            Hr(device.As(&info), "obtain actual device debug queue");
        {
            Session    session;
            const auto logs = root / L"MTA/logs";
            if (mode == L"normal")
            {
                Check(!std::filesystem::exists(logs), "fresh actual adapter layout has no generated logs before Prepare");
                {
                    std::ofstream collision(logs, std::ios::binary);
                    collision << "owned fixture file collision";
                    Check(collision.good(), "create only an owned generated-directory collision control");
                }
                Session blocked;
                Check(blocked.Prepare((root / L"cgui.dll").c_str(), root.c_str()) == Status::Failed && !blocked.Snapshot().modulePrepared &&
                          !blocked.Snapshot().moduleRetained,
                      "failed generated-directory creation refuses before cgui loader/GPU work");
                Drain(blocked);
                Check(std::filesystem::remove(logs) && !std::filesystem::exists(logs), "remove only owned collision file before fresh successful Prepare");
            }
            Check(session.Prepare((root / L"cgui.dll").c_str(), root.c_str()) == Status::Ok,
                  "production Prepare preloads/configures actual MTA GUI and assets");
            Check(std::filesystem::is_directory(logs), "production Prepare creates or preserves generated logs before actual cgui configuration");
            if (mode == L"normal")
                std::printf("Fresh generated logs absent before Prepare=1 created=1\n");
            Check(!GetModuleHandleW(L"d3dx9_43.dll"), "actual D3D12 GUI preparation keeps legacy D3DX9 delay import unloaded");
            const auto prepared = session.Snapshot();
            Check(prepared.modulePrepared && prepared.moduleRetained && !prepared.deviceAttached && !prepared.nodeCount && !prepared.queueCreated &&
                      !prepared.targetCreated && !prepared.sessionCreated && !prepared.ownQueueSubmissions,
                  "preparation performs no device queries, resource creation or GUI session work");
            bool        wrongOwner = false;
            std::thread foreign(
                [&]
                {
                    wrongOwner = session.AttachDevice(reinterpret_cast<ID3D12Device*>(UINT_PTR(1))) == Status::WrongThread &&
                                 session.Start() == Status::WrongThread && session.Poll() == Status::WrongThread && session.Shutdown() == Status::WrongThread;
                });
            foreign.join();
            Check(wrongOwner, "actual production helper rejects every foreign owner operation");
            ComPtr<ID3D12Fence> gate;
            if (mode == L"signal")
                Hr(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&gate)), "create private CPU-controlled queue gate");
            // The normal case passes the real device directly. The proxy is
            // reserved for the separate deterministic submitted-ownership case.
            if (mode == L"normal")
            {
                device->AddRef();
                Check(session.AttachDevice(device.Get()) == Status::Ok, "Attach consumes one actual owned device reference");
                Check(session.Start() == Status::Ok, "production helper starts actual GUI workload on its own queue/target");
                Check(!GetModuleHandleW(L"d3dx9_43.dll"), "actual fonts/widgets and D3D12 submission do not load legacy D3DX9");
                Check(
                    session.Start() == Status::InvalidArgument && session.AttachDevice(reinterpret_cast<ID3D12Device*>(UINT_PTR(1))) == Status::InvalidArgument,
                    "one-shot helper never replaces an attached device or repeats submissions");
                const auto start = GetTickCount64();
                Status     status;
                do
                {
                    const auto before = GetTickCount64();
                    status = session.Poll();
                    Check(GetTickCount64() - before < 1000, "production Poll never blocks for GPU completion");
                    if (status == Status::Busy)
                        Sleep(1);
                } while (status == Status::Busy && GetTickCount64() - start < 5000);
                const auto result = session.Snapshot();
                Check(status == Status::Ok && result.workloadSucceeded && !result.hresult && !result.guiStatus && result.nodeCount && result.queueCreated &&
                          result.targetCreated && result.sessionCreated && result.widgetsCreated,
                      "real MTA fonts/widgets complete on actual owned WARP resources");
                Check(result.readbackComplete && result.changedPixels > 10000 && result.backgroundVerified && result.textureVerified &&
                          result.opaqueAlphaVerified,
                      "production raw readback verifies actual widget coverage, green texture, background and alpha");
                Check(result.ownQueueSubmissions == 2 && result.guiFrames == 1 && result.fenceValue == 1 && result.completedFenceValue >= 1,
                      "distinct clear/copy allocations and actual GUI frame finish before mapped readback");
                Drain(session);
                Check(session.Snapshot().workloadSucceeded && !session.Snapshot().hresult, "cleanup preserves successful workload facts");
            }
            else if (mode == L"signal")
            {
                DeviceProxy proxy(device.Get(), gate.Get());
                device->AddRef();
                const auto before = proxy.calls;
                Check(session.AttachDevice(&proxy) == Status::Ok && proxy.calls == before, "production Attach is pointer storage only with zero COM calls");
                Check(session.Start() == Status::Failed, "one failed owned completion Signal is reported after actual submitted work");
                const auto failed = session.Snapshot();
                Check(proxy.created && proxy.created->executions == 3 && proxy.created->failedSignals == 1 && failed.ownQueueSubmissions == 2 &&
                          failed.guiFrames == 1 && failed.signalAttempts == 1 && failed.hresult == static_cast<std::uint32_t>(E_FAIL),
                      "fault targets helper completion only, after real clear/GUI/copy submissions");
                const auto start = GetTickCount64();
                Check(session.Shutdown() == Status::Busy && GetTickCount64() - start < 1000,
                      "unknown submitted completion retries Signal without a blocking drain");
                const auto pending = session.Snapshot();
                Check(pending.signalAttempts == 2 && !pending.completedFenceValue && !pending.cleanupComplete && pending.moduleRetained &&
                          pending.sessionRetained && pending.deviceRetained && !pending.readbackComplete && !pending.workloadSucceeded &&
                          GetModuleHandleW(L"cgui.dll"),
                      "blocked queue retains module/session/device and cannot claim successful pixels or cleanup");
                Check(session.Poll() == Status::InvalidArgument && session.Start() == Status::InvalidArgument, "pending stop forbids a future workload/frame");
                Hr(gate->Signal(1), "release only owned gate from CPU without a future frame");
                Drain(session);
                const auto final = session.Snapshot();
                Check(final.hresult == static_cast<std::uint32_t>(E_FAIL) && final.failureStage == static_cast<std::uint32_t>(Stage::Submitted) &&
                          !final.workloadSucceeded && !final.moduleRetained && !final.sessionRetained && !final.deviceRetained,
                      "safe cleanup preserves first signal failure and drops all helper leases only after completion");
            }
            else
            {
                DeviceProxy proxy(device.Get(), nullptr);
                proxy.faultFinalRelease = true;
                device->AddRef();
                Check(session.AttachDevice(&proxy) == Status::Ok, "release-fault case transfers one owned reference through fixture proxy");
                Check(session.Start() == Status::Ok, "release-fault case first completes ordinary actual GUI submissions");
                const auto start = GetTickCount64();
                Status     status;
                do
                {
                    status = session.Poll();
                    if (status == Status::Busy)
                        Sleep(1);
                } while (status == Status::Busy && GetTickCount64() - start < 5000);
                Check(status == Status::Ok && session.Snapshot().workloadSucceeded, "release fault is isolated after verified real widget pixels");
                Check(session.Shutdown() == Status::Failed && proxy.releaseFaults == 1, "guarded transferred-device Release catches one owned SEH fault");
                const auto failed = session.Snapshot();
                Check(failed.deviceReleaseFault && !failed.cleanupComplete && failed.moduleRetained && !failed.sessionRetained && !failed.deviceRetained &&
                          failed.hresult == static_cast<std::uint32_t>(E_UNEXPECTED) && failed.workloadSucceeded,
                      "unknown Release effects clear the pointer, retain module and preserve earlier workload success");
                const auto before = proxy.calls;
                Check(session.Shutdown() == Status::Failed && session.Shutdown() == Status::Failed && proxy.calls == before && proxy.releaseFaults == 1,
                      "later cleanup never retries the unknown transferred-device Release");
                DebugErrors(info.Get());
                Check(!GetModuleHandleW(L"d3dx9_43.dll"), "even retained-release-fault GUI workload leaves legacy D3DX9 unloaded");
                // This deliberately unresolved process proves retention, not
                // zero references. The fixture knows its stub did not forward,
                // but production may not infer that after an arbitrary fault.
                std::printf("Owned release fault retained=1 cleanupComplete=0 retryCalls=0\n");
                std::printf("Passed %u production GUI startup host checks\n", checks);
                return 0;
            }
            Check(session.Shutdown() == Status::Ok && !session.Snapshot().moduleRetained && !session.Snapshot().sessionRetained &&
                      !session.Snapshot().deviceRetained && !GetModuleHandleW(L"cgui.dll"),
                  "idempotent completed cleanup leaves no actual GUI module or helper device lease");
            Check(!GetModuleHandleW(L"d3dx9_43.dll"), "completed actual D3D12 GUI workload and cleanup keep legacy D3DX9 unloaded");
            gate.Reset();
        }
        DebugErrors(info.Get());
        info.Reset();
        debug.Reset();
        auto* rawDevice = device.Detach();
        Check(rawDevice->Release() == 0, "helper/GUI/queue/fixture leave zero actual WARP device references");
        Check(FreeLibrary(core) != FALSE && !GetModuleHandleW(L"core.dll"), "unload uninitialized Core asset resolver after safe GUI cleanup");
        std::printf("Passed %u production GUI startup host checks\n", checks);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "Production GUI startup fixture failed after %u checks: %s\n", checks, error.what());
        return 1;
    }
}
