/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Checked native weapon-stat prerequisites over owned OS image bytes
 ****************************************************************************/
#include <windows.h>
#include "../../Client/game_de/NativeWeaponInfoExtraProfile.h"
#include <array>
#include <cstdint>
#include <vector>
#include <functional>
#include <limits>
#include <intrin.h>

namespace ExtraFixture
{
    constexpr std::uintptr_t ModelVtableRva = 0x4304000;
    template <class Seed>
    void SeedImage(Seed&& seed, DWORD code, DWORD literal, DWORD bank)
    {
        namespace E = mta::game_de::NativeWeaponInfoExtraProfile;
        for (const auto& pin : E::Code)
            seed(pin.rva, pin.bytes.data(), pin.size, code);
        const std::vector<std::uintptr_t> models(E::ModelCount);
        seed(E::ModelTableRva, models.data(), models.size() * sizeof(models[0]), bank);
        const std::array<unsigned char, E::AimRecordBytes * E::AimRecordCount> aim{};
        seed(E::AimTableRva, aim.data(), aim.size(), bank);
        const std::array<unsigned char, E::ComboRecordBytes * 11> combos{};
        seed(E::ComboTableRva, combos.data(), combos.size(), bank);
        const std::array<std::uintptr_t, 13> vtable{};
        seed(ModelVtableRva, vtable.data(), sizeof(vtable), literal);
    }
}
#define MTA_DE_WEAPON_FIXTURE_EXTRA_SEED(seed, code, literal, bank) ExtraFixture::SeedImage(seed, code, literal, bank)
#define wmain                                                       FrozenExtraReadFixtureMain
#include "NativeWeaponInfoReaderTests.cpp"
#undef wmain
#undef MTA_DE_WEAPON_FIXTURE_EXTRA_SEED
#include "../../Client/game_de/NativeWeaponInfoWriteMemory.h"

namespace ExtraWriteHost
{
    unsigned                      calls = 0, ownerChecks = 0, expireAt = 0;
    bool                          fault = false;
    std::function<void()>         before, after;
    std::function<void(unsigned)> admissionStep;
    ClockResult                   Admit() noexcept
    {
        ++ownerChecks;
        if (admissionStep)
            admissionStep(ownerChecks);
        return ownerChecks == expireAt ? ClockResult::Expired : ClockResult::Ok;
    }
    void Before()
    {
        ++calls;
        if (fault)
            RaiseException(EXCEPTION_ACCESS_VIOLATION, 0, 0, nullptr);
        if (before)
            before();
    }
    SHORT Cas16(volatile SHORT* address, SHORT target, SHORT expected)
    {
        Before();
        const SHORT value = _InterlockedCompareExchange16(address, target, expected);
        if (after)
            after();
        return value;
    }
    LONG Cas32(volatile LONG* address, LONG target, LONG expected)
    {
        Before();
        const LONG value = _InterlockedCompareExchange(address, target, expected);
        if (after)
            after();
        return value;
    }
    void Reset()
    {
        calls = ownerChecks = expireAt = 0;
        fault = false;
        before = after = {};
        admissionStep = {};
        WeaponTestHost::failAt = 0;
    }
}
#undef InterlockedCompareExchange
#undef InterlockedCompareExchange16
#define InterlockedCompareExchange   ExtraWriteHost::Cas32
#define InterlockedCompareExchange16 ExtraWriteHost::Cas16
#define ReadProcessMemory            WeaponTestHost::Read
#define GetModuleHandleW             WeaponTestHost::Module
#include "../../Client/game_de/NativeWeaponInfoWriteMemory.cpp"
#undef GetModuleHandleW
#undef ReadProcessMemory
#undef InterlockedCompareExchange
#undef InterlockedCompareExchange16

namespace
{
    namespace E = NativeWeaponInfoExtraProfile;
    namespace ExtraMemory = NativeWeaponInfoWriteMemory;
    using ExtraBank = std::array<unsigned char, P::RowCount * P::RowBytes>;
    ExtraBank ExtraSnapshot(std::uintptr_t image)
    {
        ExtraBank value;
        std::memcpy(value.data(), reinterpret_cast<const void*>(image + P::BankRva), value.size());
        return value;
    }
    template <class T>
    void ExtraPut(std::uintptr_t image, std::size_t slot, std::size_t offset, T value)
    {
        std::memcpy(reinterpret_cast<void*>(image + P::BankRva + slot * P::RowBytes + offset), &value, sizeof(value));
    }
    template <class T>
    void ExtraExact(const ExtraBank& before, std::uintptr_t image, std::size_t slot, std::size_t offset, T value)
    {
        auto expected = before;
        std::memcpy(expected.data() + slot * P::RowBytes + offset, &value, sizeof(value));
        Check(ExtraSnapshot(image) == expected, "new intent changes precisely its proven field and preserves all other native bank bytes");
    }
    struct ModelObjects
    {
        std::uintptr_t memory = reinterpret_cast<std::uintptr_t>(VirtualAlloc(nullptr, 4096, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        ~ModelObjects()
        {
            if (memory)
                VirtualFree(reinterpret_cast<void*>(memory), 0, MEM_RELEASE);
        }
        void Initialize(std::uintptr_t image)
        {
            Check(memory != 0, "allocate only owned model-like pointer storage");
            for (const auto offset : {std::size_t{0}, std::size_t{0x100}})
                *reinterpret_cast<std::uintptr_t*>(memory + offset) = image + ExtraFixture::ModelVtableRva;
        }
        std::uintptr_t First() const { return memory; }
        std::uintptr_t Second() const { return memory + 0x100; }
    };
    void ModelCell(std::uintptr_t image, std::uint32_t index, std::uintptr_t object)
    {
        Check(index < E::ModelCount, "fixture model table cell is bounded independently");
        std::memcpy(reinterpret_cast<void*>(image + E::ModelTableRva + index * sizeof(object)), &object, sizeof(object));
    }

    void ExtraScalarChecks(NativeWeaponInfoReader& reader, std::uintptr_t image)
    {
        struct Field
        {
            NativeWeaponFloatField value;
            std::size_t            offset;
        };
        constexpr Field fields[] = {{NativeWeaponFloatField::AnimLoopStart, 0x40},
                                    {NativeWeaponFloatField::AnimLoopStop, 0x44},
                                    {NativeWeaponFloatField::Anim2LoopStart, 0x4c},
                                    {NativeWeaponFloatField::Anim2LoopStop, 0x50},
                                    {NativeWeaponFloatField::Anim2LoopFireTime, 0x54}};
        for (std::size_t slot = 0; slot < P::RowCount; ++slot)
        {
            for (const auto& field : fields)
            {
                for (const float value : {-31.75f, -0.0f, 19.5f})
                {
                    const auto before = ExtraSnapshot(image);
                    Check(ExtraMemory::SetFloat(reader, image, slot, field.value, value, ExtraWriteHost::Admit) == ClockResult::Ok,
                          "qualified additional animation timing scalar succeeds through the existing owner access path");
                    ExtraExact(before, image, slot, field.offset, value);
                }
                const auto before = ExtraSnapshot(image);
                const auto calls = ExtraWriteHost::calls;
                for (const auto value :
                     {std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()})
                    Check(ExtraMemory::SetFloat(reader, image, slot, field.value, value, ExtraWriteHost::Admit) == ClockResult::InvalidArgument,
                          "new native timing values refuse nonfinite inputs before mutation");
                Check(ExtraWriteHost::calls == calls && ExtraSnapshot(image) == before, "nonfinite extra scalars preserve every bank byte");
            }
            for (std::uint32_t type = 0; type != 6; ++type)
            {
                const auto before = ExtraSnapshot(image);
                Check(ExtraMemory::SetDword(reader, image, slot, NativeWeaponDwordField::FireType, type, ExtraWriteHost::Admit) == ClockResult::Ok,
                      "each actual native fire classifier value is admitted");
                ExtraExact(before, image, slot, 0x00, type);
            }
        }
        const auto before = ExtraSnapshot(image);
        for (std::uint32_t type = 6; type <= 255; ++type)
            Check(ExtraMemory::SetDword(reader, image, 0, NativeWeaponDwordField::FireType, type, ExtraWriteHost::Admit) == ClockResult::InvalidArgument,
                  "unproved native fire classifier values are refused");
        Check(ExtraMemory::SetDword(reader, image, 0, NativeWeaponDwordField::FireType, UINT32_MAX, ExtraWriteHost::Admit) == ClockResult::InvalidArgument &&
                  ExtraSnapshot(image) == before,
              "large fire enum casts do not mutate native data");
    }

    void ExtraAimComboChecks(NativeWeaponInfoReader& reader, std::uintptr_t image)
    {
        for (const auto slot : {std::size_t{0}, P::RowCount - 1})
        {
            for (std::int16_t aim = 0; aim != 21; ++aim)
            {
                const auto before = ExtraSnapshot(image);
                Check(ExtraMemory::SetShort(reader, image, slot, NativeWeaponShortField::AimOffsetIndex, aim, ExtraWriteHost::Admit) == ClockResult::Ok,
                      "actual21-record native aim table accepts each proven index");
                ExtraExact(before, image, slot, 0x6c, aim);
            }
            for (const auto combo : E::KnownCombos)
            {
                for (const std::uint8_t neighbor : {std::uint8_t{0}, std::uint8_t{1}, std::uint8_t{127}, std::uint8_t{255}})
                {
                    ExtraPut(image, slot, 0x6f, neighbor);
                    const auto before = ExtraSnapshot(image);
                    Check(ExtraMemory::SetShort(reader, image, slot, NativeWeaponShortField::DefaultCombo, combo, ExtraWriteHost::Admit) == ClockResult::Ok,
                          "actual native combo classifier IDs are admitted");
                    ExtraExact(before, image, slot, 0x6e, combo);
                }
            }
        }
        const auto before = ExtraSnapshot(image);
        const auto calls = ExtraWriteHost::calls;
        for (int aim = (std::numeric_limits<std::int16_t>::min)(); aim <= (std::numeric_limits<std::int16_t>::max)(); ++aim)
        {
            if (aim >= 0 && aim < 21)
                continue;
            Check(ExtraMemory::SetShort(reader, image, 0, NativeWeaponShortField::AimOffsetIndex, static_cast<std::int16_t>(aim), ExtraWriteHost::Admit) ==
                      ClockResult::InvalidArgument,
                  "all other signed16 aim values are refused without indexing native storage");
        }
        for (std::int16_t combo = 0; combo <= 255; ++combo)
        {
            bool known = false;
            for (const auto value : E::KnownCombos)
                known = known || value == combo;
            if (!known)
                Check(
                    ExtraMemory::SetShort(reader, image, 0, NativeWeaponShortField::DefaultCombo, combo, ExtraWriteHost::Admit) == ClockResult::InvalidArgument,
                    "unknown combo bytes including unproved intermediate IDs are refused");
        }
        for (const std::int16_t combo : {std::int16_t{-32768}, std::int16_t{-1}, std::int16_t{32767}})
            Check(ExtraMemory::SetShort(reader, image, 0, NativeWeaponShortField::DefaultCombo, combo, ExtraWriteHost::Admit) == ClockResult::InvalidArgument,
                  "negative and wide short combo requests are refused");
        Check(ExtraSnapshot(image) == before && ExtraWriteHost::calls == calls, "invalid aim and combo requests never reach CAS");

        constexpr std::size_t slot = 3;
        ExtraWriteHost::Reset();
        ExtraPut(image, slot, 0x6e, std::uint8_t{4});
        ExtraPut(image, slot, 0x6f, std::uint8_t{1});
        ExtraWriteHost::before = [&] { ExtraPut(image, slot, 0x6f, std::uint8_t{222}); };
        Check(ExtraMemory::SetShort(reader, image, slot, NativeWeaponShortField::DefaultCombo, 8, ExtraWriteHost::Admit) == ClockResult::Ok &&
                  ExtraWriteHost::calls == 2,
              "masked short retries a foreign neighboring-byte change");
        NativeWeaponInfoValues row;
        Check(reader.Read(slot, row) == Status::Ok && row.defaultCombo == 8 && row.combosAvailable == 222, "masked combo write preserves the foreign byte");
        ExtraWriteHost::Reset();
        ExtraPut(image, slot, 0x6e, std::uint8_t{4});
        ExtraWriteHost::before = [&] { ExtraPut(image, slot, 0x6e, std::uint8_t{9}); };
        Check(ExtraMemory::SetShort(reader, image, slot, NativeWeaponShortField::DefaultCombo, 8, ExtraWriteHost::Admit) == ClockResult::Busy &&
                  ExtraWriteHost::calls == 1,
              "masked short refuses a competing combo field instead of replacing it");
        Check(reader.Read(slot, row) == Status::Ok && row.defaultCombo == 9, "foreign combo ownership survives refused intent");
        ExtraWriteHost::Reset();
        ExtraPut(image, slot, 0x6e, std::uint8_t{4});
        ExtraPut(image, slot, 0x6f, std::uint8_t{1});
        ExtraWriteHost::before = [&] { ExtraPut(image, slot, 0x6f, std::uint8_t{2}); };
        ExtraWriteHost::after = [&] { ExtraPut(image, slot, 0x6e, std::uint8_t{10}); };
        Check(ExtraMemory::SetShort(reader, image, slot, NativeWeaponShortField::DefaultCombo, 8, ExtraWriteHost::Admit) == ClockResult::Busy &&
                  ExtraWriteHost::calls == 1,
              "later fresh row cannot authorize overwriting a newly foreign combo field during a neighbor retry");
        Check(reader.Read(slot, row) == Status::Ok && row.defaultCombo == 10 && row.combosAvailable == 2,
              "both foreign fields survive a deferred masked retry refusal");
        ExtraWriteHost::Reset();
        ExtraPut(image, slot, 0x6e, std::uint8_t{4});
        ExtraPut(image, slot, 0x6f, std::uint8_t{0});
        ExtraWriteHost::before = [&] { ExtraPut(image, slot, 0x6f, static_cast<std::uint8_t>(ExtraWriteHost::calls)); };
        Check(ExtraMemory::SetShort(reader, image, slot, NativeWeaponShortField::DefaultCombo, 8, ExtraWriteHost::Admit) == ClockResult::Busy &&
                  ExtraWriteHost::calls == 8,
              "neighbor contention has a bounded masked-short retry budget");
        Check(reader.Read(slot, row) == Status::Ok && row.defaultCombo == 4 && row.combosAvailable == 8,
              "exhausted neighbor contention preserves the foreign byte and uncommitted combo");
        ExtraWriteHost::Reset();
        ExtraWriteHost::after = [&]
        {
            ExtraPut(image, slot, 0x6e, std::uint8_t{14});
            ExtraPut(image, slot, 0x6f, std::uint8_t{201});
        };
        Check(ExtraMemory::SetShort(reader, image, slot, NativeWeaponShortField::DefaultCombo, 8, ExtraWriteHost::Admit) == ClockResult::WriteFailure &&
                  ExtraWriteHost::calls == 1,
              "foreign combo supersession after the exchange is not reported as success");
        Check(reader.Read(slot, row) == Status::Ok && row.defaultCombo == 14 && row.combosAvailable == 201,
              "failed combo readback never rolls back either foreign byte");
        ExtraWriteHost::Reset();
    }

    void ExtraModelChecks(OwnedImage& fixture, NativeWeaponInfoReader& reader, ModelObjects& objects)
    {
        const auto image = fixture.image;
        for (const auto index : {std::uint32_t{0}, std::uint32_t{1}, std::uint32_t{346}, E::ModelCount - 1})
            ModelCell(image, index, objects.First());
        const auto method = image + P::Code[0].rva;
        fixture.Patch(ExtraFixture::ModelVtableRva + E::ModelVirtualMethodOffset, &method, sizeof(method));
        for (const auto slot : {std::size_t{0}, P::RowCount - 1})
        {
            for (const auto field : {NativeWeaponDwordField::Model, NativeWeaponDwordField::Model2})
            {
                const std::size_t offset = field == NativeWeaponDwordField::Model ? 0x0c : 0x10;
                for (const auto index : {UINT32_MAX, std::uint32_t{0}, std::uint32_t{1}, std::uint32_t{346}, E::ModelCount - 1})
                {
                    const auto before = ExtraSnapshot(image);
                    Check(ExtraMemory::SetDword(reader, image, slot, field, index, ExtraWriteHost::Admit) == ClockResult::Ok,
                          "both actual model fields accept native -1 or a bounded existing native model-table entry");
                    ExtraExact(before, image, slot, offset, index);
                }
            }
        }
        const auto before = ExtraSnapshot(image);
        for (const auto index : {E::ModelCount, E::ModelCount + 1, std::uint32_t{0x7fffffff}, std::uint32_t{0xfffffffe}})
            Check(ExtraMemory::SetDword(reader, image, 0, NativeWeaponDwordField::Model, index, ExtraWriteHost::Admit) == ClockResult::InvalidArgument,
                  "unbounded model integer casts cannot index native model table");
        Check(ExtraMemory::SetDword(reader, image, 0, NativeWeaponDwordField::Model2, 2, ExtraWriteHost::Admit) == ClockResult::NotInitialized,
              "an empty native model-table slot is not an admitted existing model");
        Check(ExtraSnapshot(image) == before, "invalid and absent models leave the bank untouched");

        ExtraWriteHost::Reset();
        ExtraWriteHost::admissionStep = [&](unsigned count)
        {
            if (count == 1)
                ModelCell(image, 346, objects.Second());
        };
        Check(ExtraMemory::SetDword(reader, image, 0, NativeWeaponDwordField::Model, 346, ExtraWriteHost::Admit) == ClockResult::Busy && !ExtraWriteHost::calls,
              "native model pointer replacement during pre-CAS metadata bookend refuses without mutation");
        Check(ExtraSnapshot(image) == before, "pre-CAS model publisher change preserves all row bytes");
        ModelCell(image, 346, objects.First());
        ExtraWriteHost::Reset();
        const auto replacementMethod = image + P::Code[1].rva;
        ExtraWriteHost::admissionStep = [&](unsigned count)
        {
            if (count == 1)
                fixture.Patch(ExtraFixture::ModelVtableRva + E::ModelVirtualMethodOffset, &replacementMethod, sizeof(replacementMethod));
        };
        Check(ExtraMemory::SetDword(reader, image, 0, NativeWeaponDwordField::Model, 346, ExtraWriteHost::Admit) == ClockResult::Busy && !ExtraWriteHost::calls,
              "changed executable model method identity during metadata bookend refuses");
        fixture.Patch(ExtraFixture::ModelVtableRva + E::ModelVirtualMethodOffset, &method, sizeof(method));
        ExtraWriteHost::Reset();
        DWORD old = 0, ignored = 0;
        Check(VirtualProtect(reinterpret_cast<void*>(objects.memory), 4096, PAGE_NOACCESS, &old) != FALSE, "protect only owned model test storage");
        const auto inaccessible = ExtraMemory::SetDword(reader, image, 0, NativeWeaponDwordField::Model, 346, ExtraWriteHost::Admit);
        Check(VirtualProtect(reinterpret_cast<void*>(objects.memory), 4096, old, &ignored) != FALSE, "restore owned model storage protection");
        Check(inaccessible == ClockResult::BindingChanged && !ExtraWriteHost::calls && ExtraSnapshot(image) == before,
              "unreadable model object is refused without dereferencing or invoking it");
        const auto originalVtable = *reinterpret_cast<std::uintptr_t*>(objects.First());
        *reinterpret_cast<std::uintptr_t*>(objects.First()) = 0;
        Check(ExtraMemory::SetDword(reader, image, 0, NativeWeaponDwordField::Model, 346, ExtraWriteHost::Admit) == ClockResult::BindingChanged,
              "missing borrowed model vtable is not readiness");
        *reinterpret_cast<std::uintptr_t*>(objects.First()) = originalVtable;
        const auto dataTarget = image + E::ModelTableRva;
        fixture.Patch(ExtraFixture::ModelVtableRva + E::ModelVirtualMethodOffset, &dataTarget, sizeof(dataTarget));
        Check(ExtraMemory::SetDword(reader, image, 0, NativeWeaponDwordField::Model, 346, ExtraWriteHost::Admit) == ClockResult::BindingChanged &&
                  !ExtraWriteHost::calls,
              "nonexecuting vtable method cannot qualify a native model-table object");
        fixture.Patch(ExtraFixture::ModelVtableRva + E::ModelVirtualMethodOffset, &method, sizeof(method));
    }

    void ExtraFailureChecks(OwnedImage& fixture, NativeWeaponInfoReader& reader, ModelObjects& objects)
    {
        const auto            image = fixture.image;
        constexpr std::size_t slot = 4;
        ExtraWriteHost::Reset();
        ExtraPut(image, slot, 0x6c, std::int16_t{2});
        auto before = ExtraSnapshot(image);
        ExtraWriteHost::expireAt = 2;
        Check(ExtraMemory::SetShort(reader, image, slot, NativeWeaponShortField::AimOffsetIndex, 20, ExtraWriteHost::Admit) == ClockResult::Expired &&
                  !ExtraWriteHost::calls,
              "owner loss after metadata read and before actual CAS refuses without mutation");
        Check(ExtraSnapshot(image) == before, "pre-CAS scope expiry preserves the complete native bank");
        ExtraWriteHost::Reset();
        ExtraWriteHost::expireAt = 3;
        Check(ExtraMemory::SetShort(reader, image, slot, NativeWeaponShortField::AimOffsetIndex, 20, ExtraWriteHost::Admit) == ClockResult::Expired &&
                  ExtraWriteHost::calls == 1,
              "owner loss after exchange reports the actual failure");
        ExtraExact(before, image, slot, 0x6c, std::int16_t{20});
        ExtraWriteHost::Reset();
        before = ExtraSnapshot(image);
        ExtraWriteHost::expireAt = 4;
        Check(ExtraMemory::SetShort(reader, image, slot, NativeWeaponShortField::AimOffsetIndex, 3, ExtraWriteHost::Admit) == ClockResult::Expired &&
                  ExtraWriteHost::calls == 1,
              "owner loss after final metadata recheck is also an honest post-exchange failure");
        ExtraExact(before, image, slot, 0x6c, std::int16_t{3});
        ExtraWriteHost::Reset();

        ModelCell(image, 346, objects.First());
        ExtraPut(image, slot, 0x0c, std::int32_t{-1});
        before = ExtraSnapshot(image);
        ExtraWriteHost::after = [&]
        {
            ModelCell(image, 346, objects.Second());
            ExtraPut(image, slot, 0x18, UINT32_C(0xabcdef01));
        };
        Check(ExtraMemory::SetDword(reader, image, slot, NativeWeaponDwordField::Model, 346, ExtraWriteHost::Admit) == ClockResult::WriteFailure &&
                  ExtraWriteHost::calls == 1,
              "model metadata changed after exchange is an honest post-write failure");
        auto                expected = before;
        const std::uint32_t index = 346, foreignFlags = UINT32_C(0xabcdef01);
        std::memcpy(expected.data() + slot * P::RowBytes + 0x0c, &index, sizeof(index));
        std::memcpy(expected.data() + slot * P::RowBytes + 0x18, &foreignFlags, sizeof(foreignFlags));
        Check(ExtraSnapshot(image) == expected, "postmetadata failure preserves accepted field intent and foreign row edits without whole-row rollback");
        std::uintptr_t publishedModel = 0;
        std::memcpy(&publishedModel, reinterpret_cast<void*>(image + E::ModelTableRva + 346 * sizeof(publishedModel)), sizeof(publishedModel));
        Check(publishedModel == objects.Second(), "postmetadata failure never restores a foreign model-table publisher");
        ModelCell(image, 346, objects.First());
        ExtraWriteHost::Reset();

        ExtraPut(image, slot, 0x40, 0.5f);
        before = ExtraSnapshot(image);
        ExtraWriteHost::fault = true;
        Check(ExtraMemory::SetFloat(reader, image, slot, NativeWeaponFloatField::AnimLoopStart, 2.0f, ExtraWriteHost::Admit) == ClockResult::WriteFailure &&
                  ExtraWriteHost::calls == 1,
              "new32-bit field catches an actual raised CAS fault");
        Check(ExtraSnapshot(image) == before, "failed new32-bit CAS writes no bank data");
        ExtraWriteHost::Reset();
        ExtraWriteHost::fault = true;
        Check(ExtraMemory::SetShort(reader, image, slot, NativeWeaponShortField::DefaultCombo, 8, ExtraWriteHost::Admit) == ClockResult::WriteFailure,
              "masked16-bit new field catches actual CAS fault");
        Check(ExtraSnapshot(image) == before, "failed masked16-bit CAS preserves its neighbor and all rows");
        ExtraWriteHost::Reset();

        // Determine the real successful guarded-copy count, then fail each OS
        // copy in turn. A failure after CAS is not called a clean refusal and
        // is never hidden behind a restoration of unrelated native fields.
        const auto firstRead = WeaponTestHost::reads.load();
        Check(ExtraMemory::SetDword(reader, image, slot, NativeWeaponDwordField::Model, 346, ExtraWriteHost::Admit) == ClockResult::Ok,
              "measure actual guarded native model-copy path");
        const auto copyCount = WeaponTestHost::reads.load() - firstRead;
        Check(copyCount > 0, "successful model intent used real OS copies");
        unsigned partialEffects = 0;
        for (std::size_t failure = 1; failure <= copyCount; ++failure)
        {
            ExtraWriteHost::Reset();
            ExtraPut(image, slot, 0x0c, std::int32_t{-1});
            before = ExtraSnapshot(image);
            WeaponTestHost::failAt = WeaponTestHost::reads.load() + failure;
            const auto result = ExtraMemory::SetDword(reader, image, slot, NativeWeaponDwordField::Model, 346, ExtraWriteHost::Admit);
            WeaponTestHost::failAt = 0;
            Check(result != ClockResult::Ok, "every failed guarded model OS copy refuses success");
            if (ExtraWriteHost::calls)
            {
                ++partialEffects;
                ExtraExact(before, image, slot, 0x0c, std::uint32_t{346});
            }
            else
                Check(ExtraSnapshot(image) == before, "failed pre-CAS model copy leaves all bank bytes unchanged");
        }
        Check(partialEffects != 0, "model copy failures exercise honest accepted/partial effects after exchange");
        ExtraWriteHost::Reset();

        before = ExtraSnapshot(image);
        ClockResult wrongThread = ClockResult::Ok;
        std::thread foreign([&]
                            { wrongThread = ExtraMemory::SetShort(reader, image, slot, NativeWeaponShortField::AimOffsetIndex, 1, ExtraWriteHost::Admit); });
        foreign.join();
        Check(wrongThread == ClockResult::WrongThread && ExtraSnapshot(image) == before && !ExtraWriteHost::calls,
              "wrong-thread extra writes refuse before touching native fields");
        Check(
            ExtraMemory::SetShort(reader, image, P::RowCount, NativeWeaponShortField::AimOffsetIndex, 1, ExtraWriteHost::Admit) == ClockResult::InvalidArgument,
            "extra fields retain exact80-row bounds");
        Check(ExtraMemory::SetShort(reader, image, slot, static_cast<NativeWeaponShortField>(9999), 1, ExtraWriteHost::Admit) == ClockResult::InvalidArgument,
              "unknown extra short field is not a raw offset API");
        Check(ExtraMemory::SetDword(reader, image, slot, static_cast<NativeWeaponDwordField>(9999), 1, ExtraWriteHost::Admit) == ClockResult::InvalidArgument,
              "unknown extra DWORD field is not a raw offset API");
        Check(
            ExtraMemory::SetFloat(reader, image, slot, static_cast<NativeWeaponFloatField>(9999), 1.0f, ExtraWriteHost::Admit) == ClockResult::InvalidArgument,
            "unknown extra float field is not a raw offset API");
        const auto actualModule = WeaponTestHost::image;
        WeaponTestHost::image = 0;
        const auto wrongImage = ExtraMemory::SetShort(reader, image, slot, NativeWeaponShortField::AimOffsetIndex, 1, ExtraWriteHost::Admit);
        WeaponTestHost::image = actualModule;
        Check(wrongImage == ClockResult::BindingChanged && ExtraSnapshot(image) == before, "unadmitted main-image identity refuses without mutation");
    }

    void ExtraProfileChecks(OwnedImage& fixture, NativeWeaponInfoReader& reader)
    {
        ExtraWriteHost::Reset();
        for (const auto& pin : E::Code)
        {
            for (const auto index : {std::size_t{0}, pin.size - 1})
            {
                const auto rva = pin.rva + index;
                const auto original = *reinterpret_cast<const unsigned char*>(fixture.image + rva);
                const auto corrupt = static_cast<unsigned char>(original ^ 1);
                const auto before = ExtraSnapshot(fixture.image);
                fixture.Patch(rva, &corrupt, 1);
                Check(ExtraMemory::SetShort(reader, fixture.image, 0, NativeWeaponShortField::AimOffsetIndex, 0, ExtraWriteHost::Admit) ==
                          ClockResult::BindingChanged,
                      "every new producer/consumer pin endpoint is required by actual writes");
                Check(ExtraSnapshot(fixture.image) == before && !ExtraWriteHost::calls,
                      "changed extra code provenance preserves all fields and reaches no CAS");
                fixture.Patch(rva, &original, 1);
            }
        }
        const auto rva = E::Code[0].rva;
        const auto original = *reinterpret_cast<const unsigned char*>(fixture.image + rva);
        const auto corrupt = static_cast<unsigned char>(original ^ 1);
        ExtraWriteHost::admissionStep = [&](unsigned count)
        {
            if (count == 1)
                fixture.Patch(rva, &corrupt, 1);
        };
        const auto before = ExtraSnapshot(fixture.image);
        Check(
            ExtraMemory::SetShort(reader, fixture.image, 0, NativeWeaponShortField::AimOffsetIndex, 0, ExtraWriteHost::Admit) == ClockResult::BindingChanged &&
                !ExtraWriteHost::calls,
            "new native provenance is rechecked after initial row/metadata capture before CAS");
        Check(ExtraSnapshot(fixture.image) == before, "late pre-CAS pin loss preserves the complete bank");
        fixture.Patch(rva, &original, 1);
        ExtraWriteHost::Reset();
    }
}

int wmain(int argc, wchar_t** argv)
{
    Check(argc == 2, "owned extra-field host receives only its workspace evidence directory");
    OwnedImage fixture;
    fixture.Create(argv[1]);
    WeaponTestHost::image = fixture.image;
    NativeWeaponInfoReader reader;
    Check(reader.Bind() == Status::Ok, "actual qualified native weapon reader binds owned sparse OS image");
    ModelObjects objects;
    objects.Initialize(fixture.image);
    ExtraScalarChecks(reader, fixture.image);
    ExtraAimComboChecks(reader, fixture.image);
    ExtraModelChecks(fixture, reader, objects);
    ExtraFailureChecks(fixture, reader, objects);
    ExtraProfileChecks(fixture, reader);
    reader.Reset();
    Check(!reader.IsBound(), "extra-field fixture retires only its owned reader state");
    std::printf("Passed %u native weapon-stat extra-field checks (64-bit)\n", checks);
    return 0;
}
