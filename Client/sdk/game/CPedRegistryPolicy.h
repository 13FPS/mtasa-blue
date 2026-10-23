/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Reuse MTA's existing ped slot/count policy without native ownership
 *****************************************************************************/
#pragma once
#include "CPoolClientEntity.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace MtaGame
{
    class CPedRegistryPolicy final
    {
    public:
        template <class Count>
        static bool MayCreate(Count count, std::size_t capacity) noexcept
        {
            static_assert(std::is_integral_v<Count> && std::is_unsigned_v<Count>);
            return count < capacity;
        }
        template <class Ped, std::size_t Size, class Count>
        static bool Register(std::array<SClientEntity<Ped>, Size>& entries, Count& count, std::size_t index, Ped* ped, CClientEntity* client) noexcept
        {
            static_assert(std::is_integral_v<Count> && std::is_unsigned_v<Count>);
            if (index >= Size)
                return false;
            // Existing registration replaces this index and increments count.
            // Do not reinterpret count as occupancy or silently free an old
            // wrapper: native construction/destruction belongs to the caller.
            entries[index] = {ped, client};
            ++count;
            return true;
        }
        template <class Ped, std::size_t Size>
        static SClientEntity<Ped>* Find(std::array<SClientEntity<Ped>, Size>& entries, std::size_t index) noexcept
        {
            return index < Size && entries[index].pEntity ? &entries[index] : nullptr;
        }
        template <class Ped, std::size_t Size>
        static SClientEntity<Ped>* FindByWrapper(std::array<SClientEntity<Ped>, Size>& entries, const Ped* ped) noexcept
        {
            if (!ped)
                return nullptr;
            // Slots are sparse and count can be reset independently. Search
            // the actual bounded registry, never a guessed native capacity or
            // the independently writable StoredPointer bookkeeping field.
            for (auto& entry : entries)
                if (entry.pEntity == ped)
                    return &entry;
            return nullptr;
        }
        template <class Ped, std::size_t Size>
        static bool AssociateIfPresent(std::array<SClientEntity<Ped>, Size>& entries, std::size_t index, CClientEntity* client) noexcept
        {
            auto* slot = Find(entries, index);
            if (!slot)
                return false;
            // Reassociating a client neither creates/returns a wrapper nor
            // increments the separately maintained registry count.
            slot->pClientEntity = client;
            return true;
        }
        template <class Ped, std::size_t Size, class Resolve>
        static Ped* FindFromRef(std::array<SClientEntity<Ped>, Size>& entries, std::uint32_t handle, Resolve&& resolve)
        {
            // The real native resolver must run first, even if a CPU wrapper
            // remains in this slot. Its generation result is not replaceable
            // by a cached pointer, a numeric handle1 or a count check.
            if (!resolve(handle))
                return nullptr;
            const auto  index = static_cast<std::size_t>(handle >> 8);
            const auto* slot = Find(entries, index);
            return slot ? slot->pEntity : nullptr;
        }
        template <class Ped, std::size_t Size>
        static Ped* Clear(std::array<SClientEntity<Ped>, Size>& entries, std::size_t index) noexcept
        {
            if (index >= Size)
                return nullptr;
            auto* ped = entries[index].pEntity;
            entries[index] = {nullptr, nullptr};
            return ped;
        }
        template <class Count>
        static void FinishRemoval(Count& count) noexcept
        {
            static_assert(std::is_integral_v<Count> && std::is_unsigned_v<Count>);
            // Actual classic removal decrements only after destructor work.
            // Keep its unsigned arithmetic; this helper owns no deletion gate.
            --count;
        }
        template <class Count>
        static void ResetCount(Count& count) noexcept
        {
            static_assert(std::is_integral_v<Count> && std::is_unsigned_v<Count>);
            // InitLocalPlayer's established reset leaves registered slots in
            // place so the existing-slot association path remains meaningful.
            count = 0;
        }
        template <class Ped, std::size_t Size>
        static void InvalidateLocalClient(std::array<SClientEntity<Ped>, Size>& entries) noexcept
        {
            static_assert(Size > 0);
            entries[0].pClientEntity = nullptr;
        }
    };
}
