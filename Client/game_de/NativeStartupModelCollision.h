/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Journal MTA's conditional borrowed startup collision-pointer copy
 ****************************************************************************/
#pragma once
#include "NativeModelBankReader.h"
#include <atomic>

namespace mta::game_de::NativeStartupModelCollision
{
    using CheckOwner = NativeModelBankReader::CheckOwner;
    enum class Stage : std::uint32_t
    {
        Empty,
        Preparing,
        Entering,
        Complete,
        FaultDebt
    };
    struct CompareExchangeReceipt
    {
        std::uintptr_t observed = 0;
        std::uint32_t  exceptionCode = 0;
        bool           returned = false, accepted = false;
    };
    struct Receipt
    {
        std::uint64_t  generation = 0;
        std::uint32_t  targetModel = 0, sourceModel = 0, exceptionCode = 0;
        std::uintptr_t targetInterface = 0, sourceInterface = 0, targetField = 0, sourceCollision = 0, observed = 0;
        bool           entered = false, returned = false, accepted = false, verified = false, skipped = false, debt = false;
    };
    using Exchange = ClockResult (*)(std::uintptr_t field, std::uintptr_t expected, std::uintptr_t replacement, CompareExchangeReceipt&) noexcept;
    struct Access;
    class Record final
    {
    public:
        Record() noexcept = default;
        Record(const Record&) = delete;
        Record& operator=(const Record&) = delete;
        Record(Record&&) = delete;
        Record& operator=(Record&&) = delete;
        // This record owns acknowledgement only. Its destruction never frees
        // either borrowed collider or rewinds an accepted metadata pointer.
        ~Record() = default;
        Stage GetStage() const noexcept { return stage.load(std::memory_order_acquire); }
        bool  CanDiscard() const noexcept
        {
            const auto s = GetStage();
            return s == Stage::Empty || s == Stage::Complete;
        }
        bool HasDebt() const noexcept { return GetStage() == Stage::FaultDebt; }
        void MarkUnacknowledged() noexcept;

    private:
        friend struct Access;
        std::atomic<Stage> stage{Stage::Empty};
        Receipt            journal{};
    };
    // Current capability prerequisites only: no record, selected model field,
    // pointer CAS or persistent lifetime is produced by this observation.
    ClockResult CheckReady(std::uintptr_t image, CheckOwner) noexcept;
    // One attempt per record, including skips and rejected CAS. A later
    // explicit Initialize call may create new records; failure never retries.
    ClockResult Copy(std::uintptr_t image, Record&, std::uint64_t generation, std::uint32_t targetModel, std::uint32_t sourceModel, CheckOwner,
                     Receipt* = nullptr, Exchange = nullptr) noexcept;
    ClockResult CompareExchange(std::uintptr_t field, std::uintptr_t expected, std::uintptr_t replacement, CompareExchangeReceipt&) noexcept;
}
