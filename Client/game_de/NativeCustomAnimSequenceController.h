/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Keep issued native animation allocations with their callback owner
 ****************************************************************************/
#pragma once
#include "NativeCustomAnimSequenceStorage.h"
#include "NativeAnimationSequenceRowCalls.h"
#include "NativeAnimationSequenceCopyCalls.h"
#include "NativeAnimationSequenceNameCalls.h"
#include "NativeAnimationSequenceFrameBindCalls.h"
#include "NativeAnimationSequenceFrameViewCalls.h"
#include "NativeAnimationSequenceAccess.h"
#include "NativeAnimationSequenceLogicalState.h"
#include "NativeAnimationSequenceDependencyCalls.h"
#include "NativeAnimationSequencePayloadCalls.h"
#include "NativeCustomAnimationCleanupAccess.h"
#include "NativeCustomAnimationSequenceOperationAccess.h"
#include "NativePrivateAnimationHierarchyStorage.h"
#include <atomic>
#include <list>
#include <memory>
#include <vector>

namespace mta::game_de
{
    class NativePrivateAnimationHierarchyController;
    class NativeCustomAnimSequenceController final
    {
    public:
        using Storage = NativeCustomAnimSequenceStorage;
        using Receipt = NativeCustomAnimSequenceStorageCalls::Receipt;
        using RowReceipt = NativeAnimationSequenceRowCalls::Receipt;
        using CopyReceipt = NativeAnimationSequenceCopyCalls::Receipt;
        using NameReceipt = NativeAnimationSequenceNameCalls::Receipt;
        using FrameBindReceipt = NativeAnimationSequenceFrameBindCalls::Receipt;
        using CheckOwner = NativeCustomAnimSequenceStorageCalls::CheckOwner;
        using InvokeMalloc = NativeCustomAnimSequenceStorageCalls::InvokeMalloc;
        using InvokeFree = NativeCustomAnimSequenceStorageCalls::InvokeFree;
        enum class FrameFormat : std::uint32_t
        {
            Unknown,
            Compressed,
            Uncompressed
        };
        struct AcceptedFrameFormat
        {
            FrameFormat    format{FrameFormat::Unknown};
            std::uintptr_t frameBase = 0;
            std::size_t    count = 0;
            bool           root = false;
        };
        // Current owner observation only, not a lease or permission to run a
        // payload operation later without its own held native qualification.
        ClockResult ReadSequenceFrameFormat(std::uintptr_t, const NativeAnimationSequenceReferenceToken&, CheckOwner, AcceptedFrameFormat&) noexcept;
        struct RetireProgress
        {
            std::size_t scanned = 0, attempted = 0, released = 0, retained = 0;
        };
        struct CleanupProgress
        {
            std::size_t scanned = 0, attempted = 0, released = 0, retained = 0;
            bool        pending = false, unknown = false;
            ClockResult ownerStatus = ClockResult::Ok;
        };
        // Begin reserves only CPU intent and actual issued-record claims. It
        // is legal before native admission; Resume performs every native read,
        // matching detach and destructive acknowledgement on a real callback.
        ClockResult BeginCleanup(std::uintptr_t, const NativeCustomAnimationCleanupRequest&, NativePrivateAnimationHierarchyController&, CheckOwner,
                                 NativeCustomAnimationCleanupBeginReceipt&) noexcept;
        ClockResult ResumeCleanup(std::uintptr_t, const NativeCustomAnimationCleanupToken&, NativePrivateAnimationHierarchyController&, std::size_t, CheckOwner,
                                  SCustomAnimationCleanupResult&) noexcept;
        ClockResult ResumePendingCleanups(std::uintptr_t, std::uint64_t, NativePrivateAnimationHierarchyController&, std::size_t, CheckOwner,
                                          CleanupProgress&) noexcept;
        bool        HasPendingCleanup() const noexcept;
        ClockResult AllocateStandaloneSequence(std::uintptr_t, std::uint64_t, CheckOwner, NativeAnimationSequenceReferenceToken&, Receipt* = nullptr,
                                               InvokeMalloc = nullptr) noexcept;
        ClockResult ResolveStandaloneSequence(std::uintptr_t, std::uint64_t, std::uintptr_t, CheckOwner, NativeAnimationSequenceReferenceToken&) noexcept;
        ClockResult BeginSequenceCleanup(std::uintptr_t, const NativeAnimationSequenceReferenceToken&, NativePrivateAnimationHierarchyController&, CheckOwner,
                                         NativeCustomAnimationSequenceOperationBeginReceipt&) noexcept;
        ClockResult BeginSequenceTransfer(std::uintptr_t, const NativeAnimationSequenceReferenceToken&, const NativeAnimationSequenceReferenceToken&,
                                          NativePrivateAnimationHierarchyController&, CheckOwner, NativeCustomAnimationSequenceOperationBeginReceipt&) noexcept;
        ClockResult BeginFrameAttachment(std::uintptr_t, const NativeAnimationSequenceReferenceToken&, std::size_t, bool, bool, std::uintptr_t,
                                         NativePrivateAnimationHierarchyController&, CheckOwner, NativeCustomAnimationSequenceOperationBeginReceipt&) noexcept;
        ClockResult ResumeSequenceOperation(std::uintptr_t, const NativeCustomAnimationSequenceOperationToken&, NativePrivateAnimationHierarchyController&,
                                            std::size_t, CheckOwner, SCustomAnimationSequenceResult&) noexcept;
        NativeCustomAnimSequenceController() noexcept = default;
        NativeCustomAnimSequenceController(const NativeCustomAnimSequenceController&) = delete;
        NativeCustomAnimSequenceController& operator=(const NativeCustomAnimSequenceController&) = delete;
        NativeCustomAnimSequenceController(NativeCustomAnimSequenceController&&) = delete;
        NativeCustomAnimSequenceController& operator=(NativeCustomAnimSequenceController&&) = delete;

        ClockResult ResolveRowAt(std::uintptr_t, std::uint64_t, std::uintptr_t, std::uint32_t, CheckOwner, bool&,
                                 NativeAnimationSequenceReferenceToken&) noexcept;
        ClockResult Allocate(std::uintptr_t image, std::uint64_t generation, std::size_t capacity, CheckOwner, std::uintptr_t& output, Receipt* = nullptr,
                             InvokeMalloc = nullptr) noexcept;
        ClockResult FreeStorage(std::uintptr_t image, std::uint64_t generation, std::uintptr_t issuedTag, CheckOwner, Receipt* = nullptr,
                                InvokeFree = nullptr) noexcept;
        // Frame bytes have no sequence cookie; the issued tag is the original
        // allocation base. Both resource kinds stay on this one native owner.
        ClockResult AllocateFrames(std::uintptr_t image, std::uint64_t generation, std::uint32_t bytes, CheckOwner, std::uintptr_t& output, Receipt* = nullptr,
                                   InvokeMalloc = nullptr) noexcept;
        ClockResult FreeFrames(std::uintptr_t image, std::uint64_t generation, std::uintptr_t issuedBase, CheckOwner, Receipt* = nullptr,
                               InvokeFree = nullptr) noexcept;
        ClockResult Retire(std::uintptr_t image, std::uint64_t generation, std::size_t budget, CheckOwner cleanupOwner, RetireProgress&,
                           InvokeFree = nullptr) noexcept;
        ClockResult InitializeSequence(std::uintptr_t, const NativeAnimationSequenceReferenceToken&, CheckOwner, RowReceipt* = nullptr) noexcept;
        ClockResult SetSequenceBoneTag(std::uintptr_t, const NativeAnimationSequenceReferenceToken&, std::int32_t, CheckOwner, RowReceipt* = nullptr) noexcept;
        ClockResult ReadSequence(std::uintptr_t, const NativeAnimationSequenceReferenceToken&, CheckOwner, NativeAnimationSequenceValues&) noexcept;
        ClockResult ReadSequenceHash(std::uintptr_t, const NativeAnimationSequenceReferenceToken&, CheckOwner, std::uint32_t&) noexcept;
        ClockResult CopySequenceProperties(std::uintptr_t, const NativeAnimationSequenceReferenceToken&, std::uintptr_t, CheckOwner,
                                           CopyReceipt* = nullptr) noexcept;
        ClockResult ReadSequenceFrame(std::uintptr_t, const NativeAnimationSequenceReferenceToken&, std::size_t, std::uint32_t, CheckOwner,
                                      std::uintptr_t&) noexcept;
        ClockResult SetSequenceName(std::uintptr_t, const NativeAnimationSequenceReferenceToken&, const NativeAnimationSequenceName&, CheckOwner,
                                    NameReceipt* = nullptr) noexcept;
        ClockResult PrepareFreshCompressedSequence(std::uintptr_t, const NativeAnimationSequenceReferenceToken&, std::size_t, bool, std::uintptr_t, CheckOwner,
                                                   FrameBindReceipt* = nullptr) noexcept;
        ClockResult PrepareFreshUncompressedSequence(std::uintptr_t, const NativeAnimationSequenceReferenceToken&, std::size_t, bool, std::uintptr_t,
                                                     CheckOwner, FrameBindReceipt* = nullptr) noexcept;
        struct PrivateDependencyKey
        {
            std::uint64_t  ownerGeneration = 0, command = 0;
            std::uintptr_t hierarchy = 0, arrayTag = 0;
        };
        struct PrivateDependencyReceipt
        {
            std::size_t capacity = 0, frameRecords = 0;
            bool        reserved = false, verified = false, released = false, uncertain = false;
        };
        using PrivatePreparedAction = NativeAnimationSequenceDependencyCalls::Action;
        // Internal owner prerequisites, not SDK attachment or detach ACK.
        // Parent plans its durable nonzero command before reserving anything.
        ClockResult ReservePrivatePreparedArray(std::uintptr_t, const PrivateDependencyKey&, std::size_t expectedCapacity, CheckOwner,
                                                PrivateDependencyReceipt* = nullptr) noexcept;
        ClockResult WithPrivatePreparedArray(std::uintptr_t, const PrivateDependencyKey&, CheckOwner, PrivatePreparedAction, void*,
                                             PrivateDependencyReceipt* = nullptr) noexcept;
        ClockResult CancelPrivatePreparedReservation(std::uintptr_t, const PrivateDependencyKey&, CheckOwner, PrivatePreparedAction, void*,
                                                     PrivateDependencyReceipt* = nullptr) noexcept;
        // This is private-owner bookkeeping for a future qualified hierarchy
        // attachment, not a public raw-pointer adoption or detachment API.
        ClockResult PinPublication(std::uint64_t generation, std::uintptr_t issuedTag, std::uintptr_t hierarchyAffinity) noexcept;

        // Only these conservative summaries may be read by producer threads.
        // The actual runtime retains records, its callback and module pin
        // until every allocation is discharged. Destruction never frees GTA.
        bool IsRetained() const noexcept { return retained.load(std::memory_order_acquire); }
        bool RecoveryPending() const noexcept { return recovery.load(std::memory_order_acquire); }

    private:
        friend class NativePrivateAnimationHierarchyController;
        ClockResult ReserveIssuedPrivatePreparedArray(std::uintptr_t, const PrivateDependencyKey&, CheckOwner, PrivateDependencyReceipt*) noexcept;
        ClockResult ReservePrivatePreparedArrayImpl(std::uintptr_t, const PrivateDependencyKey&, std::size_t, CheckOwner, PrivateDependencyReceipt*,
                                                    bool actualCapacity) noexcept;
        enum class PrivatePhase : std::uint32_t
        {
            Preparing,
            Reserved,
            AttachEntered,
            AttachedAccepted,
            DetachEntered,
            DetachAccepted,
            Uncertain
        };
        enum class SequencePurpose : std::uint32_t
        {
            ArrayRows,
            StandaloneSequence
        };
        enum class CommandKind : std::uint32_t
        {
            AnimationCleanup,
            SequenceCleanup,
            SequenceTransfer,
            FrameAttachment
        };
        struct Entry;
        enum class CleanupStage : std::uint32_t
        {
            Prepared,
            Entered,
            Returned,
            Unknown
        };
        struct CleanupResource
        {
            Entry*         live = nullptr;
            Storage::Kind  kind{Storage::Kind::SequenceArray};
            std::uint64_t  generation = 0;
            std::uintptr_t tag = 0;
            std::size_t    claims = 0;
            CleanupStage   stage{CleanupStage::Prepared};
            Receipt        receipt{};
        };
        struct CleanupRecord
        {
            NativeCustomAnimationCleanupRequest        request{};
            CommandKind                                commandKind{CommandKind::AnimationCleanup};
            NativeAnimationSequenceReferenceToken      source{}, target{};
            Entry*                                     targetEntry = nullptr;
            std::size_t                                frameCount = 0;
            std::uintptr_t                             originalFrames = 0;
            bool                                       rootFrames = false, compressedFrames = false;
            bool                                       cancelRequested = false, writerAcknowledged = false, targetObligationAdded = false;
            std::shared_ptr<CleanupResource>           transferFrame;
            CopyReceipt                                copy{};
            FrameBindReceipt                           attachment{};
            SCustomAnimationSequenceResult             sequenceResult{};
            NativeCustomAnimationCleanupToken          token{};
            NativePrivateAnimationHierarchyController* hierarchyOwner = nullptr;
            Entry*                                     array = nullptr;
            // Internal producer identity only. Raw SDK tags retain their
            // documented currently-issued/no-ABA limitation.
            std::uint64_t                                         hierarchyIssue = 0, arrayIssue = 0;
            std::shared_ptr<NativeCustomAnimationCleanupLifetime> lifetime;
            std::shared_ptr<CleanupResource>                      arrayAck;
            std::vector<std::shared_ptr<CleanupResource>>         frames;
            std::vector<NativeAnimationSequenceRowCalls::Value>   rows;
            NativePrivateAnimationHierarchyCalls::Receipt         detach{}, outer{};
            bool                                                  snapshotVerified = false, dependencyReleased = false;
            std::atomic<bool>                                     resuming{false}, refused{false}, complete{false};
        };
        struct PrivateDependency
        {
            PrivateDependencyKey                                key;
            PrivatePhase                                        phase{PrivatePhase::Preparing};
            std::vector<NativeAnimationSequenceRowCalls::Value> rows;
            std::vector<Entry*>                                 frames;
            bool                                                releaseRequested = false, cancelRequested = false;
        };
        // Only the actual parent controller may advance an entered/accepted
        // physical command or request its acknowledged release. Action Ok,
        // raw null fields and map erasure cannot manufacture that proof.
        ClockResult AdvancePrivateDependencyHeld(const PrivateDependencyKey&, PrivatePhase expected, PrivatePhase next) noexcept;
        ClockResult RequestPrivateDetachReleaseHeld(const PrivateDependencyKey&) noexcept;
        ClockResult RequestPrivateReservationCancelHeld(const PrivateDependencyKey&) noexcept;
        void        RetainPrivateDependencyHeld(const PrivateDependencyKey&) noexcept;
        struct FrameDescriptor
        {
            FrameFormat    format{FrameFormat::Unknown};
            const Storage* record = nullptr;
            std::uintptr_t base = 0;
            std::size_t    count = 0;
            bool           root = false;
            bool           Matches(const NativeAnimationSequenceRowCalls::Value&) const noexcept;
        };
        struct RowState
        {
            explicit RowState(std::size_t value) noexcept : index(value) { logical.Initialize(); }
            std::size_t                         index;
            NativeAnimationSequenceLogicalState logical;
            // Accepted format belongs to this issued row and exact original
            // frame record; raw flags/parent bool cannot manufacture it. No
            // payload content validity or lifetime lease is implied.
            FrameDescriptor frames;
        };
        struct Entry
        {
            Storage         storage;
            std::uint64_t   issue = 0;
            SequencePurpose purpose{SequencePurpose::ArrayRows};
            std::uint64_t   operationOwner = 0;
            // Accepted/transferred buffers remain lifetime obligations even
            // when Initialize or a later full row copy replaces their field.
            std::vector<std::shared_ptr<CleanupResource>> frameObligations;
            // Logical state belongs to the allocation, not a CPU facade.
            // Reserve a lazy row node before a qualified write can enter.
            std::list<RowState>                rows;
            bool                               issued = false;
            std::unique_ptr<PrivateDependency> privateDependency;
            std::size_t                        privateHolds = 0;
            // Claims live on these same allocation records. Shared returned
            // receipts outlive native consumption; no missing-map inference.
            std::uint64_t                    cleanupOwner = 0;
            std::shared_ptr<CleanupResource> cleanupAck;
        };
        using Entries = std::list<Entry>;
        Entries::iterator Find(std::uint64_t generation, std::uintptr_t issuedTag) noexcept;
        ClockResult       AllocateResource(std::uintptr_t, std::uint64_t, std::size_t, Storage::Kind, CheckOwner, std::uintptr_t&, Receipt*, InvokeMalloc,
                                           SequencePurpose = SequencePurpose::ArrayRows) noexcept;
        ClockResult       FreeResource(std::uintptr_t, std::uint64_t, std::uintptr_t, Storage::Kind, CheckOwner, Receipt*, InvokeFree) noexcept;
        ClockResult       SelectRow(const NativeAnimationSequenceReferenceToken&, Entries::iterator&, RowState*&) noexcept;
        ClockResult       ReadSequenceValue(std::uintptr_t, const NativeAnimationSequenceReferenceToken&, CheckOwner, NativeAnimationSequenceValues*,
                                            std::uint32_t*) noexcept;
        ClockResult       CheckSequenceFramePointer(std::uintptr_t, std::uint64_t, std::uintptr_t, CheckOwner) noexcept;
        struct PayloadVisitContext
        {
            NativeCustomAnimSequenceController* self;
            Entry*                              array;
        };
        static ClockResult ResolveUncompressedFrame(std::size_t, const NativeAnimationSequenceRowCalls::Value&, const Storage*&, void*) noexcept;
        ClockResult        QualifyUncompressedRowsHeld(const NativeAnimationSequenceDependencyCalls::Epoch&, const PrivateDependencyKey&, std::uint16_t,
                                                       NativeAnimationSequencePayloadCalls::Operation, CheckOwner) noexcept;
        struct PrivateVisitContext
        {
            NativeCustomAnimSequenceController* self;
            Entry*                              array;
        };
        static bool        SamePrivateKey(const PrivateDependencyKey&, const PrivateDependencyKey&) noexcept;
        static ClockResult ResolvePrivateFrame(std::size_t, const NativeAnimationSequenceRowCalls::Value&, const Storage*&, void*) noexcept;
        static ClockResult ReservePrivateAction(Storage&, const NativeAnimationSequenceDependencyCalls::Epoch&, void*) noexcept;
        ClockResult        WithPrivateArray(std::uintptr_t, const PrivateDependencyKey&, CheckOwner, PrivatePreparedAction, void*, PrivateDependencyReceipt*,
                                            bool) noexcept;
        ClockResult        ReleasePrivateDependencyHeld(Entry&) noexcept;
        ClockResult PrepareFreshSequence(std::uintptr_t, const NativeAnimationSequenceReferenceToken&, std::size_t, bool, std::uintptr_t, bool compressed,
                                         CheckOwner, FrameBindReceipt*) noexcept;
        bool        CurrentFrameDescriptor(const FrameDescriptor&) const noexcept;
        void        ForgetFrameAuthority(const Storage&) noexcept;
        ClockResult BeginCleanupImpl(std::uintptr_t, const NativeCustomAnimationCleanupRequest&, NativePrivateAnimationHierarchyController&, CheckOwner,
                                     NativeCustomAnimationCleanupBeginReceipt&, SequencePurpose, CommandKind, bool alreadyHeld = false) noexcept;
        ClockResult BeginSequenceCommand(CommandKind, const NativeAnimationSequenceReferenceToken&, const NativeAnimationSequenceReferenceToken&, std::size_t,
                                         bool, bool, std::uintptr_t, NativePrivateAnimationHierarchyController&,
                                         NativeCustomAnimationSequenceOperationBeginReceipt&) noexcept;
        ClockResult ReserveFrameObligation(Entry&, Entry&, std::shared_ptr<CleanupResource>&, bool&) noexcept;
        void        RecordResourceReceipt(Entry&, const Receipt&) noexcept;
        static ClockResult ResolveCleanupFrame(std::size_t, const NativeAnimationSequenceRowCalls::Value&, const Storage*&, void*) noexcept;
        static ClockResult VerifyCleanupAction(Storage&, const NativeAnimationSequenceDependencyCalls::Epoch&, void*) noexcept;
        bool               CleanupResourceBlocked(const CleanupResource&) const noexcept;
        void               PruneCleanupHeld() noexcept;
        void               Publish() noexcept;
        Entries            entries;
        std::list<std::shared_ptr<CleanupRecord>> cleanups;
        std::uint64_t                             nextCleanupCommand = 0, nextResourceIssue = 0;
        std::uint64_t                             ownerGeneration = 0;
        bool                                      stopping = false;
        mutable std::atomic_flag                  operation = ATOMIC_FLAG_INIT;
        std::atomic<bool>                         retained{false}, recovery{false};
    };
}
