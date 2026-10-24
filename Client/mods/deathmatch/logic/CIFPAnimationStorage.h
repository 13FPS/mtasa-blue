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
#include <memory>
#include <set>
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
    void DestroyAnimation(Manager& manager, Animation& animation)
    {
        auto*           currentHierarchy = animation.pHierarchy.get();
        auto*           sequences = animation.pSequences;
        const auto      capacity = animation.sequenceCapacity;
        const bool      attached = animation.sequencesAttached;
        std::set<void*> individualFrames;
        void*           sharedBase = nullptr;
        if (currentHierarchy && sequences && attached)
        {
            // Every capacity row was initialized before publication, including
            // unknown/dummy rows beyond the temporary logical input count.
            for (std::size_t index = 0; index != capacity; ++index)
            {
                auto sequence = manager.GetCustomAnimBlendSequence(currentHierarchy->GetSequence(static_cast<DWORD>(index)));
                if (!sequence)
                    throw std::runtime_error("IFP backend returned no initialized sequence row");
                auto* frames = sequence->GetKeyFrames();
                if (sequence->IsBigChunkForAllSequences())
                {
                    // Only row0 supplies the original shared allocation base.
                    // Other flagged rows can be arbitrary interior slices.
                    if (!index)
                        sharedBase = frames;
                }
                else if (frames)
                    individualFrames.insert(frames);
            }
        }
        individualFrames.erase(sharedBase);
        // Finish potentially allocating observations before relinquishing any
        // record ownership. A failed gather can still be retried by its owner.
        if (currentHierarchy && sequences && attached)
            manager.RemoveFromUncompressedCache(currentHierarchy->GetInterface());
        auto hierarchy = std::move(animation.pHierarchy);
        animation.pSequences = nullptr;
        animation.sequenceCapacity = 0;
        animation.sequencesAttached = false;
        for (auto* frames : individualFrames)
            manager.FreeKeyFramesMemory(frames);
        if (sequences)
            manager.FreeCustomAnimSequences(sequences);
        if (sharedBase)
            manager.FreeKeyFramesMemory(sharedBase);
        if (hierarchy)
            manager.DeleteCustomAnimHierarchyInterface(hierarchy->GetInterface());
    }
}
