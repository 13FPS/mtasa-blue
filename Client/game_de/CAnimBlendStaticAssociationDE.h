/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual MTA static-association reads over one borrowed physical row
 ****************************************************************************/
#pragma once
#include "NativeAnimationStaticAssociationAccess.h"
#include <game/CAnimBlendStaticAssociation.h>
#include <type_traits>
namespace mta::game_de
{
    class CAnimBlendStaticAssociationDE : public CAnimBlendStaticAssociation
    {
    public:
        unsigned short                          GetNumBlendNodes() override;
        short                                   GetAnimID() override;
        short                                   GetAnimGroup() override;
        short                                   GetFlags() override;
        CAnimBlendHierarchySAInterface*         GetAnimHierarchyInterface() override;
        CAnimBlendStaticAssociationSAInterface* GetInterface() override;

    protected:
        CAnimBlendStaticAssociationDE(NativeAnimationStaticAssociationAccess& owner, const NativeAnimationStaticAssociationReferenceToken& token) noexcept
            : native(owner), reference(token)
        {
        }

    private:
        NativeAnimationStaticAssociationValues               Read(const char*) const;
        NativeAnimationStaticAssociationAccess&              native;
        const NativeAnimationStaticAssociationReferenceToken reference;
    };
    // All five real initialization/setter contracts remain pure. There is no
    // public factory or deletion through the SDK base without a virtual dtor.
    static_assert(std::is_abstract_v<CAnimBlendStaticAssociationDE> && std::is_base_of_v<CAnimBlendStaticAssociation, CAnimBlendStaticAssociationDE>);
    static_assert(!std::is_default_constructible_v<CAnimBlendStaticAssociationDE>);
}
