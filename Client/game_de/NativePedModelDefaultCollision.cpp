/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Qualify only the canonical completed ped-default collision borrow
 ****************************************************************************/
#include "NativePedModelDefaultCollision.h"
#include "NativePedModelMetadataProfile.h"
#include "NativeImageAccess.h"
#include "NativeStartGameCalls.h"
#include "NativeGameStateMemory.h"
#include <algorithm>
#include <cstring>
#include <limits>

namespace mta::game_de::NativePedModelDefaultCollision
{
    namespace Detail
    {
        namespace P = NativePedModelMetadataProfile;
        namespace Image = NativeImageAccess;
        bool Range(std::uintptr_t address, std::size_t bytes) noexcept
        {
            return address && bytes && bytes <= (std::numeric_limits<std::uintptr_t>::max)() - address;
        }
        bool Disjoint(std::uintptr_t a, std::size_t aBytes, std::uintptr_t b, std::size_t bBytes) noexcept
        {
            return Range(a, aBytes) && Range(b, bBytes) && (a + aBytes <= b || b + bBytes <= a);
        }
        ClockResult Owner(std::uintptr_t image, CheckOwner owner) noexcept
        {
            if (!owner)
                return ClockResult::NotInitialized;
            auto result = owner();
            if (result != ClockResult::Ok || (result = NativeStartGameCalls::CheckThread(image)) != ClockResult::Ok)
                return result;
            std::uint32_t state = 0;
            if (!NativeGameStateMemory::Read(image, state))
                return ClockResult::BindingChanged;
            return state == 9 ? owner() : ClockResult::NotInitialized;
        }
        bool ImageProtection(std::uintptr_t image, std::uintptr_t address, std::size_t bytes, DWORD protection) noexcept
        {
            if (!Range(address, bytes))
                return false;
            const auto end = address + bytes;
            while (address < end)
            {
                MEMORY_BASIC_INFORMATION page{};
                if (VirtualQuery(reinterpret_cast<const void*>(address), &page, sizeof(page)) != sizeof(page) || page.State != MEM_COMMIT ||
                    page.Type != MEM_IMAGE || reinterpret_cast<std::uintptr_t>(page.AllocationBase) != image || (page.Protect & (PAGE_GUARD | PAGE_NOACCESS)) ||
                    (page.Protect & 0xff) != protection)
                    return false;
                const auto base = reinterpret_cast<std::uintptr_t>(page.BaseAddress);
                if (base > address || !page.RegionSize || page.RegionSize > (std::numeric_limits<std::uintptr_t>::max)() - base ||
                    base + page.RegionSize <= address)
                    return false;
                address = (std::min)(end, base + page.RegionSize);
            }
            return true;
        }
        bool Profile(std::uintptr_t image) noexcept
        {
            if (!NativeModelBankReader::Revalidate({image}))
                return false;
            for (const auto& pin : P::Code)
            {
                std::array<unsigned char, P::MaximumPinBytes> bytes{};
                if (!pin.size || pin.size > bytes.size() || pin.rva > P::ImageSize || pin.size > P::ImageSize - pin.rva ||
                    !ImageProtection(image, image + pin.rva, pin.size, PAGE_EXECUTE_READ) ||
                    !Image::ReadImage(image, image + pin.rva, bytes.data(), pin.size, false, true) || std::memcmp(bytes.data(), pin.bytes, pin.size))
                    return false;
            }
            return true;
        }
        bool PrivatePages(std::uintptr_t image, std::uintptr_t address, std::size_t bytes, std::uintptr_t& allocation) noexcept
        {
            if (!Range(address, bytes) || !Disjoint(address, bytes, image, P::ImageSize))
                return false;
            const auto     end = address + bytes;
            std::uintptr_t firstAllocation = 0;
            while (address < end)
            {
                MEMORY_BASIC_INFORMATION page{};
                if (VirtualQuery(reinterpret_cast<const void*>(address), &page, sizeof(page)) != sizeof(page) || page.State != MEM_COMMIT ||
                    page.Type != MEM_PRIVATE || (page.Protect & (PAGE_GUARD | PAGE_NOACCESS)))
                    return false;
                const auto protection = page.Protect & 0xff;
                if (protection != PAGE_READONLY && protection != PAGE_READWRITE && protection != PAGE_WRITECOPY)
                    return false;
                const auto base = reinterpret_cast<std::uintptr_t>(page.BaseAddress);
                const auto current = reinterpret_cast<std::uintptr_t>(page.AllocationBase);
                if (!current || (firstAllocation && current != firstAllocation) || base > address || !page.RegionSize ||
                    page.RegionSize > (std::numeric_limits<std::uintptr_t>::max)() - base || base + page.RegionSize <= address)
                    return false;
                firstAllocation = current;
                address = (std::min)(end, base + page.RegionSize);
            }
            allocation = firstAllocation;
            return true;
        }
        template <std::size_t N>
        bool PrivateRead(std::uintptr_t image, std::uintptr_t address, std::array<unsigned char, N>& output, std::uintptr_t& allocation) noexcept
        {
            SIZE_T copied = 0;
            return PrivatePages(image, address, output.size(), allocation) &&
                   ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<const void*>(address), output.data(), output.size(), &copied) &&
                   copied == output.size();
        }
        template <typename T, std::size_t N>
        T At(const std::array<unsigned char, N>& bytes, std::size_t offset) noexcept
        {
            T value{};
            std::memcpy(&value, bytes.data() + offset, sizeof(value));
            return value;
        }
        bool Outer(const Value& value) noexcept
        {
            return !std::memcmp(value.canonicalBytes.data(), value.boundsLiteral.data(), value.boundsLiteral.size()) &&
                   !std::memcmp(value.canonicalBytes.data() + 0x10, value.maxYLiteral.data(), value.maxYLiteral.size()) &&
                   At<std::uint32_t>(value.canonicalBytes, 0x14) == 0x3f733333 && At<std::uint64_t>(value.canonicalBytes, 0x18) == 0 &&
                   At<std::uint32_t>(value.canonicalBytes, 0x20) == 0 && At<std::uint32_t>(value.canonicalBytes, 0x24) == 0x3f800000 &&
                   value.canonicalBytes[0x28] == 0;
        }
        bool Records(const Value& value) noexcept
        {
            if (value.dataBytes[6] != 0)
                return false;
            constexpr std::array<std::uint32_t, 3> z = {0xbe4ccccc, 0x3e4cccd0, 0x3f19999a};
            for (std::size_t index = 0; index != z.size(); ++index)
            {
                const auto at = index * 0x14;
                if (At<std::uint64_t>(value.sphereBytes, at) != 0 || At<std::uint32_t>(value.sphereBytes, at + 8) != z[index] ||
                    At<std::uint32_t>(value.sphereBytes, at + 0xc) != 0x3eb33333 ||
                    At<std::uint16_t>(value.sphereBytes, at + 0x10) != static_cast<std::uint16_t>(0x3e + index * 0x100) || value.sphereBytes[at + 0x12] != 0xff)
                    return false;
            }
            for (std::size_t index = 0; index != 2; ++index)
            {
                const auto at = index * 0x20;
                if (At<std::uint64_t>(value.segmentBytes, at) != 0 || At<std::uint32_t>(value.segmentBytes, at + 8) != 0 ||
                    At<std::uint64_t>(value.segmentBytes, at + 0x10) != 0 ||
                    At<std::uint32_t>(value.segmentBytes, at + 0x18) != (index ? 0x3f800000u : 0xbf800000u))
                    return false;
            }
            return true;
        }
        ClockResult Capture(std::uintptr_t image, Value& value) noexcept
        {
            value.canonical = image + P::DefaultCollisionRva;
            if (!ImageProtection(image, image + P::BoundsLiteralRva, value.boundsLiteral.size(), PAGE_READONLY) ||
                !ImageProtection(image, image + P::MaxYLiteralRva, value.maxYLiteral.size(), PAGE_READONLY) ||
                !Image::ReadImage(image, image + P::BoundsLiteralRva, value.boundsLiteral.data(), value.boundsLiteral.size()) ||
                !Image::ReadImage(image, image + P::MaxYLiteralRva, value.maxYLiteral.data(), value.maxYLiteral.size()) ||
                !Image::ReadImage(image, value.canonical, value.canonicalBytes.data(), value.canonicalBytes.size(), true))
                return ClockResult::ReadFailure;
            value.data = At<std::uintptr_t>(value.canonicalBytes, 0x30);
            if (!value.data || !Outer(value))
                return ClockResult::BindingChanged;
            if (!PrivateRead(image, value.data, value.dataBytes, value.dataAllocation))
                return ClockResult::ReadFailure;
            value.spheres = At<std::uintptr_t>(value.dataBytes, 8);
            value.segments = At<std::uintptr_t>(value.dataBytes, 0x18);
            if (!Disjoint(value.data, value.dataBytes.size(), value.spheres, value.sphereBytes.size()) ||
                !Disjoint(value.data, value.dataBytes.size(), value.segments, value.segmentBytes.size()) ||
                !Disjoint(value.spheres, value.sphereBytes.size(), value.segments, value.segmentBytes.size()))
                return ClockResult::BindingChanged;
            if (!PrivateRead(image, value.spheres, value.sphereBytes, value.sphereAllocation) ||
                !PrivateRead(image, value.segments, value.segmentBytes, value.segmentAllocation))
                return ClockResult::ReadFailure;
            return Records(value) ? ClockResult::Ok : ClockResult::BindingChanged;
        }
    }
    bool Same(const Value& a, const Value& b) noexcept
    {
        // Unwritten padding and header bytes have no invented expected value;
        // retaining them only in the bookend detects change during this borrow.
        return a.canonical == b.canonical && a.data == b.data && a.spheres == b.spheres && a.segments == b.segments && a.dataAllocation == b.dataAllocation &&
               a.sphereAllocation == b.sphereAllocation && a.segmentAllocation == b.segmentAllocation && a.canonicalBytes == b.canonicalBytes &&
               a.dataBytes == b.dataBytes && a.sphereBytes == b.sphereBytes && a.segmentBytes == b.segmentBytes && a.boundsLiteral == b.boundsLiteral &&
               a.maxYLiteral == b.maxYLiteral;
    }
    ClockResult Read(std::uintptr_t image, CheckOwner owner, Value& output) noexcept
    {
        namespace D = Detail;
        if (!image || !owner)
            return ClockResult::NotInitialized;
        if (!D::Range(image, NativePedModelMetadataProfile::ImageSize))
            return ClockResult::BindingChanged;
        auto result = D::Owner(image, owner);
        if (result != ClockResult::Ok)
            return result;
        if (!D::Profile(image))
            return ClockResult::BindingChanged;
        Value first, second;
        if ((result = D::Capture(image, first)) != ClockResult::Ok || (result = D::Owner(image, owner)) != ClockResult::Ok ||
            (result = D::Capture(image, second)) != ClockResult::Ok)
            return result;
        if (!Same(first, second) || !D::Profile(image))
            return ClockResult::BindingChanged;
        if ((result = D::Owner(image, owner)) != ClockResult::Ok)
            return result;
        output = second;
        return ClockResult::Ok;
    }
    ClockResult Recheck(std::uintptr_t image, CheckOwner owner, const Value& previous) noexcept
    {
        Value      next;
        const auto result = Read(image, owner, next);
        return result != ClockResult::Ok ? result : Same(previous, next) ? ClockResult::Ok : ClockResult::BindingChanged;
    }
}
