/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Preserve MTA wheel selection, raw floats and visible owner refusals
 *****************************************************************************/
#include "CModelInfoWheelDE.h"
#include <game/CModelWheelSizePolicy.h>
#include <stdexcept>
#include <string>

namespace mta::game_de::ModelWheelSdk
{
    namespace
    {
        void Require(ClockResult result, const char* operation)
        {
            if (result != ClockResult::Ok)
                throw std::runtime_error(std::string("DE ") + operation + ": " + std::to_string(static_cast<std::uint32_t>(result)));
        }
    }
    NativeModelClassificationAccess& RequireSameOwner(NativeModelClassificationAccess& classification, NativeModelWheelAccess& wheel)
    {
        // Both capabilities must be sibling views of one retained backend.
        // A separately supplied wheel writer cannot retarget a model facade.
        if (dynamic_cast<NativeModelWheelAccess*>(&classification) != &wheel)
            throw std::runtime_error("DE model wheel methods require the same classification owner");
        return classification;
    }
    float Get(NativeModelWheelAccess& access, std::uint32_t model, ResizableVehicleWheelGroup::Enum group)
    {
        NativeModelWheelSizes value;
        Require(access.ReadModelWheelSizes(model, value), "GetVehicleWheelSize");
        return MtaGame::CModelWheelSizePolicy::Get(static_cast<std::uint32_t>(group), [&] { return value.front; }, [&] { return value.rear; });
    }
    void Set(NativeModelWheelAccess& access, std::uint32_t model, ResizableVehicleWheelGroup::Enum group, float value)
    {
        static_assert(static_cast<std::uint32_t>(ResizableVehicleWheelGroup::FRONT_AXLE) == static_cast<std::uint32_t>(NativeModelWheelGroup::Front) &&
                      static_cast<std::uint32_t>(ResizableVehicleWheelGroup::REAR_AXLE) == static_cast<std::uint32_t>(NativeModelWheelGroup::Rear) &&
                      static_cast<std::uint32_t>(ResizableVehicleWheelGroup::ALL_WHEELS) == static_cast<std::uint32_t>(NativeModelWheelGroup::All));
        // Script validation belongs to its existing caller. This SDK route
        // preserves raw float bits and the classic ignored-group/cache order.
        Require(access.SetModelWheelSize(model, static_cast<NativeModelWheelGroup>(group), value), "SetVehicleWheelSize");
    }
    void Reset(NativeModelWheelAccess& access, std::uint32_t model, const std::pair<float, float>* defaults)
    {
        NativeModelWheelSizes copied;
        if (defaults)
            copied = {defaults->first, defaults->second};
        Require(access.ResetModelWheelSizes(model, defaults ? &copied : nullptr), "ResetVehicleWheelSizes");
    }
}
