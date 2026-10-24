/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Durable private CPU hierarchy records on the existing runtime owner
 ****************************************************************************/
#pragma once
#include "NativePrivateAnimationHierarchyStorage.h"
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
        struct RetireProgress
        {
            std::size_t scanned = 0, attempted = 0, released = 0, retained = 0;
        };
        ClockResult Create(std::uintptr_t image, std::uint64_t generation, CheckOwner, Token& output, Receipt* = nullptr) noexcept;
        ClockResult Initialize(std::uintptr_t image, const Token&, CheckOwner, Receipt* = nullptr) noexcept;
        ClockResult Destroy(std::uintptr_t image, const Token&, CheckOwner, Receipt* = nullptr) noexcept;
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
        struct Entry
        {
            explicit Entry(std::uint64_t generation) noexcept : storage(generation) {}
            Storage storage;
            bool    issued = false;
        };
        using Entries = std::list<Entry>;
        Entries::iterator Find(const Token&) noexcept;
        void              Publish() noexcept;
        Entries           entries;
        bool              stopping = false;
        std::atomic_flag  operation = ATOMIC_FLAG_INIT;
        std::atomic<bool> retained{false}, recovery{false};
    };
}
