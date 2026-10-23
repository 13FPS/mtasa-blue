/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Commit exact MTA ped position intent without fabricating world effects
 ****************************************************************************/
#include "NativePedPlacementMemory.h"
#include "NativePedPoseMutation.h"
#include <game/CEntityPlacementPolicy.h>
#include <windows.h>
#include <cstring>
#include <limits>
#include <type_traits>

namespace mta::game_de
{
    namespace PedPlacementDetail
    {
        using Bits = std::array<std::uint32_t, 3>;
        static_assert(std::is_standard_layout_v<CVector> && std::is_trivially_copyable_v<CVector> && sizeof(CVector) == 12 && alignof(CVector) == 4);
        static_assert(offsetof(CVector, fX) == 0 && offsetof(CVector, fY) == 4 && offsetof(CVector, fZ) == 8);
        bool Writable(std::uintptr_t address, std::size_t bytes) noexcept
        {
            if (!address || !bytes || bytes > (std::numeric_limits<std::uintptr_t>::max)() - address)
                return false;
            const auto     end = address + bytes;
            std::uintptr_t allocation = 0;
            while (address < end)
            {
                MEMORY_BASIC_INFORMATION page{};
                if (::VirtualQuery(reinterpret_cast<void*>(address), &page, sizeof(page)) != sizeof(page) || page.State != MEM_COMMIT ||
                    (page.Type != MEM_PRIVATE && page.Type != MEM_MAPPED) || (page.Protect & (PAGE_GUARD | PAGE_NOACCESS)) ||
                    (page.Protect & 0xff) != PAGE_READWRITE)
                    return false;
                const auto base = reinterpret_cast<std::uintptr_t>(page.BaseAddress);
                const auto current = reinterpret_cast<std::uintptr_t>(page.AllocationBase);
                if (!current || (allocation && allocation != current) || base > address ||
                    page.RegionSize > (std::numeric_limits<std::uintptr_t>::max)() - base || base + page.RegionSize <= address)
                    return false;
                allocation = current;
                address = base + page.RegionSize < end ? base + page.RegionSize : end;
            }
            return true;
        }
        bool SamePed(const NativePedPoolReader::Value& a, const NativePedPoolReader::Value& b) noexcept
        {
            return a.handle == b.handle && a.slot == b.slot && a.pool == b.pool && a.objects == b.objects && a.flags == b.flags && a.nativePed == b.nativePed &&
                   a.vtable == b.vtable && a.firstMethod == b.firstMethod && a.capacity == b.capacity && a.cursor == b.cursor && a.generation == b.generation &&
                   a.ownsAllocations == b.ownsAllocations;
        }
        struct Snapshot
        {
            NativePedTransformReader::Value transform;
            NativePedMetadata               metadata;
            Bits                            bits{};
        };
        ClockResult Capture(NativePedPlacementMemory::Binding binding, NativePedPlacementMemory::CheckOwner owner,
                            const NativePedPlacementMemory::Ped& expected, Snapshot& output) noexcept
        {
            NativePedMetadata       before, after;
            NativePedMetadataMemory metadata(binding, owner);
            auto                    result = metadata.Read(expected, before);
            if (result != ClockResult::Ok)
                return result;
            // CEntitySA's six numeric model cases call an additional native
            // rail recalculation even from this inherited method. Its absence
            // is a pre-write refusal, never a guessed model-classification rule.
            if (MtaGame::CEntityPlacementPolicy::RequiresTrainRailRecalculation(before.modelIndex))
                return ClockResult::UnsupportedBuild;
            Snapshot next;
            result = NativePedTransformReader(binding, owner).Read(expected.handle, next.transform);
            if (result != ClockResult::Ok)
                return result;
            if (!SamePed(expected, next.transform.ped))
                return ClockResult::BindingChanged;
            result = metadata.Read(next.transform.ped, after);
            if (result != ClockResult::Ok)
                return result;
            if (before.modelIndex != after.modelIndex || before.entityType != after.entityType)
                return ClockResult::BindingChanged;
            next.metadata = after;
            std::memcpy(next.bits.data(), &next.transform.position, sizeof(CVector));
            output = next;
            return ClockResult::Ok;
        }
        bool SameBinding(const Snapshot& original, const Snapshot& current) noexcept
        {
            return SamePed(original.transform.ped, current.transform.ped) && original.transform.matrix == current.transform.matrix &&
                   original.transform.positionAddress == current.transform.positionAddress && original.metadata.modelIndex == current.metadata.modelIndex &&
                   original.metadata.entityType == current.metadata.entityType;
        }
        bool Source(const CVector& value, Bits& output) noexcept
        {
            Bits   first{}, second{};
            SIZE_T copied = 0;
            // Copy before writing any component: a caller may pass the actual
            // current position or an overlapping native vector as its source.
            if (!ReadProcessMemory(GetCurrentProcess(), &value, first.data(), sizeof(CVector), &copied) || copied != sizeof(CVector) ||
                !ReadProcessMemory(GetCurrentProcess(), &value, second.data(), sizeof(CVector), &copied) || copied != sizeof(CVector) || first != second)
                return false;
            output = second;
            return true;
        }
#if !defined(MTA_DE_PED_PLACEMENT_EXCHANGE)
        bool Exchange(std::uintptr_t address, std::uint32_t expected, std::uint32_t target, std::uint32_t& observed, std::uint32_t& exception) noexcept
        {
            LONG before = 0, after = 0, previous = 0;
            static_assert(sizeof(LONG) == sizeof(expected));
            std::memcpy(&before, &expected, sizeof(before));
            std::memcpy(&after, &target, sizeof(after));
            __try
            {
                previous = InterlockedCompareExchange(reinterpret_cast<volatile LONG*>(address), after, before);
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                exception = GetExceptionCode();
                return false;
            }
            std::memcpy(&observed, &previous, sizeof(previous));
            return true;
        }
#endif
    }
    bool NativePedPlacementMemory::Qualify(Binding& output) noexcept
    {
        Binding next;
        if (!NativePedTransformReader::Qualify(next) || !Revalidate(next))
            return false;
        output = next;
        return true;
    }
    bool NativePedPlacementMemory::Revalidate(const Binding& binding) noexcept
    {
        return NativePedTransformReader::Revalidate(binding) && NativePedMetadataMemory::Revalidate(binding);
    }
    ClockResult NativePedPlacementMemory::Set(const Ped& expected, const CVector& source, Receipt* output) const noexcept
    {
        namespace D = PedPlacementDetail;
        Receipt receipt;
        receipt.handle = expected.handle;
        const auto finish = [&](ClockResult result)
        {
            if (output)
                *output = receipt;
            return result;
        };
        NativePedPoseMutation scope;
        if (!scope)
            return finish(ClockResult::Busy);
        if (!owner)
            return finish(ClockResult::NotInitialized);
        auto result = owner();
        if (result != ClockResult::Ok)
            return finish(result);
        if (!Revalidate(binding))
            return finish(ClockResult::UnsupportedBuild);
        if (!D::Source(source, receipt.target))
            return finish(ClockResult::ReadFailure);
        CVector requested;
        std::memcpy(&requested, receipt.target.data(), sizeof(requested));
        if (!MtaGame::CEntityPlacementPolicy::IsFinite(requested))
            return finish(ClockResult::InvalidArgument);
        D::Snapshot original, current;
        result = D::Capture(binding, owner, expected, original);
        if (result != ClockResult::Ok)
            return finish(result);
        receipt.nativePed = original.transform.ped.nativePed;
        receipt.matrix = original.transform.matrix;
        receipt.address = original.transform.positionAddress;
        receipt.modelIndex = original.metadata.modelIndex;
        receipt.original = original.bits;
        if (receipt.address % alignof(std::uint32_t))
            return finish(ClockResult::BindingChanged);
        for (unsigned index = 0; index != 3; ++index)
        {
            result = D::Capture(binding, owner, expected, current);
            if (result != ClockResult::Ok)
                return finish(result);
            if (!D::SameBinding(original, current))
                return finish(ClockResult::BindingChanged);
            receipt.observed = current.bits;
            receipt.observedMask = 7;
            for (unsigned component = 0; component != 3; ++component)
            {
                const auto wanted = component < index ? receipt.target[component] : receipt.original[component];
                if (current.bits[component] != wanted)
                    receipt.foreignMask |= static_cast<std::uint8_t>(1u << component);
            }
            if (receipt.foreignMask)
                return finish(ClockResult::OwnerConflict);
            const auto bit = static_cast<std::uint8_t>(1u << index);
            if (receipt.original[index] == receipt.target[index])
            {
                receipt.unchangedMask |= bit;
                continue;
            }
            if (!D::Writable(receipt.address, sizeof(CVector)))
                return finish(ClockResult::WriteFailure);
            // No guessed vslot is invoked. Each final scalar CAS owns exactly
            // one existing XYZ DWORD; matrix/ped bookends remain necessary and
            // are not elevated to a native lifetime lock between instructions.
            std::uint32_t observed = 0;
            receipt.attemptedMask |= bit;
            receipt.entered = true;
#if defined(MTA_DE_PED_PLACEMENT_EXCHANGE)
            const bool acknowledged =
                MTA_DE_PED_PLACEMENT_EXCHANGE(receipt.address + index * 4, receipt.original[index], receipt.target[index], observed, receipt.exceptionCode);
#else
            const bool acknowledged = D::Exchange(receipt.address + index * 4, receipt.original[index], receipt.target[index], observed, receipt.exceptionCode);
#endif
            if (!acknowledged)
                return finish(ClockResult::WriteFailure);
            if (observed != receipt.original[index])
            {
                receipt.foreignMask |= bit;
                return finish(ClockResult::OwnerConflict);
            }
            receipt.appliedMask |= bit;
        }
        result = D::Capture(binding, owner, expected, current);
        if (result != ClockResult::Ok)
            return finish(result);
        if (!D::SameBinding(original, current))
            return finish(ClockResult::BindingChanged);
        receipt.observed = current.bits;
        receipt.observedMask = 7;
        for (unsigned component = 0; component != 3; ++component)
            if (current.bits[component] != receipt.target[component])
                receipt.foreignMask |= static_cast<std::uint8_t>(1u << component);
        if (receipt.foreignMask)
            return finish(ClockResult::OwnerConflict);
        receipt.verified = true;
        return finish(ClockResult::Ok);
    }
}
