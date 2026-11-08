/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Borrow only genuine resource-empty pre-backup cleanup receivers
 ****************************************************************************/
#pragma once
#include "NativeWorldBoundRectMemory.h"
#include "NativeWorldBackupCleanupProfile.h"
namespace mta::game_de::NativeWorldBackupCleanupMemory
{
    using Kind = NativeWorldBoundRectMemory::Kind;
    using CheckOwner = NativeWorldBoundRectMemory::CheckOwner;
    struct Value
    {
        // Reuse existing allocated normalAtomic/typed entity/matrix admission.
        // No geometry original is needed or invoked by cleanup.
        NativeWorldBoundRectMemory::Value inputs;
        std::vector<unsigned char>        manager;
    };
    bool        Profile(std::uintptr_t image) noexcept;
    ClockResult CaptureEmpty(std::uintptr_t, std::uint64_t, Kind, std::uint32_t, CheckOwner, CheckOwner, Value&);
    bool        SealEmpty(std::uintptr_t, const Value&) noexcept;
}
