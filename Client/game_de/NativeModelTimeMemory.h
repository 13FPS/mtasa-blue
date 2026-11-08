/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Observe the real timed-model tuple and write only its hour bytes
 ****************************************************************************/
#pragma once
#include "NativeModelBankReader.h"
#include "NativeStreamingModelContext.h"
#include <array>

namespace mta::game_de::NativeModelTimeMemory
{
    using CheckOwner = NativeModelBankReader::CheckOwner;
    struct Value
    {
        std::uint64_t                      generation = 0;
        NativeModelBankReader::Value       model;
        NativeStreamingModelContext::Value context;
        std::array<unsigned char, 6>       typeBytes{};
        std::int32_t                       poolCount = 0;
        std::array<unsigned char, 4>       tuple{};
        bool                               timed = false;
    };
    struct Receipt
    {
        Value         before;
        std::uint16_t desired = 0, observed = 0;
        std::uint32_t exceptionCode = 0;
        ClockResult   result = ClockResult::NotInitialized;
        bool          entered = false, accepted = false, uncertain = false;
    };
    // Retained defaults keep plain identities only. These copied fields do
    // not renew a native bank, UObject or static metadata borrow next frame.
    struct Member
    {
        std::uintptr_t object = 0, vtable = 0, klass = 0, outer = 0, item = 0;
        std::int32_t   index = -1;
        std::uint32_t  serial = 0;
    };
    struct Origin
    {
        std::uint64_t                generation = 0;
        std::uint32_t                model = 0;
        std::uintptr_t               slot = 0, metadata = 0, vtable = 0, typeMethod = 0, animationMethod = 0;
        std::uintptr_t               root = 0, provider = 0, rows = 0;
        std::int32_t                 count = 0, poolCount = 0;
        Member                       rootMembership, providerMembership;
        std::array<unsigned char, 6> typeBytes{};
        std::uint16_t                otherModel = 0;
    };
    using Exchange = bool (*)(std::uintptr_t, std::uint16_t, std::uint16_t, std::uint16_t&, std::uint32_t&) noexcept;
    bool   SameIdentity(const Value&, const Value&) noexcept;
    Origin Identity(const Value&) noexcept;
    bool   SameIdentity(const Origin&, const Value&) noexcept;
    // A second model-bank key may alias this exact static timed-model row.
    // Ignore only the key and bank slot; the storage and owner identities,
    // allocated prefix and day/night pairing still have to agree in full.
    bool        SameStorageIdentity(const Origin&, const Value&) noexcept;
    ClockResult Capture(std::uintptr_t image, std::uint64_t generation, std::uint32_t model, CheckOwner, CheckOwner afterApplication, Value&) noexcept;
    ClockResult Write(std::uintptr_t image, const Value&, std::uint16_t desiredHours, CheckOwner, CheckOwner afterApplication, Receipt&,
                      Exchange = nullptr) noexcept;
}
