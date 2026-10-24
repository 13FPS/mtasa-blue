/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: One source-owned reference increment on a freshly admitted block
 ****************************************************************************/
#include "NativeAnimationBlockRefMemory.h"
#include "NativeAnimationBlockRefProfile.h"
#include "NativeImageAccess.h"
#include "NativeStartGameCalls.h"
#include "NativeGameStateMemory.h"
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
        struct CounterSnapshot
        {
            std::int32_t             count = 0;
            NativeAnimationBlockName name{};
            std::uint16_t            refs = 0;
        };
        ClockResult CounterOwner(NativeAnimationBlockRefMemory::Binding binding, NativeAnimationBlockRefMemory::CheckOwner owner) noexcept
        {
            if (!owner)
                return ClockResult::NotInitialized;
            auto result = owner();
            if (result != ClockResult::Ok || (result = NativeStartGameCalls::CheckThread(binding.image)) != ClockResult::Ok)
                return result;
            std::uint32_t state = 0;
            if (!NativeGameStateMemory::Read(binding.image, state))
                return ClockResult::BindingChanged;
            return state == 9 ? owner() : ClockResult::NotInitialized;
        }
        bool CounterRow(NativeAnimationBlockRefMemory::Binding binding, std::uintptr_t rva) noexcept
        {
            // Revalidate already qualifies the exact native image/profile.
            // Also prove the selected bounded row's own PE writable extent;
            // the first bank row is not a substitute for this receiver.
            IMAGE_DOS_HEADER   dos{};
            IMAGE_NT_HEADERS64 nt{};
            if (!Image::ReadImage(binding.image, binding.image, &dos, sizeof(dos)) || dos.e_lfanew <= 0 ||
                static_cast<std::size_t>(dos.e_lfanew) > NativePedPoolProfile::ImageSize - sizeof(nt) ||
                !Image::ReadImage(binding.image, binding.image + static_cast<std::uintptr_t>(dos.e_lfanew), &nt, sizeof(nt)))
                return false;
            const auto at = static_cast<std::uintptr_t>(dos.e_lfanew) + offsetof(IMAGE_NT_HEADERS64, OptionalHeader) + nt.FileHeader.SizeOfOptionalHeader;
            std::array<IMAGE_SECTION_HEADER, 96> sections{};
            const auto                           bytes = static_cast<std::size_t>(nt.FileHeader.NumberOfSections) * sizeof(IMAGE_SECTION_HEADER);
            if (!nt.FileHeader.NumberOfSections || nt.FileHeader.NumberOfSections > sections.size() || at > NativePedPoolProfile::ImageSize ||
                bytes > NativePedPoolProfile::ImageSize - at || !Image::ReadImage(binding.image, binding.image + at, sections.data(), bytes))
                return false;
            for (std::size_t i = 0; i != nt.FileHeader.NumberOfSections; ++i)
                if (Image::SectionContains(sections[i], rva, NativeAnimationBlockProfile::RowBytes, IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_WRITE,
                                           IMAGE_SCN_MEM_EXECUTE))
                    return true;
            return false;
        }
        ClockResult CaptureCounter(NativeAnimationBlockRefMemory::Binding binding, NativeAnimationBlockRefMemory::CheckOwner owner, std::uint32_t index,
                                   const NativeAnimationBlockName* expected, CounterSnapshot& output) noexcept
        {
            auto result = CounterOwner(binding, owner);
            if (result != ClockResult::Ok)
                return result;
            if (!NativeAnimationBlockRefMemory::Revalidate(binding))
                return ClockResult::BindingChanged;
            CounterSnapshot next;
            if (!Image::ReadImage(binding.image, binding.image + NativeAnimationBlockProfile::CountRva, &next.count, sizeof(next.count), true))
                return ClockResult::ReadFailure;
            if (next.count < 0 || static_cast<std::uint32_t>(next.count) > NativeAnimationBlockProfile::ObservationLimit)
                return ClockResult::BindingChanged;
            if (index >= static_cast<std::uint32_t>(next.count))
                return ClockResult::NotInitialized;
            const auto rva = NativeAnimationBlockProfile::BankRva + static_cast<std::uintptr_t>(index) * NativeAnimationBlockProfile::RowBytes;
            if (!CounterRow(binding, rva))
                return ClockResult::BindingChanged;
            if (!Image::ReadImage(binding.image, binding.image + rva, next.name.data(), next.name.size(), true) ||
                !Image::ReadImage(binding.image, binding.image + rva + Profile::RefOffset, &next.refs, sizeof(next.refs), true))
                return ClockResult::ReadFailure;
            if (expected && next.name != *expected)
                return ClockResult::BindingChanged;
            std::int32_t after = 0;
            if (!Image::ReadImage(binding.image, binding.image + NativeAnimationBlockProfile::CountRva, &after, sizeof(after), true))
                return ClockResult::ReadFailure;
            if (after != next.count || !NativeAnimationBlockRefMemory::Revalidate(binding) || !CounterRow(binding, rva))
                return ClockResult::BindingChanged;
            if ((result = CounterOwner(binding, owner)) != ClockResult::Ok)
                return result;
            // Only name16/count/ref are facts needed by this integer API.
            // Loaded state and CString termination do not become ref policy.
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
    ClockResult NativeAnimationBlockRefMemory::ReadCounterIdentity(std::uint32_t index, NativeAnimationBlockIdentity& output) const noexcept
    {
        namespace Detail = AnimationBlockRefDetail;
        if (index >= NativeAnimationBlockProfile::ObservationLimit)
            return ClockResult::InvalidArgument;
        Detail::CounterSnapshot first, second;
        auto                    result = Detail::CaptureCounter(binding, owner, index, nullptr, first);
        if (result != ClockResult::Ok || (result = Detail::CaptureCounter(binding, owner, index, &first.name, second)) != ClockResult::Ok)
            return result;
        if (first.count != second.count || first.name != second.name)
            return ClockResult::BindingChanged;
        NativeAnimationBlockIdentity next;
        next.index = index;
        next.name = second.name;
        // Native observation grants no CPU generation or retained bank lease.
        // Reference progress between captures is legitimate for identity lookup.
        output = next;
        return ClockResult::Ok;
    }
    ClockResult NativeAnimationBlockRefMemory::ReadCounter(const NativeAnimationBlockIdentity& identity, std::uint16_t& output) const noexcept
    {
        namespace Detail = AnimationBlockRefDetail;
        const auto expected = identity;
        if (expected.index >= NativeAnimationBlockProfile::ObservationLimit)
            return ClockResult::InvalidArgument;
        Detail::CounterSnapshot first, second;
        auto                    result = Detail::CaptureCounter(binding, owner, expected.index, &expected.name, first);
        if (result != ClockResult::Ok || (result = Detail::CaptureCounter(binding, owner, expected.index, &expected.name, second)) != ClockResult::Ok)
            return result;
        if (first.refs != second.refs)
            return ClockResult::OwnerConflict;
        output = second.refs;
        return ClockResult::Ok;
    }
    ClockResult NativeAnimationBlockRefMemory::Add(const NativeAnimationBlockIdentity& identity, Receipt* output, Exchange exchange) const noexcept
    {
        return Adjust(identity, IdentityPolicy::Named, 1, output, exchange);
    }
    ClockResult NativeAnimationBlockRefMemory::AddCounter(const NativeAnimationBlockIdentity& identity, Receipt* output, Exchange exchange) const noexcept
    {
        return Adjust(identity, IdentityPolicy::Counter, 1, output, exchange);
    }
    ClockResult NativeAnimationBlockRefMemory::RemoveWithoutDelete(const NativeAnimationBlockIdentity& identity, Receipt* output,
                                                                   Exchange exchange) const noexcept
    {
        return Adjust(identity, IdentityPolicy::Counter, -1, output, exchange);
    }
    ClockResult NativeAnimationBlockRefMemory::Adjust(const NativeAnimationBlockIdentity& identity, IdentityPolicy policy, int direction, Receipt* output,
                                                      Exchange exchange) const noexcept
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
        if (expected.index >= NativeAnimationBlockProfile::ObservationLimit || (policy == IdentityPolicy::Named && !Detail::ValidIdentity(expected)))
            return finish(ClockResult::InvalidArgument);
        auto result = policy == IdentityPolicy::Named ? Read(expected, receipt.before) : ReadCounter(expected, receipt.before);
        if (result != ClockResult::Ok)
            return finish(result);
        receipt.address = binding.image + NativeAnimationBlockProfile::BankRva +
                          static_cast<std::uintptr_t>(expected.index) * NativeAnimationBlockProfile::RowBytes + NativeAnimationBlockRefProfile::RefOffset;
        if (!Detail::Image::ImagePages(binding.image, receipt.address, sizeof(std::uint16_t), true))
            return finish(ClockResult::WriteFailure);
        // Both addressed MTA operations preserve the full raw WORD. The
        // private directions are +1/-1; unsigned32 addition plus narrowing
        // provides exactly modulo65536, including decrement of zero toFFFF.
        receipt.target = static_cast<std::uint16_t>(static_cast<std::uint32_t>(receipt.before) + (direction > 0 ? 1u : 0xffffu));
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
        if (policy == IdentityPolicy::Named)
        {
            std::uint16_t current = 0;
            if ((result = Detail::Capture(binding, owner, expected, current)) != ClockResult::Ok)
                return finish(result);
        }
        else
        {
            Detail::CounterSnapshot current;
            if ((result = Detail::CaptureCounter(binding, owner, expected.index, &expected.name, current)) != ClockResult::Ok)
                return finish(result);
        }
        // The successful CAS already proves this exact adjustment. Native
        // progress can subsequently change the current ref/loaded metadata;
        // require the same valid receiver, never restore its earlier count.
        receipt.verified = true;
        return finish(ClockResult::Ok);
    }
}
