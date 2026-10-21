/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Preserve raw CPed vitals and honest persistent scalar effects
 ****************************************************************************/
#include "NativePedVitalsMemory.h"
#include "NativePedVitalsProfile.h"
#include "NativeImageAccess.h"
#include "NativeStartGameCalls.h"
#include "NativeGameStateMemory.h"
#include <cstring>
#include <limits>
#include <intrin.h>

namespace mta::game_de
{
    namespace PedVitalsDetail
    {
        namespace P = NativePedVitalsProfile;
        namespace Image = NativeImageAccess;
        using Memory = NativePedVitalsMemory;
        using Value = NativePedPoolReader::Value;
        thread_local bool writing = false;
        struct WriteScope
        {
            WriteScope() noexcept { writing = true; }
            ~WriteScope() { writing = false; }
        };
        static_assert(sizeof(float) == sizeof(std::uint32_t) && sizeof(LONG) == sizeof(std::uint32_t));

        bool Field(NativePedVitalField field, std::size_t& offset) noexcept
        {
            switch (field)
            {
                case NativePedVitalField::Health:
                    offset = P::HealthOffset;
                    return true;
                case NativePedVitalField::Armor:
                    offset = P::ArmorOffset;
                    return true;
                default:
                    return false;
            }
        }
        bool SamePed(const Value& a, const Value& b) noexcept
        {
            return a.handle == b.handle && a.slot == b.slot && a.pool == b.pool && a.objects == b.objects && a.flags == b.flags && a.nativePed == b.nativePed &&
                   a.vtable == b.vtable && a.firstMethod == b.firstMethod && a.capacity == b.capacity && a.cursor == b.cursor && a.generation == b.generation &&
                   a.ownsAllocations == b.ownsAllocations;
        }
        bool Pages(std::uintptr_t address, std::size_t bytes) noexcept
        {
            if (!address || !bytes || bytes > (std::numeric_limits<std::uintptr_t>::max)() - address)
                return false;
            const auto     end = address + bytes;
            std::uintptr_t allocation = 0;
            while (address < end)
            {
                MEMORY_BASIC_INFORMATION info{};
                if (VirtualQuery(reinterpret_cast<void*>(address), &info, sizeof(info)) != sizeof(info) || info.State != MEM_COMMIT ||
                    (info.Type != MEM_PRIVATE && info.Type != MEM_MAPPED) || (info.Protect & (PAGE_GUARD | PAGE_NOACCESS)) ||
                    (info.Protect & 0xff) != PAGE_READWRITE)
                    return false;
                const auto base = reinterpret_cast<std::uintptr_t>(info.BaseAddress);
                const auto current = reinterpret_cast<std::uintptr_t>(info.AllocationBase);
                if (!current || (allocation && allocation != current) || base > address ||
                    info.RegionSize > (std::numeric_limits<std::uintptr_t>::max)() - base || base + info.RegionSize <= address)
                    return false;
                allocation = current;
                address = base + info.RegionSize < end ? base + info.RegionSize : end;
            }
            return true;
        }
        template <class T>
        bool Object(std::uintptr_t address, T& output) noexcept
        {
            T      next{};
            SIZE_T copied = 0;
            if (!Pages(address, sizeof(next)) || !ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(address), &next, sizeof(next), &copied) ||
                copied != sizeof(next))
                return false;
            output = next;
            return true;
        }
        ClockResult Owner(Memory::Binding binding, Memory::CheckOwner owner) noexcept
        {
            if (!owner)
                return ClockResult::NotInitialized;
            auto result = owner();
            if (result != ClockResult::Ok)
                return result;
            result = NativeStartGameCalls::CheckThread(binding.image);
            if (result != ClockResult::Ok)
                return result;
            std::uint32_t state = 0;
            if (!NativeGameStateMemory::Read(binding.image, state))
                return ClockResult::BindingChanged;
            if (state != 9)
                return ClockResult::NotInitialized;
            return owner();
        }
        ClockResult Identity(Memory::Binding binding, Memory::CheckOwner owner, const Value& expected) noexcept
        {
            auto result = Owner(binding, owner);
            if (result != ClockResult::Ok)
                return result;
            if (!Memory::Revalidate(binding))
                return ClockResult::BindingChanged;
            // The common native CPed constructor proves these fields on its
            // own generic receiver; the exact player class inherits that same
            // base layout. Neither route needs a CURRENT-player substitution.
            if (expected.vtable != binding.image + P::PedVtableRva && expected.vtable != binding.image + P::PlayerVtableRva)
                return ClockResult::UnsupportedBuild;
            NativePedPoolReader pool(binding, owner);
            Value               current;
            if ((result = pool.Resolve(expected.handle, current)) != ClockResult::Ok)
                return result;
            if (!SamePed(expected, current))
                return ClockResult::BindingChanged;
            if ((result = pool.Recheck(current)) != ClockResult::Ok)
                return result;
            return Owner(binding, owner);
        }
        struct Snapshot
        {
            std::uint32_t bits = 0;
            unsigned char type = 0;
        };
        ClockResult Capture(Memory::Binding binding, Memory::CheckOwner owner, const Value& expected, std::size_t offset, Snapshot& output) noexcept
        {
            auto result = Identity(binding, owner, expected);
            if (result != ClockResult::Ok)
                return result;
            Snapshot      next;
            unsigned char after = 0;
            if (!Object(expected.nativePed + P::TypeOffset, next.type) || !Object(expected.nativePed + offset, next.bits) ||
                !Object(expected.nativePed + P::TypeOffset, after))
                return ClockResult::ReadFailure;
            if ((next.type & 7) != 3 || next.type != after)
                return ClockResult::BindingChanged;
            if ((result = Identity(binding, owner, expected)) != ClockResult::Ok)
                return result;
            output = next;
            return ClockResult::Ok;
        }
#if !defined(MTA_DE_PED_VITALS_EXCHANGE)
        bool Exchange(std::uintptr_t address, std::uint32_t expected, std::uint32_t target, std::uint32_t& observed, std::uint32_t& exception) noexcept
        {
            LONG before = 0, after = 0, result = 0;
            std::memcpy(&before, &expected, sizeof(before));
            std::memcpy(&after, &target, sizeof(after));
            __try
            {
                result = InterlockedCompareExchange(reinterpret_cast<volatile LONG*>(address), after, before);
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                exception = GetExceptionCode();
                return false;
            }
            std::memcpy(&observed, &result, sizeof(observed));
            return true;
        }
#endif
    }

    bool NativePedVitalsMemory::Qualify(Binding& output) noexcept
    {
        Binding next;
        if (!NativePedPoolReader::Qualify(next) || !Revalidate(next))
            return false;
        output = next;
        return true;
    }
    bool NativePedVitalsMemory::Revalidate(const Binding& binding) noexcept
    {
        namespace Image = NativeImageAccess;
        if (!NativePedPoolReader::Revalidate(binding))
            return false;
        IMAGE_DOS_HEADER   dos{};
        IMAGE_NT_HEADERS64 nt{};
        if (!Image::ReadImage(binding.image, binding.image, &dos, sizeof(dos)) || dos.e_magic != IMAGE_DOS_SIGNATURE || dos.e_lfanew <= 0 ||
            static_cast<std::size_t>(dos.e_lfanew) > NativePedPoolProfile::ImageSize - sizeof(nt) ||
            !Image::ReadImage(binding.image, binding.image + static_cast<std::uintptr_t>(dos.e_lfanew), &nt, sizeof(nt)) ||
            nt.Signature != IMAGE_NT_SIGNATURE || nt.FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64 ||
            nt.OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC || nt.OptionalHeader.SizeOfImage != NativePedPoolProfile::ImageSize ||
            nt.FileHeader.SizeOfOptionalHeader < sizeof(IMAGE_OPTIONAL_HEADER64) || !nt.FileHeader.NumberOfSections || nt.FileHeader.NumberOfSections > 96)
            return false;
        const auto at = static_cast<std::uintptr_t>(dos.e_lfanew) + offsetof(IMAGE_NT_HEADERS64, OptionalHeader) + nt.FileHeader.SizeOfOptionalHeader;
        const auto bytes = static_cast<std::size_t>(nt.FileHeader.NumberOfSections) * sizeof(IMAGE_SECTION_HEADER);
        std::array<IMAGE_SECTION_HEADER, 96> sections{};
        if (at > NativePedPoolProfile::ImageSize || bytes > NativePedPoolProfile::ImageSize - at ||
            !Image::ReadImage(binding.image, binding.image + at, sections.data(), bytes))
            return false;
        for (const auto& pin : NativePedVitalsProfile::Code)
        {
            bool contained = false;
            for (std::size_t i = 0; i != nt.FileHeader.NumberOfSections; ++i)
                if (Image::SectionContains(sections[i], pin.rva, pin.size, IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_EXECUTE, IMAGE_SCN_MEM_WRITE))
                    contained = true;
            std::array<unsigned char, 160> actual{};
            if (!contained || !pin.size || pin.size > actual.size() ||
                !Image::ReadImage(binding.image, binding.image + pin.rva, actual.data(), pin.size, false, true) ||
                std::memcmp(actual.data(), pin.bytes.data(), pin.size))
                return false;
        }
        return true;
    }
    ClockResult NativePedVitalsMemory::Read(const Value& expected, NativePedVitalField field, float& output) const noexcept
    {
        namespace D = PedVitalsDetail;
        std::size_t offset = 0;
        if (!D::Field(field, offset))
            return ClockResult::InvalidArgument;
        D::Snapshot first, second;
        auto        result = D::Capture(binding, owner, expected, offset, first);
        if (result != ClockResult::Ok || (result = D::Capture(binding, owner, expected, offset, second)) != ClockResult::Ok)
            return result;
        if (first.type != second.type || first.bits != second.bits)
            return ClockResult::OwnerConflict;
        if ((result = D::Owner(binding, owner)) != ClockResult::Ok)
            return result;
        // CPed's existing SDK accessors copy raw float state, including NaN
        // payloads and signed zero. Arithmetic comparisons would change that
        // contract or manufacture a plausible clamped health/armour value.
        std::memcpy(&output, &second.bits, sizeof(output));
        return ClockResult::Ok;
    }
    ClockResult NativePedVitalsMemory::Set(const Value& expected, NativePedVitalField field, float value, Receipt* receiptOutput) const noexcept
    {
        namespace D = PedVitalsDetail;
        Receipt receipt;
        receipt.field = field;
        receipt.handle = expected.handle;
        std::memcpy(&receipt.target, &value, sizeof(value));
        const auto finish = [&](ClockResult result)
        {
            if (receiptOutput)
                *receiptOutput = receipt;
            return result;
        };
        if (D::writing)
            return finish(ClockResult::Busy);
        std::size_t offset = 0;
        if (!D::Field(field, offset))
            return finish(ClockResult::InvalidArgument);
        D::WriteScope scope;
        D::Snapshot   first, second;
        auto          result = D::Capture(binding, owner, expected, offset, first);
        if (result != ClockResult::Ok)
            return finish(result);
        receipt.original = first.bits;
        receipt.nativePed = expected.nativePed;
        receipt.address = expected.nativePed + offset;
        if ((result = D::Capture(binding, owner, expected, offset, second)) != ClockResult::Ok)
            return finish(result);
        receipt.observed = second.bits;
        receipt.observedValid = true;
        if (first.type != second.type || first.bits != second.bits)
            return finish(ClockResult::OwnerConflict);
        if (receipt.address % alignof(std::uint32_t) || !D::Pages(receipt.address, sizeof(std::uint32_t)))
            return finish(ClockResult::WriteFailure);
        if ((result = D::Identity(binding, owner, expected)) != ClockResult::Ok)
            return finish(result);
#if defined(MTA_DE_PED_VITALS_EXCHANGE)
        const auto exchange = &MTA_DE_PED_VITALS_EXCHANGE;
#else
        const auto exchange = &D::Exchange;
#endif
        // One exact scalar CAS implements persistent caller intent. Even a
        // failed acknowledgement can follow a committed store: mark entry
        // first and never retry, resurrect, or restore an old native snapshot.
        receipt.entered = receipt.attempted = true;
        std::uint32_t previous = 0;
        if (!exchange(receipt.address, receipt.original, receipt.target, previous, receipt.exceptionCode))
            return finish(ClockResult::WriteFailure);
        receipt.observed = previous;
        receipt.observedValid = true;
        if (previous != receipt.original)
            return finish(ClockResult::OwnerConflict);
        receipt.applied = true;
        D::Snapshot final;
        if ((result = D::Capture(binding, owner, expected, offset, final)) != ClockResult::Ok)
            return finish(result);
        receipt.observed = final.bits;
        if (final.type != first.type || final.bits != receipt.target)
            return finish(ClockResult::OwnerConflict);
        if ((result = D::Owner(binding, owner)) != ClockResult::Ok)
            return finish(result);
        receipt.verified = true;
        return finish(ClockResult::Ok);
    }
}
