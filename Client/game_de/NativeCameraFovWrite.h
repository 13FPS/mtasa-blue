/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: One current camera FOV store with retained atomic uncertainty
 ****************************************************************************/
#pragma once
#include "NativeCameraDataMemory.h"
#include <atomic>
#include <memory>
namespace mta::game_de
{
    namespace NativeCameraFovWriteCalls
    {
        using CheckOwner = NativeCameraDataMemory::CheckOwner;
        using Exchange = ClockResult (*)(std::uintptr_t, std::uint32_t, std::uint32_t, std::uint32_t&, std::uint32_t&) noexcept;
        struct Receipt
        {
            std::uintptr_t image = 0, target = 0, root = 0, world = 0;
            std::uint32_t  logicalRow = 0, before = 0, replacement = 0, observed = 0, after = 0, exceptionCode = 0;
            bool           entered = false, returned = false, accepted = false, knownNoEffect = false, effectsVerified = false, debt = false;
            ClockResult    result = ClockResult::NotInitialized;
        };
    }
    class NativeCameraFovWrite final
    {
    public:
        using CheckOwner = NativeCameraFovWriteCalls::CheckOwner;
        using Exchange = NativeCameraFovWriteCalls::Exchange;
        using Receipt = NativeCameraFovWriteCalls::Receipt;
        NativeCameraFovWrite() noexcept;
        ~NativeCameraFovWrite();
        NativeCameraFovWrite(const NativeCameraFovWrite&) = delete;
        NativeCameraFovWrite& operator=(const NativeCameraFovWrite&) = delete;
        ClockResult Write(std::uintptr_t image, std::uint32_t logicalRow, std::uint32_t replacement, CheckOwner owner, CheckOwner afterApplicationFrameOwner,
                          Receipt* = nullptr, Exchange = nullptr) noexcept;
        bool        IsRetained() const noexcept { return busy.load() || debt.load(); }
        bool        RecoveryPending() const noexcept { return debt.load(); }

    private:
        // A returned CAS mismatch proves this adapter stored nothing. Every
        // accepted/unknown write whose postconditions cannot be acknowledged
        // keeps this exact CPU journal/module. End never restores camera bits.
        struct Data;
        std::unique_ptr<Data> data;
        std::atomic<bool>     busy{false}, debt{false};
    };
}
