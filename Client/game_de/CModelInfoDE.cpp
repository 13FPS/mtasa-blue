/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Preserve actual SDK predicate results and explicit borrowed-owner refusals
 ****************************************************************************/
#include "CModelInfoDE.h"
#include <stdexcept>
#include <string>
namespace mta::game_de
{
    eModelInfoType CModelInfoDE::QueryType() const
    {
        NativeModelTypeObservation type;
        const auto                 status = access.ReadModelType(model, type);
        if (status != ClockResult::Ok)
            throw std::runtime_error("DE model owner refused model type: " + std::to_string(static_cast<unsigned>(status)));
        return type.hasModel ? static_cast<eModelInfoType>(type.nativeType) : eModelInfoType::UNKNOWN;
    }
    eModelInfoType CModelInfoDE::GetModelType()
    {
        return QueryType();
    }
    bool CModelInfoDE::IsVehicle() const
    {
        // Preserve the actual MTA wrapper's key/archive prerequisites. The
        // archive method stays pure until its DE backing is qualified; a pure
        // type observation must not invent an allocated native archive result.
        if (model >= 20000 || !IsAllocatedInArchive())
            return false;
        return QueryType() == eModelInfoType::VEHICLE;
    }
    bool CModelInfoDE::Query(NativeModelVehiclePredicate predicate)
    {
        bool       result = false;
        const auto status = access.ReadModelVehiclePredicate(model, predicate, result);
        if (status != ClockResult::Ok)
            throw std::runtime_error("DE model owner refused vehicle predicate: " + std::to_string(static_cast<unsigned>(status)));
        return result;
    }
    bool CModelInfoDE::IsBoat()
    {
        return Query(NativeModelVehiclePredicate::Boat);
    }
    bool CModelInfoDE::IsCar()
    {
        return Query(NativeModelVehiclePredicate::Car);
    }
    bool CModelInfoDE::IsPlane()
    {
        return Query(NativeModelVehiclePredicate::Plane);
    }
    bool CModelInfoDE::IsHeli()
    {
        return Query(NativeModelVehiclePredicate::Heli);
    }
}
