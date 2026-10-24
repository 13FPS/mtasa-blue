/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Preserve sequence scalar widths and exact current-owner refusals
 ****************************************************************************/
#include "CAnimBlendSequenceDE.h"
#include <stdexcept>
#include <string>

namespace mta::game_de
{
    namespace
    {
        void Require(ClockResult result, const char* operation)
        {
            if (result != ClockResult::Ok)
                throw std::runtime_error(std::string("DE custom animation sequence owner refused ") + operation + ": " +
                                         std::to_string(static_cast<unsigned>(result)));
        }
    }

    NativeAnimationSequenceValues CAnimBlendSequenceDE::Read(const char* operation) const
    {
        const auto                    token = reference;
        NativeAnimationSequenceValues value;
        Require(native.ReadAnimationSequence(token, value), operation);
        return value;
    }

    void CAnimBlendSequenceDE::Initialize()
    {
        // The same owner changes the physical row and its logical union.
        // A facade cannot restore either half after an entered failure.
        const auto token = reference;
        Require(native.InitializeAnimationSequence(token), "Initialize");
    }

    void CAnimBlendSequenceDE::SetBoneTag(std::int32_t value)
    {
        // Preserve full-width -1 versus 65535; only the qualified owner
        // applies the original no-op and modulo-WORD assignment policy.
        const auto token = reference;
        Require(native.SetAnimationSequenceBoneTag(token, value), "SetBoneTag");
    }

    std::uint32_t CAnimBlendSequenceDE::GetHash()
    {
        const auto    token = reference;
        std::uint32_t value = 0;
        Require(native.ReadAnimationSequenceHash(token, value), "GetHash");
        return value;
    }

    std::uint16_t CAnimBlendSequenceDE::GetBoneTag()
    {
        // Actual MTA reads the logical union's low WORD even after SetName.
        // The owner supplies that state, not DE's separate physical bone.
        return Read("GetBoneTag").boneTag;
    }

    BYTE* CAnimBlendSequenceDE::GetKeyFrames()
    {
        return reinterpret_cast<BYTE*>(Read("GetKeyFrames").keyFrames);
    }

    unsigned short CAnimBlendSequenceDE::GetKeyFramesCount()
    {
        static_assert(sizeof(unsigned short) == sizeof(std::uint16_t));
        return Read("GetKeyFramesCount").frameCount;
    }

    bool CAnimBlendSequenceDE::IsBigChunkForAllSequences()
    {
        return Read("IsBigChunkForAllSequences").bigChunk;
    }

    CAnimBlendSequenceSAInterface* CAnimBlendSequenceDE::GetInterface()
    {
        // This legacy pointer is only a same-backend opaque row locator.
        // It neither applies SA's layout nor extends the current borrow.
        return reinterpret_cast<CAnimBlendSequenceSAInterface*>(Read("GetInterface").interfaceAddress);
    }
    void CAnimBlendSequenceDE::CopySequenceProperties(CAnimBlendSequenceSAInterface* source)
    {
        // The same owner admits both current rows and transfers its logical
        // metadata after accepted stores. Never adopt or free aliased frames.
        const auto token = reference;
        const auto locator = reinterpret_cast<std::uintptr_t>(source);
        Require(native.CopyAnimationSequenceProperties(token, locator), "CopySequenceProperties");
    }

    void* CAnimBlendSequenceDE::GetKeyFrame(std::size_t index, std::uint32_t stride)
    {
        // Preserve source byte-arithmetic inputs in full, including zero
        // stride with a large index. Only the owner knows the current extent.
        const auto     token = reference;
        const auto     frameIndex = index;
        const auto     frameStride = stride;
        std::uintptr_t value = 0;
        Require(native.ReadAnimationSequenceFrame(token, frameIndex, frameStride, value), "GetKeyFrame");
        return reinterpret_cast<void*>(value);
    }

}
