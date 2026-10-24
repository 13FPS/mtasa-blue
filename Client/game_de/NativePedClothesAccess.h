/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Forward actual ped clothes intent through the existing native owner
 ****************************************************************************/
#pragma once
#include "NativePedReferenceToken.h"
#include <game/CGameClockService.h>
#include <cstdint>
namespace mta::game_de
{
    class NativePedClothesAccess
    {
    public:
        // The synchronous owner first qualifies the current descriptor,
        // preserving its null early return before reading caller names. It
        // then copies supported nullable names for the genuine entry; this
        // contract adds neither rebuilding nor a complete player factory.
        virtual ClockResult SetPedClothes(const NativePedReferenceToken&, const char*, const char*, std::uint8_t) noexcept
        {
            return ClockResult::NotInitialized;
        }

    protected:
        ~NativePedClothesAccess() = default;
    };
}
