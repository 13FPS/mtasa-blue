/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual raw collision-use setter on both qualified native ped classes
 ****************************************************************************/
#pragma once
#include "NativePedCollisionAccess.h"
namespace mta::game_de
{
    class NativePedCollisionMemory final
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
        NativePedCollisionMemory(Binding binding, CheckOwner owner) noexcept : binding(binding), owner(owner) {}
        static bool Qualify(Binding&) noexcept;
        static bool Revalidate(const Binding&) noexcept;
        ClockResult SetUsesCollision(const Value&, bool, Receipt* = nullptr) const noexcept;
        // Actual MTA exposes only SetUsesCollision. Named0619 independently
        // writes the same raw bool, then returns without component/actor calls;
        // the real physical contact consumer reads it. This is not a UE
        // component collision rebuild or a model/world collision override.
        // One masked CAS accepts this intent or refuses a racing word. The
        // retained owner Stops on any entered failure, without undoing or
        // replaying effects or retrying even an unaccepted neighbor race.
    private:
        Binding    binding;
        CheckOwner owner = nullptr;
    };
}
