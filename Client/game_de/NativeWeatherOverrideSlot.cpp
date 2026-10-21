/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Own nonexecutable reachable scratch without confusing its interior slot
 *****************************************************************************/
#include "NativeWeatherOverrideSlot.h"
#include <windows.h>

namespace mta::game_de
{
    namespace Placement = NativeWeatherOverridePlacementProfile;
    bool NativeWeatherOverrideSlot::Valid() const noexcept
    {
        MEMORY_BASIC_INFORMATION page{};
        return allocation && allocation % Placement::AllocationBytes == 0 && accumulator >= allocation &&
               accumulator - allocation <= Placement::AllocationBytes - sizeof(float) && accumulator % sizeof(float) == 0 &&
               VirtualQuery(reinterpret_cast<const void*>(allocation), &page, sizeof(page)) && page.AllocationBase == reinterpret_cast<void*>(allocation) &&
               page.State == MEM_COMMIT && page.Type == MEM_PRIVATE && page.Protect == PAGE_READWRITE && page.RegionSize >= Placement::AllocationBytes;
    }
    ClockResult NativeWeatherOverrideSlot::Allocate(const Placement::Placement& placement, NativeWeatherOverrideSlot& output) noexcept
    {
        if (output.HasAllocation())
            return ClockResult::Busy;
        if (!placement.Count())
            return ClockResult::InvalidArgument;
        SYSTEM_INFO system{};
        GetSystemInfo(&system);
        if (system.dwAllocationGranularity != Placement::AllocationBytes)
            return ClockResult::UnsupportedBuild;
        for (std::size_t index = 0; index < placement.Count(); ++index)
        {
            Placement::Candidate candidate;
            if (!placement.At(index, candidate))
                continue;
            MEMORY_BASIC_INFORMATION page{};
            if (!VirtualQuery(reinterpret_cast<const void*>(candidate.allocation), &page, sizeof(page)) || page.State != MEM_FREE)
                continue;
            const auto base = reinterpret_cast<std::uintptr_t>(page.BaseAddress);
            if (candidate.allocation < base || candidate.allocation - base > page.RegionSize ||
                Placement::AllocationBytes > page.RegionSize - (candidate.allocation - base))
                continue;
            const auto value = reinterpret_cast<std::uintptr_t>(
                VirtualAlloc(reinterpret_cast<void*>(candidate.allocation), Placement::AllocationBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
            if (!value)
                continue;
            // Publish lifetime ownership before any fallible confirmation.
            output.allocation = value;
            output.accumulator = value + (candidate.accumulator - candidate.allocation);
            if (value != candidate.allocation || !placement.Accepts({output.allocation, output.accumulator}) || !output.Valid())
                return ClockResult::WriteFailure;
            return ClockResult::Ok;
        }
        return ClockResult::WriteFailure;
    }
    bool NativeWeatherOverrideSlot::Release() noexcept
    {
        if (!HasAllocation())
            return true;
        if (!Valid() || !VirtualFree(reinterpret_cast<void*>(allocation), 0, MEM_RELEASE))
            return false;
        allocation = accumulator = 0;
        return true;
    }
}
