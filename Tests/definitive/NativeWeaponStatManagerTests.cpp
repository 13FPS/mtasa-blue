/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual manager SDK ownership and live bank behavior on owned images
 *****************************************************************************/
#include <windows.h>
#include "../../Client/game_de/NativeWeaponInfoExtraProfile.h"
#include "../../Client/game_de/NativeWeaponInfoVectorProfile.h"
#include "../../Client/game_de/NativeWeaponInfoWriteProfile.h"
#include <array>
#include <cstdint>
#include <vector>

namespace WeaponManagerSeed
{
    constexpr std::uintptr_t VtableRva = 0x4304000;
    template <class Seed>
    void Image(Seed&& seed, DWORD code, DWORD literal, DWORD bank)
    {
        namespace E = mta::game_de::NativeWeaponInfoExtraProfile;
        namespace V = mta::game_de::NativeWeaponInfoVectorProfile;
        namespace W = mta::game_de::NativeWeaponInfoWriteProfile;
        for (const auto& pin : E::Code)
            seed(pin.rva, pin.bytes.data(), pin.size, code);
        for (const auto& pin : V::Code)
            seed(pin.rva, pin.bytes.data(), pin.size, code);
        seed(W::AnimationCountLoadRva, W::AnimationCountLoad.data(), W::AnimationCountLoad.size(), code);
        const std::int32_t animationCount = 256;
        seed(W::AnimationCountRva, &animationCount, sizeof(animationCount), bank);
        const std::vector<std::uintptr_t> models(E::ModelCount);
        seed(E::ModelTableRva, models.data(), models.size() * sizeof(models[0]), bank);
        const std::array<unsigned char, E::AimRecordBytes * E::AimRecordCount> aim{};
        seed(E::AimTableRva, aim.data(), aim.size(), bank);
        const std::array<unsigned char, E::ComboRecordBytes * 11> combos{};
        seed(E::ComboTableRva, combos.data(), combos.size(), bank);
        const std::array<std::uintptr_t, 13> vtable{};
        seed(VtableRva, vtable.data(), sizeof(vtable), literal);
    }
}
#define MTA_DE_WEAPON_FIXTURE_EXTRA_SEED(seed, code, literal, bank) WeaponManagerSeed::Image(seed, code, literal, bank)
#pragma push_macro("wmain")
#undef wmain
#define wmain RetainedWeaponReaderFixtureMain
#include "NativeWeaponInfoReaderTests.cpp"
#undef wmain
#pragma pop_macro("wmain")
#undef MTA_DE_WEAPON_FIXTURE_EXTRA_SEED

#define GetModuleHandleW  WeaponTestHost::Module
#define ReadProcessMemory WeaponTestHost::Read
#include "../../Client/game_de/NativeWeaponInfoWriteMemory.cpp"
#include "../../Client/game_de/NativeWeaponInfoVectorMemory.cpp"
#undef ReadProcessMemory
#undef GetModuleHandleW
#include "../../Client/game_de/CWeaponInfoWriteDE.cpp"
#include "../../Client/game_de/CWeaponInfoDE.cpp"
#include "../../Client/game_de/CWeaponInfoBankDE.cpp"
#include "../../Client/game_de/CWeaponStatNativeDE.cpp"
#include "../../Client/game_de/CWeaponStatManagerPolicyDE.cpp"
#include "../../Client/game_de/CWeaponStatManagerDE.cpp"
#include <cmath>
#include <limits>
#include <memory>

namespace WeaponManagerFixture
{
    using Manager = CWeaponStatManagerDE;
    using Bank = std::array<unsigned char, P::RowCount * P::RowBytes>;
    namespace Memory = NativeWeaponInfoWriteMemory;
    namespace VectorMemory = NativeWeaponInfoVectorMemory;

    Bank Snapshot(std::uintptr_t image)
    {
        Bank value;
        std::memcpy(value.data(), reinterpret_cast<const void*>(image + P::BankRva), value.size());
        return value;
    }
    NativeWeaponInfoValues GetRow(std::uintptr_t image, std::size_t slot)
    {
        NativeWeaponInfoValues value;
        std::memcpy(&value, reinterpret_cast<const void*>(image + P::BankRva + slot * P::RowBytes), sizeof(value));
        return value;
    }
    void SetRow(std::uintptr_t image, std::size_t slot, const NativeWeaponInfoValues& value)
    {
        std::memcpy(reinterpret_cast<void*>(image + P::BankRva + slot * P::RowBytes), &value, sizeof(value));
    }
    void ResetRows(std::uintptr_t image)
    {
        for (std::size_t slot = 0; slot < P::RowCount; ++slot)
            SetRow(image, slot, Row(slot));
    }
    template <class Operation>
    bool Invalid(Operation&& operation)
    {
        try
        {
            operation();
        }
        catch (const std::invalid_argument&)
        {
            return true;
        }
        return false;
    }
    template <class Operation>
    bool Unavailable(Operation&& operation)
    {
        try
        {
            operation();
        }
        catch (const std::runtime_error&)
        {
            return true;
        }
        return false;
    }

    // This fixture dispatches actual production primitives only to its owned
    // sparse OS image. Live callback/Stop integration has a separate real owner
    // fixture; no fixture record is exposed as a native CWeaponStat object.
    class Owner final : public NativeWeaponInfoWriteAccess, public NativeWeaponInfoVectorAccess
    {
    public:
        const NativeWeaponInfoReader&   reader;
        const std::uintptr_t            image;
        VectorMemory::FireOffsetJournal journal;
        bool                            allowed = true;
        unsigned                        calls = 0, failAt = 0;
        std::size_t                     lastSlot = P::RowCount;
        explicit Owner(const NativeWeaponInfoReader& source, std::uintptr_t base) : reader(source), image(base) {}
        bool Call(std::size_t slot) noexcept
        {
            ++calls;
            lastSlot = slot;
            return allowed && calls != failAt;
        }
        ClockResult SetWeaponFloat(std::size_t slot, NativeWeaponFloatField field, float value) noexcept override
        {
            return Call(slot) ? Memory::SetFloat(reader, image, slot, field, value) : ClockResult::Expired;
        }
        ClockResult SetWeaponShort(std::size_t slot, NativeWeaponShortField field, std::int16_t value) noexcept override
        {
            return Call(slot) ? Memory::SetShort(reader, image, slot, field, value) : ClockResult::Expired;
        }
        ClockResult SetWeaponDword(std::size_t slot, NativeWeaponDwordField field, std::uint32_t value) noexcept override
        {
            return Call(slot) ? Memory::SetDword(reader, image, slot, field, value) : ClockResult::Expired;
        }
        ClockResult UpdateWeaponFlags(std::size_t slot, std::uint32_t mask, bool enabled) noexcept override
        {
            return Call(slot) ? Memory::UpdateFlags(reader, image, slot, mask, enabled) : ClockResult::Expired;
        }
        ClockResult BorrowWeaponFireOffset(std::size_t slot, CVector*& output) noexcept override
        {
            return Call(slot) ? VectorMemory::BorrowFireOffset(reader, image, slot, output) : ClockResult::Expired;
        }
        ClockResult SetWeaponFireOffset(std::size_t slot, const CVector* input) noexcept override
        {
            return Call(slot) ? VectorMemory::SetFireOffset(reader, image, slot, input, nullptr, &journal) : ClockResult::Expired;
        }
        ClockResult SetWeaponSlot(std::size_t slot, eWeaponSlot value) noexcept override
        {
            return Call(slot) ? VectorMemory::SetSlot(reader, image, slot, value) : ClockResult::Expired;
        }
    };

    class ModelObjects final
    {
    public:
        std::uintptr_t storage = reinterpret_cast<std::uintptr_t>(VirtualAlloc(nullptr, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
        ~ModelObjects()
        {
            if (storage)
                VirtualFree(reinterpret_cast<void*>(storage), 0, MEM_RELEASE);
        }
        void Publish(OwnedImage& fixture)
        {
            Check(storage != 0, "allocate only an owned model-metadata pointer for default model prerequisites");
            const std::uintptr_t table = fixture.image + WeaponManagerSeed::VtableRva;
            std::memcpy(reinterpret_cast<void*>(storage), &table, sizeof(table));
            const std::uintptr_t method = fixture.image + NativeWeaponInfoExtraProfile::Code[0].rva;
            fixture.Patch(WeaponManagerSeed::VtableRva + NativeWeaponInfoExtraProfile::ModelVirtualMethodOffset, &method, sizeof(method));
            // The genuine weapon writer qualifies these identities but never
            // executes a model virtual. No modeled metadata becomes a live row.
            for (unsigned model = 300; model != 401; ++model)
                std::memcpy(reinterpret_cast<void*>(fixture.image + NativeWeaponInfoExtraProfile::ModelTableRva + model * sizeof(storage)), &storage,
                            sizeof(storage));
        }
    };

    void EmptyAndForeign(NativeWeaponInfoReader& reader, std::uintptr_t image)
    {
        Owner             owner(reader, image);
        CWeaponInfoBankDE bank(reader, owner, owner), foreignBank(reader, owner, owner);
        const auto        before = Snapshot(image);
        const auto        reads = WeaponTestHost::reads.load();
        {
            Manager             manager(bank, owner);
            CWeaponStatManager& sdk = manager;
            Check(!sdk.GetWeaponStats(WEAPONTYPE_PISTOL) && !sdk.GetWeaponStatsFromSkillLevel(WEAPONTYPE_PISTOL, 1000) &&
                      sdk.GetWeaponSkillFromSkillLevel(WEAPONTYPE_PISTOL, 1000) == WEAPONSKILL_STD,
                  "actual constructor preserves MTA's empty live registry until explicit factory calls");
            Check(sdk.GetOriginalWeaponStats(WEAPONTYPE_PISTOL) && sdk.GetOriginalWeaponStats(WEAPONTYPE_PISTOL)->GetMaximumClipAmmo() == 17 &&
                      owner.calls == 0 && WeaponTestHost::reads.load() == reads && Snapshot(image) == before,
                  "constructor builds mutable CPU originals without touching native data or reading the bank");
            unsigned char marker = 0;
            for (auto* invalid : {static_cast<CWeaponInfo*>(nullptr), foreignBank.GetSlot(22), reinterpret_cast<CWeaponInfo*>(&marker)})
                Check(Invalid([&] { sdk.CreateWeaponStat(invalid, WEAPONTYPE_PISTOL, WEAPONSKILL_STD); }),
                      "factory refuses null, foreign genuine bank and arbitrary pointer before native access");
            Check(owner.calls == 0 && !sdk.GetWeaponStats(WEAPONTYPE_PISTOL) && Snapshot(image) == before,
                  "invalid factory identities neither append live records nor write defaults");
            sdk.ResetLists();
            Check(owner.calls == 0, "the shared intentionally disabled ResetLists performs no engine initialization");
        }
        Check(owner.calls == 0 && Snapshot(image) == before, "empty concrete manager destruction requires no native writer or row restoration");
    }

    void EveryBankIdentity(NativeWeaponInfoReader& reader, std::uintptr_t image)
    {
        Owner                                 owner(reader, image);
        CWeaponInfoBankDE                     bank(reader, owner, owner);
        std::array<CWeaponStat*, P::RowCount> values{};
        const auto                            before = Snapshot(image);
        Bank                                  committed{};
        unsigned                              callsAtRetirement = 0;
        std::size_t                           readsAtRetirement = 0;
        {
            Manager             manager(bank, owner);
            CWeaponStatManager& sdk = manager;
            for (std::size_t row = 0; row < values.size(); ++row)
            {
                // Actual MTA permits a metadata key without default data. This
                // creates the real wrapper without changing the chosen row.
                sdk.CreateWeaponStat(bank.GetSlot(row), WEAPONTYPE_UNARMED, WEAPONSKILL_STD);
                auto* value = sdk.GetWeaponStats(WEAPONTYPE_UNARMED);
                Check(value && value->GetWeaponRange() == Row(row).weaponRange && value->GetModel() == static_cast<eWeaponModel>(Row(row).model),
                      "each exact retained bank identity supplies a complete genuine native-backed SDK stat");
                value->SetWeaponType(WEAPONTYPE_INVALID);
                values[row] = value;
            }
            Check(owner.calls == 0 && Snapshot(image) == before,
                  "unsupported default metadata follows classic success convention without manufacturing native writes");
            for (std::size_t row = 0; row < values.size(); ++row)
            {
                const auto skill = row < 47 ? WEAPONSKILL_STD : row < 58 ? WEAPONSKILL_POOR : row < 69 ? WEAPONSKILL_PRO : WEAPONSKILL_SPECIAL;
                const auto type = static_cast<eWeaponType>(row < 47 ? row : 22 + (row - 47) % 11);
                values[row]->SetWeaponType(type);
                values[row]->SetWeaponSkillLevel(skill);
                Check(sdk.GetWeaponStats(type, skill) == values[row], "live metadata follows actual shared MTA first-match bank/skill selection");
                values[row]->SetWeaponRange(1000.25f + static_cast<float>(row));
                Check(owner.lastSlot == row && bank.GetSlot(row)->GetWeaponRange() == values[row]->GetWeaponRange(),
                      "metadata edits do not rebind a live stat away from its exact original native row");
                values[row]->SetCombosAvailable(static_cast<BYTE>(row));
                Check(GetRow(image, row).combosAvailable == static_cast<BYTE>(row) && GetRow(image, row).defaultCombo == Row(row).defaultCombo,
                      "complete stat count setter changes its genuine high byte while preserving adjacent default combo");
            }
            const auto writes = owner.calls;
            const auto current = Snapshot(image);
            sdk.ResetLists();
            Check(owner.calls == writes && Snapshot(image) == current, "ResetLists does not replay defaults over any of the eighty native stats");
            committed = current;
            callsAtRetirement = owner.calls;
            readsAtRetirement = WeaponTestHost::reads.load();
        }
        const auto after = Snapshot(image);
        Check(after == committed && after != before && owner.calls == callsAtRetirement && WeaponTestHost::reads.load() == readsAtRetirement,
              "concrete wrapper retirement requires no native reads or writes and leaves all committed data untouched");
        Check(bank.GetSlot(79)->GetWeaponRange() == 1079.25f,
              "manager retires typed wrappers while leaving its borrowed eighty bank identities and accepted native intent intact");
    }

    void DefaultsAndMutableOriginals(NativeWeaponInfoReader& reader, std::uintptr_t image)
    {
        Owner               owner(reader, image);
        CWeaponInfoBankDE   bank(reader, owner, owner);
        Manager             manager(bank, owner);
        CWeaponStatManager& sdk = manager;
        sdk.CreateWeaponStat(bank.GetSlot(22), WEAPONTYPE_PISTOL, WEAPONSKILL_STD);
        auto*       first = sdk.GetWeaponStats(WEAPONTYPE_PISTOL);
        const auto* defaults = MtaGame::CWeaponStatDefaults::Select(WEAPONTYPE_PISTOL, WEAPONSKILL_STD);
        Check(first && first->GetMaximumClipAmmo() == defaults->maximum_clip_ammo && first->GetModel() == static_cast<eWeaponModel>(defaults->model) &&
                  first->GetDefaultCombo() == defaults->default_combo && first->GetCombosAvailable() == defaults->combos_available &&
                  first->GetFireOffset() == bank.GetSlot(22)->GetFireOffset(),
              "actual factory applies the shared complete MTA defaults through retained native bank and borrowed vector alias");
        sdk.CreateWeaponStat(bank.GetSlot(22), WEAPONTYPE_PISTOL, WEAPONSKILL_STD);
        Check(sdk.GetWeaponStats(WEAPONTYPE_PISTOL) == first, "duplicate factory entries preserve actual insertion-order first-match behavior");
        first->SetWeaponType(WEAPONTYPE_INVALID);
        auto* second = sdk.GetWeaponStats(WEAPONTYPE_PISTOL);
        Check(second && second != first && second->GetWeaponRange() == first->GetWeaponRange(),
              "changing first metadata exposes next owned native wrapper over the same bank identity");
        first->SetWeaponType(WEAPONTYPE_PISTOL);
        auto* original = sdk.GetOriginalWeaponStats(WEAPONTYPE_PISTOL);
        original->SetFlags(0);
        const auto beforeInit = owner.calls;
        sdk.Init();
        Check(original->GetFlags() == 0 && owner.calls == beforeInit, "shared Init keeps current mutable manager originals and does not reapply live defaults");
        first->SetFlags(0);
        first->SetFlagBits(WEAPONTYPE_ANIM_RELOAD);
        Check(!first->IsFlagSet(WEAPONTYPE_ANIM_RELOAD), "flag policy resolves the manager's currently mutable original rather than cached native defaults");
        original->SetFlags(WEAPONTYPE_ANIM_RELOAD);
        original->SetAnimGroup(9);
        first->SetFlagBits(WEAPONTYPE_ANIM_RELOAD | WEAPONTYPE_TWIN_PISTOLS);
        Check(first->IsFlagSet(WEAPONTYPE_ANIM_RELOAD) && first->GetAnimGroup() == 17,
              "actual shared flag policy and dual-wield transition reach the native SDK writers");
        first->ClearFlagBits(WEAPONTYPE_TWIN_PISTOLS);
        Check(first->GetAnimGroup() == 9, "removing dual wield reads the later mutable original animation group");
        std::unique_ptr<MtaGame::CWeaponStatOwned> cpu(
            static_cast<MtaGame::CWeaponStatOwned*>(sdk.CreateWeaponStatUnlisted(WEAPONTYPE_PISTOL, WEAPONSKILL_STD)));
        cpu->SetDamagePerHit(1234);
        Check(first->GetDamagePerHit() == defaults->damage && sdk.GetWeaponStats(WEAPONTYPE_PISTOL) == first,
              "unlisted complete CPU custom data stays separate from registered native live rows");
        first->SetRequiredStatLevel(200);
        sdk.CreateWeaponStat(bank.GetSlot(47), WEAPONTYPE_PISTOL, WEAPONSKILL_POOR);
        sdk.CreateWeaponStat(bank.GetSlot(58), WEAPONTYPE_PISTOL, WEAPONSKILL_PRO);
        auto* poor = sdk.GetWeaponStats(WEAPONTYPE_PISTOL, WEAPONSKILL_POOR);
        auto* professional = sdk.GetWeaponStats(WEAPONTYPE_PISTOL, WEAPONSKILL_PRO);
        professional->SetRequiredStatLevel(700);
        Check(sdk.GetWeaponStatsFromSkillLevel(WEAPONTYPE_PISTOL, 199) == poor && sdk.GetWeaponStatsFromSkillLevel(WEAPONTYPE_PISTOL, 200) == first &&
                  sdk.GetWeaponStatsFromSkillLevel(WEAPONTYPE_PISTOL, 700) == professional &&
                  sdk.GetWeaponStatsFromSkillLevel(WEAPONTYPE_PISTOL, (std::numeric_limits<float>::quiet_NaN)()) == poor,
              "native manager reuses actual MTA threshold ordering and NaN-to-poor selection");
        Check(sdk.GetWeaponSkillFromSkillLevel(WEAPONTYPE_PISTOL, 700) == WEAPONSKILL_PRO,
              "native manager's skill enumeration follows the same existing shared selection");
    }

    void RefusalOrderingAndRetirement(NativeWeaponInfoReader& reader, std::uintptr_t image)
    {
        Owner             owner(reader, image);
        CWeaponInfoBankDE bank(reader, owner, owner);
        const auto        before = Snapshot(image);
        unsigned          callsAtRetirement = 0;
        Bank              accepted{};
        {
            Manager             manager(bank, owner);
            CWeaponStatManager& sdk = manager;
            owner.allowed = false;
            Check(Unavailable([&] { sdk.CreateWeaponStat(bank.GetSlot(22), WEAPONTYPE_PISTOL, WEAPONSKILL_STD); }) && sdk.GetWeaponStats(WEAPONTYPE_PISTOL) &&
                      Snapshot(image) == before && owner.calls == 1,
                  "first default-write refusal still preserves classic append-before-LoadDefault ordering");
            auto* first = sdk.GetWeaponStats(WEAPONTYPE_PISTOL);
            first->SetWeaponType(WEAPONTYPE_INVALID);
            owner.allowed = true;
            owner.failAt = owner.calls + 3;
            Check(Unavailable([&] { sdk.CreateWeaponStat(bank.GetSlot(22), WEAPONTYPE_PISTOL, WEAPONSKILL_STD); }),
                  "later default-write refusal propagates through the actual SDK factory");
            auto*       partial = sdk.GetWeaponStats(WEAPONTYPE_PISTOL);
            const auto* defaults = MtaGame::CWeaponStatDefaults::Select(WEAPONTYPE_PISTOL, WEAPONSKILL_STD);
            auto        expected = before;
            std::memcpy(expected.data() + 22 * P::RowBytes + offsetof(NativeWeaponInfoValues, weaponRange), &defaults->weapon_range, sizeof(float));
            std::memcpy(expected.data() + 22 * P::RowBytes + offsetof(NativeWeaponInfoValues, targetRange), &defaults->target_range, sizeof(float));
            Check(partial && partial != first && Snapshot(image) == expected,
                  "partial default failure keeps only accepted first two native fields and registered concrete identity without row rollback");
            owner.failAt = 0;
            partial->SetWeaponRange(901.25f);
            Check(bank.GetSlot(22)->GetWeaponRange() == 901.25f, "partially initialized native stat remains the original live wrapper on later admitted use");
            owner.allowed = false;
            accepted = Snapshot(image);
            callsAtRetirement = owner.calls;
            Check(Unavailable([&] { partial->SetCombosAvailable(3); }) && Snapshot(image) == accepted,
                  "retained complete native stat cannot bypass a refusing count writer");
            callsAtRetirement = owner.calls;
            reader.Reset();
        }
        Check(owner.calls == callsAtRetirement && Snapshot(image) == accepted,
              "unregister-before-concrete deletion performs no default writes, native reads or rollback after owner retirement");
        Check(reader.Bind() == Status::Ok && bank.GetSlot(22)->GetWeaponRange() == 901.25f,
              "native bank survives manager retirement and observes persistent accepted intent after its own reader rebind");
    }
}

int wmain(int argc, wchar_t** argv)
{
    static_assert(!std::is_abstract_v<CWeaponStatNativeDE> && !std::is_abstract_v<CWeaponStatManagerDE>);
    static_assert(!std::has_virtual_destructor_v<CWeaponStat> && !std::has_virtual_destructor_v<CWeaponStatManager>);
    Check(argc == 2, "manager fixture receives only its workspace evidence directory");
    OwnedImage fixture;
    fixture.Create(argv[1]);
    WeaponTestHost::image = fixture.image;
    NativeWeaponInfoReader reader;
    Check(reader.Bind() == Status::Ok, "actual weapon reader binds the owned sparse Windows image");
    WeaponManagerFixture::ModelObjects models;
    models.Publish(fixture);
    WeaponManagerFixture::EmptyAndForeign(reader, fixture.image);
    WeaponManagerFixture::EveryBankIdentity(reader, fixture.image);
    WeaponManagerFixture::ResetRows(fixture.image);
    WeaponManagerFixture::DefaultsAndMutableOriginals(reader, fixture.image);
    WeaponManagerFixture::ResetRows(fixture.image);
    WeaponManagerFixture::RefusalOrderingAndRetirement(reader, fixture.image);
    reader.Reset();
    std::printf("Passed %u actual native weapon-stat manager SDK lifecycle checks (64-bit)\n", checks);
    return 0;
}
