/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Compose object-group SDK methods on the same wheel/model facade
 *****************************************************************************/
#pragma once
#include "CModelInfoWheelDE.h"
#include "NativeModelObjectGroupAccess.h"
#include <type_traits>
#include <utility>

namespace mta::game_de
{
    namespace ModelObjectGroupSdk
    {
        NativeModelClassificationAccess& RequireSameOwner(NativeModelClassificationAccess&, NativeModelObjectGroupAccess&);
        std::uint16_t                    Get(NativeModelObjectGroupAccess&, std::uint32_t);
        void                             Set(NativeModelObjectGroupAccess&, std::uint32_t, std::uint16_t);
        void                             Restore(NativeModelObjectGroupAccess&, std::uint32_t);
    }
    template <class ModelBase>
    class CModelInfoObjectGroupMixinDE : public ModelBase
    {
        static_assert(std::is_base_of_v<CModelInfoDE, ModelBase>);

    public:
        unsigned short GetObjectPropertiesGroup() override { return ModelObjectGroupSdk::Get(groups, CModelInfoDE::GetModel()); }
        void           SetObjectPropertiesGroup(unsigned short value) override { ModelObjectGroupSdk::Set(groups, CModelInfoDE::GetModel(), value); }
        void           RestoreObjectPropertiesGroup() override { ModelObjectGroupSdk::Restore(groups, CModelInfoDE::GetModel()); }

    protected:
        template <class... Args>
        CModelInfoObjectGroupMixinDE(std::uint32_t model, NativeModelClassificationAccess& classification, NativeModelWheelAccess& wheelOwner,
                                     NativeModelObjectGroupAccess& groupOwner, Args&&... args)
            : ModelBase(model, ModelObjectGroupSdk::RequireSameOwner(classification, groupOwner), wheelOwner, std::forward<Args>(args)...), groups(groupOwner)
        {
        }

    private:
        NativeModelObjectGroupAccess& groups;
    };
    class CModelInfoObjectGroupDE : public CModelInfoObjectGroupMixinDE<CModelInfoWheelDE>
    {
    protected:
        using CModelInfoObjectGroupMixinDE<CModelInfoWheelDE>::CModelInfoObjectGroupMixinDE;
    };
    static_assert(std::is_abstract_v<CModelInfoObjectGroupDE> && std::is_convertible_v<CModelInfoObjectGroupDE*, CModelInfo*>);
}
