/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Enumerate only current typed resolved LOD references under one owner
 ****************************************************************************/
#include "NativeWorldLodMemory.h"
#include "NativeWorldLodProfile.h"
#include "NativeBuildingPoolDefaultProfile.h"
#include "NativeImageAccess.h"
#include <algorithm>
#include <cstring>
#include <unordered_map>
#include <utility>
namespace mta::game_de::NativeWorldLodMemory
{
    namespace LodD = NativeStreamingCompletionContext;
    namespace LodE = NativeWorldIplLoadedProfile;
    namespace LodP = NativeWorldLodProfile;
    namespace LodI = NativeImageAccess;
    namespace
    {
        template <class T, class Bytes>
        T LodField(const Bytes& bytes, std::size_t offset) noexcept
        {
            T result{};
            std::memcpy(&result, bytes.data() + offset, sizeof(result));
            return result;
        }
        template <class T>
        bool LodRead(std::uintptr_t address, T& value) noexcept
        {
            return LodD::Copy(address, &value, sizeof(value));
        }
        template <class Pins>
        bool LodMatch(std::uintptr_t image, const Pins& pins, bool typed)
        {
            for (const auto& pin : pins)
            {
                std::vector<unsigned char> bytes(pin.size);
                if (!pin.size || !LodI::ReadImage(image, image + pin.rva, bytes.data(), bytes.size(), false, typed ? pin.code : true) ||
                    std::memcmp(bytes.data(), pin.bytes, pin.size))
                    return false;
            }
            return true;
        }
        bool LodProfile(std::uintptr_t image)
        {
            if (!LodD::Profile(image) || !NativeModelReferenceHeap::QualifyProfile(image) || !LodMatch(image, LodP::Pins, true))
                return false;
            for (const auto& pin : NativeBuildingPoolDefaultProfile::Pins)
            {
                std::vector<unsigned char> bytes(pin.size);
                if (!LodI::ReadImage(image, image + pin.rva, bytes.data(), bytes.size(), false, true) || std::memcmp(bytes.data(), pin.bytes, pin.size))
                    return false;
            }
            // Exact default allocations plus the common constructor and
            // Object/Dummy producers. Optional actor/matrix/physics fields are
            // not borrowed or invoked by this read-only descriptor operation.
            const LodP::Pin pins[] = {{0x117161f, LodE::ObjectPoolBytes.data(), LodE::ObjectPoolBytes.size(), true},
                                      {0x11716ef, LodE::DummyPoolBytes.data(), LodE::DummyPoolBytes.size(), true},
                                      {0x112e8a0, LodE::BaseConstructBytes.data(), LodE::BaseConstructBytes.size(), true},
                                      {0x1141700, LodE::IplConstructBytes.data(), LodE::IplConstructBytes.size(), true},
                                      {0x112e820, LodE::DummyAllocateBytes.data(), LodE::DummyAllocateBytes.size(), true},
                                      {0x12122c0, LodE::ObjectConstructBytes.data(), LodE::ObjectConstructBytes.size(), true},
                                      {0x12128a0, LodE::ObjectAllocateBytes.data(), LodE::ObjectAllocateBytes.size(), true},
                                      {0x1212340, LodE::RelatedConstructBytes.data(), LodE::RelatedConstructBytes.size(), true},
                                      {0x1212a60, LodE::RelatedAllocateBytes.data(), LodE::RelatedAllocateBytes.size(), true}};
            return LodMatch(image, pins, true);
        }
        ClockResult LodOwner(std::uintptr_t image, CheckOwner owner, CheckOwner after) noexcept
        {
            if (!image || !owner || !after)
                return ClockResult::InvalidArgument;
            const auto result = after();
            return result == ClockResult::Ok ? LodD::Owner(image, owner) : result;
        }
        void LodIdentity(const LodD::Value& world, Snapshot& value)
        {
            value.identity = {world.root, world.gameMode, world.level, world.world, world.gameClass, world.levelClass, world.mode, world.count};
            for (std::size_t at = 0; at < world.count; ++at)
            {
                const auto& m = world.members[at];
                value.identity.insert(value.identity.end(), {m.objectAddress, m.vtableAddress, m.classAddress, m.outerAddress, m.itemAddress,
                                                             static_cast<std::uintptr_t>(m.index), m.serial});
            }
        }
        bool LodSeal(std::uintptr_t image, const Snapshot& value, const LodD::Value& world)
        {
            if (!LodProfile(image) || NativeModelReferenceHeap::Recheck(image, value.heap) != ClockResult::Ok || !LodD::Fence(image, world))
                return false;
            for (const auto& pool : value.pools)
            {
                std::uintptr_t                address = 0;
                std::array<unsigned char, 32> header{};
                std::vector<unsigned char>    flags(pool.generations.size());
                if (!LodI::ReadImage(image, image + pool.root, &address, sizeof(address), true) || address != pool.address || !LodRead(address, header) ||
                    header != pool.header || !LodD::Copy(pool.flags, flags.data(), flags.size()) || flags != pool.generations)
                    return false;
            }
            for (const auto& row : value.rows)
            {
                std::vector<unsigned char> bytes(row.bytes.size());
                if (!LodD::Copy(row.address, bytes.data(), bytes.size()) || bytes != row.bytes)
                    return false;
            }
            LodD::Value current;
            return LodD::Capture(image, current) == ClockResult::Ok && LodD::Same(world, current, true) && LodD::Fence(image, current) &&
                   NativeModelReferenceHeap::Recheck(image, value.heap) == ClockResult::Ok;
        }
    }
    ClockResult Capture(std::uintptr_t image, CheckOwner owner, CheckOwner after, Snapshot& output) noexcept
    {
        if (!LodD::TryEnter())
            return ClockResult::Busy;
        LodD::Scope scope;
        try
        {
            auto result = LodOwner(image, owner, after);
            if (result != ClockResult::Ok)
                return result;
            if (!LodProfile(image))
                return ClockResult::UnsupportedBuild;
            Snapshot    next;
            LodD::Value world;
            if ((result = LodD::Capture(image, world)) != ClockResult::Ok || (result = NativeModelReferenceHeap::Capture(image, next.heap)) != ClockResult::Ok)
                return result;
            LodIdentity(world, next);
            const std::uintptr_t roots[] = {LodE::BuildingRoot, LodE::DummyRoot, LodE::ObjectRoot};
            const std::int32_t   capacities[] = {static_cast<std::int32_t>(LodE::BuildingCapacity), static_cast<std::int32_t>(LodE::DummyCapacity),
                                                 static_cast<std::int32_t>(LodE::ObjectCapacity)};
            const std::size_t    strides[] = {LodE::BuildingBytes, LodE::DummyBytes, LodE::ObjectBytes};
            const unsigned char  types[] = {1, 5, 4};
            std::unordered_map<std::uintptr_t, std::size_t> rows;
            for (unsigned p = 0; p < next.pools.size(); ++p)
            {
                auto& pool = next.pools[p];
                pool.root = roots[p];
                pool.capacity = capacities[p];
                pool.stride = strides[p];
                if (!LodI::ReadImage(image, image + pool.root, &pool.address, sizeof(pool.address), true) ||
                    !LodD::Writable(pool.address, pool.header.size()) || !LodRead(pool.address, pool.header))
                    return ClockResult::ReadFailure;
                pool.objects = LodField<std::uintptr_t>(pool.header, 0);
                pool.flags = LodField<std::uintptr_t>(pool.header, 8);
                const auto cursor = LodField<std::int32_t>(pool.header, 0x14);
                const auto bytes = static_cast<std::size_t>(pool.capacity) * pool.stride;
                if (LodField<std::int32_t>(pool.header, 0x10) != pool.capacity || cursor < -1 || cursor >= pool.capacity || pool.header[0x18] != 1 ||
                    pool.header[0x19] || !LodD::Writable(pool.objects, bytes) || !LodD::Writable(pool.flags, static_cast<std::size_t>(pool.capacity)) ||
                    !NativeModelReferenceMap::Disjoint(pool.address, pool.header.size(), pool.objects, bytes) ||
                    !NativeModelReferenceMap::Disjoint(pool.address, pool.header.size(), pool.flags, static_cast<std::size_t>(pool.capacity)) ||
                    !NativeModelReferenceMap::Disjoint(pool.objects, bytes, pool.flags, static_cast<std::size_t>(pool.capacity)))
                    return ClockResult::BindingChanged;
                for (unsigned old = 0; old < p; ++old)
                {
                    const auto&                                  before = next.pools[old];
                    const std::pair<std::uintptr_t, std::size_t> currentRanges[] = {
                        {pool.address, pool.header.size()}, {pool.objects, bytes}, {pool.flags, static_cast<std::size_t>(pool.capacity)}};
                    const std::pair<std::uintptr_t, std::size_t> oldRanges[] = {{before.address, before.header.size()},
                                                                                {before.objects, static_cast<std::size_t>(before.capacity) * before.stride},
                                                                                {before.flags, static_cast<std::size_t>(before.capacity)}};
                    for (const auto& a : currentRanges)
                        for (const auto& b : oldRanges)
                            if (!NativeModelReferenceMap::Disjoint(a.first, a.second, b.first, b.second))
                                return ClockResult::BindingChanged;
                }
                pool.generations.resize(static_cast<std::size_t>(pool.capacity));
                if (!LodD::Copy(pool.flags, pool.generations.data(), pool.generations.size()))
                    return ClockResult::ReadFailure;
                for (std::uint32_t index = 0; index < static_cast<std::uint32_t>(pool.capacity); ++index)
                {
                    if (pool.generations[index] & 0x80)
                        continue;
                    Row row;
                    row.pool = p;
                    row.index = index;
                    row.generation = pool.generations[index];
                    row.address = pool.objects + index * pool.stride;
                    row.bytes.resize(pool.stride);
                    if (!LodD::Copy(row.address, row.bytes.data(), row.bytes.size()))
                        return ClockResult::ReadFailure;
                    const auto     table = LodField<std::uintptr_t>(row.bytes, 0);
                    const bool     tableMatches = p == 0   ? (table == image + LodE::BuildingVtable || table == image + LodE::SpecialBuildingVtable)
                                                  : p == 1 ? table == image + LodE::DummyVtable
                                                           : table == image + LodE::ObjectVtable;
                    std::uintptr_t destructor = 0;
                    const auto     destructorRva = p == 0 ? 0x10367b0u : p == 1 ? 0x1212120u : 0x1212230u;
                    if (!tableMatches || (row.bytes[0x6a] & 7) != types[p] || !NativeModelReferenceHeap::ReadOnlyImage(image, table, sizeof(destructor)) ||
                        !LodI::ReadImage(image, table, &destructor, sizeof(destructor)) || destructor != image + destructorRva)
                        return ClockResult::UnsupportedBuild;
                    row.lod = LodField<std::uintptr_t>(row.bytes, LodP::LodOffset);
                    row.lodChildren = row.bytes[LodP::ChildrenOffset];
                    if (!rows.emplace(row.address, next.rows.size()).second)
                        return ClockResult::BindingChanged;
                    next.rows.push_back(std::move(row));
                }
            }
            // A QWORD value is not yet proof of a resolved LOD. Every nonnull
            // target must be the exact start of another current occupied typed
            // row. Raw -1 and small sign-extended placement indices refuse.
            for (const auto& row : next.rows)
                if (row.lod && !rows.count(row.lod))
                    return ClockResult::BindingChanged;
            if ((result = LodOwner(image, owner, after)) != ClockResult::Ok)
                return result;
            if (!LodSeal(image, next, world))
                return ClockResult::BindingChanged;
            output = std::move(next);
            return ClockResult::Ok;
        }
        catch (...)
        {
            return ClockResult::ReadFailure;
        }
    }
    bool Same(const Snapshot& a, const Snapshot& b) noexcept
    {
        if (a.representation != b.representation || a.identity != b.identity || !NativeModelReferenceHeap::Same(a.heap, b.heap) ||
            a.rows.size() != b.rows.size())
            return false;
        for (std::size_t at = 0; at < a.pools.size(); ++at)
        {
            const auto& x = a.pools[at];
            const auto& y = b.pools[at];
            if (x.root != y.root || x.address != y.address || x.objects != y.objects || x.flags != y.flags || x.capacity != y.capacity ||
                x.stride != y.stride || x.header != y.header || x.generations != y.generations)
                return false;
        }
        for (std::size_t at = 0; at < a.rows.size(); ++at)
        {
            const auto& x = a.rows[at];
            const auto& y = b.rows[at];
            if (x.pool != y.pool || x.index != y.index || x.address != y.address || x.lod != y.lod || x.generation != y.generation ||
                x.lodChildren != y.lodChildren || x.bytes != y.bytes)
                return false;
        }
        return true;
    }
    ClockResult Recheck(std::uintptr_t image, CheckOwner owner, CheckOwner after, const Snapshot& before) noexcept
    {
        Snapshot   current;
        const auto result = Capture(image, owner, after, current);
        return result != ClockResult::Ok ? result : Same(before, current) ? ClockResult::Ok : ClockResult::BindingChanged;
    }
}
