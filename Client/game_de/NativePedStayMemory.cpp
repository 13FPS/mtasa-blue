/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Preserve named stay-in-place bool storage and all unowned neighboring flags
 ****************************************************************************/
#include "NativePedStayMemory.h"
#include "NativePedStayProfile.h"
#include "NativePedMetadataMemory.h"
#include "NativeImageAccess.h"
#include <array>
#include <cstring>
#include <limits>
#include <intrin.h>
namespace mta::game_de
{
    namespace PedStayDetail
    {
        namespace P = NativePedStayProfile;
        using Memory = NativePedStayMemory;
        thread_local bool writing = false;
        struct Scope
        {
            Scope() noexcept { writing = true; }
            ~Scope() { writing = false; }
        };
        static_assert(sizeof(LONG) == sizeof(std::uint32_t));
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
        ClockResult Capture(Memory::Binding binding, Memory::CheckOwner owner, const Memory::Value& expected, std::uint32_t& output) noexcept
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
#if !defined(MTA_DE_PED_STAY_EXCHANGE)
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
    bool NativePedStayMemory::Qualify(Binding& output) noexcept
    {
        Binding next;
        if (!NativePedMetadataMemory::Qualify(next) || !Revalidate(next))
            return false;
        output = next;
        return true;
    }
    bool NativePedStayMemory::Revalidate(const Binding& binding) noexcept
    {
        namespace P = NativePedStayProfile;
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
        return true;
    }
    ClockResult NativePedStayMemory::ReadStayInSamePlace(const Value& expected, bool& output) const noexcept
    {
        namespace D = PedStayDetail;
        std::uint32_t first = 0, second = 0;
        auto          result = D::Capture(binding, owner, expected, first);
        if (result != ClockResult::Ok || (result = D::Capture(binding, owner, expected, second)) != ClockResult::Ok)
            return result;
        if ((first ^ second) & D::P::StayMask)
            return ClockResult::OwnerConflict;
        if ((result = D::Identity(binding, owner, expected)) != ClockResult::Ok)
            return result;
        output = (second & D::P::StayMask) != 0;
        return ClockResult::Ok;
    }
    ClockResult NativePedStayMemory::SetStayInSamePlace(const Value& expected, bool enabled, Receipt* output) const noexcept
    {
        namespace D = PedStayDetail;
        Receipt receipt;
        receipt.handle = expected.handle;
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
        auto          result = D::Capture(binding, owner, expected, receipt.original);
        if (result != ClockResult::Ok || (result = D::Capture(binding, owner, expected, current)) != ClockResult::Ok)
            return finish(result);
        constexpr auto mask = D::P::StayMask;
        const auto     originalOwned = receipt.original & mask;
        if ((current & mask) != originalOwned)
            return finish(ClockResult::OwnerConflict);
        receipt.nativePed = expected.nativePed;
        receipt.address = expected.nativePed + D::P::FlagsOffset;
        receipt.observed = current;
        receipt.observedValid = true;
#if defined(MTA_DE_PED_STAY_EXCHANGE)
        const auto exchange = &MTA_DE_PED_STAY_EXCHANGE;
#else
        const auto exchange = &D::Exchange;
#endif
        for (std::size_t attempt = 0; attempt != D::P::MaximumAttempts; ++attempt)
        {
            if ((current & mask) != originalOwned)
                return finish(ClockResult::OwnerConflict);
            if (!D::Pages(receipt.address))
                return finish(ClockResult::WriteFailure);
            if ((result = D::Identity(binding, owner, expected)) != ClockResult::Ok)
                return finish(result);
            receipt.target = (current & ~mask) | (enabled ? mask : 0);
            receipt.entered = true;
            ++receipt.attempts;
            std::uint32_t previous = 0;
            if (!exchange(receipt.address, current, receipt.target, previous, receipt.exceptionCode))
                return finish(ClockResult::WriteFailure);
            receipt.observed = previous;
            if (previous != current)
            {
                // A failed CAS has not written anything. Preserve any change
                // to our bit as a conflict; only independent neighboring bits
                // may be reacquired before another bounded attempt.
                if ((previous & mask) != originalOwned)
                    return finish(ClockResult::OwnerConflict);
                if ((result = D::Capture(binding, owner, expected, current)) != ClockResult::Ok)
                    return finish(result);
                continue;
            }
            receipt.accepted = true;
            // Nothing after this point retries the accepted write. Current
            // native state is observed afresh, and only our mask is verified.
            if ((result = D::Capture(binding, owner, expected, receipt.observed)) != ClockResult::Ok)
                return finish(result);
            if ((receipt.observed ^ receipt.target) & mask)
                return finish(ClockResult::OwnerConflict);
            if ((result = D::Identity(binding, owner, expected)) != ClockResult::Ok)
                return finish(result);
            receipt.verified = true;
            return finish(ClockResult::Ok);
        }
        return finish(ClockResult::OwnerConflict);
    }
}
