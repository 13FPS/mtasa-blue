/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: One explicit workload on the existing retained native timing owner
 ****************************************************************************/
#pragma once
#include <windows.h>
#include <game/CGameMoveAnimWorkload.h>
#include "NativeGameTimingAccess.h"
#include <array>
#include <atomic>

namespace mta::game_de
{
    class NativeMoveAnimWorkload final
    {
    public:
        NativeMoveAnimWorkload() = default;
        NativeMoveAnimWorkload(const NativeMoveAnimWorkload&) = delete;
        NativeMoveAnimWorkload& operator=(const NativeMoveAnimWorkload&) = delete;
        static bool             ValidRequest(const MoveAnimWorkloadRequestV1&) noexcept;
        // Reset/Retired are producer-only after the prior native owner retires.
        // Tick alone owns the token and operation prefix on real engine frames.
        bool Reset(const MoveAnimWorkloadRequestV1&) noexcept;
        void Tick(NativeGameTimingAccess&, std::uint64_t nowMs) noexcept;
        void RequestStop() noexcept;
        bool Read(MoveAnimWorkloadStateV1&) const noexcept;
        void NotEntered(ClockResult) noexcept;
        void Retired(const ClockServiceStateV1&) noexcept;

    private:
        enum class Step : std::uint32_t
        {
            Candidate,
            Group,
            Acquire,
            Enable,
            Publish,
            Write,
            Reapply,
            Observe,
            Withdraw,
            Release,
            Finished
        };
        void                                                                                            Publish() noexcept;
        void                                                                                            Fail(ClockResult, bool uncertain = false) noexcept;
        bool                                                                                            SameCurrentPlayer(NativeGameTimingAccess&) noexcept;
        MoveAnimWorkloadRequestV1                                                                       request;
        MoveAnimWorkloadStateV1                                                                         state;
        NativePedReferenceToken                                                                         token;
        Step                                                                                            step = Step::Finished;
        std::uint64_t                                                                                   observedSince = 0;
        std::atomic<bool>                                                                               stopRequested{false};
        mutable std::atomic<std::uint64_t>                                                              publication{0};
        std::array<std::atomic<std::uint32_t>, sizeof(MoveAnimWorkloadStateV1) / sizeof(std::uint32_t)> words{};
    };
}
