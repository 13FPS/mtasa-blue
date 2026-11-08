/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Preserve actual nested building size and free-slot observations
 ****************************************************************************/
#include "CBuildingsPoolUsageDE.h"
#include <stdexcept>
#include <string>

namespace mta::game_de
{
    ClockResult CBuildingsPoolUsageDE::ReadUsage(NativePoolUsageAccess::Value& output) const noexcept
    {
        return pools.ReadPoolUsage(BUILDING_POOL, output);
    }
    void CBuildingsPoolUsageDE::RequireUsage(ClockResult result, const char* operation)
    {
        if (result != ClockResult::Ok)
            throw std::runtime_error(std::string("DE building pool owner refused ") + operation + ": " + std::to_string(static_cast<unsigned>(result)));
    }
    int CBuildingsPoolUsageDE::GetSize() const
    {
        NativePoolUsageAccess::Value value;
        RequireUsage(ReadUsage(value), "GetSize");
        return static_cast<int>(value.capacity);
    }
    bool CBuildingsPoolUsageDE::HasFreeBuildingSlot()
    {
        NativePoolUsageAccess::Value value;
        RequireUsage(ReadUsage(value), "HasFreeBuildingSlot");
        // Classic asks the native pool for an available slot. The admitted
        // complete flag census proves exactly the same existence predicate;
        // it neither reserves a slot nor promises a later allocation can run.
        return value.used < value.capacity;
    }
}
