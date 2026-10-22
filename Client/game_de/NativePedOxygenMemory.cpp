/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Raw breath intent with exact ped and published player-data owner bookends
 ****************************************************************************/
#include "NativePedOxygenMemory.h"
#include "NativePedOxygenProfile.h"
#include "NativePedMetadataMemory.h"
#include "NativeImageAccess.h"
#include <array>
#include <cstring>
#include <limits>
namespace mta::game_de
{
    namespace PedOxygenDetail
    {
        namespace P = NativePedOxygenProfile;
        using Memory = NativePedOxygenMemory;
        thread_local bool writing = false;
        struct Scope
        {
            Scope() noexcept { writing = true; }
            ~Scope() { writing = false; }
        };
        static_assert(sizeof(float) == sizeof(std::uint32_t) && sizeof(LONG) == sizeof(std::uint32_t));
        template <class T>
        bool Object(std::uintptr_t at, T& value) noexcept
        {
            if (!at || at > (std::numeric_limits<std::uintptr_t>::max)() - sizeof(T))
                return false;
            MEMORY_BASIC_INFORMATION page{};
            if (VirtualQuery(reinterpret_cast<void*>(at), &page, sizeof(page)) != sizeof(page) || page.State != MEM_COMMIT ||
                (page.Type != MEM_PRIVATE && page.Type != MEM_MAPPED) || (page.Protect & (PAGE_GUARD | PAGE_NOACCESS)) ||
                (page.Protect & 0xff) != PAGE_READWRITE)
                return false;
            const auto begin = reinterpret_cast<std::uintptr_t>(page.BaseAddress);
            SIZE_T     copied = 0;
            return at >= begin && page.RegionSize >= sizeof(T) && at - begin <= page.RegionSize - sizeof(T) &&
                   ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(at), &value, sizeof(value), &copied) && copied == sizeof(value);
        }
        ClockResult Context(Memory::Binding binding, Memory::CheckOwner owner, const Memory::Value& expected, std::uintptr_t& output) noexcept
        {
            if (!owner)
                return ClockResult::NotInitialized;
            if (!Memory::Revalidate(binding))
                return ClockResult::BindingChanged;
            // The actual metadata reader retains native GT/GC/state, occupied
            // pool generation and exact ped identity. Oxygen never substitutes
            // a current-player pointer for the registered receiver.
            NativePedMetadata metadata;
            auto              result = NativePedMetadataMemory(binding, owner).Read(expected, metadata);
            if (result != ClockResult::Ok)
                return result;
            if (metadata.entityType != 3 || expected.nativePed > (std::numeric_limits<std::uintptr_t>::max)() - P::PlayerTagOffset)
                return ClockResult::BindingChanged;
            std::uintptr_t data = 0, published = 0, control = 0;
            std::int32_t   tag = -1;
            if (!Object(expected.nativePed + P::PlayerDataOffset, data))
                return ClockResult::ReadFailure;
            if (!data)
                return ClockResult::NotInitialized;
            if (expected.vtable != binding.image + P::PlayerVtableRva)
                return ClockResult::UnsupportedBuild;
            if (!Object(expected.nativePed + P::PlayerTagOffset, tag))
                return ClockResult::ReadFailure;
            if (tag != 0 || data != binding.image + P::BankRva + P::InlineDataOffset)
                return ClockResult::BindingChanged;
            if (!NativeImageAccess::ImagePages(binding.image, binding.image + P::BankRva, P::RowBytes, true) ||
                !NativeImageAccess::ReadImage(binding.image, binding.image + P::BankRva, &published, sizeof(published), true) ||
                !NativeImageAccess::ReadImage(binding.image, expected.vtable + 0x50, &control, sizeof(control)))
                return ClockResult::ReadFailure;
            if (published != expected.nativePed || control != binding.image + P::PlayerControlRva)
                return ClockResult::BindingChanged;
            if ((result = owner()) != ClockResult::Ok)
                return result;
            output = data;
            return ClockResult::Ok;
        }
        struct Snapshot
        {
            std::uintptr_t data = 0;
            std::uint32_t  bits = 0;
        };
        ClockResult Capture(Memory::Binding binding, Memory::CheckOwner owner, const Memory::Value& expected, Snapshot& output) noexcept
        {
            Snapshot value;
            auto     result = Context(binding, owner, expected, value.data);
            if (result != ClockResult::Ok)
                return result;
            if (!NativeImageAccess::ReadImage(binding.image, value.data + P::BreathOffset, &value.bits, sizeof(value.bits), true))
                return ClockResult::ReadFailure;
            std::uintptr_t after = 0;
            if ((result = Context(binding, owner, expected, after)) != ClockResult::Ok)
                return result;
            if (after != value.data)
                return ClockResult::BindingChanged;
            output = value;
            return ClockResult::Ok;
        }
#if !defined(MTA_DE_PED_OXYGEN_EXCHANGE)
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
    bool NativePedOxygenMemory::Qualify(Binding& output) noexcept
    {
        Binding next;
        if (!NativePedMetadataMemory::Qualify(next) || !Revalidate(next))
            return false;
        output = next;
        return true;
    }
    bool NativePedOxygenMemory::Revalidate(const Binding& binding) noexcept
    {
        namespace P = NativePedOxygenProfile;
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
        return contains(P::BankRva, P::RowBytes, IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_WRITE, IMAGE_SCN_MEM_EXECUTE);
    }
    ClockResult NativePedOxygenMemory::ReadOxygen(const Value& expected, float& output) const noexcept
    {
        namespace D = PedOxygenDetail;
        D::Snapshot first, second;
        auto        result = D::Capture(binding, owner, expected, first);
        if (result != ClockResult::Ok || (result = D::Capture(binding, owner, expected, second)) != ClockResult::Ok)
            return result;
        if (first.data != second.data || first.bits != second.bits)
            return ClockResult::OwnerConflict;
        std::uintptr_t finalData = 0;
        if ((result = D::Context(binding, owner, expected, finalData)) != ClockResult::Ok)
            return result;
        if (finalData != second.data)
            return ClockResult::BindingChanged;
        std::memcpy(&output, &second.bits, sizeof(output));
        return ClockResult::Ok;
    }
    ClockResult NativePedOxygenMemory::SetOxygen(const Value& expected, float value, Receipt* output) const noexcept
    {
        namespace D = PedOxygenDetail;
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
        D::Scope    scope;
        D::Snapshot first, second;
        auto        result = D::Capture(binding, owner, expected, first);
        if (result != ClockResult::Ok || (result = D::Capture(binding, owner, expected, second)) != ClockResult::Ok)
            return finish(result);
        receipt.nativePed = expected.nativePed;
        receipt.playerData = first.data;
        receipt.address = first.data + D::P::BreathOffset;
        receipt.original = first.bits;
        receipt.observed = second.bits;
        receipt.observedValid = true;
        if (first.data != second.data || first.bits != second.bits)
            return finish(ClockResult::OwnerConflict);
        std::uintptr_t finalData = 0;
        if ((result = D::Context(binding, owner, expected, finalData)) != ClockResult::Ok)
            return finish(result);
        if (finalData != first.data)
            return finish(ClockResult::BindingChanged);
        if (!NativeImageAccess::ImagePages(binding.image, receipt.address, sizeof(receipt.target), true))
            return finish(ClockResult::WriteFailure);
#if defined(MTA_DE_PED_OXYGEN_EXCHANGE)
        const auto exchange = &MTA_DE_PED_OXYGEN_EXCHANGE;
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
        D::Snapshot after;
        if ((result = D::Capture(binding, owner, expected, after)) != ClockResult::Ok)
            return finish(result);
        receipt.observed = after.bits;
        if (after.data != first.data || after.bits != receipt.target)
            return finish(ClockResult::OwnerConflict);
        if ((result = D::Context(binding, owner, expected, finalData)) != ClockResult::Ok)
            return finish(result);
        if (finalData != first.data)
            return finish(ClockResult::BindingChanged);
        receipt.verified = true;
        return finish(ClockResult::Ok);
    }
}
