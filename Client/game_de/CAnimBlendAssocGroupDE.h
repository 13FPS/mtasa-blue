/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual MTA association-group reads over one borrowed current row
 ****************************************************************************/
#pragma once
#include "NativeAnimationAssociationGroupAccess.h"
#include <cstddef>
#include <game/CAnimBlendAssocGroup.h>
#include <game/CAnimBlock.h>
#include <game/CAnimBlendStaticAssociation.h>
#include <memory>
#include <vector>
#include <type_traits>
class CGame;
namespace mta::game_de
{
    class CAnimBlendAssocGroupDE : public CAnimBlendAssocGroup
    {
    public:
        CAnimBlendAssociationSAInterface* CopyAnimation(unsigned int) override;
        void                              InitEmptyAssociations(RpClump*) override;
        CAnimBlock*                       GetAnimBlock() override;
        CAnimBlendStaticAssociation*      GetAnimation(unsigned int) override;
        bool                              IsCreated() override;
        int                               GetNumAnimations() override;
        eAnimGroup                        GetGroupID() override;
        bool                              IsLoaded() override;
        void                              SetIDOffset(int) override;
        CAnimBlendAssocGroupSAInterface*  GetInterface() override;

    protected:
        CAnimBlendAssocGroupDE(NativeAnimationAssociationGroupAccess& owner, const NativeAnimationAssociationGroupReferenceToken& token) noexcept
            : native(owner), reference(token)
        {
        }
        // Preserve the observation-only constructor. Publishing a nonnull
        // block facade additionally requires the existing explicit game used
        // by CAnimBlockRequestDE; no global or second provider is invented.
        CAnimBlendAssocGroupDE(NativeAnimationAssociationGroupAccess& owner, const NativeAnimationAssociationGroupReferenceToken& token, CGame& game) noexcept
            : CAnimBlendAssocGroupDE(owner, token)
        {
            gameProvider = &game;
        }

    private:
        NativeAnimationAssociationGroupValues               Read(const char*) const;
        NativeAnimationAssociationGroupAccess&              native;
        const NativeAnimationAssociationGroupReferenceToken reference;
        // Both optional child services come from this exact complete owner.
        NativeAnimationBlockAccess*             blockAccess = dynamic_cast<NativeAnimationBlockAccess*>(&native);
        NativeAnimationStaticAssociationAccess* staticAccess = dynamic_cast<NativeAnimationStaticAssociationAccess*>(&native);
        CGame*                                  gameProvider = nullptr;
        NativeAnimationAssociationGroupBlock    blockIdentity{};
        std::unique_ptr<CAnimBlock>             blockFacade;
        struct StaticProduct
        {
            NativeAnimationStaticAssociationReferenceToken token;
            std::unique_ptr<CAnimBlendStaticAssociation>   facade;
        };
        // SDK raw results borrow CPU facades owned by this group. Block
        // borrows expire on source pointer refresh/null; static borrows expire
        // with the group. Every operation still re-admits the native row.
        std::vector<StaticProduct> staticFacades;
    };
    // Genuine CreateAssociations remains
    // pure. Child CPU products do not publish a complete group factory or
    // justify deletion through the still-unmodified common group base.
    static_assert(std::is_abstract_v<CAnimBlendAssocGroupDE> && std::is_base_of_v<CAnimBlendAssocGroup, CAnimBlendAssocGroupDE>);
    static_assert(!std::is_default_constructible_v<CAnimBlendAssocGroupDE>);
}
