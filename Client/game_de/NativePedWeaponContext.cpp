/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Reacquire the same occupied ped and named inline weapon slot
 ****************************************************************************/
#include "NativePedWeaponContext.h"
#include "NativePedWeaponProfile.h"
#include "NativePedMetadataMemory.h"
#include "NativeImageAccess.h"
#include <array>
#include <algorithm>
#include <cstring>
#include <limits>
namespace mta::game_de
{
    static_assert(NativePedWeaponProfile::ArrayOffset + NativePedWeaponProfile::Count * NativePedWeaponProfile::Stride <= NativePedPoolProfile::ObjectStride);
    namespace PedWeaponContextDetail
    {
        thread_local bool writing = false;
        ClockResult       Identity(NativePedWeaponContext::Binding binding, NativePedWeaponContext::CheckOwner owner,
                                   const NativePedWeaponContext::Ped& expected) noexcept
        {
            if (!owner)
                return ClockResult::NotInitialized;
            if (!NativePedWeaponContext::Revalidate(binding))
                return ClockResult::BindingChanged;
            NativePedMetadata metadata;
            const auto        result = NativePedMetadataMemory(binding, owner).Read(expected, metadata);
            if (result != ClockResult::Ok)
                return result;
            if (metadata.entityType != 3)
                return ClockResult::BindingChanged;
            return owner();
        }
    }
    NativePedWeaponContext::Mutation::Mutation() noexcept
    {
        acquired = !PedWeaponContextDetail::writing;
        if (acquired)
            PedWeaponContextDetail::writing = true;
    }
    NativePedWeaponContext::Mutation::~Mutation()
    {
        if (acquired)
            PedWeaponContextDetail::writing = false;
    }
    bool NativePedWeaponContext::Writable(std::uintptr_t at, std::size_t bytes) noexcept
    {
        if (!at || !bytes || at > (std::numeric_limits<std::uintptr_t>::max)() - bytes)
            return false;
        const auto end = at + bytes;
        while (at < end)
        {
            MEMORY_BASIC_INFORMATION page{};
            if (VirtualQuery(reinterpret_cast<void*>(at), &page, sizeof(page)) != sizeof(page) || page.State != MEM_COMMIT ||
                (page.Type != MEM_PRIVATE && page.Type != MEM_MAPPED) || (page.Protect & (PAGE_GUARD | PAGE_NOACCESS)) ||
                (page.Protect & 0xff) != PAGE_READWRITE)
                return false;
            const auto begin = reinterpret_cast<std::uintptr_t>(page.BaseAddress);
            if (at < begin || !page.RegionSize || begin > (std::numeric_limits<std::uintptr_t>::max)() - page.RegionSize || at >= begin + page.RegionSize)
                return false;
            at = (std::min)(end, begin + page.RegionSize);
        }
        return true;
    }
    bool NativePedWeaponContext::Qualify(Binding& output) noexcept
    {
        Binding next;
        if (!NativePedMetadataMemory::Qualify(next) || !Revalidate(next))
            return false;
        output = next;
        return true;
    }
    bool NativePedWeaponContext::Revalidate(const Binding& binding) noexcept
    {
        namespace I = NativeImageAccess;
        namespace P = NativePedWeaponProfile;
        if (!NativePedMetadataMemory::Revalidate(binding))
            return false;
        IMAGE_DOS_HEADER   dos{};
        IMAGE_NT_HEADERS64 nt{};
        if (!I::ReadImage(binding.image, binding.image, &dos, sizeof(dos)) || dos.e_magic != IMAGE_DOS_SIGNATURE || dos.e_lfanew <= 0 ||
            static_cast<std::size_t>(dos.e_lfanew) > NativePedPoolProfile::ImageSize - sizeof(nt) ||
            !I::ReadImage(binding.image, binding.image + static_cast<std::uintptr_t>(dos.e_lfanew), &nt, sizeof(nt)) || nt.Signature != IMAGE_NT_SIGNATURE ||
            nt.FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64 || nt.OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC ||
            nt.OptionalHeader.SizeOfImage != NativePedPoolProfile::ImageSize || nt.FileHeader.SizeOfOptionalHeader < sizeof(IMAGE_OPTIONAL_HEADER64) ||
            !nt.FileHeader.NumberOfSections || nt.FileHeader.NumberOfSections > 96)
            return false;
        const auto at = static_cast<std::uintptr_t>(dos.e_lfanew) + offsetof(IMAGE_NT_HEADERS64, OptionalHeader) + nt.FileHeader.SizeOfOptionalHeader;
        const auto bytes = static_cast<std::size_t>(nt.FileHeader.NumberOfSections) * sizeof(IMAGE_SECTION_HEADER);
        std::array<IMAGE_SECTION_HEADER, 96> sections{};
        if (at > NativePedPoolProfile::ImageSize || bytes > NativePedPoolProfile::ImageSize - at ||
            !I::ReadImage(binding.image, binding.image + at, sections.data(), bytes))
            return false;
        for (const auto& pin : P::Code)
        {
            bool contains = false;
            for (std::size_t i = 0; i < nt.FileHeader.NumberOfSections; ++i)
                if (I::SectionContains(sections[i], pin.rva, pin.size, IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_EXECUTE, IMAGE_SCN_MEM_WRITE))
                    contains = true;
            std::array<unsigned char, P::MaximumPinBytes> actual{};
            if (!contains || !pin.size || pin.size > actual.size() ||
                !I::ReadImage(binding.image, binding.image + pin.rva, actual.data(), pin.size, false, true) || std::memcmp(actual.data(), pin.bytes, pin.size))
                return false;
        }
        return true;
    }
    ClockResult NativePedWeaponContext::Capture(const Ped& ped, std::uint32_t slot, Value& output) const noexcept
    {
        namespace P = NativePedWeaponProfile;
        // The independently unrolled named query establishes thirteen rows;
        // the current equipped slot is intentionally not a selector here.
        if (slot >= P::Count)
            return ClockResult::InvalidArgument;
        auto result = PedWeaponContextDetail::Identity(binding, owner, ped);
        if (result != ClockResult::Ok)
            return result;
        const auto offset = P::ArrayOffset + static_cast<std::size_t>(slot) * P::Stride;
        if (ped.nativePed > (std::numeric_limits<std::uintptr_t>::max)() - offset)
            return ClockResult::ReadFailure;
        const auto at = ped.nativePed + offset;
        if (at % alignof(std::uint32_t) || !Writable(at, P::Stride))
            return ClockResult::ReadFailure;
        std::uint32_t first = 0, second = 0;
        SIZE_T        copied = 0;
        if (!ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(at), &first, sizeof(first), &copied) || copied != sizeof(first))
            return ClockResult::ReadFailure;
        if ((result = PedWeaponContextDetail::Identity(binding, owner, ped)) != ClockResult::Ok)
            return result;
        if (!Writable(at, P::Stride) || !ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(at), &second, sizeof(second), &copied) ||
            copied != sizeof(second))
            return ClockResult::ReadFailure;
        if (first != second)
            return ClockResult::OwnerConflict;
        if ((result = PedWeaponContextDetail::Identity(binding, owner, ped)) != ClockResult::Ok)
            return result;
        Value next;
        next.ped = ped;
        next.slot = slot;
        next.rawType = second;
        next.nativeWeapon = at;
        output = next;
        return ClockResult::Ok;
    }
    ClockResult NativePedWeaponContext::Recheck(const Value& expected) const noexcept
    {
        return Recheck(expected, expected.rawType);
    }
    ClockResult NativePedWeaponContext::Recheck(const Value& expected, std::uint32_t expectedType) const noexcept
    {
        Value      next;
        const auto result = Capture(expected.ped, expected.slot, next);
        if (result != ClockResult::Ok)
            return result;
        if (next.nativeWeapon != expected.nativeWeapon || next.rawType != expectedType)
            return ClockResult::OwnerConflict;
        return ClockResult::Ok;
    }
}
