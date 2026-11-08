/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Fresh read-only dynamic-index predicate on constructor-backed rows.
 *****************************************************************************/
#pragma once
#include "NativeModelBankReader.h"

namespace mta::game_de::NativeModelDynamicMemory
{
    using CheckOwner = NativeModelBankReader::CheckOwner;
    // A successful absent bank association is false, as in the existing SDK.
    // Unsupported present metadata refuses rather than becoming a static model.
    // Neither the boolean nor generation argument grants a later storage lease.
    ClockResult Read(std::uintptr_t image, std::uint64_t generation, std::uint32_t model, CheckOwner, CheckOwner afterApplication, bool& output) noexcept;
}
