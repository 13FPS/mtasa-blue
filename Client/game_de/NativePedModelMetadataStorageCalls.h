/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Fixed native metadata storage, without ped initialization or publication
 ****************************************************************************/
#pragma once
#include "NativeCustomAnimSequenceStorage.h"

namespace mta::game_de::NativePedModelMetadataStorageCalls
{
    using Storage = NativeCustomAnimSequenceStorage;
    using CheckOwner = NativeCustomAnimSequenceStorageCalls::CheckOwner;
    using Receipt = NativeCustomAnimSequenceStorageCalls::Receipt;
    using InvokeMalloc = NativeCustomAnimSequenceStorageCalls::InvokeMalloc;
    using InvokeFree = NativeCustomAnimSequenceStorageCalls::InvokeFree;
    inline constexpr std::size_t AllocationBytes = 0x98;

    // This typed heap allocation has no array cookie, initialized model,
    // constructor acknowledgement or bank-publication authority. The caller
    // must retain its record before native entry and after any entered failure.
    ClockResult Allocate(std::uintptr_t image, Storage&, std::uint64_t generation, CheckOwner, Receipt* = nullptr, InvokeMalloc = nullptr) noexcept;
    // Only original issued storage of this exact kind can be released. A
    // future construction owner must separately gate constructor/publication
    // effects; no raw pointer, destructor or object-state adoption occurs here.
    ClockResult FreeStorage(std::uintptr_t image, Storage&, std::uint64_t generation, CheckOwner, Receipt* = nullptr, InvokeFree = nullptr) noexcept;
}
