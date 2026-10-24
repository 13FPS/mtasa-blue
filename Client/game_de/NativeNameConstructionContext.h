/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Share existing native name-cache and heap observation across callers
 ****************************************************************************/
#pragma once
#include "NativeStreamingSpecialModelProfile.h"
#include "NativeStreamingCompletionContext.h"
#include "NativeModelReferenceHeap.h"
#include <windows.h>
#include <algorithm>
#include <array>
#include <cstring>
#include <vector>
namespace mta::game_de::NativeNameConstructionContext
{
    namespace P = NativeStreamingSpecialModelProfile;
    namespace D = NativeStreamingCompletionContext;
    namespace M = NativeModelReferenceMap;
    namespace H = NativeModelReferenceHeap;
    inline bool CallerByte(std::uintptr_t address, unsigned char& output) noexcept
    {
        // SDK input can be a literal in Core or another client DLL. Its
        // readable bytes carry no native-object/image authority; native
        // world, model and name-cache reads retain their stricter readers.
        MEMORY_BASIC_INFORMATION page{};
        if (!address || VirtualQuery(reinterpret_cast<void*>(address), &page, sizeof(page)) != sizeof(page) || page.State != MEM_COMMIT ||
            (page.Protect & (PAGE_GUARD | PAGE_NOACCESS)))
            return false;
        const auto protection = page.Protect & 0xff;
        if (protection != PAGE_READONLY && protection != PAGE_READWRITE && protection != PAGE_WRITECOPY && protection != PAGE_EXECUTE_READ &&
            protection != PAGE_EXECUTE_READWRITE && protection != PAGE_EXECUTE_WRITECOPY)
            return false;
        SIZE_T copied = 0;
        return ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<const void*>(address), &output, sizeof(output), &copied) && copied == sizeof(output);
    }
    struct Cache
    {
        std::array<unsigned char, P::CacheHeaderBytes> header{};
        std::vector<unsigned char>                     rows;
        std::vector<std::uint32_t>                     bits;
        std::vector<std::int32_t>                      hash;
    };
    struct Names
    {
        H::Value                                                             heap;
        std::array<Cache, 2>                                                 caches;
        std::uint32_t                                                        block = 0, cursor = 0;
        std::vector<std::uintptr_t>                                          blocks;
        std::array<std::array<unsigned char, P::ShardStride>, P::ShardCount> shards{};
        std::array<std::vector<std::uint32_t>, P::ShardCount>                hashes;
    };
    template <class T, class Bytes>
    inline T Field(const Bytes& bytes, std::size_t offset) noexcept
    {
        T value{};
        std::memcpy(&value, bytes.data() + offset, sizeof(value));
        return value;
    }
    inline bool Profile(std::uintptr_t image) noexcept
    {
        std::array<unsigned char, P::MaximumPinBytes> bytes{};
        for (const auto& pin : P::Code)
            if (!pin.size || pin.size > bytes.size() || !NativeImageAccess::ReadImage(image, image + pin.rva, bytes.data(), pin.size, false, true) ||
                std::memcmp(bytes.data(), pin.bytes, pin.size))
                return false;
        return H::ReadOnlyImage(image, image + P::CrcTableRva, sizeof(P::CrcTable)) &&
               NativeImageAccess::ReadImage(image, image + P::CrcTableRva, bytes.data(), sizeof(P::CrcTable)) &&
               !std::memcmp(bytes.data(), P::CrcTable, sizeof(P::CrcTable)) && H::QualifyProfile(image) && D::Profile(image);
    }
    inline std::uint32_t NameHash(std::uint64_t key) noexcept
    {
        // Windows SDK headers also define `small`; keep the native hash
        // expression independent of that legacy type macro in MTA builds.
        const auto low = static_cast<std::uint32_t>(key), lowWord = low & 0xffff;
        return (lowWord >> 4) + (low >> 16) * 0x80001u + lowWord + (lowWord << 16) + static_cast<std::uint32_t>(key >> 32);
    }
    inline ClockResult ReadCache(std::uintptr_t image, std::uintptr_t rva, bool reverse, Cache& output)
    {
        Cache      value;
        const auto map = image + rva;
        if (!NativeImageAccess::ReadImage(image, map, value.header.data(), value.header.size(), true))
            return ClockResult::ReadFailure;
        const auto num = Field<std::int32_t>(value.header, 8), capacity = Field<std::int32_t>(value.header, 0xc);
        const auto numBits = Field<std::int32_t>(value.header, 0x28), maxBits = Field<std::int32_t>(value.header, 0x2c);
        const auto freeHead = Field<std::int32_t>(value.header, 0x30), freeCount = Field<std::int32_t>(value.header, 0x34);
        const auto buckets = Field<std::int32_t>(value.header, 0x48);
        const auto rows = Field<std::uintptr_t>(value.header, 0), externalBits = Field<std::uintptr_t>(value.header, 0x20);
        const auto externalHash = Field<std::uintptr_t>(value.header, 0x40);
        if (num < 0 || capacity < num || capacity > P::MaximumCacheRows || numBits != num || maxBits < numBits || maxBits > P::MaximumCacheRows ||
            maxBits % 32 || freeCount < 0 || freeCount > num || buckets < 0 || buckets > P::MaximumBuckets || (buckets & (buckets - 1)) ||
            (!externalBits && maxBits > 128) || (!externalHash && buckets > 1) || (!buckets && externalHash) || (num != freeCount && !buckets))
            return ClockResult::BindingChanged;
        if (!num)
        {
            if (rows || capacity || externalBits || freeHead != -1 || freeCount || (maxBits != 0 && maxBits != 128) || externalHash || buckets > 1 ||
                (buckets && Field<std::int32_t>(value.header, 0x38) != -1))
                return ClockResult::BindingChanged;
        }
        else
        {
            const auto bits = externalBits ? externalBits : map + 0x10, hash = externalHash ? externalHash : map + 0x38;
            const auto rowBytes = static_cast<std::size_t>(num) * P::CacheRowBytes, bitBytes = ((static_cast<std::size_t>(num) + 31) / 32) * 4;
            const auto hashBytes = static_cast<std::size_t>(buckets) * 4;
            if (rows % 4 || bits % 4 || hash % 4 || !M::PrivateWritable(rows, static_cast<std::size_t>(capacity) * P::CacheRowBytes) ||
                (externalBits && !M::PrivateWritable(bits, static_cast<std::size_t>(maxBits) / 8)) || !M::Disjoint(rows, rowBytes, bits, bitBytes) ||
                !M::Disjoint(rows, rowBytes, hash, hashBytes) || !M::Disjoint(bits, bitBytes, hash, hashBytes))
                return ClockResult::BindingChanged;
            value.rows.resize(rowBytes);
            value.bits.resize(bitBytes / 4);
            value.hash.resize(static_cast<std::size_t>(buckets));
            if (!M::Copy(rows, value.rows.data(), rowBytes) ||
                !(externalBits ? M::Copy(bits, value.bits.data(), bitBytes) : NativeImageAccess::ReadImage(image, bits, value.bits.data(), bitBytes, true)) ||
                !(externalHash ? M::Copy(hash, value.hash.data(), hashBytes) : NativeImageAccess::ReadImage(image, hash, value.hash.data(), hashBytes, true)))
                return ClockResult::ReadFailure;
            std::vector<unsigned char> visited(static_cast<std::size_t>(num));
            std::vector<std::uint64_t> keys;
            const auto                 occupied = [&](std::int32_t index)
            { return (value.bits[static_cast<std::size_t>(index) / 32] & (std::uint32_t{1} << (index % 32))) != 0; };
            const auto integer = [&](std::int32_t index, std::size_t offset)
            { return Field<std::int32_t>(value.rows, static_cast<std::size_t>(index) * P::CacheRowBytes + offset); };
            for (std::int32_t bucket = 0; bucket != buckets; ++bucket)
                for (auto index = value.hash[static_cast<std::size_t>(bucket)]; index != -1; index = integer(index, 0xc))
                {
                    if (index < 0 || index >= num || !occupied(index) || visited[static_cast<std::size_t>(index)])
                        return ClockResult::BindingChanged;
                    visited[static_cast<std::size_t>(index)] = 1;
                    const auto offset = static_cast<std::size_t>(index) * P::CacheRowBytes;
                    const auto key = reverse ? Field<std::uint64_t>(value.rows, offset) : Field<std::uint32_t>(value.rows, offset);
                    const auto hashKey = reverse ? NameHash(key) : static_cast<std::uint32_t>(key);
                    if (integer(index, 0x10) != bucket || (hashKey & static_cast<std::uint32_t>(buckets - 1)) != static_cast<std::uint32_t>(bucket))
                        return ClockResult::BindingChanged;
                    keys.push_back(key);
                }
            std::sort(keys.begin(), keys.end());
            if (std::adjacent_find(keys.begin(), keys.end()) != keys.end())
                return ClockResult::BindingChanged;
            std::int32_t previous = -1, freeSeen = 0;
            for (auto index = freeHead; index != -1;)
            {
                if (index < 0 || index >= num || occupied(index) || visited[static_cast<std::size_t>(index)] || freeSeen >= freeCount ||
                    integer(index, 0) != previous)
                    return ClockResult::BindingChanged;
                visited[static_cast<std::size_t>(index)] = 1;
                previous = index;
                index = integer(index, 4);
                ++freeSeen;
            }
            if (freeSeen != freeCount || keys.size() + static_cast<std::size_t>(freeSeen) != static_cast<std::size_t>(num) ||
                std::find(visited.begin(), visited.end(), 0) != visited.end())
                return ClockResult::BindingChanged;
        }
        std::array<unsigned char, P::CacheHeaderBytes> after{};
        if (!NativeImageAccess::ReadImage(image, map, after.data(), after.size(), true))
            return ClockResult::ReadFailure;
        if (value.header != after)
            return ClockResult::BindingChanged;
        output = std::move(value);
        return ClockResult::Ok;
    }
    inline ClockResult ReadNames(std::uintptr_t image, Names& output)
    {
        Names value;
        auto  result = H::Capture(image, value.heap);
        if (result != ClockResult::Ok)
            return result;
        std::uintptr_t allocate = 0;
        const auto     expected = value.heap.current == value.heap.authority ? P::HeapAllocateRva : P::ProxyAllocateRva;
        if (!NativeImageAccess::ReadImage(image, value.heap.currentVtable + 0x10, &allocate, sizeof(allocate)) || allocate != image + expected ||
            !NativeImageAccess::ImagePages(image, allocate, 1, false, true))
            return ClockResult::BindingChanged;
        unsigned char initialized = 0;
        if (!NativeImageAccess::ReadImage(image, image + P::NameInitializedRva, &initialized, 1, true))
            return ClockResult::ReadFailure;
        if (initialized != 1)
            return ClockResult::NotInitialized;
        const auto pool = image + P::NamePoolRva;
        if (!NativeImageAccess::ImagePages(image, pool, P::NamePoolBytes, true) || !NativeImageAccess::ReadImage(image, pool + 8, &value.block, 4, true) ||
            !NativeImageAccess::ReadImage(image, pool + 0xc, &value.cursor, 4, true))
            return ClockResult::ReadFailure;
        if (value.block >= P::NameBlockCount || value.cursor > P::NameBlockBytes || value.cursor % 2)
            return ClockResult::BindingChanged;
        value.blocks.resize(static_cast<std::size_t>(value.block) + 1);
        if (!NativeImageAccess::ReadImage(image, pool + 0x10, value.blocks.data(), value.blocks.size() * sizeof(std::uintptr_t), true))
            return ClockResult::ReadFailure;
        for (const auto block : value.blocks)
            if (block % 2 || !M::PrivateWritable(block, P::NameBlockBytes))
                return ClockResult::BindingChanged;
        for (std::size_t index = 0; index != P::ShardCount; ++index)
        {
            auto& shard = value.shards[index];
            if (!NativeImageAccess::ReadImage(image, pool + P::ShardOffset + index * P::ShardStride, shard.data(), shard.size(), true))
                return ClockResult::ReadFailure;
            const auto count = Field<std::uint32_t>(shard, 8), mask = Field<std::uint32_t>(shard, 0xc);
            const auto hash = Field<std::uintptr_t>(shard, 0x10);
            if (Field<std::uintptr_t>(shard, 0x18) != pool || mask < 0xff || mask >= static_cast<std::uint32_t>(P::MaximumBuckets) || (mask & (mask + 1)) ||
                count > mask || hash % 4)
                return ClockResult::BindingChanged;
            auto& cells = value.hashes[index];
            cells.resize(static_cast<std::size_t>(mask) + 1);
            if (!M::Copy(hash, cells.data(), cells.size() * 4))
                return ClockResult::ReadFailure;
            std::size_t used = 0;
            for (const auto cell : cells)
                if (cell)
                {
                    ++used;
                    const auto id = cell & 0x1fffffffu, blockIndex = id >> 16, offset = (id & 0xffff) * 2;
                    if (blockIndex > value.block || offset + 2 > P::NameBlockBytes)
                        return ClockResult::BindingChanged;
                    std::uint16_t entry = 0;
                    if (!M::Copy(value.blocks[blockIndex] + offset, &entry, 2))
                        return ClockResult::ReadFailure;
                    const auto bytes = static_cast<std::uint32_t>(entry >> 6) * ((entry & 1) ? 2u : 1u);
                    if (offset + 2 + bytes > P::NameBlockBytes || (blockIndex == value.block && offset + 2 + bytes > value.cursor))
                        return ClockResult::BindingChanged;
                }
            if (used != count)
                return ClockResult::BindingChanged;
        }
        if ((result = ReadCache(image, P::CacheRva, false, value.caches[0])) != ClockResult::Ok ||
            (result = ReadCache(image, P::ReverseCacheRva, true, value.caches[1])) != ClockResult::Ok)
            return result;
        output = std::move(value);
        return ClockResult::Ok;
    }
    inline bool Same(const Names& a, const Names& b) noexcept
    {
        if (!H::Same(a.heap, b.heap) || a.block != b.block || a.cursor != b.cursor || a.blocks != b.blocks || a.shards != b.shards || a.hashes != b.hashes)
            return false;
        for (std::size_t index = 0; index != a.caches.size(); ++index)
            if (a.caches[index].header != b.caches[index].header || a.caches[index].rows != b.caches[index].rows ||
                a.caches[index].bits != b.caches[index].bits || a.caches[index].hash != b.caches[index].hash)
                return false;
        return true;
    }
}
