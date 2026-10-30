/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: MTA block-registration policy over the current qualified DE bank
 ****************************************************************************/
#pragma once
#include "NativeAnimationBlockAccess.h"
#include <atomic>
#include <memory>

namespace mta::game_de
{
    namespace NativeAnimationBlockRegistrationCalls
    {
        using CheckOwner = ClockResult (*)() noexcept;
        struct Store
        {
            std::uintptr_t address = 0;
            std::uint32_t  value = 0;
        };
        using Write = ClockResult (*)(const Store*, std::size_t, std::uint32_t&) noexcept;
        struct Receipt
        {
            std::uint64_t generation = 0;
            std::int32_t  index = -1, beforeCount = 0, afterCount = 0;
            std::uint32_t group = 0, stores = 0, exceptionCode = 0;
            bool          entered = false, returned = false, effectsVerified = false, noWrite = false, debt = false;
            ClockResult   result = ClockResult::NotInitialized;
        };
    }
    namespace AnimationBlockRegistrationDetail
    {
        ClockResult WriteStores(const NativeAnimationBlockRegistrationCalls::Store*, std::size_t, std::uint32_t&) noexcept;
    }
    class NativeAnimationBlockRegistration final
    {
    public:
        using CheckOwner = NativeAnimationBlockRegistrationCalls::CheckOwner;
        using Receipt = NativeAnimationBlockRegistrationCalls::Receipt;
        using Write = NativeAnimationBlockRegistrationCalls::Write;
        NativeAnimationBlockRegistration() noexcept;
        ~NativeAnimationBlockRegistration();
        NativeAnimationBlockRegistration(const NativeAnimationBlockRegistration&) = delete;
        NativeAnimationBlockRegistration& operator=(const NativeAnimationBlockRegistration&) = delete;
        ClockResult Register(std::uintptr_t image, std::uint64_t generation, const NativeAnimationBlockName&, CheckOwner owner, CheckOwner postFrameOwner,
                             Receipt* = nullptr, Write = nullptr) noexcept;
        bool        IsRetained() const noexcept { return busy.load() || debt.load(); }
        bool        RecoveryPending() const noexcept { return debt.load(); }

    private:
        // A partial ordered publication is not rolled back, repaired or
        // replayed. Its CPU journal and module fence survive uncertainty.
        struct Data;
        std::unique_ptr<Data> data;
        std::atomic<bool>     busy{false}, debt{false};
    };
}
