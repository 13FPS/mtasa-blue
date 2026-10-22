/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Observe genuine pointer-key/WORD asset references before native removal
 ****************************************************************************/
#pragma once
#include "NativeStreamingRemoveProfile.h"
#include "NativeModelReferenceMap.h"
#include <algorithm>
#include <array>
#include <cstring>
#include <vector>
namespace mta::game_de::NativeStreamingRemoveAssets
{
    namespace P = NativeStreamingRemoveProfile;
    namespace M = NativeModelReferenceMap;
    struct Row
    {
        std::uintptr_t               key = 0;
        std::uint16_t                count = 0;
        std::array<unsigned char, 6> padding{};
        std::int32_t                 next = -1, bucket = -1;
    };
    static_assert(sizeof(Row) == P::AssetRowBytes);
    struct Snapshot
    {
        std::uintptr_t                            map = 0, rowsAddress = 0, bitsAddress = 0, hashAddress = 0;
        std::array<unsigned char, P::HeaderBytes> header{};
        std::vector<Row>                          rows;
        std::vector<std::uint32_t>                bits;
        std::vector<std::int32_t>                 hash;
    };
    template <class T>
    inline T Field(const Snapshot& v, std::size_t at) noexcept
    {
        T value{};
        std::memcpy(&value, v.header.data() + at, sizeof(value));
        return value;
    }
    template <class T>
    inline void Set(Snapshot& v, std::size_t at, const T& value) noexcept
    {
        std::memcpy(v.header.data() + at, &value, sizeof(value));
    }
    inline bool Occupied(const Snapshot& v, std::size_t i) noexcept
    {
        return (v.bits[i / 32] & (std::uint32_t{1} << (i % 32))) != 0;
    }
    inline std::uint32_t Bucket(std::uintptr_t key, std::uint32_t mask) noexcept
    {
        // This is the exact pointer hash used by both real native find and erase.
        // Unsigned arithmetic preserves its wrapping DWORD intermediate values.
        auto t = static_cast<std::uint32_t>(key >> 4), n = std::uint32_t{0x9e3779b9} - t;
        n ^= t << 8;
        auto c = std::uint32_t{0} - (n + t);
        c ^= n >> 13;
        t -= n;
        t -= c;
        t ^= c >> 12;
        n -= t;
        n -= c;
        n ^= t << 16;
        c -= n;
        c -= t;
        c ^= n >> 5;
        t -= n;
        t -= c;
        t ^= c >> 3;
        n -= t;
        n -= c;
        n ^= t << 10;
        c -= n;
        c -= t;
        return (c ^ (n >> 15)) & mask;
    }
    inline std::int32_t Find(const Snapshot& v, std::uintptr_t key) noexcept
    {
        for (std::size_t i = 0; i < v.rows.size(); ++i)
            if (Occupied(v, i) && v.rows[i].key == key)
                return static_cast<std::int32_t>(i);
        return -1;
    }
    inline ClockResult Capture(const NativeStreamingModelContext::Value& context, const NativeModelBankReader::Value& model, Snapshot& output)
    {
        Snapshot v;
        v.map = context.root + P::AssetMapOffset;
        if (!M::Copy(v.map, v.header.data(), v.header.size()))
            return ClockResult::ReadFailure;
        const auto num = Field<std::int32_t>(v, 8), cap = Field<std::int32_t>(v, 0xc), numBits = Field<std::int32_t>(v, 0x28),
                   maxBits = Field<std::int32_t>(v, 0x2c);
        const auto free = Field<std::int32_t>(v, 0x34), head = Field<std::int32_t>(v, 0x30), buckets = Field<std::int32_t>(v, 0x48);
        if (num < 0 || num > P::MaximumAssets || cap < num || numBits != num || maxBits < num || maxBits % 32 || free < 0 || free > num || buckets < 0 ||
            buckets > P::MaximumBuckets || (buckets & (buckets - 1)))
            return ClockResult::BindingChanged;
        v.rowsAddress = Field<std::uintptr_t>(v, 0);
        const auto eb = Field<std::uintptr_t>(v, 0x20), eh = Field<std::uintptr_t>(v, 0x40);
        if ((!eb && maxBits > 128) || (!eh && buckets > 1) || (!buckets && eh) || (num != free && !buckets))
            return ClockResult::BindingChanged;
        if (!num)
        {
            if (v.rowsAddress || cap || eb || head != -1 || free || (maxBits != 0 && maxBits != 128) || buckets > 1 || eh ||
                (buckets == 1 && Field<std::int32_t>(v, 0x38) != -1))
                return ClockResult::BindingChanged;
        }
        else
        {
            v.bitsAddress = eb ? eb : v.map + 0x10;
            v.hashAddress = eh ? eh : v.map + 0x38;
            const auto rb = static_cast<std::size_t>(num) * sizeof(Row), bb = ((static_cast<std::size_t>(num) + 31) / 32) * 4,
                       hb = static_cast<std::size_t>(buckets) * 4;
            if (v.rowsAddress % 8 || v.bitsAddress % 4 || v.hashAddress % 4 ||
                !M::Disjoint(v.rowsAddress, rb, context.root, P::AssetMapOffset + P::HeaderBytes) ||
                !M::Disjoint(v.rowsAddress, rb, context.provider, NativeStreamingRequestProfile::ProviderCountOffset + 4) ||
                !M::Disjoint(v.rowsAddress, rb, context.rows, static_cast<std::size_t>(context.count) * NativeStreamingRequestProfile::ProviderRowStride) ||
                !M::Disjoint(v.rowsAddress, rb, model.nativeModelInfo, NativeStreamingRequestProfile::ModelPrefixBytes) ||
                !M::Disjoint(v.rowsAddress, rb, v.bitsAddress, bb) || (hb && !M::Disjoint(v.rowsAddress, rb, v.hashAddress, hb)) ||
                (hb && !M::Disjoint(v.bitsAddress, bb, v.hashAddress, hb)) ||
                (eb && !M::Disjoint(v.bitsAddress, bb, context.root, P::AssetMapOffset + P::HeaderBytes)) ||
                (eh && !M::Disjoint(v.hashAddress, hb, context.root, P::AssetMapOffset + P::HeaderBytes)))
                return ClockResult::BindingChanged;
            v.rows.resize(static_cast<std::size_t>(num));
            v.bits.resize(bb / 4);
            v.hash.resize(static_cast<std::size_t>(buckets));
            if (!M::Copy(v.rowsAddress, v.rows.data(), rb) || !M::Copy(v.bitsAddress, v.bits.data(), bb) || (hb && !M::Copy(v.hashAddress, v.hash.data(), hb)))
                return ClockResult::ReadFailure;
            std::vector<unsigned char>  seen(static_cast<std::size_t>(num));
            std::vector<std::uintptr_t> keys;
            for (std::int32_t b = 0; b < buckets; ++b)
                for (auto i = v.hash[static_cast<std::size_t>(b)]; i != -1;)
                {
                    if (i < 0 || i >= num || !Occupied(v, static_cast<std::size_t>(i)) || seen[static_cast<std::size_t>(i)])
                        return ClockResult::BindingChanged;
                    seen[static_cast<std::size_t>(i)] = 1;
                    const auto& row = v.rows[static_cast<std::size_t>(i)];
                    if (!row.key || row.key % 8 || !row.count || row.bucket != b ||
                        Bucket(row.key, static_cast<std::uint32_t>(buckets - 1)) != static_cast<std::uint32_t>(b))
                        return ClockResult::BindingChanged;
                    keys.push_back(row.key);
                    i = row.next;
                }
            std::sort(keys.begin(), keys.end());
            if (std::adjacent_find(keys.begin(), keys.end()) != keys.end())
                return ClockResult::BindingChanged;
            std::int32_t previous = -1, freeSeen = 0;
            for (auto i = head; i != -1;)
            {
                if (i < 0 || i >= num || Occupied(v, static_cast<std::size_t>(i)) || seen[static_cast<std::size_t>(i)] || freeSeen >= free)
                    return ClockResult::BindingChanged;
                seen[static_cast<std::size_t>(i)] = 1;
                std::int32_t p = 0, n = 0;
                std::memcpy(&p, &v.rows[static_cast<std::size_t>(i)].key, 4);
                std::memcpy(&n, reinterpret_cast<const unsigned char*>(&v.rows[static_cast<std::size_t>(i)].key) + 4, 4);
                if (p != previous)
                    return ClockResult::BindingChanged;
                previous = i;
                i = n;
                ++freeSeen;
            }
            if (freeSeen != free || keys.size() + static_cast<std::size_t>(free) != static_cast<std::size_t>(num) ||
                std::find(seen.begin(), seen.end(), 0) != seen.end())
                return ClockResult::BindingChanged;
        }
        std::array<unsigned char, P::HeaderBytes> after{};
        if (!M::Copy(v.map, after.data(), after.size()))
            return ClockResult::ReadFailure;
        if (v.header != after)
            return ClockResult::BindingChanged;
        output = std::move(v);
        return ClockResult::Ok;
    }
    inline bool Same(const Snapshot& a, const Snapshot& b) noexcept
    {
        return a.map == b.map && a.rowsAddress == b.rowsAddress && a.bitsAddress == b.bitsAddress && a.hashAddress == b.hashAddress && a.header == b.header &&
               a.bits == b.bits && a.hash == b.hash && a.rows.size() == b.rows.size() &&
               (a.rows.empty() || std::memcmp(a.rows.data(), b.rows.data(), a.rows.size() * sizeof(Row)) == 0);
    }
    inline void ExpectedDrop(Snapshot& expected, std::uintptr_t key) noexcept
    {
        // This computes a post-call observation only. It never writes engine
        // memory, calls a destructor, allocates an engine row or retries entry.
        const auto i = Find(expected, key);
        if (i < 0)
            return;
        auto& row = expected.rows[static_cast<std::size_t>(i)];
        if (--row.count)
            return;
        auto* link = &expected.hash[static_cast<std::size_t>(row.bucket)];
        while (*link != i)
            link = &expected.rows[static_cast<std::size_t>(*link)].next;
        *link = row.next;
        const auto free = Field<std::int32_t>(expected, 0x34), head = Field<std::int32_t>(expected, 0x30);
        if (free)
            std::memcpy(&expected.rows[static_cast<std::size_t>(head)].key, &i, 4);
        const std::int32_t previous = -1, next = free ? head : -1;
        std::memcpy(&row.key, &previous, 4);
        std::memcpy(reinterpret_cast<unsigned char*>(&row.key) + 4, &next, 4);
        expected.bits[static_cast<std::size_t>(i) / 32] &= ~(std::uint32_t{1} << (i % 32));
        Set(expected, 0x30, i);
        Set(expected, 0x34, free + 1);
        // Inline occupancy/hash words are physically embedded in the header.
        if (!Field<std::uintptr_t>(expected, 0x20))
            std::memcpy(expected.header.data() + 0x10, expected.bits.data(), expected.bits.size() * 4);
        if (!Field<std::uintptr_t>(expected, 0x40))
            std::memcpy(expected.header.data() + 0x38, expected.hash.data(), expected.hash.size() * 4);
    }
}
