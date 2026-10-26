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
    // Generic reference-cycle layer: removal/custom replacement/dependencies and
    // reporting stay mandatory. No complete model/game factory is published.
    class CModelInfoUnmodifiedDE;
    class CModelInfoReferenceDE : public CModelInfoRequestDE
    {
    protected:
        CModelInfoReferenceDE(std::uint32_t model, NativeModelClassificationAccess&, CGame&, NativeModelBackingReferenceAccess&,
                              const NativeModelBackingReferenceToken&);
        bool DoIsLoaded() final override;
        void IncrementNativeModelInterfaceReference() final override;

    private:
        friend class CModelInfoUnmodifiedDE;
        NativeModelBackingReferenceState   ReadReferenceState();
        void                               ClaimUnmodifiedFamily();
        void                               ReleaseUnmodifiedBackingReference();
        static void                        Require(ClockResult, const char*);
        NativeModelBackingReferenceAccess& referenceOwner;
        NativeModelBackingReferenceToken   reference;
    };
    static_assert(std::is_abstract_v<CModelInfoReferenceDE>);
}
