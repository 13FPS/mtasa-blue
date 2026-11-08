/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Read native allocation usage through the existing callback owner
 ****************************************************************************/
#pragma once
#include <game/CGameClockService.h>
#include <cstdint>

namespace mta::game_de
{
    class NativePoolUsageAccess
    {
    public:
        // These explicit native families are deliberately independent of the
        // SDK ePools ordinals. No unchecked enum cast may select a native root.
        enum class Kind : unsigned char
        {
            Building,
            Ped,
            Object,
            Dummy,
            Task,
            PointerDoubleLink,
            PointerSingleLink
        };
        struct Value
        {
            std::uint32_t capacity = 0, used = 0;
        };
        virtual ~NativePoolUsageAccess() = default;
        // The existing owner supplies current callback/generation admission.
        // This is native occupancy, independent of MTA's SDK wrapper registry.
        virtual ClockResult ReadPoolUsage(Kind, Value&) noexcept { return ClockResult::NotInitialized; }
    };
}
