/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Unique descriptor selection under the existing owner and garage byte protocol
 ****************************************************************************/
#pragma once
#include "NativeNumericGarageAccess.h"
#include "NativeNamedGarageMemory.h"

namespace mta::game_de
{
    class NativeNumericGarageMemory final
    {
    public:
        using Binding = NativeNamedGarageMemory::Binding;
        using CheckOwner = NativeNamedGarageMemory::CheckOwner;
        using Receipt = NativeNamedGarageMemory::Receipt;
        NativeNumericGarageMemory(Binding binding, CheckOwner owner) noexcept : binding(binding), owner(owner) {}
        ClockResult CheckReady() const noexcept;
        ClockResult ReadGeometry(std::uint32_t, NativeGarageGeometry&) const noexcept;
        ClockResult ReadType(std::uint32_t, std::uint8_t&) const noexcept;
        ClockResult SetType(std::uint32_t, std::uint8_t, Receipt* = nullptr) const noexcept;
        ClockResult ReadOpen(std::uint32_t, bool&) const noexcept;
        ClockResult SetOpen(std::uint32_t, bool, Receipt* = nullptr) const noexcept;

    private:
        ClockResult Read(std::uint32_t, NativeGarageGeometry*, std::uint8_t*, bool) const noexcept;
        ClockResult Set(std::uint32_t, bool, std::uint8_t, Receipt*) const noexcept;
        Binding     binding;
        CheckOwner  owner = nullptr;
    };
}
