/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Raw vitals with real current-player/pool identity bookends
 ****************************************************************************/
#pragma once
#include "NativePedVitalsAccess.h"
#include "NativePlayerInfoMemory.h"

namespace mta::game_de
{
    class NativePedVitalsMemory final
    {
    public:
        using Binding = NativePedPoolReader::Binding;
        using CheckOwner = NativePedPoolReader::CheckOwner;
        using Value = NativePedPoolReader::Value;
        struct Receipt
        {
            NativePedVitalField field = NativePedVitalField::Health;
            std::uint32_t       handle = 0, original = 0, target = 0, observed = 0, exceptionCode = 0;
            std::uintptr_t      nativePed = 0, address = 0;
            bool                entered = false, attempted = false, applied = false, observedValid = false, verified = false;
        };

        NativePedVitalsMemory(Binding binding, CheckOwner owner, const NativePlayerInfoMemory& player) noexcept : binding(binding), owner(owner), player(player)
        {
        }
        static bool Qualify(Binding&) noexcept;
        static bool Revalidate(const Binding&) noexcept;
        ClockResult Read(const Value&, NativePedVitalField, float&) const noexcept;
        ClockResult Set(const Value&, NativePedVitalField, float, Receipt* = nullptr) const noexcept;

        // The Value must be the caller's fresh registered-token resolution,
        // never a saved raw-pointer lease. This primitive independently checks
        // that same pool identity against the actual CURRENT local player.
        // All float bits survive, including NaNs and signed zero; no clamping,
        // damage/resurrection call or whole-row rollback implements raw CPed.
        // An entered non-Ok setter can already have accepted persistent effect;
        // its real owner must stop without retrying or restoring old vitals.
    private:
        Binding                       binding;
        CheckOwner                    owner = nullptr;
        const NativePlayerInfoMemory& player;
    };
}
