/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Retain model-ID defaults and one last-owned value per native field
 ****************************************************************************/
#pragma once
#include "NativeModelLodMemory.h"
#include <atomic>
#include <map>
namespace mta::game_de
{
    class NativeModelLodDomain final
    {
    public:
        using CheckOwner = NativeModelLodMemory::CheckOwner;
        using Exchange = NativeModelLodMemory::Exchange;
        ClockResult Read(std::uintptr_t image, std::uint64_t generation, std::uint32_t model, CheckOwner, CheckOwner afterApplication,
                         std::uint32_t& bits) noexcept;
        ClockResult ReadOriginal(std::uintptr_t image, std::uint64_t generation, std::uint32_t model, CheckOwner, CheckOwner afterApplication,
                                 std::uint32_t& bits) noexcept;
        ClockResult Set(std::uintptr_t image, std::uint64_t generation, std::uint32_t model, std::uint32_t bits, CheckOwner, CheckOwner afterApplication,
                        Exchange = nullptr) noexcept;
        ClockResult ResetAll(std::uintptr_t image, std::uint64_t generation, CheckOwner, CheckOwner afterApplication, Exchange = nullptr) noexcept;
        bool        Pending() const noexcept { return active.load() || pending.load(); }
        bool        Uncertain() const noexcept { return uncertain.load(); }
        bool        CanDiscard() const noexcept { return !Pending(); }

    private:
        enum class Phase : unsigned char
        {
            Pending,
            Restored,
            SkippedAbsent
        };
        struct Saved
        {
            NativeModelLodMemory::Origin origin;
            std::uintptr_t               storage = 0;
            std::uint32_t                original = 0;
            Phase                        phase = Phase::Pending;
        };
        struct Storage
        {
            NativeModelLodMemory::Origin origin;
            std::uint32_t                lastOwned = 0;
        };
        struct Unknown
        {
            // Plain identity and intent survive uncertainty. Borrowed Values
            // and their private UObject reader tokens never leave the stack.
            NativeModelLodMemory::Origin origin;
            std::uint32_t                desired = 0, observed = 0, exceptionCode = 0;
            ClockResult                  result = ClockResult::NotInitialized;
            bool                         entered = false, accepted = false;
        };
        // Classic caches first defaults by SDK model ID. Aliases intentionally
        // retain distinct originals and reset in ascending ID order. The
        // separate storage map recognizes our own intervening alias writes.
        std::map<std::uint32_t, Saved>    originals;
        std::map<std::uintptr_t, Storage> storageOwners;
        std::atomic_flag                  operation = ATOMIC_FLAG_INIT;
        std::atomic<bool>                 active{false}, pending{false}, uncertain{false};
        Unknown                           firstUnknown;
        void                              Publish() noexcept;
        ClockResult Write(std::uintptr_t, const NativeModelLodMemory::Value&, std::uint32_t, CheckOwner, CheckOwner, Exchange, bool& accepted) noexcept;
    };
}
