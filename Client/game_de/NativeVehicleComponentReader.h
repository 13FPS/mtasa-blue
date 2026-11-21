/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Copy descendant bone and attachment names of one registered vehicle
 *****************************************************************************/
#pragma once
#include "NativeImageAccess.h"
#include "NativeModelWheelProfile.h"
#include "NativeStreamingModelContext.h"
#include "NativeStreamingRequestProfile.h"
#include "NativeStreamingSpecialModelProfile.h"
#include "NativeVehicleComponentProfile.h"
#include <algorithm>
#include <new>
#include <string>
#include <unordered_set>

namespace mta::game_de::NativeVehicleComponents
{
    using Reader = NativeClassChainAccess::ReadMemory;
    struct Field
    {
        std::uintptr_t             address;
        std::vector<unsigned char> bytes;
    };
    // All copies and membership tokens live within a synchronous game-thread callback.
    // Check the exact spans again after the final owner and registration checks.
    struct Observation
    {
        std::uintptr_t                                         image;
        Reader                                                 read;
        std::vector<Field>                                     fields;
        std::vector<NativeUObjectMembershipAccess::Snapshot>   members;
        std::vector<std::pair<std::uintptr_t, std::uintptr_t>> casts;
        std::size_t                                            copied = 0;

        bool Bytes(std::uintptr_t base, std::size_t offset, void* out, std::size_t size)
        {
            constexpr std::size_t limit = 8 * 1024 * 1024;
            if (!base || offset > UINTPTR_MAX - base || !size || size > limit - copied || size > UINTPTR_MAX - (base + offset))
                return false;
            Field field{base + offset, std::vector<unsigned char>(size)};
            if (!read(field.address, field.bytes.data(), size))
                return false;
            std::memcpy(out, field.bytes.data(), size);
            copied += size;
            fields.push_back(std::move(field));
            return true;
        }
        template <class T>
        bool At(std::uintptr_t base, std::size_t offset, T& value)
        {
            return Bytes(base, offset, &value, sizeof(value));
        }
        bool Member(std::uintptr_t address)
        {
            NativeUObjectMembershipAccess::Snapshot member;
            if (!NativeUObjectMembershipAccess(image, read).Capture(address, member) || !NativeImageAccess::ImagePages(image, member.vtableAddress, 8))
                return false;
            members.push_back(member);
            return true;
        }
        bool Cast(std::uintptr_t cls, std::uintptr_t cache)
        {
            std::uintptr_t base = 0;
            bool           matches = false;
            if (!At(image, cache, base) || !Member(base) || !NativeClassChainAccess(read).IsChildOf(cls, base, matches) || !matches)
                return false;
            casts.emplace_back(cls, base);
            return true;
        }
        bool Stable() const
        {
            std::vector<unsigned char> current;
            for (const auto& field : fields)
            {
                current.resize(field.bytes.size());
                if (!read(field.address, current.data(), current.size()) || current != field.bytes)
                    return false;
            }
            for (const auto& member : members)
                if (!NativeUObjectMembershipAccess(image, read).Recheck(member))
                    return false;
            for (const auto& cast : casts)
            {
                bool matches = false;
                if (!NativeClassChainAccess(read).IsChildOf(cast.first, cast.second, matches) || !matches)
                    return false;
            }
            return true;
        }
    };

    struct ArrayHeader
    {
        std::uintptr_t data;
        std::int32_t   count, capacity;
    };
    struct Bone
    {
        std::uint32_t id, number;
        std::int32_t  parent;
    };
    static_assert(sizeof(ArrayHeader) == 16 && sizeof(Bone) == 12);

    struct NamePool
    {
        std::uint32_t block = 0, cursor = 0;
        bool          Open(Observation& observation)
        {
            namespace P = NativeStreamingSpecialModelProfile;
            std::uint8_t initialized = 0;
            return observation.At(observation.image, P::NameInitializedRva, initialized) && initialized == 1 &&
                   observation.At(observation.image, P::NamePoolRva + 8, block) && observation.At(observation.image, P::NamePoolRva + 12, cursor) &&
                   block < P::NameBlockCount && cursor <= P::NameBlockBytes && !(cursor % 2);
        }
        bool Append(Observation& observation, std::uint64_t key, std::vector<std::string>& names) const
        {
            namespace P = NativeStreamingSpecialModelProfile;
            // The classic upgrade names are unnumbered ASCII, up to 19 bytes.
            if (key >> 32)
                return true;
            const auto     page = static_cast<std::uint32_t>(key) >> 16;
            const auto     offset = (key & 0xffff) * 2;
            std::uintptr_t address = 0;
            std::uint16_t  header = 0;
            if (page > block || !observation.At(observation.image, P::NamePoolRva + 16 + page * sizeof(address), address) ||
                !observation.At(address, offset, header))
                return false;
            const auto count = static_cast<std::size_t>(header >> 6), width = std::size_t{1} + (header & 1);
            const auto end = offset + 2 + count * width;
            if (!count || end > P::NameBlockBytes || (page == block && end > cursor))
                return false;
            if (count > 32)
                return true;
            std::array<unsigned char, 64> buffer{};
            if (!observation.Bytes(address, offset + 2, buffer.data(), count * width))
                return false;
            std::string name;
            for (std::size_t i = 0; i < count; ++i)
            {
                if (!buffer[i * width] || buffer[i * width] >= 128 || (width == 2 && buffer[i * width + 1]))
                    return true;
                name.push_back(static_cast<char>(buffer[i * width]));
            }
            names.push_back(std::move(name));
            return true;
        }
    };

    // The lookup uses mesh+238 to resolve each full FName to an index. Verify
    // all occupied chains against the separate twelve-byte hierarchy rows.
    inline bool BoneMap(Observation& observation, std::uintptr_t mesh, const std::vector<Bone>& bones)
    {
        struct Row
        {
            std::uint32_t id, number;
            std::int32_t  index, next, bucket;
        };
        static_assert(sizeof(Row) == 20);
        if (mesh > UINTPTR_MAX - 0x238)
            return false;
        const auto     map = mesh + 0x238;
        ArrayHeader    head{};
        std::int32_t   free = 0, buckets = 0;
        std::uintptr_t hash = 0;
        if (!observation.At(map, 0, head) || !observation.At(map, 52, free) || !observation.At(map, 64, hash) || !observation.At(map, 72, buckets) ||
            head.count < 1 || head.count > 4096 || head.capacity < head.count || free < 0 || free > head.count ||
            static_cast<std::size_t>(head.count - free) != bones.size() || buckets < 1 || buckets > 4096 || (buckets & (buckets - 1)) ||
            (!hash && buckets != 1))
            return false;
        std::vector<Row>          rows(head.count);
        std::vector<std::int32_t> chains(buckets);
        if (!observation.Bytes(head.data, 0, rows.data(), rows.size() * sizeof(Row)) ||
            !observation.Bytes(hash ? hash : map, hash ? 0 : 56, chains.data(), chains.size() * sizeof(chains.front())))
            return false;
        std::vector<bool> seen(bones.size());
        for (int bucket = 0; bucket < buckets; ++bucket)
            for (auto i = chains[bucket]; i != -1; i = rows[i].next)
            {
                if (i < 0 || i >= head.count)
                    return false;
                const auto& row = rows[i];
                const auto  lo = row.id & 0xffff;
                const auto  hashKey = (lo >> 4) + (row.id >> 16) * 0x80001u + lo + (lo << 16) + row.number;
                if (row.bucket != bucket || (hashKey & (buckets - 1)) != static_cast<unsigned>(bucket) || row.index < 0 ||
                    static_cast<std::size_t>(row.index) >= bones.size() || seen[row.index] || bones[row.index].id != row.id ||
                    bones[row.index].number != row.number)
                    return false;
                seen[row.index] = true;
            }
        return std::all_of(seen.begin(), seen.end(), [](bool value) { return value; });
    }
    inline bool Descendants(Observation& observation, std::uintptr_t mesh, std::vector<std::string>& names)
    {
        ArrayHeader hierarchy{};
        NamePool    pool;
        if (!pool.Open(observation) || !observation.At(mesh, 0x1c8, hierarchy) || hierarchy.count < 1 || hierarchy.count > 2048 ||
            hierarchy.capacity < hierarchy.count)
            return false;
        std::vector<Bone> bones(hierarchy.count);
        if (!observation.Bytes(hierarchy.data, 0, bones.data(), bones.size() * sizeof(Bone)) || !BoneMap(observation, mesh, bones))
            return false;
        std::unordered_set<std::uint64_t> keys;
        for (std::size_t i = 0; i < bones.size(); ++i)
        {
            const auto& bone = bones[i];
            const auto  key = (std::uint64_t{bone.number} << 32) | bone.id;
            if (!keys.insert(key).second || (!i && bone.parent != -1) || (i && (bone.parent < 0 || static_cast<std::size_t>(bone.parent) >= i)) ||
                (i && !pool.Append(observation, key, names)))
                return false;
        }
        std::uintptr_t skeleton = 0;
        if (!observation.At(mesh, 0x80, skeleton) || !skeleton)
            return false;
        ArrayHeader sockets{};
        if (!observation.Member(skeleton) || !observation.At(skeleton, 0x190, sockets) || sockets.count < 0 || sockets.count > 512 ||
            sockets.capacity < sockets.count)
            return false;
        for (int i = 0; i < sockets.count; ++i)
        {
            std::uintptr_t socket = 0;
            if (!observation.At(sockets.data, i * sizeof(socket), socket))
                return false;
            if (!socket)
                continue;
            std::uint64_t key = 0, parent = 0;
            if (!observation.Member(socket) || !observation.At(socket, 0x28, key) || !observation.At(socket, 0x30, parent) || !keys.count(parent))
                return false;
            // Native resolves a bone first. A same-name socket cannot turn the root into a descendant.
            if (!keys.count(key) && !pool.Append(observation, key, names))
                return false;
        }
        return true;
    }

    inline bool Mesh(Observation& observation, const NativeModelBankReader::Value& model, const NativeStreamingModelContext::Value& context,
                     std::uintptr_t render, std::uintptr_t& mesh)
    {
        namespace W = NativeModelWheelProfile;
        namespace R = NativeStreamingRequestProfile;
        namespace U = NativeStartupProvidersProfile;
        const auto   image = observation.image, metadata = model.nativeModelInfo, pool = image + W::PoolRva;
        std::int32_t poolCount = 0;
        if (metadata < pool || context.modelVtable != image + W::VtableRva || context.typeMethod != image + W::TypeGetterRva ||
            !observation.At(image, W::PoolCountRva, poolCount) || poolCount <= 0 || static_cast<unsigned>(poolCount) > W::PoolCapacity ||
            (metadata - pool) % W::PoolStride || (metadata - pool) / W::PoolStride >= static_cast<unsigned>(poolCount))
            return false;
        std::uintptr_t currentRender = 0, backlink = 0, bundle = 0;
        if (!observation.At(metadata, 0x40, currentRender) || currentRender != render || !observation.At(render, 0x18, backlink) || backlink != metadata ||
            !observation.At(metadata, 0x58, bundle) || !context.rows || context.count < 1 || bundle < context.rows ||
            (bundle - context.rows) % R::ProviderRowStride || (bundle - context.rows) / R::ProviderRowStride >= static_cast<unsigned>(context.count))
            return false;
        std::int32_t   index = -1, objects = 0;
        std::uint32_t  serial = 0;
        std::uintptr_t chunks = 0, chunk = 0;
        if (!observation.At(bundle, 0x18, index) || !observation.At(bundle, 0x1c, serial) || index < 0 || !serial ||
            !observation.At(image, U::objectCountRva, objects) || index >= objects || !observation.At(image, U::objectChunksRva, chunks) ||
            !observation.At(chunks, (static_cast<unsigned>(index) >> U::chunkIndexShift) * sizeof(chunk), chunk) ||
            !observation.At(chunk, (static_cast<unsigned>(index) & 0xffff) * U::objectItemBytes, mesh) || !observation.Member(mesh))
            return false;
        const auto member = observation.members.back();
        return NativeUObjectMembershipAccess::MatchesWeak(member, index, serial) && observation.Member(member.classAddress) &&
               observation.Cast(member.classAddress, 0x576e9d8) && observation.Cast(member.classAddress, 0x57e6330);
    }

    inline bool Profile(std::uintptr_t image) noexcept
    {
        std::array<unsigned char, 192> actual{};
        for (const auto& pin : NativeVehicleComponentProfile::Code)
            if (pin.size > actual.size() || !NativeImageAccess::ReadImage(image, image + pin.rva, actual.data(), pin.size, false, true) ||
                std::memcmp(actual.data(), pin.bytes, pin.size))
                return false;
        const auto& constructor = NativeModelWheelProfile::PoolConstructor;
        return NativeImageAccess::ReadImage(image, image + NativeModelWheelProfile::PoolConstructorRva, actual.data(), constructor.size(), false, true) &&
               !std::memcmp(actual.data(), constructor.data(), constructor.size());
    }

    inline bool Registered(Observation& observation, const NativeModelBankReader::Value& slot, const NativeStreamingModelContext::Value& context)
    {
        namespace R = NativeStreamingRequestProfile;
        std::uintptr_t metadata = 0, root = 0, provider = 0, rows = 0;
        std::int32_t   count = 0;
        return observation.At(slot.slotAddress, 0, metadata) && metadata == slot.nativeModelInfo && observation.At(observation.image, R::RootRva, root) &&
               root == context.root && observation.At(root, R::ProviderOffset, provider) && provider == context.provider &&
               observation.At(provider, R::ProviderRowsOffset, rows) && rows == context.rows && observation.At(provider, R::ProviderCountOffset, count) &&
               count == context.count;
    }

    inline ClockResult Read(std::uintptr_t image, std::uint32_t model, std::uintptr_t render, NativeModelBankReader::CheckOwner owner,
                            NativeModelBankReader::CheckOwner application, std::vector<std::string>& output) noexcept
    {
        if (!image || !render || render % alignof(std::uintptr_t) || !owner || !application)
            return ClockResult::InvalidArgument;
        auto result = owner();
        if (result != ClockResult::Ok || (result = application()) != ClockResult::Ok)
            return result;
        NativeModelBankReader              bank({image}, owner);
        NativeModelBankReader::Value       slot;
        NativeStreamingModelContext::Value context;
        if ((result = bank.Read(model, slot)) != ClockResult::Ok)
            return result;
        if (!slot.nativeModelInfo)
            return ClockResult::NotInitialized;
        if ((result = NativeStreamingModelContext::Capture(image, slot, context)) != ClockResult::Ok)
            return result;
        if (!Profile(image))
            return ClockResult::UnsupportedBuild;
        try
        {
            Observation              observation{image, &NativeStreamingModelContext::Copy};
            std::uintptr_t           mesh = 0;
            std::vector<std::string> names;
            if (!Registered(observation, slot, context) || !Mesh(observation, slot, context, render, mesh) || !Descendants(observation, mesh, names))
                return ClockResult::BindingChanged;
            if ((result = bank.Recheck(slot)) != ClockResult::Ok || (result = NativeStreamingModelContext::Recheck(image, slot, context)) != ClockResult::Ok ||
                (result = owner()) != ClockResult::Ok || (result = application()) != ClockResult::Ok)
                return result;
            if (!Profile(image) || !observation.Stable())
                return ClockResult::BindingChanged;
            output.swap(names);
            return ClockResult::Ok;
        }
        catch (const std::bad_alloc&)
        {
            return ClockResult::ReadFailure;
        }
    }
}
