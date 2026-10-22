/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Real selected-ped token ownership for native inline weapon scalars
 ****************************************************************************/
// Share the actual pool, registered native-visible reference cell, callback and
// retirement protocol. These scalar routes publish no weapon or ped factory.
{
    namespace W = NativePedWeaponProfile;
    for (const auto& pin : W::Code)
        admit(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
    for (const auto& pin : NativePedWeaponAmmoProfile::Code)
        admit(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
    for (const auto& pin : NativePedMetadataProfile::Code)
        admit(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
    ClockPedWeaponOwnerTest::fields = {};
    ClockPedWeaponOwnerTest::ammo = {};
    ClockPedWeaponOwnerTest::failFieldsAfter = ClockPedWeaponOwnerTest::failAmmoAfter = false;
    const auto initialize = [&]
    {
        for (std::uint32_t slot = 0; slot < W::Count; ++slot)
        {
            const auto at = W::ArrayOffset + slot * W::Stride;
            store(at, std::uint32_t{22} + slot);
            store(at + 4, 3);
            store(at + 8, 0x11223344);
            store(at + 12, 0x55667788);
            store(at + 16, 0xabcde123);
        }
        // An explicit inline slot does not borrow the current equipped slot.
        pool.objects[0xa10] = 0xff;
    };
    initialize();
    fixture.AdmitWeaponPins();
    struct DefaultWeaponOwner final : NativeGameTimingAccess
    {
        ClockResult SetTimeScale(float) noexcept override { return ClockResult::NotInitialized; }
        ClockResult SetMinuteDuration(std::uint32_t) noexcept override { return ClockResult::NotInitialized; }
        ClockResult Pause(bool) noexcept override { return ClockResult::NotInitialized; }
    } absent;
    std::uint32_t untouched = 0xdeadbeef;
    CWeaponInfo*  untouchedInfo = reinterpret_cast<CWeaponInfo*>(std::uintptr_t{0x5678});
    Check(absent.ReadPedWeaponInfo({}, 0, WEAPONSKILL_STD, untouchedInfo) == ClockResult::NotInitialized &&
              untouchedInfo == reinterpret_cast<CWeaponInfo*>(std::uintptr_t{0x5678}),
          "timing-only access cannot fabricate a weapon-info pointer");
    Check(absent.ReadPedWeaponType({}, 0, untouched) == ClockResult::NotInitialized && untouched == 0xdeadbeef &&
              absent.ReadPedWeaponState({}, 0, untouched) == ClockResult::NotInitialized && untouched == 0xdeadbeef &&
              absent.ReadPedWeaponAmmoInClip({}, 0, untouched) == ClockResult::NotInitialized && untouched == 0xdeadbeef &&
              absent.ReadPedWeaponAmmoTotal({}, 0, untouched) == ClockResult::NotInitialized && untouched == 0xdeadbeef &&
              absent.SetPedWeaponType({}, 0, 1) == ClockResult::NotInitialized && absent.SetPedWeaponState({}, 0, 1) == ClockResult::NotInitialized &&
              absent.SetPedWeaponAmmoInClip({}, 0, 1) == ClockResult::NotInitialized && absent.SetPedWeaponAmmoTotal({}, 0, 1) == ClockResult::NotInitialized,
          "timing-only access neither fabricates inline weapon state nor grants raw stores");
    {
        context.operation = 0;
        NativeClockService service;
        BeginOwned(service, fixture, false, true, true, &PedPhysicalScalarsOwnerHandler, &context);
        fixture.invokeTimer();
        Check(context.result == ClockResult::Ok, "weapon owner acquires selected generic-ped reference instead of a current-player substitute");
        const auto invoke = [&]
        {
            Check(service.Renew(runtime.generation.load()), "renew same registered-ped weapon callback");
            fixture.invokeTimer();
        };
        context.weaponOutput = 0xdeadbeef;
        Check(runtime.access.ReadPedWeaponType(context.token, 0, context.weaponOutput) != ClockResult::Ok && context.weaponOutput == 0xdeadbeef &&
                  runtime.access.SetPedWeaponAmmoTotal(context.token, 0, 0xffffffff) != ClockResult::Ok && !ClockPedWeaponOwnerTest::ammo.stores,
              "a retained token outside its callback grants no weapon output or mutation");
        ClockResult foreign = ClockResult::Ok;
        std::thread other([&] { foreign = runtime.access.SetPedWeaponState(context.token, 0, 2); });
        other.join();
        Check(foreign != ClockResult::Ok && !ClockPedWeaponOwnerTest::fields.stores, "foreign thread cannot borrow inline weapon authority");
        context.operation = 26;
        context.weaponSlot = 12;
        const auto selectedTypeOffset = W::ArrayOffset + 12 * W::Stride;
        store(selectedTypeOffset, WEAPONTYPE_PISTOL);
        for (const auto skill : {WEAPONSKILL_STD, WEAPONSKILL_POOR, WEAPONSKILL_PRO, WEAPONSKILL_SPECIAL, static_cast<eWeaponSkill>(255)})
        {
            context.weaponSkill = skill;
            context.weaponInfo = reinterpret_cast<CWeaponInfo*>(std::uintptr_t{0x5678});
            std::size_t expectedIndex = 0;
            Check(MtaGame::CWeaponInfoIndexPolicy::TryGetIndex(WEAPONTYPE_PISTOL, skill, expectedIndex),
                  "shared MTA selector provides actual current-type skill routing");
            invoke();
            Check(context.result == ClockResult::Ok && context.weaponInfo == runtime.weaponBank.GetSlot(expectedIndex),
                  "selected native type and supplied skill publish the stable actual MTA bank wrapper");
        }
        for (const auto invalidType : {std::uint32_t{WEAPONTYPE_ARMOUR}, std::uint32_t{WEAPONTYPE_INVALID}})
        {
            store(selectedTypeOffset, invalidType);
            context.weaponSkill = WEAPONSKILL_STD;
            context.weaponInfo = reinterpret_cast<CWeaponInfo*>(std::uintptr_t{0x5678});
            invoke();
            Check(context.result == ClockResult::Ok && context.weaponInfo == nullptr,
                  "only qualified invalid legal-enum mapping returns null through the actual shared selector");
        }
        for (const auto invalidRaw : {std::uint32_t{0x80000000}, std::uint32_t{0xffffffff}})
        {
            store(selectedTypeOffset, invalidRaw);
            context.weaponInfo = reinterpret_cast<CWeaponInfo*>(std::uintptr_t{0x5678});
            invoke();
            Check(context.result == ClockResult::InvalidArgument && context.weaponInfo == reinterpret_cast<CWeaponInfo*>(std::uintptr_t{0x5678}),
                  "out-of-language-range native type refuses without manufacturing an enum or publishing null");
        }
        store(selectedTypeOffset, WEAPONTYPE_ROCKETLAUNCHER);
        context.weaponSkill = WEAPONSKILL_PRO;
        invoke();
        Check(context.result == ClockResult::Ok && context.weaponInfo == nullptr,
              "classic nonstandard skill on a non-skill weapon has a qualified null bank mapping");
        store(selectedTypeOffset, WEAPONTYPE_PISTOL);
        context.weaponSkill = WEAPONSKILL_STD;
        context.weaponInfo = reinterpret_cast<CWeaponInfo*>(std::uintptr_t{0x5678});
        ClockPedWeaponOwnerTest::infoTypeAddress = reinterpret_cast<std::uintptr_t>(pool.objects) + selectedTypeOffset;
        ClockTestFaults::weaponRowAddress = fixture.image + NativeWeaponInfoProfile::BankRva + WEAPONTYPE_PISTOL * NativeWeaponInfoProfile::RowBytes;
        ClockTestFaults::weaponAfterRowRead = &ClockPedWeaponOwnerTest::ChangeInfoType;
        invoke();
        Check(context.result == ClockResult::OwnerConflict && context.weaponInfo == reinterpret_cast<CWeaponInfo*>(std::uintptr_t{0x5678}) &&
                  !runtime.stopping.load(),
              "type change during actual bank read preserves output instead of publishing a stale wrapper or turning failure into null");
        ClockTestFaults::weaponRowAddress = 0;
        ClockPedWeaponOwnerTest::infoTypeAddress = 0;
        initialize();
        for (std::uint32_t slot = 0; slot < W::Count; ++slot)
            for (unsigned field = 0; field != 4; ++field)
            {
                const auto at = W::ArrayOffset + slot * W::Stride + field * 4;
                context.weaponSlot = slot;
                context.operation = 18 + field * 2;
                context.weaponOutput = 0xdeadbeef;
                const auto original = raw(at);
                invoke();
                Check(context.result == ClockResult::Ok && context.weaponOutput == original && !runtime.playerInfo.IsBound(),
                      "current callback reads the exact selected row DWORD without weapon-info or player-data substitution");
                std::array<unsigned char, Ped::ObjectStride> before;
                std::memcpy(before.data(), pool.objects, before.size());
                context.operation = 19 + field * 2;
                context.weaponInput = 0xdead0000u | (field << 8) | slot;
                invoke();
                std::memcpy(before.data() + at, &context.weaponInput, sizeof(context.weaponInput));
                Check(context.result == ClockResult::Ok && raw(at) == context.weaponInput && std::memcmp(before.data(), pool.objects, before.size()) == 0,
                      "raw type/state/clip/total effect preserves every other weapon, timer, FX pointer and ped byte");
            }
        Check(ClockPedWeaponOwnerTest::fields.stores == W::Count * 2 && ClockPedWeaponOwnerTest::ammo.stores == W::Count * 2,
              "all thirteen explicit slots dispatch one conditional store for each complete raw DWORD");
        context.weaponInfo = reinterpret_cast<CWeaponInfo*>(std::uintptr_t{0x5678});
        Check(runtime.access.ReadPedWeaponInfo(context.token, 0, WEAPONSKILL_STD, context.weaponInfo) != ClockResult::Ok &&
                  context.weaponInfo == reinterpret_cast<CWeaponInfo*>(std::uintptr_t{0x5678}),
              "outside-callback invalid type cannot bypass ownership to return null");
        context.weaponSlot = 13;
        context.operation = 19;
        invoke();
        Check(context.result == ClockResult::InvalidArgument && !runtime.stopping.load(), "invalid SDK slot sentinel refuses before entering a write");
        context.weaponSlot = 0;
        const auto token = context.token;
        ++context.token.version;
        context.operation = 18;
        context.weaponOutput = 0xdeadbeef;
        invoke();
        Check(context.result != ClockResult::Ok && context.weaponOutput == 0xdeadbeef && !runtime.stopping.load(),
              "stale selected token preserves weapon read output");
        context.operation = 25;
        invoke();
        Check(context.result != ClockResult::Ok && !runtime.stopping.load(), "stale token cannot enter an ammo write or retire unrelated ownership");
        context.token = token;
        const auto fields = ClockPedWeaponOwnerTest::fields.stores, ammo = ClockPedWeaponOwnerTest::ammo.stores;
        const auto accepted = raw(W::ArrayOffset + W::TotalOffset);
        retire(service);
        Check(ClockPedWeaponOwnerTest::fields.stores == fields && ClockPedWeaponOwnerTest::ammo.stores == ammo &&
                  raw(W::ArrayOffset + W::TotalOffset) == accepted,
              "normal native reference retirement never replays or restores accepted weapon scalars");
    }
    // Both primitive families must stop after accepted effects when the final
    // registered-token acknowledgement is revoked. Address debt is independent
    // of those persistent scalar effects and cannot be erased by rollback.
    for (const auto operation : {19u, 25u})
    {
        initialize();
        context.operation = 0;
        NativeClockService service;
        BeginOwned(service, fixture, false, true, true, &PedPhysicalScalarsOwnerHandler, &context);
        fixture.invokeTimer();
        Check(context.result == ClockResult::Ok, "partial weapon effect owns a newly registered ped generation");
        auto& host = operation == 19 ? ClockPedWeaponOwnerTest::fields : ClockPedWeaponOwnerTest::ammo;
        host.cell = ClockPedReferenceOwnerTest::cell;
        host.loseToken = true;
        const auto stores = host.stores, unlinks = ClockPedReferenceOwnerTest::unregisters;
        const auto node = ClockPedReferenceOwnerTest::node;
        context.operation = operation;
        context.weaponSlot = 12;
        context.weaponInput = 0xffffffff;
        const auto at = W::ArrayOffset + 12 * W::Stride + (operation == 19 ? W::TypeOffset : W::TotalOffset);
        Check(service.Renew(runtime.generation.load()), "renew before accepted weapon scalar loses token acknowledgement");
        fixture.invokeTimer();
        Check(context.result != ClockResult::Ok && runtime.stopping.load() && host.stores == stores + 1 && raw(at) == 0xffffffff && !host.loseToken,
              "accepted weapon DWORD survives token loss while the same owner Stops");
        Check(runtime.pedReferences.IsRetained() && !service.End(), "still-native-visible weapon reference cell retains address debt");
        fixture.invokeTimer();
        Check(host.stores == stores + 1 && ClockPedReferenceOwnerTest::unregisters == unlinks && !service.End(),
              "stopping callbacks cannot replay weapon intent or free still-visible reference storage");
        ClockPedReferenceOwnerTest::Put(node + 8, std::uintptr_t{0});
        const auto head = ClockPedReferenceOwnerTest::Get<std::uintptr_t>(fixture.image + R::FreeHeadRva);
        ClockPedReferenceOwnerTest::Put(node, head);
        ClockPedReferenceOwnerTest::Put(fixture.image + R::FreeHeadRva, node);
        ClockPedReferenceOwnerTest::Put(reinterpret_cast<std::uintptr_t>(pool.objects) + R::EntityHeadOffset, std::uintptr_t{0});
        fixture.invokeTimer();
        retire(service);
        Check(host.stores == stores + 1 && raw(at) == 0xffffffff, "native address retirement completes without weapon rollback or replay");
        host.cell = 0;
    }
    for (const auto operation : {21u, 23u})
    {
        initialize();
        context.operation = 0;
        NativeClockService service;
        BeginOwned(service, fixture, false, true, true, &PedPhysicalScalarsOwnerHandler, &context);
        fixture.invokeTimer();
        Check(context.result == ClockResult::Ok, "unknown weapon acknowledgement owns another actual callback generation");
        auto&      host = operation == 21 ? ClockPedWeaponOwnerTest::fields : ClockPedWeaponOwnerTest::ammo;
        auto&      fail = operation == 21 ? ClockPedWeaponOwnerTest::failFieldsAfter : ClockPedWeaponOwnerTest::failAmmoAfter;
        const auto stores = host.stores;
        context.operation = operation;
        context.weaponSlot = 0;
        context.weaponInput = 0x80000000;
        fail = true;
        Check(service.Renew(runtime.generation.load()), "renew before actual weapon CAS loses final acknowledgement");
        fixture.invokeTimer();
        fail = false;
        const auto at = W::ArrayOffset + (operation == 21 ? W::StateOffset : W::ClipOffset);
        Check(context.result != ClockResult::Ok && runtime.stopping.load() && host.stores == stores + 1 && raw(at) == 0x80000000,
              "unknown post-store weapon effect Stops without guessing an accepted result or replaying");
        retire(service);
        Check(host.stores == stores + 1 && raw(at) == 0x80000000, "retirement cannot repair an unacknowledged persistent weapon effect");
    }
    std::puts("Production native ped weapon instance callback owner fixture completed");
}
