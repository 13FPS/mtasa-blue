/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Raw area property, preserving all neighboring bytes and native debt
 ****************************************************************************/
#include "NativePedAreaMemory.h"
#include "NativePedAreaProfile.h"
#include "NativePedMetadataMemory.h"
#include "NativeImageAccess.h"
#include <array>
#include <cstring>
#include <limits>
#include <intrin.h>
namespace mta::game_de
{
    namespace PedAreaDetail
    {
        namespace P = NativePedAreaProfile;
        namespace I = NativeImageAccess;
        using Memory = NativePedAreaMemory;
        thread_local bool writing = false;
        struct Scope
        {
            Scope() noexcept { writing = true; }
            ~Scope() { writing = false; }
        };
        bool Pages(std::uintptr_t at) noexcept
        {
            if (!at || at == (std::numeric_limits<std::uintptr_t>::max)())
                return false;
            MEMORY_BASIC_INFORMATION page{};
            if (VirtualQuery(reinterpret_cast<void*>(at), &page, sizeof(page)) != sizeof(page) || page.State != MEM_COMMIT ||
                (page.Type != MEM_PRIVATE && page.Type != MEM_MAPPED) || (page.Protect & (PAGE_GUARD | PAGE_NOACCESS)) ||
                (page.Protect & 0xff) != PAGE_READWRITE)
                return false;
            const auto begin = reinterpret_cast<std::uintptr_t>(page.BaseAddress);
            return at >= begin && at - begin < page.RegionSize;
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
            if (metadata.entityType != 3)
                return ClockResult::BindingChanged;
            return owner();
        }
        ClockResult Capture(Memory::Binding binding, Memory::CheckOwner owner, const Memory::Value& expected, std::uint8_t& output) noexcept
        {
            auto result = Identity(binding, owner, expected);
            if (result != ClockResult::Ok)
                return result;
            if (expected.nativePed > (std::numeric_limits<std::uintptr_t>::max)() - P::AreaOffset)
                return ClockResult::ReadFailure;
            const auto   at = expected.nativePed + P::AreaOffset;
            std::uint8_t next = 0;
            SIZE_T       copied = 0;
            if (!Pages(at) || !ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(at), &next, sizeof(next), &copied) || copied != sizeof(next))
                return ClockResult::ReadFailure;
            if ((result = Identity(binding, owner, expected)) != ClockResult::Ok)
                return result;
            output = next;
            return ClockResult::Ok;
        }
#if !defined(MTA_DE_PED_AREA_EXCHANGE)
        bool Exchange(std::uintptr_t at, std::uint8_t expected, std::uint8_t target, std::uint8_t& observed, std::uint32_t& exception) noexcept
        {
            static_assert(sizeof(CHAR) == sizeof(std::uint8_t));
            CHAR before = 0, after = 0;
            std::memcpy(&before, &expected, 1);
            std::memcpy(&after, &target, 1);
            __try
            {
                const auto previous = _InterlockedCompareExchange8(reinterpret_cast<volatile CHAR*>(at), after, before);
                std::memcpy(&observed, &previous, 1);
                return true;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                exception = GetExceptionCode();
                return false;
            }
        }
#endif
    }
    bool NativePedAreaMemory::Qualify(Binding& output) noexcept
    {
        Binding next;
        if (!NativePedMetadataMemory::Qualify(next) || !Revalidate(next))
            return false;
        output = next;
        return true;
    }
    bool NativePedAreaMemory::Revalidate(const Binding& binding) noexcept
    {
        namespace D = PedAreaDetail;
        namespace P = NativePedAreaProfile;
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
        for (const auto& group : P::Groups)
        {
            std::uintptr_t entry = 0;
            std::uint32_t  branch = 0;
            if (!contains(group.slotRva, 8, IMAGE_SCN_MEM_READ, IMAGE_SCN_MEM_WRITE | IMAGE_SCN_MEM_EXECUTE) ||
                !contains(group.tableRva + group.index * 4, 4, rx, IMAGE_SCN_MEM_WRITE) ||
                !I::ReadImage(binding.image, binding.image + group.slotRva, &entry, sizeof(entry)) || entry != binding.image + group.entryRva ||
                !I::ReadImage(binding.image, binding.image + group.tableRva + group.index * 4, &branch, sizeof(branch), false, true) ||
                branch != group.branchRva)
                return false;
        }
        return true;
    }
    ClockResult NativePedAreaMemory::ReadAreaCode(const Value& expected, std::uint8_t& output) const noexcept
    {
        namespace D = PedAreaDetail;
        std::uint8_t first = 0, second = 0;
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
    ClockResult NativePedAreaMemory::SetAreaCode(const Value& expected, std::uint8_t target, Receipt* output) const noexcept
    {
        namespace D = PedAreaDetail;
        Receipt receipt;
        receipt.handle = expected.handle;
        receipt.target = target;
        const auto finish = [&](ClockResult result)
        {
            if (output)
                *output = receipt;
            return result;
        };
        if (D::writing)
            return finish(ClockResult::Busy);
        D::Scope     areaMutation;
        std::uint8_t current = 0;
        auto         result = D::Capture(binding, owner, expected, receipt.original);
        if (result != ClockResult::Ok || (result = D::Capture(binding, owner, expected, current)) != ClockResult::Ok)
            return finish(result);
        if (current != receipt.original)
            return finish(ClockResult::OwnerConflict);
        receipt.nativePed = expected.nativePed;
        receipt.address = expected.nativePed + D::P::AreaOffset;
        receipt.observed = current;
        receipt.observedValid = true;
        if (!D::Pages(receipt.address))
            return finish(ClockResult::WriteFailure);
        if ((result = D::Identity(binding, owner, expected)) != ClockResult::Ok)
            return finish(result);
#if defined(MTA_DE_PED_AREA_EXCHANGE)
        const auto exchange = &MTA_DE_PED_AREA_EXCHANGE;
#else
        const auto exchange = &D::Exchange;
#endif
        receipt.entered = true;
        std::uint8_t previous = 0;
        if (!exchange(receipt.address, current, target, previous, receipt.exceptionCode))
            return finish(ClockResult::WriteFailure);
        receipt.observed = previous;
        if (previous != current)
            return finish(ClockResult::OwnerConflict);
        receipt.accepted = true;
        // Exactly one BYTE CAS preserves all neighbors inherently. Even a
        // same-value request must pass current owner/token admission and that
        // conditional boundary. Accepted or uncertain effects never replay.
        if ((result = D::Capture(binding, owner, expected, receipt.observed)) != ClockResult::Ok)
            return finish(result);
        if (receipt.observed != target)
            return finish(ClockResult::OwnerConflict);
        if ((result = D::Identity(binding, owner, expected)) != ClockResult::Ok)
            return finish(result);
        receipt.verified = true;
        return finish(ClockResult::Ok);
    }
}
