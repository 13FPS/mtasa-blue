/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Optional immediate named garage properties without SDK numeric-ID publication
 ****************************************************************************/
#pragma once
#include "NativePedPoolReader.h"
#include <array>
namespace mta::game_de
{
    using NativeGarageName = std::array<unsigned char, 8>;
    class NativeNamedGarageAccess
    {
    public:
        virtual ~NativeNamedGarageAccess() = default;
        virtual ClockResult ReadNamedGarageType(const NativeGarageName&, std::uint8_t&) noexcept { return ClockResult::NotInitialized; }
        virtual ClockResult SetNamedGarageType(const NativeGarageName&, std::uint8_t) noexcept { return ClockResult::NotInitialized; }
        virtual ClockResult ReadNamedGarageOpen(const NativeGarageName&, bool&) noexcept { return ClockResult::NotInitialized; }
        virtual ClockResult SetNamedGarageOpen(const NativeGarageName&, bool) noexcept { return ClockResult::NotInitialized; }
    };
}
