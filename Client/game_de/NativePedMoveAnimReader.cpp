/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Bookend the real registered ped and preserve its complete raw group
 ****************************************************************************/
#include "NativePedMoveAnimReader.h"
#include "NativePedMoveAnimProfile.h"
#include "NativePedMetadataMemory.h"
#include "NativeImageAccess.h"
#include <array>
#include <cstring>
#include <limits>
namespace mta::game_de
{
    namespace PedMoveAnimDetail
    {
        namespace P = NativePedMoveAnimProfile;
        using Reader = NativePedMoveAnimReader;
        bool Pages(std::uintptr_t at) noexcept
        {
            if (!at || at % alignof(std::int32_t) || at > (std::numeric_limits<std::uintptr_t>::max)() - sizeof(std::int32_t))
                return false;
            MEMORY_BASIC_INFORMATION page{};
            if (VirtualQuery(reinterpret_cast<void*>(at), &page, sizeof(page)) != sizeof(page) || page.State != MEM_COMMIT ||
                (page.Type != MEM_PRIVATE && page.Type != MEM_MAPPED) || (page.Protect & (PAGE_GUARD | PAGE_NOACCESS)) ||
                (page.Protect & 0xff) != PAGE_READWRITE)
                return false;
            const auto begin = reinterpret_cast<std::uintptr_t>(page.BaseAddress);
            return at >= begin && page.RegionSize >= sizeof(std::int32_t) && at - begin <= page.RegionSize - sizeof(std::int32_t);
        }
        ClockResult Identity(Reader::Binding binding, Reader::CheckOwner owner, const Reader::Value& expected) noexcept
        {
            if (!owner)
                return ClockResult::NotInitialized;
            if (!Reader::Revalidate(binding))
                return ClockResult::BindingChanged;
            // Reuse the actual occupied-ped metadata identity, which already
            // checks native thread/GC/state and the before/after pool handle.
            // No global local-player substitution or model lookup is needed.
            NativePedMetadata metadata;
            const auto        result = NativePedMetadataMemory(binding, owner).Read(expected, metadata);
            if (result != ClockResult::Ok)
                return result;
            return owner();
        }
        ClockResult Capture(Reader::Binding binding, Reader::CheckOwner owner, const Reader::Value& expected, std::int32_t& output) noexcept
        {
            auto result = Identity(binding, owner, expected);
            if (result != ClockResult::Ok)
                return result;
            if (expected.nativePed > (std::numeric_limits<std::uintptr_t>::max)() - P::GroupOffset)
                return ClockResult::ReadFailure;
            const auto   address = expected.nativePed + P::GroupOffset;
            std::int32_t next = 0;
            SIZE_T       copied = 0;
            if (!Pages(address) || !ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(address), &next, sizeof(next), &copied) ||
                copied != sizeof(next))
                return ClockResult::ReadFailure;
            if ((result = Identity(binding, owner, expected)) != ClockResult::Ok)
                return result;
            output = next;
            return ClockResult::Ok;
        }
    }
    bool NativePedMoveAnimReader::Qualify(Binding& output) noexcept
    {
        Binding next;
        if (!NativePedMetadataMemory::Qualify(next) || !Revalidate(next))
            return false;
        output = next;
        return true;
    }
    bool NativePedMoveAnimReader::Revalidate(const Binding& binding) noexcept
    {
        namespace P = NativePedMoveAnimProfile;
        namespace I = NativeImageAccess;
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
        if (nt.FileHeader.NumberOfSections > sections.size() || at > NativePedPoolProfile::ImageSize || bytes > NativePedPoolProfile::ImageSize - at ||
            !I::ReadImage(binding.image, binding.image + at, sections.data(), bytes))
            return false;
        const auto contains = [&](std::uintptr_t rva, std::size_t count, DWORD required, DWORD forbidden)
        {
            for (std::size_t i = 0; i < nt.FileHeader.NumberOfSections; ++i)
                if (I::SectionContains(sections[i], rva, count, required, forbidden))
                    return true;
            return false;
        };
        for (const auto& pin : P::Code)
        {
            std::array<unsigned char, P::MaximumPinBytes> actual{};
            if (!pin.size || pin.size > actual.size() || !contains(pin.rva, pin.size, IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_EXECUTE, IMAGE_SCN_MEM_WRITE) ||
                !I::ReadImage(binding.image, binding.image + pin.rva, actual.data(), pin.size, false, true) || std::memcmp(actual.data(), pin.bytes, pin.size))
                return false;
        }
        std::uintptr_t selectedGroup = 0;
        if (!contains(P::GroupSlotRva, sizeof(selectedGroup), IMAGE_SCN_MEM_READ, IMAGE_SCN_MEM_WRITE | IMAGE_SCN_MEM_EXECUTE) ||
            !I::ReadImage(binding.image, binding.image + P::GroupSlotRva, &selectedGroup, sizeof(selectedGroup)) ||
            selectedGroup != binding.image + P::GroupRva)
            return false;
        return true;
    }
    ClockResult NativePedMoveAnimReader::Read(const Value& selected, std::int32_t& output) const noexcept
    {
        namespace D = PedMoveAnimDetail;
        // Owner callbacks may run while observing native state. Preserve the
        // caller's exact selected identity instead of borrowing mutable input.
        const auto   expected = selected;
        std::int32_t first = 0, second = 0;
        auto         result = D::Capture(binding, owner, expected, first);
        if (result != ClockResult::Ok || (result = D::Capture(binding, owner, expected, second)) != ClockResult::Ok)
            return result;
        if (first != second)
            return ClockResult::OwnerConflict;
        if ((result = D::Identity(binding, owner, expected)) != ClockResult::Ok)
            return result;
        output = second;
        return ClockResult::Ok;
    }
}
