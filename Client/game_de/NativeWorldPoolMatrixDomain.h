/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Retain exact matrix intent without claiming full world-pool backup
 ****************************************************************************/
#pragma once
#include "NativeWorldPoolMatrixMemory.h"
#include "NativeWorldPoolEntryCalls.h"
#include <atomic>
#include <map>

namespace mta::game_de
{
    class NativeWorldPoolMatrixDomain final
    {
    public:
        using Kind = NativeWorldPoolMatrixMemory::Kind;
        using Transform = NativeWorldPoolMatrixMemory::Transform;
        using CheckOwner = NativeStreamingCompletionContext::CheckOwner;
        ClockResult Read(std::uintptr_t image, std::uint64_t generation, Kind, std::uint32_t index, CheckOwner, CheckOwner afterApplication, Transform&,
                         bool&) noexcept;
        ClockResult Detach(std::uintptr_t image, std::uint64_t generation, Kind, std::uint32_t index, CheckOwner, CheckOwner afterApplication,
                           NativeWorldPoolEntryCalls::Invoker = nullptr) noexcept;
        ClockResult RestoreOne(std::uintptr_t image, std::uint64_t generation, Kind, std::uint32_t index, CheckOwner, CheckOwner afterApplication,
                               NativeWorldPoolEntryCalls::Invoker = nullptr) noexcept;
        ClockResult RestoreAll(std::uintptr_t image, std::uint64_t generation, CheckOwner, CheckOwner afterApplication,
                               NativeWorldPoolEntryCalls::Invoker = nullptr) noexcept;
        bool        Pending() const noexcept { return active.load() || pending.load(); }
        bool        Uncertain() const noexcept { return uncertain.load(); }
        bool        CanDiscard() const noexcept { return !Pending(); }

    private:
        enum class Phase : unsigned char
        {
            Prepared,
            Removing,
            Detached,
            Allocating,
            Allocated,
            Storing,
            Unknown
        };
        struct Member
        {
            std::uintptr_t object = 0, vtable = 0, klass = 0, outer = 0, item = 0;
            std::int32_t   index = -1;
            std::uint32_t  serial = 0;
        };
        struct Record
        {
            std::uintptr_t                      image = 0, bank = 0, root = 0, gameMode = 0, level = 0, world = 0, gameClass = 0, levelClass = 0;
            std::uint64_t                       generation = 0, cookie = 0;
            unsigned char                       mode = 0;
            std::size_t                         memberCount = 0;
            std::array<Member, 64>              members{};
            NativeModelReferenceHeap::Value     heap;
            NativeWorldPoolMatrixMemory::Entity entity;
            Transform                           transform{}, allocatedTransform{};
            Phase                               phase = Phase::Prepared;
            NativeWorldPoolEntryCalls::Receipt  entry;
            ClockResult                         failure = ClockResult::NotInitialized;
        };
        std::map<std::uint64_t, Record> records;
        std::atomic_flag                operation = ATOMIC_FLAG_INIT;
        std::atomic<bool>               active{false}, pending{false}, uncertain{false};
        void                            Publish() noexcept;
        void                            MarkUnknown(Record&, ClockResult) noexcept;
        void                            CatchEntered() noexcept;
        static Record Remember(std::uintptr_t, std::uint64_t, const NativeWorldPoolMatrixMemory::Value&, const NativeWorldPoolMatrixMemory::Entity&,
                               const Transform&) noexcept;
        static bool   Matches(const Record&, std::uintptr_t, std::uint64_t, const NativeWorldPoolMatrixMemory::Value&,
                              const NativeWorldPoolMatrixMemory::Entity&) noexcept;
        ClockResult   Restore(std::uintptr_t, std::uint64_t, Record&, CheckOwner, CheckOwner, NativeWorldPoolEntryCalls::Invoker);
    };
}
