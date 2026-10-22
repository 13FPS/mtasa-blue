/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Masked bike/shot flag intents borrow the actual registered owner
 ****************************************************************************/
// The preceding fixtures have retired every native reference. Reuse their
// actual generic-ped storage/cell bank; these flags need no player-data route.
{
    namespace Bike = NativePedBikeFlagsProfile;
    namespace Shot = NativePedShotFlagsProfile;
    for (const auto& pin : Bike::Code)
        admit(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
    for (const auto& pin : Shot::Code)
        admit(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
    const auto genericVtable = fixture.image + R::PedVtableRva;
    std::memcpy(pool.objects, &genericVtable, sizeof(genericVtable));
    const std::uintptr_t missingPlayerData = 0;
    std::memcpy(pool.objects + NativePedOxygenProfile::PlayerDataOffset, &missingPlayerData, sizeof(missingPlayerData));
    ClockPedFlagsOwnerTest::bike = {};
    ClockPedFlagsOwnerTest::shot = {};
    // This fixture precedes the intelligence fixture's helper declaration.
    // Use its own older owner to exercise inherited flag refusals directly.
    struct DefaultPedFlagsOwner final : NativeGameTimingAccess
    {
        ClockResult SetTimeScale(float) noexcept override { return ClockResult::NotInitialized; }
        ClockResult SetMinuteDuration(std::uint32_t) noexcept override { return ClockResult::NotInitialized; }
        ClockResult Pause(bool) noexcept override { return ClockResult::NotInitialized; }
    } absentFlags;
    std::uint32_t absentBike = 0x12345678;
    bool          absentShot = true;
    Check(absentFlags.ReadPedCantBeKnockedOffBike(context.token, absentBike) == ClockResult::NotInitialized && absentBike == 0x12345678 &&
              absentFlags.ReadPedCanBeShotInVehicle(context.token, absentShot) == ClockResult::NotInitialized && absentShot &&
              absentFlags.SetPedCantBeKnockedOffBike(context.token, 3) == ClockResult::NotInitialized &&
              absentFlags.SetPedCanBeShotInVehicle(context.token, true) == ClockResult::NotInitialized,
          "older/default owners refuse masked flags and preserve query output without fabricated absence");
    {
        context.operation = 0;
        NativeClockService service;
        BeginOwned(service, fixture, false, true, true, &PedPhysicalScalarsOwnerHandler, &context);
        fixture.invokeTimer();
        Check(context.result == ClockResult::Ok, "masked flag owner acquires a real registered generic-ped lifetime");
        const auto invoke = [&]
        {
            Check(service.Renew(runtime.generation.load()), "renew actual registered-ped flag callback");
            fixture.invokeTimer();
        };
        context.flagOutput = 0x12345678;
        context.duckingOutput = true;
        Check(runtime.access.ReadPedCantBeKnockedOffBike(context.token, context.flagOutput) != ClockResult::Ok && context.flagOutput == 0x12345678 &&
                  runtime.access.ReadPedCanBeShotInVehicle(context.token, context.duckingOutput) != ClockResult::Ok && context.duckingOutput &&
                  runtime.access.SetPedCantBeKnockedOffBike(context.token, 1) != ClockResult::Ok &&
                  runtime.access.SetPedCanBeShotInVehicle(context.token, true) != ClockResult::Ok && !ClockPedFlagsOwnerTest::bike.stores &&
                  !ClockPedFlagsOwnerTest::shot.stores,
              "retained token between callbacks supplies neither flag output nor native masked mutation");
        ClockResult foreignBike = ClockResult::Ok, foreignShot = ClockResult::Ok;
        std::thread other(
            [&]
            {
                foreignBike = runtime.access.SetPedCantBeKnockedOffBike(context.token, 1);
                foreignShot = runtime.access.SetPedCanBeShotInVehicle(context.token, true);
            });
        other.join();
        Check(foreignBike != ClockResult::Ok && foreignShot != ClockResult::Ok && !ClockPedFlagsOwnerTest::bike.stores && !ClockPedFlagsOwnerTest::shot.stores,
              "foreign thread cannot inherit the current generic ped's flag mutation authority");
        const std::uint32_t bikeNeighbors = 0xa5a5a5a5u & ~Bike::KnockoffMask;
        store(Bike::FlagsOffset, bikeNeighbors);
        context.operation = 8;
        invoke();
        Check(context.result == ClockResult::Ok && context.flagOutput == 0 && !runtime.playerInfo.IsBound(),
              "actual bike getter reads selected generic ped's two-bit storage without player-data substitution");
        context.operation = 9;
        context.flagInput = 0xffffffffu;
        invoke();
        Check(context.result == ClockResult::Ok && raw(Bike::FlagsOffset) == (bikeNeighbors | Bike::KnockoffMask) && ClockPedFlagsOwnerTest::bike.stores == 1,
              "actual owner preserves all neighbor bits while full DWORD bike intent stores low-two-bit mode3");
        context.operation = 8;
        invoke();
        Check(context.result == ClockResult::Ok && context.flagOutput == 3,
              "actual bike getter preserves raw mode3 rather than imposing a gameplay enum clamp");
        const auto beforeNeighbor = raw(Bike::FlagsOffset);
        ClockPedFlagsOwnerTest::bike.neighborOnce = true;
        context.operation = 9;
        context.flagInput = 6;
        const auto beforeStores = ClockPedFlagsOwnerTest::bike.stores;
        invoke();
        Check(context.result == ClockResult::Ok && ClockPedFlagsOwnerTest::bike.stores == beforeStores + 2 && !ClockPedFlagsOwnerTest::bike.neighborOnce &&
                  (raw(Bike::FlagsOffset) & Bike::KnockoffMask) == (std::uint32_t{2} << Bike::Shift) &&
                  (raw(Bike::FlagsOffset) & ~Bike::KnockoffMask) == ((beforeNeighbor ^ std::uint32_t{1}) & ~Bike::KnockoffMask),
              "only an unaccepted neighbor conflict receives bounded native retry; the engine neighbor update is retained");
        store(Shot::FlagsOffset, 0xffffffffu);
        context.operation = 10;
        invoke();
        Check(context.result == ClockResult::Ok && context.duckingOutput, "actual named shot getter returns true from its independently qualified bit");
        context.operation = 11;
        context.flagBoolean = false;
        invoke();
        Check(context.result == ClockResult::Ok && raw(Shot::FlagsOffset) == (0xffffffffu & ~Shot::CanBeShotMask) && ClockPedFlagsOwnerTest::shot.stores == 1,
              "actual shot setter clears only its named bit, preserving every unowned neighbor including unqualified properties");
        context.operation = 10;
        invoke();
        Check(context.result == ClockResult::Ok && !context.duckingOutput, "actual named shot getter preserves successful false separately from refusal");
        const auto valid = context.token;
        ++context.token.version;
        context.operation = 8;
        context.flagOutput = 0x12345678;
        invoke();
        Check(context.result != ClockResult::Ok && context.flagOutput == 0x12345678 && !runtime.stopping.load(), "stale bike token preserves raw query output");
        context.operation = 10;
        context.duckingOutput = true;
        invoke();
        Check(context.result != ClockResult::Ok && context.duckingOutput && !runtime.stopping.load(), "stale shot token cannot fabricate successful false");
        context.token = valid;
        context.operation = 11;
        context.flagBoolean = true;
        invoke();
        Check(context.result == ClockResult::Ok && raw(Shot::FlagsOffset) == 0xffffffffu, "same owner remains usable after clean token refusals");
        const auto bikes = ClockPedFlagsOwnerTest::bike.stores, shots = ClockPedFlagsOwnerTest::shot.stores;
        retire(service);
        Check(ClockPedFlagsOwnerTest::bike.stores == bikes && ClockPedFlagsOwnerTest::shot.stores == shots,
              "normal reference retirement does not replay accepted masked flag intents");
    }
    for (unsigned which = 0; which != 2; ++which)
    {
        context.operation = 0;
        NativeClockService service;
        BeginOwned(service, fixture, false, true, true, &PedPhysicalScalarsOwnerHandler, &context);
        fixture.invokeTimer();
        Check(context.result == ClockResult::Ok, "each entered flag-failure generation independently acquires its registered token");
        auto&               host = which ? ClockPedFlagsOwnerTest::shot : ClockPedFlagsOwnerTest::bike;
        const auto          offset = which ? Shot::FlagsOffset : Bike::FlagsOffset;
        const auto          mask = which ? Shot::CanBeShotMask : Bike::KnockoffMask;
        const std::uint32_t before = 0x66666666u & ~mask;
        const auto          expected = before | (which ? mask : (std::uint32_t{1} << Bike::Shift));
        store(offset, before);
        host.cell = ClockPedReferenceOwnerTest::cell;
        host.loseToken = true;
        const auto stores = host.stores, unlinks = ClockPedReferenceOwnerTest::unregisters;
        const auto node = ClockPedReferenceOwnerTest::node;
        context.operation = which ? 11 : 9;
        context.flagInput = 1;
        context.flagBoolean = true;
        Check(service.Renew(runtime.generation.load()), "renew before accepted flag mutation loses token acknowledgement");
        fixture.invokeTimer();
        Check(context.result != ClockResult::Ok && runtime.stopping.load() && host.stores == stores + 1 && raw(offset) == expected && !host.loseToken,
              "accepted masked flag write followed by token loss stops same owner and retains target plus all neighbors");
        Check(runtime.pedReferences.IsRetained() && !service.End(), "native node still naming the token cell retains flag-owner storage debt");
        fixture.invokeTimer();
        Check(host.stores == stores + 1 && ClockPedReferenceOwnerTest::unregisters == unlinks && !service.End(),
              "stopping callbacks cannot replay flag mutation or reclaim a cell whose address remains native-visible");
        // Supply only the established native reference address-retirement
        // postcondition. Persistent flags are neither restored nor repeated.
        ClockPedReferenceOwnerTest::Put(node + 8, std::uintptr_t{0});
        const auto head = ClockPedReferenceOwnerTest::Get<std::uintptr_t>(fixture.image + R::FreeHeadRva);
        ClockPedReferenceOwnerTest::Put(node, head);
        ClockPedReferenceOwnerTest::Put(fixture.image + R::FreeHeadRva, node);
        ClockPedReferenceOwnerTest::Put(reinterpret_cast<std::uintptr_t>(pool.objects) + R::EntityHeadOffset, std::uintptr_t{0});
        fixture.invokeTimer();
        retire(service);
        Check(host.stores == stores + 1 && raw(offset) == expected, "resolved native storage debt retires without flag rollback or action replay");
        host.cell = 0;
    }
    std::puts("Production native bike flags callback owner fixture completed");
    std::puts("Production native shot flags callback owner fixture completed");
}
