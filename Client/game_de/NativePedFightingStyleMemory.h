/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Complete MTA style/extra storage intent on qualified native peds
 ****************************************************************************/
#pragma once
#include "NativePedFightingStyleAccess.h"
namespace mta::game_de
{
    class NativePedFightingStyleMemory final
    {
    public:
        using Binding = NativePedPoolReader::Binding;
        using CheckOwner = NativePedPoolReader::CheckOwner;
        using Value = NativePedPoolReader::Value;
        struct Receipt
        {
            std::uint32_t  handle = 0, original = 0, target = 0, observed = 0, exceptionCode = 0, attempts = 0;
            std::uint8_t   requestedStyle = 0, requestedExtra = 0;
            std::uintptr_t nativePed = 0, address = 0;
            bool           entered = false, accepted = false, observedValid = false, verified = false, changed = false;
        };
        NativePedFightingStyleMemory(Binding binding, CheckOwner owner) noexcept : binding(binding), owner(owner) {}
        static bool Qualify(Binding&) noexcept;
        static bool Revalidate(const Binding&) noexcept;
        ClockResult ReadFightingStyle(const Value&, std::uint8_t&) const noexcept;
        ClockResult SetFightingStyle(const Value&, std::uint8_t style, std::uint8_t styleExtra, Receipt* = nullptr) const noexcept;
        // Same-style acknowledgement never applies an extra mask or enters
        // CAS. Changed style owns its byte and only the eligible extra OR bit;
        // other extra bits plus both neighboring bytes remain unowned. Retry
        // only unaccepted neighbor conflicts; entered failure Stop/no replay.
    private:
        Binding    binding;
        CheckOwner owner = nullptr;
    };
}
