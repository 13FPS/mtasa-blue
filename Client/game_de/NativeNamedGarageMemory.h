/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: First-match named garage type and MTA door-byte intent under the native owner
 ****************************************************************************/
#pragma once
#include "NativeNamedGarageAccess.h"
namespace mta::game_de
{
    class NativeNamedGarageMemory final
    {
    public:
        using Binding = NativePedPoolReader::Binding;
        using CheckOwner = NativePedPoolReader::CheckOwner;
        struct Receipt
        {
            std::uint32_t  count = 0, index = 0, exceptionCode = 0;
            std::uintptr_t row = 0, address = 0;
            std::uint8_t   original = 0, target = 0, observed = 0;
            bool           entered = false, accepted = false, observedValid = false, verified = false;
        };
        NativeNamedGarageMemory(Binding binding, CheckOwner owner) noexcept : binding(binding), owner(owner) {}
        static bool Qualify(Binding&) noexcept;
        static bool Revalidate(const Binding&) noexcept;
        static bool QualifyGeometry(Binding&) noexcept;
        static bool RevalidateGeometry(const Binding&) noexcept;
        ClockResult ReadGeometry(const NativeGarageName&, NativeGarageGeometry&) const noexcept;
        ClockResult ReadType(const NativeGarageName&, std::uint8_t&) const noexcept;
        ClockResult SetType(const NativeGarageName&, std::uint8_t, Receipt* = nullptr) const noexcept;
        ClockResult ReadOpen(const NativeGarageName&, bool&) const noexcept;
        ClockResult SetOpen(const NativeGarageName&, bool, Receipt* = nullptr) const noexcept;
        // Names must terminate within eight bytes. Every call reacquires the
        // first native casefolded match in the currently published count. The
        // full DWORD count must agree with the nonnegative signed-WORD lookup
        // domain; this does not invent a bank capacity or classic SDK ID map.
        // No row address escapes or survives the borrowed owner callback.
        // Only one selected byte may be conditionally replaced. Any entered
        // failure must Stop at the retained owner; accepted effects persist,
        // with no retry, replay or rollback. Geometry is read-only coherent
        // raw binary32 storage; default16 startup garage selection remains
        // separately unqualified.
    private:
        ClockResult Read(const NativeGarageName&, bool door, std::uint8_t&) const noexcept;
        ClockResult Set(const NativeGarageName&, bool door, std::uint8_t, Receipt*) const noexcept;
        Binding     binding;
        CheckOwner  owner = nullptr;
    };
}
