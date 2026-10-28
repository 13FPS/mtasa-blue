/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: One native present-hook owner with a bounded diagnostic consumer
 *****************************************************************************/

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include "../sdk/game/CGameRenderObservation.h"
#include "../sdk/game/CGameRenderSetupDiagnostic.h"
#include "../sdk/game/CGameClockService.h"

namespace mta::game_de::NativePresentOwner
{
    using PresentFunction = bool (*)(void*, std::uint32_t);

    struct RouteDescriptor
    {
        RenderObservationRoute        route;
        std::uintptr_t                presentRva;
        std::size_t                   presentBytes;
        std::array<unsigned char, 32> presentHash;
        std::uintptr_t                viewportTable, swapchainOffset;
    };

    struct Config
    {
        RenderObservationRoute route = RenderObservationRoute::D3D11;
        bool                   queryGpuOwnership = false, classifyCustomPresent = false, queryFrameBoundary = false;
        bool                   nativeDispatch = false, captureDevice = false, captureDeviceWindow = false;
    };

    // The callback receives the real admitted original and current hook depth.
    // It must forward that original exactly once. Its storage/function belongs
    // to the pinned module, never to a NativeRenderObservation object.
    struct CallbackContext
    {
        PresentFunction original = nullptr;
        std::uint64_t   generation = 0;
        unsigned        depth = 0;
    };
    using Consumer = bool (*)(const CallbackContext&, void*, std::uint32_t);
    using FailureHandler = void (*)(std::uint32_t) noexcept;

    // This serializes the actual shared manager's worker operations, including
    // diagnostic device retirement. A Busy refusal never waits for a callback.
    class Operation final
    {
    public:
        Operation() noexcept;
        ~Operation();
        Operation(const Operation&) = delete;
        Operation& operator=(const Operation&) = delete;
        const bool acquired;
    };

    struct State
    {
        std::uint64_t          generation = 0;
        bool                   bound = false, hooked = false, modulePinned = false, retained = false, pendingHookWork = false;
        std::uint32_t          producerThread = 0;
        std::uintptr_t         image = 0;
        const RouteDescriptor* route = nullptr;
    };

    using PolicyIntent = bool (*)(std::uint64_t) noexcept;
    struct PolicyState
    {
        std::uint64_t generation = 0, callbacks = 0, applied = 0, rejected = 0;
        bool          hooked = false, active = false, retiring = false, poisoned = false, pendingCleanup = false, modulePinned = false;
        ClockResult   failure = ClockResult::NotInitialized;
    };
    // The installed policy has a separate epoch from bounded diagnostic leases.
    // Its only callback borrow is static code; client/CGame storage is never read.
    ClockResult BeginPolicy(const Operation&, PolicyIntent, std::uint64_t&) noexcept;
    ClockResult RetirePolicy(std::uint64_t) noexcept;
    ClockResult EndPolicy(const Operation&, std::uint64_t) noexcept;
    PolicyState ReadPolicyState() noexcept;
    bool        SharedDiagnostic(std::uint64_t) noexcept;

    std::uint64_t CurrentGeneration() noexcept;
    bool          ModulePinned() noexcept;
    State         ReadOwnerState() noexcept;
    // The diagnostic's borrowed COM entry needs the same executable-page guard
    // as the manager, without copying its policy or issuing another read.
    bool ValidatePages(std::uintptr_t address, std::size_t bytes, std::uintptr_t image, bool executable) noexcept;
    bool ReadBytes(std::uintptr_t address, void* destination, std::size_t bytes, std::uintptr_t image = 0, bool executable = false) noexcept;
    bool Reserve(const Operation& operation, const Config& config, Consumer consumer, FailureHandler failure, std::uint64_t& generation) noexcept;
    bool Install(const Operation& operation, std::uint64_t generation) noexcept;
    void FinishFailedInstall(const Operation& operation, std::uint64_t generation) noexcept;
    void StopBoundaryAdmission(const Operation& operation, std::uint64_t generation) noexcept;
    bool Detach(const Operation& operation, std::uint64_t generation) noexcept;
    void CleanupIncomplete(const Operation& operation, std::uint64_t generation) noexcept;
    bool CompleteCleanup(const Operation& operation, std::uint64_t generation) noexcept;
    bool Release(const Operation& operation, std::uint64_t generation) noexcept;
    bool ReadSetupDiagnostic(std::uint64_t generation, RenderSetupDiagnosticV1& state) noexcept;
}
