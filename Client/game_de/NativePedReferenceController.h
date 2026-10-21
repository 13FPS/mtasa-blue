/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Retain verified native reference cells behind versioned CPU tokens
 ****************************************************************************/
#pragma once
#include "NativePedReferenceMemory.h"
#include "NativePedReferenceToken.h"
#include <array>
#include <atomic>

namespace mta::game_de
{
    class NativePedReferenceController final
    {
    public:
        using Token = NativePedReferenceToken;
        using Memory = NativePedReferenceMemory;
        using Value = NativePedPoolReader::Value;
        using Receipt = Memory::Receipt;
        static constexpr std::size_t Capacity = NativePedPoolProfile::NativeCapacity;
        struct RetireProgress
        {
            std::uint32_t scanned = 0, attempted = 0, released = 0, retained = 0;
        };

        NativePedReferenceController() noexcept = default;
        ~NativePedReferenceController() = default;
        NativePedReferenceController(const NativePedReferenceController&) = delete;
        NativePedReferenceController& operator=(const NativePedReferenceController&) = delete;
        NativePedReferenceController(NativePedReferenceController&&) = delete;
        NativePedReferenceController& operator=(NativePedReferenceController&&) = delete;

        // The caller supplies the generation-bound primitive from its actual
        // admitted callback. A token is only CPU lookup identity; it supplies
        // no native authority, public CPed wrapper, or deferred pointer lease.
        ClockResult Acquire(const Memory&, std::uint64_t generation, std::uint32_t handle, Token&, Receipt* = nullptr,
                            Memory::InvokeRegister = nullptr) noexcept;
        ClockResult Resolve(const Memory&, std::uint64_t generation, const Token&, Value&) noexcept;
        ClockResult Release(const Memory&, std::uint64_t generation, const Token&, Receipt* = nullptr, Memory::InvokeUnregister = nullptr) noexcept;
        ClockResult Retire(const Memory&, std::uint64_t generation, std::uint32_t budget, RetireProgress&, Memory::InvokeUnregister = nullptr) noexcept;

        // Producer threads may observe only these published conservative
        // summaries. The owner must retain this controller and its module
        // until IsRetained is false, including failed unpublished acquisitions.
        // Ordinary native invalidation still permits its issued token's normal
        // Release; RecoveryPending denotes failed acquisition/cleanup or End.
        bool IsRetained() const noexcept { return retained.load(); }
        bool RecoveryPending() const noexcept { return recovery.load(); }

    private:
        struct Slot
        {
            NativePedReferenceCell cell;
            std::uint64_t          version = 0, generation = 0;
            bool                   issued = false, published = false, releaseRequested = false, recoveryNeeded = false;
        };
        ClockResult                Validate(std::uint64_t generation, const Token&, Slot*&, bool releasing = false) noexcept;
        std::uint32_t              Publish() noexcept;
        std::array<Slot, Capacity> slots{};
        std::uint64_t              ownerGeneration = 0;
        std::int32_t               cursor = -1;
        std::uint32_t              retireCursor = 0;
        bool                       stopping = false;
        std::atomic_flag           operation = ATOMIC_FLAG_INIT;
        std::atomic<bool>          retained{false}, recovery{false};
    };
}
