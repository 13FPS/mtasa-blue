/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Optional current-owner properties for qualified classic garage identities
 ****************************************************************************/
#pragma once
#include "NativeNamedGarageAccess.h"
#include <cstdint>

class CGarages;

namespace mta::game_de
{
    struct NativeNumericGarageDescriptor
    {
        std::uint32_t        sdkId = 0;
        NativeGarageName     name{};
        NativeGarageGeometry geometry{};
    };
    // An immutable descriptor is value identity, never a borrowed native row
    // or lease. Every operation reacquires its one current native match.
    class NativeNumericGarageAccess
    {
    public:
        virtual ~NativeNumericGarageAccess() = default;
        virtual ClockResult RequireStartupNumericGaragesReady() noexcept { return ClockResult::NotInitialized; }
        virtual ClockResult GetStartupNumericGarages(CGarages*&) noexcept { return ClockResult::NotInitialized; }
        virtual ClockResult ReadNumericGarageGeometry(std::uint32_t, NativeGarageGeometry&) noexcept { return ClockResult::NotInitialized; }
        virtual ClockResult ReadNumericGarageType(std::uint32_t, std::uint8_t&) noexcept { return ClockResult::NotInitialized; }
        virtual ClockResult SetNumericGarageType(std::uint32_t, std::uint8_t) noexcept { return ClockResult::NotInitialized; }
        virtual ClockResult ReadNumericGarageOpen(std::uint32_t, bool&) noexcept { return ClockResult::NotInitialized; }
        virtual ClockResult SetNumericGarageOpen(std::uint32_t, bool) noexcept { return ClockResult::NotInitialized; }
    };
}
