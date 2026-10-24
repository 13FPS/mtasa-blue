/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual MTA sequence operations over one current issued custom row
 ****************************************************************************/
#pragma once
#include "NativeAnimationSequenceAccess.h"
#include <windows.h>
#include <game/CAnimBlendSequence.h>
#include <type_traits>

namespace mta::game_de
{
    class CAnimBlendSequenceDE : public CAnimBlendSequence
    {
    public:
        void                           Initialize() override;
        void                           SetBoneTag(std::int32_t) override;
        void                           CopySequenceProperties(CAnimBlendSequenceSAInterface*) override;
        void*                          GetKeyFrame(std::size_t, std::uint32_t) override;
        std::uint32_t                  GetHash() override;
        std::uint16_t                  GetBoneTag() override;
        BYTE*                          GetKeyFrames() override;
        unsigned short                 GetKeyFramesCount() override;
        bool                           IsBigChunkForAllSequences() override;
        CAnimBlendSequenceSAInterface* GetInterface() override;

    protected:
        CAnimBlendSequenceDE(NativeAnimationSequenceAccess& value, const NativeAnimationSequenceReferenceToken& token) noexcept
            : native(value), reference(token)
        {
        }

    private:
        NativeAnimationSequenceValues               Read(const char*) const;
        NativeAnimationSequenceAccess&              native;
        const NativeAnimationSequenceReferenceToken reference;
    };

    // General name/frame assignment remain genuine pure operations. These
    // qualified row operations do not expose a complete custom factory.
    static_assert(std::is_abstract_v<CAnimBlendSequenceDE> && std::is_base_of_v<CAnimBlendSequence, CAnimBlendSequenceDE>);
}
