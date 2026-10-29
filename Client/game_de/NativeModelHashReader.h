/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: First ascending model hash match without name-cursor side effects
 ****************************************************************************/
#pragma once
#include "NativeModelBankReader.h"
namespace mta::game_de
{
    class NativeModelHashReader final
    {
    public:
        using Binding = NativeModelBankReader::Binding;
        using CheckOwner = NativeModelBankReader::CheckOwner;
        struct Value
        {
            bool                         found = false;
            NativeModelBankReader::Value model{};
            std::uint32_t                hash = 0;
        };
        NativeModelHashReader(Binding binding, CheckOwner owner) noexcept : binding(binding), owner(owner) {}
        static bool Qualify(Binding&) noexcept;
        static bool Revalidate(const Binding&) noexcept;
        ClockResult FindFirstByHash(std::uint32_t hash, Value&) const noexcept;
        // Value is an immediate bank tuple and one observed DWORD, never a
        // model-object lease, loaded/type predicate or allocation authority.
        // Only an admitted complete prefix can establish the first match;
        // a complete miss requires every one of the real 20000 bank slots.
    private:
        Binding    binding;
        CheckOwner owner = nullptr;
    };
}
