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
    struct NativeGarageGeometry
    {
        // Actual storage words: position3, direction4, raw upperZ/width/depth3,
        // bounds4. Keep binary32 bits opaque until the actual SDK copies them.
        std::array<std::uint32_t, 14> words{};
    };
    class NativeNamedGarageAccess
    {
    public:
        virtual ~NativeNamedGarageAccess() = default;
        virtual ClockResult ReadNamedGarageGeometry(const NativeGarageName&, NativeGarageGeometry&) noexcept { return ClockResult::NotInitialized; }
        virtual ClockResult ReadNamedGarageType(const NativeGarageName&, std::uint8_t&) noexcept { return ClockResult::NotInitialized; }
        virtual ClockResult SetNamedGarageType(const NativeGarageName&, std::uint8_t) noexcept { return ClockResult::NotInitialized; }
        virtual ClockResult ReadNamedGarageOpen(const NativeGarageName&, bool&) noexcept { return ClockResult::NotInitialized; }
        virtual ClockResult SetNamedGarageOpen(const NativeGarageName&, bool) noexcept { return ClockResult::NotInitialized; }
    };
}
