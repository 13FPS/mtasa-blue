/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Retain typed ped metadata initialization/publication acknowledgements
 ****************************************************************************/
#pragma once
#include "NativePedModelMetadataStorageCalls.h"
#include "NativeModelBankReader.h"
#include <atomic>

namespace mta::game_de::NativePedModelCreation
{
    using CheckOwner = NativeModelBankReader::CheckOwner;
    using InvokeMalloc = NativePedModelMetadataStorageCalls::InvokeMalloc;
    using InvokeFree = NativePedModelMetadataStorageCalls::InvokeFree;
    enum class Stage : std::uint32_t
    {
        Empty,
        Allocating,
        Unpublished,
        Initializing,
        InitializedUnpublished,
        Publishing,
        Published,
        Freeing,
        Released,
        FaultDebt
    };
    struct CompareExchangeReceipt
    {
        std::uintptr_t observed = 0;
        std::uint32_t  exceptionCode = 0;
        bool           returned = false, accepted = false;
    };
    struct Receipt
    {
        NativePedModelMetadataStorageCalls::Receipt allocation{}, retirement{};
        CompareExchangeReceipt                      publication{};
        std::uint64_t                               generation = 0;
        std::uint32_t                               modelIndex = 0, initExceptionCode = 0;
        std::uintptr_t                              native = 0, slotAddress = 0, previousBorrowedModel = 0;
        bool                                        initEntered = false, initReturned = false, initVerified = false;
        bool                                        publishEntered = false, publishVerified = false, unpublishedProved = false;
        bool                                        entered = false, returned = false, verified = false, debt = false;
    };
    // Test seams replace only these bounded CPU effects. The real guarded
    // writer/CAS remain available; neither seam supplies object ownership.
    using InvokeWriteDefaults = ClockResult (*)(std::uintptr_t object, std::uintptr_t pedVtable, std::uintptr_t canonicalCollision,
                                                std::uint32_t& exceptionCode) noexcept;
    using InvokeCompareExchange = ClockResult (*)(std::uintptr_t slotAddress, std::uintptr_t expectedBorrowedModel, std::uintptr_t replacement,
                                                  CompareExchangeReceipt&) noexcept;
    struct Access;
    class Record final
    {
    public:
        Record() noexcept = default;
        Record(const Record&) = delete;
        Record& operator=(const Record&) = delete;
        Record(Record&&) = delete;
        Record& operator=(Record&&) = delete;
        // The retaining runtime owns this record before native allocation.
        // Dropping CPU metadata never guesses a native free or bank rollback.
        ~Record() = default;
        Stage GetStage() const noexcept { return stage.load(std::memory_order_acquire); }
        bool  CanDiscard() const noexcept;
        bool  HasOutstandingResource() const noexcept { return !CanDiscard(); }
        // This predicate and the identity views require the same serialized
        // owner as CreateAndPublish/RetireUnpublished, not a cross-thread lease.
        bool          CanRetireUnpublished() const noexcept;
        std::uint64_t OwnerGeneration() const noexcept { return generation; }
        std::uint32_t ModelIndex() const noexcept { return modelIndex; }

    private:
        friend struct Access;
        NativeCustomAnimSequenceStorage storage;
        std::atomic<Stage>              stage{Stage::Empty};
        std::uint64_t                   generation = 0;
        std::uint32_t                   modelIndex = 0;
        Receipt                         journal{};
    };
    // One-shot creation. Accepted/unknown publication is retained, never
    // replayed or undone. The prior bank pointer is only a borrowed CAS value.
    ClockResult CreateAndPublish(std::uintptr_t image, Record&, std::uint64_t generation, std::uint32_t modelIndex, CheckOwner, Receipt* = nullptr,
                                 InvokeWriteDefaults = nullptr, InvokeCompareExchange = nullptr, InvokeMalloc = nullptr) noexcept;
    // Only a proved unpublished original allocation may take this path.
    // No model destructor, child cleanup, shared collider free or generic unpin.
    ClockResult RetireUnpublished(std::uintptr_t image, Record&, std::uint64_t generation, CheckOwner, Receipt* = nullptr, InvokeFree = nullptr) noexcept;
    ClockResult WriteDefaults(std::uintptr_t object, std::uintptr_t pedVtable, std::uintptr_t canonicalCollision, std::uint32_t& exceptionCode) noexcept;
    ClockResult CompareExchange(std::uintptr_t slotAddress, std::uintptr_t expectedBorrowedModel, std::uintptr_t replacement, CompareExchangeReceipt&) noexcept;
}
