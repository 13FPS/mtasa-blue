/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Real MTA operations on an explicitly issued private association
 ****************************************************************************/
#pragma once
#include "NativeAnimationAssociationAccess.h"
#include "NativeAnimationAssociationIssuedView.h"
// Keep the shared SDK's legacy opaque spelling available without bringing a
// classic RenderWare layout into this backend; the opaque spelling does not alter native layout.
struct RpClump;
#include <game/CAnimBlendAssociation.h>
#include <type_traits>
namespace mta::game_de
{
    class CAnimBlendAssociationDE : public CAnimBlendAssociation
    {
    public:
        using CAnimBlendAssociation::Constructor;
        CAnimBlendAssociationSAInterface*    Constructor(CAnimBlendStaticAssociationSAInterface&) override;
        CAnimBlendAssociationSAInterface*    Constructor(RpClump*, CAnimBlendHierarchySAInterface*) override;
        void                                 Init(RpClump*, CAnimBlendHierarchySAInterface*) override;
        CAnimBlendAssociationSAInterface*    InitializeForCustomAnimation(RpClump*, CAnimBlendHierarchySAInterface*) override;
        void                                 FreeAnimBlendNodeArray() override;
        CAnimBlendAssociationSAInterface*    GetInterface() override;
        eAnimGroup                           GetAnimGroup() override;
        eAnimID                              GetAnimID() override;
        std::unique_ptr<CAnimBlendHierarchy> GetAnimHierarchy() override;
        float                                GetBlendAmount() override;
        void                                 SetBlendAmount(float) override;
        bool                                 IsPartial() const override;
        void                                 RestrictToBonesOf(const CAnimBlendStaticAssociationSAInterface*) override;
        void                                 RestrictToBones(std::bitset<64>) override;
        void                                 SetCurrentProgress(float) override;
        float                                GetCurrentProgress() const override;
        void                                 SetCurrentSpeed(float) override;
        float                                GetCurrentSpeed() const override;
        float                                GetLength() const override;
        void                                 SetAnimID(short) override;
        void                                 SetAnimGroup(short) override;
        void                                 SetFlags(short) override;

        friend class CAnimManagerDE;

    protected:
        CAnimBlendAssociationDE(NativeAnimationAssociationAccess& owner, const NativeAnimationAssociationReferenceToken& token) noexcept
            : native(owner), reference(token)
        {
        }
        CAnimBlendAssociationDE(NativeAnimationAssociationAccess& owner, const NativeAnimationAssociationReferenceToken& token,
                                NativeAnimationAssociationFacadeView&& issuedCode) noexcept
            : native(owner), reference(token), code(std::move(issuedCode))
        {
        }
        // The CPU observer never constructs, frees, unlinks or releases the
        // native record from a destructor. Retirement is an explicit owner ACK.
        ~CAnimBlendAssociationDE() override = default;

    private:
        NativeAnimationAssociationValues               Read(const char*) const;
        NativeAnimationAssociationAccess&              native;
        const NativeAnimationAssociationReferenceToken reference;
        // This view fences CPU code only; it neither extends native ownership
        // nor invokes native retirement from a destructor.
        NativeAnimationAssociationFacadeView code;
    };
    // All22 operations use the actual issued owner. The matching manager
    // creates CPU products with causal issued identity and code lifetime;
    // protected construction still forbids generic raw-interface adoption.
    static_assert(!std::is_abstract_v<CAnimBlendAssociationDE> && std::is_base_of_v<CAnimBlendAssociation, CAnimBlendAssociationDE>);
}
