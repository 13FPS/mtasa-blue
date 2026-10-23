/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Read the native visibility bit without publishing unrelated flag state
 ****************************************************************************/
#include "NativePedVisibilityMemory.h"
#include "NativePedVisibilityProfile.h"
#include "NativePedMetadataMemory.h"
#include "NativeImageAccess.h"
#include <array>
#include <cstring>
#include <limits>
#include <intrin.h>
namespace mta::game_de
{
    namespace PedVisibilityDetail
    {
        namespace P = NativePedVisibilityProfile;
        using Memory = NativePedVisibilityMemory;
        thread_local bool writing = false;
        struct Scope
        {
            Scope() noexcept { writing = true; }
            ~Scope() { writing = false; }
        };
        bool Pages(std::uintptr_t at) noexcept
        {
            if (!at || at % alignof(std::uint32_t) || at > (std::numeric_limits<std::uintptr_t>::max)() - sizeof(std::uint32_t))
                return false;
            MEMORY_BASIC_INFORMATION page{};
            if (VirtualQuery(reinterpret_cast<void*>(at), &page, sizeof(page)) != sizeof(page) || page.State != MEM_COMMIT ||
                (page.Type != MEM_PRIVATE && page.Type != MEM_MAPPED) || (page.Protect & (PAGE_GUARD | PAGE_NOACCESS)) ||
                (page.Protect & 0xff) != PAGE_READWRITE)
                return false;
            const auto begin = reinterpret_cast<std::uintptr_t>(page.BaseAddress);
            return at >= begin && page.RegionSize >= sizeof(std::uint32_t) && at - begin <= page.RegionSize - sizeof(std::uint32_t);
        }
        ClockResult Identity(Memory::Binding binding, Memory::CheckOwner owner, const Memory::Value& expected) noexcept
        {
            if (!owner)
                return ClockResult::NotInitialized;
            if (!Memory::Revalidate(binding))
                return ClockResult::BindingChanged;
            // The actual metadata primitive already owns occupied generic/
            // player identity and native GT/GC/state admission. Reuse it;
            // its raw model WORD is never interpreted as a model lookup.
            NativePedMetadata metadata;
            const auto        result = NativePedMetadataMemory(binding, owner).Read(expected, metadata);
            if (result != ClockResult::Ok)
                return result;
            if (metadata.entityType != 3)
                return ClockResult::BindingChanged;
            return owner();
        }
        ClockResult Capture(Memory::Binding binding, Memory::CheckOwner owner, const Memory::Value& expected, bool& output) noexcept
        {
            auto result = Identity(binding, owner, expected);
            if (result != ClockResult::Ok)
                return result;
            if (expected.nativePed > (std::numeric_limits<std::uintptr_t>::max)() - P::FlagsOffset)
                return ClockResult::ReadFailure;
            const auto    at = expected.nativePed + P::FlagsOffset;
            std::uint32_t flags = 0;
            SIZE_T        copied = 0;
            if (!Pages(at) || !ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(at), &flags, sizeof(flags), &copied) || copied != sizeof(flags))
                return ClockResult::ReadFailure;
            if ((result = Identity(binding, owner, expected)) != ClockResult::Ok)
                return result;
            output = (flags & P::VisibleMask) != 0;
            return ClockResult::Ok;
        }
        ClockResult CaptureWord(Memory::Binding binding, Memory::CheckOwner owner, const Memory::Value& expected, std::uint32_t& output) noexcept
        {
            auto result = Identity(binding, owner, expected);
            if (result != ClockResult::Ok)
                return result;
            if (expected.nativePed > (std::numeric_limits<std::uintptr_t>::max)() - P::FlagsOffset)
                return ClockResult::ReadFailure;
            const auto    at = expected.nativePed + P::FlagsOffset;
            std::uint32_t flags = 0;
            SIZE_T        copied = 0;
            if (!Pages(at) || !ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(at), &flags, sizeof(flags), &copied) || copied != sizeof(flags))
                return ClockResult::ReadFailure;
            if ((result = Identity(binding, owner, expected)) != ClockResult::Ok)
                return result;
            output = flags;
            return ClockResult::Ok;
        }
#if !defined(MTA_DE_PED_VISIBILITY_EXCHANGE)
        bool Exchange(std::uintptr_t at, std::uint32_t expected, std::uint32_t target, std::uint32_t& observed, std::uint32_t& exception) noexcept
        {
            LONG before = 0, after = 0;
            std::memcpy(&before, &expected, sizeof(before));
            std::memcpy(&after, &target, sizeof(after));
            __try
            {
                const auto previous = InterlockedCompareExchange(reinterpret_cast<volatile LONG*>(at), after, before);
                std::memcpy(&observed, &previous, sizeof(observed));
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
    bool NativePedVisibilityMemory::Qualify(Binding& output) noexcept
    {
        Binding next;
        if (!NativePedMetadataMemory::Qualify(next) || !Revalidate(next))
            return false;
        output = next;
        return true;
    }
    bool NativePedVisibilityMemory::Revalidate(const Binding& binding) noexcept
    {
        namespace P = NativePedVisibilityProfile;
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
        std::uintptr_t group = 0;
        std::uint32_t  branch = 0;
        unsigned char  selector = 0;
        if (!contains(P::GroupSlotRva, sizeof(group), IMAGE_SCN_MEM_READ, IMAGE_SCN_MEM_WRITE | IMAGE_SCN_MEM_EXECUTE) ||
            !contains(P::RemapRva + P::CaseIndex, 1, IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_EXECUTE, IMAGE_SCN_MEM_WRITE) ||
            !contains(P::TableRva + P::Selector * 4, sizeof(branch), IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_EXECUTE, IMAGE_SCN_MEM_WRITE) ||
            !I::ReadImage(binding.image, binding.image + P::GroupSlotRva, &group, sizeof(group)) || group != binding.image + P::GroupRva ||
            !I::ReadImage(binding.image, binding.image + P::RemapRva + P::CaseIndex, &selector, sizeof(selector), false, true) || selector != P::Selector ||
            !I::ReadImage(binding.image, binding.image + P::TableRva + P::Selector * 4, &branch, sizeof(branch), false, true) || branch != P::CaseRva)
            return false;
        return true;
    }
    ClockResult NativePedVisibilityMemory::ReadVisible(const Value& expected, bool& output) const noexcept
    {
        namespace D = PedVisibilityDetail;
        const Value selected = expected;
        bool        first = false, second = false;
        auto        result = D::Capture(binding, owner, selected, first);
        if (result != ClockResult::Ok || (result = D::Capture(binding, owner, selected, second)) != ClockResult::Ok)
            return result;
        if (first != second)
            return ClockResult::OwnerConflict;
        if ((result = D::Identity(binding, owner, selected)) != ClockResult::Ok)
            return result;
        // Neighboring flags may legitimately change while the named visibility
        // bit remains stable. They are neither returned nor a made-up state
        // machine, so only this proven boolean participates in coherence.
        output = second;
        return ClockResult::Ok;
    }
    ClockResult NativePedVisibilityMemory::SetVisible(const Value& expected, bool enabled, Receipt* output) const noexcept
    {
        namespace D = PedVisibilityDetail;
        const Value selected = expected;
        Receipt     receipt;
        receipt.handle = selected.handle;
        const auto finish = [&](ClockResult result)
        {
            if (output)
                *output = receipt;
            return result;
        };
        if (D::writing)
            return finish(ClockResult::Busy);
        D::Scope      scope;
        std::uint32_t current = 0;
        auto          result = D::CaptureWord(binding, owner, selected, receipt.original);
        if (result != ClockResult::Ok || (result = D::CaptureWord(binding, owner, selected, current)) != ClockResult::Ok)
            return finish(result);
        constexpr auto mask = D::P::VisibleMask;
        const auto     originalOwned = receipt.original & mask;
        if ((current & mask) != originalOwned)
            return finish(ClockResult::OwnerConflict);
        receipt.nativePed = selected.nativePed;
        receipt.address = selected.nativePed + D::P::FlagsOffset;
        receipt.observed = current;
        receipt.observedValid = true;
#if defined(MTA_DE_PED_VISIBILITY_EXCHANGE)
        const auto exchange = &MTA_DE_PED_VISIBILITY_EXCHANGE;
#else
        const auto exchange = &D::Exchange;
#endif
        if (!D::Pages(receipt.address))
            return finish(ClockResult::WriteFailure);
        if ((result = D::Identity(binding, owner, selected)) != ClockResult::Ok)
            return finish(result);
        receipt.target = (current & ~mask) | (enabled ? mask : 0);
        receipt.entered = true;
        receipt.attempts = 1;
        std::uint32_t previous = 0;
        if (!exchange(receipt.address, current, receipt.target, previous, receipt.exceptionCode))
            return finish(ClockResult::WriteFailure);
        receipt.observed = previous;
        // A failed CAS has written nothing. Keep the racing engine word;
        // this SDK intent never reacquires it for a second write attempt.
        if (previous != current)
            return finish(ClockResult::OwnerConflict);
        receipt.accepted = true;
        if ((result = D::CaptureWord(binding, owner, selected, receipt.observed)) != ClockResult::Ok)
            return finish(result);
        if ((receipt.observed ^ receipt.target) & mask)
            return finish(ClockResult::OwnerConflict);
        if ((result = D::Identity(binding, owner, selected)) != ClockResult::Ok)
            return finish(result);
        receipt.verified = true;
        return finish(ClockResult::Ok);
    }
}
