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
        void                           SetName(const char*) override;
        void                           SetBoneTag(std::int32_t) override;
        void                           SetKeyFrames(std::size_t, bool, bool, void*) override;
        void                           CopySequenceProperties(CAnimBlendSequenceSAInterface*) override;
        void*                          GetKeyFrame(std::size_t, std::uint32_t) override;
        std::uint32_t                  GetHash() override;
        std::uint16_t                  GetBoneTag() override;
        BYTE*                          GetKeyFrames() override;
        unsigned short                 GetKeyFramesCount() override;
        bool                           IsBigChunkForAllSequences() override;
        CAnimBlendSequenceSAInterface* GetInterface() override;

        // Only the actual same-owner manager factory can create this CPU
        // facade. Its destructor never retires native hierarchy/frame data.
        friend class CAnimManagerDE;

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

    // SetKeyFrames supports only qualified fresh supplied-buffer construction;
    // repeated/unknown/native-allocation domains explicitly refuse. This does
    // not reconstruct the original general compression predicate or publish a
    // factory. The complete method surface keeps construction protected.
    static_assert(!std::is_abstract_v<CAnimBlendSequenceDE> && std::is_base_of_v<CAnimBlendSequence, CAnimBlendSequenceDE>);
    static_assert(!std::is_default_constructible_v<CAnimBlendSequenceDE> &&
                  !std::is_constructible_v<CAnimBlendSequenceDE, NativeAnimationSequenceAccess&, const NativeAnimationSequenceReferenceToken&>);
}
