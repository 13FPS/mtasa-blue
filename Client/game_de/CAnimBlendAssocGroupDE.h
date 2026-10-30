/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual MTA association-group reads over one borrowed current row
 ****************************************************************************/
#pragma once
#include "NativeAnimationAssociationGroupAccess.h"
#include "NativeAnimationAssociationGroupChildBorrow.h"
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
        ~CAnimBlendAssocGroupDE() override = default;
        CAnimBlendAssociationSAInterface* CopyAnimation(unsigned int) override;
        void                              InitEmptyAssociations(RpClump*) override;
        CAnimBlock*                       GetAnimBlock() override;
        CAnimBlendStaticAssociation*      GetAnimation(unsigned int) override;
        bool                              IsCreated() override;
        int                               GetNumAnimations() override;
        eAnimGroup                        GetGroupID() override;
        void                              CreateAssociations(const char*) override;
        bool                              IsLoaded() override;
        void                              SetIDOffset(int) override;
        CAnimBlendAssocGroupSAInterface*  GetInterface() override;

    protected:
        // The source-order Create composer receives this exact CPU owner
        // to the replacement ledger; it is not a raw native release callback.
        NativeAnimationAssociationGroupChildBorrowInvalidator ChildBorrowInvalidator() noexcept;
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
        // The real same-owner manager creates only this borrowed CPU facade;
        // its public unique_ptr owns the CPU child caches, not native rows.
        friend class CAnimManagerDE;
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
        // with the group or entry into native child replacement. Every operation
        // still re-admits the native row.
        std::vector<StaticProduct> staticFacades;
    };
    // Same-owner manager factories own this CPU facade through the common
    // virtual destructor. Destruction retires only CPU child caches; the native
    // owner and explicit game must outlive the facade and its borrowed products.
    // Tokens re-admit native rows and grant no native allocation or GC lease.
    static_assert(!std::is_abstract_v<CAnimBlendAssocGroupDE> && std::is_base_of_v<CAnimBlendAssocGroup, CAnimBlendAssocGroupDE>);
    static_assert(!std::is_default_constructible_v<CAnimBlendAssocGroupDE>);
}
