/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Route qualified entity unlinking to the existing runtime owner
 ****************************************************************************/
#pragma once
#include "NativeWorldRemoveMemory.h"

namespace mta::game_de
{
    class NativeWorldRemoveAccess
    {
    public:
        using Kind = NativeWorldRemoveMemory::Kind;
        virtual ~NativeWorldRemoveAccess() = default;
        virtual ClockResult CheckWorldEntityRemoveReady(Kind, std::uint32_t) noexcept { return ClockResult::NotInitialized; }
        virtual ClockResult RemoveWorldEntity(Kind, std::uint32_t) noexcept { return ClockResult::NotInitialized; }
    };
}
