/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Module-owned empty private hierarchy storage, never a GTA allocation
 ****************************************************************************/
#pragma once
#include "NativeAnimationHierarchyAccess.h"
#include "NativeAnimationSequenceDependencyCalls.h"
#include "NativeAnimationSequencePayloadCalls.h"
#include <array>
#include <atomic>
namespace mta::game_de
{
    struct NativePrivateAnimationHierarchyStorageAccess;
    class NativePrivateAnimationHierarchyStorage final
    {
    public:
        enum class State : std::uint32_t
        {
            Fresh,
            Entered,
            Ready,
            Failed,
            PublicationPinned,
            Retired
        };
        // Actual MTA allocates a plain interface, then explicitly initializes
        // it. Keep the native40-byte hash/alignment bytes uninitialized.
        explicit NativePrivateAnimationHierarchyStorage(std::uint64_t generation) noexcept : generation(generation) {}
        ~NativePrivateAnimationHierarchyStorage() = default;
        NativePrivateAnimationHierarchyStorage(const NativePrivateAnimationHierarchyStorage&) = delete;
        NativePrivateAnimationHierarchyStorage& operator=(const NativePrivateAnimationHierarchyStorage&) = delete;
        NativePrivateAnimationHierarchyStorage(NativePrivateAnimationHierarchyStorage&&) = delete;
        NativePrivateAnimationHierarchyStorage& operator=(NativePrivateAnimationHierarchyStorage&&) = delete;
        State                                   GetState() const noexcept { return state.load(std::memory_order_acquire); }
        std::uint64_t                           OwnerGeneration() const noexcept { return generation; }
        std::uintptr_t                          Address() const noexcept { return reinterpret_cast<std::uintptr_t>(bytes.data()); }
        bool                                    CanDiscard() const noexcept { return GetState() == State::Retired; }

    private:
        friend struct NativePrivateAnimationHierarchyStorageAccess;
        alignas(std::uintptr_t) std::array<unsigned char, 40> bytes;
        std::uint64_t generation;
        // These acknowledged source-owned fields distinguish legitimate private
        // links/counts from an observed foreign write. They grant no engine
        // publication, association lifetime or native child-free authority.
        std::uintptr_t             expectedSequences = 0;
        mutable std::atomic<State> state{State::Fresh};
        std::uint16_t              expectedCount = 0;
        bool                       hashDefined = false, pointerAckPending = false;
        // A normal native return still needs the outer array/owner bookends.
        // Failed/unknown payload effects are never replayed automatically;
        // pointer detach and ordinary known storage retirement remain possible.
        bool                         postProcessBlocked = false, postProcessVerified = false;
        std::array<unsigned char, 2> postProcessPadding{};
        std::uint32_t                postProcessEntry = 0;
        static_assert(sizeof(std::atomic<State>) == 4);
        // Explicitly laid out40+8+8+4+2+1+1+8 CPU bytes avoid implicit padding.
    };
    namespace NativePrivateAnimationHierarchyCalls
    {
        using Storage = NativePrivateAnimationHierarchyStorage;
        using CheckOwner = ClockResult (*)() noexcept;
        using Write = ClockResult (*)(std::uintptr_t, std::uint32_t&) noexcept;
        enum class ScalarOperation : std::uint32_t
        {
            RunningCompressed,
            AnimationBlockID,
            NameHash,
            SequenceCount
        };
        using SequenceWrite = ClockResult (*)(std::uintptr_t, std::uintptr_t, std::uint32_t&) noexcept;
        using Epoch = NativeAnimationSequenceDependencyCalls::Epoch;
        using ScalarWrite = ClockResult (*)(std::uintptr_t, ScalarOperation, std::uint32_t, std::uint32_t&) noexcept;
        struct Receipt
        {
            std::uintptr_t hierarchy = 0;
            std::uint32_t  exceptionCode = 0;
            bool           entered = false, accepted = false, retired = false, verified = false, acknowledged = false;
        };
        using PostProcessOperation = NativeAnimationSequencePayloadCalls::Operation;
        using NativePostProcessFunction = void (*)(std::uintptr_t hierarchy);
        using InvokePostProcess = ClockResult (*)(NativePostProcessFunction, std::uintptr_t, std::uint32_t&) noexcept;
        using PayloadCheck = ClockResult (*)(const Epoch&, std::uint16_t, PostProcessOperation, void*) noexcept;
        struct PostProcessReceipt
        {
            std::uintptr_t hierarchy = 0, entryRva = 0;
            std::uint32_t  exceptionCode = 0;
            bool           entered = false, returned = false, accepted = false, verified = false;
        };
        ClockResult PostProcessEmpty(std::uintptr_t, Storage&, std::uint64_t, PostProcessOperation, CheckOwner, PostProcessReceipt* = nullptr,
                                     InvokePostProcess = nullptr) noexcept;
        ClockResult PostProcessHeld(const Epoch&, Storage&, PostProcessOperation, CheckOwner, PayloadCheck, void*, PostProcessReceipt* = nullptr,
                                    InvokePostProcess = nullptr) noexcept;
        // Only the actual outer returned array/owner receipt calls this CPU
        // acknowledgement. It cannot clear PublicationPinned or replay code.
        ClockResult AcknowledgePostProcess(Storage&, std::uint64_t, const PostProcessReceipt&) noexcept;
        ClockResult Initialize(std::uintptr_t image, Storage&, std::uint64_t, CheckOwner, Receipt* = nullptr, Write = nullptr) noexcept;
        // Source-owned private scalar assignments only; all other40 bytes and
        // hash definedness survive, including accepted writes with lost ack.
        ClockResult SetRunningCompressed(std::uintptr_t image, Storage&, std::uint64_t, bool, CheckOwner, Receipt* = nullptr, ScalarWrite = nullptr) noexcept;
        ClockResult SetAnimationBlockID(std::uintptr_t image, Storage&, std::uint64_t, std::int32_t, CheckOwner, Receipt* = nullptr,
                                        ScalarWrite = nullptr) noexcept;
        // The addressed classic hierarchy setter defines only this DWORD.
        // Define knownness on accepted writes even if postflight later fails.
        ClockResult SetNameHash(std::uintptr_t image, Storage&, std::uint64_t, std::uint32_t, CheckOwner, Receipt* = nullptr, ScalarWrite = nullptr) noexcept;
        ClockResult SetSequenceCount(std::uintptr_t, Storage&, std::uint64_t, std::uint16_t, CheckOwner, Receipt* = nullptr, ScalarWrite = nullptr) noexcept;
        // Epoch is issued only by the actual qualified array owner. These
        // helpers borrow its existing gate; they never reacquire it or grant
        // permission to release sequence/frame journals themselves.
        ClockResult SetScalarHeld(const Epoch&, Storage&, ScalarOperation, std::uint32_t, CheckOwner, Receipt* = nullptr, ScalarWrite = nullptr) noexcept;
        ClockResult SetEmptySequences(std::uintptr_t, Storage&, std::uint64_t, CheckOwner, Receipt* = nullptr, SequenceWrite = nullptr) noexcept;
        ClockResult SetSequencesHeld(const Epoch&, Storage&, std::uintptr_t, CheckOwner, Receipt* = nullptr, SequenceWrite = nullptr) noexcept;
        ClockResult AcknowledgeSequencesHeld(const Epoch&, Storage&, std::uintptr_t, CheckOwner, Receipt* = nullptr) noexcept;
        ClockResult ReadHeld(const Epoch&, const Storage&, CheckOwner, NativeAnimationHierarchyValues&) noexcept;
        ClockResult Read(std::uintptr_t image, const Storage&, std::uint64_t, CheckOwner, NativeAnimationHierarchyValues&) noexcept;
        // Genuine FEC640's no-sequence/cache0 branch clears only8/10/18.
        // This explicit restricted operation never retires children or drops
        // an active parent dependency; the controller owns that refusal.
        ClockResult RemoveEmptySequences(std::uintptr_t, Storage&, std::uint64_t, CheckOwner, Receipt* = nullptr, Write = nullptr) noexcept;
        // Only a known neverpublished empty/cache0 receiver may retire. The
        // controller performs plain CPU deletion after a retired receipt,
        // including a lost post-ack; no child/native destructor is invoked.
        ClockResult Retire(std::uintptr_t image, Storage&, std::uint64_t, CheckOwner, Receipt* = nullptr) noexcept;
        // Conservative irreversible bookkeeping for future real native entry.
        // No generic Unpin, map-erasure detach or child cleanup is provided.
        ClockResult PinPublication(Storage&, std::uint64_t) noexcept;
    }
}
