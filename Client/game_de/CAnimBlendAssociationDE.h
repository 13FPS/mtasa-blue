/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Real MTA operations on an explicitly issued private association
 ****************************************************************************/
#pragma once
#include "NativeAnimationAssociationAccess.h"
// Keep the shared SDK's legacy opaque spelling available without bringing a
// classic RenderWare layout into this backend or changing the common ABI.
struct RpClump;
#include <game/CAnimBlendAssociation.h>
#include <type_traits>
namespace mta::game_de
{
    class CAnimBlendAssociationDE : public CAnimBlendAssociation
    {
    public:
        using CAnimBlendAssociation::Constructor;
        CAnimBlendAssociationSAInterface* Constructor(CAnimBlendStaticAssociationSAInterface&) override;
        CAnimBlendAssociationSAInterface* GetInterface() override;
        eAnimGroup                        GetAnimGroup() override;
        eAnimID                           GetAnimID() override;
        float                             GetBlendAmount() override;
        void                              SetBlendAmount(float) override;
        bool                              IsPartial() const override;
        void                              SetCurrentSpeed(float) override;
        void                              SetAnimID(short) override;
        void                              SetAnimGroup(short) override;
        void                              SetFlags(short) override;

    protected:
        CAnimBlendAssociationDE(NativeAnimationAssociationAccess& owner, const NativeAnimationAssociationReferenceToken& token) noexcept
            : native(owner), reference(token)
        {
        }
        // The CPU observer never constructs, frees, unlinks or releases the
        // native record from a destructor. Retirement is an explicit owner ACK.
        ~CAnimBlendAssociationDE() = default;

    private:
        NativeAnimationAssociationValues               Read(const char*) const;
        NativeAnimationAssociationAccess&              native;
        const NativeAnimationAssociationReferenceToken reference;
    };
    // Eleven real operations are supported; the other eleven retain their
    // genuine pure contracts, including clump Init, node disposal, hierarchy
    // factory and all three noexcept getters. No public factory/base delete.
    static_assert(std::is_abstract_v<CAnimBlendAssociationDE> && std::is_base_of_v<CAnimBlendAssociation, CAnimBlendAssociationDE>);
}
