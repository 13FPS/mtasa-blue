/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: One coherent internal native sparse-model map observation
 ****************************************************************************/
#pragma once
#include "NativeModelReferenceProfile.h"
#include "NativeStreamingModelContext.h"
#include "NativeStreamingRequestProfile.h"
#include "NativeImageAccess.h"
#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <utility>
#include <vector>
namespace mta::game_de::NativeModelReferenceMap
{
    namespace P = NativeModelReferenceProfile;
    namespace C = NativeStreamingModelContext;
    struct Row
    {
        std::uint32_t key = 0, count = 0;
        std::int32_t  next = -1, bucket = -1;
    };
    static_assert(sizeof(Row) == P::RowBytes);
    struct Snapshot
    {
        std::uintptr_t                            map = 0, rowsAddress = 0, bitsAddress = 0, hashAddress = 0;
        std::array<unsigned char, P::HeaderBytes> header{};
        std::vector<Row>                          rows;
        std::vector<std::uint32_t>                bits;
        std::vector<std::int32_t>                 hash;
        std::int32_t                              selected = -1;
    };
    template <class T>
    inline T Field(const Snapshot& value, std::size_t offset) noexcept
    {
        T field{};
        std::memcpy(&field, value.header.data() + offset, sizeof(field));
        return field;
    }
    inline bool PrivateWritable(std::uintptr_t address, std::size_t bytes) noexcept
    {
        if (!address || !bytes || bytes > (std::numeric_limits<std::uintptr_t>::max)() - address)
            return false;
        const auto end = address + bytes;
        void*      allocation = nullptr;
        while (address < end)
        {
            MEMORY_BASIC_INFORMATION page{};
            if (VirtualQuery(reinterpret_cast<void*>(address), &page, sizeof(page)) != sizeof(page) || page.State != MEM_COMMIT ||
                (page.Protect & (PAGE_GUARD | PAGE_NOACCESS)) || (page.Type != MEM_PRIVATE && page.Type != MEM_MAPPED) ||
                ((page.Protect & 0xff) != PAGE_READWRITE && (page.Protect & 0xff) != PAGE_WRITECOPY) || !page.RegionSize)
                return false;
            if (!allocation)
                allocation = page.AllocationBase;
            if (allocation != page.AllocationBase)
                return false;
            const auto begin = reinterpret_cast<std::uintptr_t>(page.BaseAddress);
            if (begin > address || page.RegionSize > (std::numeric_limits<std::uintptr_t>::max)() - begin || begin + page.RegionSize <= address)
                return false;
            address = (std::min)(end, begin + page.RegionSize);
        }
        return true;
    }
    inline bool Copy(std::uintptr_t address, void* output, std::size_t bytes) noexcept
    {
        if (!PrivateWritable(address, bytes))
            return false;
        SIZE_T copied = 0;
        return ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(address), output, bytes, &copied) && copied == bytes;
    }
    inline bool Disjoint(std::uintptr_t a, std::size_t an, std::uintptr_t b, std::size_t bn) noexcept
    {
        return a && b && an && bn && an <= (std::numeric_limits<std::uintptr_t>::max)() - a && bn <= (std::numeric_limits<std::uintptr_t>::max)() - b &&
               (a + an <= b || b + bn <= a);
    }
    inline bool Occupied(const Snapshot& value, std::size_t index) noexcept
    {
        return (value.bits[index / 32] & (std::uint32_t{1} << (index % 32))) != 0;
    }
    inline bool QualifyProfile(std::uintptr_t image) noexcept
    {
        std::array<unsigned char, P::MaximumPinBytes> bytes{};
        for (const auto& pin : P::Code)
            if (!pin.size || pin.size > bytes.size() || !NativeImageAccess::ReadImage(image, image + pin.rva, bytes.data(), pin.size, false, true) ||
                std::memcmp(bytes.data(), pin.bytes, pin.size))
                return false;
        return C::QualifyProfile(image);
    }
    inline ClockResult Capture(const C::Value& context, const NativeModelBankReader::Value& model, Snapshot& output, bool requireKey = true)
    {
        Snapshot value;
        if (P::MapOffset + P::HeaderBytes > (std::numeric_limits<std::uintptr_t>::max)() - context.root)
            return ClockResult::BindingChanged;
        value.map = context.root + P::MapOffset;
        if (!Copy(value.map, value.header.data(), value.header.size()))
            return ClockResult::ReadFailure;
        const auto num = Field<std::int32_t>(value, 8), capacity = Field<std::int32_t>(value, 0xc);
        const auto numBits = Field<std::int32_t>(value, 0x28), maxBits = Field<std::int32_t>(value, 0x2c);
        const auto freeHead = Field<std::int32_t>(value, 0x30), freeCount = Field<std::int32_t>(value, 0x34);
        const auto hashCount = Field<std::int32_t>(value, 0x48);
        // These are conservative admission bounds, not a claim that the
        // native allocator promises a particular maximum capacity.
        if (num < 0 || num > P::MaximumRows || capacity < num || numBits != num || maxBits < numBits || maxBits % 32 || freeCount < 0 || freeCount > num ||
            hashCount < 0 || hashCount > P::MaximumBuckets || (hashCount & (hashCount - 1)))
            return ClockResult::BindingChanged;
        value.rowsAddress = Field<std::uintptr_t>(value, 0);
        const auto externalBits = Field<std::uintptr_t>(value, 0x20), externalHash = Field<std::uintptr_t>(value, 0x40);
        if ((!externalBits && maxBits > 128) || (!externalHash && hashCount > 1) || (!hashCount && externalHash) || (num != freeCount && !hashCount))
            return ClockResult::BindingChanged;
        if (!num)
        {
            // A native first insertion grows an empty sparse map itself. No row
            // allocation, occupancy word or capacity is synthesized by this reader.
            if (value.rowsAddress || capacity || externalBits || freeHead != -1 || freeCount || (maxBits != 0 && maxBits != 128) || hashCount > 1 ||
                externalHash || (hashCount == 1 && Field<std::int32_t>(value, 0x38) != -1))
                return ClockResult::BindingChanged;
            std::array<unsigned char, P::HeaderBytes> after{};
            if (!Copy(value.map, after.data(), after.size()))
                return ClockResult::ReadFailure;
            if (value.header != after)
                return ClockResult::BindingChanged;
            if (requireKey)
                return ClockResult::NotInitialized;
            output = std::move(value);
            return ClockResult::Ok;
        }
        value.bitsAddress = externalBits ? externalBits : value.map + 0x10;
        value.hashAddress = externalHash ? externalHash : value.map + 0x38;
        const auto rowBytes = static_cast<std::size_t>(num) * sizeof(Row), bitBytes = ((static_cast<std::size_t>(num) + 31) / 32) * 4;
        const auto hashBytes = static_cast<std::size_t>(hashCount) * 4;
        if (value.rowsAddress % alignof(std::uintptr_t) || value.bitsAddress % 4 || value.hashAddress % 4 ||
            !Disjoint(value.rowsAddress, rowBytes, context.root, P::MapOffset + P::HeaderBytes) ||
            !Disjoint(value.rowsAddress, rowBytes, context.provider, NativeStreamingRequestProfile::ProviderCountOffset + 4) ||
            !Disjoint(value.rowsAddress, rowBytes, context.rows, static_cast<std::size_t>(context.count) * NativeStreamingRequestProfile::ProviderRowStride) ||
            !Disjoint(value.rowsAddress, rowBytes, model.nativeModelInfo, NativeStreamingRequestProfile::ModelPrefixBytes) ||
            !Disjoint(value.rowsAddress, rowBytes, value.bitsAddress, bitBytes) ||
            (hashBytes && !Disjoint(value.rowsAddress, rowBytes, value.hashAddress, hashBytes)) ||
            (hashBytes && !Disjoint(value.bitsAddress, bitBytes, value.hashAddress, hashBytes)) ||
            (externalBits && !Disjoint(value.bitsAddress, bitBytes, context.root, P::MapOffset + P::HeaderBytes)) ||
            (externalHash && !Disjoint(value.hashAddress, hashBytes, context.root, P::MapOffset + P::HeaderBytes)))
            return ClockResult::BindingChanged;
        value.rows.resize(static_cast<std::size_t>(num));
        value.bits.resize(bitBytes / 4);
        value.hash.resize(static_cast<std::size_t>(hashCount));
        if (!Copy(value.rowsAddress, value.rows.data(), rowBytes) || !Copy(value.bitsAddress, value.bits.data(), bitBytes) ||
            (hashBytes && !Copy(value.hashAddress, value.hash.data(), hashBytes)))
            return ClockResult::ReadFailure;
        std::array<unsigned char, P::HeaderBytes> headerAfter{};
        if (!Copy(value.map, headerAfter.data(), headerAfter.size()))
            return ClockResult::ReadFailure;
        if (value.header != headerAfter)
            return ClockResult::BindingChanged;
        std::vector<unsigned char> visited(static_cast<std::size_t>(num));
        std::vector<std::uint32_t> keys;
        keys.reserve(static_cast<std::size_t>(num - freeCount));
        std::size_t active = 0;
        for (std::int32_t bucket = 0; bucket != hashCount; ++bucket)
            for (auto index = value.hash[static_cast<std::size_t>(bucket)]; index != -1;)
            {
                if (index < 0 || index >= num || !Occupied(value, static_cast<std::size_t>(index)) || visited[static_cast<std::size_t>(index)])
                    return ClockResult::BindingChanged;
                visited[static_cast<std::size_t>(index)] = 1;
                const auto& row = value.rows[static_cast<std::size_t>(index)];
                if (row.bucket != bucket || (row.key & static_cast<std::uint32_t>(hashCount - 1)) != static_cast<std::uint32_t>(bucket))
                    return ClockResult::BindingChanged;
                if (row.key == model.modelIndex)
                {
                    if (value.selected != -1)
                        return ClockResult::BindingChanged;
                    value.selected = index;
                }
                keys.push_back(row.key);
                ++active;
                index = row.next;
            }
        std::sort(keys.begin(), keys.end());
        if (std::adjacent_find(keys.begin(), keys.end()) != keys.end())
            return ClockResult::BindingChanged;
        // Free rows reuse key/value as previous/next free indices. They
        // must never be mistaken for an occupied model with a small count.
        std::int32_t previous = -1, freeSeen = 0;
        for (auto index = freeHead; index != -1;)
        {
            if (index < 0 || index >= num || Occupied(value, static_cast<std::size_t>(index)) || visited[static_cast<std::size_t>(index)] ||
                freeSeen >= freeCount)
                return ClockResult::BindingChanged;
            const auto&  row = value.rows[static_cast<std::size_t>(index)];
            std::int32_t prev = 0, next = 0;
            std::memcpy(&prev, &row.key, 4);
            std::memcpy(&next, &row.count, 4);
            if (prev != previous)
                return ClockResult::BindingChanged;
            visited[static_cast<std::size_t>(index)] = 1;
            previous = index;
            index = next;
            ++freeSeen;
        }
        if (freeSeen != freeCount || active + static_cast<std::size_t>(freeCount) != static_cast<std::size_t>(num) ||
            std::find(visited.begin(), visited.end(), 0) != visited.end())
            return ClockResult::BindingChanged;
        if (requireKey && value.selected < 0)
            return ClockResult::NotInitialized;
        output = std::move(value);
        return ClockResult::Ok;
    }
    inline bool Same(const Snapshot& before, const Snapshot& after, bool changed, std::uint32_t target) noexcept
    {
        if (before.map != after.map || before.header != after.header || before.bits != after.bits || before.hash != after.hash ||
            before.selected != after.selected || before.rows.size() != after.rows.size())
            return false;
        for (std::size_t i = 0; i != before.rows.size(); ++i)
        {
            const auto& a = before.rows[i];
            const auto& b = after.rows[i];
            if (a.key != b.key || a.next != b.next || a.bucket != b.bucket ||
                (changed && i == static_cast<std::size_t>(before.selected) ? target : a.count) != b.count)
                return false;
        }
        return true;
    }
}
