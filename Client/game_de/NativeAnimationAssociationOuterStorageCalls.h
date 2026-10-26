/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Typed dynamic outer allocation through the existing genuine heap
 ****************************************************************************/
#pragma once
#include "NativeCustomAnimSequenceStorage.h"
namespace mta::game_de::NativeAnimationAssociationOuterStorageCalls
{
    using CheckOwner = NativeCustomAnimSequenceStorageCalls::CheckOwner;
    using Receipt = NativeCustomAnimSequenceStorageCalls::Receipt;
    using InvokeMalloc = NativeCustomAnimSequenceStorageCalls::InvokeMalloc;
    using InvokeFree = NativeCustomAnimSequenceStorageCalls::InvokeFree;
    ClockResult Allocate(std::uintptr_t, NativeCustomAnimSequenceStorage&, std::uint64_t, CheckOwner, Receipt* = nullptr, InvokeMalloc = nullptr) noexcept;
    ClockResult FreeUnconstructed(std::uintptr_t, NativeCustomAnimSequenceStorage&, std::uint64_t, CheckOwner, Receipt* = nullptr,
                                  InvokeFree = nullptr) noexcept;
    // Read-only qualification may run under the caller's single shared gate.
    // It grants no acknowledgement of a constructor/destructor's effects.
    ClockResult Qualify(std::uintptr_t, const NativeCustomAnimSequenceStorage&, std::uint64_t, CheckOwner, NativeModelReferenceHeap::Value&) noexcept;
}
