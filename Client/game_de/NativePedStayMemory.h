/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Masked named stay-in-place storage on both known native ped classes
 ****************************************************************************/
#pragma once
#include "NativePedStayAccess.h"
namespace mta::game_de
{
    class NativePedStayMemory final
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
        NativePedStayMemory(Binding binding, CheckOwner owner) noexcept : binding(binding), owner(owner) {}
        static bool Qualify(Binding&) noexcept;
        static bool Revalidate(const Binding&) noexcept;
        ClockResult ReadStayInSamePlace(const Value&, bool&) const noexcept;
        ClockResult SetStayInSamePlace(const Value&, bool, Receipt* = nullptr) const noexcept;
        // Reuse real registered-ped metadata/pool/native GT/GC/state admission.
        // Named0350 proves bool storage, not a replacement task/control policy. A bounded
        // neighbor-only CAS conflict may be reacquired before any accepted
        // write; accepted effects are never replayed, cleared or rolled back.
        // Retained owner must Stop on any entered failure, including an unknown
        // partial write. KindaStay and task commands are separate contracts;
        // neither substitutes for this actual MTA boolean property.
    private:
        Binding    binding;
        CheckOwner owner = nullptr;
    };
}
