/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Typed exact-width CAS and readback for persistent SDK weapon intents
 *****************************************************************************/
#include "NativeWeaponInfoWriteMemory.h"
#include "NativeWeaponInfoProfile.h"
#include "NativeWeaponInfoWriteProfile.h"
#include "NativeImageAccess.h"

#include <array>
#include <cstring>

namespace mta::game_de::NativeWeaponInfoWriteMemory
{
    namespace
    {
        ClockResult ReadResult(Status status) noexcept
        {
            switch (status)
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

        bool FloatOffset(NativeWeaponFloatField field, std::size_t& output) noexcept
        {
            switch (field)
            {
                case NativeWeaponFloatField::WeaponRange:
                    output = offsetof(NativeWeaponInfoValues, weaponRange);
                    return true;
                case NativeWeaponFloatField::TargetRange:
                    output = offsetof(NativeWeaponInfoValues, targetRange);
                    return true;
                case NativeWeaponFloatField::Accuracy:
                    output = offsetof(NativeWeaponInfoValues, accuracy);
                    return true;
                case NativeWeaponFloatField::MoveSpeed:
                    output = offsetof(NativeWeaponInfoValues, moveSpeed);
                    return true;
                case NativeWeaponFloatField::FiringSpeed:
                    output = offsetof(NativeWeaponInfoValues, speed);
                    return true;
                case NativeWeaponFloatField::Radius:
                    output = offsetof(NativeWeaponInfoValues, radius);
                    return true;
                case NativeWeaponFloatField::LifeSpan:
                    output = offsetof(NativeWeaponInfoValues, lifeSpan);
                    return true;
                case NativeWeaponFloatField::Spread:
                    output = offsetof(NativeWeaponInfoValues, spread);
                    return true;
                case NativeWeaponFloatField::AnimBreakoutTime:
                    output = offsetof(NativeWeaponInfoValues, animBreakout);
                    return true;
                case NativeWeaponFloatField::AnimLoopFireTime:
                    output = offsetof(NativeWeaponInfoValues, animFireTime);
                    return true;
                default:
                    return false;
            }
        }

        bool ShortOffset(NativeWeaponShortField field, std::size_t& output) noexcept
        {
            switch (field)
            {
                case NativeWeaponShortField::Damage:
                    output = offsetof(NativeWeaponInfoValues, damage);
                    return true;
                case NativeWeaponShortField::MaximumClipAmmo:
                    output = offsetof(NativeWeaponInfoValues, clipAmmo);
                    return true;
                default:
                    return false;
            }
        }

        bool DwordOffset(NativeWeaponDwordField field, std::size_t& output) noexcept
        {
            switch (field)
            {
                case NativeWeaponDwordField::Skill:
                    output = offsetof(NativeWeaponInfoValues, skill);
                    return true;
                case NativeWeaponDwordField::RequiredStatLevel:
                    output = offsetof(NativeWeaponInfoValues, requiredStat);
                    return true;
                case NativeWeaponDwordField::Flags:
                    output = offsetof(NativeWeaponInfoValues, flags);
                    return true;
                case NativeWeaponDwordField::AnimGroup:
                    output = offsetof(NativeWeaponInfoValues, animGroup);
                    return true;
                default:
                    return false;
            }
        }

        bool AnimationCount(std::uintptr_t image, std::int32_t& output) noexcept
        {
            namespace WriteProfile = NativeWeaponInfoWriteProfile;
            std::array<unsigned char, WriteProfile::AnimationCountLoad.size()> actual{};
            return NativeImageAccess::ReadImage(image, image + WriteProfile::AnimationCountLoadRva, actual.data(), actual.size(), false, true) &&
                   actual == WriteProfile::AnimationCountLoad &&
                   NativeImageAccess::ReadImage(image, image + WriteProfile::AnimationCountRva, &output, sizeof(output), true) && output > 0;
        }

        template <typename Bits>
        Bits FieldBits(const NativeWeaponInfoValues& row, std::size_t offset) noexcept
        {
            Bits value;
            std::memcpy(&value, reinterpret_cast<const unsigned char*>(&row) + offset, sizeof(value));
            return value;
        }

        bool Exchange16(std::uintptr_t address, std::uint16_t expected, std::uint16_t target, std::uint16_t& previous) noexcept
        {
            static_assert(sizeof(SHORT) == sizeof(expected));
            SHORT expectedBits = 0, targetBits = 0, result = 0;
            std::memcpy(&expectedBits, &expected, sizeof(expected));
            std::memcpy(&targetBits, &target, sizeof(target));
            __try
            {
                result = InterlockedCompareExchange16(reinterpret_cast<volatile SHORT*>(address), targetBits, expectedBits);
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                return false;
            }
            std::memcpy(&previous, &result, sizeof(result));
            return true;
        }

        bool Exchange32(std::uintptr_t address, std::uint32_t expected, std::uint32_t target, std::uint32_t& previous) noexcept
        {
            static_assert(sizeof(LONG) == sizeof(expected));
            LONG expectedBits = 0, targetBits = 0, result = 0;
            std::memcpy(&expectedBits, &expected, sizeof(expected));
            std::memcpy(&targetBits, &target, sizeof(target));
            __try
            {
                result = InterlockedCompareExchange(reinterpret_cast<volatile LONG*>(address), targetBits, expectedBits);
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                return false;
            }
            std::memcpy(&previous, &result, sizeof(result));
            return true;
        }

        template <typename Bits>
        bool Exchange(std::uintptr_t address, Bits expected, Bits target, Bits& previous) noexcept
        {
            if constexpr (sizeof(Bits) == 2)
                return Exchange16(address, expected, target, previous);
            else
                return Exchange32(address, expected, target, previous);
        }

        template <typename Bits>
        ClockResult Store(const NativeWeaponInfoReader& reader, std::uintptr_t image, std::size_t slot, std::size_t offset, Bits value, Bits mask, bool merge,
                          CheckOwner checkOwner, bool boundedAnimation = false) noexcept
        {
            static_assert(sizeof(Bits) == 2 || sizeof(Bits) == 4);
            NativeWeaponInfoValues row;
            if (slot >= NativeWeaponInfoProfile::RowCount)
                return ClockResult::InvalidArgument;
            if (!image || image != reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr)))
                return ClockResult::BindingChanged;
            auto result = ReadResult(reader.Read(slot, row));
            if (result != ClockResult::Ok)
                return result;
            if (image != reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr)))
                return ClockResult::BindingChanged;
            const auto address = image + NativeWeaponInfoProfile::BankRva + slot * NativeWeaponInfoProfile::RowBytes + offset;
            // The reader has qualified this exact image and bounded slot. Admit
            // only the proven aligned field width, not a writable whole row.
            if (!image || address % sizeof(Bits) || !NativeImageAccess::ImagePages(image, address, sizeof(Bits), true))
                return ClockResult::BindingChanged;
            std::int32_t animationCount = 0;
            if (boundedAnimation)
            {
                if (!AnimationCount(image, animationCount))
                    return ClockResult::BindingChanged;
                if (value >= static_cast<std::uint32_t>(animationCount))
                    return ClockResult::InvalidArgument;
            }
            constexpr unsigned maximumAttempts = 8;
            for (unsigned attempt = 0; attempt < maximumAttempts; ++attempt)
            {
                if (image != reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr)))
                    return ClockResult::BindingChanged;
                if (boundedAnimation)
                {
                    std::int32_t currentCount = 0;
                    if (!AnimationCount(image, currentCount))
                        return ClockResult::BindingChanged;
                    if (currentCount != animationCount)
                        return ClockResult::Busy;
                }
                if (checkOwner)
                {
                    result = checkOwner();
                    if (result != ClockResult::Ok)
                        return result;
                }
                const auto expected = FieldBits<Bits>(row, offset);
                const auto target = static_cast<Bits>((expected & static_cast<Bits>(~mask)) | (value & mask));
                Bits       previous = 0;
                if (!Exchange(address, expected, target, previous))
                    return ClockResult::WriteFailure;
                if (previous != expected)
                {
                    // Whole-field intent must not erase intervening foreign
                    // writes. Masked intents can retry against the fresh field,
                    // preserving every bit outside their explicitly owned mask.
                    if (!merge)
                        return ClockResult::Busy;
                    result = ReadResult(reader.Read(slot, row));
                    if (result != ClockResult::Ok)
                        return result;
                    continue;
                }
                NativeWeaponInfoValues after;
                if (reader.Read(slot, after) != Status::Ok)
                    return ClockResult::WriteFailure;
                if (boundedAnimation)
                {
                    // The native loader indexes animation definitions through
                    // its live count. A changed bound after our persistent
                    // store cannot authorize restoring somebody else's row.
                    std::int32_t currentCount = 0;
                    if (!AnimationCount(image, currentCount) || currentCount != animationCount)
                        return ClockResult::WriteFailure;
                }
                if (checkOwner)
                {
                    result = checkOwner();
                    if (result != ClockResult::Ok)
                        return result;
                }
                const auto observed = FieldBits<Bits>(after, offset);
                // Another owner may already have changed unrelated flag bits.
                // Verify only this intent's mask and never roll anything back.
                return ((observed ^ target) & mask) == 0 ? ClockResult::Ok : ClockResult::WriteFailure;
            }
            return ClockResult::Busy;
        }
    }

    ClockResult SetFloat(const NativeWeaponInfoReader& reader, std::uintptr_t image, std::size_t slot, NativeWeaponFloatField field, float value,
                         CheckOwner checkOwner) noexcept
    {
        static_assert(sizeof(float) == sizeof(std::uint32_t));
        std::size_t offset = 0;
        if (!FloatOffset(field, offset))
            return ClockResult::InvalidArgument;
        std::uint32_t bits = 0;
        std::memcpy(&bits, &value, sizeof(bits));
        return Store(reader, image, slot, offset, bits, UINT32_MAX, false, checkOwner);
    }

    ClockResult SetShort(const NativeWeaponInfoReader& reader, std::uintptr_t image, std::size_t slot, NativeWeaponShortField field, std::int16_t value,
                         CheckOwner checkOwner) noexcept
    {
        std::size_t offset = 0;
        if (!ShortOffset(field, offset))
            return ClockResult::InvalidArgument;
        std::uint16_t bits = 0;
        std::memcpy(&bits, &value, sizeof(bits));
        return Store(reader, image, slot, offset, bits, static_cast<std::uint16_t>(UINT16_MAX), false, checkOwner);
    }

    ClockResult SetDword(const NativeWeaponInfoReader& reader, std::uintptr_t image, std::size_t slot, NativeWeaponDwordField field, std::uint32_t value,
                         CheckOwner checkOwner) noexcept
    {
        std::size_t offset = 0;
        if (!DwordOffset(field, offset) || (field == NativeWeaponDwordField::Skill && value > 3))
            return ClockResult::InvalidArgument;
        if (field == NativeWeaponDwordField::Skill)
        {
            // The SDK skill enum occupies its low byte. Preserve the upper
            // three bytes while admitting only the four native proved skills;
            // lookup's unknown-skill fallback does not authorize stored 255.
            return Store(reader, image, slot, offset, value, UINT32_C(0xff), true, checkOwner);
        }
        return Store(reader, image, slot, offset, value, UINT32_MAX, false, checkOwner, field == NativeWeaponDwordField::AnimGroup);
    }

    ClockResult UpdateFlags(const NativeWeaponInfoReader& reader, std::uintptr_t image, std::size_t slot, std::uint32_t mask, bool enabled,
                            CheckOwner checkOwner) noexcept
    {
        return Store(reader, image, slot, offsetof(NativeWeaponInfoValues, flags), enabled ? mask : 0, mask, true, checkOwner);
    }
}
