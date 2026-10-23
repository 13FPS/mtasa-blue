/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Admit genuine engine-owned sector scans and native matrix recycling
 ****************************************************************************/
#include "NativeWorldGroundContext.h"
#include "NativeWorldGroundProfile.h"
#include "NativeImageAccess.h"
#include <cstring>
#include <limits>
namespace mta::game_de::NativeWorldGroundContext
{
    namespace P = NativeWorldGroundProfile;
    namespace D = NativeStreamingCompletionContext;
    namespace Image = NativeImageAccess;
    namespace
    {
        bool Sentinel(std::uintptr_t image, std::uintptr_t node) noexcept
        {
            return node == image + P::ActiveStartRva || node == image + P::ActiveEndRva || node == image + P::FreeStartRva || node == image + P::FreeEndRva;
        }
        bool WritableNative(std::uintptr_t address, std::size_t bytes) noexcept
        {
            if (!address || address % alignof(std::uintptr_t) || !bytes || bytes > (std::numeric_limits<std::uintptr_t>::max)() - address)
                return false;
            const auto     end = address + bytes;
            std::uintptr_t allocation = 0;
            while (address < end)
            {
                MEMORY_BASIC_INFORMATION info{};
                if (VirtualQuery(reinterpret_cast<void*>(address), &info, sizeof(info)) != sizeof(info) || info.State != MEM_COMMIT ||
                    (info.Type != MEM_PRIVATE && info.Type != MEM_MAPPED) || (info.Protect != PAGE_READWRITE && info.Protect != PAGE_WRITECOPY))
                    return false;
                const auto owner = reinterpret_cast<std::uintptr_t>(info.AllocationBase);
                const auto begin = reinterpret_cast<std::uintptr_t>(info.BaseAddress);
                if (!owner || (allocation && allocation != owner) || begin > address || !info.RegionSize ||
                    info.RegionSize > (std::numeric_limits<std::uintptr_t>::max)() - begin || begin + info.RegionSize <= address)
                    return false;
                allocation = owner;
                address = begin + info.RegionSize < end ? begin + info.RegionSize : end;
            }
            return true;
        }
        bool NativeNode(std::uintptr_t image, std::uintptr_t node) noexcept
        {
            return node && !Sentinel(image, node) && WritableNative(node, P::NodeBytes);
        }
        bool Observe(Value& next, std::uintptr_t address, std::uintptr_t& value) noexcept
        {
            if (next.count == next.roots.size() || !D::Copy(address, &value, sizeof(value)))
                return false;
            next.roots[next.count++] = {address, value};
            return true;
        }
        bool List(std::uintptr_t image, std::uintptr_t start, std::uintptr_t end, Value& next, std::uintptr_t& first, std::uintptr_t& last) noexcept
        {
            if (!Observe(next, start + P::NextOffset, first) || !Observe(next, end + P::PrevOffset, last))
                return false;
            if (first == end)
                return last == start;
            if (!NativeNode(image, first) || !NativeNode(image, last))
                return false;
            std::uintptr_t firstPrev = 0, firstNext = 0, nextPrev = 0, lastNext = 0, lastPrev = 0, prevNext = 0;
            if (!Observe(next, first + P::PrevOffset, firstPrev) || firstPrev != start || !Observe(next, first + P::NextOffset, firstNext) ||
                (firstNext != end && !NativeNode(image, firstNext)) || !Observe(next, firstNext + P::PrevOffset, nextPrev) || nextPrev != first ||
                !Observe(next, last + P::NextOffset, lastNext) || lastNext != end || !Observe(next, last + P::PrevOffset, lastPrev) ||
                (lastPrev != start && !NativeNode(image, lastPrev)) || !Observe(next, lastPrev + P::NextOffset, prevNext) || prevNext != last)
                return false;
            return true;
        }
    }
    bool Profile(std::uintptr_t image) noexcept
    {
        for (const auto& pin : P::Pins)
        {
            std::array<unsigned char, P::MaximumPinBytes> bytes{};
            if (!pin.size || pin.size > bytes.size() || !Image::ReadImage(image, image + pin.rva, bytes.data(), pin.size, false, pin.code) ||
                std::memcmp(bytes.data(), pin.bytes, pin.size))
                return false;
        }
        return D::Profile(image);
    }
    ClockResult Capture(std::uintptr_t image, Value& output) noexcept
    {
        Value next;
        auto  result = D::Capture(image, next.graph);
        if (result != ClockResult::Ok)
            return result;
        // Sector heads have static image lifetime. A real empty sector is
        // valid; never invent a nonempty-world heuristic or enumerate every
        // engine-owned entity into the adapter's ped registration registry.
        if (!Image::ImagePages(image, image + P::SectorsRva, P::SectorBytes, true) ||
            !Image::ImagePages(image, image + P::HitScratchRva, sizeof(std::uint32_t), true) ||
            !Image::ImagePages(image, image + P::IgnoredEntityRva, sizeof(std::uintptr_t), true) ||
            !Image::ImagePages(image, image + P::ActiveStartRva, 2 * P::NodeBytes, true) ||
            !Image::ImagePages(image, image + P::FreeStartRva, 2 * P::NodeBytes, true) ||
            !Image::ReadImage(image, image + P::ScanRva, &next.scan, sizeof(next.scan), true))
            return ClockResult::BindingChanged;
        std::uintptr_t ignored = 0, freeFirst = 0, freeLast = 0, activeFirst = 0, activeLast = 0;
        if (!Observe(next, image + P::IgnoredEntityRva, ignored) || !List(image, image + P::FreeStartRva, image + P::FreeEndRva, next, freeFirst, freeLast) ||
            !List(image, image + P::ActiveStartRva, image + P::ActiveEndRva, next, activeFirst, activeLast))
            return ClockResult::BindingChanged;
        // The exact allocator consumes an existing free matrix link. When
        // empty it retires the native active tail's owning entity and retries
        // once. Coherent real sentinels establish this current prerequisite;
        // no heap allocator call, guessed capacity or adapter entity lease is
        // substituted for the engine's own allocation/recycling policy.
        if (freeFirst == image + P::FreeEndRva)
        {
            if (activeFirst == image + P::ActiveEndRva)
                return ClockResult::NotInitialized;
            std::uintptr_t owner = 0, matrix = 0;
            if (!Observe(next, activeLast + P::OwnerOffset, owner) || !WritableNative(owner, 0x20) || !Observe(next, owner + 0x18, matrix) ||
                matrix != activeLast)
                return ClockResult::BindingChanged;
        }
        if (!D::Fence(image, next.graph))
            return ClockResult::BindingChanged;
        output = next;
        return ClockResult::Ok;
    }
    bool SameBefore(const Value& first, const Value& next) noexcept
    {
        if (first.scan != next.scan || first.count != next.count || !D::Same(first.graph, next.graph, true))
            return false;
        for (std::size_t i = 0; i != first.count; ++i)
            if (first.roots[i].address != next.roots[i].address || first.roots[i].value != next.roots[i].value)
                return false;
        return true;
    }
    bool SameWorld(const Value& first, const Value& next) noexcept
    {
        return D::Same(first.graph, next.graph, false);
    }
}
