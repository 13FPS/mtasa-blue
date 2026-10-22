/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Admit the current constructor-published native heap, never create it
 ****************************************************************************/
#pragma once
#include "NativeModelReferenceTransactionProfile.h"
#include "NativeModelReferenceMap.h"
namespace mta::game_de::NativeModelReferenceHeap
{
    namespace P = NativeModelReferenceTransactionProfile;
    namespace M = NativeModelReferenceMap;
    namespace Image = NativeImageAccess;
    struct Value
    {
        std::uintptr_t current = 0, authority = 0, currentVtable = 0, authorityVtable = 0, inner = 0;
    };
    inline bool ReadOnlyImage(std::uintptr_t image, std::uintptr_t address, std::size_t size) noexcept
    {
        if (!Image::ImagePages(image, address, size))
            return false;
        const auto end = address + size;
        while (address < end)
        {
            MEMORY_BASIC_INFORMATION page{};
            if (VirtualQuery(reinterpret_cast<void*>(address), &page, sizeof(page)) != sizeof(page) || page.Protect != PAGE_READONLY)
                return false;
            const auto begin = reinterpret_cast<std::uintptr_t>(page.BaseAddress);
            if (begin > address || page.RegionSize > (std::numeric_limits<std::uintptr_t>::max)() - begin || begin + page.RegionSize <= address)
                return false;
            address = (std::min)(end, begin + page.RegionSize);
        }
        return true;
    }
    inline bool QualifyProfile(std::uintptr_t image) noexcept
    {
        std::array<unsigned char, P::MaximumPinBytes> bytes{};
        for (const auto& pin : P::Code)
            if (!pin.size || pin.size > bytes.size() || !Image::ReadImage(image, image + pin.rva, bytes.data(), pin.size, false, true) ||
                std::memcmp(bytes.data(), pin.bytes, pin.size))
                return false;
        std::array<std::uint16_t, P::ClassCount> classes{};
        return ReadOnlyImage(image, image + P::ClassSizesRva, sizeof(classes)) &&
               Image::ReadImage(image, image + P::ClassSizesRva, classes.data(), sizeof(classes)) && classes == P::ClassSizes && M::QualifyProfile(image);
    }
    inline bool HeapSlots(std::uintptr_t image, std::uintptr_t table, std::uintptr_t reallocate, std::uintptr_t quantize) noexcept
    {
        std::uintptr_t a = 0, b = 0;
        return ReadOnlyImage(image, table, 0x40) && Image::ReadImage(image, table + 0x20, &a, sizeof(a)) &&
               Image::ReadImage(image, table + 0x38, &b, sizeof(b)) && a == image + reallocate && b == image + quantize &&
               Image::ImagePages(image, a, 1, false, true) && Image::ImagePages(image, b, 1, false, true);
    }
    inline ClockResult Capture(std::uintptr_t image, Value& output) noexcept
    {
        Value value;
        if (!Image::ReadImage(image, image + P::HeapRva, &value.current, sizeof(value.current), true) ||
            !Image::ReadImage(image, image + P::HeapAuthorityRva, &value.authority, sizeof(value.authority), true))
            return ClockResult::ReadFailure;
        if (!value.current || !value.authority)
            return ClockResult::NotInitialized;
        if (value.current % alignof(std::uintptr_t) || value.authority % alignof(std::uintptr_t) || !M::PrivateWritable(value.authority, P::HeapBytes) ||
            !M::Copy(value.authority, &value.authorityVtable, sizeof(value.authorityVtable)) || value.authorityVtable != image + P::HeapVtableRva ||
            !HeapSlots(image, value.authorityVtable, P::HeapReallocateRva, P::HeapQuantizeRva))
            return ClockResult::BindingChanged;
        if (value.current == value.authority)
            value.currentVtable = value.authorityVtable;
        else
        {
            if (!M::PrivateWritable(value.current, P::ProxyBytes) || !M::Disjoint(value.current, P::ProxyBytes, value.authority, P::HeapBytes) ||
                !M::Copy(value.current, &value.currentVtable, sizeof(value.currentVtable)) || value.currentVtable != image + P::ProxyVtableRva ||
                !M::Copy(value.current + 8, &value.inner, sizeof(value.inner)) || value.inner != value.authority ||
                !HeapSlots(image, value.currentVtable, P::ProxyReallocateRva, P::ProxyQuantizeRva))
                return ClockResult::BindingChanged;
        }
        // The constructor fills both tables before publishing authority. Their
        // exact native source ordering distinguishes completed initialization
        // from an object with only a matching vtable word.
        std::array<unsigned char, P::LookupBytes> lookup{};
        std::array<std::uint16_t, P::ClassCount>  reverse{};
        if (!Image::ReadImage(image, image + P::LookupRva, lookup.data(), lookup.size(), true) ||
            !Image::ReadImage(image, image + P::ReverseSizesRva, reverse.data(), sizeof(reverse), true))
            return ClockResult::ReadFailure;
        std::size_t category = 0;
        for (std::size_t index = 0; index != lookup.size(); ++index)
        {
            while (category < P::ClassSizes.size() && P::ClassSizes[category] < index * 16)
                ++category;
            if (category == P::ClassSizes.size() || lookup[index] != static_cast<unsigned char>(category))
                return ClockResult::BindingChanged;
        }
        for (std::size_t index = 0; index != reverse.size(); ++index)
            if (reverse[index] != P::ClassSizes[P::ClassSizes.size() - index - 1])
                return ClockResult::BindingChanged;
        output = value;
        return ClockResult::Ok;
    }
    inline bool Same(const Value& a, const Value& b) noexcept
    {
        return a.current == b.current && a.authority == b.authority && a.currentVtable == b.currentVtable && a.authorityVtable == b.authorityVtable &&
               a.inner == b.inner;
    }
    inline ClockResult Recheck(std::uintptr_t image, const Value& previous) noexcept
    {
        Value      current;
        const auto result = Capture(image, current);
        return result != ClockResult::Ok ? result : Same(previous, current) ? ClockResult::Ok : ClockResult::BindingChanged;
    }
}
