/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Existing runtime's durable acquire-only model reference journal
 *****************************************************************************/
#pragma once
#include "NativeModelBackingReferenceAccess.h"
#include "NativeModelReferenceTransactions.h"
#include <atomic>
#include <list>

namespace mta::game_de
{
    class NativeModelBackingReferenceController final
    {
    public:
        using Token = NativeModelBackingReferenceToken;
        using State = NativeModelBackingReferenceState;
        using Phase = NativeModelBackingReferencePhase;
        using CheckOwner = NativeModelReferenceTransactions::CheckOwner;
        NativeModelBackingReferenceController() noexcept = default;
        NativeModelBackingReferenceController(const NativeModelBackingReferenceController&) = delete;
        NativeModelBackingReferenceController& operator=(const NativeModelBackingReferenceController&) = delete;
        ClockResult                            Reserve(std::uint64_t generation, std::uint32_t model, CheckOwner, Token&) noexcept;
        ClockResult                            Bind(std::uint64_t generation, const Token&, CheckOwner) noexcept;
        ClockResult                            ClaimUnmodifiedFamily(std::uint64_t generation, const Token&, CheckOwner) noexcept;
        ClockResult                            RenewCycle(std::uint64_t generation, const Token&, CheckOwner, Token&) noexcept;
        ClockResult                            Acquire(std::uintptr_t image, std::uint64_t generation, const Token&, CheckOwner,
                                                       const NativeModelReferenceTransactions::Calls* = nullptr) noexcept;
        ClockResult Release(std::uintptr_t image, std::uint64_t generation, const Token&, NativeModelReferenceRelease::ZeroPolicy, CheckOwner,
                            NativeModelReferenceRelease::Receipt* = nullptr, NativeModelReferenceMemory::Exchange = nullptr,
                            NativeStreamingRemoveCalls::Invoke = nullptr) noexcept;
        ClockResult Read(std::uint64_t generation, const Token&, CheckOwner, State&) noexcept;
        // End may erase no-effect CPU records and acknowledged Released
        // records only. Held/unknown references never receive automatic release.
        ClockResult RevokeUnacquired(std::uint64_t generation) noexcept;
        bool        IsRetained() const noexcept { return busy.load() || retained.load(); }
        bool        HasPending() const noexcept { return busy.load() || pending.load(); }
        bool        RecoveryPending() const noexcept { return recovery.load(); }

    private:
        std::list<State>  records;
        std::uint64_t     serial = 0;
        std::atomic<bool> busy{false}, retained{false}, pending{false}, recovery{false};
        State*            Find(std::uint64_t generation, const Token&) noexcept;
        void              Publish() noexcept;
    };
}
