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
    bool CModelInfoDE::IsTowableBy(CModelInfo* towingModel)
    {
        // Preserve MTA's portable towing policy and virtual-call order. The
        // selected classifiers remain genuine backend obligations: refusal
        // must propagate rather than silently permitting a trailer link.
        bool       isTowable = true;
        const bool isTowTruck = towingModel->GetModel() == 525;
        const bool isTractor = towingModel->GetModel() == 531;
        if (IsTrain() || towingModel->IsTrain())
        {
            isTowable = false;
        }
        else if (isTowTruck || isTractor)
        {
            const bool isFarmTrailer = GetModel() == 610;
            if (IsBoat() || IsBike() || IsBmx())
            {
                isTowable = false;
            }
            else if (IsTrailer() && !(isTractor && isFarmTrailer))
            {
                isTowable = false;
            }
        }
        return isTowable;
    }

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
    std::uint32_t CModelInfoDE::GetAnimFileIndex()
    {
        const auto    index = model;
        std::uint32_t animation = 0;
        const auto    status = access.ReadModelAnimFileIndex(index, animation);
        if (status != ClockResult::Ok)
            throw std::runtime_error("DE model owner refused animation file index: " + std::to_string(static_cast<unsigned>(status)));
        return animation;
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
    bool CModelInfoDE::IsAllocatedInArchive() const
    {
        const auto index = model;
        bool       allocated = false;
        // Preserve the genuine named predicate, including successful false.
        // Owner refusal must remain catchable rather than terminate the client
        // or masquerade as absence and permit model-removal policy to continue.
        const auto status = access.ReadModelArchiveAllocated(index, allocated);
        if (status != ClockResult::Ok)
            throw std::runtime_error("DE model owner refused archive allocation: " + std::to_string(static_cast<unsigned>(status)));
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
