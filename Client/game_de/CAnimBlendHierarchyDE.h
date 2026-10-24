/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual MTA hierarchy reads through one current bank owner
 ****************************************************************************/
#pragma once
#include "NativeAnimationHierarchyAccess.h"
#include <windows.h>
#include <game/CAnimBlendHierarchy.h>
#include <type_traits>

namespace mta::game_de
{
    class CAnimBlendHierarchyDE : public CAnimBlendHierarchy
    {
    public:
        float                           GetTotalTime() const override;
        CAnimBlendSequenceSAInterface*  GetSequence(DWORD) override;
        CAnimBlendSequenceSAInterface*  GetSequences() override;
        unsigned short                  GetNumSequences() override;
        bool                            IsRunningCompressed() override;
        bool                            IsCustom() override;
        int                             GetAnimBlockID() override;
        CAnimBlendHierarchySAInterface* GetInterface() override;
        unsigned int                    GetNameHashKey() override;

    protected:
        CAnimBlendHierarchyDE(NativeAnimationHierarchyAccess& owner, const NativeAnimationHierarchyReferenceToken& token) noexcept
            : native(owner), reference(token)
        {
        }

    private:
        NativeAnimationHierarchyValues               Read(const char*) const;
        NativeAnimationHierarchyAccess&              native;
        const NativeAnimationHierarchyReferenceToken reference;
    };
    // All ten mutating operations retain their genuine pure contracts. A
    // checked bank borrow is not a custom hierarchy or a complete factory.
    static_assert(std::is_abstract_v<CAnimBlendHierarchyDE> && std::is_base_of_v<CAnimBlendHierarchy, CAnimBlendHierarchyDE>);
}
