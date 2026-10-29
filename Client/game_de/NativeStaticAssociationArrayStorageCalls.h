/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Causally issued fresh static32 arrays, never raw-pointer adoption
 ****************************************************************************/
#pragma once
#include "NativeCustomAnimSequenceStorage.h"
#include <array>
namespace mta::game_de::NativeStaticAssociationArrayStorageCalls
{
    using Storage = NativeCustomAnimSequenceStorage;
    using CheckOwner = NativeCustomAnimSequenceStorageCalls::CheckOwner;
    using Receipt = NativeCustomAnimSequenceStorageCalls::Receipt;
    using InvokeMalloc = NativeCustomAnimSequenceStorageCalls::InvokeMalloc;
    using InvokeFree = NativeCustomAnimSequenceStorageCalls::InvokeFree;
    using DeletingDestructorFunction = std::uintptr_t (*)(std::uintptr_t, std::uint32_t);
    using InvokeDeletingDestructor = ClockResult (*)(DeletingDestructorFunction, std::uintptr_t, std::uint32_t, std::uintptr_t&, std::uint32_t&) noexcept;
    struct FreshRow
    {
        std::uint32_t                 index = 0;
        std::uintptr_t                locator = 0;
        std::array<unsigned char, 32> bytes{};
    };
    // This read-only shared qualification may run under the caller's native
    // gate. It grants no old-child or arbitrary raw-array ownership.
    ClockResult CaptureRetirementHeap(std::uintptr_t, CheckOwner, NativeModelReferenceHeap::Value&) noexcept;
    ClockResult AllocateFresh(std::uintptr_t, Storage&, std::uint64_t, std::uint32_t, CheckOwner, Receipt* = nullptr, InvokeMalloc = nullptr) noexcept;
    ClockResult ReadFreshRow(std::uintptr_t, const Storage&, std::uint64_t, std::uint32_t, CheckOwner, FreshRow&) noexcept;
    ClockResult RetireFresh(std::uintptr_t, Storage&, std::uint64_t, CheckOwner, Receipt* = nullptr, InvokeDeletingDestructor = nullptr,
                            InvokeFree = nullptr) noexcept;
    // Observations are not initialization or publication tickets. Only the
    // private friends validate actual issued storage for those later phases.
}
