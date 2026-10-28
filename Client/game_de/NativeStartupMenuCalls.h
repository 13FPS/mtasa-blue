/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Journal one actual loaded-world native frontend close
 ****************************************************************************/
#pragma once
#include <game/CGameClockService.h>
#include <atomic>
#include <cstdint>

namespace mta::game_de::NativeStartupMenuCalls
{
    using CheckOwner = ClockResult (*)() noexcept;
    using NativeFunction = bool (*)(std::uintptr_t);
    using Invoke = ClockResult (*)(NativeFunction, std::uintptr_t, bool&, std::uint32_t&) noexcept;
    enum class Stage : std::uint32_t
    {
        Empty,
        Preparing,
        Entering,
        Complete,
        FaultDebt
    };
    struct Receipt
    {
        std::uint64_t  generation = 0;
        std::uintptr_t root = 0, gameMode = 0, level = 0, world = 0, widgetBefore = 0, widgetAfter = 0;
        std::uint32_t  auxiliaryBefore = 0, auxiliaryAfter = 0, exceptionCode = 0;
        unsigned char  flagBefore = 0, flagAfter = 0;
        bool           entered = false, returned = false, nativeResult = false, verified = false, debt = false;
    };
    struct Access;
    class Record final
    {
    public:
        Record() noexcept = default;
        Record(const Record&) = delete;
        Record& operator=(const Record&) = delete;
        Record(Record&&) = delete;
        Record& operator=(Record&&) = delete;
        // A receipt owns no widget. Teardown must never remove it again,
        // reconstruct its owner, or rewind any accepted native side effect.
        ~Record() = default;
        Stage GetStage() const noexcept { return stage.load(std::memory_order_acquire); }
        bool  CanDiscard() const noexcept { return GetStage() == Stage::Empty || GetStage() == Stage::Complete; }
        bool  HasDebt() const noexcept { return GetStage() == Stage::FaultDebt; }
        void  MarkUnacknowledged() noexcept;

    private:
        friend struct Access;
        std::atomic<Stage> stage{Stage::Empty};
        Receipt            journal{};
    };
    // This is later Initialize's loaded-world timing owner, not application
    // startup, a menu observation, or a producer-made UObject lifetime lease.
    ClockResult CheckReady(std::uintptr_t image, CheckOwner) noexcept;
    // Exactly one attempt per record. A false native result means no widget
    // was owned; it still performs native effects and needs acknowledgement.
    ClockResult Close(std::uintptr_t image, Record&, std::uint64_t generation, CheckOwner, Receipt* = nullptr, Invoke = nullptr) noexcept;
}
