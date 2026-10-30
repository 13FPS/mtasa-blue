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
    bool CModelInfoDE::IsPlayerModel()
    {
        const auto index = model;
        bool       value = false;
        const auto status = access.ReadModelPlayerPredicate(index, value);
        if (status != ClockResult::Ok)
            throw std::runtime_error("DE model owner refused IsPlayerModel: " + std::to_string(static_cast<unsigned>(status)));
        // Keep the actual MTA predicate in its current qualified owner. An
        // opaque interface cannot be cast to the classic collision layout.
        return value;
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
    void CModelInfoDE::CreateAndPublishPedModelMetadata(std::uint32_t modelIndex)
    {
        const auto index = modelIndex;
        const auto status = access.CreateAndPublishPedModelMetadata(index);
        // Preserve the existing missing-provider refusal before the first
        // streaming lookup. Other owner failures retain their exact status;
        // neither path may turn an accepted native prefix into a retry.
        if (status == ClockResult::NotInitialized)
            throw std::runtime_error("DE MakePedModel requires qualified native ped metadata creation and bank publication");
        if (status != ClockResult::Ok)
            throw std::runtime_error("DE model owner refused ped metadata creation/publication: " + std::to_string(static_cast<unsigned>(status)));
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
    BYTE CModelInfoDE::GetVehicleType() const
    {
        // Preserve the source archive/type gate before reading a vehicle
        // field. A successful nonvehicle result owns the 0xff sentinel;
        // owner refusal must not masquerade as that ordinary result.
        if (!IsVehicle())
            return static_cast<BYTE>(0xff);

        const auto    index = model;
        std::uint32_t subtype = 0;
        const auto    status = access.ReadModelVehicleSubtype(index, subtype);
        if (status != ClockResult::Ok)
            throw std::runtime_error("DE model owner refused vehicle subtype: " + std::to_string(static_cast<unsigned>(status)));
        // Genuine classifiers prove the raw DWORD+84 role. The SDK's
        // BYTE contract consumes its low byte without inventing names
        // or clamping subtype values outside those four classifiers.
        return static_cast<BYTE>(subtype);
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
    // The named IDE producer independently qualifies these policies over
    // current native type/subtype storage; the existing owner refuses gaps.
    bool CModelInfoDE::IsTrain()
    {
        return Query(NativeModelVehiclePredicate::Train);
    }
    bool CModelInfoDE::IsBike()
    {
        return Query(NativeModelVehiclePredicate::Bike);
    }
    bool CModelInfoDE::IsBmx()
    {
        return Query(NativeModelVehiclePredicate::Bmx);
    }
    bool CModelInfoDE::IsTrailer()
    {
        return Query(NativeModelVehiclePredicate::Trailer);
    }
    bool CModelInfoDE::IsMonsterTruck()
    {
        return Query(NativeModelVehiclePredicate::MonsterTruck);
    }
    bool CModelInfoDE::IsQuadBike()
    {
        return Query(NativeModelVehiclePredicate::QuadBike);
    }
    bool CModelInfoDE::IsFakePlane()
    {
        return Query(NativeModelVehiclePredicate::FakePlane);
    }

}
