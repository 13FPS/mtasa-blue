/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: One source-owned reference increment on a freshly admitted block
 ****************************************************************************/
#include "NativeAnimationBlockRefMemory.h"
#include "NativeAnimationBlockRefProfile.h"
#include "NativeImageAccess.h"
#include <array>
#include <cstring>
#include <intrin.h>
namespace mta::game_de
{
    namespace AnimationBlockRefDetail
    {
        namespace Profile = NativeAnimationBlockRefProfile;
        namespace Image = NativeImageAccess;
        bool ValidIdentity(const NativeAnimationBlockIdentity& identity) noexcept
        {
            if (identity.index >= NativeAnimationBlockProfile::ObservationLimit)
                return false;
            for (const auto byte : identity.name)
                if (!byte)
                    return true;
            return false;
        }
        ClockResult Capture(NativeAnimationBlockRefMemory::Binding binding, NativeAnimationBlockRefMemory::CheckOwner owner,
                            const NativeAnimationBlockIdentity& identity, std::uint16_t& output) noexcept
        {
            NativeAnimationBlockMemory memory(binding, owner);
            NativeAnimationBlockValues ignored;
            auto                       result = memory.Read(identity, ignored);
            if (result != ClockResult::Ok)
                return result;
            if (!NativeAnimationBlockRefMemory::Revalidate(binding))
                return ClockResult::BindingChanged;
            const auto address = binding.image + NativeAnimationBlockProfile::BankRva +
                                 static_cast<std::uintptr_t>(identity.index) * NativeAnimationBlockProfile::RowBytes + Profile::RefOffset;
            std::uint16_t next = 0;
            if (!Image::ReadImage(binding.image, address, &next, sizeof(next), true))
                return ClockResult::ReadFailure;
            // Reuse the real current block/name/count and native GT/GC/state9
            // admission. Metadata and loaded status do not become ref policy.
            if (!NativeAnimationBlockRefMemory::Revalidate(binding))
                return ClockResult::BindingChanged;
            if ((result = memory.Read(identity, ignored)) != ClockResult::Ok)
                return result;
            output = next;
            return ClockResult::Ok;
        }
#if !defined(MTA_DE_ANIMATION_BLOCK_REF_EXCHANGE)
        ClockResult NativeExchange(std::uintptr_t address, std::uint16_t expected, std::uint16_t target, std::uint16_t& observed,
                                   std::uint32_t& exception) noexcept
        {
            static_assert(sizeof(SHORT) == sizeof(std::uint16_t));
            SHORT compare = 0, desired = 0;
            std::memcpy(&compare, &expected, sizeof(compare));
            std::memcpy(&desired, &target, sizeof(desired));
            __try
            {
                const auto previous = InterlockedCompareExchange16(reinterpret_cast<volatile SHORT*>(address), desired, compare);
                std::memcpy(&observed, &previous, sizeof(observed));
                return ClockResult::Ok;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                exception = GetExceptionCode();
                return ClockResult::OriginalException;
            }
        }
#endif
    }
    bool NativeAnimationBlockRefMemory::Revalidate(const Binding& binding) noexcept
    {
        namespace Detail = AnimationBlockRefDetail;
        namespace Profile = NativeAnimationBlockRefProfile;
        if (!NativeAnimationBlockMemory::Revalidate(binding))
            return false;
        IMAGE_DOS_HEADER   dos{};
        IMAGE_NT_HEADERS64 nt{};
        if (!Detail::Image::ReadImage(binding.image, binding.image, &dos, sizeof(dos)) || dos.e_lfanew <= 0 ||
            static_cast<std::size_t>(dos.e_lfanew) > NativePedPoolProfile::ImageSize - sizeof(nt) ||
            !Detail::Image::ReadImage(binding.image, binding.image + static_cast<std::uintptr_t>(dos.e_lfanew), &nt, sizeof(nt)))
            return false;
        const auto at = static_cast<std::uintptr_t>(dos.e_lfanew) + offsetof(IMAGE_NT_HEADERS64, OptionalHeader) + nt.FileHeader.SizeOfOptionalHeader;
        std::array<IMAGE_SECTION_HEADER, 96> sections{};
        const auto                           bytes = static_cast<std::size_t>(nt.FileHeader.NumberOfSections) * sizeof(IMAGE_SECTION_HEADER);
        if (!nt.FileHeader.NumberOfSections || nt.FileHeader.NumberOfSections > sections.size() || at > NativePedPoolProfile::ImageSize ||
            bytes > NativePedPoolProfile::ImageSize - at || !Detail::Image::ReadImage(binding.image, binding.image + at, sections.data(), bytes))
            return false;
        for (const auto& pin : Profile::Code)
        {
            bool executable = false;
            for (std::size_t index = 0; index != nt.FileHeader.NumberOfSections; ++index)
                if (Detail::Image::SectionContains(sections[index], pin.rva, pin.size, IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_EXECUTE, IMAGE_SCN_MEM_WRITE))
                    executable = true;
            std::array<unsigned char, Profile::MaximumPinBytes> actual{};
            if (!executable || !pin.size || pin.size > actual.size() ||
                !Detail::Image::ReadImage(binding.image, binding.image + pin.rva, actual.data(), pin.size, false, true) ||
                std::memcmp(actual.data(), pin.bytes, pin.size))
                return false;
        }
        return true;
    }
    ClockResult NativeAnimationBlockRefMemory::Read(const NativeAnimationBlockIdentity& identity, std::uint16_t& output) const noexcept
    {
        namespace Detail = AnimationBlockRefDetail;
        const auto expected = identity;
        if (!Detail::ValidIdentity(expected))
            return ClockResult::InvalidArgument;
        std::uint16_t first = 0, second = 0;
        auto          result = Detail::Capture(binding, owner, expected, first);
        if (result != ClockResult::Ok || (result = Detail::Capture(binding, owner, expected, second)) != ClockResult::Ok)
            return result;
        if (first != second)
            return ClockResult::OwnerConflict;
        output = second;
        return ClockResult::Ok;
    }
    ClockResult NativeAnimationBlockRefMemory::Add(const NativeAnimationBlockIdentity& identity, Receipt* output, Exchange exchange) const noexcept
    {
        namespace Detail = AnimationBlockRefDetail;
        Receipt    receipt;
        const auto finish = [&](ClockResult result)
        {
            if (output)
                *output = receipt;
            return result;
        };
        const auto expected = identity;
        if (!Detail::ValidIdentity(expected))
            return finish(ClockResult::InvalidArgument);
        auto result = Read(expected, receipt.before);
        if (result != ClockResult::Ok)
            return finish(result);
        receipt.address = binding.image + NativeAnimationBlockProfile::BankRva +
                          static_cast<std::uintptr_t>(expected.index) * NativeAnimationBlockProfile::RowBytes + NativeAnimationBlockRefProfile::RefOffset;
        if (!Detail::Image::ImagePages(binding.image, receipt.address, sizeof(std::uint16_t), true))
            return finish(ClockResult::WriteFailure);
        // Integral promotion computes in unsigned32; conversion deliberately
        // preserves MTA's ushort wrap rather than adding a saturation policy.
        receipt.target = static_cast<std::uint16_t>(static_cast<std::uint32_t>(receipt.before) + 1u);
#if defined(MTA_DE_ANIMATION_BLOCK_REF_EXCHANGE)
        const auto entry = exchange ? exchange : &MTA_DE_ANIMATION_BLOCK_REF_EXCHANGE;
#else
        const auto entry = exchange ? exchange : &Detail::NativeExchange;
#endif
        receipt.entered = true;
        result = entry(receipt.address, receipt.before, receipt.target, receipt.observed, receipt.exceptionCode);
        receipt.accepted = result == ClockResult::Ok && receipt.observed == receipt.before;
        if (result != ClockResult::Ok)
            return finish(result);
        if (!receipt.accepted)
            return finish(ClockResult::Busy);
        std::uint16_t current = 0;
        if ((result = Detail::Capture(binding, owner, expected, current)) != ClockResult::Ok)
            return finish(result);
        // The successful CAS already proves this exact increment. Native
        // progress can subsequently change the current ref/loaded metadata;
        // require the same valid receiver, never restore its earlier count.
        receipt.verified = true;
        return finish(ClockResult::Ok);
    }
}
