/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Observe or conditionally write one genuine inline weapon DWORD
 ****************************************************************************/
#include "NativePedWeaponFieldsMemory.h"
#include "NativePedWeaponProfile.h"
#include <cstring>
#include <windows.h>
#include <intrin.h>
namespace mta::game_de
{
    namespace PedWeaponFieldsDetail
    {
        using Context = NativePedWeaponContext;
        bool Word(std::uintptr_t at, std::uint32_t& output) noexcept
        {
            SIZE_T copied = 0;
            return at % alignof(std::uint32_t) == 0 && Context::Writable(at, sizeof(output)) &&
                   ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(at), &output, sizeof(output), &copied) && copied == sizeof(output);
        }
#if !defined(MTA_DE_PED_WEAPON_FIELDS_EXCHANGE)
        bool Exchange(std::uintptr_t at, std::uint32_t expected, std::uint32_t target, std::uint32_t& observed, std::uint32_t& exception) noexcept
        {
            static_assert(sizeof(LONG) == sizeof(std::uint32_t));
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
    ClockResult NativePedWeaponFieldsMemory::Read(const Ped& ped, std::uint32_t slot, std::size_t offset, std::uint32_t& output) const noexcept
    {
        namespace D = PedWeaponFieldsDetail;
        D::Context        context(binding, owner);
        D::Context::Value value;
        auto              result = context.Capture(ped, slot, value);
        if (result != ClockResult::Ok)
            return result;
        std::uint32_t first = 0, second = 0;
        if (!D::Word(value.nativeWeapon + offset, first))
            return ClockResult::ReadFailure;
        if ((result = context.Recheck(value)) != ClockResult::Ok)
            return result;
        if (!D::Word(value.nativeWeapon + offset, second))
            return ClockResult::ReadFailure;
        if (first != second)
            return ClockResult::OwnerConflict;
        if ((result = context.Recheck(value)) != ClockResult::Ok)
            return result;
        output = second;
        return ClockResult::Ok;
    }
    ClockResult NativePedWeaponFieldsMemory::Set(const Ped& ped, std::uint32_t slot, std::size_t offset, std::uint32_t requested,
                                                 Receipt* output) const noexcept
    {
        namespace D = PedWeaponFieldsDetail;
        Receipt receipt;
        receipt.handle = ped.handle;
        receipt.slot = slot;
        receipt.target = requested;
        const auto finish = [&](ClockResult result)
        {
            if (output)
                *output = receipt;
            return result;
        };
        D::Context::Mutation weaponMutation;
        if (!weaponMutation)
            return finish(ClockResult::Busy);
        D::Context        context(binding, owner);
        D::Context::Value value;
        auto              result = context.Capture(ped, slot, value);
        if (result != ClockResult::Ok)
            return finish(result);
        receipt.rawType = value.rawType;
        receipt.nativePed = ped.nativePed;
        receipt.nativeWeapon = value.nativeWeapon;
        receipt.address = value.nativeWeapon + offset;
        if ((result = Read(ped, slot, offset, receipt.original)) != ClockResult::Ok)
            return finish(result);
        if ((result = context.Recheck(value)) != ClockResult::Ok)
            return finish(result);
        receipt.observed = receipt.original;
        receipt.observedValid = true;
        if (!D::Context::Writable(receipt.address, sizeof(std::uint32_t)))
            return finish(ClockResult::WriteFailure);
#if defined(MTA_DE_PED_WEAPON_FIELDS_EXCHANGE)
        const auto exchange = &MTA_DE_PED_WEAPON_FIELDS_EXCHANGE;
#else
        const auto exchange = &D::Exchange;
#endif
        receipt.entered = true;
        std::uint32_t previous = 0;
        if (!exchange(receipt.address, receipt.original, requested, previous, receipt.exceptionCode))
            return finish(ClockResult::WriteFailure);
        receipt.observed = previous;
        if (previous != receipt.original)
            return finish(ClockResult::OwnerConflict);
        receipt.accepted = true;
        // A raw type store changes the immediate slot identity. Only its
        // explicit new value is accepted by the post-write type bookend.
        const auto expectedType = offset == NativePedWeaponProfile::TypeOffset ? requested : value.rawType;
        if ((result = context.Recheck(value, expectedType)) != ClockResult::Ok)
            return finish(result);
        if (!D::Word(receipt.address, receipt.observed))
            return finish(ClockResult::ReadFailure);
        if (receipt.observed != requested)
            return finish(ClockResult::OwnerConflict);
        if ((result = context.Recheck(value, expectedType)) != ClockResult::Ok)
            return finish(result);
        receipt.verified = true;
        return finish(ClockResult::Ok);
    }
    ClockResult NativePedWeaponFieldsMemory::ReadType(const Ped& ped, std::uint32_t slot, std::uint32_t& output) const noexcept
    {
        return Read(ped, slot, NativePedWeaponProfile::TypeOffset, output);
    }
    ClockResult NativePedWeaponFieldsMemory::SetType(const Ped& ped, std::uint32_t slot, std::uint32_t value, Receipt* output) const noexcept
    {
        return Set(ped, slot, NativePedWeaponProfile::TypeOffset, value, output);
    }
    ClockResult NativePedWeaponFieldsMemory::ReadState(const Ped& ped, std::uint32_t slot, std::uint32_t& output) const noexcept
    {
        return Read(ped, slot, NativePedWeaponProfile::StateOffset, output);
    }
    ClockResult NativePedWeaponFieldsMemory::SetState(const Ped& ped, std::uint32_t slot, std::uint32_t value, Receipt* output) const noexcept
    {
        return Set(ped, slot, NativePedWeaponProfile::StateOffset, value, output);
    }
}
