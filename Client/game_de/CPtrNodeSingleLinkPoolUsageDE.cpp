/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Keep nested SingleLink capacity and occupancy on one pools owner
 ****************************************************************************/
#include "CPtrNodeSingleLinkPoolUsageDE.h"
#include <stdexcept>
#include <string>

namespace mta::game_de
{
    ClockResult CPtrNodeSingleLinkPoolUsageDE::ReadUsage(NativePoolUsageAccess::Value& output) const noexcept
    {
        return pools.ReadPoolUsage(POINTER_SINGLE_LINK_POOL, output);
    }
    void CPtrNodeSingleLinkPoolUsageDE::RequireUsage(ClockResult result, const char* operation)
    {
        if (result != ClockResult::Ok)
            throw std::runtime_error(std::string("DE SingleLink pool owner refused ") + operation + ": " + std::to_string(static_cast<unsigned>(result)));
    }
    std::size_t CPtrNodeSingleLinkPoolUsageDE::GetCapacity() const
    {
        NativePoolUsageAccess::Value value;
        RequireUsage(ReadUsage(value), "GetCapacity");
        return static_cast<std::size_t>(value.capacity);
    }
    std::size_t CPtrNodeSingleLinkPoolUsageDE::GetUsedSize() const
    {
        NativePoolUsageAccess::Value value;
        // Occupied native nodes include the world's and IPL's own nodes.
        // Counting SDK wrappers would omit those genuine shared allocations.
        RequireUsage(ReadUsage(value), "GetUsedSize");
        return static_cast<std::size_t>(value.used);
    }
}
