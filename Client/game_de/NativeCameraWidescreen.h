/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Observe configured widescreen and retain uncertain one-byte writes
 ****************************************************************************/
#pragma once
#include <game/CGameClockService.h>
#include <atomic>
#include <cstdint>
#include <memory>

namespace mta::game_de
{
    namespace NativeCameraWidescreenCalls
    {
        using CheckOwner = ClockResult (*)() noexcept;
        using Exchange = ClockResult (*)(std::uintptr_t, std::uint8_t, std::uint8_t, std::uint8_t&, bool&, std::uint32_t&) noexcept;
        struct Receipt
        {
            std::uintptr_t image = 0, target = 0, root = 0, world = 0;
            std::uint8_t   before = 0, replacement = 0, observed = 0, after = 0;
            std::uint32_t  exceptionCode = 0;
            bool           entered = false, returned = false, accepted = false, knownNoEffect = false, effectsVerified = false, debt = false;
            ClockResult    result = ClockResult::NotInitialized;
        };
    }
    class NativeCameraWidescreen final
    {
    public:
        using CheckOwner = NativeCameraWidescreenCalls::CheckOwner;
        using Exchange = NativeCameraWidescreenCalls::Exchange;
        using Receipt = NativeCameraWidescreenCalls::Receipt;
        NativeCameraWidescreen() noexcept;
        ~NativeCameraWidescreen();
        NativeCameraWidescreen(const NativeCameraWidescreen&) = delete;
        NativeCameraWidescreen& operator=(const NativeCameraWidescreen&) = delete;
        ClockResult             Read(std::uintptr_t image, CheckOwner owner, CheckOwner afterApplicationOwner, bool& output) noexcept;
        ClockResult Write(std::uintptr_t image, bool mode, CheckOwner owner, CheckOwner afterApplicationOwner, Receipt* = nullptr, Exchange = nullptr) noexcept;
        bool        IsRetained() const noexcept { return busy.load() || debt.load(); }
        bool        RecoveryPending() const noexcept { return debt.load(); }

    private:
        // Accepted or unknown writes keep this first journal/module if their
        // mode/neighbor/world acknowledgement fails. End never repairs flags.
        struct Data;
        std::unique_ptr<Data> data;
        std::atomic<bool>     busy{false}, debt{false};
    };
}
