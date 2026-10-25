/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual animation-block metadata through one borrowed native owner
 ****************************************************************************/
#pragma once
#include "NativeAnimationBlockAccess.h"
#include <cstddef>
#include <game/CAnimBlock.h>
#include <type_traits>
namespace mta::game_de
{
    class CAnimBlockDE : public CAnimBlock
    {
    public:
        CAnimBlockSAInterface*          GetInterface() override;
        CAnimBlendHierarchySAInterface* GetAnimationHierarchyInterface(std::size_t) override;
        char*                           GetName() override;
        int                             GetIndex() override;
        void                            AddRef() override;
        unsigned short                  GetRefs() override;
        bool                            IsLoaded() override;
        int                             GetIDOffset() override;
        std::size_t                     GetAnimationCount() override;

    protected:
        CAnimBlockDE(NativeAnimationBlockAccess&, const NativeAnimationBlockIdentity&);

    private:
        // The same-owner manager reads this immutable CPU affinity directly;
        // a virtual GetInterface call cannot certify a hash-search lifetime.
        friend class CAnimManagerDE;
        NativeAnimationBlockValues         Read(const char* operation) const;
        NativeAnimationBlockAccess&        native;
        const NativeAnimationBlockIdentity identity;
    };
    // Legacy pointer types are opaque same-backend locators, never SA layout
    // promises or native leases. Request still requires its real game provider;
    // the base remains abstract rather than fabricating that dependency.
    static_assert(std::is_abstract_v<CAnimBlockDE> && std::is_base_of_v<CAnimBlock, CAnimBlockDE>);
}
