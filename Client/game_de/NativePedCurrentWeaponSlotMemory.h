/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Fresh raw current-slot BYTE observation on occupied generic/player peds
 ****************************************************************************/
#pragma once
#include "NativePedCurrentWeaponSlotAccess.h"
namespace mta::game_de
{
    class NativePedCurrentWeaponSlotMemory final
    {
    public:
        using Binding = NativePedPoolReader::Binding;
        using CheckOwner = NativePedPoolReader::CheckOwner;
        using Value = NativePedPoolReader::Value;
        NativePedCurrentWeaponSlotMemory(Binding binding, CheckOwner owner) noexcept : binding(binding), owner(owner) {}
        static bool Qualify(Binding&) noexcept;
        static bool Revalidate(const Binding&) noexcept;
        ClockResult ReadCurrentSlot(const Value&, std::uint32_t&) const noexcept;
        // Exact native consumers use a signed byte for array access. This
        // actual SDK storage observation widens the original BYTE without
        // indexing, clamping or inventing a valid equipped slot. A typed SDK
        // enum boundary must separately reject values outside language0..15;
        // the native inline array remains bounded to gameplay slots0..12.
        // No setter, chosen-player byte, model/FX effect or lifetime is granted.
    private:
        Binding    binding;
        CheckOwner owner = nullptr;
    };
}
