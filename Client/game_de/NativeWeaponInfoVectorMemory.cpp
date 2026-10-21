/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Preserve native weapon vector aliases and exact conditional writes
 *****************************************************************************/
#include "NativeWeaponInfoVectorMemory.h"
#include "NativeWeaponInfoVectorProfile.h"
#include "NativeImageAccess.h"
#include <cstring>
#include <type_traits>

namespace mta::game_de::NativeWeaponInfoVectorMemory
{
    // This is the actual shared MTA type, not a second guessed vector layout.
    // The native loader independently stores XY at+0x24 and Z at+0x2c.
    static_assert(std::is_standard_layout_v<CVector> && std::is_trivially_copyable_v<CVector>);
    static_assert(sizeof(CVector) == 12 && alignof(CVector) == 4 && offsetof(CVector, fX) == 0 && offsetof(CVector, fY) == 4 && offsetof(CVector, fZ) == 8);
    static_assert(offsetof(NativeWeaponInfoValues, fireOffset) == 0x24 && sizeof(NativeWeaponInfoValues::fireOffset) == sizeof(CVector));

    namespace
    {
        namespace Profile = NativeWeaponInfoProfile;
        namespace VectorProfile = NativeWeaponInfoVectorProfile;
        using CheckOwner = NativeWeaponInfoWriteMemory::CheckOwner;

        ClockResult ReadStatus(Status value) noexcept
        {
            switch (value)
            {
                case Status::Ok:
                    return ClockResult::Ok;
                case Status::InvalidArgument:
                    return ClockResult::InvalidArgument;
                case Status::NotInitialized:
                    return ClockResult::NotInitialized;
                case Status::WrongThread:
                    return ClockResult::WrongThread;
                default:
                    return ClockResult::BindingChanged;
            }
        }
        ClockResult Admission(std::uintptr_t image, CheckOwner checkOwner) noexcept
        {
            if (!image || image != reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr)))
                return ClockResult::BindingChanged;
            return checkOwner ? checkOwner() : ClockResult::Ok;
        }
        ClockResult QualifiedRow(const NativeWeaponInfoReader& reader, std::uintptr_t image, std::size_t slot, NativeWeaponInfoValues& output) noexcept
        {
            if (slot >= Profile::RowCount)
                return ClockResult::InvalidArgument;
            if (!image || image != reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr)))
                return ClockResult::BindingChanged;
            const auto result = ReadStatus(reader.Read(slot, output));
            if (result != ClockResult::Ok)
                return result;
            for (const auto& pin : VectorProfile::Code)
            {
                std::array<unsigned char, 160> actual{};
                if (!NativeImageAccess::ReadImage(image, image + pin.rva, actual.data(), pin.size, false, true) ||
                    std::memcmp(actual.data(), pin.bytes.data(), pin.size))
                    return ClockResult::BindingChanged;
            }
            return Admission(image, nullptr);
        }
        bool Exchange(std::uintptr_t address, std::uint32_t expected, std::uint32_t target, std::uint32_t& previous) noexcept
        {
            LONG before = 0, after = 0, result = 0;
            static_assert(sizeof(LONG) == sizeof(expected));
            std::memcpy(&before, &expected, sizeof(before));
            std::memcpy(&after, &target, sizeof(after));
            __try
            {
                result = InterlockedCompareExchange(reinterpret_cast<volatile LONG*>(address), after, before);
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                return false;
            }
            std::memcpy(&previous, &result, sizeof(previous));
            return true;
        }
        std::array<std::uint32_t, 3> FireBits(const NativeWeaponInfoValues& row) noexcept
        {
            std::array<std::uint32_t, 3> output;
            std::memcpy(output.data(), row.fireOffset, sizeof(row.fireOffset));
            return output;
        }
        bool SourceBits(const CVector* source, std::array<std::uint32_t, 3>& output) noexcept
        {
            std::array<std::uint32_t, 3> first, second;
            SIZE_T                       copied = 0;
            // Read the caller's complete vector before any destination store.
            // This preserves SDK self/overlap aliasing and refuses unreadable
            // caller pointers through the OS rather than dereferencing them.
            if (!ReadProcessMemory(GetCurrentProcess(), source, first.data(), sizeof(CVector), &copied) || copied != sizeof(CVector))
                return false;
            copied = 0;
            if (!ReadProcessMemory(GetCurrentProcess(), source, second.data(), sizeof(CVector), &copied) || copied != sizeof(CVector) || first != second)
                return false;
            output = second;
            return true;
        }
        ClockResult RetirePartial(const NativeWeaponInfoReader& reader, std::uintptr_t image, std::size_t slot, CheckOwner checkOwner,
                                  FireOffsetJournal& journal) noexcept
        {
            for (unsigned next = 3; next != 0; --next)
            {
                const auto index = next - 1;
                const auto bit = static_cast<std::uint8_t>(1u << index);
                if (!(journal.appliedMask & bit) || ((journal.restoredMask | journal.foreignMask) & bit))
                    continue;
                NativeWeaponInfoValues row;
                if (QualifiedRow(reader, image, slot, row) != ClockResult::Ok)
                    continue;
                const auto current = FireBits(row)[index];
                if (current == journal.original[index])
                {
                    // A prior exact rollback may have committed before a
                    // failed verification copy. Original bytes carry no debt.
                    journal.restoredMask |= bit;
                    continue;
                }
                if (current != journal.target[index])
                {
                    journal.foreignMask |= bit;
                    continue;
                }
                if (Admission(image, checkOwner) != ClockResult::Ok)
                    continue;
                const auto    address = image + Profile::BankRva + slot * Profile::RowBytes + 0x24 + index * sizeof(std::uint32_t);
                std::uint32_t previous = 0;
                if (!Exchange(address, journal.target[index], journal.original[index], previous))
                    continue;
                if (previous != journal.target[index])
                {
                    if (previous == journal.original[index])
                        journal.restoredMask |= bit;
                    else
                        journal.foreignMask |= bit;
                    continue;
                }
                if (QualifiedRow(reader, image, slot, row) != ClockResult::Ok || Admission(image, checkOwner) != ClockResult::Ok)
                    continue;
                if (FireBits(row)[index] == journal.original[index])
                    journal.restoredMask |= bit;
                else
                    journal.foreignMask |= bit;
            }
            journal.remainingMask = static_cast<std::uint8_t>(journal.appliedMask & ~(journal.restoredMask | journal.foreignMask));
            return journal.remainingMask ? ClockResult::RestoreFailure : journal.failure;
        }
    }

    ClockResult BorrowFireOffset(const NativeWeaponInfoReader& reader, std::uintptr_t image, std::size_t slot, CVector*& output,
                                 NativeWeaponInfoWriteMemory::CheckOwner checkOwner) noexcept
    {
        NativeWeaponInfoValues row;
        auto                   result = QualifiedRow(reader, image, slot, row);
        if (result != ClockResult::Ok)
            return result;
        const auto address = image + Profile::BankRva + slot * Profile::RowBytes + offsetof(NativeWeaponInfoValues, fireOffset);
        if (address % alignof(CVector) || !NativeImageAccess::ImagePages(image, address, sizeof(CVector), true))
            return ClockResult::BindingChanged;
        result = Admission(image, checkOwner);
        if (result != ClockResult::Ok)
            return result;
        // Main-image storage is never owned or freed by this wrapper. This is
        // the SDK's genuine borrowed native alias, with no copied backing row.
        output = reinterpret_cast<CVector*>(address);
        return ClockResult::Ok;
    }

    ClockResult SetSlot(const NativeWeaponInfoReader& reader, std::uintptr_t image, std::size_t slot, eWeaponSlot value,
                        NativeWeaponInfoWriteMemory::CheckOwner checkOwner) noexcept
    {
        const auto target = static_cast<std::uint32_t>(value);
        if (target >= VectorProfile::SlotCount)
            return ClockResult::InvalidArgument;
        NativeWeaponInfoValues row;
        auto                   result = QualifiedRow(reader, image, slot, row);
        if (result != ClockResult::Ok)
            return result;
        const auto    address = image + Profile::BankRva + slot * Profile::RowBytes + offsetof(NativeWeaponInfoValues, slot);
        std::uint32_t expected = 0, previous = 0;
        std::memcpy(&expected, &row.slot, sizeof(expected));
        result = Admission(image, checkOwner);
        if (result != ClockResult::Ok)
            return result;
        if (!Exchange(address, expected, target, previous))
            return ClockResult::WriteFailure;
        if (previous != expected)
            return ClockResult::Busy;
        result = QualifiedRow(reader, image, slot, row);
        if (result != ClockResult::Ok)
            return ClockResult::WriteFailure;
        result = Admission(image, checkOwner);
        if (result != ClockResult::Ok)
            return result;
        return static_cast<std::uint32_t>(row.slot) == target ? ClockResult::Ok : ClockResult::WriteFailure;
    }

    ClockResult SetFireOffset(const NativeWeaponInfoReader& reader, std::uintptr_t image, std::size_t slot, const CVector* source,
                              NativeWeaponInfoWriteMemory::CheckOwner checkOwner, FireOffsetJournal* journalOutput) noexcept
    {
        FireOffsetJournal journal;
        const auto        fail = [&](ClockResult failure)
        {
            journal.failure = failure;
            const auto result = journal.appliedMask ? RetirePartial(reader, image, slot, checkOwner, journal) : failure;
            if (journalOutput)
                *journalOutput = journal;
            return result;
        };
        if (!source || slot >= Profile::RowCount)
            return fail(ClockResult::InvalidArgument);
        if (!SourceBits(source, journal.target))
            return fail(ClockResult::ReadFailure);
        NativeWeaponInfoValues row;
        auto                   result = QualifiedRow(reader, image, slot, row);
        if (result != ClockResult::Ok)
            return fail(result);
        journal.original = FireBits(row);
        journal.image = image;
        journal.slot = slot;
        const auto base = image + Profile::BankRva + slot * Profile::RowBytes + offsetof(NativeWeaponInfoValues, fireOffset);
        if (base % alignof(CVector) || !NativeImageAccess::ImagePages(image, base, sizeof(CVector), true))
            return fail(ClockResult::BindingChanged);
        for (unsigned index = 0; index != 3; ++index)
        {
            result = QualifiedRow(reader, image, slot, row);
            if (result != ClockResult::Ok)
                return fail(result);
            const auto current = FireBits(row);
            for (unsigned prior = 0; prior != index; ++prior)
                if ((journal.appliedMask & (1u << prior)) && current[prior] != journal.target[prior])
                    return fail(ClockResult::Busy);
            if (current[index] != journal.original[index])
                return fail(ClockResult::Busy);
            result = Admission(image, checkOwner);
            if (result != ClockResult::Ok)
                return fail(result);
            if (journal.original[index] == journal.target[index])
                continue;
            std::uint32_t previous = 0;
            if (!Exchange(base + index * sizeof(std::uint32_t), journal.original[index], journal.target[index], previous))
                return fail(ClockResult::WriteFailure);
            if (previous != journal.original[index])
                return fail(ClockResult::Busy);
            journal.appliedMask |= static_cast<std::uint8_t>(1u << index);
        }
        result = QualifiedRow(reader, image, slot, row);
        if (result != ClockResult::Ok)
            return fail(ClockResult::WriteFailure);
        result = Admission(image, checkOwner);
        if (result != ClockResult::Ok)
            return fail(result);
        if (FireBits(row) != journal.target)
            return fail(ClockResult::WriteFailure);
        journal.committed = true;
        if (journalOutput)
            *journalOutput = journal;
        return ClockResult::Ok;
    }

    ClockResult RecoverFireOffset(const NativeWeaponInfoReader& reader, std::uintptr_t image, std::size_t slot, FireOffsetJournal& journal,
                                  NativeWeaponInfoWriteMemory::CheckOwner checkOwner) noexcept
    {
        if (journal.committed || image != journal.image || slot != journal.slot || slot >= Profile::RowCount)
            return ClockResult::InvalidArgument;
        if (!journal.remainingMask)
            return journal.foreignMask ? ClockResult::OwnerConflict : ClockResult::Ok;
        auto result = Admission(image, checkOwner);
        if (result != ClockResult::Ok)
            return result;
        NativeWeaponInfoValues row;
        result = QualifiedRow(reader, image, slot, row);
        if (result != ClockResult::Ok)
            return result;
        RetirePartial(reader, image, slot, checkOwner, journal);
        return journal.remainingMask ? ClockResult::RestoreFailure : journal.foreignMask ? ClockResult::OwnerConflict : ClockResult::Ok;
    }
}
