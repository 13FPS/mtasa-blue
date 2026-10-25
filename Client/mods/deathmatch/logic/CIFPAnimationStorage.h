/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Explicit common IFP ownership over backend sequence storage
 ****************************************************************************/
#pragma once
#include <cstddef>
#include <cstdint>
#include <windows.h>
#include <game/CAnimBlendHierarchy.h>
#include <game/CAnimBlendSequence.h>
#include <game/CAnimManager.h>
#include <game/CCustomAnimationCleanup.h>
#include <game/CCustomAnimationSequenceOperation.h>
#include <memory>
#include <limits>
#include <stdexcept>
#include <utility>
#include <exception>
namespace MtaGame::IfpStorage
{
    template <class Manager>
    class SequenceOwner
    {
    public:
        SequenceOwner() = default;
        SequenceOwner(Manager& manager, std::unique_ptr<CAnimBlendSequence> sequence, bool ownsInterface)
            : manager(&manager), sequence(std::move(sequence)), ownsInterface(ownsInterface)
        {
        }
        SequenceOwner(const SequenceOwner&) = delete;
        SequenceOwner& operator=(const SequenceOwner&) = delete;
        SequenceOwner(SequenceOwner&& other) noexcept
            : manager(other.manager),
              sequence(std::move(other.sequence)),
              operation(std::move(other.operation)),
              frameOperation(std::move(other.frameOperation)),
              ownsInterface(other.ownsInterface),
              initialized(other.initialized)
        {
            other.ownsInterface = false;
        }
        SequenceOwner& operator=(SequenceOwner&& other)
        {
            if (this != &other)
            {
                Reset();
                manager = other.manager;
                sequence = std::move(other.sequence);
                operation = std::move(other.operation);
                frameOperation = std::move(other.frameOperation);
                ownsInterface = other.ownsInterface;
                initialized = other.initialized;
                other.ownsInterface = false;
            }
            return *this;
        }
        ~SequenceOwner()
        {
            Reset();
            // Even when cleanup is pending, dispose only the CPU facade while
            // accepted operation views still retain its backend code lifetime.
            sequence.reset();
        }
        std::unique_ptr<CAnimBlendSequence>&                Get() noexcept { return sequence; }
        void                                                MarkInitialized() noexcept { initialized = true; }
        std::unique_ptr<CCustomAnimationSequenceOperation>& FrameOperation() noexcept { return frameOperation; }
        void                                                TransferTo(CAnimBlendSequence& destination)
        {
            if (!manager || !sequence || !ownsInterface)
                throw std::logic_error("IFP transfer requires its owned standalone source");
            if (frameOperation)
            {
                const auto frameResult = frameOperation->Resume();
                if (frameResult.status != ECustomAnimationSequenceStatus::Complete ||
                    frameResult.disposition != ECustomAnimationSequenceDisposition::FramesAttached)
                    throw std::runtime_error("IFP source frame construction acknowledgement is pending or refused");
                frameOperation.reset();
            }
            if (!operation)
                operation = manager->BeginCustomAnimationSequenceTransfer({manager, sequence.get(), &destination});
            if (!operation)
                throw std::runtime_error("IFP backend did not accept sequence transfer");
            const auto result = operation->Resume();
            if (result.status != ECustomAnimationSequenceStatus::Complete || result.disposition != ECustomAnimationSequenceDisposition::SourceTransferred)
                throw std::runtime_error("IFP sequence transfer acknowledgement is pending or refused");
            // The backend moved only acknowledged frame ownership and retired
            // the source interface. Keep the CPU view until its facade is gone.
            ownsInterface = false;
            initialized = false;
            sequence.reset();
            operation.reset();
        }

    private:
        void Reset() noexcept
        {
            // Resume the accepted assignment first. Standalone cleanup can
            // then cancel a pre-entry assignment or wait for its real ACK;
            // no getter or fresh source lookup substitutes for that ticket.
            if (frameOperation)
            {
                const auto result = frameOperation->Resume();
                if (result.status == ECustomAnimationSequenceStatus::Complete && result.disposition == ECustomAnimationSequenceDisposition::FramesAttached)
                    frameOperation.reset();
            }
            if (sequence && ownsInterface)
            {
                try
                {
                    if (!operation)
                        operation = manager->BeginCustomAnimationSequenceCleanup({manager, sequence.get(), initialized});
                    if (!operation)
                        return;
                    const auto result = operation->Resume();
                    if (result.status != ECustomAnimationSequenceStatus::Complete ||
                        (result.disposition != ECustomAnimationSequenceDisposition::SourceReleased &&
                         result.disposition != ECustomAnimationSequenceDisposition::SourceTransferred))
                        return;
                    ownsInterface = false;
                    initialized = false;
                }
                catch (...)
                {
                    // The factory's original owner reservation and any accepted
                    // command retain the resources if this last CPU guard dies.
                    return;
                }
            }
            sequence.reset();
            operation.reset();
            frameOperation.reset();
        }
        Manager*                                           manager = nullptr;
        std::unique_ptr<CAnimBlendSequence>                sequence;
        std::unique_ptr<CCustomAnimationSequenceOperation> operation;
        std::unique_ptr<CCustomAnimationSequenceOperation> frameOperation;
        bool                                               ownsInterface = false, initialized = false;
    };

    template <class Manager>
    void AttachFrames(Manager& manager, CAnimBlendSequence& sequence, std::size_t count, bool root, bool compressed, BYTE* frames,
                      std::unique_ptr<CCustomAnimationSequenceOperation>* retained = nullptr)
    {
        std::unique_ptr<CCustomAnimationSequenceOperation> local;
        auto&                                              operation = retained ? *retained : local;
        if (operation)
            throw std::logic_error("IFP frame assignment already has an accepted operation");
        try
        {
            operation = manager.BeginCustomAnimationFrameAttachment({&manager, &sequence, count, root, compressed, frames});
            if (!operation)
                throw std::runtime_error("IFP backend did not accept original-frame assignment");
        }
        catch (...)
        {
            // Throw/null guarantees no operation claim was accepted. Explicitly
            // release this original allocation once; its actual owner records
            // returned versus entered/unknown free, with no getter inference.
            if (frames)
            {
                try
                {
                    manager.FreeKeyFramesMemory(frames);
                }
                catch (...)
                {
                    std::throw_with_nested(std::runtime_error("IFP frame assignment admission failed; owner release acknowledgement refused"));
                }
            }
            throw;
        }
        const auto result = operation->Resume();
        if (result.status != ECustomAnimationSequenceStatus::Complete || result.disposition != ECustomAnimationSequenceDisposition::FramesAttached)
            throw std::runtime_error("IFP original-frame assignment acknowledgement is pending or refused");
        // Once accepted, this command's owner retains the original frame even
        // if binding/postflight fails. Never free it from a pointer comparison.
        operation.reset();
    }

    template <class Manager, class Animation>
    bool DestroyAnimation(Manager& manager, Animation& animation)
    {
        if (!animation.cleanupTicket && !animation.pHierarchy && !animation.pSequences && !animation.sequenceCapacity)
            return true;
        if (!animation.cleanupTicket)
        {
            SCustomAnimationCleanupRequest request;
            request.origin = &manager;
            request.hierarchy = animation.pHierarchy.get();
            request.originalArray = animation.pSequences;
            request.capacity = animation.sequenceCapacity;
            request.cacheWasAttached = animation.sequencesAttached;
            // Begin copies backend identity and persists the source ownership
            // before anything can detach or free. A null ticket grants nothing.
            try
            {
                animation.cleanupTicket = manager.BeginCustomAnimationCleanup(request);
            }
            catch (...)
            {
                // Factories reserve backend ownership before exposure. Ticket
                // allocation/refusal must not terminate a destructor or grant
                // permission to forget those retained native resources.
                return false;
            }
            if (!animation.cleanupTicket)
                return false;
        }
        const auto result = animation.cleanupTicket->Resume();
        if (result.status != ECustomAnimationCleanupStatus::Complete || (result.disposition != ECustomAnimationCleanupDisposition::ClassicExternalChildren &&
                                                                         result.disposition != ECustomAnimationCleanupDisposition::PrivateDetached))
            return false;
        // Only real per-original release/outer-delete acknowledgements permit
        // CPU ownership reset. Pending work survives in the same backend owner;
        // no cleared parent is regathered and no void free is replayed here.
        animation.pHierarchy.reset();
        animation.pSequences = nullptr;
        animation.sequenceCapacity = 0;
        animation.sequencesAttached = false;
        animation.cleanupTicket.reset();
        return true;
    }
}
