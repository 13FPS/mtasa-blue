/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual MTA association-group reads over one borrowed current row
 ****************************************************************************/
#pragma once
#include "NativeAnimationAssociationGroupAccess.h"
#include <game/CAnimBlendAssocGroup.h>
#include <type_traits>
namespace mta::game_de
{
    class CAnimBlendAssocGroupDE : public CAnimBlendAssocGroup
    {
    public:
        bool                             IsCreated() override;
        int                              GetNumAnimations() override;
        eAnimGroup                       GetGroupID() override;
        bool                             IsLoaded() override;
        void                             SetIDOffset(int) override;
        CAnimBlendAssocGroupSAInterface* GetInterface() override;

    protected:
        CAnimBlendAssocGroupDE(NativeAnimationAssociationGroupAccess& owner, const NativeAnimationAssociationGroupReferenceToken& token) noexcept
            : native(owner), reference(token)
        {
        }

    private:
        NativeAnimationAssociationGroupValues               Read(const char*) const;
        NativeAnimationAssociationGroupAccess&              native;
        const NativeAnimationAssociationGroupReferenceToken reference;
    };
    // The remaining genuine copy/create/initialization/static-association
    // operations stay pure. No group factory or SDK-base deletion is
    // introduced by these immediate reads of a borrowed current owner.
    static_assert(std::is_abstract_v<CAnimBlendAssocGroupDE> && std::is_base_of_v<CAnimBlendAssocGroup, CAnimBlendAssocGroupDE>);
    static_assert(!std::is_default_constructible_v<CAnimBlendAssocGroupDE>);
}
