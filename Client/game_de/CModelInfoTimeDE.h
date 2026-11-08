/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Add actual timed-model writes to the same composed SDK receiver
 ****************************************************************************/
#pragma once
#include "CModelInfoDummiesDE.h"
#include "NativeModelTimeAccess.h"
#include <type_traits>
#include <utility>
namespace mta::game_de
{
    namespace ModelTimeSdk
    {
        NativeModelClassificationAccess& RequireSameOwner(NativeModelClassificationAccess&, NativeModelTimeAccess&);
        bool                             Set(NativeModelTimeAccess&, std::uint32_t model, char on, char off);
    }
    template <class ModelBase>
    class CModelInfoTimeMixinDE : public ModelBase
    {
        static_assert(std::is_base_of_v<CModelInfoDE, ModelBase>);

    public:
        bool SetTime(char on, char off) override { return ModelTimeSdk::Set(time, this->NativeModelIndex(), on, off); }

    protected:
        template <class... Args>
        CModelInfoTimeMixinDE(std::uint32_t model, NativeModelClassificationAccess& classification, NativeModelTimeAccess& timeOwner, Args&&... args)
            : ModelBase(model, ModelTimeSdk::RequireSameOwner(classification, timeOwner), std::forward<Args>(args)...), time(timeOwner)
        {
        }

    private:
        NativeModelTimeAccess& time;
    };
    class CModelInfoTimeDE : public CModelInfoTimeMixinDE<CModelInfoDummiesDE>
    {
    protected:
        using CModelInfoTimeMixinDE<CModelInfoDummiesDE>::CModelInfoTimeMixinDE;
    };
    // An implemented setter does not publish the unfinished model factory or
    // grant collision, renderer, creation or borrowed lifetime capabilities.
    static_assert(std::is_abstract_v<CModelInfoTimeDE> && std::is_convertible_v<CModelInfoTimeDE*, CModelInfo*>);
}
