/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Existing SDK wheel methods over the same qualified native owner
 *****************************************************************************/
#pragma once
#include "CModelInfoDE.h"
#include "NativeModelWheelAccess.h"
#include <type_traits>
#include <utility>

namespace mta::game_de
{
    namespace ModelWheelSdk
    {
        NativeModelClassificationAccess& RequireSameOwner(NativeModelClassificationAccess&, NativeModelWheelAccess&);
        float                            Get(NativeModelWheelAccess&, std::uint32_t, ResizableVehicleWheelGroup::Enum);
        void                             Set(NativeModelWheelAccess&, std::uint32_t, ResizableVehicleWheelGroup::Enum, float);
        void                             Reset(NativeModelWheelAccess&, std::uint32_t, const std::pair<float, float>*);
    }
    template <class ModelBase>
    class CModelInfoWheelMixinDE : public ModelBase
    {
        static_assert(std::is_base_of_v<CModelInfoDE, ModelBase>);

    public:
        float GetVehicleWheelSize(ResizableVehicleWheelGroup::Enum group) override { return ModelWheelSdk::Get(wheel, CModelInfoDE::GetModel(), group); }
        void  SetVehicleWheelSize(ResizableVehicleWheelGroup::Enum group, float value) override
        {
            ModelWheelSdk::Set(wheel, CModelInfoDE::GetModel(), group, value);
        }
        void ResetVehicleWheelSizes(std::pair<float, float>* defaults = nullptr) override { ModelWheelSdk::Reset(wheel, CModelInfoDE::GetModel(), defaults); }

    protected:
        template <class... Args>
        CModelInfoWheelMixinDE(std::uint32_t model, NativeModelClassificationAccess& classification, NativeModelWheelAccess& wheelOwner, Args&&... args)
            : ModelBase(model, ModelWheelSdk::RequireSameOwner(classification, wheelOwner), std::forward<Args>(args)...), wheel(wheelOwner)
        {
        }

    private:
        NativeModelWheelAccess& wheel;
    };
    class CModelInfoWheelDE : public CModelInfoWheelMixinDE<CModelInfoDE>
    {
    protected:
        using CModelInfoWheelMixinDE<CModelInfoDE>::CModelInfoWheelMixinDE;
    };
    static_assert(std::is_abstract_v<CModelInfoWheelDE> && std::is_convertible_v<CModelInfoWheelDE*, CModelInfo*>);
}
