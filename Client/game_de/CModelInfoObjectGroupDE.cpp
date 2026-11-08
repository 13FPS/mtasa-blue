/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Dispatch actual unsigned group operations without an extra cache
 *****************************************************************************/
#include "CModelInfoObjectGroupDE.h"
#include <stdexcept>
#include <string>

namespace mta::game_de::ModelObjectGroupSdk
{
    namespace
    {
        void Require(ClockResult status, const char* operation)
        {
            if (status != ClockResult::Ok)
                throw std::runtime_error(std::string("DE ") + operation + ": " + std::to_string(static_cast<std::uint32_t>(status)));
        }
    }
    NativeModelClassificationAccess& RequireSameOwner(NativeModelClassificationAccess& classification, NativeModelObjectGroupAccess& groups)
    {
        // The wheel base checks its own sibling capability during this same
        // constructor chain. A third unrelated writer cannot retarget it.
        if (dynamic_cast<NativeModelObjectGroupAccess*>(&classification) != &groups)
            throw std::runtime_error("DE model object-group methods require the same classification owner");
        return classification;
    }
    std::uint16_t Get(NativeModelObjectGroupAccess& owner, std::uint32_t model)
    {
        std::uint16_t value = 0;
        Require(owner.ReadModelObjectPropertiesGroup(model, value), "GetObjectPropertiesGroup");
        return value;
    }
    void Set(NativeModelObjectGroupAccess& owner, std::uint32_t model, std::uint16_t value)
    {
        // The public SDK already supplies an unsigned WORD. Preserve all its
        // bits, including the static sentinel, and let the qualified owner
        // perform the original same-value/default-map/native-store itinerary.
        Require(owner.SetModelObjectPropertiesGroup(model, value), "SetObjectPropertiesGroup");
    }
    void Restore(NativeModelObjectGroupAccess& owner, std::uint32_t model)
    {
        Require(owner.RestoreModelObjectPropertiesGroup(model), "RestoreObjectPropertiesGroup");
    }
}
