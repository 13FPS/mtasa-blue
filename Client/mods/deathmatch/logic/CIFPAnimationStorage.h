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
#include <memory>
#include <limits>
#include <stdexcept>
#include <utility>
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
            : manager(other.manager), sequence(std::move(other.sequence)), ownsInterface(other.ownsInterface), initialized(other.initialized)
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
                ownsInterface = other.ownsInterface;
                initialized = other.initialized;
                other.ownsInterface = false;
            }
            return *this;
        }
        ~SequenceOwner() { Reset(); }
        std::unique_ptr<CAnimBlendSequence>& Get() noexcept { return sequence; }
        void                                 MarkInitialized() noexcept { initialized = true; }
        void                                 TransferTo(CAnimBlendSequence& destination)
        {
            auto* frames = sequence->GetKeyFrames();
            try
            {
                destination.CopySequenceProperties(sequence->GetInterface());
            }
            catch (...)
            {
                // A setter can report failure after publishing its pointer.
                // Retain that accepted destination ownership, never free it
                // through both the detached source and the hierarchy array.
                if (frames && destination.GetKeyFrames() == frames)
                    initialized = false;
                throw;
            }
            initialized = false;
            auto* native = sequence->GetInterface();
            ownsInterface = false;
            manager->DeleteCustomAnimSequenceInterface(native);
        }

    private:
        void Reset()
        {
            if (sequence && ownsInterface)
            {
                auto* native = sequence->GetInterface();
                auto* frames = initialized ? sequence->GetKeyFrames() : nullptr;
                ownsInterface = false;
                if (frames)
                    manager->FreeKeyFramesMemory(frames);
                manager->DeleteCustomAnimSequenceInterface(native);
            }
            sequence.reset();
        }
        Manager*                            manager = nullptr;
        std::unique_ptr<CAnimBlendSequence> sequence;
        bool                                ownsInterface = false, initialized = false;
    };

    template <class Manager>
    void AttachFrames(Manager& manager, CAnimBlendSequence& sequence, std::size_t count, bool root, bool compressed, BYTE* frames)
    {
        try
        {
            sequence.SetKeyFrames(count, root, compressed, frames);
            if (sequence.GetKeyFrames() != frames)
                throw std::runtime_error("IFP backend did not attach supplied keyframes");
        }
        catch (...)
        {
            // Once published, the row's existing owner performs cleanup.
            // Before publication this allocation is still the caller's.
            if (sequence.GetKeyFrames() != frames)
                manager.FreeKeyFramesMemory(frames);
            throw;
        }
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
