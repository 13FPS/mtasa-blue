/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Raw current-facing intent with real registered-ped owner bookends
 ****************************************************************************/
#include "NativePedRotationMemory.h"
#include "NativePedRotationProfile.h"
#include "NativePedMetadataMemory.h"
#include "NativeImageAccess.h"
#include <array>
#include <cstring>
#include <limits>
namespace mta::game_de
{
    namespace PedRotationDetail
    {
        namespace P = NativePedRotationProfile;
        using Memory = NativePedRotationMemory;
        thread_local bool writing = false;
        struct Scope
        {
            Scope() noexcept { writing = true; }
            ~Scope() { writing = false; }
        };
        static_assert(sizeof(float) == sizeof(std::uint32_t) && sizeof(LONG) == sizeof(std::uint32_t));
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
            // Reuse the actual admitted ped metadata reader for occupied
            // generic/player identity and native GT/GC/state bookends. Its raw
            // model WORD is not interpreted or turned into a model lookup.
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
            if (expected.nativePed > (std::numeric_limits<std::uintptr_t>::max)() - P::CurrentOffset)
                return ClockResult::ReadFailure;
            const auto    at = expected.nativePed + P::CurrentOffset;
            std::uint32_t value = 0;
            SIZE_T        copied = 0;
            if (!Pages(at) || !ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(at), &value, sizeof(value), &copied) || copied != sizeof(value))
                return ClockResult::ReadFailure;
            if ((result = Identity(binding, owner, expected)) != ClockResult::Ok)
                return result;
            output = value;
            return ClockResult::Ok;
        }
#if !defined(MTA_DE_PED_ROTATION_EXCHANGE)
        bool Exchange(std::uintptr_t at, std::uint32_t expected, std::uint32_t target, std::uint32_t& observed, std::uint32_t& exception) noexcept
        {
            LONG e = 0, t = 0;
            std::memcpy(&e, &expected, sizeof(e));
            std::memcpy(&t, &target, sizeof(t));
            __try
            {
                const auto before = InterlockedCompareExchange(reinterpret_cast<volatile LONG*>(at), t, e);
                std::memcpy(&observed, &before, sizeof(observed));
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
    bool NativePedRotationMemory::Qualify(Binding& output) noexcept
    {
        Binding next;
        if (!NativePedMetadataMemory::Qualify(next) || !Revalidate(next))
            return false;
        output = next;
        return true;
    }
    bool NativePedRotationMemory::Revalidate(const Binding& binding) noexcept
    {
        namespace P = NativePedRotationProfile;
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
        std::uint32_t scale = 0;
        return contains(P::ScriptRadiansScaleRva, sizeof(scale), IMAGE_SCN_MEM_READ, IMAGE_SCN_MEM_WRITE | IMAGE_SCN_MEM_EXECUTE) &&
               I::ReadImage(binding.image, binding.image + P::ScriptRadiansScaleRva, &scale, sizeof(scale)) && scale == P::ScriptRadiansScaleBits;
    }
    ClockResult NativePedRotationMemory::ReadCurrent(const Value& expected, float& output) const noexcept
    {
        namespace D = PedRotationDetail;
        std::uint32_t first = 0, second = 0;
        auto          result = D::Capture(binding, owner, expected, first);
        if (result != ClockResult::Ok || (result = D::Capture(binding, owner, expected, second)) != ClockResult::Ok)
            return result;
        if (first != second)
            return ClockResult::OwnerConflict;
        if ((result = D::Identity(binding, owner, expected)) != ClockResult::Ok)
            return result;
        std::memcpy(&output, &second, sizeof(output));
        return ClockResult::Ok;
    }
    ClockResult NativePedRotationMemory::SetCurrent(const Value& expected, float value, Receipt* output) const noexcept
    {
        namespace D = PedRotationDetail;
        Receipt receipt;
        receipt.handle = expected.handle;
        std::memcpy(&receipt.target, &value, sizeof(value));
        const auto finish = [&](ClockResult result)
        {
            if (output)
                *output = receipt;
            return result;
        };
        if (D::writing)
            return finish(ClockResult::Busy);
        D::Scope      scope;
        std::uint32_t second = 0;
        auto          result = D::Capture(binding, owner, expected, receipt.original);
        if (result != ClockResult::Ok || (result = D::Capture(binding, owner, expected, second)) != ClockResult::Ok)
            return finish(result);
        receipt.nativePed = expected.nativePed;
        receipt.address = expected.nativePed + D::P::CurrentOffset;
        receipt.observed = second;
        receipt.observedValid = true;
        if (receipt.original != second)
            return finish(ClockResult::OwnerConflict);
        if (!D::Pages(receipt.address))
            return finish(ClockResult::WriteFailure);
        if ((result = D::Identity(binding, owner, expected)) != ClockResult::Ok)
            return finish(result);
#if defined(MTA_DE_PED_ROTATION_EXCHANGE)
        const auto exchange = &MTA_DE_PED_ROTATION_EXCHANGE;
#else
        const auto exchange = &D::Exchange;
#endif
        receipt.entered = true;
        std::uint32_t previous = 0;
        if (!exchange(receipt.address, receipt.original, receipt.target, previous, receipt.exceptionCode))
            return finish(ClockResult::WriteFailure);
        receipt.observed = previous;
        if (previous != receipt.original)
            return finish(ClockResult::OwnerConflict);
        receipt.accepted = true;
        if ((result = D::Capture(binding, owner, expected, receipt.observed)) != ClockResult::Ok)
            return finish(result);
        if (receipt.observed != receipt.target)
            return finish(ClockResult::OwnerConflict);
        if ((result = D::Identity(binding, owner, expected)) != ClockResult::Ok)
            return finish(result);
        receipt.verified = true;
        return finish(ClockResult::Ok);
    }
}
