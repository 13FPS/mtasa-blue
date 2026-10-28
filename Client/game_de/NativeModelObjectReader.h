/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Immediate opaque model interface and render observations
 ****************************************************************************/
#pragma once
#include "NativeModelBankReader.h"
namespace mta::game_de
{
    class NativeModelObjectReader final
    {
    public:
        using Binding = NativeModelBankReader::Binding;
        using CheckOwner = NativeModelBankReader::CheckOwner;
        NativeModelObjectReader(Binding binding, CheckOwner owner) noexcept : binding(binding), owner(owner) {}
        // Interface observation has exactly the existing opaque bank domain;
        // render's separate field proof must not constrain that old authority.
        static bool Qualify(Binding&) noexcept;
        static bool Revalidate(const Binding&) noexcept;
        static bool RevalidateRender(const Binding&) noexcept;
        static bool RevalidatePlayerIdentity(const Binding&) noexcept;
        ClockResult ReadInterface(std::uint32_t fullModelKey, std::uintptr_t&) const noexcept;
        ClockResult ReadRender(std::uint32_t fullModelKey, std::uintptr_t&) const noexcept;
        // Preserve MTA's collision-pointer identity predicate. No type,
        // loaded state or default-geometry validity substitutes for equality.
        ClockResult ReadPlayerModel(std::uint32_t fullModelKey, bool&) const noexcept;
        // Returned tags, including null and poison values, grant no target
        // dereference, persistent lease, reference/free or factory authority.
    private:
        Binding    binding;
        CheckOwner owner = nullptr;
    };
}
