/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Raw native physical scalars with occupied-ped and owner bookends
 ****************************************************************************/
#pragma once
#include "NativePedPhysicalScalarsAccess.h"

namespace mta::game_de
{
    class NativePedPhysicalScalarsMemory final
    {
    public:
        using Binding = NativePedPoolReader::Binding;
        using CheckOwner = NativePedPoolReader::CheckOwner;
        using Value = NativePedPoolReader::Value;
        struct Receipt
        {
            NativePedPhysicalScalarField field = NativePedPhysicalScalarField::Mass;
            std::uint32_t                handle = 0, original = 0, target = 0, observed = 0, exceptionCode = 0;
            std::uintptr_t               nativePed = 0, address = 0;
            bool                         entered = false, attempted = false, applied = false, observedValid = false, verified = false;
        };

        NativePedPhysicalScalarsMemory(Binding binding, CheckOwner owner) noexcept : binding(binding), owner(owner) {}
        static bool Qualify(Binding&) noexcept;
        static bool Revalidate(const Binding&) noexcept;
        ClockResult Read(const Value&, NativePedPhysicalScalarField, float&) const noexcept;
        ClockResult Set(const Value&, NativePedPhysicalScalarField, float, Receipt* = nullptr) const noexcept;

        // The Value must be the caller's fresh registered-token resolution,
        // never a saved raw-pointer lease. This primitive independently checks
        // that same occupied generic/player ped identity without substituting
        // the local-player root or inventing a borrowed native lifetime.
        // All float bits survive, including NaNs and signed zero. No native
        // force, repair, clamping or row rollback is hidden in raw accessors.
        // An entered non-Ok setter can already have accepted persistent effect;
        // its real owner must stop without retrying or restoring old mass state.
    private:
        Binding    binding;
        CheckOwner owner = nullptr;
    };
}
