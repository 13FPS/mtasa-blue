/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Masked raw CanBeShotInVehicle storage on both known native ped classes
 ****************************************************************************/
#pragma once
#include "NativePedShotFlagsAccess.h"
namespace mta::game_de
{
    class NativePedShotFlagsMemory final
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
        NativePedShotFlagsMemory(Binding binding, CheckOwner owner) noexcept : binding(binding), owner(owner) {}
        static bool Qualify(Binding&) noexcept;
        static bool Revalidate(const Binding&) noexcept;
        ClockResult ReadCanBeShotInVehicle(const Value&, bool&) const noexcept;
        ClockResult SetCanBeShotInVehicle(const Value&, bool, Receipt* = nullptr) const noexcept;
        // Reuse real registered-ped metadata/pool/native GT/GC/state admission.
        // Named054A proves bool storage, not a damage-engine promise. A bounded
        // neighbor-only CAS conflict may be reacquired before any accepted
        // write; accepted effects are never replayed, cleared or rolled back.
        // Retained owner must Stop on any entered failure, including an unknown
        // partial write. The independently named TestForShot flag is unproved
        // and is deliberately not exposed by this primitive.
    private:
        Binding    binding;
        CheckOwner owner = nullptr;
    };
}
