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
        void                                    SetNumBlendNodes(unsigned short) override;
        void                                    SetAnimID(short) override;
        void                                    SetAnimGroup(short) override;
        void                                    SetFlags(short) override;
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
    // Initialize still needs genuine clump/node/hierarchy authority and stays
    // pure. Scalar writes grant no child ownership or complete factory; no
    // deletion through the SDK base without a virtual dtor is introduced.
    static_assert(std::is_abstract_v<CAnimBlendStaticAssociationDE> && std::is_base_of_v<CAnimBlendStaticAssociation, CAnimBlendStaticAssociationDE>);
    static_assert(!std::is_default_constructible_v<CAnimBlendStaticAssociationDE>);
}
