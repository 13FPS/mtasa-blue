/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Compose real dummy loading/default/reference policy on the MTA model
 *****************************************************************************/
#pragma once
#include "CModelInfoObjectGroupDE.h"
#include "NativeModelDummiesAccess.h"

namespace mta::game_de
{
    namespace ModelDummiesSdk
    {
        NativeModelClassificationAccess& RequireSameOwner(NativeModelClassificationAccess&, NativeModelDummiesAccess&);
        void                             Require(ClockResult, const char*);
        std::uint32_t                    Index(VehicleDummies::Enum);
        CVector                          Decode(const NativeModelDummyVector&) noexcept;
        NativeModelDummyVector           Encode(const CVector&) noexcept;
    }
    template <class ModelBase>
    class CModelInfoDummiesMixinDE : public ModelBase
    {
        static_assert(std::is_base_of_v<CModelInfoDE, ModelBase>);
        static_assert(static_cast<std::size_t>(VehicleDummies::VEHICLE_DUMMY_COUNT) == NativeModelDummyPositions{}.size());

    public:
        CVector GetVehicleExhaustFumesPosition() override { return GetVehicleDummyPosition(VehicleDummies::EXHAUST); }
        void    SetVehicleExhaustFumesPosition(const CVector& position) override { SetVehicleDummyPosition(VehicleDummies::EXHAUST, position); }
        CVector GetVehicleDummyDefaultPosition(VehicleDummies::Enum selected) override
        {
            if (!this->IsVehicle())
                return CVector();
            const auto             index = ModelDummiesSdk::Index(selected);
            NativeModelDummyVector original;
            bool                   found = false;
            ModelDummiesSdk::Require(dummies.ReadCachedModelVehicleDummyDefault(CModelInfoDE::NativeModelIndex(), index, original, found),
                                     "GetVehicleDummyDefaultPosition");
            if (found)
                return ModelDummiesSdk::Decode(original);
            // Preserve the actual wrapper's temporary ref/load policy. A read
            // failure retains its admitted reference prefix with that owner;
            // it never guesses an automatic release during exception cleanup.
            this->ModelAddRef(BLOCKING, "GetVehicleDummyDefaultPosition");
            NativeModelDummyPositions positions;
            ModelDummiesSdk::Require(dummies.ReadModelVehicleDummyPositions(CModelInfoDE::NativeModelIndex(), positions), "GetVehicleDummyDefaultPosition");
            const CVector result = ModelDummiesSdk::Decode(positions[index]);
            this->RemoveRef();
            return result;
        }
        CVector GetVehicleDummyPosition(VehicleDummies::Enum selected) override
        {
            if (!this->IsVehicle())
                return CVector();
            const auto index = ModelDummiesSdk::Index(selected);
            if (!this->IsLoaded())
                this->Request(BLOCKING, "GetVehicleDummyPosition");
            NativeModelDummyPositions positions;
            ModelDummiesSdk::Require(dummies.ReadModelVehicleDummyPositions(CModelInfoDE::NativeModelIndex(), positions), "GetVehicleDummyPosition");
            return ModelDummiesSdk::Decode(positions[index]);
        }
        bool GetVehicleDummyPositions(std::array<CVector, static_cast<std::size_t>(VehicleDummies::VEHICLE_DUMMY_COUNT)>& output) const override
        {
            if (!this->IsVehicle())
                return false;
            NativeModelDummyPositions positions;
            ModelDummiesSdk::Require(dummies.ReadModelVehicleDummyPositions(CModelInfoDE::NativeModelIndex(), positions), "GetVehicleDummyPositions");
            for (std::size_t n = 0; n < output.size(); ++n)
                output[n] = ModelDummiesSdk::Decode(positions[n]);
            return true;
        }
        void SetVehicleDummyPosition(VehicleDummies::Enum selected, const CVector& position) override
        {
            if (!this->IsVehicle())
                return;
            const auto index = ModelDummiesSdk::Index(selected);
            if (!this->IsLoaded())
                this->Request(BLOCKING, "SetVehicleDummyPosition");
            // Cache and first-model hold live with Runtime, so dropping or
            // reopening this partial facade cannot orphan modified defaults.
            ModelDummiesSdk::Require(dummies.SetModelVehicleDummyPosition(CModelInfoDE::NativeModelIndex(), index, ModelDummiesSdk::Encode(position)),
                                     "SetVehicleDummyPosition");
        }
        void ResetVehicleDummies(bool remove) override
        {
            if (this->IsVehicle())
                ModelDummiesSdk::Require(dummies.ResetModelVehicleDummies(CModelInfoDE::NativeModelIndex(), remove), "ResetVehicleDummies");
        }

    protected:
        template <class... Args>
        CModelInfoDummiesMixinDE(std::uint32_t model, NativeModelClassificationAccess& classification, NativeModelWheelAccess& wheel,
                                 NativeModelObjectGroupAccess& groups, NativeModelDummiesAccess& owner, Args&&... args)
            : ModelBase(model, ModelDummiesSdk::RequireSameOwner(classification, owner), wheel, groups, std::forward<Args>(args)...), dummies(owner)
        {
        }

    private:
        NativeModelDummiesAccess& dummies;
    };
    class CModelInfoDummiesDE : public CModelInfoDummiesMixinDE<CModelInfoObjectGroupDE>
    {
    protected:
        using CModelInfoDummiesMixinDE<CModelInfoObjectGroupDE>::CModelInfoDummiesMixinDE;
    };
    static_assert(std::is_abstract_v<CModelInfoDummiesDE> && std::is_convertible_v<CModelInfoDummiesDE*, CModelInfo*>);
}
