/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Callback-local read-only COM ownership diagnostics
 *****************************************************************************/
#pragma once
#include "../sdk/game/CGameRenderObservation.h"

namespace mta::game_de
{
    // The stage executor is shared by the real COM transaction and fault tests.
    // It owns only callback-local acquired references; Close attempts every
    // release even when an earlier query or release fails.
    class GpuOwnershipQueries
    {
    public:
        virtual std::int32_t Execute(GpuOwnershipQueryStage stage, GpuOwnershipProbeState& state) = 0;
        virtual bool         Close() noexcept = 0;

    protected:
        ~GpuOwnershipQueries() = default;
    };
    void RunGpuOwnershipQueries(GpuOwnershipQueries& queries, GpuOwnershipProbeState& state) noexcept;
    void ProbeD3D12Ownership(void* swapchain, void* queue, GpuOwnershipProbeState& state) noexcept;
}
