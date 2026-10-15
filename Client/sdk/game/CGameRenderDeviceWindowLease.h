/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Explicit device handoff for an MTA-owned surface inside the game window
 *****************************************************************************/
#pragma once
#include "CGameRenderObservation.h"
#include <cstddef>
#include <type_traits>

struct ID3D12Device;

namespace mta::game_de
{
    inline constexpr std::uint32_t RenderDeviceWindowLeaseVersionV2 = 2;
    struct RenderDeviceWindowLeaseV2
    {
        std::uint32_t size = sizeof(RenderDeviceWindowLeaseV2), version = RenderDeviceWindowLeaseVersionV2;
        // Only device transfers one acquired COM reference. The observed HWND
        // has no reference-counted lifetime: revalidate it before attaching an
        // MTA-owned child surface, and never treat it as native-frame authority.
        ID3D12Device* device = nullptr;
        void*         window = nullptr;
        std::uint64_t generation = 0, acceptedSnapshots = 0, dispatchEntries = 0;
        std::uint32_t sourceProfileId = 0, windowed = 0, reserved[2]{};
    };
    static_assert(std::is_standard_layout_v<RenderDeviceWindowLeaseV2> && std::is_trivially_copyable_v<RenderDeviceWindowLeaseV2>);
#if defined(_WIN64)
    static_assert(sizeof(RenderDeviceWindowLeaseV2) == 64 && offsetof(RenderDeviceWindowLeaseV2, device) == 8 &&
                  offsetof(RenderDeviceWindowLeaseV2, window) == 16 && offsetof(RenderDeviceWindowLeaseV2, generation) == 24 &&
                  offsetof(RenderDeviceWindowLeaseV2, sourceProfileId) == 48 && offsetof(RenderDeviceWindowLeaseV2, reserved) == 56);
#endif
    // V1 and V2 requests are mutually exclusive on a fresh V6 factory-thread
    // lease, before Begin. V2 observes the HWND/Windowed flag from the same owned
    // swapchain query that supplies the device. Only a current-process root HWND
    // in windowed mode may publish; no native queue/backbuffer is transferred.
    // Take is output-only and moves the acquired ref once without COM calls.
    // Every non-Ok result preserves output. End releases an untaken ref only
    // after hook/callback quiescence, using the V1 irreversible Release-fault rule.
    using RequestRenderDeviceWindowLeaseV2 = Status (*)(std::uint32_t version, IGameRenderObservationV6* retainedLease) noexcept;
    using TakeRenderDeviceWindowLeaseV2 = Status (*)(std::uint32_t version, IGameRenderObservationV6* retainedLease,
                                                     RenderDeviceWindowLeaseV2* output) noexcept;
}

#if defined(_WIN32) && defined(MTA_GAME_DE_EXPORTS)
extern "C" __declspec(dllexport) mta::game_de::Status RequestGameRenderDeviceWindowLeaseV2(std::uint32_t                           version,
                                                                                           mta::game_de::IGameRenderObservationV6* retainedLease) noexcept;
extern "C" __declspec(dllexport) mta::game_de::Status TakeGameRenderDeviceWindowLeaseV2(std::uint32_t                            version,
                                                                                        mta::game_de::IGameRenderObservationV6*  retainedLease,
                                                                                        mta::game_de::RenderDeviceWindowLeaseV2* output) noexcept;
#else
extern "C" mta::game_de::Status RequestGameRenderDeviceWindowLeaseV2(std::uint32_t version, mta::game_de::IGameRenderObservationV6* retainedLease) noexcept;
extern "C" mta::game_de::Status TakeGameRenderDeviceWindowLeaseV2(std::uint32_t version, mta::game_de::IGameRenderObservationV6* retainedLease,
                                                                  mta::game_de::RenderDeviceWindowLeaseV2* output) noexcept;
#endif
