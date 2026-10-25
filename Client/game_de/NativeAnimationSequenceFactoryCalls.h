/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Original issued array extent without interpreting any child row
 ****************************************************************************/
#pragma once
#include "NativeCustomAnimSequenceStorage.h"
namespace mta::game_de::NativeAnimationSequenceFactoryCalls
{
    using Storage = NativeCustomAnimSequenceStorage;
    using CheckOwner = NativeCustomAnimSequenceStorageCalls::CheckOwner;
    // This qualifies only the original allocation, cookie and current heap.
    // It reads no name/flags/frame/count cell from row0 or any other child.
    ClockResult ReadCapacity(std::uintptr_t, const Storage&, std::uint64_t, CheckOwner, std::size_t&) noexcept;
}
