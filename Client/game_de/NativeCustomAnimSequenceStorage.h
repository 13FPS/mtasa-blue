/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Private native allocation authority for MTA custom sequence storage
 ****************************************************************************/
#pragma once
#include "NativeModelReferenceHeap.h"
#include <game/CGameClockService.h>
#include <atomic>
#include <cstddef>
#include <cstdint>
namespace mta::game_de
{
    struct NativeCustomAnimSequenceStorageAccess;
    class NativeCustomAnimSequenceStorage final
    {
    public:
        enum class State : std::uint32_t
        {
            Empty,
            Preparing,
            AllocateEntered,
            LiveUnpublished,
            PublicationPinned,
            FreeEntered,
            Released,
            FaultDebt
        };
        NativeCustomAnimSequenceStorage() noexcept = default;
        NativeCustomAnimSequenceStorage(const NativeCustomAnimSequenceStorage&) = delete;
        NativeCustomAnimSequenceStorage& operator=(const NativeCustomAnimSequenceStorage&) = delete;
        NativeCustomAnimSequenceStorage(NativeCustomAnimSequenceStorage&&) = delete;
        NativeCustomAnimSequenceStorage& operator=(NativeCustomAnimSequenceStorage&&) = delete;
        // The real runtime keeps outstanding records/module ownership alive.
        // A CPU destructor never invokes native cleanup or guesses free state.
        ~NativeCustomAnimSequenceStorage() = default;
        State GetState() const noexcept { return state.load(std::memory_order_acquire); }
        bool  CanDiscard() const noexcept;
        bool  HasOutstandingResource() const noexcept { return !CanDiscard(); }
        bool  CanFreeStorage() const noexcept;
        // These bookkeeping views are for the serialized owning controller,
        // not native address publication or cross-thread object observation.
        std::uintptr_t IssuedTag() const noexcept { return tag; }
        std::size_t    Capacity() const noexcept { return capacity; }
        std::uint64_t  OwnerGeneration() const noexcept { return generation; }
        std::uintptr_t PublicationAffinity() const noexcept { return hierarchyAffinity; }

    private:
        friend struct NativeCustomAnimSequenceStorageAccess;
        NativeModelReferenceHeap::Value origin{};
        std::uintptr_t                  base = 0, tag = 0, hierarchyAffinity = 0;
        std::size_t                     capacity = 0, bytes = 0;
        std::uint64_t                   generation = 0;
        std::uint32_t                   effects = 0;
        std::atomic<State>              state{State::Empty};
    };
    namespace NativeCustomAnimSequenceStorageCalls
    {
        using CheckOwner = ClockResult (*)() noexcept;
        using MallocFunction = std::uintptr_t (*)(std::uintptr_t heap, std::size_t bytes, std::uint32_t alignment);
        using FreeFunction = void (*)(std::uintptr_t heap, std::uintptr_t originalBase);
        using InvokeMalloc = ClockResult (*)(MallocFunction, std::uintptr_t, std::size_t, std::uint32_t, std::uintptr_t&, std::uint32_t&) noexcept;
        using InvokeFree = ClockResult (*)(FreeFunction, std::uintptr_t, std::uintptr_t, std::uint32_t&) noexcept;
        struct Receipt
        {
            std::uintptr_t entryRva = 0, heap = 0, authority = 0, base = 0, readyTag = 0;
            std::size_t    capacity = 0, bytes = 0;
            std::uint64_t  generation = 0;
            std::uint32_t  exceptionCode = 0;
            bool           entered = false, returned = false, allocated = false, initialized = false, released = false, verified = false, debt = false;
        };
        ClockResult Allocate(std::uintptr_t image, NativeCustomAnimSequenceStorage&, std::uint64_t generation, std::size_t capacity, CheckOwner,
                             Receipt* = nullptr, InvokeMalloc = nullptr) noexcept;
        // Storage-only free must never run FEC640 or touch transferred frames.
        // A known returned free consumes the record before post-ack checks.
        ClockResult FreeStorage(std::uintptr_t image, NativeCustomAnimSequenceStorage&, std::uint64_t generation, CheckOwner, Receipt* = nullptr,
                                InvokeFree = nullptr) noexcept;
        // Internal conservative bookkeeping before a real owner publishes an
        // array. It grants no hierarchy authority and has no generic Unpin.
        ClockResult PinPublication(NativeCustomAnimSequenceStorage&, std::uint64_t generation, std::uintptr_t hierarchyAffinity) noexcept;
        // The runtime maps trusted caller-owned currently issued/unfreed tags
        // to these records. A raw tag pair carries no serial capability: do
        // not promise stale-pointer ABA detection if an address is reissued.
    }
}
