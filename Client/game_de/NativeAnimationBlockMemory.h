/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Checked current native block metadata without a retained block cell
 ****************************************************************************/
#pragma once
#include "NativeAnimationBlockAccess.h"
namespace mta::game_de
{
    class NativeAnimationBlockMemory final
    {
    public:
        using Binding = NativePedPoolReader::Binding;
        using CheckOwner = NativePedPoolReader::CheckOwner;
        NativeAnimationBlockMemory(Binding binding, CheckOwner owner) noexcept : binding(binding), owner(owner) {}
        static bool Qualify(Binding&) noexcept;
        static bool Revalidate(const Binding&) noexcept;
        ClockResult Read(const NativeAnimationBlockIdentity&, NativeAnimationBlockValues&) const noexcept;
        // Returning an immediate address does not freeze native name contents or
        // callback lifetime. The runtime also checks CPU generation at commit.
        ClockResult ReadName(const NativeAnimationBlockIdentity&, char*&) const noexcept;

    private:
        Binding binding;
        // The enclosing runtime also owns CPU generation admission. A matching
        // index/name is only an immediate native observation, never authority.
        CheckOwner owner = nullptr;
    };
}
