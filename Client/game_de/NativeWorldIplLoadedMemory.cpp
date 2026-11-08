/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Close loaded IPL pool/entity and ordinary UE API preconditions
 ****************************************************************************/
#include "NativeWorldIplLoadedMemory.h"
#include "NativeWorldIplLoadedProfile.h"
#include "NativeBuildingPoolDefaultProfile.h"
#include "NativeModelObjectGroupProfile.h"
#include "NativeModelTimeProfile.h"
#include "NativeModelReferenceMap.h"
#include "NativeModelReferenceHeap.h"
#include "NativeModelBankProfile.h"
#include "NativeModelLoadedProfile.h"
#include "NativeStartupWorldAccess.h"
#include "NativeClassChainAccess.h"
#include "NativeImageAccess.h"
#include <windows.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace mta::game_de::NativeWorldIplLoadedMemory
{
    namespace P = NativeWorldIplLoadedProfile;
    namespace C = NativeStreamingCompletionContext;
    namespace R = NativeModelReferenceMap;
    namespace A = NativeModelObjectGroupProfile;
    namespace T = NativeModelTimeProfile;
    namespace Image = NativeImageAccess;
    namespace
    {
        bool Writable(std::uintptr_t image, std::uintptr_t address, std::size_t size, std::size_t alignment = 8) noexcept
        {
            if (!address || address % alignment || !size || size > (std::numeric_limits<std::uintptr_t>::max)() - address)
                return false;
            const auto end = address + size;
            while (address < end)
            {
                MEMORY_BASIC_INFORMATION page{};
                if (VirtualQuery(reinterpret_cast<void*>(address), &page, sizeof(page)) != sizeof(page) || page.State != MEM_COMMIT ||
                    (page.Protect != PAGE_READWRITE && page.Protect != PAGE_WRITECOPY))
                    return false;
                if (page.Type == MEM_IMAGE)
                {
                    if (reinterpret_cast<std::uintptr_t>(page.AllocationBase) != image || !Image::ImagePages(image, address, 1, true))
                        return false;
                }
                else if (page.Type != MEM_PRIVATE && page.Type != MEM_MAPPED)
                    return false;
                const auto begin = reinterpret_cast<std::uintptr_t>(page.BaseAddress);
                if (begin > address || page.RegionSize > (std::numeric_limits<std::uintptr_t>::max)() - begin || begin + page.RegionSize <= address)
                    return false;
                address = (std::min)(end, begin + page.RegionSize);
            }
            return true;
        }
        template <class Pins>
        bool Match(std::uintptr_t image, const Pins& pins, bool typed = true)
        {
            for (const auto& pin : pins)
            {
                const bool                 code = typed ? pin.code : true;
                std::vector<unsigned char> bytes(pin.size);
                if (!pin.size || !Image::ReadImage(image, image + pin.rva, bytes.data(), bytes.size(), false, code) ||
                    std::memcmp(bytes.data(), pin.bytes, bytes.size()))
                    return false;
            }
            return true;
        }
        bool Profile(std::uintptr_t image)
        {
            if (!Match(image, P::Pins) || !Match(image, A::Pins) || !Match(image, T::Pins) || !R::QualifyProfile(image))
                return false;
            for (const auto& pin : NativeBuildingPoolDefaultProfile::Pins)
            {
                std::vector<unsigned char> bytes(pin.size);
                if (!Image::ReadImage(image, image + pin.rva, bytes.data(), bytes.size(), false, true) || std::memcmp(bytes.data(), pin.bytes, bytes.size()))
                    return false;
            }
            for (const auto& slot : P::Slots)
            {
                std::uintptr_t value = 0;
                if (!NativeModelReferenceHeap::ReadOnlyImage(image, image + slot.rva, sizeof(value)) ||
                    !Image::ReadImage(image, image + slot.rva, &value, sizeof(value)) || value != image + slot.target)
                    return false;
            }
            return true;
        }
        template <class V, std::size_t N>
        V Field(const std::array<unsigned char, N>& bytes, std::size_t offset) noexcept
        {
            V value{};
            if (offset <= N && sizeof(value) <= N - offset)
                std::memcpy(&value, bytes.data() + offset, sizeof(value));
            return value;
        }
        struct Model
        {
            std::uint32_t  index = 0, removed = 0, actors = 0;
            std::uintptr_t address = 0;
            std::uint32_t  count = 0;
            std::uint16_t  actorCount = 0;
        };
        struct Entry
        {
            std::uintptr_t address = 0;
            std::uint32_t  ipl = 0;
            std::size_t    pool = 0;
        };
        struct Reader
        {
            std::uintptr_t                                   image;
            const C::Value&                                  world;
            Snapshot                                         next;
            std::array<std::vector<unsigned char>, 3>        flags;
            std::unordered_map<std::uintptr_t, std::size_t>  members;
            std::unordered_map<std::uint32_t, std::size_t>   models;
            std::unordered_map<std::uint32_t, std::uint32_t> nativeRefcounts;
            bool                                             referenceCaptured = false;
            std::vector<Model>                               modelValues;
            std::unordered_set<std::uintptr_t>               entities, visiting, components, references;
            std::unordered_set<std::uintptr_t>               classes, authorityAddresses;
            std::unordered_map<std::uintptr_t, Entry>        mainSelections;
            std::size_t                                      byteCount = 0;
            bool                                             Copy(std::uintptr_t address, void* value, std::size_t bytes)
            {
                if (!address || !bytes || bytes > 1024 * 1024 || next.tokens.size() >= 200000 || byteCount > 32 * 1024 * 1024 - bytes)
                    return false;
                Token token;
                token.address = address;
                token.bytes.resize(bytes);
                if (!C::Copy(address, token.bytes.data(), bytes))
                    return false;
                std::memcpy(value, token.bytes.data(), bytes);
                byteCount += bytes;
                next.tokens.push_back(std::move(token));
                return true;
            }
            template <class V>
            bool At(std::uintptr_t address, V& value)
            {
                return Copy(address, &value, sizeof(value));
            }
            template <class V>
            bool AuthorityAt(std::uintptr_t address, V& value)
            {
                if (!At(address, value))
                    return false;
                if (authorityAddresses.insert(address).second)
                    next.authorityTokens.push_back(next.tokens.back());
                return true;
            }
            bool Exact(std::uintptr_t address, const void* expected, std::size_t bytes)
            {
                if (!bytes)
                    return true;
                std::vector<unsigned char> value(bytes);
                return Copy(address, value.data(), bytes) && !std::memcmp(value.data(), expected, bytes);
            }
            bool Member(std::uintptr_t address, std::size_t& index)
            {
                const auto known = members.find(address);
                if (known != members.end())
                {
                    index = known->second;
                    return true;
                }
                NativeUObjectMembershipAccess::Snapshot member;
                if (!NativeUObjectMembershipAccess(image, &C::Copy).Capture(address, member))
                    return false;
                index = next.objects.size();
                members.emplace(address, index);
                next.objects.push_back(member);
                return true;
            }
            bool Class(std::uintptr_t address)
            {
                if (classes.count(address))
                    return true;
                const auto     begin = next.tokens.size();
                std::size_t    member = 0;
                std::uintptr_t chain = 0;
                std::int32_t   index = -1;
                if (!Member(address, member) || !At(address + 0x30, chain) || !At(address + 0x38, index) || !chain || index < 0 ||
                    index > NativeClassChainAccess::maximumIndex)
                    return false;
                std::vector<std::uintptr_t> values(static_cast<std::size_t>(index) + 1);
                bool                        matches = false;
                if (!Copy(chain, values.data(), values.size() * sizeof(values[0])) || values.back() != address + 0x30 ||
                    !NativeClassChainAccess(&C::Copy).IsChildOf(address, address, matches) || !matches)
                    return false;
                next.authorities.push_back(next.objects[member]);
                for (auto at = begin; at != next.tokens.size(); ++at)
                    next.authorityTokens.push_back(next.tokens[at]);
                classes.insert(address);
                return true;
            }
            bool PoolRead(std::size_t index, std::uintptr_t root, std::uint32_t capacity, std::size_t stride)
            {
                Pool pool;
                pool.root = root;
                pool.capacity = static_cast<std::int32_t>(capacity);
                pool.stride = stride;
                std::array<unsigned char, 32> header{};
                if (!At(image + root, pool.address) || !C::Writable(pool.address, header.size()) || !At(pool.address, header))
                    return false;
                pool.objects = Field<std::uintptr_t>(header, 0);
                pool.flags = Field<std::uintptr_t>(header, 8);
                const auto cursor = Field<std::int32_t>(header, 0x14);
                if (Field<std::int32_t>(header, 0x10) != pool.capacity || cursor < -1 || cursor >= pool.capacity || header[0x18] != 1 || header[0x19] ||
                    !C::Writable(pool.objects, capacity * stride) || !C::Writable(pool.flags, capacity))
                    return false;
                flags[index].resize(capacity);
                if (!Copy(pool.flags, flags[index].data(), capacity))
                    return false;
                next.pools.push_back(pool);
                return true;
            }
            bool Slot(std::size_t poolIndex, std::uintptr_t address, std::uint32_t& index) const noexcept
            {
                const auto& pool = next.pools[poolIndex];
                if (address < pool.objects || (address - pool.objects) % pool.stride)
                    return false;
                const auto slot = (address - pool.objects) / pool.stride;
                if (slot >= flags[poolIndex].size() || (flags[poolIndex][slot] & 0x80))
                    return false;
                index = static_cast<std::uint32_t>(slot);
                return true;
            }
            bool Component(std::uintptr_t address, std::uintptr_t actor, std::uintptr_t parent)
            {
                if (components.size() >= P::MaximumComponents || !components.insert(address).second || !Writable(image, address, P::ComponentBytes))
                    return false;
                std::size_t    member = 0;
                std::uintptr_t type = 0, attach = 0, data = 0;
                std::int32_t   count = 0, capacity = 0;
                unsigned char  mobility = 0;
                if (!Member(address, member) || next.objects[member].vtableAddress != image + P::ComponentVtable ||
                    next.objects[member].outerAddress != actor || !AuthorityAt(image + P::ComponentClass, type) || next.objects[member].classAddress != type ||
                    !Class(type) || !At(address + 0xc0, attach) || (parent ? attach != parent : attach != 0) || !At(address + 0x14f, mobility) ||
                    mobility > 2 || !At(address + 0xd0, data) || !At(address + 0xd8, count) || !At(address + 0xdc, capacity) || count < 0 || capacity < count ||
                    capacity > static_cast<std::int32_t>(P::MaximumComponents))
                    return false;
                if (!count)
                    return true;
                if (!Writable(image, data, static_cast<std::size_t>(capacity) * 8))
                    return false;
                std::vector<std::uintptr_t> children(static_cast<std::size_t>(count));
                if (!Copy(data, children.data(), children.size() * 8))
                    return false;
                for (auto child : children)
                    if (child && !Component(child, actor, address))
                        return false;
                return true;
            }
            bool Actor(std::uintptr_t address, std::uintptr_t entity, bool mustOwn, bool saved)
            {
                if (!address)
                    return true;
                std::size_t    member = 0;
                std::uintptr_t regular = 0, dynamic = 0, root = 0, mesh = 0, getter = 0, back = 0;
                if (!AuthorityAt(image + P::ActorClass, regular) || !AuthorityAt(image + P::DynamicActorClass, dynamic) || !Member(address, member))
                    return false;
                const auto value = next.objects[member];
                const bool isDynamic = value.classAddress == dynamic;
                bool       derived = false;
                if ((!isDynamic && value.classAddress != regular) || !Class(regular) || !Class(dynamic) ||
                    !NativeClassChainAccess(&C::Copy).IsChildOf(dynamic, regular, derived) || !derived ||
                    value.vtableAddress != image + (isDynamic ? P::DynamicActorVtable : P::ActorVtable) ||
                    !Writable(image, address, isDynamic ? P::DynamicActorBytes : P::ActorBytes) || value.outerAddress != world.level ||
                    !At(value.vtableAddress + 0x158, getter) || getter != image + P::ActorGetWorld || !At(address + 0x130, root) || !root ||
                    !At(address + 0x298, mesh) || mesh != root)
                    return false;
                if ((mustOwn || saved) && (!isDynamic || !At(address + 0x2c8, back) || (back && back != entity)))
                    return false;
                unsigned char traits = 0, destruction = 0;
                std::uint32_t actorFlags = 0, levelFlags = 0;
                if (!At(address + 0x2b0, traits) || !At(address + 8, actorFlags) || (actorFlags & 0x10) || !At(address + 0x5c, destruction) ||
                    (destruction & 8) || !At(world.level + 8, levelFlags) || (levelFlags & 0x8000))
                    return false;
                // Both constructor-owned fields bind one current component.
                // UE owns ordinary mobility registration/child traversal; we
                // fence the exact admitted outer/root graph and live members.
                if (components.count(root))
                {
                    std::size_t rootMember = 0;
                    return Member(root, rootMember) && next.objects[rootMember].outerAddress == address;
                }
                return Component(root, address, 0);
            }
            bool Metadata(std::int16_t id, bool removed, bool actor)
            {
                if (id < 0 || static_cast<std::uint32_t>(id) >= NativeModelBankProfile::ModelCount)
                    return false;
                const auto known = models.find(static_cast<std::uint32_t>(id));
                if (known != models.end())
                {
                    auto& model = modelValues[known->second];
                    model.removed += removed ? 1 : 0;
                    model.actors += removed && actor ? 1 : 0;
                    return true;
                }
                Model model;
                model.index = static_cast<std::uint32_t>(id);
                model.removed = removed ? 1 : 0;
                model.actors = removed && actor ? 1 : 0;
                std::uintptr_t table = 0, expected = 0, countAddress = 0, base = 0;
                std::int32_t   count = 0;
                std::size_t    stride = 0, maximum = 0;
                if (!At(image + NativeModelBankProfile::BankRva + model.index * 8, model.address) || !model.address || !At(model.address, table))
                    return false;
                if (table == image + A::VtableRva)
                {
                    base = image + A::PoolRva;
                    countAddress = image + A::PoolCountRva;
                    stride = A::PoolStride;
                    maximum = A::PoolCapacity;
                    expected = image + A::TypeGetterRva;
                }
                else if (table == image + T::VtableRva)
                {
                    base = image + T::PoolRva;
                    countAddress = image + T::PoolCountRva;
                    stride = T::RowBytes;
                    maximum = T::PoolCapacity;
                    expected = image + T::TypeMethodRva;
                }
                else
                    return false;
                std::uintptr_t method = 0;
                if (!At(countAddress, count) || count < 0 || static_cast<std::size_t>(count) > maximum || model.address < base ||
                    (model.address - base) % stride || (model.address - base) / stride >= static_cast<std::size_t>(count) ||
                    !Image::ImagePages(image, model.address, stride, true) || !At(table + 0x20, method) || method != expected ||
                    !At(model.address + 0x22, model.actorCount))
                    return false;
                std::array<unsigned char, 0x60> bytes{};
                if (!Copy(model.address, bytes.data(), bytes.size()) || Field<std::uint16_t>(bytes, 0x22) != model.actorCount ||
                    Field<std::uintptr_t>(bytes, 0) != table)
                    return false;
                if (!referenceCaptured)
                {
                    NativeStreamingModelContext::Value context;
                    const auto                         providerBegin = next.tokens.size();
                    if (NativeStreamingModelContext::CaptureProvider(image, context) != ClockResult::Ok || context.root != world.root ||
                        !Exact(context.root + NativeStreamingRequestProfile::ProviderOffset, &context.provider, sizeof(context.provider)) ||
                        !Exact(context.provider + NativeStreamingRequestProfile::ProviderRowsOffset, &context.rows, sizeof(context.rows)) ||
                        !Exact(context.provider + NativeStreamingRequestProfile::ProviderCountOffset, &context.count, sizeof(context.count)))
                        return false;
                    next.objects.push_back(context.rootMembership);
                    next.objects.push_back(context.providerMembership);
                    next.authorities.push_back(context.rootMembership);
                    next.authorities.push_back(context.providerMembership);
                    for (auto at = providerBegin; at != next.tokens.size(); ++at)
                        next.authorityTokens.push_back(next.tokens[at]);
                    NativeModelBankReader::Value selected;
                    selected.modelIndex = model.index;
                    selected.nativeModelInfo = model.address;
                    R::Snapshot mapSnapshot, again;
                    if (R::Capture(context, selected, mapSnapshot) != ClockResult::Ok || mapSnapshot.selected < 0 ||
                        R::Capture(context, selected, again) != ClockResult::Ok || !R::Same(mapSnapshot, again, false, 0) ||
                        !Exact(mapSnapshot.map, mapSnapshot.header.data(), mapSnapshot.header.size()) ||
                        !Exact(mapSnapshot.rowsAddress, mapSnapshot.rows.data(), mapSnapshot.rows.size() * sizeof(R::Row)) ||
                        !Exact(mapSnapshot.bitsAddress, mapSnapshot.bits.data(), mapSnapshot.bits.size() * 4) ||
                        !Exact(mapSnapshot.hashAddress, mapSnapshot.hash.data(), mapSnapshot.hash.size() * 4))
                        return false;
                    // The root owns one sparse map shared by every selected
                    // model. Fence its genuine rows/bits/hash once instead of
                    // copying the entire allocation again for each IPL/model.
                    for (std::size_t row = 0; row != mapSnapshot.rows.size(); ++row)
                        if (R::Occupied(mapSnapshot, row))
                            nativeRefcounts.emplace(mapSnapshot.rows[row].key, mapSnapshot.rows[row].count);
                    referenceCaptured = true;
                }
                const auto reference = nativeRefcounts.find(model.index);
                if (reference == nativeRefcounts.end())
                    return false;
                model.count = reference->second;
                models.emplace(model.index, modelValues.size());
                modelValues.push_back(model);
                return true;
            }
            bool References(std::uintptr_t head, std::uintptr_t entity)
            {
                std::unordered_set<std::uintptr_t> visited;
                for (auto at = head; at;)
                {
                    if (references.size() >= P::MaximumReferences || !visited.insert(at).second || !references.insert(at).second || !Writable(image, at, 16))
                        return false;
                    std::array<std::uintptr_t, 2> node{};
                    std::uintptr_t                value = 0;
                    if (!At(at, node) || !Writable(image, node[1], 8) || !At(node[1], value) || value != entity)
                        return false;
                    at = node[0];
                }
                return true;
            }
            bool Entity(std::uintptr_t address, std::size_t poolIndex, bool removed, std::uint32_t ipl)
            {
                if (visiting.count(address))
                    return false;
                if (!entities.insert(address).second)
                    return true;
                visiting.insert(address);
                struct Visit
                {
                    std::unordered_set<std::uintptr_t>& values;
                    std::uintptr_t                      address;
                    ~Visit() { values.erase(address); }
                } visit{visiting, address};
                const auto main = mainSelections.find(address);
                if (main != mainSelections.end())
                {
                    removed = true;
                    ipl = main->second.ipl;
                    if (poolIndex != main->second.pool)
                        return false;
                }
                std::uint32_t index = 0;
                if (!Slot(poolIndex, address, index))
                    return false;
                const auto&                pool = next.pools[poolIndex];
                std::vector<unsigned char> bytes(pool.stride);
                if (!Copy(address, bytes.data(), bytes.size()))
                    return false;
                std::array<unsigned char, 0x248> row{};
                std::memcpy(row.data(), bytes.data(), bytes.size());
                const auto table = Field<std::uintptr_t>(row, 0);
                const auto type = static_cast<unsigned char>(row[0x6a] & 7);
                if (poolIndex == 0   ? (type != 1 || (table != image + P::BuildingVtable && table != image + P::SpecialBuildingVtable))
                    : poolIndex == 1 ? (type != 5 || table != image + P::DummyVtable)
                                     : (type != 4 || table != image + P::ObjectVtable))
                    return false;
                if (removed)
                    next.deleted.push_back({address, pool.flags + index, ipl, flags[poolIndex][index]});
                const auto actor = Field<std::uintptr_t>(row, 0x20);
                const bool mustOwn = (row[0x34] & 0x20) != 0;
                if (!Actor(actor, address, mustOwn, false) || !Metadata(Field<std::int16_t>(row, 0x3a), removed, actor != 0) ||
                    !References(Field<std::uintptr_t>(row, 0x40), address))
                    return false;
                const auto matrix = Field<std::uintptr_t>(row, 0x18);
                if (matrix)
                {
                    std::array<unsigned char, 0x58> value{};
                    if (!Writable(image, matrix, value.size()) || !At(matrix, value) || Field<std::uintptr_t>(value, 0x50) != address)
                        return false;
                    for (std::size_t r = 0; r != 4; ++r)
                        for (std::size_t c = 0; c != 3; ++c)
                            if (!std::isfinite(Field<float>(value, r * 16 + c * 4)))
                                return false;
                }
                else
                    for (std::size_t at = 8; at != 24; at += 4)
                        if (!std::isfinite(Field<float>(row, at)))
                            return false;
                const auto lod = Field<std::uintptr_t>(row, 0x60);
                if (lod)
                {
                    bool found = false;
                    for (std::size_t p = 0; p != 3; ++p)
                    {
                        std::uint32_t unused = 0;
                        if (Slot(p, lod, unused))
                        {
                            found = Entity(lod, p, false, 0);
                            break;
                        }
                    }
                    if (!found)
                        return false;
                }
                if (poolIndex == 1 && !Actor(Field<std::uintptr_t>(row, 0x70), address, true, true))
                    return false;
                if (poolIndex == 2)
                {
                    // These optional native children need distinct owner
                    // domains. Refuse before the whole IPL call rather than
                    // claim FX/fire/alternate texture/animation or the extra
                    // moving-object list's allocation/neighbor cleanup here.
                    if ((Field<std::uint32_t>(row, 0x1bc) & 0x310000) || Field<std::int16_t>(row, 0x1c6) != -1 || Field<std::uintptr_t>(row, 0x1f0) ||
                        Field<std::uintptr_t>(row, 0x1f8) || Field<std::uintptr_t>(row, 0x1e4) || Field<std::uintptr_t>(row, 0x1b0) ||
                        !Actor(Field<std::uintptr_t>(row, 0x1a8), address, true, true))
                        return false;
                    const auto dummy = Field<std::uintptr_t>(row, 0x200);
                    if (dummy)
                    {
                        std::uint32_t dummyIndex = 0;
                        std::int16_t  model = -1;
                        unsigned char dummyIpl = 0;
                        if (!Slot(1, dummy, dummyIndex) || !At(dummy + 0x3a, model) || model != Field<std::int16_t>(row, 0x3a) || !At(dummy + 0x56, dummyIpl) ||
                            dummyIpl != row[0x56] || !Entity(dummy, 1, false, 0))
                            return false;
                        std::uintptr_t                     head = 0;
                        bool                               registered = false;
                        std::unordered_set<std::uintptr_t> seen;
                        if (!At(dummy + 0x40, head))
                            return false;
                        while (head)
                        {
                            std::array<std::uintptr_t, 2> ref{};
                            if (seen.size() >= P::MaximumReferences || !seen.insert(head).second || !At(head, ref))
                                return false;
                            if (ref[1] == address + 0x200)
                                registered = true;
                            head = ref[0];
                        }
                        if (!registered)
                            return false;
                    }
                }
                return true;
            }
            bool Select(const std::vector<Ipl>& ipls)
            {
                std::vector<Entry>                selected;
                std::unordered_set<std::uint32_t> selectedIpls;
                const auto                        ranges = [&](const Ipl& ipl, std::size_t pool, std::size_t offset)
                {
                    const auto first = Field<std::int16_t>(ipl.bytes, offset), last = Field<std::int16_t>(ipl.bytes, offset + 2);
                    if (first > last)
                        return true;
                    if (first < 0 || last >= next.pools[pool].capacity)
                        return false;
                    for (auto index = first; index <= last; ++index)
                    {
                        if (flags[pool][static_cast<std::size_t>(index)] & 0x80)
                            continue;
                        const auto    address = next.pools[pool].objects + static_cast<std::size_t>(index) * next.pools[pool].stride;
                        unsigned char value = 0;
                        if (!At(address + 0x56, value))
                            return false;
                        if (value == ipl.index)
                            selected.push_back({address, ipl.index, pool});
                    }
                    return true;
                };
                for (const auto& ipl : ipls)
                {
                    if (!ipl.index || ipl.index >= 256 || !selectedIpls.insert(ipl.index).second || !ranges(ipl, 0, 0x22) || !ranges(ipl, 1, 0x26))
                        return false;
                }
                // Native RemoveIPL scans this shared pool for each index.
                // Snapshot it once: unrelated rows need only their IPL byte,
                // while selected rows own one bounded full-row preimage.
                for (std::size_t index = 0; index != flags[2].size(); ++index)
                {
                    if (flags[2][index] & 0x80)
                        continue;
                    const auto    address = next.pools[2].objects + index * P::ObjectBytes;
                    unsigned char value = 0;
                    if (!At(address + 0x56, value))
                        return false;
                    if (selectedIpls.count(value))
                        selected.push_back({address, value, 2});
                }
                // Mark all removals first. A related Dummy/LOD discovered
                // before its own loop still contributes one destruction and
                // one model-map decrement, rather than disappearing in dedup.
                for (const auto& entry : selected)
                    if (!mainSelections.emplace(entry.address, entry).second)
                        return false;
                for (const auto& entry : selected)
                    if (!Entity(entry.address, entry.pool, true, entry.ipl))
                        return false;
                for (const auto& value : modelValues)
                    if (value.removed > value.count || value.actors > value.actorCount)
                        return false;
                return true;
            }
        };
    }
    ClockResult Capture(std::uintptr_t image, const C::Value& world, const std::vector<Ipl>& ipls, Snapshot& output)
    {
        if (ipls.empty())
        {
            output = {};
            return ClockResult::Ok;
        }
        if (!Profile(image))
            return ClockResult::UnsupportedBuild;
        Reader reader{image, world};
        if (!reader.PoolRead(0, P::BuildingRoot, P::BuildingCapacity, P::BuildingBytes) || !reader.PoolRead(1, P::DummyRoot, P::DummyCapacity, P::DummyBytes) ||
            !reader.PoolRead(2, P::ObjectRoot, P::ObjectCapacity, P::ObjectBytes))
            return ClockResult::UnsupportedBuild;
        struct Span
        {
            std::uintptr_t address;
            std::size_t    bytes;
        };
        std::array<Span, 9> spans{};
        std::size_t         count = 0;
        for (const auto& pool : reader.next.pools)
        {
            spans[count++] = {pool.address, 32};
            spans[count++] = {pool.objects, static_cast<std::size_t>(pool.capacity) * pool.stride};
            spans[count++] = {pool.flags, static_cast<std::size_t>(pool.capacity)};
        }
        for (std::size_t a = 0; a != count; ++a)
            for (std::size_t b = a + 1; b != count; ++b)
                if (spans[a].address < spans[b].address + spans[b].bytes && spans[b].address < spans[a].address + spans[a].bytes)
                    return ClockResult::UnsupportedBuild;
        if (!reader.Select(ipls) || !Seal(image, reader.next))
            return ClockResult::UnsupportedBuild;
        output = std::move(reader.next);
        return ClockResult::Ok;
    }
    bool Seal(std::uintptr_t image, const Snapshot& snapshot) noexcept
    {
        try
        {
            if (snapshot.pools.empty())
                return true;
            if (!Profile(image))
                return false;
            for (const auto& token : snapshot.tokens)
            {
                std::vector<unsigned char> current(token.bytes.size());
                if (!C::Copy(token.address, current.data(), current.size()) || current != token.bytes)
                    return false;
            }
            NativeUObjectMembershipAccess members(image, &C::Copy);
            for (const auto& object : snapshot.objects)
                if (!members.Recheck(object))
                    return false;
            return true;
        }
        catch (...)
        {
            return false;
        }
    }
    bool Removed(std::uintptr_t image, const Snapshot& snapshot, std::uint32_t ipl) noexcept
    {
        try
        {
            if (!Profile(image))
                return false;
            for (const auto& token : snapshot.authorityTokens)
            {
                std::vector<unsigned char> current(token.bytes.size());
                if (!C::Copy(token.address, current.data(), current.size()) || current != token.bytes)
                    return false;
            }
            NativeUObjectMembershipAccess members(image, &C::Copy);
            for (const auto& object : snapshot.authorities)
                if (!members.Recheck(object))
                    return false;
            for (const auto& pool : snapshot.pools)
            {
                std::uintptr_t                address = 0;
                std::array<unsigned char, 32> header{};
                if (!C::Copy(image + pool.root, &address, sizeof(address)) || address != pool.address || !C::Copy(address, header.data(), header.size()) ||
                    Field<std::uintptr_t>(header, 0) != pool.objects || Field<std::uintptr_t>(header, 8) != pool.flags ||
                    Field<std::int32_t>(header, 0x10) != pool.capacity)
                    return false;
            }
            for (const auto& entry : snapshot.deleted)
            {
                if (entry.ipl != ipl)
                    continue;
                unsigned char flag = 0;
                if (!C::Copy(entry.flag, &flag, sizeof(flag)) || flag != static_cast<unsigned char>(entry.generation | 0x80))
                    return false;
            }
            return true;
        }
        catch (...)
        {
            return false;
        }
    }
}
