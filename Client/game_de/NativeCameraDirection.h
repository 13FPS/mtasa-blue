/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Retain the exact ordered camera-angle pair after uncertain writes
 ****************************************************************************/
#pragma once
#include "NativeCameraDataMemory.h"
#include <atomic>
#include <memory>
namespace mta::game_de
{
    namespace NativeCameraDirectionCalls
    {
        using CheckOwner = NativeCameraDataMemory::CheckOwner;
        using Exchange = ClockResult (*)(std::uintptr_t, std::uint32_t, std::uint32_t, std::uint32_t&, std::uint32_t&) noexcept;
        struct Bits
        {
            std::uint32_t horizontal = 0, vertical = 0;
        };
        struct Step
        {
            std::uintptr_t target = 0;
            std::uint32_t  before = 0, replacement = 0, observed = 0, after = 0, exceptionCode = 0;
            bool           entered = false, returned = false, accepted = false;
        };
        struct Receipt
        {
            std::uintptr_t image = 0, root = 0, world = 0;
            std::uint32_t  logicalRow = 0;
            Bits           expected, original, replacement, after;
            Step           horizontal, vertical;
            bool           getter = false, repair = false, knownNoEffect = false, effectsVerified = false, debt = false;
            ClockResult    result = ClockResult::NotInitialized;
        };
    }
    class NativeCameraDirection final
    {
    public:
        using CheckOwner = NativeCameraDirectionCalls::CheckOwner;
        using Exchange = NativeCameraDirectionCalls::Exchange;
        using Bits = NativeCameraDirectionCalls::Bits;
        using Receipt = NativeCameraDirectionCalls::Receipt;
        NativeCameraDirection() noexcept;
        ~NativeCameraDirection();
        NativeCameraDirection(const NativeCameraDirection&) = delete;
        NativeCameraDirection& operator=(const NativeCameraDirection&) = delete;
        ClockResult            Set(std::uintptr_t image, std::uint32_t row, std::uint32_t wrappedH, std::uint32_t wrappedV, CheckOwner owner,
                                   CheckOwner afterApplicationOwner, Receipt* = nullptr, Exchange = nullptr) noexcept;
        ClockResult            Repair(std::uintptr_t image, std::uint32_t row, std::uint32_t expectedH, std::uint32_t expectedV, std::uint32_t wrappedH,
                                      std::uint32_t wrappedV, CheckOwner owner, CheckOwner afterApplicationOwner, Receipt* = nullptr, Exchange = nullptr) noexcept;
        bool                   IsRetained() const noexcept { return busy.load() || debt.load(); }
        bool                   RecoveryPending() const noexcept { return debt.load(); }

    private:
        // Both stores belong to one source-ordered request. Once the first
        // CAS is accepted, even a known second mismatch keeps this journal;
        // retry, rollback, End repair and generation reuse are not authorized.
        struct Data;
        std::unique_ptr<Data> data;
        std::atomic<bool>     busy{false}, debt{false};
        ClockResult           Run(std::uintptr_t, std::uint32_t, const Bits*, const Bits&, CheckOwner, CheckOwner, Receipt*, Exchange) noexcept;
    };
}
