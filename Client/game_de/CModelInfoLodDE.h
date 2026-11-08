/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Extend the same real model facade with MTA's distance/default policy
 ****************************************************************************/
#pragma once
#include "CModelInfoTimeDE.h"
#include "NativeModelLodAccess.h"
#include <type_traits>
#include <utility>
namespace mta::game_de
{
    namespace ModelLodSdk
    {
        NativeModelClassificationAccess& RequireSameOwner(NativeModelClassificationAccess&, NativeModelLodAccess&);
        float                            Get(NativeModelLodAccess&, std::uint32_t model);
        float                            GetOriginal(NativeModelLodAccess&, std::uint32_t model);
        void                             Set(NativeModelLodAccess&, std::uint32_t model, float distance, bool overrideMaxDistance);
    }
    template <class ModelBase>
    class CModelInfoLodMixinDE : public ModelBase
    {
        static_assert(std::is_base_of_v<CModelInfoDE, ModelBase>);

    public:
        float GetLODDistance() override { return ModelLodSdk::Get(lod, this->NativeModelIndex()); }
        float GetOriginalLODDistance() override { return ModelLodSdk::GetOriginal(lod, this->NativeModelIndex()); }
        void  SetLODDistance(float distance, bool overrideMaxDistance = false) override
        {
            ModelLodSdk::Set(lod, this->NativeModelIndex(), distance, overrideMaxDistance);
        }

    protected:
        template <class... Args>
        CModelInfoLodMixinDE(std::uint32_t model, NativeModelClassificationAccess& classification, NativeModelLodAccess& lodOwner, Args&&... args)
            : ModelBase(model, ModelLodSdk::RequireSameOwner(classification, lodOwner), std::forward<Args>(args)...), lod(lodOwner)
        {
        }

    private:
        NativeModelLodAccess& lod;
    };
    class CModelInfoLodDE : public CModelInfoLodMixinDE<CModelInfoTimeDE>
    {
    protected:
        using CModelInfoLodMixinDE<CModelInfoTimeDE>::CModelInfoLodMixinDE;
    };
    // These actual SDK methods add neither a complete model factory nor
    // collision, renderer or native allocation capability to the facade.
    static_assert(std::is_abstract_v<CModelInfoLodDE> && std::is_convertible_v<CModelInfoLodDE*, CModelInfo*>);
}
