/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Observe one current allocated metadata object's unsigned group word
 ****************************************************************************/
#pragma once
#include "NativeModelObjectGroupAccess.h"
#include "NativeModelBankReader.h"
#include "NativeStreamingModelContext.h"
#include <array>

namespace mta::game_de::NativeModelObjectGroupMemory
{
    using CheckOwner = NativeModelBankReader::CheckOwner;
    struct Value
    {
        std::uint64_t                      generation = 0;
        NativeModelBankReader::Value       model;
        NativeStreamingModelContext::Value context;
        std::array<unsigned char, 6>       typeBytes{};
        std::int32_t                       poolCount = 0;
        std::uint16_t                      group = 0;
        bool                               present = false;
    };
    struct Receipt
    {
        Value         before;
        std::uint16_t desired = 0, observed = 0;
        std::uint32_t exceptionCode = 0;
        ClockResult   result = ClockResult::NotInitialized;
        bool          entered = false, accepted = false, uncertain = false;
    };
    // A cache entry preserves plain identities and the first MTA default,
    // never a borrowed UObject/metadata lifetime ticket across callbacks.
    struct Origin
    {
        std::uint64_t                generation = 0;
        std::uint32_t                model = 0;
        std::uintptr_t               slot = 0, metadata = 0, vtable = 0, typeMethod = 0;
        std::uintptr_t               root = 0, provider = 0, rows = 0;
        std::int32_t                 count = 0, rootIndex = -1, providerIndex = -1;
        std::uint32_t                rootSerial = 0, providerSerial = 0;
        std::array<unsigned char, 6> typeBytes{};
    };
    using Exchange = bool (*)(std::uintptr_t, std::uint16_t, std::uint16_t, std::uint16_t&, std::uint32_t&) noexcept;
    bool        SameIdentity(const Value&, const Value&) noexcept;
    Origin      Identity(const Value&) noexcept;
    bool        SameIdentity(const Origin&, const Value&) noexcept;
    ClockResult Capture(std::uintptr_t image, std::uint64_t generation, std::uint32_t model, CheckOwner, CheckOwner afterApplication, Value&) noexcept;
    ClockResult Write(std::uintptr_t image, const Value&, std::uint16_t desired, CheckOwner, CheckOwner afterApplication, Receipt&,
                      Exchange = nullptr) noexcept;
}
