/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Raw current-slot observation through the actual registered ped callback
 ****************************************************************************/
{
    namespace C = NativePedCurrentWeaponSlotProfile;
    for (const auto& pin : C::Code)
        admit(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
    for (const auto& pin : NativePedMetadataProfile::Code)
        admit(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
    ClockPedCurrentWeaponOwnerTest::address = reinterpret_cast<std::uintptr_t>(pool.objects) + C::CurrentOffset;
    ClockPedCurrentWeaponOwnerTest::cell = 0;
    ClockPedCurrentWeaponOwnerTest::action = 0;
    struct DefaultCurrentWeaponOwner final : NativeGameTimingAccess
    {
        ClockResult SetTimeScale(float) noexcept override { return ClockResult::NotInitialized; }
        ClockResult SetMinuteDuration(std::uint32_t) noexcept override { return ClockResult::NotInitialized; }
        ClockResult Pause(bool) noexcept override { return ClockResult::NotInitialized; }
    } absent;
    std::uint32_t untouched = 0xdeadbeef;
    Check(absent.ReadPedCurrentWeaponSlot({}, untouched) == ClockResult::NotInitialized && untouched == 0xdeadbeef,
          "timing-only access refuses current-slot output without an actual ped callback");
    context.operation = 0;
    NativeClockService service;
    BeginOwned(service, fixture, false, true, true, &PedPhysicalScalarsOwnerHandler, &context);
    fixture.invokeTimer();
    Check(context.result == ClockResult::Ok, "current-slot reader acquires the selected generic ped through the real reference owner");
    const auto invoke = [&]
    {
        Check(service.Renew(runtime.generation.load()), "renew actual registered-ped current-slot callback");
        fixture.invokeTimer();
    };
    context.weaponOutput = 0xdeadbeef;
    Check(runtime.access.ReadPedCurrentWeaponSlot(context.token, context.weaponOutput) != ClockResult::Ok && context.weaponOutput == 0xdeadbeef,
          "a retained token outside its callback cannot publish current selection");
    ClockResult foreign = ClockResult::Ok;
    std::thread other([&] { foreign = runtime.access.ReadPedCurrentWeaponSlot(context.token, context.weaponOutput); });
    other.join();
    Check(foreign != ClockResult::Ok && context.weaponOutput == 0xdeadbeef, "foreign thread cannot borrow current-slot read authority");
    context.operation = 27;
    for (const auto value : {0u, 1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u, 9u, 10u, 11u, 12u, 13u, 14u, 15u, 127u, 128u, 255u})
    {
        pool.objects[C::CurrentOffset] = static_cast<std::uint8_t>(value);
        std::array<unsigned char, Ped::ObjectStride> before;
        std::memcpy(before.data(), pool.objects, before.size());
        context.weaponOutput = 0xdeadbeef;
        invoke();
        Check(context.result == ClockResult::Ok && context.weaponOutput == value && std::memcmp(before.data(), pool.objects, before.size()) == 0 &&
                  !runtime.playerInfo.IsBound(),
              "raw callback preserves unsigned current BYTE without enum fabrication, array indexing, player substitution or writes");
    }
    const auto token = context.token;
    ++context.token.version;
    context.weaponOutput = 0xdeadbeef;
    invoke();
    Check(context.result != ClockResult::Ok && context.weaponOutput == 0xdeadbeef && !runtime.stopping.load(),
          "stale selected token preserves current-slot output");
    context.token = token;
    pool.objects[C::CurrentOffset] = 0;
    ClockPedCurrentWeaponOwnerTest::action = 1;
    context.weaponOutput = 0xdeadbeef;
    invoke();
    Check(
        context.result == ClockResult::OwnerConflict && context.weaponOutput == 0xdeadbeef && pool.objects[C::CurrentOffset] == 12 && !runtime.stopping.load(),
        "concurrent native selection is retained and a mixed read cannot fabricate a current slot");
    const auto node = ClockPedReferenceOwnerTest::node;
    const auto unlinks = ClockPedReferenceOwnerTest::unregisters;
    ClockPedCurrentWeaponOwnerTest::cell = ClockPedReferenceOwnerTest::cell;
    ClockPedCurrentWeaponOwnerTest::action = 2;
    context.weaponOutput = 0xdeadbeef;
    invoke();
    Check(context.result != ClockResult::Ok && context.weaponOutput == 0xdeadbeef && !runtime.stopping.load() && runtime.pedReferences.IsRetained(),
          "token revocation after a raw observation preserves output without pretending a mutation or freeing native-visible address storage");
    Check(!service.End(), "explicit retirement retains a still-visible current-slot reference cell");
    fixture.invokeTimer();
    Check(ClockPedReferenceOwnerTest::unregisters == unlinks && !service.End(),
          "retirement cannot reclaim the cell while a genuine node still names its address");
    // Supply only the existing native ResolveReferences postcondition. The
    // observation never gains a slot setter or any restoration write.
    ClockPedReferenceOwnerTest::Put(node + 8, std::uintptr_t{0});
    const auto head = ClockPedReferenceOwnerTest::Get<std::uintptr_t>(fixture.image + R::FreeHeadRva);
    ClockPedReferenceOwnerTest::Put(node, head);
    ClockPedReferenceOwnerTest::Put(fixture.image + R::FreeHeadRva, node);
    ClockPedReferenceOwnerTest::Put(reinterpret_cast<std::uintptr_t>(pool.objects) + R::EntityHeadOffset, std::uintptr_t{0});
    fixture.invokeTimer();
    retire(service);
    Check(pool.objects[C::CurrentOffset] == 12, "reference retirement does not rewrite current weapon selection");
    ClockPedCurrentWeaponOwnerTest::address = ClockPedCurrentWeaponOwnerTest::cell = 0;
    ClockPedCurrentWeaponOwnerTest::action = 0;
    std::puts("Production native current weapon slot callback owner fixture completed");
}
