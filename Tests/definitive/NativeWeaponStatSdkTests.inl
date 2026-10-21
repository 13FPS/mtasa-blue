/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Real retained bank/owner coverage for the native stat SDK slice
 ****************************************************************************/
// Included in the existing production runtime fixture. The one unqualified
// setter is completed only by this rejecting test subclass, never a product.
struct NativeStatOriginalTest
{
    MtaGame::WeaponStatOriginal        value{0, 7};
    unsigned                           reads = 0;
    eWeaponType                        lastWeapon = WEAPONTYPE_UNARMED;
    eWeaponSkill                       lastSkill = WEAPONSKILL_POOR;
    static MtaGame::WeaponStatOriginal Lookup(void* opaque, eWeaponType weapon, eWeaponSkill skill)
    {
        auto& original = *static_cast<NativeStatOriginalTest*>(opaque);
        ++original.reads;
        original.lastWeapon = weapon;
        original.lastSkill = skill;
        return original.value;
    }
};

class NativeStatSdkFixture final : public CWeaponStatNativeDE
{
public:
    NativeStatSdkFixture(CWeaponInfoBankDE& bank, NativeWeaponInfoWriteAccess& access, std::size_t slot, MtaGame::WeaponStatOriginalLookup lookup,
                         void* context)
        : CWeaponStatNativeDE(bank, access, slot, WEAPONTYPE_PISTOL, WEAPONSKILL_STD, lookup, context)
    {
    }
    void SetCombosAvailable(BYTE) override { throw std::logic_error("unqualified native combo-count setter reached fixture"); }
};
static_assert(std::is_abstract_v<CWeaponStatNativeDE> && !std::is_abstract_v<NativeStatSdkFixture>);

struct NativeStatSdkContext
{
    NativeStatOriginalTest original;
    NativeStatSdkFixture*  object = nullptr;
    unsigned               calls = 0;
};

void NativeStatSdkHandler(NativeGameTimingAccess& access, void* opaque)
{
    auto& context = *static_cast<NativeStatSdkContext*>(opaque);
    ++context.calls;
    Check(context.object && callbackDepth == 1 && timingCallbackGeneration == runtime.generation.load(),
          "the native stat SDK test executes only through the real timing callback owner");
    CWeaponInfo* selected = nullptr;
    Check(access.GetWeaponInfoSlot(77, selected) == ClockResult::Ok && selected == runtime.weaponBank.GetSlot(77),
          "native stats borrow the runtime's existing genuine bank wrapper rather than a copied SDK record");
    auto& stat = *context.object;
    stat.SetModel(-1);
    stat.SetModel2(-1);
    stat.SetFireType(FIRETYPE_USE);
    stat.SetAimOffsetIndex(20);
    stat.SetDefaultCombo(14);
    stat.SetWeaponAnimLoopStart(1.25f);
    stat.SetWeaponAnimLoopStop(2.5f);
    stat.SetWeaponAnim2LoopStart(3.75f);
    stat.SetWeaponAnim2LoopStop(4.5f);
    stat.SetWeaponAnim2LoopFireTime(5.25f);
    stat.SetRequiredStatLevel(0x01000001);
    stat.SetWeaponSpeed(6.5f);
    stat.SetWeaponRadius(7.75f);
    Check(stat.GetModel() == static_cast<eWeaponModel>(-1) && stat.GetModel2() == static_cast<eWeaponModel>(-1) && stat.GetFireType() == FIRETYPE_USE &&
              stat.GetAimOffsetIndex() == 20 && stat.GetDefaultCombo() == 14 && stat.GetCombosAvailable() == 4 &&
              stat.GetWeaponSpeed() == stat.GetFiringSpeed() && stat.GetWeaponRadius() == stat.GetRadius(),
          "the actual native SDK getters observe accepted extra-field intents and shared aliases while preserving the adjacent combo-count byte");
    const auto required = reinterpret_cast<const NativeWeaponInfoValues*>(runtime.image + NativeWeaponInfoProfile::BankRva)[77].requiredStat;
    Check(required == 0x01000001, "the integer SDK stat overload preserves bits that a float intermediate would round away");
    auto* alias = stat.GetFireOffset();
    Check(alias == selected->GetFireOffset() &&
              reinterpret_cast<std::uintptr_t>(alias) == runtime.image + NativeWeaponInfoProfile::BankRva + 77 * NativeWeaponInfoProfile::RowBytes + 0x24,
          "native stat fire offsets preserve the existing genuine bank's borrowed vector identity");
    stat.SetFlags(0);
    stat.SetFlagBits(WEAPONTYPE_ANIM_RELOAD);
    Check(stat.GetFlags() == 0, "shared MTA flag policy refuses reload enable absent from the current mutable original");
    context.original.value.flags = WEAPONTYPE_ANIM_RELOAD;
    stat.SetFlagBits(WEAPONTYPE_ANIM_RELOAD);
    stat.SetFlagBits(WEAPONTYPE_TWIN_PISTOLS);
    Check(stat.GetAnimGroup() == 17 && stat.IsFlagSet(WEAPONTYPE_ANIM_RELOAD), "shared dual-wield policy reaches actual native animation and flag writers");
    stat.SetWeaponType(WEAPONTYPE_MINIGUN);
    stat.SetWeaponSkillLevel(WEAPONSKILL_SPECIAL);
    context.original.value.animGroup = 9;
    stat.ClearFlagBits(WEAPONTYPE_TWIN_PISTOLS);
    Check(stat.GetAnimGroup() == 9 && context.original.lastWeapon == WEAPONTYPE_MINIGUN && context.original.lastSkill == WEAPONSKILL_SPECIAL &&
              context.original.reads >= 3 && stat.GetWeaponType() == WEAPONTYPE_MINIGUN && stat.GetWeaponSkillLevel() == WEAPONSKILL_SPECIAL,
          "metadata changes alter lazy mutable-original lookup without rebinding the selected native row");
}

void NativeWeaponStatSdkOwner(ClockImageFixture& fixture)
{
    fixture.AdmitWeaponPins();
    fixture.AdmitWeaponVectorPins();
    // Extra-field authority is independently pinned. This fixture uses the
    // native no-model sentinel, while the focused primitive suite owns live
    // model-table identity, boundary and virtual-method qualification cases.
    for (const auto& pin : NativeWeaponInfoExtraProfile::Code)
        fixture.Patch(pin.rva, pin.bytes.data(), pin.size);
    for (const auto& table :
         {std::pair<std::uintptr_t, std::size_t>{NativeWeaponInfoExtraProfile::AimTableRva,
                                                 NativeWeaponInfoExtraProfile::AimRecordBytes * NativeWeaponInfoExtraProfile::AimRecordCount},
          std::pair<std::uintptr_t, std::size_t>{NativeWeaponInfoExtraProfile::ComboTableRva, NativeWeaponInfoExtraProfile::ComboRecordBytes * 11}})
    {
        DWORD old = 0;
        Check(::VirtualProtect(reinterpret_cast<void*>(fixture.image + table.first), table.second, PAGE_READWRITE, &old) != FALSE,
              "admit only the owned native aim/combo spans required by the exact extra-field consumers");
    }
    ClockReaderTestHost::image = fixture.image;
    NativeStatSdkContext context;
    NativeStatSdkFixture stat(runtime.weaponBank, runtime.access, 77, &NativeStatOriginalTest::Lookup, &context.original);
    context.object = &stat;
    using Bank = std::array<NativeWeaponInfoValues, NativeWeaponInfoProfile::RowCount>;
    const auto         before = fixture.Get<Bank>(NativeWeaponInfoProfile::BankRva);
    NativeClockService service;
    BeginOwned(service, fixture, false, true, true, &NativeStatSdkHandler, &context);
    fixture.invokeTimer();
    const auto after = fixture.Get<Bank>(NativeWeaponInfoProfile::BankRva);
    Check(context.calls == 1 && !runtime.stopping.load(), "the real native stat SDK slice completes its admitted callback without a new service owner");
    bool neighborsUnchanged = true;
    for (std::size_t index = 0; index < before.size(); ++index)
        if (index != 77)
            neighborsUnchanged = neighborsUnchanged && std::memcmp(&before[index], &after[index], sizeof(before[index])) == 0;
    Check(neighborsUnchanged && after[77].model == -1 && after[77].model2 == -1 && after[77].requiredStat == 0x01000001,
          "native stat SDK intent remains in its original row and preserves the other seventy-nine rows");
    const auto refuses = [&](auto&& operation)
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
    };
    Check(refuses([&] { stat.SetModel(-1); }) && refuses([&] { stat.SetAimOffsetIndex(1); }) &&
              std::memcmp(after.data(), fixture.Get<Bank>(NativeWeaponInfoProfile::BankRva).data(), sizeof(after)) == 0,
          "matching producer identity between callbacks cannot borrow native model or aim write authority");
    bool        foreignRefused = false;
    std::thread foreign([&] { foreignRefused = refuses([&] { stat.SetDefaultCombo(14); }); });
    foreign.join();
    Check(foreignRefused, "a foreign thread cannot use an existing native stat object as a callback lease");
    // These writes are persistent MTA intent, so a pure stat generation has
    // no captured clock state to restore. End may retire immediately; if an
    // existing owner debt needs a callback, use that original path once.
    const auto calls = context.calls;
    if (!service.End())
    {
        fixture.invokeTimer();
        Check(service.End(), "the existing timer callback can finish native stat owner retirement");
    }
    Check(!service.IsRetained() && !runtime.hooked.load() && !runtime.timerHooked.load() && !runtime.timingHandler && !runtime.timingContext &&
              !runtime.weaponWriterReader.IsBound() && !runtime.weaponWriterGeneration && context.calls == calls &&
              !NativeServiceAdmission::IsClaimed(NativeServiceAdmission::Category::Clock),
          "native stat retirement releases real hooks, handler, bank write binding and shared admission without replaying the handler");
    Check(std::memcmp(after.data(), fixture.Get<Bank>(NativeWeaponInfoProfile::BankRva).data(), sizeof(after)) == 0 &&
              refuses([&] { stat.SetFireType(FIRETYPE_MELEE); }),
          "native stat intent persists through End while the retired owner refuses later writes");
    bool badRow = false, missingOriginal = false;
    try
    {
        NativeStatSdkFixture invalid(runtime.weaponBank, runtime.access, 80, &NativeStatOriginalTest::Lookup, &context.original);
    }
    catch (const std::invalid_argument&)
    {
        badRow = true;
    }
    try
    {
        NativeStatSdkFixture invalid(runtime.weaponBank, runtime.access, 77, nullptr, nullptr);
    }
    catch (const std::invalid_argument&)
    {
        missingOriginal = true;
    }
    Check(badRow && missingOriginal, "the native stat slice cannot manufacture a missing bank row or replace its mutable original provider");
    ClockReaderTestHost::image = 0;
    std::puts("Production native weapon-stat SDK slice fixture completed");
}
