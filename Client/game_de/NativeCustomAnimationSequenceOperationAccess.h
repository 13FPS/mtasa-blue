/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Same-owner durable standalone sequence and frame operation intents
 ****************************************************************************/
#pragma once
#include "NativeAnimationSequenceAccess.h"
#include "NativeCustomAnimationCleanupAccess.h"
#include <game/CCustomAnimationSequenceOperation.h>
namespace mta::game_de
{
    // One command namespace and CPU-code lifetime on the existing Q owner.
    // A raw row remains a locator, not a standalone allocation capability.
    using NativeCustomAnimationSequenceOperationToken = NativeCustomAnimationCleanupToken;
    using NativeCustomAnimationSequenceOperationBeginReceipt = NativeCustomAnimationCleanupBeginReceipt;
    class NativeCustomAnimationSequenceOperationAccess
    {
    public:
        virtual ~NativeCustomAnimationSequenceOperationAccess() = default;
        virtual ClockResult AllocatePrivateStandaloneAnimationSequence(std::uint64_t, NativeAnimationSequenceReferenceToken&) noexcept
        {
            return ClockResult::NotInitialized;
        }
        virtual ClockResult ResolvePrivateStandaloneAnimationSequence(std::uint64_t, std::uintptr_t, NativeAnimationSequenceReferenceToken&) noexcept
        {
            return ClockResult::NotInitialized;
        }
        virtual ClockResult BeginPrivateAnimationSequenceCleanup(const NativeAnimationSequenceReferenceToken&,
                                                                 NativeCustomAnimationSequenceOperationBeginReceipt&) noexcept
        {
            return ClockResult::NotInitialized;
        }
        virtual ClockResult BeginPrivateAnimationSequenceTransfer(const NativeAnimationSequenceReferenceToken&, const NativeAnimationSequenceReferenceToken&,
                                                                  NativeCustomAnimationSequenceOperationBeginReceipt&) noexcept
        {
            return ClockResult::NotInitialized;
        }
        virtual ClockResult BeginPrivateAnimationFrameAttachment(const NativeAnimationSequenceReferenceToken&, std::size_t, bool, bool, std::uintptr_t,
                                                                 NativeCustomAnimationSequenceOperationBeginReceipt&) noexcept
        {
            return ClockResult::NotInitialized;
        }
        virtual ClockResult ResumePrivateAnimationSequenceOperation(const NativeCustomAnimationSequenceOperationToken&,
                                                                    SCustomAnimationSequenceResult&) noexcept
        {
            return ClockResult::NotInitialized;
        }
        // Includes pending commands and every live completed CPU ticket.
        virtual bool HasPendingPrivateAnimationSequenceOperations() const noexcept { return true; }
    };
}
