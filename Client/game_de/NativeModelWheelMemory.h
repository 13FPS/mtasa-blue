/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: One current vehicle metadata tuple, never a persistent model lease
 *****************************************************************************/
#pragma once
#include "NativeModelWheelAccess.h"
#include "NativeModelBankReader.h"
#include "NativeStreamingModelContext.h"
#include <array>

namespace mta::game_de::NativeModelWheelMemory
{
    using CheckOwner = NativeModelBankReader::CheckOwner;
    struct Value
    {
        std::uint64_t                      generation = 0;
        NativeModelBankReader::Value       model;
        NativeStreamingModelContext::Value context;
        std::array<unsigned char, 6>       typeBytes{};
        std::array<std::uint32_t, 2>       bits{};
        std::int32_t                       poolCount = 0;
        bool                               vehicle = false;
    };
    struct Receipt
    {
        Value                        before;
        std::array<std::uint32_t, 2> desired{}, observed{};
        std::uint32_t                exceptionCode = 0;
        ClockResult                  result = ClockResult::NotInitialized;
        bool                         entered = false, accepted = false, uncertain = false;
    };
    // Plain identities may be compared with a freshly admitted callback;
    // they contain no membership-reader tokens and cannot renew a GC lease.
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
    using Exchange = bool (*)(std::uintptr_t, std::uint64_t, std::uint64_t, std::uint64_t&, std::uint32_t&) noexcept;
    bool        SameIdentity(const Value&, const Value&) noexcept;
    Origin      Identity(const Value&) noexcept;
    bool        SameIdentity(const Origin&, const Value&) noexcept;
    ClockResult Capture(std::uintptr_t image, std::uint64_t generation, std::uint32_t model, CheckOwner, CheckOwner afterApplication, Value&) noexcept;
    ClockResult Write(std::uintptr_t image, const Value&, const std::array<std::uint32_t, 2>& desired, CheckOwner, CheckOwner afterApplication, Receipt&,
                      Exchange = nullptr) noexcept;
}
