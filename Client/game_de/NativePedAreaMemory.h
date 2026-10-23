/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Exact-width raw area read/CAS on a freshly resolved occupied ped
 ****************************************************************************/
#pragma once
#include "NativePedAreaAccess.h"
#include "NativePedPoolReader.h"
namespace mta::game_de
{
    class NativePedAreaMemory final
    {
    public:
        using Binding = NativePedPoolReader::Binding;
        using CheckOwner = NativePedPoolReader::CheckOwner;
        using Value = NativePedPoolReader::Value;
        struct Receipt
        {
            std::uint32_t  handle = 0, exceptionCode = 0;
            std::uintptr_t nativePed = 0, address = 0;
            std::uint8_t   original = 0, target = 0, observed = 0;
            bool           entered = false, accepted = false, observedValid = false, verified = false;
        };
        NativePedAreaMemory(Binding value, CheckOwner callback) noexcept : binding(value), owner(callback) {}
        static bool Qualify(Binding&) noexcept;
        static bool Revalidate(const Binding&) noexcept;
        ClockResult ReadAreaCode(const Value&, std::uint8_t&) const noexcept;
        ClockResult SetAreaCode(const Value&, std::uint8_t, Receipt* = nullptr) const noexcept;
        // BYTE intent is the actual MTA SDK contract. The named DE command
        // independently proves this field but has extra interior/player
        // cleanup, so it is not invoked. Every value is retained; no adjacent
        // field, streaming/camera operation or factory is supplied here.
        // Entered failures retain possible accepted effects. The real token
        // owner must stop, never replay or restore the old native byte.
    private:
        Binding    binding;
        CheckOwner owner;
    };
}
