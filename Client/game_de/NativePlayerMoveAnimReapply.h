/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: One current-group native original with first-uncertainty retention
 ****************************************************************************/
#pragma once
#include "NativePedPoolReader.h"
#include <atomic>
#include <memory>
namespace mta::game_de::NativePlayerMoveAnimReapply
{
    using Ped = NativePedPoolReader::Value;
    using CheckOwner = NativePedPoolReader::CheckOwner;
    using NativeFunction = void (*)(std::uintptr_t nativePed);
    using Invoke = ClockResult (*)(NativeFunction, std::uintptr_t nativePed, std::uint32_t& exceptionCode) noexcept;
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
        std::uintptr_t image = 0, entryRva = 0, nativePed = 0, playerData = 0, render = 0, container = 0, world = 0;
        std::uint32_t  handle = 0, exceptionCode = 0;
        std::int32_t   currentSelector = -1, beforeGroup = 0, afterGroup = 0;
        bool           entered = false, returned = false, acknowledged = false, unknown = false;
    };
    class Controller final
    {
    public:
        Controller() noexcept;
        ~Controller();
        Controller(const Controller&) = delete;
        Controller& operator=(const Controller&) = delete;
        ClockResult Reapply(std::uintptr_t image, const Ped&, CheckOwner owner, CheckOwner afterApplicationOwner, Receipt* = nullptr,
                            Invoke = nullptr) noexcept;
        bool        IsRetained() const noexcept { return busy.load() || debt.load(); }
        bool        RecoveryPending() const noexcept { return debt.load(); }
        // The enclosing runtime must also retain a call whose inner ACK was
        // followed by loss of the exact registered CPU/native token. This only
        // records uncertainty; it cannot replay, repair or enter native code.
        void RetainAfterEntry(const Receipt&, ClockResult) noexcept;

    private:
        struct Data;
        std::unique_ptr<Data> data;
        std::atomic<bool>     busy{false}, debt{false};
    };
}
