/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Query borrowed D3D12 roots without issuing GPU work or retaining them
 *****************************************************************************/
#include "NativeD3D12OwnershipProbe.h"
#include <windows.h>
#include <d3d12.h>
#include <dxgi1_4.h>

namespace mta::game_de
{
    namespace
    {
        using Stage = GpuOwnershipQueryStage;
        using QueryStatus = GpuOwnershipQueryStatus;

        bool ExecuteGuarded(GpuOwnershipQueries& queries, Stage stage, GpuOwnershipProbeState& state, std::int32_t& result) noexcept
        {
            __try
            {
                result = queries.Execute(stage, state);
                return true;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                return false;
            }
        }
        bool CloseGuarded(GpuOwnershipQueries& queries) noexcept
        {
            __try
            {
                return queries.Close();
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                return false;
            }
        }

        template <typename T>
        bool ReleaseGuarded(T*& reference) noexcept
        {
            // A throwing Release has an unknown effect; never release the same
            // pointer twice. Continue releasing the other acquired references.
            T* value = reference;
            reference = nullptr;
            if (!value)
                return true;
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

        class ComQueries final : public GpuOwnershipQueries
        {
        public:
            ComQueries(void* swapchain, void* queue) noexcept
                : borrowedSwapchain(static_cast<IUnknown*>(swapchain)), borrowedQueue(static_cast<IUnknown*>(queue))
            {
            }
            std::int32_t Execute(Stage stage, GpuOwnershipProbeState& state) override
            {
                switch (stage)
                {
                    case Stage::SwapchainQuery:
                    {
                        const auto result = borrowedSwapchain->QueryInterface(IID_PPV_ARGS(&swapchain));
                        return SUCCEEDED(result) && !swapchain ? E_POINTER : result;
                    }
                    case Stage::QueueQuery:
                    {
                        const auto result = borrowedQueue->QueryInterface(IID_PPV_ARGS(&queue));
                        return SUCCEEDED(result) && !queue ? E_POINTER : result;
                    }
                    case Stage::QueueDescription:
                    {
                        const auto description = queue->GetDesc();
                        state.queueType = static_cast<std::uint32_t>(description.Type);
                        state.queuePriority = static_cast<std::uint32_t>(description.Priority);
                        state.queueFlags = static_cast<std::uint32_t>(description.Flags);
                        state.queueNodeMask = description.NodeMask;
                        return S_OK;
                    }
                    case Stage::QueueDevice:
                    {
                        const auto result = queue->GetDevice(IID_PPV_ARGS(&queueDevice));
                        if (SUCCEEDED(result) && queueDevice)
                        {
                            state.deviceNodeCount = queueDevice->GetNodeCount();
                            const auto luid = queueDevice->GetAdapterLuid();
                            state.adapterLuidLow = luid.LowPart;
                            state.adapterLuidHigh = static_cast<std::uint32_t>(luid.HighPart);
                        }
                        return SUCCEEDED(result) && !queueDevice ? E_POINTER : result;
                    }
                    case Stage::SwapchainDevice:
                    {
                        const auto result = swapchain->GetDevice(IID_PPV_ARGS(&swapchainDevice));
                        return SUCCEEDED(result) && !swapchainDevice ? E_POINTER : result;
                    }
                    case Stage::CanonicalQueueDevice:
                    {
                        const auto result = queueDevice->QueryInterface(IID_PPV_ARGS(&queueIdentity));
                        if (SUCCEEDED(result) && queueIdentity)
                            state.canonicalIdentityFlags |= 1;
                        return SUCCEEDED(result) && !queueIdentity ? E_POINTER : result;
                    }
                    case Stage::CanonicalSwapchainDevice:
                    {
                        const auto result = swapchainDevice->QueryInterface(IID_PPV_ARGS(&swapchainIdentity));
                        if (SUCCEEDED(result) && swapchainIdentity == queueIdentity)
                            state.canonicalIdentityFlags |= 2;
                        return SUCCEEDED(result) && !swapchainIdentity ? E_POINTER : result;
                    }
                    case Stage::SwapchainDescription:
                    {
                        DXGI_SWAP_CHAIN_DESC1 description{};
                        const auto            result = swapchain->GetDesc1(&description);
                        if (SUCCEEDED(result))
                        {
                            state.swapchainBufferCount = description.BufferCount;
                            state.swapchainFormat = static_cast<std::uint32_t>(description.Format);
                            state.swapchainFlags = description.Flags;
                            state.swapchainWidth = description.Width;
                            state.swapchainHeight = description.Height;
                        }
                        return result;
                    }
                    case Stage::CurrentBackbufferIndex:
                        state.currentBackbufferIndex = swapchain->GetCurrentBackBufferIndex();
                        return S_OK;
                    case Stage::Backbuffer:
                    {
                        const auto result = swapchain->GetBuffer(state.currentBackbufferIndex, IID_PPV_ARGS(&backbuffer));
                        return SUCCEEDED(result) && !backbuffer ? E_POINTER : result;
                    }
                    case Stage::BackbufferDescription:
                    {
                        const auto description = backbuffer->GetDesc();
                        state.backbufferWidth = description.Width;
                        state.backbufferHeight = description.Height;
                        state.backbufferFormat = static_cast<std::uint32_t>(description.Format);
                        state.backbufferSampleCount = description.SampleDesc.Count;
                        state.backbufferSampleQuality = description.SampleDesc.Quality;
                        return description.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D ? S_OK : E_NOTIMPL;
                    }
                    case Stage::BackbufferDevice:
                    {
                        const auto result = backbuffer->GetDevice(IID_PPV_ARGS(&backbufferDevice));
                        return SUCCEEDED(result) && !backbufferDevice ? E_POINTER : result;
                    }
                    case Stage::CanonicalBackbufferDevice:
                    {
                        const auto result = backbufferDevice->QueryInterface(IID_PPV_ARGS(&backbufferIdentity));
                        if (SUCCEEDED(result) && backbufferIdentity == queueIdentity)
                            state.canonicalIdentityFlags |= 4;
                        return SUCCEEDED(result) && !backbufferIdentity ? E_POINTER : result;
                    }
                    default:
                        return E_INVALIDARG;
                }
            }
            bool Close() noexcept override
            {
                bool clean = true;
                clean = ReleaseGuarded(backbufferIdentity) && clean;
                clean = ReleaseGuarded(backbufferDevice) && clean;
                clean = ReleaseGuarded(backbuffer) && clean;
                clean = ReleaseGuarded(swapchainIdentity) && clean;
                clean = ReleaseGuarded(queueIdentity) && clean;
                clean = ReleaseGuarded(swapchainDevice) && clean;
                clean = ReleaseGuarded(queueDevice) && clean;
                clean = ReleaseGuarded(queue) && clean;
                clean = ReleaseGuarded(swapchain) && clean;
                return clean;
            }

        private:
            IUnknown *          borrowedSwapchain, *borrowedQueue;
            IDXGISwapChain3*    swapchain = nullptr;
            ID3D12CommandQueue* queue = nullptr;
            ID3D12Device *      queueDevice = nullptr, *swapchainDevice = nullptr, *backbufferDevice = nullptr;
            IUnknown *          queueIdentity = nullptr, *swapchainIdentity = nullptr, *backbufferIdentity = nullptr;
            ID3D12Resource*     backbuffer = nullptr;
        };
    }

    void RunGpuOwnershipQueries(GpuOwnershipQueries& queries, GpuOwnershipProbeState& state) noexcept
    {
        // Even reused output cannot retain an older successful identity or
        // description after a new query fails before populating those fields.
        const auto native = state;
        state = {};
        state.customPresentPresent = native.customPresentPresent;
        state.nativeExplicitGPUCount = native.nativeExplicitGPUCount;
        state.nativeAFRCount = native.nativeAFRCount;
        state.queryStatus = static_cast<std::uint32_t>(QueryStatus::Failed);
        for (std::uint32_t value = static_cast<std::uint32_t>(Stage::SwapchainQuery); value <= static_cast<std::uint32_t>(Stage::CanonicalBackbufferDevice);
             ++value)
        {
            state.queryStage = value;
            std::int32_t result = E_UNEXPECTED;
            if (!ExecuteGuarded(queries, static_cast<Stage>(value), state, result))
            {
                state.queryStatus = static_cast<std::uint32_t>(QueryStatus::Exception);
                state.hresult = static_cast<std::uint32_t>(E_UNEXPECTED);
                break;
            }
            state.hresult = static_cast<std::uint32_t>(result);
            if (result < 0)
                break;
            if (value == static_cast<std::uint32_t>(Stage::CanonicalBackbufferDevice))
            {
                state.metadataValid = 1;
                state.supportPredicate =
                    state.queueType == D3D12_COMMAND_LIST_TYPE_DIRECT && state.deviceNodeCount == 1 && (state.queueNodeMask == 0 || state.queueNodeMask == 1) &&
                    state.nativeExplicitGPUCount == 1 && state.nativeAFRCount == 1 && !state.customPresentPresent && state.canonicalIdentityFlags == 7 &&
                    state.swapchainBufferCount >= 2 && state.swapchainBufferCount <= 16 && state.currentBackbufferIndex < state.swapchainBufferCount &&
                    state.swapchainWidth && state.swapchainWidth <= 32768 && state.swapchainHeight && state.swapchainHeight <= 32768 &&
                    state.backbufferWidth == state.swapchainWidth && state.backbufferHeight == state.swapchainHeight && state.swapchainFormat &&
                    state.backbufferFormat == state.swapchainFormat && state.backbufferSampleCount == 1 && state.backbufferSampleQuality == 0;
                state.queryStatus = static_cast<std::uint32_t>(state.supportPredicate ? QueryStatus::Accepted : QueryStatus::Unsupported);
                state.queryStage = static_cast<std::uint32_t>(Stage::Complete);
                state.hresult = 0;
            }
        }
        if (!CloseGuarded(queries))
        {
            state.queryStage = static_cast<std::uint32_t>(Stage::ReleaseReferences);
            state.queryStatus = static_cast<std::uint32_t>(QueryStatus::CleanupFailed);
            state.metadataValid = state.supportPredicate = 0;
            state.hresult = static_cast<std::uint32_t>(E_UNEXPECTED);
        }
    }

    void ProbeD3D12Ownership(void* swapchain, void* queue, GpuOwnershipProbeState& state) noexcept
    {
        ComQueries queries(swapchain, queue);
        RunGpuOwnershipQueries(queries, state);
    }
}
