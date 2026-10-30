/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual MTA hierarchy reads and private initialization through one owner
 ****************************************************************************/
#pragma once
#include "NativeAnimationHierarchyAccess.h"
#include "NativeAnimationAssociationIssuedView.h"
#include <windows.h>
#include <game/CAnimBlendHierarchy.h>
#include <type_traits>

namespace mta::game_de
{
    class CAnimBlendHierarchyDE : public CAnimBlendHierarchy
    {
    public:
        void                            Initialize() override;
        void                            SetName(const char*) override;
        void                            SetSequences(CAnimBlendSequenceSAInterface*) override;
        void                            SetNumSequences(unsigned short) override;
        void                            SetRunningCompressed(bool) override;
        void                            SetAnimationBlockID(int) override;
        void                            RemoveAnimSequences() override;
        void                            RemoveFromUncompressedCache() override;
        void                            RemoveQuaternionFlips() override;
        void                            CalculateTotalTime() override;
        float                           GetTotalTime() const override;
        CAnimBlendSequenceSAInterface*  GetSequence(DWORD) override;
        CAnimBlendSequenceSAInterface*  GetSequences() override;
        unsigned short                  GetNumSequences() override;
        bool                            IsRunningCompressed() override;
        bool                            IsCustom() override;
        int                             GetAnimBlockID() override;
        CAnimBlendHierarchySAInterface* GetInterface() override;
        unsigned int                    GetNameHashKey() override;

        // Same-owner SDK factory routes create only this CPU borrow. The
        // association route receives a jointly qualified parent/bank token;
        // neither destructor retires native hierarchy or frame data.
        friend class CAnimManagerDE;
        friend class CAnimBlendAssociationDE;

    protected:
        CAnimBlendHierarchyDE(NativeAnimationHierarchyAccess& owner, const NativeAnimationHierarchyReferenceToken& token) noexcept
            : native(owner), reference(token)
        {
        }

        CAnimBlendHierarchyDE(NativeAnimationHierarchyAccess& owner, const NativeAnimationHierarchyReferenceToken& token,
                              NativeAnimationAssociationFacadeView&& borrowedCode) noexcept
            : native(owner), reference(token), code(std::move(borrowedCode))
        {
        }

    private:
        NativeAnimationHierarchyValues               Read(const char*) const;
        NativeAnimationHierarchyAccess&              native;
        const NativeAnimationHierarchyReferenceToken reference;
        // Association children may outlive their parent CPU facade. This
        // retains backend code only, not native H storage or a bank reference.
        NativeAnimationAssociationFacadeView code;
    };
    // All actual hierarchy operations dispatch to their qualified owner.
    // The complete method surface does not publish a factory or grant native
    // lifetime; construction remains protected and resource ownership explicit.
    static_assert(!std::is_abstract_v<CAnimBlendHierarchyDE> && std::is_base_of_v<CAnimBlendHierarchy, CAnimBlendHierarchyDE>);
    static_assert(!std::is_default_constructible_v<CAnimBlendHierarchyDE> &&
                  !std::is_constructible_v<CAnimBlendHierarchyDE, NativeAnimationHierarchyAccess&, const NativeAnimationHierarchyReferenceToken&>);
}
