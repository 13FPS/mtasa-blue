/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Observe the selected registered ped's actual movement animation group
 ****************************************************************************/
#pragma once
#include "NativePedReferenceToken.h"
#include "NativePedPoolReader.h"
namespace mta::game_de
{
    class NativePedMoveAnimAccess
    {
    public:
        virtual ClockResult ReadPedMoveAnim(const NativePedReferenceToken&, std::int32_t&) noexcept { return ClockResult::NotInitialized; }

    protected:
        ~NativePedMoveAnimAccess() = default;
    };
}
