/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Preserve actual SDK predicate results and explicit borrowed-owner refusals
 ****************************************************************************/
#include "CModelInfoDE.h"
#include <stdexcept>
#include <exception>
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
        // Preserve MTA's key/archive gate using DE's genuine named archive
        // predicate before consuming the native model-type observation.
        if (model >= 20000 || !IsAllocatedInArchive())
            return false;
        return QueryType() == eModelInfoType::VEHICLE;
    }
    bool CModelInfoDE::IsAllocatedInArchive() const noexcept
    {
        bool allocated = false;
        // The genuine DE named predicate is the counterpart of this SDK
        // intent, not a fabricated classic physical sizeInBlocks field.
        // noexcept offers no refusal value: do not turn owner loss into an
        // apparently successful absent archive. This slice stays unpublished.
        if (access.ReadModelArchiveAllocated(model, allocated) != ClockResult::Ok)
            std::terminate();
        return allocated;
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
