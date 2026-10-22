/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Observe actual bounded model bank slots without model lifetime claims
 ****************************************************************************/
#pragma once
#include "NativePedPoolReader.h"

namespace mta::game_de
{
    class NativeModelBankReader final
    {
    public:
        using Binding = NativePedPoolReader::Binding;
        using CheckOwner = NativePedPoolReader::CheckOwner;
        struct Value
        {
            std::uint32_t  modelIndex = 0;
            std::uintptr_t slotAddress = 0, nativeModelInfo = 0;
        };
        NativeModelBankReader(Binding binding, CheckOwner owner) noexcept : binding(binding), owner(owner) {}
        static bool Qualify(Binding&) noexcept;
        static bool Revalidate(const Binding&) noexcept;
        ClockResult Read(std::uint32_t modelIndex, Value&) const noexcept;
        ClockResult Recheck(const Value&) const noexcept;

        // A null entry is a successful observation of an absent model-bank
        // association. A nonnull entry remains opaque: no model object, UE
        // object, RwObject or reference ownership is created or dereferenced.
        // Values apply only to this native callback; identical pointers across
        // callbacks cannot prove persistent model lifetime or loaded state.
    private:
        Binding    binding;
        CheckOwner owner = nullptr;
    };
}
