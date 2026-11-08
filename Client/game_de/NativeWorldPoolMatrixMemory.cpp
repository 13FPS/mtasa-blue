/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Admit the real matrix bank and static list without owner eviction
 ****************************************************************************/
#include "NativeWorldPoolMatrixMemory.h"
#include "NativeWorldIplLoadedProfile.h"
#include "NativeBuildingPoolDefaultProfile.h"
#include "NativeImageAccess.h"
#include <windows.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <utility>
namespace mta::game_de::NativeWorldPoolMatrixMemory
{
    namespace P = NativeWorldPoolMatrixProfile;
    namespace D = NativeStreamingCompletionContext;
    namespace H = NativeModelReferenceHeap;
    namespace E = NativeWorldIplLoadedProfile;
    namespace Image = NativeImageAccess;
    namespace
    {
        template <class V, class Bytes>
        V Field(const Bytes& bytes, std::size_t offset) noexcept
        {
            V value{};
            if (offset <= bytes.size() && sizeof(value) <= bytes.size() - offset)
                std::memcpy(&value, bytes.data() + offset, sizeof(value));
            return value;
        }
        template <class V>
        bool At(std::uintptr_t address, V& output) noexcept
        {
            return D::Copy(address, &output, sizeof(output));
        }
        bool Disjoint(std::uintptr_t a, std::size_t an, std::uintptr_t b, std::size_t bn) noexcept
        {
            return a && b && an && bn && an <= (std::numeric_limits<std::uintptr_t>::max)() - a && bn <= (std::numeric_limits<std::uintptr_t>::max)() - b &&
                   (a + an <= b || b + bn <= a);
        }
        template <class Pins>
        bool Match(std::uintptr_t image, const Pins& pins, bool typed)
        {
            for (const auto& pin : pins)
            {
                std::vector<unsigned char> bytes(pin.size);
                if (!pin.size || !Image::ReadImage(image, image + pin.rva, bytes.data(), bytes.size(), false, typed ? pin.code : true) ||
                    std::memcmp(bytes.data(), pin.bytes, bytes.size()))
                    return false;
            }
            return true;
        }
        bool TypedPoolProfile(std::uintptr_t image)
        {
            for (const auto& pin : NativeBuildingPoolDefaultProfile::Pins)
            {
                std::vector<unsigned char> bytes(pin.size);
                if (!Image::ReadImage(image, image + pin.rva, bytes.data(), bytes.size(), false, true) || std::memcmp(bytes.data(), pin.bytes, bytes.size()))
                    return false;
            }
            // The actual Dummy allocator and IPL constructor publish the
            // current Type5/4318020 receiver. No classic byte layout is used.
            const P::Pin pins[] = {{0x11716ef, E::DummyPoolBytes.data(), E::DummyPoolBytes.size(), true},
                                   {0x112e820, E::DummyAllocateBytes.data(), E::DummyAllocateBytes.size(), true},
                                   {0x1141700, E::IplConstructBytes.data(), E::IplConstructBytes.size(), true}};
            return Match(image, pins, true);
        }
        bool Slot(const Value& value, std::uintptr_t address, std::size_t& index) noexcept
        {
            if (!value.bank || address < value.bank || (address - value.bank) % P::NodeBytes)
                return false;
            index = (address - value.bank) / P::NodeBytes;
            return index < value.nodes.size();
        }
        bool ListWalk(std::uintptr_t image, Value& value, List list, std::uintptr_t start, std::uintptr_t end)
        {
            const auto offset = static_cast<std::size_t>(start - P::DynamicStart);
            const auto endOffset = static_cast<std::size_t>(end - P::DynamicStart);
            auto       current = Field<std::uintptr_t>(value.sentinels, offset + P::NextOffset);
            auto       previous = image + start;
            auto&      order = value.lists[static_cast<std::size_t>(list)];
            while (current != image + end)
            {
                std::size_t index = 0;
                if (order.size() >= P::Capacity || !Slot(value, current, index))
                    return false;
                auto& node = value.nodes[index];
                if (node.list != List::Unbound || node.previous != previous)
                    return false;
                node.list = list;
                order.push_back(index);
                previous = current;
                current = node.next;
            }
            return Field<std::uintptr_t>(value.sentinels, endOffset + P::PrevOffset) == previous;
        }
        ClockResult Store(const NodeView& node, const Transform& value) noexcept
        {
            __try
            {
                std::memcpy(reinterpret_cast<void*>(node.address), value.data(), value.size());
                return ClockResult::Ok;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                return ClockResult::WriteFailure;
            }
        }
    }
    bool Profile(std::uintptr_t image) noexcept
    {
        try
        {
            return image && D::Profile(image) && H::QualifyProfile(image) && Match(image, P::MatrixPins, true) && TypedPoolProfile(image);
        }
        catch (...)
        {
            return false;
        }
    }
    ClockResult CaptureEntity(std::uintptr_t image, Kind kind, std::uint32_t index, Entity& output)
    {
        if (kind != Kind::Building && kind != Kind::Dummy)
            return ClockResult::InvalidArgument;
        if (!Profile(image))
            return ClockResult::UnsupportedBuild;
        Entity value;
        value.kind = kind;
        value.index = index;
        value.root = kind == Kind::Building ? E::BuildingRoot : E::DummyRoot;
        value.capacity = static_cast<std::int32_t>(kind == Kind::Building ? E::BuildingCapacity : E::DummyCapacity);
        value.stride = kind == Kind::Building ? E::BuildingBytes : E::DummyBytes;
        if (index >= static_cast<std::uint32_t>(value.capacity))
            return ClockResult::InvalidArgument;
        if (!Image::ReadImage(image, image + value.root, &value.pool, sizeof(value.pool), true) || !D::Writable(value.pool, value.header.size()) ||
            !At(value.pool, value.header))
            return ClockResult::ReadFailure;
        value.objects = Field<std::uintptr_t>(value.header, 0);
        value.flags = Field<std::uintptr_t>(value.header, 8);
        const auto cursor = Field<std::int32_t>(value.header, 0x14);
        if (Field<std::int32_t>(value.header, 0x10) != value.capacity || cursor < -1 || cursor >= value.capacity || value.header[0x18] != 1 ||
            value.header[0x19] || !D::Writable(value.objects, static_cast<std::size_t>(value.capacity) * value.stride) ||
            !D::Writable(value.flags, static_cast<std::size_t>(value.capacity)) || !At(value.flags + index, value.generation))
            return ClockResult::BindingChanged;
        if (!Disjoint(value.pool, value.header.size(), value.objects, static_cast<std::size_t>(value.capacity) * value.stride) ||
            !Disjoint(value.pool, value.header.size(), value.flags, static_cast<std::size_t>(value.capacity)) ||
            !Disjoint(value.objects, static_cast<std::size_t>(value.capacity) * value.stride, value.flags, static_cast<std::size_t>(value.capacity)))
            return ClockResult::BindingChanged;
        if (value.generation & 0x80)
            return ClockResult::NotInitialized;
        value.address = value.objects + index * value.stride;
        if (!D::Copy(value.address, value.bytes.data(), value.stride))
            return ClockResult::ReadFailure;
        value.vtable = Field<std::uintptr_t>(value.bytes, 0);
        value.matrix = Field<std::uintptr_t>(value.bytes, 0x18);
        const auto type = static_cast<unsigned char>(value.bytes[0x6a] & 7);
        if ((kind == Kind::Building ? (type != 1 || (value.vtable != image + E::BuildingVtable && value.vtable != image + E::SpecialBuildingVtable))
                                    : (type != 5 || value.vtable != image + E::DummyVtable)) ||
            !H::ReadOnlyImage(image, value.vtable, sizeof(std::uintptr_t)))
            return ClockResult::BindingChanged;
        if (!SealEntity(image, value))
            return ClockResult::BindingChanged;
        output = value;
        return ClockResult::Ok;
    }
    bool SealEntity(std::uintptr_t image, const Entity& before) noexcept
    {
        std::uintptr_t                  pool = 0;
        unsigned char                   generation = 0;
        std::array<unsigned char, 32>   header{};
        std::array<unsigned char, 0x78> bytes{};
        return Image::ReadImage(image, image + before.root, &pool, sizeof(pool), true) && pool == before.pool && At(pool, header) && header == before.header &&
               At(before.flags + before.index, generation) && generation == before.generation && D::Copy(before.address, bytes.data(), before.stride) &&
               bytes == before.bytes;
    }
    bool ValidTransform(const Transform& bytes) noexcept
    {
        for (std::size_t row = 0; row != 4; ++row)
            for (std::size_t column = 0; column != 3; ++column)
                if (!std::isfinite(Field<float>(bytes, row * 16 + column * 4)))
                    return false;
        return true;
    }
    ClockResult Capture(std::uintptr_t image, Value& output)
    {
        if (!Profile(image))
            return ClockResult::UnsupportedBuild;
        Value value;
        auto  result = D::Capture(image, value.world);
        if (result != ClockResult::Ok)
            return result;
        if ((result = H::Capture(image, value.heap)) != ClockResult::Ok)
            return result;
        if (!Image::ReadImage(image, image + P::BankRva, &value.bank, sizeof(value.bank), true) || value.bank < P::CookieBytes ||
            !D::Writable(value.bank - P::CookieBytes, P::CookieBytes + P::BankBytes) || !At(value.bank - P::CookieBytes, value.cookie) ||
            value.cookie != P::Capacity || !Image::ReadImage(image, image + P::DynamicStart, value.sentinels.data(), value.sentinels.size(), true))
            return ClockResult::BindingChanged;
        value.nodes.resize(P::Capacity);
        for (std::size_t index = 0; index != P::Capacity; ++index)
        {
            auto& node = value.nodes[index];
            node.address = value.bank + index * P::NodeBytes;
            if (!At(node.address, node.bytes))
                return ClockResult::ReadFailure;
            node.owner = Field<std::uintptr_t>(node.bytes, P::OwnerOffset);
            node.previous = Field<std::uintptr_t>(node.bytes, P::PrevOffset);
            node.next = Field<std::uintptr_t>(node.bytes, P::NextOffset);
            node.attached = Field<std::uintptr_t>(node.bytes, P::AttachedOffset);
            node.opaque48 = Field<std::uint64_t>(node.bytes, P::Opaque48Offset);
        }
        if (!ListWalk(image, value, List::Dynamic, P::DynamicStart, P::DynamicEnd) || !ListWalk(image, value, List::Static, P::StaticStart, P::StaticEnd) ||
            !ListWalk(image, value, List::Free, P::FreeStart, P::FreeEnd))
            return ClockResult::BindingChanged;
        for (const auto& node : value.nodes)
        {
            if (node.list == List::Unbound || (node.list == List::Free ? node.owner != 0 : node.owner == 0))
                return ClockResult::BindingChanged;
            if (node.list != List::Free)
            {
                std::uintptr_t matrix = 0;
                if (!D::Writable(node.owner, 0x20) || !At(node.owner + 0x18, matrix) || matrix != node.address)
                    return ClockResult::BindingChanged;
            }
        }
        if (!Seal(image, value))
            return ClockResult::BindingChanged;
        output = std::move(value);
        return ClockResult::Ok;
    }
    bool SameBank(const Value& a, const Value& b) noexcept
    {
        return a.bank == b.bank && a.cookie == b.cookie && D::Same(a.world, b.world, true) && H::Same(a.heap, b.heap);
    }
    bool Seal(std::uintptr_t image, const Value& before) noexcept
    {
        try
        {
            D::Value                   world;
            std::uintptr_t             bank = 0;
            std::uint64_t              cookie = 0;
            decltype(before.sentinels) sentinels{};
            if (!Profile(image) || D::Capture(image, world) != ClockResult::Ok || !D::Same(before.world, world, true) ||
                H::Recheck(image, before.heap) != ClockResult::Ok || !Image::ReadImage(image, image + P::BankRva, &bank, sizeof(bank), true) ||
                bank != before.bank || !At(bank - P::CookieBytes, cookie) || cookie != before.cookie ||
                !Image::ReadImage(image, image + P::DynamicStart, sentinels.data(), sentinels.size(), true) || sentinels != before.sentinels ||
                before.nodes.size() != P::Capacity)
                return false;
            for (const auto& node : before.nodes)
            {
                std::array<unsigned char, P::NodeBytes> bytes{};
                if (!At(node.address, bytes) || bytes != node.bytes)
                    return false;
                if (node.list != List::Free)
                {
                    std::uintptr_t matrix = 0;
                    if (!At(node.owner + 0x18, matrix) || matrix != node.address)
                        return false;
                }
            }
            return true;
        }
        catch (...)
        {
            return false;
        }
    }
    ClockResult FindOwner(const Value& value, const Entity& entity, NodeView& output) noexcept
    {
        if (!entity.matrix)
            return ClockResult::NotInitialized;
        if (!Disjoint(value.bank - P::CookieBytes, P::CookieBytes + P::BankBytes, entity.pool, entity.header.size()) ||
            !Disjoint(value.bank - P::CookieBytes, P::CookieBytes + P::BankBytes, entity.objects, static_cast<std::size_t>(entity.capacity) * entity.stride) ||
            !Disjoint(value.bank - P::CookieBytes, P::CookieBytes + P::BankBytes, entity.flags, static_cast<std::size_t>(entity.capacity)))
            return ClockResult::BindingChanged;
        std::size_t index = 0;
        if (!Slot(value, entity.matrix, index) || value.nodes[index].owner != entity.address || value.nodes[index].list == List::Free)
            return ClockResult::BindingChanged;
        const auto& node = value.nodes[index];
        Transform   transform{};
        std::copy_n(node.bytes.begin(), transform.size(), transform.begin());
        if (node.attached || !ValidTransform(transform))
            return ClockResult::UnsupportedBuild;
        output = node;
        return ClockResult::Ok;
    }
    ClockResult RequireFree(const Value& value, std::size_t count) noexcept
    {
        const auto& order = value.lists[static_cast<std::size_t>(List::Free)];
        if (count > order.size())
            return ClockResult::NotInitialized;
        // The native static allocator consumes the head in this exact order.
        // A later unqualified attached node cannot be hidden by a free count.
        for (std::size_t at = 0; at != count; ++at)
        {
            if (order[at] >= value.nodes.size())
                return ClockResult::BindingChanged;
            const auto& node = value.nodes[order[at]];
            if (node.list != List::Free || node.owner)
                return ClockResult::BindingChanged;
            if (node.attached)
                return ClockResult::UnsupportedBuild;
        }
        return ClockResult::Ok;
    }
    ClockResult StoreTransform(std::uintptr_t image, const Value& value, const Entity& entity, const Transform& transform) noexcept
    {
        if (!ValidTransform(transform))
            return ClockResult::InvalidArgument;
        NodeView   node;
        const auto result = FindOwner(value, entity, node);
        if (result != ClockResult::Ok)
            return result;
        if (node.list != List::Static || !SealEntity(image, entity) || !Seal(image, value))
            return ClockResult::BindingChanged;
        return Store(node, transform);
    }
}
