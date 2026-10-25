/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Durable private CPU hierarchy records on the existing runtime owner
 ****************************************************************************/
#pragma once
#include "NativePrivateAnimationHierarchyStorage.h"
#include "NativeCustomAnimSequenceController.h"
#include <list>
namespace mta::game_de
{
    class NativePrivateAnimationHierarchyController final
    {
    public:
        using Token = NativeAnimationHierarchyReferenceToken;
        using Values = NativeAnimationHierarchyValues;
        using Storage = NativePrivateAnimationHierarchyStorage;
        using CheckOwner = NativePrivateAnimationHierarchyCalls::CheckOwner;
        using Receipt = NativePrivateAnimationHierarchyCalls::Receipt;
        using PostProcessOperation = NativePrivateAnimationHierarchyCalls::PostProcessOperation;
        using PostProcessReceipt = NativePrivateAnimationHierarchyCalls::PostProcessReceipt;
        using InvokePostProcess = NativePrivateAnimationHierarchyCalls::InvokePostProcess;
        struct RetireProgress
        {
            std::size_t scanned = 0, attempted = 0, released = 0, retained = 0;
        };
        explicit NativePrivateAnimationHierarchyController(NativeCustomAnimSequenceController* sequences = nullptr) noexcept : sequences(sequences) {}
        ClockResult SetSequenceCount(std::uintptr_t, const Token&, std::uint16_t, CheckOwner, Receipt* = nullptr) noexcept;
        ClockResult SetSequences(std::uintptr_t, const Token&, std::uintptr_t, CheckOwner, Receipt* = nullptr) noexcept;
        ClockResult CheckCacheZero(std::uintptr_t, const Token&, CheckOwner) noexcept;
        ClockResult RemoveEmptySequences(std::uintptr_t, const Token&, CheckOwner, Receipt* = nullptr) noexcept;
        ClockResult RemoveQuaternionFlips(std::uintptr_t, const Token&, CheckOwner, PostProcessReceipt* = nullptr, InvokePostProcess = nullptr) noexcept;
        ClockResult CalculateTotalTime(std::uintptr_t, const Token&, CheckOwner, PostProcessReceipt* = nullptr, InvokePostProcess = nullptr) noexcept;
        ClockResult ResolvePrivate(std::uintptr_t, std::uint64_t, std::uintptr_t, CheckOwner, Token&) noexcept;
        ClockResult Create(std::uintptr_t image, std::uint64_t generation, CheckOwner, Token& output, Receipt* = nullptr) noexcept;
        ClockResult Initialize(std::uintptr_t image, const Token&, CheckOwner, Receipt* = nullptr) noexcept;
        ClockResult Destroy(std::uintptr_t image, const Token&, CheckOwner, Receipt* = nullptr) noexcept;
        ClockResult SetRunningCompressed(std::uintptr_t image, const Token&, bool, CheckOwner, Receipt* = nullptr) noexcept;
        ClockResult SetAnimationBlockID(std::uintptr_t image, const Token&, std::int32_t, CheckOwner, Receipt* = nullptr) noexcept;
        ClockResult SetNameHash(std::uintptr_t image, const Token&, std::uint32_t, CheckOwner, Receipt* = nullptr) noexcept;
        ClockResult Read(std::uintptr_t image, const Token&, CheckOwner, Values&) noexcept;
        ClockResult ReadSequences(std::uintptr_t image, const Token&, CheckOwner, std::uintptr_t&) noexcept;
        ClockResult ReadSequence(std::uintptr_t image, const Token&, std::uint32_t index, CheckOwner, std::uintptr_t&) noexcept;
        ClockResult RetireEmpty(std::uintptr_t image, std::uint64_t generation, std::size_t budget, CheckOwner, RetireProgress&) noexcept;
        ClockResult PinPublication(const Token&) noexcept;
        // Serialized current-owner routing only. Raw locators neither adopt a
        // foreign pointer nor grant stale-address ABA/child-free authority.
        bool Owns(const Token&) const noexcept;
        bool IsRetained() const noexcept { return retained.load(std::memory_order_acquire); }
        bool RecoveryPending() const noexcept { return recovery.load(std::memory_order_acquire); }

    private:
        friend class NativeCustomAnimSequenceController;
        ClockResult ReadCleanupIssue(const Token&, std::uint64_t&) noexcept;
        ClockResult ClaimCleanupIntent(const Token&, std::uintptr_t, std::uint64_t) noexcept;
        ClockResult VerifyCleanupHeld(const NativeAnimationSequenceDependencyCalls::Epoch&, const Token&, std::uint64_t, CheckOwner) noexcept;
        ClockResult DetachCleanup(std::uintptr_t, const Token&, std::uint64_t, CheckOwner, Receipt&, bool&) noexcept;
        ClockResult RetireCleanup(std::uintptr_t, const Token&, std::uint64_t, CheckOwner, Receipt&) noexcept;
        using SequenceOwner = NativeCustomAnimSequenceController;
        using Key = SequenceOwner::PrivateDependencyKey;
        using Epoch = NativeAnimationSequenceDependencyCalls::Epoch;
        enum class LinkPhase : std::uint32_t
        {
            Planned,
            AttachEntered,
            AttachedAccepted,
            DetachEntered,
            DetachAccepted,
            Uncertain
        };
        struct Link
        {
            Key       key{};
            LinkPhase phase{LinkPhase::Planned};
            // The sequence owner writes the actual release receipt directly
            // into this durable journal. No absent map/raw-null inference.
            SequenceOwner::PrivateDependencyReceipt dependency{};
            bool                                    active = false;
        };
        struct Entry
        {
            explicit Entry(std::uint64_t generation) noexcept : storage(generation) {}
            Storage           storage;
            std::uint64_t     issue = 0;
            Link              link;
            bool              issued = false;
            std::uint64_t     cleanupCommand = 0;
            bool              cleanupHadLink = false;
            std::atomic<bool> cleanupReleased{false};
        };
        using Entries = std::list<Entry>;
        Entries::iterator Find(const Token&) noexcept;
        ClockResult       SetScalar(std::uintptr_t, const Token&, NativePrivateAnimationHierarchyCalls::ScalarOperation, std::uint32_t, CheckOwner,
                                    Receipt*) noexcept;
        enum class Action : std::uint32_t
        {
            Attach,
            Detach,
            Cancel,
            Read,
            ReadSequences,
            ReadSequence,
            Scalar,
            PostProcess
        };
        struct ActionContext
        {
            NativePrivateAnimationHierarchyController*            self;
            Token                                                 token;
            Key                                                   key;
            CheckOwner                                            owner;
            Action                                                action;
            NativePrivateAnimationHierarchyCalls::ScalarOperation scalar{NativePrivateAnimationHierarchyCalls::ScalarOperation::SequenceCount};
            std::uint32_t                                         bits = 0, index = 0;
            Receipt                                               receipt{};
            Values                                                values{};
            std::uintptr_t                                        pointer = 0;
            std::uint64_t                                         cleanupCommand = 0;
            PostProcessOperation                                  postProcess{PostProcessOperation::RemoveQuaternionFlips};
            PostProcessReceipt                                    postReceipt{};
            InvokePostProcess                                     invokePostProcess = nullptr;
        };
        static ClockResult ValidatePayloadHeld(const Epoch&, std::uint16_t, PostProcessOperation, void*) noexcept;
        ClockResult        ProcessPayload(std::uintptr_t, const Token&, PostProcessOperation, CheckOwner, PostProcessReceipt*, InvokePostProcess) noexcept;
        static ClockResult ParentAction(NativeCustomAnimSequenceStorage&, const Epoch&, void*) noexcept;
        ClockResult SetSequencesImpl(std::uintptr_t, const Token&, std::uintptr_t, CheckOwner, Receipt*, bool retirement, std::uint64_t cleanupCommand = 0,
                                     bool* releaseAcknowledged = nullptr) noexcept;
        ClockResult DestroyImpl(std::uintptr_t, const Token&, CheckOwner, Receipt*, bool retirement) noexcept;
        ClockResult LinkedAction(std::uintptr_t, const Token&, CheckOwner, ActionContext&, Receipt*) noexcept;
        void        Publish() noexcept;
        Entries     entries;
        // Runtime owns both controllers for the same callback/module lifetime.
        // A default-null provider preserves the prior empty-only product.
        NativeCustomAnimSequenceController* sequences;
        std::uint64_t                       nextCommand = 0, nextIssue = 0;
        bool                                stopping = false;
        std::atomic_flag                    operation = ATOMIC_FLAG_INIT;
        std::atomic<bool>                   retained{false}, recovery{false};
    };
}
