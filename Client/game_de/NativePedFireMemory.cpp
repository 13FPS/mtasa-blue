/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Read current ped fire-association presence without target dereference
 ****************************************************************************/
#include "NativePedFireMemory.h"
#include "NativePedFireProfile.h"
#include "NativePedMetadataMemory.h"
#include "NativeImageAccess.h"
#include <array>
#include <cstring>
#include <limits>
namespace mta::game_de
{
    namespace PedFireDetail
    {
        namespace P = NativePedFireProfile;
        using Memory = NativePedFireMemory;
        bool Pages(std::uintptr_t at) noexcept
        {
            if (!at || at % alignof(std::uintptr_t) || at > (std::numeric_limits<std::uintptr_t>::max)() - sizeof(std::uintptr_t))
                return false;
            MEMORY_BASIC_INFORMATION page{};
            if (VirtualQuery(reinterpret_cast<void*>(at), &page, sizeof(page)) != sizeof(page) || page.State != MEM_COMMIT ||
                (page.Type != MEM_PRIVATE && page.Type != MEM_MAPPED) || (page.Protect & (PAGE_GUARD | PAGE_NOACCESS)) ||
                (page.Protect & 0xff) != PAGE_READWRITE)
                return false;
            const auto begin = reinterpret_cast<std::uintptr_t>(page.BaseAddress);
            return at >= begin && page.RegionSize >= sizeof(std::uintptr_t) && at - begin <= page.RegionSize - sizeof(std::uintptr_t);
        }
        ClockResult Identity(Memory::Binding binding, Memory::CheckOwner owner, const Memory::Value& expected) noexcept
        {
            if (!owner)
                return ClockResult::NotInitialized;
            if (!Memory::Revalidate(binding))
                return ClockResult::BindingChanged;
            NativePedMetadata metadata;
            const auto        result = NativePedMetadataMemory(binding, owner).Read(expected, metadata);
            if (result != ClockResult::Ok)
                return result;
            return metadata.entityType == 3 ? owner() : ClockResult::BindingChanged;
        }
        ClockResult Capture(Memory::Binding binding, Memory::CheckOwner owner, const Memory::Value& expected, std::uintptr_t& output) noexcept
        {
            auto result = Identity(binding, owner, expected);
            if (result != ClockResult::Ok)
                return result;
            if (expected.nativePed > (std::numeric_limits<std::uintptr_t>::max)() - P::FireOffset)
                return ClockResult::ReadFailure;
            const auto     at = expected.nativePed + P::FireOffset;
            std::uintptr_t next = 0;
            SIZE_T         copied = 0;
            if (!Pages(at) || !ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(at), &next, sizeof(next), &copied) || copied != sizeof(next))
                return ClockResult::ReadFailure;
            if ((result = Identity(binding, owner, expected)) != ClockResult::Ok)
                return result;
            output = next;
            return ClockResult::Ok;
        }
    }
    bool NativePedFireMemory::Qualify(Binding& output) noexcept
    {
        Binding next;
        if (!NativePedMetadataMemory::Qualify(next) || !Revalidate(next))
            return false;
        output = next;
        return true;
    }
    bool NativePedFireMemory::Revalidate(const Binding& binding) noexcept
    {
        namespace D = PedFireDetail;
        namespace P = NativePedFireProfile;
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
        if (at > NativePedPoolProfile::ImageSize || bytes > NativePedPoolProfile::ImageSize - at ||
            !I::ReadImage(binding.image, binding.image + at, sections.data(), bytes))
            return false;
        const auto contains = [&](std::uintptr_t rva, std::size_t count, DWORD required, DWORD forbidden)
        {
            for (std::size_t i = 0; i < nt.FileHeader.NumberOfSections; ++i)
                if (I::SectionContains(sections[i], rva, count, required, forbidden))
                    return true;
            return false;
        };
        constexpr auto rx = IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_EXECUTE;
        for (const auto& pin : P::Code)
        {
            std::array<unsigned char, P::MaximumPinBytes> actual{};
            if (!pin.size || pin.size > actual.size() || !contains(pin.rva, pin.size, rx, IMAGE_SCN_MEM_WRITE) ||
                !I::ReadImage(binding.image, binding.image + pin.rva, actual.data(), pin.size, false, true) || std::memcmp(actual.data(), pin.bytes, pin.size))
                return false;
        }
        std::uintptr_t entry = 0;
        std::uint8_t   selector = 0;
        std::uint32_t  branch = 0;
        return contains(P::GroupSlotRva, 8, IMAGE_SCN_MEM_READ, IMAGE_SCN_MEM_WRITE | IMAGE_SCN_MEM_EXECUTE) &&
               contains(P::RemapRva + P::OpcodeIndex, 1, rx, IMAGE_SCN_MEM_WRITE) && contains(P::DispatchRva + P::Selector * 4, 4, rx, IMAGE_SCN_MEM_WRITE) &&
               I::ReadImage(binding.image, binding.image + P::GroupSlotRva, &entry, sizeof(entry)) && entry == binding.image + P::GroupRva &&
               I::ReadImage(binding.image, binding.image + P::RemapRva + P::OpcodeIndex, &selector, sizeof(selector), false, true) && selector == P::Selector &&
               I::ReadImage(binding.image, binding.image + P::DispatchRva + P::Selector * 4, &branch, sizeof(branch), false, true) && branch == P::BranchRva;
    }
    ClockResult NativePedFireMemory::ReadOnFire(const Value& expected, bool& output) const noexcept
    {
        namespace D = PedFireDetail;
        std::uintptr_t first = 0, second = 0;
        auto           result = D::Capture(binding, owner, expected, first);
        if (result != ClockResult::Ok || (result = D::Capture(binding, owner, expected, second)) != ClockResult::Ok)
            return result;
        if (first != second)
            return ClockResult::OwnerConflict;
        if ((result = D::Identity(binding, owner, expected)) != ClockResult::Ok)
            return result;
        // No target read or native fire operation occurs: actual const CPed
        // semantics are solely the current association pointer's presence.
        output = second != 0;
        return ClockResult::Ok;
    }
}
