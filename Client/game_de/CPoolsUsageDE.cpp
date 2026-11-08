/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Preserve SDK pool identity and explicit refusal for native census
 ****************************************************************************/
#include "CPoolsUsageDE.h"
#include "NativeGameTimingAccess.h"
#include <limits>
#include <stdexcept>
#include <string>

namespace mta::game_de
{
    ClockResult CPoolsUsageDE::ReadPoolUsage(ePools pool, NativePoolUsageAccess::Value& output) noexcept
    {
        using Kind = NativePoolUsageAccess::Kind;
        Kind kind;
        switch (pool)
        {
            case BUILDING_POOL:
                kind = Kind::Building;
                break;
            case PED_POOL:
                kind = Kind::Ped;
                break;
            case OBJECT_POOL:
                kind = Kind::Object;
                break;
            case DUMMY_POOL:
                kind = Kind::Dummy;
                break;
            case TASK_POOL:
                kind = Kind::Task;
                break;
            case POINTER_DOUBLE_LINK_POOL:
                kind = Kind::PointerDoubleLink;
                break;
            case POINTER_SINGLE_LINK_POOL:
                kind = Kind::PointerSingleLink;
                break;
            default:
                return ClockResult::InvalidArgument;
        }
        NativePoolUsageAccess::Value value;
        const auto                   result = usageOwner.ReadPoolUsage(kind, value);
        if (result != ClockResult::Ok)
            return result;
        // The SDK returns int. Reject a malformed owner response before a
        // narrowing conversion can publish a negative size or impossible count.
        if (!value.capacity || value.capacity > static_cast<std::uint32_t>((std::numeric_limits<int>::max)()) || value.used > value.capacity)
            return ClockResult::BindingChanged;
        output = value;
        return ClockResult::Ok;
    }
    void CPoolsUsageDE::RequireUsage(ClockResult result, const char* operation)
    {
        if (result != ClockResult::Ok)
            throw std::runtime_error(std::string("DE native pool owner refused ") + operation + ": " + std::to_string(static_cast<unsigned>(result)));
    }
    int CPoolsUsageDE::GetNumberOfUsedSpaces(ePools pool)
    {
        NativePoolUsageAccess::Value value;
        RequireUsage(ReadPoolUsage(pool, value), "GetNumberOfUsedSpaces");
        return static_cast<int>(value.used);
    }
    int CPoolsUsageDE::GetPoolCapacity(ePools pool)
    {
        NativePoolUsageAccess::Value value;
        // Every currently admitted bank retains its source-qualified native
        // geometry. Read that current header instead of reusing the unrelated
        // classic constructor immediates or the CPU ped registration count.
        RequireUsage(ReadPoolUsage(pool, value), "GetPoolCapacity");
        return static_cast<int>(value.capacity);
    }
}
