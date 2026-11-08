/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Keep object-group defaults and writes with the existing native owner
 *****************************************************************************/
#pragma once
#include <game/CGameClockService.h>
#include <cstdint>

namespace mta::game_de
{
    class NativeModelObjectGroupAccess
    {
    public:
        virtual ~NativeModelObjectGroupAccess() = default;
        // Missing native authority must refuse, rather than invent a static
        // group, mutate a classic layout or acknowledge an unperformed restore.
        virtual ClockResult ReadModelObjectPropertiesGroup(std::uint32_t, std::uint16_t&) noexcept { return ClockResult::NotInitialized; }
        virtual ClockResult SetModelObjectPropertiesGroup(std::uint32_t, std::uint16_t) noexcept { return ClockResult::NotInitialized; }
        virtual ClockResult RestoreModelObjectPropertiesGroup(std::uint32_t) noexcept { return ClockResult::NotInitialized; }
        virtual ClockResult RestoreAllModelObjectPropertiesGroups() noexcept { return ClockResult::NotInitialized; }
    };
}
