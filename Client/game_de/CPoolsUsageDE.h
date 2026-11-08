/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Serve actual MTA pool usage queries from their admitted native banks
 ****************************************************************************/
#pragma once
#include "CPoolsEntityDE.h"
#include "NativePoolUsageAccess.h"

namespace mta::game_de
{
    // The existing SDK object and ped registry remain the same receiver. Only
    // current allocation usage is implemented here; allocation, resizing and
    // unsupported default/max-capacity policies still require their own owner.
    class CPoolsUsageDE : public CPoolsEntityDE
    {
    public:
        int         GetNumberOfUsedSpaces(ePools) override;
        int         GetPoolCapacity(ePools) override;
        ClockResult ReadPoolUsage(ePools, NativePoolUsageAccess::Value&) noexcept;

    protected:
        explicit CPoolsUsageDE(NativeGameTimingAccess& owner) noexcept : CPoolsEntityDE(owner), usageOwner(owner) {}

    private:
        static void             RequireUsage(ClockResult, const char*);
        NativeGameTimingAccess& usageOwner;
    };
    static_assert(std::is_abstract_v<CPoolsUsageDE> && std::is_base_of_v<CPoolsEntityDE, CPoolsUsageDE>);
}
