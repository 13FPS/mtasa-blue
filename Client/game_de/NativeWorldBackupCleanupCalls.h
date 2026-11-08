/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Keep a genuine cleanup prefix separate from whole MTA backup readiness
 ****************************************************************************/
#pragma once
#include "NativeWorldBackupCleanupMemory.h"
namespace mta::game_de::NativeWorldBackupCleanupCalls
{
    using Kind = NativeWorldBackupCleanupMemory::Kind;
    using CheckOwner = NativeWorldBackupCleanupMemory::CheckOwner;
    enum class Operation : unsigned char
    {
        DeleteRwObject,
        ResolveReferences
    };
    using Entry = void (*)(void*);
    using Invoker = ClockResult (*)(Operation, Entry, std::uintptr_t, std::uint32_t&) noexcept;
    struct Stage
    {
        bool          entered = false, returned = false, acknowledged = false;
        std::uint32_t exception = 0;
        ClockResult   result = ClockResult::NotInitialized;
    };
    struct Receipt
    {
        std::uintptr_t       entity = 0;
        std::array<Stage, 2> stages;
        unsigned             completed = 0;
        ClockResult          result = ClockResult::NotInitialized;
        bool                 uncertain = false;
    };
    // Exact native prefix only: classic MTA still needs separate per-owner
    // stencil cleanup before this can authorize pool release or row capture.
    ClockResult RunEmptyPrefix(std::uintptr_t, std::uint64_t, Kind, std::uint32_t, CheckOwner, CheckOwner, Receipt* = nullptr, Invoker = nullptr) noexcept;
}
