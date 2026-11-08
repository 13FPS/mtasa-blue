/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Raw SDK vector conversion and explicit same-owner dummy refusals
 *****************************************************************************/
#include "CModelInfoDummiesDE.h"
#include <stdexcept>
#include <string>

namespace mta::game_de::ModelDummiesSdk
{
    NativeModelClassificationAccess& RequireSameOwner(NativeModelClassificationAccess& classification, NativeModelDummiesAccess& dummies)
    {
        if (dynamic_cast<NativeModelDummiesAccess*>(&classification) != &dummies)
            throw std::runtime_error("DE model dummy methods require the same classification owner");
        return classification;
    }
    void Require(ClockResult result, const char* operation)
    {
        if (result != ClockResult::Ok)
            throw std::runtime_error(std::string("DE ") + operation + ": " + std::to_string(static_cast<std::uint32_t>(result)));
    }
    std::uint32_t Index(VehicleDummies::Enum selected)
    {
        const auto index = static_cast<std::uint32_t>(selected);
        // Native has an extra fifteenth vector. The real MTA SDK has fourteen
        // IDs; invalid enums cannot extend this qualified operation into it.
        Require(index < NativeModelDummyPositions{}.size() ? ClockResult::Ok : ClockResult::InvalidArgument, "vehicle dummy index");
        return index;
    }
    CVector Decode(const NativeModelDummyVector& value) noexcept
    {
        return CVector(value.x, value.y, value.z);
    }
    NativeModelDummyVector Encode(const CVector& value) noexcept
    {
        return {value.fX, value.fY, value.fZ};
    }
}
