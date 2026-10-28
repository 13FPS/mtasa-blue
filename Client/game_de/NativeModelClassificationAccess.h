/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Keep MTA model predicate intents with the retained native callback owner
 ****************************************************************************/
#pragma once
#include "NativeModelClassificationCalls.h"
namespace mta::game_de
{
    class NativeModelClassificationAccess
    {
    public:
        virtual ~NativeModelClassificationAccess() = default;
        // An old/read-only owner cannot manufacture a negative vehicle class
        // result. Refusal preserves the caller's previous observation.
        virtual ClockResult ReadModelArchiveAllocated(std::uint32_t, bool&) noexcept { return ClockResult::NotInitialized; }
        virtual ClockResult ReadModelType(std::uint32_t, NativeModelTypeObservation&) noexcept { return ClockResult::NotInitialized; }
        virtual ClockResult ReadModelVehiclePredicate(std::uint32_t, NativeModelVehiclePredicate, bool&) noexcept { return ClockResult::NotInitialized; }
        // Older owners must refuse this distinct raw DWORD observation. The
        // SDK owns any explicit low-byte conversion, not an inferred enum map.
        virtual ClockResult ReadModelVehicleSubtype(std::uint32_t, std::uint32_t&) noexcept { return ClockResult::NotInitialized; }
        // These current opaque observations are independent of classification
        // and loaded state. Missing authority refuses instead of manufacturing
        // null; successful null values grant no target layout or lifetime.
        virtual ClockResult ReadModelInterface(std::uint32_t, std::uintptr_t&) noexcept { return ClockResult::NotInitialized; }
        virtual ClockResult ReadModelRender(std::uint32_t, std::uintptr_t&) noexcept { return ClockResult::NotInitialized; }
        // The qualified slot78 observes vehicle6c0 or genuine ped/clump68,
        // bookending the fullQWORD before the native request caller's EAX
        // conversion. Neither getter nor a sampled pointee is invoked.
        virtual ClockResult ReadModelAnimFileIndex(std::uint32_t, std::uint32_t&) noexcept { return ClockResult::NotInitialized; }
        // Read-only owners cannot substitute a special streaming request
        // for native creation and irreversible bank publication. The real
        // owner must journal every entered effect before returning status.
        virtual ClockResult CreateAndPublishPedModelMetadata(std::uint32_t) noexcept { return ClockResult::NotInitialized; }
        // This is collision-pointer identity, not a ped category or geometry
        // test. Older owners refuse without manufacturing a negative answer.
        virtual ClockResult ReadModelPlayerPredicate(std::uint32_t, bool&) noexcept { return ClockResult::NotInitialized; }
        // Startup's nullable borrowed-pointer copy has no SetColModel policy,
        // reference, deletion or adoption effects. The current owner journals
        // an entered write before acknowledging this optional capability.
        virtual ClockResult CopyStartupModelCollision(std::uint32_t, std::uint32_t) noexcept { return ClockResult::NotInitialized; }
    };
}
