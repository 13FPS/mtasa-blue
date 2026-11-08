/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: One Runtime's real dummy defaults and independent native model holds
 *****************************************************************************/
#pragma once
#include "NativeModelDummiesMemory.h"
#include <atomic>
#include <map>

namespace mta::game_de
{
    class NativeModelDummiesDomain final
    {
    public:
        using CheckOwner = NativeModelDummiesMemory::CheckOwner;
        NativeModelDummiesDomain() noexcept = default;
        NativeModelDummiesDomain(const NativeModelDummiesDomain&) = delete;
        NativeModelDummiesDomain& operator=(const NativeModelDummiesDomain&) = delete;
        ClockResult               Read(std::uintptr_t, std::uint64_t, std::uint32_t, CheckOwner, CheckOwner, NativeModelDummyPositions&) noexcept;
        ClockResult ReadCachedDefault(std::uintptr_t, std::uint64_t, std::uint32_t, std::uint32_t, CheckOwner, CheckOwner, NativeModelDummyVector&,
                                      bool&) noexcept;
        ClockResult Set(std::uintptr_t, std::uint64_t, std::uint32_t, std::uint32_t, const NativeModelDummyVector&, CheckOwner, CheckOwner) noexcept;
        ClockResult ResetOne(std::uintptr_t, std::uint64_t, std::uint32_t, bool, CheckOwner, CheckOwner) noexcept;
        ClockResult ResetAll(std::uintptr_t, std::uint64_t, CheckOwner, CheckOwner) noexcept;
        // Stop/no-inFlight may discard only CPU preparations and acknowledged
        // restoration/release records. This method never enters native memory.
        ClockResult RetireUnacquired(std::uint64_t) noexcept;
        bool        Pending() const noexcept { return active.load() || pending.load(); }
        bool        Active() const noexcept { return active.load(); }
        bool        Uncertain() const noexcept { return uncertain.load(); }

    private:
        struct Saved
        {
            NativeModelDummiesMemory::VectorBits bits{};
            bool                                 restored = false;
        };
        struct Record
        {
            NativeModelDummiesMemory::Origin     origin;
            NativeModelBackingReferenceToken     token;
            std::map<std::uint32_t, Saved>       originals;
            NativeModelDummiesMemory::Receipt    write;
            NativeModelReferenceRelease::Receipt release;
            bool                                 cacheActive = false, held = false, restored = false, released = false;
        };
        // This private instance has its own one-family-per-model namespace.
        // Dummy tokens never enter the facade family's controller or public API.
        NativeModelBackingReferenceController references;
        std::map<std::uint32_t, Record>       records;
        std::uint64_t                         generation = 0;
        std::atomic<bool>                     active{false}, pending{false}, uncertain{false};
        ClockResult                           Admit(std::uint64_t, CheckOwner, CheckOwner) noexcept;
        ClockResult                           PrepareReference(std::uint32_t, Record&, CheckOwner) noexcept;
        ClockResult                           Restore(std::uintptr_t, std::uint32_t, Record&, bool, CheckOwner, CheckOwner) noexcept;
        ClockResult                           PurgeReleased() noexcept;
        void                                  Publish() noexcept;
    };
}
