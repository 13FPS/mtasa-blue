/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Census complete source-qualified native allocation flag banks
 ****************************************************************************/
#pragma once
#include "NativePoolUsageAccess.h"
#include "NativeStreamingCompletionContext.h"

namespace mta::game_de::NativePoolUsageMemory
{
    using Kind = NativePoolUsageAccess::Kind;
    using Value = NativePoolUsageAccess::Value;
    using CheckOwner = NativeStreamingCompletionContext::CheckOwner;
    // The owner and its post-application callback must remain current through
    // the final raw header/flag seal. Failure preserves the caller's value;
    // successful counts expose no native row, pointer or future write lease.
    ClockResult Read(std::uintptr_t image, std::uint64_t generation, Kind, CheckOwner, CheckOwner afterApplication, Value&) noexcept;
}
