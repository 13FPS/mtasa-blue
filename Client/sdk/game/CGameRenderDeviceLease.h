/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Explicit one-shot device ownership handoff for an MTA-owned workload
 *****************************************************************************/
#pragma once
#include "CGameRenderObservation.h"
#include <cstddef>
#include <type_traits>

struct ID3D12Device;

namespace mta::game_de
{
    inline constexpr std::uint32_t RenderDeviceLeaseVersionV1 = 1;
    struct RenderDeviceLeaseV1
    {
        std::uint32_t size = sizeof(RenderDeviceLeaseV1), version = RenderDeviceLeaseVersionV1;
        // Ok transfers exactly one acquired COM reference. The receiver owns
        // Release; this is never a borrowed native root or a frame permission.
        ID3D12Device* device = nullptr;
        std::uint64_t generation = 0, acceptedSnapshots = 0, dispatchEntries = 0;
        std::uint32_t sourceProfileId = 0, reserved = 0;
    };
    static_assert(std::is_standard_layout_v<RenderDeviceLeaseV1> && std::is_trivially_copyable_v<RenderDeviceLeaseV1>);
#if defined(_WIN64)
    static_assert(sizeof(RenderDeviceLeaseV1) == 48 && offsetof(RenderDeviceLeaseV1, device) == 8 && offsetof(RenderDeviceLeaseV1, generation) == 16 &&
                  offsetof(RenderDeviceLeaseV1, sourceProfileId) == 40);
#endif
    // Request belongs to a fresh, allocated V6 lease on its factory thread,
    // before its first Begin. It is consumed once, even if Begin fails. Ordinary
    // V6 observers retain no device. Take is owner-thread-only while active;
    // NotInitialized means no transferable result, and leaves output untouched.
    // The record is output-only; callers do not initialize size or reserved as input.
    // The first coherent true-returned callback publishes one existing owned
    // reference. Take transfers it once without AddRef or native pointer reads.
    // End releases an untaken result after callback/hook quiescence. A Release
    // fault retains cleanup ownership and cannot authorize a new generation.
    using RequestRenderDeviceLeaseV1 = Status (*)(std::uint32_t version, IGameRenderObservationV6* retainedLease) noexcept;
    using TakeRenderDeviceLeaseV1 = Status (*)(std::uint32_t version, IGameRenderObservationV6* retainedLease, RenderDeviceLeaseV1* output) noexcept;
}

#if defined(_WIN32) && defined(MTA_GAME_DE_EXPORTS)
extern "C" __declspec(dllexport) mta::game_de::Status RequestGameRenderDeviceLeaseV1(std::uint32_t                           version,
                                                                                     mta::game_de::IGameRenderObservationV6* retainedLease) noexcept;
extern "C" __declspec(dllexport) mta::game_de::Status TakeGameRenderDeviceLeaseV1(std::uint32_t version, mta::game_de::IGameRenderObservationV6* retainedLease,
                                                                                  mta::game_de::RenderDeviceLeaseV1* output) noexcept;
#else
extern "C" mta::game_de::Status RequestGameRenderDeviceLeaseV1(std::uint32_t version, mta::game_de::IGameRenderObservationV6* retainedLease) noexcept;
extern "C" mta::game_de::Status TakeGameRenderDeviceLeaseV1(std::uint32_t version, mta::game_de::IGameRenderObservationV6* retainedLease,
                                                            mta::game_de::RenderDeviceLeaseV1* output) noexcept;
#endif
