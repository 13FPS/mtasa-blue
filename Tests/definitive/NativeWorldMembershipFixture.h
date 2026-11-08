/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Owned full native head banks and fixed Single/Double allocations
 ****************************************************************************/
#pragma once
#include "NativeWorldPoolMatrixFixture.h"
#include "../../Client/game_de/NativeWorldMembershipMemory.h"
#include <vector>
namespace NativeWorldMembershipFixture
{
    namespace M = mta::game_de::NativeWorldMembershipMemory;
    namespace P = mta::game_de::NativeWorldMembershipProfile;
    namespace A = mta::game_de::NativeWorldAddProfile;
    namespace W = NativeWorldIplFixture;
    struct State
    {
        NativeWorldPoolMatrixFixture::State entities;
        W::Pool                             singles{static_cast<std::int32_t>(A::SingleCapacity), A::SingleBytes};
        W::Pool                             doubles{static_cast<std::int32_t>(A::DoubleCapacity), A::DoubleBytes};
        std::uintptr_t                      image = 0;
        W::Writer                           writer = nullptr;
        void*                               host = nullptr;
        M::Kind                             kind = M::Kind::Building;
        State(std::uintptr_t selected, W::Writer selectedWriter, void* selectedHost)
            : entities(selected, selectedWriter, selectedHost), image(selected), writer(selectedWriter), host(selectedHost)
        {
            Reset();
        }
        bool           Ready() const noexcept { return entities.Ready() && singles.Ready() && doubles.Ready(); }
        std::uintptr_t Entity() const noexcept { return kind == M::Kind::Building ? entities.buildings.Row(0) : entities.dummies.Row(0); }
        template <class T>
        void Global(std::uintptr_t rva, const T& value, DWORD protection = PAGE_READWRITE)
        {
            writer(host, rva, &value, sizeof(value), protection);
        }
        void Reset(M::Kind selected = M::Kind::Building)
        {
            if (!Ready())
                return;
            kind = selected;
            entities.Reset();
            singles.Reset();
            doubles.Reset();
            for (const auto& pin : A::Code)
                writer(host, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
            for (const auto& scalar : A::Scalars)
                Global(scalar.rva, scalar.bits, PAGE_READONLY);
            for (const auto& slot : A::Slots)
                Global(slot.rva, image + slot.target, PAGE_READONLY);
            for (const auto& pin : P::MovingCode)
                writer(host, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
            Global(A::SingleRootRva, singles.Address());
            Global(A::DoubleRootRva, doubles.Address());
            // These are the native finite banks, not an enlarged readable
            // allocation offered as evidence for their unknown extent.
            std::vector<std::uintptr_t> heads(P::NormalHeads + P::RepeatHeads + P::LodHeads, 0);
            writer(host, P::NormalRva, heads.data(), heads.size() * sizeof(std::uintptr_t), PAGE_READWRITE);
            Global(P::MovingRva, std::uintptr_t{0});
        }
        void Node(unsigned pool, std::size_t index, std::uintptr_t item, std::uintptr_t next = 0, std::uintptr_t previous = 0)
        {
            auto& target = pool ? doubles : singles;
            target.flags[index] = static_cast<unsigned char>(index % 127);
            W::Put(target.Row(index), 0, item);
            W::Put(target.Row(index), 8, next);
            if (pool)
                W::Put(target.Row(index), 0x10, previous);
        }
        void EveryGroup()
        {
            Node(0, 0, Entity());
            Node(0, 1, Entity());
            Node(1, 0, Entity());
            Node(1, 1, Entity(), doubles.Row(2));
            Node(1, 2, Entity(), 0, doubles.Row(1));
            Node(1, 3, Entity());
            Node(1, 4, Entity());
            Node(1, 5, Entity());
            Global(P::NormalRva + (P::NormalHeads - 2) * 8, singles.Row(0));
            Global(P::NormalRva + (P::NormalHeads - 1) * 8, doubles.Row(0));
            Global(P::RepeatRva + (P::RepeatHeads - 3) * 8, doubles.Row(1));
            Global(P::RepeatRva + (P::RepeatHeads - 2) * 8, doubles.Row(3));
            Global(P::RepeatRva + (P::RepeatHeads - 1) * 8, doubles.Row(4));
            Global(P::LodRva + (P::LodHeads - 1) * 8, singles.Row(1));
            Global(P::MovingRva, doubles.Row(5));
            // Legitimately occupied non-world allocations are not missing
            // world heads. Their payloads are never dereferenced or released.
            Node(0, 10, Entity(), std::uintptr_t{0x13579});
            Node(1, 10, Entity(), std::uintptr_t{0x24681}, std::uintptr_t{0x24689});
        }
    };
}
