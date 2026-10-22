/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Raw breath over an immediately resolved ped and its own inline player data
 ****************************************************************************/
#pragma once
#include "NativePedOxygenAccess.h"
namespace mta::game_de
{
    class NativePedOxygenMemory final
    {
    public:
        using Binding = NativePedPoolReader::Binding;
        using CheckOwner = NativePedPoolReader::CheckOwner;
        using Value = NativePedPoolReader::Value;
        struct Receipt
        {
            std::uint32_t  handle = 0, original = 0, target = 0, observed = 0, exceptionCode = 0;
            std::uintptr_t nativePed = 0, playerData = 0, address = 0;
            bool           entered = false, accepted = false, observedValid = false, verified = false;
        };
        NativePedOxygenMemory(Binding binding, CheckOwner owner) noexcept : binding(binding), owner(owner) {}
        static bool Qualify(Binding&) noexcept;
        static bool Revalidate(const Binding&) noexcept;
        ClockResult ReadOxygen(const Value&, float&) const noexcept;
        ClockResult SetOxygen(const Value&, float, Receipt* = nullptr) const noexcept;
        // Fresh occupied ped resolution alone does not grant player-data
        // access. Match its own exact constructor link and the published row
        // zero ped each time; no current-player selector or generic fallback.
        // Preserve every raw binary32 bit, as actual CPedSA oxygen does. No
        // stamina/stat conversion, capacity clamp or cheat-state change exists.
        // Entered failures retain persistent or unknown partial effects; the
        // runtime owner must stop without replay or restoring old native data.
    private:
        Binding    binding;
        CheckOwner owner = nullptr;
    };
}
