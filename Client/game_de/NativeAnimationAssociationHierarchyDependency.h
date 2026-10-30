/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Closed association-specific holds on actual issued private hierarchies
 ****************************************************************************/
#pragma once
#include "NativeAnimationAssociationAccess.h"
#include "NativeAnimationHierarchyAccess.h"
#include <atomic>
namespace mta::game_de
{
    class NativeAnimationAssociationStorage;
    class NativePrivateAnimationHierarchyController;
    struct NativeAnimationAssociationStorageAccess;
    struct NativeAnimationAssociationClumpOwnershipAccess;
    struct NativeAnimationAssociationHierarchyDependencyReceipt
    {
        std::uint64_t  receiverGeneration = 0, receiverSerial = 0, command = 0, hierarchyIssue = 0;
        std::uintptr_t hierarchy = 0, sequences = 0;
        bool           planned = false, held = false, releaseEntered = false, released = false, uncertain = false, verified = false;
        ClockResult    result = ClockResult::NotInitialized;
    };
    class NativeAnimationAssociationHierarchyDependency final
    {
    public:
        enum class State : std::uint32_t
        {
            Planned,
            Held,
            ReleaseEntered,
            Released,
            Uncertain
        };
        NativeAnimationAssociationHierarchyDependency(const NativeAnimationAssociationHierarchyDependency&) = delete;
        NativeAnimationAssociationHierarchyDependency& operator=(const NativeAnimationAssociationHierarchyDependency&) = delete;
        ~NativeAnimationAssociationHierarchyDependency() = default;
        State GetState() const noexcept { return state.load(std::memory_order_acquire); }
        bool  IsHeld() const noexcept { return GetState() == State::Held; }
        bool  CanDiscard() const noexcept { return GetState() == State::Planned || GetState() == State::Released; }

    private:
        friend struct NativeAnimationAssociationStorageAccess;
        friend struct NativeAnimationAssociationClumpOwnershipAccess;
        friend class NativePrivateAnimationHierarchyController;
        NativeAnimationAssociationHierarchyDependency(NativeAnimationAssociationStorage& storage, const NativeAnimationAssociationReferenceToken& token,
                                                      std::uint64_t operation) noexcept
            : target(&storage), receiver(token), command(operation)
        {
        }
        NativeAnimationAssociationStorage*         target;
        NativeAnimationAssociationReferenceToken   receiver;
        std::uint64_t                              command;
        NativePrivateAnimationHierarchyController* controller = nullptr;
        NativeAnimationHierarchyReferenceToken     hierarchy{};
        std::uintptr_t                             image = 0, sequences = 0;
        std::uint64_t                              hierarchyIssue = 0, sequenceCommand = 0;
        bool                                       hadSequenceLink = false;
        std::atomic<State>                         state{State::Planned};
    };
    class NativeAnimationAssociationHierarchyRelease final
    {
    public:
        enum class Reason : std::uint32_t
        {
            ReceiverRetired,
            HierarchyReplaced,
            UnenteredNewHoldCancellation
        };
        NativeAnimationAssociationHierarchyRelease(const NativeAnimationAssociationHierarchyRelease&) = delete;
        NativeAnimationAssociationHierarchyRelease& operator=(const NativeAnimationAssociationHierarchyRelease&) = delete;

    private:
        friend struct NativeAnimationAssociationStorageAccess;
        friend class NativePrivateAnimationHierarchyController;
        // Only the actual storage owner creates this proof from its genuine
        // retirement/replacement/cancellation journal. A public receipt is not
        // a substitute, and node release alone never constructs this proof.
        NativeAnimationAssociationHierarchyRelease(const NativeAnimationAssociationHierarchyDependency& dependency, Reason why,
                                                   std::uint64_t acknowledgedCommand, std::uintptr_t newHierarchy = 0,
                                                   const NativeAnimationAssociationHierarchyDependency* newDependency = nullptr) noexcept
            : target(&dependency), reason(why), command(acknowledgedCommand), replacementHierarchy(newHierarchy), replacement(newDependency)
        {
        }
        const NativeAnimationAssociationHierarchyDependency* target;
        Reason                                               reason;
        std::uint64_t                                        command;
        std::uintptr_t                                       replacementHierarchy;
        const NativeAnimationAssociationHierarchyDependency* replacement;
    };
}
