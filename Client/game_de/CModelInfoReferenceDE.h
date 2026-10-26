/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Bind actual model request policy to an issued backing-reference owner
 *****************************************************************************/
#pragma once
#include "CModelInfoRequestDE.h"
#include "NativeModelBackingReferenceAccess.h"

namespace mta::game_de
{
    // Acquire-only partial layer: removal/custom replacement/dependencies and
    // reporting stay mandatory. No complete model/game factory is published.
    class CModelInfoReferenceDE : public CModelInfoRequestDE
    {
    protected:
        CModelInfoReferenceDE(std::uint32_t model, NativeModelClassificationAccess&, CGame&, NativeModelBackingReferenceAccess&,
                              const NativeModelBackingReferenceToken&);
        bool DoIsLoaded() final override;
        void IncrementNativeModelInterfaceReference() final override;

    private:
        static void                            Require(ClockResult, const char*);
        NativeModelBackingReferenceAccess&     referenceOwner;
        const NativeModelBackingReferenceToken reference;
    };
    static_assert(std::is_abstract_v<CModelInfoReferenceDE>);
}
