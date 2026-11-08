/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Qualify the real IPL bank and region tree without loaded removal
 ****************************************************************************/
#include "NativeWorldIplMemory.h"
#include "NativeWorldIplProfile.h"
#include "NativeWorldIplPoolProfile.h"
#include "NativeModelLoadedReader.h"
#include "NativeModelLoadedProfile.h"
#include "NativeStreamingRemoveProfile.h"
#include "NativeImageAccess.h"
#include <windows.h>
#include <intrin.h>
#include <algorithm>
#include <cstring>
#include <limits>
#include <unordered_set>
#include <utility>

namespace mta::game_de::NativeWorldIplMemory
{
    namespace P = NativeWorldIplProfile;
    namespace Q = NativeWorldIplPoolProfile;
    namespace C = NativeStreamingCompletionContext;
    namespace H = NativeModelReferenceHeap;
    namespace Image = NativeImageAccess;
    namespace
    {
        template <class Pins>
        bool PinsMatch(std::uintptr_t image, const Pins& pins)
        {
            for (const auto& pin : pins)
            {
                std::vector<unsigned char> bytes(pin.size);
                if (!pin.size || !Image::ReadImage(image, image + pin.rva, bytes.data(), bytes.size(), false, true) ||
                    std::memcmp(bytes.data(), pin.bytes, bytes.size()))
                    return false;
            }
            return true;
        }
        bool Writable(std::uintptr_t address, std::size_t size, std::size_t alignment = alignof(std::uintptr_t)) noexcept
        {
            if (!address || address % alignment || !size || size > (std::numeric_limits<std::uintptr_t>::max)() - address)
                return false;
            const auto end = address + size;
            while (address < end)
            {
                MEMORY_BASIC_INFORMATION page{};
                if (VirtualQuery(reinterpret_cast<void*>(address), &page, sizeof(page)) != sizeof(page) || page.State != MEM_COMMIT ||
                    (page.Type != MEM_PRIVATE && page.Type != MEM_MAPPED) || (page.Protect != PAGE_READWRITE && page.Protect != PAGE_WRITECOPY))
                    return false;
                const auto begin = reinterpret_cast<std::uintptr_t>(page.BaseAddress);
                if (begin > address || page.RegionSize > (std::numeric_limits<std::uintptr_t>::max)() - begin || begin + page.RegionSize <= address)
                    return false;
                address = (std::min)(end, begin + page.RegionSize);
            }
            return true;
        }
        template <class T>
        bool Read(std::uintptr_t address, T& value) noexcept
        {
            return C::Copy(address, &value, sizeof(value));
        }
        template <class T, std::size_t N>
        T Field(const std::array<unsigned char, N>& bytes, std::size_t offset) noexcept
        {
            T value{};
            if (offset <= N && sizeof(value) <= N - offset)
                std::memcpy(&value, bytes.data() + offset, sizeof(value));
            return value;
        }
        bool PoolRead(std::uintptr_t image, std::uintptr_t rva, std::size_t stride, std::int32_t maximum, std::int32_t exact, Pool& output)
        {
            Pool pool;
            if (!Image::ReadImage(image, image + rva, &pool.address, sizeof(pool.address), true) || !pool.address ||
                !Writable(pool.address, pool.header.size()) || !Read(pool.address, pool.header))
                return false;
            pool.objects = Field<std::uintptr_t>(pool.header, 0);
            pool.flags = Field<std::uintptr_t>(pool.header, 8);
            pool.capacity = Field<std::int32_t>(pool.header, 0x10);
            pool.cursor = Field<std::int32_t>(pool.header, 0x14);
            if (pool.capacity <= 0 || pool.capacity > maximum || (exact && pool.capacity != exact) || pool.cursor < -1 || pool.cursor >= pool.capacity ||
                pool.header[0x18] != 1 || !Writable(pool.objects, static_cast<std::size_t>(pool.capacity) * stride, stride == P::IplBytes ? 4 : 8) ||
                !Writable(pool.flags, static_cast<std::size_t>(pool.capacity), 1))
                return false;
            const auto overlaps = [](std::uintptr_t a, std::size_t aSize, std::uintptr_t b, std::size_t bSize) { return a < b + bSize && b < a + aSize; };
            const auto objectBytes = static_cast<std::size_t>(pool.capacity) * stride;
            if (overlaps(pool.address, pool.header.size(), pool.objects, objectBytes) ||
                overlaps(pool.address, pool.header.size(), pool.flags, static_cast<std::size_t>(pool.capacity)) ||
                overlaps(pool.objects, objectBytes, pool.flags, static_cast<std::size_t>(pool.capacity)))
                return false;
            pool.states.resize(static_cast<std::size_t>(pool.capacity));
            if (!C::Copy(pool.flags, pool.states.data(), pool.states.size()))
                return false;
            for (auto value : pool.states)
                if (value & 0x80)
                    ++pool.free;
            output = std::move(pool);
            return true;
        }
        bool Member(const Pool& pool, std::size_t stride, std::uintptr_t address, std::uint32_t& index) noexcept
        {
            if (!address || address < pool.objects || (address - pool.objects) % stride)
                return false;
            const auto slot = (address - pool.objects) / stride;
            if (slot >= pool.states.size() || (pool.states[slot] & 0x80))
                return false;
            index = static_cast<std::uint32_t>(slot);
            return true;
        }
        bool SamePool(const Pool& a, const Pool& b) noexcept
        {
            return a.address == b.address && a.objects == b.objects && a.flags == b.flags && a.capacity == b.capacity;
        }
        bool DisjointPools(const Snapshot& snapshot) noexcept
        {
            struct Span
            {
                std::uintptr_t address;
                std::size_t    bytes;
            };
            std::array<Span, 9> spans{};
            std::size_t         count = 0;
            const auto          append = [&](const Pool& pool, std::size_t stride)
            {
                if (!pool.address)
                    return;
                spans[count++] = {pool.address, pool.header.size()};
                spans[count++] = {pool.objects, static_cast<std::size_t>(pool.capacity) * stride};
                spans[count++] = {pool.flags, static_cast<std::size_t>(pool.capacity)};
            };
            append(snapshot.ipl, P::IplBytes);
            append(snapshot.quad, P::QuadBytes);
            append(snapshot.links, P::LinkBytes);
            for (std::size_t a = 0; a != count; ++a)
                for (std::size_t b = a + 1; b != count; ++b)
                    if (spans[a].address < spans[b].address + spans[b].bytes && spans[b].address < spans[a].address + spans[a].bytes)
                        return false;
            return true;
        }
        bool SealPool(std::uintptr_t image, std::uintptr_t root, const Pool& pool) noexcept
        {
            if (!pool.address)
                return true;
            std::uintptr_t                current = 0;
            std::array<unsigned char, 32> header{};
            if (!Image::ReadImage(image, image + root, &current, sizeof(current), true) || current != pool.address || !Read(current, header) ||
                header != pool.header)
                return false;
            std::array<unsigned char, 256> chunk{};
            for (std::size_t at = 0; at < pool.states.size(); at += chunk.size())
            {
                const auto size = (std::min)(chunk.size(), pool.states.size() - at);
                if (!C::Copy(pool.flags + at, chunk.data(), size) || std::memcmp(chunk.data(), pool.states.data() + at, size))
                    return false;
            }
            return true;
        }
        bool Tree(std::uintptr_t image, Snapshot& snapshot)
        {
            if (!Image::ReadImage(image, image + P::TreeRva, &snapshot.tree, sizeof(snapshot.tree), true))
                return false;
            if (!snapshot.tree)
                return true;
            if (!PoolRead(image, P::QuadPoolRva, P::QuadBytes, P::MaximumNodes, 400, snapshot.quad) ||
                !PoolRead(image, P::LinkPoolRva, P::LinkBytes, P::MaximumLinks, 75000, snapshot.links))
                return false;
            std::unordered_set<std::uintptr_t> seenNodes, seenLinks;
            struct Work
            {
                std::uintptr_t address;
                std::int32_t   parent;
                std::uint32_t  quadrant;
            };
            std::vector<Work> pending{{snapshot.tree, -1, 0}};
            while (!pending.empty())
            {
                const auto work = pending.back();
                pending.pop_back();
                std::uint32_t slot = 0;
                if (!Member(snapshot.quad, P::QuadBytes, work.address, slot) || !seenNodes.insert(work.address).second)
                    return false;
                Node node;
                node.address = work.address;
                if (!Read(node.address, node.bytes))
                    return false;
                std::memcpy(node.plan.bounds.data(), node.bytes.data(), sizeof(node.plan.bounds));
                node.plan.level = Field<std::uint32_t>(node.bytes, 0x38);
                node.head = Field<std::uintptr_t>(node.bytes, 0x10);
                if (!NativeWorldIplPlan::Valid(node.plan.bounds) || node.plan.level > P::MaximumLevel || (node.plan.level && node.head))
                    return false;
                const auto index = static_cast<std::int32_t>(snapshot.nodes.size());
                if (work.parent >= 0)
                {
                    auto& parent = snapshot.nodes[static_cast<std::size_t>(work.parent)];
                    if (!parent.plan.level || parent.plan.level - 1 != node.plan.level ||
                        NativeWorldIplPlan::Child(parent.plan.bounds, work.quadrant) != node.plan.bounds)
                        return false;
                    parent.plan.child[work.quadrant] = index;
                }
                snapshot.nodes.push_back(node);
                for (std::uint32_t remaining = 4; remaining; --remaining)
                {
                    const auto quadrant = remaining - 1;
                    const auto child = Field<std::uintptr_t>(node.bytes, 0x18 + quadrant * 8);
                    if (child)
                    {
                        if (!node.plan.level)
                            return false;
                        pending.push_back(Work{child, index, quadrant});
                    }
                }
                auto link = node.head;
                while (link)
                {
                    if (!Member(snapshot.links, P::LinkBytes, link, slot) || !seenLinks.insert(link).second)
                        return false;
                    Link value;
                    value.address = link;
                    if (!Read(link, value.bytes))
                        return false;
                    value.item = Field<std::uintptr_t>(value.bytes, 0);
                    value.next = Field<std::uintptr_t>(value.bytes, 8);
                    if (!Member(snapshot.ipl, P::IplBytes, value.item, slot))
                        return false;
                    snapshot.list.push_back(value);
                    link = value.next;
                }
            }
            return true;
        }
        ClockResult Sample(std::uintptr_t image, Snapshot& snapshot, bool forRemoval)
        {
            if (!PoolRead(image, P::IplPoolRva, P::IplBytes, P::IplCount, P::IplCount, snapshot.ipl))
                return ClockResult::ReadFailure;
            if (!Image::ReadImage(image, image + NativeModelLoadedProfile::ArrayRva + P::IplFileBase * 16, snapshot.stream.data(), sizeof(snapshot.stream),
                                  true))
                return ClockResult::ReadFailure;
            for (std::uint32_t index = 0; index != P::IplCount; ++index)
            {
                if (snapshot.ipl.states[index] & 0x80)
                    continue;
                Row row;
                row.index = index;
                row.flag = snapshot.ipl.states[index];
                row.address = snapshot.ipl.objects + static_cast<std::size_t>(index) * P::IplBytes;
                if (!Read(row.address, row.bytes))
                    return ClockResult::ReadFailure;
                std::memcpy(row.bounds.data(), row.bytes.data(), sizeof(row.bounds));
                std::memcpy(row.name.data(), row.bytes.data() + P::IplNameOffset, row.name.size());
                row.streamState = snapshot.stream[index][13];
                if (!NativeWorldIplPlan::Valid(row.bounds) || row.bytes[P::DisabledOffset] > 1)
                    return ClockResult::BindingChanged;
                // The loaded IPL branch enters native entity destructors and
                // UE actor/component cleanup not yet qualified by this service.
                // Refuse the whole bank before its first effect; a typed pool
                // header or a readable entity vtable cannot substitute for it.
                if (forRemoval && index && row.streamState != 0)
                    return ClockResult::UnsupportedBuild;
                snapshot.rows.push_back(row);
            }
            // These are separate allocations in the pinned initializer. A
            // forged alias must not turn an IPL flag store into tree metadata.
            if (!Tree(image, snapshot) || !DisjointPools(snapshot))
                return ClockResult::BindingChanged;
            return ClockResult::Ok;
        }
    }
    bool Profile(std::uintptr_t image) noexcept
    {
        try
        {
            if (!image || !C::Profile(image) || !H::QualifyProfile(image) || !PinsMatch(image, P::Code) || !PinsMatch(image, Q::Code))
                return false;
            std::array<unsigned char, NativeStreamingRemoveProfile::Remove.size()> remove{};
            std::uint32_t                                                          half = 0;
            NativeModelLoadedReader::Binding                                       loaded{image};
            return Image::ReadImage(image, image + NativeStreamingRemoveProfile::RemoveRva, remove.data(), remove.size(), false, true) &&
                   !std::memcmp(remove.data(), NativeStreamingRemoveProfile::Remove.data(), remove.size()) && NativeModelLoadedReader::Revalidate(loaded) &&
                   H::ReadOnlyImage(image, image + Q::HalfRva, sizeof(half)) && Image::ReadImage(image, image + Q::HalfRva, &half, sizeof(half)) &&
                   half == Q::HalfBits;
        }
        catch (...)
        {
            return false;
        }
    }
    bool ReadRow(const Snapshot& snapshot, std::uint32_t index, const Row*& output) noexcept
    {
        for (const auto& row : snapshot.rows)
            if (row.index == index)
            {
                output = &row;
                return true;
            }
        return false;
    }
    bool SameRows(const Snapshot& a, const Snapshot& b) noexcept
    {
        if (a.rows.size() != b.rows.size())
            return false;
        for (std::size_t index = 0; index != a.rows.size(); ++index)
        {
            const auto& x = a.rows[index];
            const auto& y = b.rows[index];
            if (x.index != y.index || x.address != y.address || x.flag != y.flag || x.name != y.name || x.bounds != y.bounds)
                return false;
        }
        return true;
    }
    bool SameIdentity(const Snapshot& a, const Snapshot& b) noexcept
    {
        return C::Same(a.world, b.world, true) && H::Same(a.heap, b.heap) && a.tree == b.tree && SamePool(a.ipl, b.ipl) && SamePool(a.quad, b.quad) &&
               SamePool(a.links, b.links) && SameRows(a, b);
    }
    bool Seal(std::uintptr_t image, const Snapshot& before) noexcept
    {
        try
        {
            C::Value world;
            // Membership alone cannot fence a last-owner edit to root->game,
            // game->level or level->world. Recapture the whole callback-free
            // graph before accepting this exact copied bank/tree observation.
            if (!Profile(image) || C::Capture(image, world) != ClockResult::Ok || !C::Same(before.world, world, true) ||
                H::Recheck(image, before.heap) != ClockResult::Ok || !SealPool(image, P::IplPoolRva, before.ipl) ||
                !SealPool(image, P::QuadPoolRva, before.quad) || !SealPool(image, P::LinkPoolRva, before.links))
                return false;
            std::uintptr_t                                 tree = 0;
            std::array<std::array<unsigned char, 16>, 256> stream{};
            if (!Image::ReadImage(image, image + P::TreeRva, &tree, sizeof(tree), true) || tree != before.tree ||
                !Image::ReadImage(image, image + NativeModelLoadedProfile::ArrayRva + P::IplFileBase * 16, stream.data(), sizeof(stream), true) ||
                stream != before.stream)
                return false;
            for (const auto& row : before.rows)
            {
                std::array<unsigned char, 0x34> bytes{};
                if (!Read(row.address, bytes) || bytes != row.bytes)
                    return false;
            }
            for (const auto& node : before.nodes)
            {
                std::array<unsigned char, 64> bytes{};
                if (!Read(node.address, bytes) || bytes != node.bytes)
                    return false;
            }
            for (const auto& link : before.list)
            {
                std::array<unsigned char, 16> bytes{};
                if (!Read(link.address, bytes) || bytes != link.bytes)
                    return false;
            }
            return true;
        }
        catch (...)
        {
            return false;
        }
    }
    ClockResult Capture(std::uintptr_t image, NativeWorldIplCalls::CheckOwner owner, NativeWorldIplCalls::CheckOwner afterApplicationOwner, Snapshot& output,
                        bool forRemoval)
    {
        if (!owner || !afterApplicationOwner)
            return ClockResult::NotInitialized;
        auto result = afterApplicationOwner();
        if (result != ClockResult::Ok || (result = C::Owner(image, owner)) != ClockResult::Ok)
            return result;
        if (!Profile(image))
            return ClockResult::UnsupportedBuild;
        Snapshot snapshot;
        if ((result = C::Capture(image, snapshot.world)) != ClockResult::Ok || (result = H::Capture(image, snapshot.heap)) != ClockResult::Ok ||
            (result = Sample(image, snapshot, forRemoval)) != ClockResult::Ok)
            return result;
        if ((result = afterApplicationOwner()) != ClockResult::Ok || (result = C::Owner(image, owner)) != ClockResult::Ok)
            return result;
        if (!Seal(image, snapshot))
            return ClockResult::BindingChanged;
        output = std::move(snapshot);
        return ClockResult::Ok;
    }
    bool StoreEnabled(const Row& row) noexcept
    {
        // Preserve the actual SetDynamicIplStreamingEnabled ordering: accept
        // this exact byte before the genuine region-tree Add. A later failure
        // retains that accepted prefix; it is never hidden by a scalar rollback.
        __try
        {
            return static_cast<unsigned char>(_InterlockedCompareExchange8(reinterpret_cast<volatile char*>(row.address + P::DisabledOffset), 0,
                                                                           static_cast<char>(row.bytes[P::DisabledOffset]))) == row.bytes[P::DisabledOffset];
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return false;
        }
    }
}
