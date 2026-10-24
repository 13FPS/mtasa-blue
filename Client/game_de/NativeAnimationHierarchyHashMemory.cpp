/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Observe only a genuine current hierarchy hash cell and its origin
 ****************************************************************************/
#include "NativeAnimationHierarchyHashMemory.h"
#include "NativeAnimationHierarchyOrigin.h"
namespace mta::game_de
{
    namespace AnimationHierarchyHashDetail
    {
        using Binding = NativeAnimationHierarchyHashMemory::Binding;
        ClockResult Observe(Binding binding, std::uintptr_t row, std::uint32_t& output) noexcept
        {
            // Preserve the gateway's sole DWORD payload read. Sharing origin
            // qualification must not introduce sequence or cache observation.
            return NativeImageAccess::ReadImage(binding.image, row, &output, sizeof(output), true) ? ClockResult::Ok : ClockResult::ReadFailure;
        }
    }
    ClockResult NativeAnimationHierarchyHashMemory::Read(std::uintptr_t locator, std::uint32_t& output) const noexcept
    {
        namespace Origin = NativeAnimationHierarchyOrigin;
        Origin::Location location;
        auto             result = Origin::Locate(binding, locator, location);
        if (result != ClockResult::Ok)
            return result;
        Origin::Observation<std::uint32_t> first, second;
        result = Origin::Capture(binding, owner, location.ordinal, location.rva, &AnimationHierarchyHashDetail::Observe, first);
        if (result != ClockResult::Ok ||
            (result = Origin::Capture(binding, owner, location.ordinal, location.rva, &AnimationHierarchyHashDetail::Observe, second)) != ClockResult::Ok)
            return result;
        if (!Origin::Same(first.origin, second.origin) || first.payload != second.payload)
            return ClockResult::BindingChanged;
        // Native unload retains this static row's current hash/owner/range.
        // Dynamic children and prior issued tags gain no lifetime guarantee.
        output = second.payload;
        return ClockResult::Ok;
    }
}
