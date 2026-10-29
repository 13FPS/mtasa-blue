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
        void                                    Initialize(RpClump*, CAnimBlendHierarchySAInterface*) override;
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
        friend class CAnimManagerDE;
        friend class CAnimBlendAssocGroupDE;
        NativeAnimationStaticAssociationValues               Read(const char*) const;
        NativeAnimationStaticAssociationAccess&              native;
        const NativeAnimationStaticAssociationReferenceToken reference;
    };
    // This receiver completes its eleven SDK methods through the same owner.
    // Its destructor frees the CPU facade only; unsupported initialization
    // phases remain explicit owner refusals rather than successful no-ops.
    static_assert(!std::is_abstract_v<CAnimBlendStaticAssociationDE> && std::is_base_of_v<CAnimBlendStaticAssociation, CAnimBlendStaticAssociationDE>);
    static_assert(std::has_virtual_destructor_v<CAnimBlendStaticAssociation>);
    static_assert(!std::is_default_constructible_v<CAnimBlendStaticAssociationDE>);
}
