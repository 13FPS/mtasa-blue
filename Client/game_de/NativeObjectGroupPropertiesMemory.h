/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Fresh world/image admission for the supported builtin scalar rows
 ****************************************************************************/
#pragma once
#include "NativeObjectGroupPropertiesAccess.h"
#include "NativeStreamingCompletionContext.h"
#include <array>

namespace mta::game_de::NativeObjectGroupPropertiesMemory
{
    using CheckOwner = NativeStreamingCompletionContext::CheckOwner;
    using Bits = std::array<std::uint32_t, 5>;
    struct Value
    {
        std::uintptr_t                          image = 0;
        std::uint64_t                           generation = 0;
        std::uint8_t                            group = 0;
        NativeStreamingCompletionContext::Value world;
        Bits                                    bits{};
    };
    struct Member
    {
        std::uintptr_t object = 0, vtable = 0, klass = 0, outer = 0, item = 0;
        std::int32_t   index = -1;
        std::uint32_t  serial = 0;
    };
    struct Origin
    {
        std::uintptr_t         image = 0, root = 0, gameMode = 0, level = 0, world = 0, gameClass = 0, levelClass = 0;
        std::uint64_t          generation = 0;
        std::uint8_t           group = 0, mode = 0;
        std::array<Member, 64> members{};
        std::size_t            count = 0;
    };
    struct Receipt
    {
        // Only plain forensic identities survive an entered uncertainty. No
        // captured membership-reader tokens become a cross-callback GC lease.
        Origin                    before;
        NativeObjectGroupProperty field = NativeObjectGroupProperty::Count;
        std::uint32_t             desired = 0, observed = 0, exceptionCode = 0;
        ClockResult               result = ClockResult::NotInitialized;
        bool                      entered = false, accepted = false, uncertain = false;
    };
    using Exchange = bool (*)(std::uintptr_t, std::uint32_t, std::uint32_t, std::uint32_t&, std::uint32_t&) noexcept;
    Origin      Identity(const Value&) noexcept;
    bool        SameIdentity(const Origin&, const Value&) noexcept;
    bool        SameIdentity(const Value&, const Value&) noexcept;
    ClockResult Capture(std::uintptr_t image, std::uint64_t generation, std::uint8_t group, CheckOwner, CheckOwner afterApplication, Value&) noexcept;
    ClockResult Write(std::uintptr_t image, const Value&, NativeObjectGroupProperty, std::uint32_t desired, CheckOwner, CheckOwner afterApplication, Receipt&,
                      Exchange = nullptr) noexcept;
}
