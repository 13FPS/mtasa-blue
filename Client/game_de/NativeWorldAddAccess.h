/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Route qualified WorldAdd intent through the existing native owner
 ****************************************************************************/
#pragma once
#include "NativeWorldAddEntryCalls.h"
namespace mta::game_de
{
    class NativeWorldAddAccess
    {
    public:
        using Kind = NativeWorldAddEntryCalls::Kind;
        virtual ~NativeWorldAddAccess() = default;
        virtual ClockResult CheckWorldEntityAddReady(Kind, std::uint32_t) noexcept { return ClockResult::NotInitialized; }
        virtual ClockResult AddWorldEntity(Kind, std::uint32_t) noexcept { return ClockResult::NotInitialized; }
    };
}
