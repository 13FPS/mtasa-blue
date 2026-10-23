/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Guarded compare/exchange of the native ignored-entity pointer
 ****************************************************************************/
#pragma once
#include "NativePedPoolReader.h"
namespace mta::game_de::NativeWorldIgnoredEntityMemory
{
    using Ped = NativePedPoolReader::Value;
    using CheckOwner = NativePedPoolReader::CheckOwner;
    using CompareExchange = ClockResult (*)(std::uintptr_t, std::uintptr_t, std::uintptr_t, std::uintptr_t&, std::uint32_t&) noexcept;
    struct Receipt
    {
        std::uintptr_t address = 0, expected = 0, desired = 0, before = 0, observed = 0, after = 0;
        std::uint32_t  exceptionCode = 0;
        bool           attempted = false, applied = false, acknowledged = false, foreignReplacement = false;
    };
    // expectedOwnedValue comes only from the runtime's sole journal: initial
    // zero or its own installed value. Nonnull desired is copied and checked
    // as a current native ped, never a raw SDK entity/interface cast. Runtime
    // retains old/new candidate debt whenever an attempted effect lacks ack.
    ClockResult Exchange(std::uintptr_t image, std::uintptr_t expectedOwnedValue, const Ped* desiredOrNull, CheckOwner, Receipt* = nullptr,
                         CompareExchange = nullptr) noexcept;
    // Recovery can only compare the journal's owned nonzero value to null.
    // The matching generation/callback and native GT/GC still apply, while
    // loaded-world/state9 are not required to remove a stale address filter.
    // Foreign nonzero replacement is never overwritten. Stable foreign
    // replacement or already-zero acknowledges that our filter is absent.
    // Two candidates cover an uncertain replace without choosing by a separate
    // unlocked read. At most one matching nonzero candidate is nulled once.
    ClockResult ClearOwned(std::uintptr_t image, std::uintptr_t committedOwnedValue, std::uintptr_t pendingOwnedValue, CheckOwner recovery, Receipt* = nullptr,
                           CompareExchange = nullptr) noexcept;
}
