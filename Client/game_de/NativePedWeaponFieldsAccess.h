/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Raw CWeapon type/state intent through selected registered ped slots
 ****************************************************************************/
#pragma once
#include "NativePedReferenceToken.h"
#include "NativePedPoolReader.h"
namespace mta::game_de
{
    class NativePedWeaponFieldsAccess
    {
    public:
        virtual ClockResult ReadPedWeaponType(const NativePedReferenceToken&, std::uint32_t, std::uint32_t&) noexcept { return ClockResult::NotInitialized; }
        virtual ClockResult SetPedWeaponType(const NativePedReferenceToken&, std::uint32_t, std::uint32_t) noexcept { return ClockResult::NotInitialized; }
        virtual ClockResult ReadPedWeaponState(const NativePedReferenceToken&, std::uint32_t, std::uint32_t&) noexcept { return ClockResult::NotInitialized; }
        virtual ClockResult SetPedWeaponState(const NativePedReferenceToken&, std::uint32_t, std::uint32_t) noexcept { return ClockResult::NotInitialized; }

    protected:
        ~NativePedWeaponFieldsAccess() = default;
    };
}
