/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Read complete native flag banks without publishing row lifetimes
 ****************************************************************************/
#include "NativePoolUsageMemory.h"
#include "NativeBuildingPoolDefaultProfile.h"
#include "NativeWorldIplLoadedProfile.h"
#include "NativeWorldAddProfile.h"
#include "NativePedPoolProfile.h"
#include "NativeTaskPoolProfile.h"
#include "NativeModelReferenceHeap.h"
#include "NativeImageAccess.h"
#include <game/CPoolSlotPolicy.h>
#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <utility>
#include <vector>

namespace mta::game_de::NativePoolUsageMemory
{
    namespace UsageD = NativeStreamingCompletionContext;
    namespace UsageH = NativeModelReferenceHeap;
    namespace UsageI = NativeImageAccess;
    namespace UsageE = NativeWorldIplLoadedProfile;
    namespace UsageA = NativeWorldAddProfile;
    namespace
    {
        struct Geometry
        {
            std::uintptr_t root = 0;
            std::uint32_t  capacity = 0;
            std::size_t    stride = 0;
        };
        bool Select(Kind kind, Geometry& output) noexcept
        {
            switch (kind)
            {
                case Kind::Building:
                    output = {UsageE::BuildingRoot, UsageE::BuildingCapacity, UsageE::BuildingBytes};
                    break;
                case Kind::Ped:
                    output = {NativePedPoolProfile::RootRva, static_cast<std::uint32_t>(NativePedPoolProfile::NativeCapacity),
                              NativePedPoolProfile::ObjectStride};
                    break;
                case Kind::Object:
                    output = {UsageE::ObjectRoot, UsageE::ObjectCapacity, UsageE::ObjectBytes};
                    break;
                case Kind::Dummy:
                    output = {UsageE::DummyRoot, UsageE::DummyCapacity, UsageE::DummyBytes};
                    break;
                case Kind::Task:
                    output = {NativeTaskPoolProfile::RootRva, static_cast<std::uint32_t>(NativeTaskPoolProfile::NativeCapacity),
                              NativeTaskPoolProfile::ObjectStride};
                    break;
                case Kind::PointerDoubleLink:
                    output = {UsageA::DoubleRootRva, static_cast<std::uint32_t>(UsageA::DoubleCapacity), UsageA::DoubleBytes};
                    break;
                case Kind::PointerSingleLink:
                    output = {UsageA::SingleRootRva, static_cast<std::uint32_t>(UsageA::SingleCapacity), UsageA::SingleBytes};
                    break;
                default:
                    return false;
            }
            return true;
        }
        template <class Bytes>
        bool Match(std::uintptr_t image, std::uintptr_t rva, const Bytes& bytes)
        {
            std::vector<unsigned char> current(bytes.size());
            return !current.empty() && UsageI::ReadImage(image, image + rva, current.data(), current.size(), false, true) &&
                   std::memcmp(current.data(), bytes.data(), current.size()) == 0;
        }
        bool Profile(std::uintptr_t image, Kind kind)
        {
            if (!UsageD::Profile(image) || !UsageH::QualifyProfile(image))
                return false;
            // Match the selected constructor publication and flag allocator.
            // No native count leaf, constructor or allocator is called: the
            // existing source-qualified bit7 convention supplies occupancy.
            switch (kind)
            {
                case Kind::Building:
                    for (const auto& pin : NativeBuildingPoolDefaultProfile::Pins)
                    {
                        std::vector<unsigned char> current(pin.size);
                        if (!pin.size || !UsageI::ReadImage(image, image + pin.rva, current.data(), current.size(), false, true) ||
                            std::memcmp(current.data(), pin.bytes, current.size()))
                            return false;
                    }
                    return true;
                case Kind::Ped:
                    return Match(image, 0x11713c0, NativePedPoolProfile::Initializer) && Match(image, 0x121a860, NativePedPoolProfile::Allocator);
                case Kind::Object:
                    return Match(image, 0x117161f, UsageE::ObjectPoolBytes) && Match(image, 0x12128a0, UsageE::ObjectAllocateBytes);
                case Kind::Dummy:
                    return Match(image, 0x11716ef, UsageE::DummyPoolBytes) && Match(image, 0x112e820, UsageE::DummyAllocateBytes);
                case Kind::Task:
                    return Match(image, 0x11718a0, NativeTaskPoolProfile::Initializer) && Match(image, 0x12d2e20, NativeTaskPoolProfile::Allocator);
                case Kind::PointerDoubleLink:
                    return Match(image, 0x1171202, NativeWorldCoverProfile::DoubleLinkPoolPublishingIntervalBytes) &&
                           Match(image, 0x1173820, UsageA::DoubleAllocateBytes);
                case Kind::PointerSingleLink:
                    return Match(image, 0x1171130, NativeWorldIplPoolProfile::SingleInitialize) && Match(image, 0x1131ff0, UsageA::AddRectBytes);
                default:
                    return false;
            }
        }
        template <class T, class Bytes>
        T Field(const Bytes& bytes, std::size_t offset) noexcept
        {
            T value{};
            std::memcpy(&value, bytes.data() + offset, sizeof(value));
            return value;
        }
        struct Snapshot
        {
            UsageD::Value                 world;
            UsageH::Value                 heap;
            std::uintptr_t                address = 0, objects = 0, flags = 0;
            std::array<unsigned char, 32> header{};
            std::vector<unsigned char>    states;
            Value                         usage;
        };
        bool Same(const Snapshot& a, const Snapshot& b) noexcept
        {
            return a.address == b.address && a.objects == b.objects && a.flags == b.flags && a.header == b.header && a.states == b.states &&
                   a.usage.capacity == b.usage.capacity && a.usage.used == b.usage.used && UsageD::Same(a.world, b.world, true) && UsageH::Same(a.heap, b.heap);
        }
        ClockResult Owner(std::uintptr_t image, CheckOwner owner, CheckOwner after) noexcept
        {
            const auto result = after();
            return result == ClockResult::Ok ? UsageD::Owner(image, owner) : result;
        }
        bool Seal(std::uintptr_t image, Kind kind, const Geometry& geometry, const Snapshot& value)
        {
            if (!Profile(image, kind) || UsageH::Recheck(image, value.heap) != ClockResult::Ok || !UsageD::Fence(image, value.world))
                return false;
            std::uintptr_t                address = 0;
            std::array<unsigned char, 32> header{};
            if (!UsageI::ReadImage(image, image + geometry.root, &address, sizeof(address), true) || address != value.address ||
                !UsageD::Copy(address, header.data(), header.size()) || header != value.header)
                return false;
            // Compare every generation byte, including the tail. An unchanged
            // aggregate count cannot conceal a free/occupied swap or recycled
            // slot during an owner callback; the entire observation must hold.
            std::array<unsigned char, 256> chunk{};
            for (std::size_t at = 0; at < value.states.size(); at += chunk.size())
            {
                const auto count = (std::min)(chunk.size(), value.states.size() - at);
                if (!UsageD::Copy(value.flags + at, chunk.data(), count) || std::memcmp(chunk.data(), value.states.data() + at, count))
                    return false;
            }
            UsageD::Value currentWorld;
            return UsageD::Capture(image, currentWorld) == ClockResult::Ok && UsageD::Same(value.world, currentWorld, true) &&
                   UsageD::Fence(image, currentWorld) && UsageH::Recheck(image, value.heap) == ClockResult::Ok;
        }
        ClockResult Capture(std::uintptr_t image, Kind kind, const Geometry& geometry, CheckOwner owner, CheckOwner after, Snapshot& output)
        {
            auto result = Owner(image, owner, after);
            if (result != ClockResult::Ok)
                return result;
            if (!Profile(image, kind))
                return ClockResult::UnsupportedBuild;
            Snapshot next;
            if ((result = UsageD::Capture(image, next.world)) != ClockResult::Ok || (result = UsageH::Capture(image, next.heap)) != ClockResult::Ok)
                return result;
            if (!UsageI::ReadImage(image, image + geometry.root, &next.address, sizeof(next.address), true))
                return ClockResult::ReadFailure;
            if (!next.address)
                return ClockResult::NotInitialized;
            if (!UsageD::Writable(next.address, next.header.size()) || !UsageD::Copy(next.address, next.header.data(), next.header.size()))
                return ClockResult::ReadFailure;
            next.objects = Field<std::uintptr_t>(next.header, 0);
            next.flags = Field<std::uintptr_t>(next.header, 8);
            const auto capacity = Field<std::int32_t>(next.header, 0x10);
            const auto cursor = Field<std::int32_t>(next.header, 0x14);
            // Only the currently proved allocation geometries are admitted.
            // A future resize must qualify its publication before these SDK
            // readers can accept a changed header as a different native pool.
            const auto objectBytes = static_cast<std::size_t>(geometry.capacity) * geometry.stride;
            if (capacity != static_cast<std::int32_t>(geometry.capacity) || (cursor != -1 && !MtaGame::CPoolSlotPolicy::ContainsIndex(capacity, cursor)) ||
                next.header[0x18] != 1 || !UsageD::Writable(next.objects, objectBytes) || !UsageD::Writable(next.flags, geometry.capacity) ||
                !NativeModelReferenceMap::Disjoint(next.address, next.header.size(), next.objects, objectBytes) ||
                !NativeModelReferenceMap::Disjoint(next.address, next.header.size(), next.flags, geometry.capacity) ||
                !NativeModelReferenceMap::Disjoint(next.objects, objectBytes, next.flags, geometry.capacity))
                return ClockResult::BindingChanged;
            next.states.resize(geometry.capacity);
            if (!UsageD::Copy(next.flags, next.states.data(), next.states.size()))
                return ClockResult::ReadFailure;
            next.usage.capacity = geometry.capacity;
            for (const auto state : next.states)
                if (!(state & NativePedPoolProfile::EmptyBit))
                    ++next.usage.used;
            if ((result = Owner(image, owner, after)) != ClockResult::Ok)
                return result;
            if (!Seal(image, kind, geometry, next))
                return ClockResult::BindingChanged;
            output = std::move(next);
            return ClockResult::Ok;
        }
    }
    ClockResult Read(std::uintptr_t image, std::uint64_t generation, Kind kind, CheckOwner owner, CheckOwner after, Value& output) noexcept
    {
        Geometry geometry;
        if (!Select(kind, geometry) || !image || !generation || !owner || !after)
            return ClockResult::InvalidArgument;
        if (image > (std::numeric_limits<std::uintptr_t>::max)() - NativePedPoolProfile::ImageSize)
            return ClockResult::UnsupportedBuild;
        if (!UsageD::TryEnter())
            return ClockResult::Busy;
        UsageD::Scope scope;
        try
        {
            Snapshot first, current, confirmed;
            auto     result = Capture(image, kind, geometry, owner, after, first);
            if (result != ClockResult::Ok || (result = Capture(image, kind, geometry, owner, after, current)) != ClockResult::Ok ||
                (result = Capture(image, kind, geometry, owner, after, confirmed)) != ClockResult::Ok)
                return result;
            if (!Same(first, current) || !Same(current, confirmed))
                return ClockResult::BindingChanged;
            if ((result = Owner(image, owner, after)) != ClockResult::Ok)
                return result;
            // No caller callback or native entry occurs after this final seal.
            // Publish only plain counts; native allocation identity stays on
            // this stack and retires with the existing callback borrow.
            if (!Seal(image, kind, geometry, confirmed))
                return ClockResult::BindingChanged;
            output = confirmed.usage;
            return ClockResult::Ok;
        }
        catch (...)
        {
            return ClockResult::ReadFailure;
        }
    }
}
