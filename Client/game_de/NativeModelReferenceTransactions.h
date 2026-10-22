/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Acquire one genuine native model reference without allocator clones
 ****************************************************************************/
#pragma once
#include "NativeModelReferenceMemory.h"
#include <cstddef>
namespace mta::game_de::NativeModelReferenceTransactions
{
    using CheckOwner = NativeModelReferenceMemory::CheckOwner;
    struct ReservedRow
    {
        std::int32_t   index = -1;
        std::uint32_t  padding = 0;
        std::uintptr_t row = 0;
    };
    static_assert(sizeof(ReservedRow) == 16 && offsetof(ReservedRow, row) == 8);
    using ReserveFunction = ReservedRow* (*)(std::uintptr_t, ReservedRow*);
    using PublishFunction = std::int32_t* (*)(std::uintptr_t, std::int32_t*, std::int32_t, std::uintptr_t, std::int32_t);
    struct Calls
    {
        ReserveFunction reserve = nullptr;
        PublishFunction publish = nullptr;
    };
    struct Receipt
    {
        std::uintptr_t root = 0, map = 0, row = 0, heap = 0;
        std::uint32_t  modelIndex = 0, before = 0, target = 0, observed = 0, exceptionCode = 0;
        std::int32_t   reservedIndex = -1, publishedIndex = -1;
        bool           entered = false, reserved = false, published = false, accepted = false;
    };
    // An existing key uses the real current count adjustment. An absent key
    // uses the native reserve/init/publish protocol inside one admitted call.
    // No owner acknowledgement interrupts an unpublished reservation. Native
    // effects survive failures; entered failures require Stop without replay.
    // This internal operation does not publish a CModelInfo or lifetime lease.
    ClockResult Acquire(std::uintptr_t image, std::uint32_t modelIndex, CheckOwner, Receipt* = nullptr, const Calls* = nullptr) noexcept;
}
