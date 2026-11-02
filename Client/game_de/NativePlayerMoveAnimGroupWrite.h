/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: One acknowledged current-player movement DWORD with retained debt
 ****************************************************************************/
#pragma once
#include "NativePedMoveAnimReader.h"
#include <atomic>

namespace mta::game_de::NativePlayerMoveAnimGroupWrite
{
    using Ped = NativePedPoolReader::Value;
    using CheckOwner = NativePedPoolReader::CheckOwner;
    using CheckGroup = ClockResult (*)(std::int32_t) noexcept;
    using Exchange = bool (*)(std::uintptr_t, std::int32_t, std::int32_t, std::int32_t&, std::uint32_t&) noexcept;
    enum class Phase : std::uint32_t
    {
        Refused,
        Admitted,
        Entered,
        Returned,
        Acknowledged,
        Unknown
    };
    struct Receipt
    {
        Phase          phase = Phase::Refused;
        ClockResult    result = ClockResult::NotInitialized;
        std::uintptr_t image = 0, nativePed = 0, address = 0, playerData = 0, world = 0;
        std::uint32_t  handle = 0, exceptionCode = 0;
        std::int32_t   selector = -1, before = 0, desired = 0, observed = 0, after = 0;
        bool           entered = false, returned = false, accepted = false, acknowledged = false, unknown = false;
    };
    class Controller final
    {
    public:
        ClockResult Write(std::uintptr_t image, const Ped&, std::int32_t desired, CheckOwner, CheckOwner afterApplication, CheckGroup, Receipt* = nullptr,
                          Exchange = nullptr) noexcept;
        // An outer registered-token bookend can fail after the scalar was
        // acknowledged. Preserve that first accepted prefix without replay or
        // pretending a native ped field can be restored safely during End.
        void RetainAfterEntry(const Receipt&, ClockResult) noexcept;
        bool IsRetained() const noexcept { return retained.load(); }
        bool RecoveryPending() const noexcept { return retained.load(); }
        bool Read(Receipt&) const noexcept;

    private:
        mutable std::atomic_flag operation = ATOMIC_FLAG_INIT;
        std::atomic<bool>        retained{false};
        Receipt                  firstUnknown;
    };
}
