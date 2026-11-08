/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Compose the qualified startup model methods on one reference family
 ****************************************************************************/
#pragma once
#include "CModelInfoLodDE.h"
#include "CModelInfoUnmodifiedDE.h"

namespace mta::game_de
{
    // These methods must share the existing request counters, backing token and
    // Core reporter. Deriving from the separate partial facades would duplicate
    // CModelInfoDE and leave dummy loads disconnected from the reference family.
    class CModelInfoStartupDE
        : public CModelInfoLodMixinDE<
              CModelInfoTimeMixinDE<CModelInfoDummiesMixinDE<CModelInfoObjectGroupMixinDE<CModelInfoWheelMixinDE<CModelInfoUnmodifiedDE>>>>>
    {
        using ModelBase =
            CModelInfoLodMixinDE<CModelInfoTimeMixinDE<CModelInfoDummiesMixinDE<CModelInfoObjectGroupMixinDE<CModelInfoWheelMixinDE<CModelInfoUnmodifiedDE>>>>>;

    protected:
        CModelInfoStartupDE(std::uint32_t model, NativeModelClassificationAccess& classification, NativeModelLodAccess& lod, NativeModelTimeAccess& time,
                            NativeModelWheelAccess& wheel, NativeModelObjectGroupAccess& groups, NativeModelDummiesAccess& dummies, CGame& game,
                            NativeModelBackingReferenceAccess& referenceOwner, const NativeModelBackingReferenceToken& reference, CCoreInterface& core)
            : ModelBase(model, classification, lod, time, wheel, groups, dummies, game, referenceOwner, reference, core)
        {
            // Every mutation mixin checks its classification-owner affinity
            // before the inner reference layer can bind the issued token. That
            // layer also validates the complete backing owner and model key;
            // reuse those checks instead of introducing another token or owner.
        }
    };
    // The unmodified-family replacement refusals stay final. Remaining model
    // methods are still mandatory; composition does not publish a model factory.
    static_assert(std::is_abstract_v<CModelInfoStartupDE> && std::is_convertible_v<CModelInfoStartupDE*, CModelInfoDE*> &&
                  std::is_base_of_v<CModelInfoUnmodifiedDE, CModelInfoStartupDE>);
}
