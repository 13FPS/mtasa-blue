/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Read-only scoped observations at an admitted standard Present entry
 *****************************************************************************/
#pragma once
#include "../sdk/game/CGameRenderObservation.h"
#include <cstddef>

namespace mta::game_de
{
    // The query executor lets owned fixtures exercise faults and cleanup without
    // loading the game, installing its hooks, or submitting GPU work.
    class FrameBoundaryQueries
    {
    public:
        virtual std::int32_t Execute(FrameBoundaryQueryStage stage, FrameBoundaryObservationState& state) = 0;
        virtual bool         Close() noexcept = 0;

    protected:
        ~FrameBoundaryQueries() = default;
    };
    bool RunFrameBoundaryQueries(FrameBoundaryQueries& queries, FrameBoundaryObservationState& state) noexcept;

    struct FrameBoundaryScope
    {
        FrameBoundaryObservationState state;
        std::uintptr_t                image = 0, viewport = 0, swapchain = 0;
        std::uint32_t                 callbackThread = 0;
        bool                          knownPresenter = false, cleanupFailed = false, entered = false;
        std::uint64_t                 epoch = 0;
        void (*cleanupFailure)() noexcept = nullptr;
        FrameBoundaryScope* previous = nullptr;
    };
    // Prepare is worker-only and performs no COM calls. Change appends to the
    // observer's existing all-thread Detours transaction; it never commits one.
    bool     PrepareFrameBoundaryHooks(std::uintptr_t image) noexcept;
    bool     ChangeFrameBoundaryHooks(bool install) noexcept;
    bool     FrameBoundaryHooksCommitted(bool install) noexcept;
    bool     FrameBoundaryHooksOwned() noexcept;
    unsigned FrameBoundaryInFlight() noexcept;
    void     StopFrameBoundaryAdmission() noexcept;
    void     ResetFrameBoundaryObservation() noexcept;
    void     EnterFrameBoundaryScope(FrameBoundaryScope& scope, std::uintptr_t image, void* viewport, std::uint32_t callbackThread,
                                     const CustomPresentClassificationState& custom, void (*cleanupFailure)() noexcept = nullptr) noexcept;
    void     LeaveFrameBoundaryScope(FrameBoundaryScope& scope) noexcept;
    // API5 alone may normalize our verified installed pre-release patch back to
    // its fully pinned original bytes. Historical API4 uses its original reader.
    bool ReadFrameBoundaryOwnedOriginal(std::uintptr_t address, void* output, std::size_t bytes) noexcept;
}
