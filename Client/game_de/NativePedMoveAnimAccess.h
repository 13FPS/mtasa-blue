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
        // Internal whole original on the registered player's CURRENT group.
        // It writes no requested group or CPU override and grants neither
        // block references, persistent hooks nor a complete SetMoveAnim.
        virtual ClockResult ReapplyCurrentPlayerMoveAnim(const NativePedReferenceToken&) noexcept { return ClockResult::NotInitialized; }

    protected:
        ~NativePedMoveAnimAccess() = default;
    };
}
