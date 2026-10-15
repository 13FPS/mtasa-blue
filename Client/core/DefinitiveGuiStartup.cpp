/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual MTA GUI startup on a private, fence-protected D3D12 workload
 *****************************************************************************/
#include "DefinitiveGuiStartup.h"

#if defined(_WIN64) && defined(MTA_WITH_DEFINITIVE_ADAPTER)
    #include <windows.h>
    #include <d3d12.h>
    #include <dxgi.h>
    #include <wrl/client.h>
    #include <array>
    #include <cstring>
    #include <filesystem>
    #include <new>
    #include <SharedUtil.Defines.h>
    #include <SString.h>
    #include <gui/CGUI.h>
    #include <gui/CGUID3D12.h>

namespace DefinitiveGuiStartup
{
    using Microsoft::WRL::ComPtr;
    using mta::gui::GuiD3D12Status;

    namespace
    {
        void Barrier(ID3D12GraphicsCommandList* list, ID3D12Resource* resource, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after) noexcept
        {
            D3D12_RESOURCE_BARRIER barrier{};
            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Transition.pResource = resource;
            barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            barrier.Transition.StateBefore = before;
            barrier.Transition.StateAfter = after;
            list->ResourceBarrier(1, &barrier);
        }

        bool Near(unsigned value, unsigned expected) noexcept
        {
            return value + 1 >= expected && value <= expected + 1;
        }

        bool ReleaseUnknown(IUnknown* value) noexcept
        {
            // An SEH fault has unknown side effects. Clear the transferred
            // pointer before the call, and never retry it after such a fault.
            __try
            {
                value->Release();
                return true;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                return false;
            }
        }

        template <typename T>
        bool ReleaseOwned(ComPtr<T>& stored) noexcept
        {
            auto* value = stored.Detach();
            return !value || ReleaseUnknown(value);
        }

        bool ReleaseDevice(ID3D12Device*& stored) noexcept
        {
            auto* value = stored;
            stored = nullptr;
            return ReleaseUnknown(value);
        }

        bool ReleaseGui(mta::gui::IGuiD3D12Session*& stored, GuiD3D12Status& status) noexcept
        {
            auto* value = stored;
            stored = nullptr;
            __try
            {
                status = value->Release();
                if (status != GuiD3D12Status::Ok)
                    stored = value;
                return true;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                return false;
            }
        }
    }

    struct Session::Impl
    {
        DWORD   owner = GetCurrentThreadId();
        Result  result;
        bool    prepared = false, started = false, submitted = false, signalPending = false, gpuComplete = false, failed = false, releaseFault = false;
        HMODULE module = nullptr;
        mta::gui::CreateGuiD3D12Session create = nullptr;
        // AttachDevice writes only this pointer. It cannot invoke COM while
        // native observation cleanup might still own suspended peers.
        ID3D12Device*                      device = nullptr;
        ComPtr<ID3D12CommandQueue>         queue;
        ComPtr<ID3D12CommandAllocator>     clearAllocator, copyAllocator;
        ComPtr<ID3D12GraphicsCommandList>  clearList, copyList;
        ComPtr<ID3D12Resource>             target, readback;
        ComPtr<ID3D12DescriptorHeap>       rtv;
        ComPtr<ID3D12Fence>                fence;
        D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
        mta::gui::IGuiD3D12Session*        session = nullptr;
        CGUI*                              gui = nullptr;
        CGUITexture*                       texture = nullptr;
        CGUIStaticImage*                   image = nullptr;

        void Retained() noexcept
        {
            result.moduleRetained = module != nullptr;
            result.sessionRetained = session != nullptr;
            result.deviceRetained = device != nullptr;
        }

        Status Fail(HRESULT hr, GuiD3D12Status guiFailure = GuiD3D12Status::Ok) noexcept
        {
            if (!failed)
            {
                result.failureStage = result.stage;
                result.hresult = static_cast<std::uint32_t>(FAILED(hr) ? hr : E_FAIL);
                failed = true;
            }
            if (guiFailure != GuiD3D12Status::Ok && guiFailure != GuiD3D12Status::Busy && !result.guiStatus)
                result.guiStatus = static_cast<std::uint32_t>(guiFailure);
            Retained();
            return Status::Failed;
        }

        bool Removed() noexcept
        {
            const auto reason = device->GetDeviceRemovedReason();
            if (SUCCEEDED(reason))
                return false;
            result.deviceRemoved = 1;
            Fail(reason);
            // A removed device cannot execute further commands, but this alone
            // does not authorize destroying a GUI session whose API retains it.
            gpuComplete = true;
            return true;
        }

        Status Completion() noexcept
        {
            if (!submitted || gpuComplete)
                return Status::Ok;
            if (signalPending)
            {
                ++result.signalAttempts;
                const auto hr = queue->Signal(fence.Get(), result.fenceValue);
                if (FAILED(hr))
                {
                    Fail(hr);
                    return Removed() ? Status::Ok : Status::Busy;
                }
                signalPending = false;
            }
            result.completedFenceValue = fence->GetCompletedValue();
            if (result.completedFenceValue == UINT64_MAX)
            {
                if (!Removed())
                    Fail(DXGI_ERROR_DEVICE_REMOVED);
                return gpuComplete ? Status::Ok : Status::Busy;
            }
            gpuComplete = result.completedFenceValue >= result.fenceValue;
            return gpuComplete ? Status::Ok : Status::Busy;
        }
    };

    Session::~Session() noexcept
    {
        // A destructor may run during failed observer cleanup. It must not call
        // COM, a loader, or GUI code then. Explicit Shutdown is the only drain;
        // unresolved ownership deliberately remains allocated and keeps leases.
        if (impl && impl->result.cleanupComplete)
            delete impl;
    }

    Status Session::Prepare(const wchar_t* absoluteGuiDll, const wchar_t* absoluteAssetRoot) noexcept
    {
        if (impl || !absoluteGuiDll || !absoluteAssetRoot)
            return Status::InvalidArgument;
        impl = new (std::nothrow) Impl;
        if (!impl)
            return Status::Failed;
        try
        {
            const std::filesystem::path library(absoluteGuiDll), root(absoluteAssetRoot);
            if (!library.is_absolute() || !root.is_absolute() || library.filename() != L"cgui.dll" ||
                library.lexically_normal().parent_path() != root.lexically_normal())
                return impl->Fail(E_INVALIDARG);
            const auto counted = root.lexically_normal().wstring();
            if (counted.empty() || counted.size() > 32767)
                return impl->Fail(E_INVALIDARG);
            // Logs are generated data, so a fresh package does not own or ship
            // this directory. Create it before loading/configuring cgui and
            // before native Begin can suspend a peer inside filesystem/loader code.
            const auto logs = root.lexically_normal() / L"mta/logs";
            std::filesystem::create_directories(logs);
            if (!std::filesystem::is_directory(logs))
                return impl->Fail(E_FAIL);
            impl->module = LoadLibraryExW(library.c_str(), nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
            if (!impl->module)
                return impl->Fail(HRESULT_FROM_WIN32(GetLastError()));
            const auto configure = reinterpret_cast<mta::gui::ConfigureGuiD3D12AssetRoot>(GetProcAddress(impl->module, "ConfigureGUID3D12AssetRoot"));
            impl->create = reinterpret_cast<mta::gui::CreateGuiD3D12Session>(GetProcAddress(impl->module, "CreateGUID3D12Session"));
            if (!configure || !impl->create)
                return impl->Fail(HRESULT_FROM_WIN32(ERROR_PROC_NOT_FOUND));
            if (!configure(mta::gui::GuiD3D12AssetRootApiVersion, counted.data(), static_cast<std::uint32_t>(counted.size())))
                return impl->Fail(E_INVALIDARG);
            impl->prepared = true;
            impl->result.modulePrepared = 1;
            impl->result.stage = static_cast<std::uint32_t>(Stage::Prepared);
            impl->Retained();
            return Status::Ok;
        }
        catch (...)
        {
            return impl->Fail(E_FAIL);
        }
    }

    Status Session::AttachDevice(ID3D12Device* transferredReference) noexcept
    {
        if (!impl)
            return Status::InvalidArgument;
        if (GetCurrentThreadId() != impl->owner)
            return Status::WrongThread;
        if (!impl->prepared || impl->started || impl->device || impl->result.shutdownRequested || !transferredReference)
            return Status::InvalidArgument;
        impl->device = transferredReference;
        impl->result.deviceAttached = 1;
        impl->result.stage = static_cast<std::uint32_t>(Stage::DeviceAttached);
        impl->Retained();
        return Status::Ok;
    }

    Status Session::Start() noexcept
    {
        if (!impl)
            return Status::InvalidArgument;
        if (GetCurrentThreadId() != impl->owner)
            return Status::WrongThread;
        if (!impl->prepared || !impl->device || impl->started || impl->result.shutdownRequested)
            return Status::InvalidArgument;
        impl->started = true;
        try
        {
            const auto luid = impl->device->GetAdapterLuid();
            impl->result.deviceLuidLow = luid.LowPart;
            impl->result.deviceLuidHigh = static_cast<std::uint32_t>(luid.HighPart);
            impl->result.nodeCount = impl->device->GetNodeCount();
            D3D12_COMMAND_QUEUE_DESC queue{};
            queue.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
            auto hr = impl->device->CreateCommandQueue(&queue, IID_PPV_ARGS(&impl->queue));
            if (FAILED(hr))
                return impl->Fail(hr);
            impl->result.queueCreated = 1;
            hr = impl->device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&impl->fence));
            if (FAILED(hr))
                return impl->Fail(hr);
            // Clear and copy each have an independent one-shot allocator/list.
            // Neither is reset while the asynchronous GUI workload is in flight.
            hr = impl->device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&impl->clearAllocator));
            if (FAILED(hr))
                return impl->Fail(hr);
            hr = impl->device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&impl->copyAllocator));
            if (FAILED(hr))
                return impl->Fail(hr);
            hr = impl->device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, impl->clearAllocator.Get(), nullptr, IID_PPV_ARGS(&impl->clearList));
            if (FAILED(hr))
                return impl->Fail(hr);
            hr = impl->device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, impl->copyAllocator.Get(), nullptr, IID_PPV_ARGS(&impl->copyList));
            if (FAILED(hr))
                return impl->Fail(hr);
            D3D12_RESOURCE_DESC description{};
            description.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
            description.Width = impl->result.width;
            description.Height = impl->result.height;
            description.DepthOrArraySize = description.MipLevels = 1;
            description.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
            description.SampleDesc.Count = 1;
            description.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
            D3D12_HEAP_PROPERTIES heap{};
            heap.Type = D3D12_HEAP_TYPE_DEFAULT;
            heap.CreationNodeMask = heap.VisibleNodeMask = 1;
            hr = impl->device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &description, D3D12_RESOURCE_STATE_PRESENT, nullptr,
                                                       IID_PPV_ARGS(&impl->target));
            if (FAILED(hr))
                return impl->Fail(hr);
            impl->result.targetCreated = 1;
            UINT64 bytes = 0;
            impl->device->GetCopyableFootprints(&description, 0, 1, 0, &impl->footprint, nullptr, nullptr, &bytes);
            if (!bytes || bytes == UINT64_MAX)
                return impl->Fail(E_FAIL);
            description = {};
            description.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
            description.Width = bytes;
            description.Height = description.DepthOrArraySize = description.MipLevels = 1;
            description.SampleDesc.Count = 1;
            description.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
            heap.Type = D3D12_HEAP_TYPE_READBACK;
            hr = impl->device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &description, D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
                                                       IID_PPV_ARGS(&impl->readback));
            if (FAILED(hr))
                return impl->Fail(hr);
            D3D12_DESCRIPTOR_HEAP_DESC descriptors{};
            descriptors.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
            descriptors.NumDescriptors = 1;
            hr = impl->device->CreateDescriptorHeap(&descriptors, IID_PPV_ARGS(&impl->rtv));
            if (FAILED(hr))
                return impl->Fail(hr);
            impl->device->CreateRenderTargetView(impl->target.Get(), nullptr, impl->rtv->GetCPUDescriptorHandleForHeapStart());
            impl->result.stage = static_cast<std::uint32_t>(Stage::Resources);
            Barrier(impl->clearList.Get(), impl->target.Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
            constexpr float clear[] = {17.0f / 255, 29.0f / 255, 43.0f / 255, 1};
            impl->clearList->ClearRenderTargetView(impl->rtv->GetCPUDescriptorHandleForHeapStart(), clear, 0, nullptr);
            Barrier(impl->clearList.Get(), impl->target.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
            hr = impl->clearList->Close();
            if (FAILED(hr))
                return impl->Fail(hr);
            Barrier(impl->copyList.Get(), impl->target.Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_COPY_SOURCE);
            D3D12_TEXTURE_COPY_LOCATION source{}, destination{};
            source.pResource = impl->target.Get();
            source.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
            destination.pResource = impl->readback.Get();
            destination.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
            destination.PlacedFootprint = impl->footprint;
            impl->copyList->CopyTextureRegion(&destination, 0, 0, 0, &source, nullptr);
            Barrier(impl->copyList.Get(), impl->target.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_PRESENT);
            hr = impl->copyList->Close();
            if (FAILED(hr))
                return impl->Fail(hr);
            mta::gui::GuiD3D12CreateInfo info;
            info.device = impl->device;
            info.queue = impl->queue.Get();
            info.targetFormat = impl->result.targetFormat;
            info.width = impl->result.width;
            info.height = impl->result.height;
            impl->session = impl->create(mta::gui::GuiD3D12ApiVersion, &info);
            if (!impl->session)
                return impl->Fail(E_FAIL, GuiD3D12Status::Failed);
            impl->result.sessionCreated = 1;
            impl->result.stage = static_cast<std::uint32_t>(Stage::Session);
            impl->Retained();
            auto status = impl->session->GetGUI(impl->gui);
            if (status != GuiD3D12Status::Ok || !impl->gui)
                return impl->Fail(E_FAIL, status);
            impl->gui->SetSkin("Default");
            if (!impl->gui->GetDefaultFont() || impl->gui->GetTextExtent("MTA owned GUI startup") <= 0)
                return impl->Fail(E_FAIL);
            auto* window = impl->gui->CreateWnd(nullptr, "MTA GUI startup");
            window->SetPosition(CVector2D(40, 40));
            window->SetSize(CVector2D(430, 300));
            window->SetVisible(true);
            auto* button = impl->gui->CreateButton(static_cast<CGUIElement*>(window), "Existing MTA widgets");
            button->SetPosition(CVector2D(25, 45));
            button->SetSize(CVector2D(220, 45));
            button->SetVisible(true);
            impl->texture = impl->gui->CreateTexture();
            std::array<std::uint32_t, 16> pixels{};
            pixels.fill(0xff00ff00);
            impl->texture->LoadFromMemory(pixels.data(), 4, 4);
            impl->image = impl->gui->CreateStaticImage();
            impl->image->SetPosition(CVector2D(500, 100));
            impl->image->SetSize(CVector2D(64, 64));
            impl->image->SetFrameEnabled(false);
            impl->image->SetVisible(true);
            if (!impl->image->LoadFromTexture(impl->texture))
                return impl->Fail(E_FAIL);
            impl->result.widgetsCreated = 1;
            // From the first Execute onward, any missing/failed Signal is
            // unknown completion. Shutdown retains everything and retries Signal.
            impl->submitted = true;
            impl->signalPending = true;
            impl->result.fenceValue = 1;
            impl->result.stage = static_cast<std::uint32_t>(Stage::Submitted);
            ID3D12CommandList* clearLists[] = {impl->clearList.Get()};
            impl->queue->ExecuteCommandLists(1, clearLists);
            ++impl->result.ownQueueSubmissions;
            const mta::gui::GuiD3D12Frame frame{impl->target.Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_PRESENT, impl->result.width,
                                                impl->result.height};
            status = impl->session->RenderFrame(frame);
            if (status != GuiD3D12Status::Ok)
                return impl->Fail(E_FAIL, status);
            ++impl->result.guiFrames;
            ID3D12CommandList* copyLists[] = {impl->copyList.Get()};
            impl->queue->ExecuteCommandLists(1, copyLists);
            ++impl->result.ownQueueSubmissions;
            impl->Completion();
            return impl->failed ? Status::Failed : Status::Ok;
        }
        catch (...)
        {
            return impl->Fail(E_FAIL);
        }
    }

    Status Session::Poll() noexcept
    {
        if (!impl)
            return Status::InvalidArgument;
        if (GetCurrentThreadId() != impl->owner)
            return Status::WrongThread;
        if (!impl->started || impl->result.shutdownRequested)
            return Status::InvalidArgument;
        if (impl->result.readbackComplete)
            return impl->failed ? Status::Failed : Status::Ok;
        const auto completion = impl->Completion();
        if (impl->failed)
            return Status::Failed;
        if (completion != Status::Ok)
            return completion;
        void*             mapped = nullptr;
        const D3D12_RANGE range{0, static_cast<SIZE_T>(impl->readback->GetDesc().Width)};
        const auto        hr = impl->readback->Map(0, &range, &mapped);
        if (FAILED(hr))
            return impl->Fail(hr);
        // Read only after our own fence covers clear, actual GUI draw and copy.
        // The numerical check is independent of any preview/image conversion.
        std::uint32_t background = 0, sample = 0, changed = 0, nonOpaque = 0;
        std::memcpy(&background, static_cast<const BYTE*>(mapped) + impl->footprint.Offset, sizeof(background));
        for (UINT y = 0; y < impl->result.height; ++y)
        {
            const auto* row = static_cast<const BYTE*>(mapped) + impl->footprint.Offset + SIZE_T(y) * impl->footprint.Footprint.RowPitch;
            for (UINT x = 0; x < impl->result.width; ++x)
            {
                std::uint32_t pixel = 0;
                std::memcpy(&pixel, row + SIZE_T(x) * 4, sizeof(pixel));
                changed += pixel != background;
                nonOpaque += (pixel >> 24) != 255;
                if (y == 132 && x == 532)
                    sample = pixel;
            }
        }
        const D3D12_RANGE noWrite{};
        impl->readback->Unmap(0, &noWrite);
        impl->result.readbackComplete = 1;
        impl->result.changedPixels = changed;
        impl->result.backgroundVerified = Near((background >> 16) & 255, 17) && Near((background >> 8) & 255, 29) && Near(background & 255, 43);
        impl->result.textureVerified = sample == 0xff00ff00;
        impl->result.opaqueAlphaVerified = nonOpaque == 0;
        impl->result.stage = static_cast<std::uint32_t>(Stage::Readback);
        if (!impl->result.backgroundVerified || !impl->result.textureVerified || !impl->result.opaqueAlphaVerified || changed <= 10000)
            return impl->Fail(E_FAIL);
        impl->result.workloadSucceeded = 1;
        return Status::Ok;
    }

    Status Session::Shutdown() noexcept
    {
        if (!impl)
            return Status::Ok;
        if (GetCurrentThreadId() != impl->owner)
            return Status::WrongThread;
        if (impl->result.cleanupComplete)
            return Status::Ok;
        if (impl->result.deviceReleaseFault || impl->releaseFault)
            return Status::Failed;
        impl->result.shutdownRequested = 1;
        impl->result.stage = static_cast<std::uint32_t>(Stage::Stopping);
        if (impl->Completion() != Status::Ok)
            return Status::Busy;
        try
        {
            if (impl->texture)
            {
                if (impl->image)
                    impl->image->Clear();
                delete impl->texture;
                impl->texture = nullptr;
            }
            if (impl->session)
            {
                auto status = GuiD3D12Status::Failed;
                if (!ReleaseGui(impl->session, status))
                {
                    impl->releaseFault = true;
                    return impl->Fail(E_UNEXPECTED);
                }
                if (status == GuiD3D12Status::Busy)
                    return Status::Busy;
                if (status != GuiD3D12Status::Ok)
                    return impl->Fail(E_FAIL, status);
                impl->gui = nullptr;
                impl->image = nullptr;
            }
            if (!ReleaseOwned(impl->copyList) || !ReleaseOwned(impl->clearList) || !ReleaseOwned(impl->copyAllocator) || !ReleaseOwned(impl->clearAllocator) ||
                !ReleaseOwned(impl->rtv) || !ReleaseOwned(impl->readback) || !ReleaseOwned(impl->target) || !ReleaseOwned(impl->fence) ||
                !ReleaseOwned(impl->queue))
            {
                impl->releaseFault = true;
                return impl->Fail(E_UNEXPECTED);
            }
            if (impl->device)
            {
                if (!ReleaseDevice(impl->device))
                {
                    impl->result.deviceReleaseFault = 1;
                    return impl->Fail(E_UNEXPECTED);
                }
            }
            if (impl->module)
            {
                if (!FreeLibrary(impl->module))
                {
                    impl->releaseFault = true;
                    return impl->Fail(HRESULT_FROM_WIN32(GetLastError()));
                }
                impl->module = nullptr;
                impl->create = nullptr;
            }
            impl->result.cleanupComplete = 1;
            impl->result.stage = static_cast<std::uint32_t>(Stage::Complete);
            impl->Retained();
            return Status::Ok;
        }
        catch (...)
        {
            return impl->Fail(E_FAIL);
        }
    }

    Result Session::Snapshot() const noexcept
    {
        if (!impl)
        {
            // Cancellation before Prepare and allocation failure own no lease.
            // The worker must be able to finish without waiting for a nonexistent
            // session, while a nonempty foreign-owner read still yields no proof.
            Result empty;
            empty.cleanupComplete = 1;
            return empty;
        }
        return GetCurrentThreadId() == impl->owner ? impl->result : Result{};
    }
}
#endif
