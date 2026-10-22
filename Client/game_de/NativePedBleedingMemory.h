/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Masked raw bleeding storage on both known native ped classes
 ****************************************************************************/
#pragma once
#include "NativePedBleedingAccess.h"
namespace mta::game_de
{
    class NativePedBleedingMemory final
    {
    public:
        using Binding = NativePedPoolReader::Binding;
        using CheckOwner = NativePedPoolReader::CheckOwner;
        using Value = NativePedPoolReader::Value;
        struct Receipt
        {
            std::uint32_t  handle = 0, original = 0, target = 0, observed = 0, exceptionCode = 0, attempts = 0;
            std::uintptr_t nativePed = 0, address = 0;
            bool           entered = false, accepted = false, observedValid = false, verified = false;
        };
        NativePedBleedingMemory(Binding binding, CheckOwner owner) noexcept : binding(binding), owner(owner) {}
        static bool Qualify(Binding&) noexcept;
        static bool Revalidate(const Binding&) noexcept;
        ClockResult ReadBleeding(const Value&, bool&) const noexcept;
        ClockResult SetBleeding(const Value&, bool, Receipt* = nullptr) const noexcept;
        // Reuse actual metadata/pool/native GT/GC/state admission. Named0332
        // and common initialization establish raw bool storage, not a blood
        // or damage-engine promise. Only unaccepted neighbor CAS conflicts
        // may be reacquired within the bound. Accepted or unknown effects
        // are never replayed, cleared or rolled back; entered failure requires
        // owning-runtime Stop. No adjacent flag gains authority here.
    private:
        Binding    binding;
        CheckOwner owner = nullptr;
    };
}
