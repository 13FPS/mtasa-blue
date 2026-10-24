/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Keep issued native animation allocations with their callback owner
 ****************************************************************************/
#pragma once
#include "NativeCustomAnimSequenceStorage.h"
#include <atomic>
#include <list>

namespace mta::game_de
{
    class NativeCustomAnimSequenceController final
    {
    public:
        using Storage = NativeCustomAnimSequenceStorage;
        using Receipt = NativeCustomAnimSequenceStorageCalls::Receipt;
        using CheckOwner = NativeCustomAnimSequenceStorageCalls::CheckOwner;
        using InvokeMalloc = NativeCustomAnimSequenceStorageCalls::InvokeMalloc;
        using InvokeFree = NativeCustomAnimSequenceStorageCalls::InvokeFree;
        struct RetireProgress
        {
            std::size_t scanned = 0, attempted = 0, released = 0, retained = 0;
        };
        NativeCustomAnimSequenceController() noexcept = default;
        NativeCustomAnimSequenceController(const NativeCustomAnimSequenceController&) = delete;
        NativeCustomAnimSequenceController& operator=(const NativeCustomAnimSequenceController&) = delete;
        NativeCustomAnimSequenceController(NativeCustomAnimSequenceController&&) = delete;
        NativeCustomAnimSequenceController& operator=(NativeCustomAnimSequenceController&&) = delete;

        ClockResult Allocate(std::uintptr_t image, std::uint64_t generation, std::size_t capacity, CheckOwner, std::uintptr_t& output, Receipt* = nullptr,
                             InvokeMalloc = nullptr) noexcept;
        ClockResult FreeStorage(std::uintptr_t image, std::uint64_t generation, std::uintptr_t issuedTag, CheckOwner, Receipt* = nullptr,
                                InvokeFree = nullptr) noexcept;
        ClockResult Retire(std::uintptr_t image, std::uint64_t generation, std::size_t budget, CheckOwner cleanupOwner, RetireProgress&,
                           InvokeFree = nullptr) noexcept;
        // This is private-owner bookkeeping for a future qualified hierarchy
        // attachment, not a public raw-pointer adoption or detachment API.
        ClockResult PinPublication(std::uint64_t generation, std::uintptr_t issuedTag, std::uintptr_t hierarchyAffinity) noexcept;

        // Only these conservative summaries may be read by producer threads.
        // The actual runtime retains records, its callback and module pin
        // until every allocation is discharged. Destruction never frees GTA.
        bool IsRetained() const noexcept { return retained.load(std::memory_order_acquire); }
        bool RecoveryPending() const noexcept { return recovery.load(std::memory_order_acquire); }

    private:
        struct Entry
        {
            Storage storage;
            bool    issued = false;
        };
        using Entries = std::list<Entry>;
        Entries::iterator Find(std::uint64_t generation, std::uintptr_t issuedTag) noexcept;
        void              Publish() noexcept;
        Entries           entries;
        std::uint64_t     ownerGeneration = 0;
        bool              stopping = false;
        std::atomic_flag  operation = ATOMIC_FLAG_INIT;
        std::atomic<bool> retained{false}, recovery{false};
    };
}
