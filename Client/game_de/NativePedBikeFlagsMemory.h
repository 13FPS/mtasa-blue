/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Masked low-two-bit bike-knockoff storage on both known native ped classes
 ****************************************************************************/
#pragma once
#include "NativePedBikeFlagsAccess.h"
namespace mta::game_de
{
    class NativePedBikeFlagsMemory final
    {
    public:
        using Binding = NativePedPoolReader::Binding;
        using CheckOwner = NativePedPoolReader::CheckOwner;
        using Value = NativePedPoolReader::Value;
        struct Receipt
        {
            std::uint32_t  handle = 0, requested = 0, original = 0, target = 0, observed = 0, exceptionCode = 0, attempts = 0;
            std::uintptr_t nativePed = 0, address = 0;
            bool           entered = false, accepted = false, observedValid = false, verified = false;
        };
        NativePedBikeFlagsMemory(Binding binding, CheckOwner owner) noexcept : binding(binding), owner(owner) {}
        static bool Qualify(Binding&) noexcept;
        static bool Revalidate(const Binding&) noexcept;
        ClockResult ReadCantBeKnockedOffBike(const Value&, std::uint32_t&) const noexcept;
        ClockResult SetCantBeKnockedOffBike(const Value&, std::uint32_t, Receipt* = nullptr) const noexcept;
        // Reuse actual metadata/pool/native GT/GC/state admission. The SDK
        // passes its full DWORD once; this native pair stores only its low2.
        // Preserve every neighboring bit and retry only unaccepted neighbor
        // CAS conflicts within the bound. Accepted writes are never replayed,
        // cleared or rolled back; entered failure requires owning-runtime Stop.
        // Value3 is raw storage only, not an invented gameplay mode.
    private:
        Binding    binding;
        CheckOwner owner = nullptr;
    };
}
