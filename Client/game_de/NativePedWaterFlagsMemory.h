/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Preserve the qualified pair of physical water flags
 ****************************************************************************/
#pragma once
#include "NativePedWaterFlagsAccess.h"
namespace mta::game_de
{
    class NativePedWaterFlagsMemory final
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
        NativePedWaterFlagsMemory(Binding binding, CheckOwner owner) noexcept : binding(binding), owner(owner) {}
        static bool Qualify(Binding&) noexcept;
        static bool Revalidate(const Binding&) noexcept;
        ClockResult SetInWaterFlags(const Value&, bool, Receipt* = nullptr) const noexcept;
        // Actual MTA SetInWaterFlags writes both physical water booleans.
        // DE's named submerged consumer and native water-contact producer
        // qualify this pair, not the distinct task-based SDK IsInWater query.
        // Only unaccepted neighbor conflicts may retry; any entered failure
        // stops the retained owner, preserving accepted/unknown effects.
    private:
        Binding    binding;
        CheckOwner owner = nullptr;
    };
}
