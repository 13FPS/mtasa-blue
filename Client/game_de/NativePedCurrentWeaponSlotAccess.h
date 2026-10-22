/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Observe current weapon selection through the registered ped owner
 ****************************************************************************/
#pragma once
#include "NativePedReferenceToken.h"
#include "NativePedPoolReader.h"
namespace mta::game_de
{
    class NativePedCurrentWeaponSlotAccess
    {
    public:
        virtual ClockResult ReadPedCurrentWeaponSlot(const NativePedReferenceToken&, std::uint32_t&) noexcept { return ClockResult::NotInitialized; }

    protected:
        ~NativePedCurrentWeaponSlotAccess() = default;
    };
}
