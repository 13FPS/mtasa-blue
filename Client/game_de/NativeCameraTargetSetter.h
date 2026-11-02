/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Acknowledge retarget effects on an existing engine-owned camera row
 ****************************************************************************/
#pragma once
#include "NativeCameraRowBorrow.h"
#include "NativePedPoolReader.h"
#include "NativePedReferenceToken.h"
#include <atomic>
#include <memory>

namespace mta::game_de
{
    namespace NativeCameraTargetSetterCalls
    {
        using CheckOwner = ClockResult (*)() noexcept;
        using RegisterFunction = void (*)(void*, void**, bool);
        using UnregisterFunction = void (*)(void*, void**);
        using InvokeRegister = ClockResult (*)(RegisterFunction, void*, void**, bool, std::uint32_t&) noexcept;
        using InvokeUnregister = ClockResult (*)(UnregisterFunction, void*, void**, std::uint32_t&) noexcept;
        using ExchangePointer = ClockResult (*)(std::uintptr_t, std::uintptr_t, std::uintptr_t, std::uintptr_t&, bool&, std::uint32_t&) noexcept;
        using ResolveTarget = ClockResult (*)(void*, const NativePedReferenceToken&, NativePedPoolReader::Value&) noexcept;

        struct Resolver
        {
            // Runtime supplies its own ResolvePedReference endpoint. This
            // borrows an existing identity; it creates no entity or lease.
            void*         owner = nullptr;
            ResolveTarget resolve = nullptr;
        };
        enum class Prefix : std::uint8_t
        {
            None,
            Preflight,
            OldUnregistered,
            PointerReplaced,
            NewRegistered,
            Complete
        };
        struct NativeCallReceipt
        {
            std::uint32_t exceptionCode = 0;
            bool          entered = false, returned = false, verified = false;
        };
        struct PointerReceipt
        {
            std::uintptr_t observed = 0;
            std::uint32_t  exceptionCode = 0;
            bool           entered = false, returned = false, accepted = false, verified = false;
        };
        struct Receipt
        {
            std::uintptr_t    image = 0, cell = 0, oldTarget = 0, newTarget = 0;
            std::uintptr_t    beforeOldHead = 0, beforeNewHead = 0, beforeFreeHead = 0;
            std::uintptr_t    afterOldHead = 0, afterNewHead = 0, afterFreeHead = 0;
            std::uint64_t     generation = 0, admissionSerial = 0, rowTokenId = 0;
            std::uint32_t     row = 0, oldHandle = 0, newHandle = 0, changedReferenceNodes = 0;
            NativeCallReceipt unregisterCall, registerCall;
            PointerReceipt    pointerWrite;
            Prefix            acknowledged = Prefix::None;
            bool              noEffect = false, effectsEntered = false, effectsVerified = false, debt = false;
            ClockResult       result = ClockResult::NotInitialized;
        };
    }

    class NativeCameraTargetSetter final
    {
    public:
        using CheckOwner = NativeCameraTargetSetterCalls::CheckOwner;
        using Resolver = NativeCameraTargetSetterCalls::Resolver;
        using Receipt = NativeCameraTargetSetterCalls::Receipt;
        NativeCameraTargetSetter() noexcept;
        ~NativeCameraTargetSetter();
        NativeCameraTargetSetter(const NativeCameraTargetSetter&) = delete;
        NativeCameraTargetSetter& operator=(const NativeCameraTargetSetter&) = delete;

        // nullptr is an explicit clear request; an invalid nonnull token
        // refuses. The row retains the module, never the target entity.
        ClockResult Set(std::uintptr_t image, NativeCameraRowBorrow& ledger, const NativeCameraRowBorrow::Identity& identity,
                        const NativeCameraRowBorrow::Token& row, const NativePedReferenceToken* newTarget, Resolver, CheckOwner owner,
                        CheckOwner afterApplicationOwner, Receipt* = nullptr, NativeCameraTargetSetterCalls::InvokeUnregister = nullptr,
                        NativeCameraTargetSetterCalls::ExchangePointer = nullptr, NativeCameraTargetSetterCalls::InvokeRegister = nullptr) noexcept;
        bool        IsRetained() const noexcept { return busy.load() || debt.load(); }
        bool        RecoveryPending() const noexcept { return debt.load(); }

    private:
        // This journal tracks engine-owned reference surgery. Unknown or
        // partial effects retain the first journal/module without replay,
        // End rollback, private-cell adoption, release or allocation.
        struct Data;
        std::unique_ptr<Data> data;
        std::atomic<bool>     busy{false}, debt{false};
    };
}
