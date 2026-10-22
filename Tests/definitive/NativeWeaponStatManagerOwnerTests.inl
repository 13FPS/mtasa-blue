/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Publish one complete manager through the existing native owner
 ****************************************************************************/
struct WeaponManagerOwnerContext
{
    CWeaponStatManager* manager = nullptr;
    CWeaponStat*        stat = nullptr;
    unsigned            calls = 0;
};

void WeaponManagerOwnerHandler(NativeGameTimingAccess& access, void* opaque)
{
    auto& context = *static_cast<WeaponManagerOwnerContext*>(opaque);
    ++context.calls;
    CWeaponStatManager* selected = nullptr;
    Check(access.GetWeaponStatManager(selected) == ClockResult::Ok && selected == &runtime.weaponStatManager,
          "actual callback publishes the complete manager owned by this same runtime and bank");
    CWeaponStatManager* repeated = nullptr;
    Check(access.GetWeaponStatManager(repeated) == ClockResult::Ok && repeated == selected,
          "qualified manager publication reuses exact SDK identity without a second native owner");
    if (context.manager)
    {
        Check(context.manager == selected && selected->GetWeaponStats(WEAPONTYPE_PISTOL) == context.stat,
              "later callback reuses existing manager and live stat without replaying default population");
        return;
    }
    context.manager = selected;
    Check(!selected->GetWeaponStats(WEAPONTYPE_PISTOL) && selected->GetOriginalWeaponStats(WEAPONTYPE_PISTOL),
          "manager construction supplies CPU originals but leaves classic live population separate and empty");
    CWeaponInfo* info = nullptr;
    Check(access.GetWeaponInfoSlot(WEAPONTYPE_PISTOL, info) == ClockResult::Ok && info == runtime.weaponBank.GetSlot(WEAPONTYPE_PISTOL),
          "manager creates only from the existing admitted genuine bank wrapper");
    selected->CreateWeaponStat(info, WEAPONTYPE_PISTOL, WEAPONSKILL_STD);
    auto* stat = selected->GetWeaponStats(WEAPONTYPE_PISTOL);
    context.stat = stat;
    const auto* defaults = MtaGame::CWeaponStatDefaults::Select(WEAPONTYPE_PISTOL, WEAPONSKILL_STD);
    Check(stat && defaults && stat->GetModel() == static_cast<eWeaponModel>(defaults->model) && stat->GetDamagePerHit() == defaults->damage &&
              stat->GetCombosAvailable() == defaults->combos_available && stat->GetDefaultCombo() == defaults->default_combo,
          "complete manager reaches the final combo count while applying genuine MTA defaults through all existing typed writers");
    stat->SetDefaultCombo(14);
    for (BYTE count : std::array<BYTE, 3>{0, 1, 255})
    {
        stat->SetCombosAvailable(count);
        Check(stat->GetCombosAvailable() == count && stat->GetDefaultCombo() == 14,
              "complete stat high-byte count intent preserves the neighboring default-combo value");
    }
    selected->ResetLists();
    Check(selected->GetWeaponStats(WEAPONTYPE_PISTOL) == stat && selected->GetWeaponStatsFromSkillLevel(WEAPONTYPE_PISTOL, 50.0f) == stat &&
              selected->GetWeaponSkillFromSkillLevel(WEAPONTYPE_PISTOL, 50.0f) == WEAPONSKILL_STD,
          "complete manager retains actual MTA no-op reset and missing-skill fallback policies");
    stat->SetFlagBits(WEAPONTYPE_TWIN_PISTOLS);
    stat->ClearFlagBits(WEAPONTYPE_TWIN_PISTOLS);
    Check(stat->GetAnimGroup() == selected->GetOriginalWeaponStats(WEAPONTYPE_PISTOL)->GetAnimGroup(),
          "native complete stat resolves its current manager's real mutable original rather than an unrelated CPU record");
    bool refused = false;
    try
    {
        selected->CreateWeaponStat(reinterpret_cast<CWeaponInfo*>(std::uintptr_t{1}), WEAPONTYPE_PISTOL, WEAPONSKILL_STD);
    }
    catch (const std::invalid_argument&)
    {
        refused = true;
    }
    Check(refused && selected->GetWeaponStats(WEAPONTYPE_PISTOL) == stat,
          "complete factory refuses foreign SDK identities without dereferencing or replacing the selected native bank wrapper");
}

void NativeWeaponStatManagerOwner(ClockImageFixture& fixture)
{
    fixture.AdmitWeaponPins();
    fixture.AdmitWeaponVectorPins();
    ClockReaderTestHost::image = fixture.image;
    for (const auto& pin : NativeWeaponInfoExtraProfile::Code)
        fixture.Patch(pin.rva, pin.bytes.data(), pin.size);
    for (const auto& table :
         {std::pair<std::uintptr_t, std::size_t>{NativeWeaponInfoExtraProfile::AimTableRva,
                                                 NativeWeaponInfoExtraProfile::AimRecordBytes * NativeWeaponInfoExtraProfile::AimRecordCount},
          {NativeWeaponInfoExtraProfile::ComboTableRva, NativeWeaponInfoExtraProfile::ComboRecordBytes * 11}})
    {
        DWORD previous = 0;
        Check(::VirtualProtect(reinterpret_cast<void*>(fixture.image + table.first), table.second, PAGE_READWRITE, &previous) != FALSE,
              "fixture owns the current native aim/combo table span required by the real default loader");
    }
    // Existing SetModel admission requires only this real plain-C++ role:
    // a current bank object and read-only vtable+60 executable target. It
    // invokes no method and does not manufacture an SDK model or UObject.
    alignas(std::uintptr_t) std::array<unsigned char, 8> model{};
    constexpr std::uintptr_t                             modelVtableRva = 0x431c100;
    const auto                                           modelAddress = reinterpret_cast<std::uintptr_t>(model.data()), vtable = fixture.image + modelVtableRva;
    std::memcpy(model.data(), &vtable, sizeof(vtable));
    const auto method = fixture.image + NativeWeaponInfoExtraProfile::Code[0].rva;
    const auto previousMethod = fixture.Get<std::uintptr_t>(modelVtableRva + NativeWeaponInfoExtraProfile::ModelVirtualMethodOffset);
    fixture.Patch(modelVtableRva + NativeWeaponInfoExtraProfile::ModelVirtualMethodOffset, &method, sizeof(method));
    DWORD old = 0;
    Check(::VirtualProtect(reinterpret_cast<void*>(fixture.image + modelVtableRva), 0x68, PAGE_READONLY, &old) != FALSE,
          "fixture native model vtable retains genuine read-only image protection");
    const auto cell = NativeWeaponInfoExtraProfile::ModelTableRva + 346 * sizeof(std::uintptr_t);
    const auto previousModel = fixture.Get<std::uintptr_t>(cell);
    fixture.Put(cell, modelAddress);
    using Bank = std::array<NativeWeaponInfoValues, NativeWeaponInfoProfile::RowCount>;
    const auto                before = fixture.Get<Bank>(NativeWeaponInfoProfile::BankRva);
    WeaponManagerOwnerContext context;
    CWeaponStatManager*       preserved = reinterpret_cast<CWeaponStatManager*>(std::uintptr_t{1});
    NativeClockService        service;
    BeginOwned(service, fixture, false, true, true, &WeaponManagerOwnerHandler, &context);
    Check(runtime.access.GetWeaponStatManager(preserved) != ClockResult::Ok && preserved == reinterpret_cast<CWeaponStatManager*>(std::uintptr_t{1}),
          "retained runtime cannot publish its complete manager outside the actual native callback");
    fixture.invokeTimer();
    Check(context.calls == 1 && context.manager && context.stat && !runtime.stopping.load(),
          "complete manager/default operation finishes on the existing callback without introducing another service owner");
    const auto after = fixture.Get<Bank>(NativeWeaponInfoProfile::BankRva);
    bool       untouched = true;
    for (std::size_t row = 0; row != before.size(); ++row)
        if (row != WEAPONTYPE_PISTOL)
            untouched = untouched && std::memcmp(&before[row], &after[row], sizeof(before[row])) == 0;
    Check(untouched && after[WEAPONTYPE_PISTOL].combosAvailable == 255 && after[WEAPONTYPE_PISTOL].defaultCombo == 14,
          "whole manager writes only its selected row and keeps exact independent final-byte ownership");
    Check(service.Renew(runtime.generation.load()), "renew complete manager publication on the same retained owner");
    fixture.invokeTimer();
    Check(context.calls == 2 && std::memcmp(after.data(), fixture.Get<Bank>(NativeWeaponInfoProfile::BankRva).data(), sizeof(after)) == 0,
          "repeated publication and lookup do not initialize defaults or replay previous native writes");
    bool        foreignRefused = false;
    std::thread foreign(
        [&]
        {
            CWeaponStatManager* output = preserved;
            foreignRefused = runtime.access.GetWeaponStatManager(output) != ClockResult::Ok && output == preserved;
        });
    foreign.join();
    Check(foreignRefused, "foreign thread cannot inherit complete manager publication authority");
    bool writeRefused = false;
    try
    {
        context.stat->SetCombosAvailable(7);
    }
    catch (const std::runtime_error&)
    {
        writeRefused = true;
    }
    Check(writeRefused, "retained complete native stat identity cannot replace its current write callback");
    const auto calls = context.calls;
    if (!service.End())
    {
        fixture.invokeTimer();
        Check(service.End(), "natural callback retires complete weapon-manager owner");
    }
    Check(context.calls == calls && !service.IsRetained() && !runtime.weaponWriterReader.IsBound() && !runtime.weaponWriterGeneration &&
              std::memcmp(after.data(), fixture.Get<Bank>(NativeWeaponInfoProfile::BankRva).data(), sizeof(after)) == 0,
          "retirement releases callback/bank authority without replaying defaults or rolling back accepted weapon fields");
    preserved = context.manager;
    Check(runtime.access.GetWeaponStatManager(preserved) != ClockResult::Ok && preserved == context.manager,
          "retired owner refuses publication while preserving a caller's prior SDK identity");
    fixture.Put(cell, previousModel);
    fixture.Patch(modelVtableRva + NativeWeaponInfoExtraProfile::ModelVirtualMethodOffset, &previousMethod, sizeof(previousMethod));
    DWORD discarded = 0;
    Check(::VirtualProtect(reinterpret_cast<void*>(fixture.image + modelVtableRva), 0x68, old, &discarded) != FALSE,
          "temporary fixture vtable restores its prior image protection without touching persistent weapon-row intent");
    ClockReaderTestHost::image = 0;
    std::puts("Production complete weapon-stat manager callback owner fixture completed");
}
