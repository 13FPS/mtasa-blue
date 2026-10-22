/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Preserve raw independent ammo DWORD writes on a qualified ped-owned slot
 ****************************************************************************/
#include "NativePedWeaponAmmoMemory.h"
#include "NativePedWeaponAmmoProfile.h"
#include "NativeImageAccess.h"
#include <array>
#include <cstring>
#include <intrin.h>
#include <limits>
namespace mta::game_de
{
    namespace PedWeaponAmmoDetail
    {
        namespace P = NativePedWeaponAmmoProfile;
        using Memory = NativePedWeaponAmmoMemory;
        using Context = NativePedWeaponContext;
        static_assert(sizeof(LONG) == sizeof(std::uint32_t));
        ClockResult Capture(Memory::Binding binding, Memory::CheckOwner owner, const Context::Value& expected, std::size_t field,
                            std::uint32_t& output) noexcept
        {
            if (!Memory::Revalidate(binding))
                return ClockResult::BindingChanged;
            Context context(binding, owner);
            auto    result = context.Recheck(expected);
            if (result != ClockResult::Ok)
                return result;
            if (expected.nativeWeapon > (std::numeric_limits<std::uintptr_t>::max)() - field)
                return ClockResult::ReadFailure;
            const auto    at = expected.nativeWeapon + field;
            std::uint32_t value = 0;
            SIZE_T        copied = 0;
            if (!Context::Writable(at, sizeof(value)) || !ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(at), &value, sizeof(value), &copied) ||
                copied != sizeof(value))
                return ClockResult::ReadFailure;
            if ((result = context.Recheck(expected)) != ClockResult::Ok)
                return result;
            output = value;
            return ClockResult::Ok;
        }
#if !defined(MTA_DE_PED_WEAPON_AMMO_EXCHANGE)
        bool Exchange(std::uintptr_t at, std::uint32_t expected, std::uint32_t target, std::uint32_t& observed, std::uint32_t& exception) noexcept
        {
            LONG original = 0, next = 0;
            std::memcpy(&original, &expected, sizeof(original));
            std::memcpy(&next, &target, sizeof(next));
            __try
            {
                const auto previous = InterlockedCompareExchange(reinterpret_cast<volatile LONG*>(at), next, original);
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
    bool NativePedWeaponAmmoMemory::Qualify(Binding& output) noexcept
    {
        Binding next;
        if (!NativePedWeaponContext::Qualify(next) || !Revalidate(next))
            return false;
        output = next;
        return true;
    }
    bool NativePedWeaponAmmoMemory::Revalidate(const Binding& binding) noexcept
    {
        namespace P = NativePedWeaponAmmoProfile;
        namespace I = NativeImageAccess;
        if (!NativePedWeaponContext::Revalidate(binding))
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
    ClockResult NativePedWeaponAmmoMemory::Read(const Ped& ped, std::uint32_t slot, std::size_t field, std::uint32_t& output) const noexcept
    {
        namespace D = PedWeaponAmmoDetail;
        NativePedWeaponContext        context(binding, owner);
        NativePedWeaponContext::Value expected;
        auto                          result = context.Capture(ped, slot, expected);
        if (result != ClockResult::Ok)
            return result;
        std::uint32_t first = 0, second = 0;
        if ((result = D::Capture(binding, owner, expected, field, first)) != ClockResult::Ok ||
            (result = D::Capture(binding, owner, expected, field, second)) != ClockResult::Ok)
            return result;
        if (first != second)
            return ClockResult::OwnerConflict;
        if ((result = context.Recheck(expected)) != ClockResult::Ok)
            return result;
        output = second;
        return ClockResult::Ok;
    }
    ClockResult NativePedWeaponAmmoMemory::Set(const Ped& ped, std::uint32_t slot, std::size_t field, std::uint32_t value, Receipt* output) const noexcept
    {
        namespace D = PedWeaponAmmoDetail;
        Receipt receipt;
        receipt.handle = ped.handle;
        receipt.slot = slot;
        receipt.target = value;
        const auto finish = [&](ClockResult result)
        {
            if (output)
                *output = receipt;
            return result;
        };
        NativePedWeaponContext::Mutation weaponMutation;
        if (!weaponMutation)
            return finish(ClockResult::Busy);
        NativePedWeaponContext        context(binding, owner);
        NativePedWeaponContext::Value expected;
        auto                          result = context.Capture(ped, slot, expected);
        if (result != ClockResult::Ok)
            return finish(result);
        std::uint32_t current = 0;
        if ((result = D::Capture(binding, owner, expected, field, receipt.original)) != ClockResult::Ok ||
            (result = D::Capture(binding, owner, expected, field, current)) != ClockResult::Ok)
            return finish(result);
        if (receipt.original != current)
            return finish(ClockResult::OwnerConflict);
        receipt.nativePed = ped.nativePed;
        receipt.nativeWeapon = expected.nativeWeapon;
        receipt.rawType = expected.rawType;
        receipt.address = expected.nativeWeapon + field;
        if (!NativePedWeaponContext::Writable(receipt.address, sizeof(value)))
            return finish(ClockResult::WriteFailure);
        if ((result = context.Recheck(expected)) != ClockResult::Ok)
            return finish(result);
#if defined(MTA_DE_PED_WEAPON_AMMO_EXCHANGE)
        const auto exchange = &MTA_DE_PED_WEAPON_AMMO_EXCHANGE;
#else
        const auto exchange = &D::Exchange;
#endif
        // Actual CWeaponSA writes the raw DWORD independently. The named game
        // opcode's clamp/clip/state effects are deliberately not a substitute.
        receipt.entered = true;
        if (!exchange(receipt.address, current, value, receipt.observed, receipt.exceptionCode))
            return finish(ClockResult::WriteFailure);
        receipt.observedValid = true;
        if (receipt.observed != current)
            return finish(ClockResult::OwnerConflict);
        receipt.accepted = true;
        if ((result = D::Capture(binding, owner, expected, field, receipt.observed)) != ClockResult::Ok)
            return finish(result);
        if (receipt.observed != value)
            return finish(ClockResult::OwnerConflict);
        if ((result = context.Recheck(expected)) != ClockResult::Ok)
            return finish(result);
        receipt.verified = true;
        return finish(ClockResult::Ok);
    }
    ClockResult NativePedWeaponAmmoMemory::ReadAmmoInClip(const Ped& ped, std::uint32_t slot, std::uint32_t& output) const noexcept
    {
        return Read(ped, slot, NativePedWeaponAmmoProfile::ClipOffset, output);
    }
    ClockResult NativePedWeaponAmmoMemory::SetAmmoInClip(const Ped& ped, std::uint32_t slot, std::uint32_t value, Receipt* receipt) const noexcept
    {
        return Set(ped, slot, NativePedWeaponAmmoProfile::ClipOffset, value, receipt);
    }
    ClockResult NativePedWeaponAmmoMemory::ReadAmmoTotal(const Ped& ped, std::uint32_t slot, std::uint32_t& output) const noexcept
    {
        return Read(ped, slot, NativePedWeaponAmmoProfile::TotalOffset, output);
    }
    ClockResult NativePedWeaponAmmoMemory::SetAmmoTotal(const Ped& ped, std::uint32_t slot, std::uint32_t value, Receipt* receipt) const noexcept
    {
        return Set(ped, slot, NativePedWeaponAmmoProfile::TotalOffset, value, receipt);
    }
}
