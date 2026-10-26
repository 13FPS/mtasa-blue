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
    CBaseModelInfoSAInterface* CModelInfoDE::GetInterface()
    {
        const auto     index = model;
        std::uintptr_t value = 0;
        const auto     status = access.ReadModelInterface(index, value);
        if (status != ClockResult::Ok)
            throw std::runtime_error("DE model owner refused GetInterface: " + std::to_string(static_cast<unsigned>(status)));
        // The source SDK spelling is only an opaque handle here. DE's
        // immutable-index facade observes its current owner each call; it
        // owns no classic interface layout or persistent cached pointer.
        return reinterpret_cast<CBaseModelInfoSAInterface*>(value);
    }
    RwObject* CModelInfoDE::GetRwObject()
    {
        const auto     index = model;
        std::uintptr_t value = 0;
        const auto     status = access.ReadModelRender(index, value);
        if (status != ClockResult::Ok)
            throw std::runtime_error("DE model owner refused GetRwObject: " + std::to_string(static_cast<unsigned>(status)));
        // Classic reads its cached interface; this DE representation instead
        // re-admits current bank/field40. Preserve raw null/poison tag bits with
        // no loaded/type/bundle query, target dereference or ownership claim.
        return reinterpret_cast<RwObject*>(value);
    }
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
