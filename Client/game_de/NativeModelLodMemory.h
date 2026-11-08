/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Current typed model draw-distance bits and one exact DWORD store
 ****************************************************************************/
#pragma once
#include "NativeModelBankReader.h"
#include "NativeStreamingModelContext.h"
#include <array>
namespace mta::game_de::NativeModelLodMemory
{
    using CheckOwner = NativeModelBankReader::CheckOwner;
    inline constexpr std::size_t FieldOffset = 0x38;
    enum class Family : unsigned char
    {
        Absent,
        Atomic,
        Time,
        Vehicle
    };
    struct Value
    {
        std::uint64_t                      generation = 0;
        NativeModelBankReader::Value       model;
        NativeStreamingModelContext::Value context;
        std::array<unsigned char, 6>       typeBytes{};
        std::int32_t                       poolCount = 0;
        std::uint32_t                      bits = 0;
        Family                             family = Family::Absent;
        bool                               present = false;
    };
    struct Member
    {
        std::uintptr_t object = 0, vtable = 0, klass = 0, outer = 0, item = 0;
        std::int32_t   index = -1;
        std::uint32_t  serial = 0;
    };
    // Only plain owner/storage identities survive in a default journal. These
    // values contain no membership-reader token or permission to dereference a
    // previous callback's bank, metadata, root or provider.
    struct Origin
    {
        std::uint64_t                generation = 0;
        std::uint32_t                model = 0;
        std::uintptr_t               slot = 0, metadata = 0, vtable = 0, typeMethod = 0, animationMethod = 0;
        std::uintptr_t               root = 0, provider = 0, rows = 0;
        std::int32_t                 count = 0, poolCount = 0;
        Member                       rootMembership, providerMembership;
        std::array<unsigned char, 6> typeBytes{};
        Family                       family = Family::Absent;
    };
    struct Receipt
    {
        Value         before;
        std::uint32_t desired = 0, observed = 0, exceptionCode = 0;
        ClockResult   result = ClockResult::NotInitialized;
        bool          entered = false, accepted = false, uncertain = false;
    };
    using Exchange = bool (*)(std::uintptr_t, std::uint32_t, std::uint32_t, std::uint32_t&, std::uint32_t&) noexcept;
    Origin Identity(const Value&) noexcept;
    bool   SameIdentity(const Value&, const Value&) noexcept;
    bool   SameIdentity(const Origin&, const Value&) noexcept;
    // Model-bank aliases may name the same storage. Only their key and slot
    // are ignored; all native family and current-owner identities must agree.
    bool        SameStorageIdentity(const Origin&, const Value&) noexcept;
    ClockResult Capture(std::uintptr_t image, std::uint64_t generation, std::uint32_t model, CheckOwner, CheckOwner afterApplication, Value&) noexcept;
    ClockResult Write(std::uintptr_t image, const Value&, std::uint32_t desiredBits, CheckOwner, CheckOwner afterApplication, Receipt&,
                      Exchange = nullptr) noexcept;
}
