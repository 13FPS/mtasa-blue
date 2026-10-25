/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Owner acknowledgements for detached sequence construction/transfer
 ****************************************************************************/
#pragma once
#include <cstddef>
class CAnimManager;
class CAnimBlendSequence;

enum class ECustomAnimationSequenceStatus
{
    Pending,
    Refused,
    Complete
};
enum class ECustomAnimationSequenceDisposition
{
    Unresolved,
    FramesAttached,
    SourceReleased,
    SourceTransferred
};
struct SCustomAnimationSequenceResult
{
    ECustomAnimationSequenceStatus      status = ECustomAnimationSequenceStatus::Pending;
    ECustomAnimationSequenceDisposition disposition = ECustomAnimationSequenceDisposition::Unresolved;
};
struct SCustomAnimationSequenceCleanupRequest
{
    CAnimManager*       origin = nullptr;
    CAnimBlendSequence* source = nullptr;
    // Classic factories return an uninitialized plain interface. This caller
    // hint avoids reading its frame field before actual Initialize; private
    // backends instead use their own issued row/operation journal.
    bool sourceWasInitialized = false;
};
struct SCustomAnimationSequenceTransferRequest
{
    CAnimManager*       origin = nullptr;
    CAnimBlendSequence* source = nullptr;
    CAnimBlendSequence* destination = nullptr;
};
struct SCustomAnimationFrameAttachmentRequest
{
    CAnimManager*       origin = nullptr;
    CAnimBlendSequence* target = nullptr;
    std::size_t         count = 0;
    bool                root = false;
    bool                compressed = false;
    void*               originalFrames = nullptr;
};
class CCustomAnimationSequenceOperation
{
public:
    // Facade pointers above are borrowed only during Begin. The same backend
    // persists copied issued identities/intent before accepting effects. An
    // accepted Begin must return its preallocated CPU ticket even on a late
    // refusal; null/exception means no operation claims were accepted.
    virtual ~CCustomAnimationSequenceOperation() = default;
    virtual SCustomAnimationSequenceResult Resume() noexcept = 0;
    // Destruction releases CPU-view lifetime only. Complete means the exact
    // operation's receipt: frame binding, source release, or accepted frame
    // ownership transfer plus source-interface release, respectively.
};
