/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Observe the qualified native full resource-row stream state predicate
 ****************************************************************************/
#pragma once
#include "NativeModelBankReader.h"
namespace mta::game_de
{
    class NativeModelLoadedReader final
    {
    public:
        using Binding = NativeModelBankReader::Binding;
        using CheckOwner = NativeModelBankReader::CheckOwner;
        struct Value
        {
            std::uint32_t  modelIndex = 0;
            std::uintptr_t rowAddress = 0;
            unsigned char  rawFlags = 0, rawLoadState = 0;
            bool           streamedIn = false;
        };
        NativeModelLoadedReader(Binding binding, CheckOwner owner) noexcept : binding(binding), owner(owner) {}
        static bool Qualify(Binding&) noexcept;
        static bool Revalidate(const Binding&) noexcept;
        ClockResult Read(std::uint32_t modelIndex, Value&) const noexcept;
        ClockResult Recheck(const Value&) const noexcept;
        // Raw stream flags/state survive unchanged and streamedIn is exactly
        // state==1. No bank model/UE/RwObject is dereferenced, retained or called.
        // Upgrade-dependent loaded policy, blocking loading and SDK ModelInfo
        // ownership stay with their genuine callers; a successful query is no
        // permission to replace those missing operations with a cached boolean.
    private:
        Binding    binding;
        CheckOwner owner = nullptr;
    };
}
