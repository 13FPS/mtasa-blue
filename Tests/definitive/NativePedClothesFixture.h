/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Owned clothes/name observations shared by primitive and owner tests
 ****************************************************************************/
#pragma once
#include "../../Client/game_de/NativePedClothesProfile.h"
#include "../../Client/game_de/NativePlayerInfoProfile.h"
#include "../../Client/game_de/NativePlayerInfoExtraProfile.h"
#include "../../Client/game_de/NativeStreamingSpecialModelProfile.h"
#include "../../Client/game_de/NativeModelReferenceTransactionProfile.h"
#include "../../Client/game_de/NativeModelReferenceProfile.h"
#include "../../Client/game_de/NativeStreamingRequestProfile.h"
#include "../../Client/game_de/NativeUObjectMembershipProfile.h"
#include <windows.h>
#include <array>
#include <cstring>
#include <utility>
#include <vector>
namespace NativePedClothesFixture
{
    using Writer = void (*)(void*, std::uintptr_t, const void*, std::size_t, DWORD);
    template <class T>
    inline void Store(void* at, std::size_t offset, const T& value)
    {
        std::memcpy(static_cast<unsigned char*>(at) + offset, &value, sizeof(value));
    }
    inline void Install(std::uintptr_t image, Writer writer, void* host)
    {
        namespace P = mta::game_de::NativePedClothesProfile;
        namespace N = mta::game_de::NativeStreamingSpecialModelProfile;
        namespace H = mta::game_de::NativeModelReferenceTransactionProfile;
        for (const auto& pin : P::Pins)
            writer(host, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : mta::game_de::NativePlayerInfoProfile::Code)
            writer(host, pin.rva, pin.bytes.data(), pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : mta::game_de::NativePlayerInfoExtraProfile::Code)
            writer(host, pin.rva, pin.bytes.data(), pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : N::Code)
            writer(host, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : H::Code)
            writer(host, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : mta::game_de::NativeModelReferenceProfile::Code)
            writer(host, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : mta::game_de::NativeStreamingRequestProfile::Code)
            writer(host, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : mta::game_de::NativeUObjectMembershipProfile::pins)
            writer(host, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        writer(host, N::CrcTableRva, N::CrcTable, sizeof(N::CrcTable), PAGE_READONLY);
        writer(host, H::ClassSizesRva, H::ClassSizes.data(), sizeof(H::ClassSizes), PAGE_READONLY);
        const auto group = image + P::GroupRva;
        const auto branch = static_cast<std::uint32_t>(P::BranchRva);
        writer(host, P::GroupSlotRva, &group, sizeof(group), PAGE_READONLY);
        writer(host, P::TableSlotRva, &branch, sizeof(branch), PAGE_EXECUTE_READ);
        // These are actual owned fixture slots; no image function is executed.
        const auto control = image + mta::game_de::NativePlayerInfoProfile::PlayerControlRva;
        writer(host, mta::game_de::NativePlayerInfoProfile::PlayerVtableRva + 0x50, &control, sizeof(control), PAGE_READONLY);
        for (const auto slot : {std::pair<std::uintptr_t, std::uintptr_t>{H::HeapVtableRva + 0x10, N::HeapAllocateRva},
                                {H::HeapVtableRva + 0x20, H::HeapReallocateRva},
                                {H::HeapVtableRva + 0x38, H::HeapQuantizeRva}})
        {
            const auto function = image + slot.second;
            writer(host, slot.first, &function, sizeof(function), PAGE_READONLY);
        }
        const unsigned char ret = 0xc3;
        writer(host, N::HeapAllocateRva, &ret, 1, PAGE_EXECUTE_READ);
    }
    namespace N = mta::game_de::NativeStreamingSpecialModelProfile;
    namespace H = mta::game_de::NativeModelReferenceTransactionProfile;
    struct NameState
    {
        alignas(std::uintptr_t) std::array<unsigned char, H::HeapBytes> heap{};
        std::vector<unsigned char> nameBlock, shardHashes, cacheRows;
        NameState() : nameBlock(N::NameBlockBytes), shardHashes(0x4000), cacheRows(0x2000) {}
        void Cache(std::uintptr_t image, Writer writer, void* host, bool occupied)
        {
            (void)image;
            std::memset(cacheRows.data(), 0, cacheRows.size());
            for (std::size_t index = 0; index < 2; ++index)
            {
                std::array<unsigned char, N::CacheHeaderBytes> header{};
                Store(header.data(), 0x2c, std::int32_t{128});
                Store(header.data(), 0x30, std::int32_t{-1});
                if (occupied)
                {
                    const auto address = reinterpret_cast<std::uintptr_t>(cacheRows.data()) + index * 0x1000;
                    Store(header.data(), 0, address);
                    Store(header.data(), 8, std::int32_t{1});
                    Store(header.data(), 0xc, std::int32_t{4});
                    Store(header.data(), 0x10, std::uint32_t{1});
                    Store(header.data(), 0x28, std::int32_t{1});
                    Store(header.data(), 0x38, std::int32_t{0});
                    Store(header.data(), 0x48, std::int32_t{1});
                    auto* row = cacheRows.data() + index * 0x1000;
                    if (!index)
                        Store(row, 0, std::uint32_t{0x12345678});
                    else
                        Store(row, 8, std::uint32_t{0x12345678});
                    Store(row, 0xc, std::int32_t{-1});
                }
                writer(host, index ? N::ReverseCacheRva : N::CacheRva, header.data(), header.size(), PAGE_READWRITE);
            }
        }
        void Reset(std::uintptr_t image, Writer writer, void* host)
        {
            heap.fill(0);
            Store(heap.data(), 0, image + H::HeapVtableRva);
            const auto heapAddress = reinterpret_cast<std::uintptr_t>(heap.data());
            writer(host, H::HeapRva, &heapAddress, sizeof(heapAddress), PAGE_READWRITE);
            writer(host, H::HeapAuthorityRva, &heapAddress, sizeof(heapAddress), PAGE_READWRITE);
            std::array<unsigned char, H::LookupBytes> lookup{};
            std::array<std::uint16_t, H::ClassCount>  reverse{};
            std::size_t                               category = 0;
            for (std::size_t index = 0; index < lookup.size(); ++index)
            {
                while (category < H::ClassSizes.size() && H::ClassSizes[category] < index * 16)
                    ++category;
                lookup[index] = static_cast<unsigned char>(category);
            }
            for (std::size_t index = 0; index < reverse.size(); ++index)
                reverse[index] = H::ClassSizes[H::ClassSizes.size() - index - 1];
            writer(host, H::LookupRva, lookup.data(), lookup.size(), PAGE_READWRITE);
            writer(host, H::ReverseSizesRva, reverse.data(), sizeof(reverse), PAGE_READWRITE);
            std::memset(nameBlock.data(), 0, nameBlock.size());
            std::memset(shardHashes.data(), 0, shardHashes.size());
            std::vector<unsigned char> pool(N::NamePoolBytes);
            Store(pool.data(), 0x10, reinterpret_cast<std::uintptr_t>(nameBlock.data()));
            for (std::size_t index = 0; index < N::ShardCount; ++index)
            {
                auto* shard = pool.data() + N::ShardOffset + index * N::ShardStride;
                Store(shard, 0xc, std::uint32_t{255});
                Store(shard, 0x10, reinterpret_cast<std::uintptr_t>(shardHashes.data()) + index * 0x400);
                Store(shard, 0x18, image + N::NamePoolRva);
            }
            writer(host, N::NamePoolRva, pool.data(), pool.size(), PAGE_READWRITE);
            const unsigned char initialized = 1;
            writer(host, N::NameInitializedRva, &initialized, sizeof(initialized), PAGE_READWRITE);
            Cache(image, writer, host, false);
        }
    };
}
