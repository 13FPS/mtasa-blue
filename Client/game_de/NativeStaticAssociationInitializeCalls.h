/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: One genuine initializer over a current registered clump and static row
 ****************************************************************************/
#pragma once
#include "NativePedPoolReader.h"
namespace mta::game_de::NativeStaticAssociationInitializeCalls
{
    using CheckOwner = NativePedPoolReader::CheckOwner;
    struct Arguments
    {
        NativePedPoolReader::Value ped;
        std::uintptr_t             clump = 0, association = 0, hierarchy = 0;
    };
    // The runtime owns physical row/bank registration and the retained command.
    // Recheck twice before entry and once after return, without recursively
    // entering this call's shared native gate. A raw pointer is not admission.
    using RecheckArguments = ClockResult (*)(const Arguments&, bool after, void* context) noexcept;
    using NativeFunction = void (*)(std::uintptr_t association, std::uintptr_t clump, std::uintptr_t hierarchy);
    using Invoke = ClockResult (*)(NativeFunction, std::uintptr_t association, std::uintptr_t clump, std::uintptr_t hierarchy,
                                   std::uint32_t& exception) noexcept;
    struct Receipt
    {
        std::uintptr_t entryRva = 0, nativePed = 0, clump = 0, container = 0, association = 0, hierarchy = 0, heap = 0;
        std::uintptr_t beforeNodes = 0, observedNodes = 0;
        std::uint16_t  frameCount = 0, observedCount = 0;
        std::uint32_t  exceptionCode = 0;
        bool           entered = false, returned = false, rowObserved = false, verified = false;
    };
    // Only nodes==0 is the fresh receiver condition. FEB3F0 leaves its prior
    // WORD+8 undefined; the genuine nonnull-clump entry defines that count.
    // On entered failure the owner keeps the receipt and native debt. This
    // primitive never retries, restores a row, releases nodes or frees a facade.
    ClockResult Initialize(std::uintptr_t image, const Arguments&, CheckOwner, RecheckArguments, void* context, Receipt* = nullptr, Invoke = nullptr) noexcept;
}
