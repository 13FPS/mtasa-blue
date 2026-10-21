/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Stable registered native pointer-cell storage and explicit debt
 ****************************************************************************/
#pragma once
#include "NativePedReferenceAccess.h"
#include <atomic>

namespace mta::game_de
{
    class NativePedReferenceMemory;
    class NativePedReferenceCell final
    {
    public:
        struct State
        {
            std::uintptr_t image = 0, allocationBase = 0, cellAddress = 0, nativePed = 0, node = 0;
            std::size_t    allocationBytes = 0;
            std::uint64_t  generation = 0;
            std::uint32_t  handle = 0;
            bool           registrationEntered = false, registrationVerified = false, cleanupEntered = false, uncertain = false;
        };
        NativePedReferenceCell() noexcept = default;
        ~NativePedReferenceCell() = default;
        NativePedReferenceCell(const NativePedReferenceCell&) = delete;
        NativePedReferenceCell& operator=(const NativePedReferenceCell&) = delete;
        NativePedReferenceCell(NativePedReferenceCell&&) = delete;
        NativePedReferenceCell& operator=(NativePedReferenceCell&&) = delete;
        bool                    IsRetained() const noexcept { return retained.load(); }
        bool                    HasReferences() const noexcept { return references.load(); }
        bool                    ReadState(State&) const noexcept;
        // Destruction deliberately never frees the private native-visible word.
        // The real owner MUST retain this metadata until acknowledged Release;
        // abandoning it quarantines storage, rather than creating a dangling
        // native pointer. This primitive supplies no Runtime unload gate by
        // itself; callers must include IsRetained in their retirement policy.
    private:
        friend class NativePedReferenceMemory;
        mutable std::atomic_flag   operation = ATOMIC_FLAG_INIT;
        std::atomic<bool>          retained{false}, references{false};
        State                      state;
        NativePedPoolReader::Value identity;
    };
    class NativePedReferenceMemory final
    {
    public:
        using Binding = NativePedPoolReader::Binding;
        using CheckOwner = NativePedPoolReader::CheckOwner;
        using RegisterFunction = void (*)(void*, void**, bool);
        using UnregisterFunction = void (*)(void*, void**);
        using InvokeRegister = ClockResult (*)(RegisterFunction, void*, void**, bool, std::uint32_t&) noexcept;
        using InvokeUnregister = ClockResult (*)(UnregisterFunction, void*, void**, std::uint32_t&) noexcept;
        enum class Operation : std::uint32_t
        {
            Register,
            Release
        };
        struct Receipt
        {
            Operation      operation = Operation::Register;
            std::uint32_t  handle = 0, exceptionCode = 0;
            std::uint64_t  generation = 0;
            std::uintptr_t nativePed = 0, cell = 0, node = 0;
            bool           entered = false, returned = false, registrationVerified = false, bankAbsent = false, allocationReleased = false;
        };
        // Both supplied checks borrow the actual existing callback owner. The
        // recovery check may survive expiry/stopping, but must retain the same
        // natural callback/image/generation authority. No numeric token alone
        // grants native entry or a lifetime lease across owner generations.
        NativePedReferenceMemory(Binding binding, CheckOwner normal, CheckOwner recovery, std::uint64_t generation) noexcept
            : binding(binding), normal(normal), recovery(recovery), generation(generation)
        {
        }
        static bool Qualify(Binding&) noexcept;
        static bool Revalidate(const Binding&) noexcept;
        ClockResult Register(std::uint32_t, NativePedReferenceCell&, Receipt* = nullptr, InvokeRegister = nullptr) const noexcept;
        ClockResult Read(const NativePedReferenceCell&, NativePedPoolReader::Value&) const noexcept;
        ClockResult Release(NativePedReferenceCell&, Receipt* = nullptr, InvokeUnregister = nullptr) const noexcept;

    private:
        Binding       binding;
        CheckOwner    normal = nullptr, recovery = nullptr;
        std::uint64_t generation = 0;
    };
}
