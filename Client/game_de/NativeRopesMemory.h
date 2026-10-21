/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Native rope transactions and immediate registered-reference checks
 *****************************************************************************/
#pragma once
#include "NativeRopesAccess.h"
#include <atomic>

namespace mta::game_de
{
    class NativeRopesMemory final
    {
    public:
        struct Vector12
        {
            float x = 0, y = 0, z = 0;
        };
        using CheckOwner = ClockResult (*)() noexcept;
        // The native Win64 ABI consumes a borrowed12-byte vector pointer and
        // low bytes of the promoted bool/count/flags arguments. Duration is a
        // real eighth DWORD argument, so no instruction immediate is patched.
        using CreateFunction = std::int32_t (*)(std::uintptr_t, std::uint32_t, const Vector12*, std::uint32_t, std::uint32_t, std::uint32_t, std::uintptr_t,
                                                std::uint32_t);
        using RemoveFunction = void (*)(void*);
        using CreateInvoke = ClockResult (*)(CreateFunction, std::uintptr_t, const Vector12&, std::uint32_t, std::int32_t&, std::uint32_t&) noexcept;
        using RemoveInvoke = ClockResult (*)(RemoveFunction, void*, std::uint32_t&) noexcept;
        struct Receipt
        {
            std::uintptr_t identity = 0, row = 0;
            std::uint32_t  exceptionCode = 0;
            std::int32_t   nativeResult = 0;
            bool           entered = false, returned = false, counterCommitted = false;
        };
        ClockResult Bind() noexcept;
        void        Reset() noexcept;
        bool        IsBound() const noexcept;
        ClockResult Qualify(CheckOwner) const noexcept;
        ClockResult Create(const CVector&, std::uint32_t duration, std::int32_t& output, CheckOwner, Receipt* = nullptr, CreateInvoke = nullptr) const noexcept;
        ClockResult Remove(std::uintptr_t opaqueKey, CheckOwner, Receipt* = nullptr, RemoveInvoke = nullptr) const noexcept;
        // This object owns only an image/thread binding. Each method borrows
        // the existing real engine callback/generation and retains no native
        // entity, reference node or row pointer when returning. Entered failures
        // must stop that lease; do not retry or roll native actions back at End.
    private:
        ClockResult                 Current(std::uintptr_t&) const noexcept;
        std::atomic<std::uintptr_t> imageAddress{0};
        std::atomic<std::uint32_t>  ownerThread{0};
    };
}
