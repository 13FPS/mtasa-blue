/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Current typed dummy pool observations and retained-hold vector stores
 *****************************************************************************/
#pragma once
#include "NativeModelDummiesAccess.h"
#include "NativeModelWheelMemory.h"
#include "NativeModelLoadedReader.h"
#include "NativeModelBackingReferenceController.h"

namespace mta::game_de::NativeModelDummiesMemory
{
    using CheckOwner = NativeModelWheelMemory::CheckOwner;
    using VectorBits = std::array<std::uint32_t, 3>;
    using PositionsBits = std::array<VectorBits, 14>;
    struct Value
    {
        NativeModelWheelMemory::Value   model;
        NativeModelLoadedReader::Value  loaded;
        std::uintptr_t                  pool = 0, data = 0, flags = 0, visual = 0;
        std::array<unsigned char, 0x19> header{};
        PositionsBits                   bits{};
        std::uint32_t                   index = 0;
        unsigned char                   flag = 0;
        bool                            present = false;
    };
    struct Origin
    {
        NativeModelWheelMemory::Origin model;
        std::uintptr_t                 pool = 0, data = 0, flags = 0, visual = 0;
        std::uint32_t                  index = 0;
        unsigned char                  flag = 0;
    };
    struct Receipt
    {
        Origin        origin;
        VectorBits    before{}, desired{}, observed{};
        std::uint32_t dummy = 0, exceptionCode = 0;
        unsigned char entered = 0, accepted = 0;
        bool          uncertain = false;
        ClockResult   result = ClockResult::NotInitialized;
    };
    Origin      Identity(const Value&) noexcept;
    bool        SameIdentity(const Origin&, const Value&) noexcept;
    ClockResult Capture(std::uintptr_t image, std::uint64_t generation, std::uint32_t model, CheckOwner, CheckOwner afterApplication, Value&) noexcept;
    // A private genuine controller/token pair is required at the store entry.
    // A caller-created receipt or a captured row alone cannot grant a hold.
    ClockResult            Write(std::uintptr_t image, const Value&, std::uint32_t               dummy, const VectorBits&, NativeModelBackingReferenceController&,
                                 const NativeModelBackingReferenceToken&, CheckOwner, CheckOwner afterApplication, Receipt&) noexcept;
    NativeModelDummyVector Decode(const VectorBits&) noexcept;
    VectorBits             Encode(const NativeModelDummyVector&) noexcept;
}
