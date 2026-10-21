/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual typed weapon writes and SDK setters over an owned OS image
 *****************************************************************************/
// Reuse only the genuine sparse SEC_IMAGE builder and qualification seams.
// The already-tested reader suite is compiled under another entry point and
// is not executed again by this new mutation suite.
#define wmain FrozenReadOnlyWeaponFixtureMain
#include "NativeWeaponInfoReaderTests.cpp"
#undef wmain
#include "../../Client/game_de/NativeWeaponInfoWriteMemory.h"
#include "../../Client/game_de/NativeWeaponInfoWriteProfile.h"
#include "../../Client/game_de/CWeaponInfoWriteDE.h"
#include <functional>
#include <limits>
#include <intrin.h>

namespace WriteTestHost
{
    unsigned                  calls = 0;
    unsigned                  ownerChecks = 0, expireOwnerAt = 0;
    mta::game_de::ClockResult admission = mta::game_de::ClockResult::Ok;
    bool                      fault = false;
    std::function<void()>     before, after;
    mta::game_de::ClockResult Admit() noexcept
    {
        ++ownerChecks;
        return ownerChecks == expireOwnerAt ? mta::game_de::ClockResult::Expired : admission;
    }
    void Before()
    {
        ++calls;
        if (fault)
            RaiseException(EXCEPTION_ACCESS_VIOLATION, 0, 0, nullptr);
        if (before)
            before();
    }
    SHORT Cas16(volatile SHORT* address, SHORT value, SHORT expected)
    {
        Before();
        const SHORT previous = _InterlockedCompareExchange16(address, value, expected);
        if (after)
            after();
        return previous;
    }
    LONG Cas32(volatile LONG* address, LONG value, LONG expected)
    {
        Before();
        const LONG previous = _InterlockedCompareExchange(address, value, expected);
        if (after)
            after();
        return previous;
    }
    void Reset()
    {
        calls = 0;
        ownerChecks = expireOwnerAt = 0;
        admission = mta::game_de::ClockResult::Ok;
        fault = false;
        before = after = {};
        WeaponTestHost::failAt.store(0);
    }
}
#undef InterlockedCompareExchange
#undef InterlockedCompareExchange16
#define InterlockedCompareExchange   WriteTestHost::Cas32
#define InterlockedCompareExchange16 WriteTestHost::Cas16
#define GetModuleHandleW             WeaponTestHost::Module
#include "../../Client/game_de/NativeWeaponInfoWriteMemory.cpp"
#undef GetModuleHandleW
#undef InterlockedCompareExchange
#undef InterlockedCompareExchange16
#include "../../Client/game_de/CWeaponInfoWriteDE.cpp"

namespace
{
    namespace Memory = NativeWeaponInfoWriteMemory;
    namespace W = NativeWeaponInfoWriteProfile;
    constexpr std::size_t bankBytes = P::RowCount * P::RowBytes;
    using Bank = std::array<unsigned char, bankBytes>;

    Bank Snapshot(std::uintptr_t image)
    {
        Bank value;
        std::memcpy(value.data(), reinterpret_cast<const void*>(image + P::BankRva), value.size());
        return value;
    }
    template <typename T>
    void Put(std::uintptr_t image, std::size_t slot, std::size_t offset, T value)
    {
        std::memcpy(reinterpret_cast<void*>(image + P::BankRva + slot * P::RowBytes + offset), &value, sizeof(value));
    }
    template <typename T>
    void ExactChange(const Bank& before, std::uintptr_t image, std::size_t slot, std::size_t offset, T value, const char* why)
    {
        Bank expected = before;
        std::memcpy(expected.data() + slot * P::RowBytes + offset, &value, sizeof(value));
        Check(Snapshot(image) == expected, why);
    }
    struct Owner final : NativeWeaponInfoWriteAccess
    {
        const NativeWeaponInfoReader& reader;
        std::uintptr_t                image;
        unsigned                      calls = 0;
        std::size_t                   lastSlot = P::RowCount;
        Owner(const NativeWeaponInfoReader& input, std::uintptr_t base) : reader(input), image(base) {}
        ClockResult SetWeaponFloat(std::size_t slot, NativeWeaponFloatField field, float value) noexcept override
        {
            ++calls;
            lastSlot = slot;
            return Memory::SetFloat(reader, image, slot, field, value);
        }
        ClockResult SetWeaponShort(std::size_t slot, NativeWeaponShortField field, std::int16_t value) noexcept override
        {
            ++calls;
            lastSlot = slot;
            return Memory::SetShort(reader, image, slot, field, value);
        }
        ClockResult SetWeaponDword(std::size_t slot, NativeWeaponDwordField field, std::uint32_t value) noexcept override
        {
            ++calls;
            lastSlot = slot;
            return Memory::SetDword(reader, image, slot, field, value);
        }
        ClockResult UpdateWeaponFlags(std::size_t slot, std::uint32_t mask, bool enabled) noexcept override
        {
            ++calls;
            lastSlot = slot;
            return Memory::UpdateFlags(reader, image, slot, mask, enabled);
        }
    };
    struct RefusingOwner final : NativeWeaponInfoWriteAccess
    {
    };
    class WritableWeapon final : public CWeaponInfoWriteDE
    {
    public:
        WritableWeapon(const NativeWeaponInfoReader& reader, std::size_t slot, NativeWeaponInfoWriteAccess& owner) : CWeaponInfoWriteDE(reader, slot, owner) {}
        CVector* GetFireOffset() override { WeaponTestHost::Unqualified(); }
        void     SetFireOffset(CVector*) override { WeaponTestHost::Unqualified(); }
        void     SetSlot(eWeaponSlot) override { WeaponTestHost::Unqualified(); }
    };
    static_assert(std::is_abstract_v<CWeaponInfoWriteDE> && !std::is_abstract_v<WritableWeapon>);

    void ScalarSdkChecks(NativeWeaponInfoReader& reader, std::uintptr_t image)
    {
        struct FloatCase
        {
            void (CWeaponInfo::*set)(float);
            float (CWeaponInfo::*get)();
            std::size_t offset;
        };
        const FloatCase floats[]{{&CWeaponInfo::SetWeaponRange, &CWeaponInfo::GetWeaponRange, 0x08},
                                 {&CWeaponInfo::SetTargetRange, &CWeaponInfo::GetTargetRange, 0x04},
                                 {&CWeaponInfo::SetAccuracy, &CWeaponInfo::GetAccuracy, 0x38},
                                 {&CWeaponInfo::SetMoveSpeed, &CWeaponInfo::GetMoveSpeed, 0x3c},
                                 {&CWeaponInfo::SetFiringSpeed, &CWeaponInfo::GetFiringSpeed, 0x5c},
                                 {&CWeaponInfo::SetRadius, &CWeaponInfo::GetRadius, 0x60},
                                 {&CWeaponInfo::SetLifeSpan, &CWeaponInfo::GetLifeSpan, 0x64},
                                 {&CWeaponInfo::SetSpread, &CWeaponInfo::GetSpread, 0x68},
                                 {&CWeaponInfo::SetAnimBreakoutTime, &CWeaponInfo::GetAnimBreakoutTime, 0x58},
                                 {&CWeaponInfo::SetWeaponAnimLoopFireTime, &CWeaponInfo::GetWeaponAnimLoopFireTime, 0x48}};
        Owner           owner(reader, image);
        RefusingOwner   refusing;
        for (const auto slot : {std::size_t{0}, P::RowCount - 1})
        {
            WritableWeapon weapon(reader, slot, owner), disabled(reader, slot, refusing);
            CWeaponInfo&   sdk = weapon;
            for (const auto& entry : floats)
            {
                const auto  before = Snapshot(image);
                const float value = -12.875f - static_cast<float>(slot);
                (sdk.*entry.set)(value);
                Check((sdk.*entry.get)() == value, "real SDK float write is immediately visible through actual native getter");
                ExactChange(before, image, slot, entry.offset, value, "float write changes exactly four bytes in whole eighty-row bank");
                Refuses([&] { (disabled.*entry.set)(value); }, "read-only/default owner refuses SDK float setter");
            }
            for (const auto value : {std::int16_t{-32768}, std::int16_t{32767}})
            {
                auto before = Snapshot(image);
                sdk.SetDamagePerHit(value);
                Check(sdk.GetDamagePerHit() == value, "damage accepts exact signed short endpoints");
                ExactChange(before, image, slot, 0x22, value, "damage CAS preserves adjacent clip half and every other row");
                before = Snapshot(image);
                sdk.SetMaximumClipAmmo(value);
                Check(sdk.GetMaximumClipAmmo() == value, "clip accepts exact signed short endpoints");
                ExactChange(before, image, slot, 0x20, value, "clip CAS preserves adjacent damage half and every other row");
            }
            Refuses([&] { disabled.SetDamagePerHit(3); }, "default owner refuses damage setter");
            Refuses([&] { disabled.SetMaximumClipAmmo(3); }, "default owner refuses clip setter");
            auto before = Snapshot(image);
            sdk.SetFlags(-1);
            ExactChange(before, image, slot, 0x18, UINT32_MAX, "full flags setter preserves signed 32-bit representation");
            before = Snapshot(image);
            sdk.ClearFlag(0x80000003);
            ExactChange(before, image, slot, 0x18, UINT32_C(0x7ffffffc), "clear masked flags preserves all unowned bits");
            sdk.SetFlag(0x80000002);
            Check(static_cast<std::uint32_t>(sdk.GetFlags()) == UINT32_C(0xfffffffe), "set masked flags preserves signed high bit");
            before = Snapshot(image);
            sdk.SetFlag(0);
            sdk.ClearFlag(0);
            Check(Snapshot(image) == before, "empty masks preserve native flags");
            Refuses([&] { disabled.SetFlags(3); }, "default owner refuses full flags");
            Refuses([&] { disabled.SetFlag(3); }, "default owner refuses flag set");
            Refuses([&] { disabled.ClearFlag(3); }, "default owner refuses flag clear");
            Put(image, slot, 0x30, UINT32_C(0xa5b6c700));
            for (unsigned skill = 0; skill != 4; ++skill)
            {
                before = Snapshot(image);
                sdk.SetSkill(static_cast<eWeaponSkill>(skill));
                ExactChange(before, image, slot, 0x30, UINT32_C(0xa5b6c700) | skill, "skill preserves upper 24 native bits");
                Check(sdk.GetSkill() == static_cast<eWeaponSkill>(skill), "actual SDK skill getter observes permitted low byte");
            }
            auto calls = owner.calls;
            Refuses([&] { sdk.SetSkill(static_cast<eWeaponSkill>(255)); }, "invalid native skill rejected before owner dispatch");
            Check(owner.calls == calls, "unknown lookup fallback cannot authorize stored skill 255");
            Refuses([&] { disabled.SetSkill(WEAPONSKILL_STD); }, "default owner refuses skill");
            for (const auto value : {-2147483648.0f, -19.75f, 0.0f, 2147483520.0f})
            {
                before = Snapshot(image);
                sdk.SetRequiredStatLevel(value);
                const auto integer = static_cast<std::int32_t>(value);
                ExactChange(before, image, slot, 0x34, integer, "required-stat conversion preserves finite in-range SDK truncation");
            }
            for (const auto value : {2147483648.0f, -2147483904.0f, std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity(),
                                     std::numeric_limits<float>::quiet_NaN()})
            {
                calls = owner.calls;
                Refuses([&] { sdk.SetRequiredStatLevel(value); }, "required-stat refuses undefined float-to-int conversion");
                Check(owner.calls == calls, "non-finite and overflow required stat never reach native writer");
            }
            Refuses([&] { disabled.SetRequiredStatLevel(1); }, "default owner refuses required stat");
            for (const DWORD value : {DWORD{0}, DWORD{19}})
            {
                before = Snapshot(image);
                sdk.SetAnimGroup(value);
                ExactChange(before, image, slot, 0x1c, value, "animation index inside actual native count changes only selected DWORD");
            }
            before = Snapshot(image);
            Refuses([&] { sdk.SetAnimGroup(20); }, "animation index equal to live count rejected");
            Refuses([&] { sdk.SetAnimGroup(MAXDWORD); }, "arbitrary animation DWORD rejected");
            Check(Snapshot(image) == before, "invalid animation indices leave entire bank untouched");
            Refuses([&] { disabled.SetAnimGroup(1); }, "default owner refuses animation group");
            Check(owner.lastSlot == slot, "SDK setter dispatch preserves first and last SDK slot identity");
        }
        const auto          before = Snapshot(image);
        const std::uint32_t negativeZero = UINT32_C(0x80000000);
        float               value = 0;
        std::memcpy(&value, &negativeZero, sizeof(value));
        Check(Memory::SetFloat(reader, image, 7, NativeWeaponFloatField::WeaponRange, value) == ClockResult::Ok,
              "native float setter preserves negative zero bits");
        ExactChange(before, image, 7, 0x08, negativeZero, "float scalar CAS performs no lossy numeric conversion");
    }

    void ContentionChecks(NativeWeaponInfoReader& reader, std::uintptr_t image)
    {
        WriteTestHost::Reset();
        constexpr std::size_t slot = 3;
        Put(image, slot, 0x18, UINT32_C(0));
        WriteTestHost::before = [&] { Put(image, slot, 0x18, UINT32_C(0x40000000)); };
        Check(Memory::SetDword(reader, image, slot, NativeWeaponDwordField::Flags, 7) == ClockResult::Busy,
              "whole-field CAS refuses an intervening foreign field");
        Check(*reinterpret_cast<const std::uint32_t*>(image + P::BankRva + slot * P::RowBytes + 0x18) == UINT32_C(0x40000000),
              "foreign flags survive failed whole-field intent");
        Check(WriteTestHost::calls == 1, "whole-field conflict does not retry over foreign owner");
        WriteTestHost::Reset();
        Put(image, slot, 0x18, UINT32_C(0));
        WriteTestHost::before = [&]
        {
            if (WriteTestHost::calls == 1)
                Put(image, slot, 0x18, UINT32_C(0x40000000));
        };
        Check(Memory::UpdateFlags(reader, image, slot, 3, true) == ClockResult::Ok, "masked CAS retries fresh foreign flags");
        Check(*reinterpret_cast<const std::uint32_t*>(image + P::BankRva + slot * P::RowBytes + 0x18) == UINT32_C(0x40000003),
              "retry merges intent while preserving foreign flag");
        Check(WriteTestHost::calls == 2, "single collision needs exactly one bounded retry");
        WriteTestHost::Reset();
        Put(image, slot, 0x18, UINT32_C(0));
        WriteTestHost::before = [&] { Put(image, slot, 0x18, static_cast<std::uint32_t>(WriteTestHost::calls << 8)); };
        Check(Memory::UpdateFlags(reader, image, slot, 1, true) == ClockResult::Busy && WriteTestHost::calls == 8,
              "perpetual foreign contention stops after eight CAS attempts");
        WriteTestHost::Reset();
        Put(image, slot, 0x18, UINT32_C(0));
        WriteTestHost::after = [&] { Put(image, slot, 0x18, UINT32_C(0x80000001)); };
        Check(Memory::UpdateFlags(reader, image, slot, 1, true) == ClockResult::Ok, "readback accepts foreign changes outside requested mask");
        Check(*reinterpret_cast<const std::uint32_t*>(image + P::BankRva + slot * P::RowBytes + 0x18) == UINT32_C(0x80000001),
              "post-CAS foreign bits are preserved");
        WriteTestHost::Reset();
        WriteTestHost::after = [&] { Put(image, slot, 0x18, UINT32_C(0x40000000)); };
        Check(Memory::UpdateFlags(reader, image, slot, 1, true) == ClockResult::WriteFailure, "readback detects foreign replacement of requested bit");
        Check(*reinterpret_cast<const std::uint32_t*>(image + P::BankRva + slot * P::RowBytes + 0x18) == UINT32_C(0x40000000),
              "failed readback never rolls back a foreign update");
        WriteTestHost::Reset();
        const auto before = Snapshot(image);
        WriteTestHost::fault = true;
        Check(Memory::SetShort(reader, image, slot, NativeWeaponShortField::Damage, 4) == ClockResult::WriteFailure,
              "actual SEH-protected 16-bit CAS refuses memory exception");
        Check(Snapshot(image) == before, "faulted CAS leaves owned bank untouched");
        Check(Memory::SetDword(reader, image, slot, NativeWeaponDwordField::Flags, 5) == ClockResult::WriteFailure,
              "actual SEH-protected 32-bit CAS also refuses memory exception");
        Check(Snapshot(image) == before, "faulted DWORD CAS preserves whole bank");
        WriteTestHost::Reset();
        WriteTestHost::after = [&] { WeaponTestHost::failAt.store(WeaponTestHost::reads.load() + 1); };
        Check(Memory::SetShort(reader, image, slot, NativeWeaponShortField::Damage, 115) == ClockResult::WriteFailure,
              "partial OS copy during readback reports write failure");
        WriteTestHost::Reset();
        ExactChange(before, image, slot, 0x22, std::int16_t{115}, "readback failure leaves persistent intent applied without blind rollback");
        auto               withForeignClip = Snapshot(image);
        const std::int16_t foreignClip = 45;
        std::memcpy(withForeignClip.data() + slot * P::RowBytes + 0x20, &foreignClip, sizeof(foreignClip));
        WriteTestHost::before = [&] { Put(image, slot, 0x20, foreignClip); };
        Check(Memory::SetShort(reader, image, slot, NativeWeaponShortField::Damage, 47) == ClockResult::Ok,
              "sixteen-bit CAS accepts an unrelated adjacent-half update");
        ExactChange(withForeignClip, image, slot, 0x22, std::int16_t{47}, "damage intent preserves concurrent foreign clip update");
        WriteTestHost::Reset();
        const std::int32_t changedCount = 19;
        WriteTestHost::after = [&] { std::memcpy(reinterpret_cast<void*>(image + W::AnimationCountRva), &changedCount, sizeof(changedCount)); };
        Check(Memory::SetDword(reader, image, slot, NativeWeaponDwordField::AnimGroup, 18) == ClockResult::WriteFailure,
              "post-CAS animation-count change invalidates verification");
        const std::int32_t restoredCount = 20;
        std::memcpy(reinterpret_cast<void*>(image + W::AnimationCountRva), &restoredCount, sizeof(restoredCount));
        WriteTestHost::Reset();
        Check(*reinterpret_cast<const std::uint32_t*>(image + P::BankRva + slot * P::RowBytes + 0x1c) == 18,
              "bound change does not restore an entire weapon row");
        auto bank = Snapshot(image);
        WriteTestHost::admission = ClockResult::Stopped;
        Check(Memory::SetFloat(reader, image, slot, NativeWeaponFloatField::Accuracy, 11, WriteTestHost::Admit) == ClockResult::Stopped,
              "owner scope is rechecked after qualification and before actual CAS");
        Check(WriteTestHost::calls == 0 && Snapshot(image) == bank, "retired generation cannot write after a slow profile copy");
        WriteTestHost::Reset();
        WriteTestHost::before = [&] { Put(image, slot, 0x18, UINT32_C(0x500)); };
        WriteTestHost::expireOwnerAt = 2;
        Check(Memory::UpdateFlags(reader, image, slot, 1, true, WriteTestHost::Admit) == ClockResult::Expired, "masked retry rechecks actual callback scope");
        Check(WriteTestHost::calls == 1, "expired scope cannot perform a second masked CAS");
        WriteTestHost::Reset();
        bank = Snapshot(image);
        WriteTestHost::after = [] { WriteTestHost::admission = ClockResult::BindingChanged; };
        Check(Memory::SetShort(reader, image, slot, NativeWeaponShortField::Damage, 90, WriteTestHost::Admit) == ClockResult::BindingChanged,
              "owner generation must still be current before accepting readback");
        ExactChange(bank, image, slot, 0x22, std::int16_t{90}, "generation loss after CAS leaves persistent write and foreign fields intact");
        WriteTestHost::Reset();
    }

    void RefusalChecks(NativeWeaponInfoReader& reader, OwnedImage& owned)
    {
        const auto image = owned.image;
        WriteTestHost::Reset();
        const auto before = Snapshot(image);
        Check(Memory::SetFloat(reader, image, 80, NativeWeaponFloatField::Accuracy, 1) == ClockResult::InvalidArgument, "slot eighty cannot write beyond bank");
        Check(Memory::SetFloat(reader, image, SIZE_MAX, NativeWeaponFloatField::Accuracy, 1) == ClockResult::InvalidArgument,
              "huge slot rejected before arithmetic");
        Check(Memory::SetFloat(reader, image, 0, static_cast<NativeWeaponFloatField>(99), 1) == ClockResult::InvalidArgument,
              "unknown float descriptor refused");
        Check(Memory::SetShort(reader, image, 0, static_cast<NativeWeaponShortField>(99), 1) == ClockResult::InvalidArgument,
              "unknown short descriptor refused");
        Check(Memory::SetDword(reader, image, 0, static_cast<NativeWeaponDwordField>(99), 1) == ClockResult::InvalidArgument,
              "unknown DWORD descriptor refused");
        Check(Memory::SetDword(reader, image, 0, NativeWeaponDwordField::Skill, 4) == ClockResult::InvalidArgument,
              "native memory helper also rejects skill four");
        Check(Memory::SetFloat(reader, 0, 0, NativeWeaponFloatField::Accuracy, 1) == ClockResult::BindingChanged, "null qualified image refused");
        Check(Memory::SetFloat(reader, image + 4096, 0, NativeWeaponFloatField::Accuracy, 1) == ClockResult::BindingChanged,
              "different explicit image refused");
        NativeWeaponInfoReader missing;
        Check(Memory::SetFloat(missing, image, 0, NativeWeaponFloatField::Accuracy, 1) == ClockResult::NotInitialized, "read-unbound owner cannot mutate");
        ClockResult other = ClockResult::Ok;
        std::thread worker([&] { other = Memory::SetFloat(reader, image, 0, NativeWeaponFloatField::Accuracy, 1); });
        worker.join();
        Check(other == ClockResult::WrongThread, "weapon reader thread contract also gates all writes");
        Check(WriteTestHost::calls == 0 && Snapshot(image) == before, "argument/thread/binding refusals never reach CAS");
        WeaponTestHost::failAt.store(WeaponTestHost::reads.load() + 1);
        Check(Memory::UpdateFlags(reader, image, 0, 1, true) == ClockResult::BindingChanged, "partial qualification copy prevents mutation");
        WriteTestHost::Reset();
        const auto code = P::Code.front();
        auto       changed = code.bytes[0] ^ 1;
        owned.Patch(code.rva, &changed, 1);
        Check(Memory::UpdateFlags(reader, image, 0, 1, true) == ClockResult::BindingChanged, "changed native provenance prevents mutation");
        owned.Patch(code.rva, code.bytes.data(), 1);
        DWORD old = 0, ignored = 0;
        Check(VirtualProtect(reinterpret_cast<void*>(image + P::BankRva), 4096, PAGE_READONLY, &old) != FALSE, "make only owned bank page read-only");
        Check(Memory::SetFloat(reader, image, 0, NativeWeaponFloatField::Accuracy, 1) == ClockResult::BindingChanged,
              "read-only native bank refuses before CAS");
        Check(VirtualProtect(reinterpret_cast<void*>(image + P::BankRva), 4096, old, &ignored) != FALSE, "restore only owned bank protection");
        for (const std::int32_t count : {0, -1})
        {
            std::memcpy(reinterpret_cast<void*>(image + W::AnimationCountRva), &count, sizeof(count));
            Check(Memory::SetDword(reader, image, 0, NativeWeaponDwordField::AnimGroup, 0) == ClockResult::BindingChanged,
                  "missing or negative live animation count refuses");
        }
        const std::int32_t count = 20;
        std::memcpy(reinterpret_cast<void*>(image + W::AnimationCountRva), &count, sizeof(count));
        Check(WriteTestHost::calls == 0 && Snapshot(image) == before, "all qualification failures preserve full bank");
    }
}

int wmain(int argc, wchar_t** argv)
{
    if (argc != 2 || sizeof(void*) != 8)
        return 2;
    OwnedImage owned;
    owned.Create(argv[1]);
    WeaponTestHost::image = owned.image;
    const std::int32_t count = 20;
    owned.Patch(W::AnimationCountRva, &count, sizeof(count));
    DWORD old = 0;
    Check(VirtualProtect(reinterpret_cast<void*>(owned.image + W::AnimationCountRva), sizeof(count), PAGE_READWRITE, &old) != FALSE,
          "live count uses genuine owned writable image page");
    NativeWeaponInfoReader reader;
    Check(reader.Bind() == Status::Ok, "actual production reader qualifies complete eighty-row owned image");
    ScalarSdkChecks(reader, owned.image);
    ContentionChecks(reader, owned.image);
    RefusalChecks(reader, owned);
    std::printf("Passed %u native weapon-write and actual SDK checks (64-bit)\n", checks);
    return 0;
}
