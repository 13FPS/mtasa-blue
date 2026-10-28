/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: One existing named/numeric owner and byte-exchange implementation boundary
 ****************************************************************************/
#pragma once
#include "NativeNamedGarageMemory.h"
#include "NativeNamedGarageProfile.h"
#include "NativeImageAccess.h"

namespace mta::game_de::NamedGarageDetail
{
    namespace P = NativeNamedGarageProfile;
    namespace I = NativeImageAccess;
    using Memory = NativeNamedGarageMemory;
    struct Value
    {
        std::uint32_t    count = 0, index = 0;
        std::uintptr_t   row = 0;
        NativeGarageName name{};
        std::uint8_t     type = 0, state = 0;
    };
    // Share exactly one CPU gate so nested calls cannot write the same native
    // byte through a different SDK identity route during an entered exchange.
    extern thread_local bool writing;
    struct Scope
    {
        Scope() noexcept { writing = true; }
        ~Scope() { writing = false; }
    };
    bool        ValidName(const NativeGarageName&) noexcept;
    ClockResult Owner(Memory::Binding, Memory::CheckOwner) noexcept;
    bool        SameIdentity(const Value&, const Value&) noexcept;
#if !defined(MTA_DE_NAMED_GARAGE_EXCHANGE)
    bool Exchange(std::uintptr_t, std::uint8_t, std::uint8_t, std::uint8_t&, std::uint32_t&) noexcept;
#endif
}
