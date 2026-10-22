/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Named bleeding/stay flags share the actual registered ped owner
 ****************************************************************************/
// Reuse the established generic-ped cell bank and masked-CAS test boundary;
// current-player data, blood effects and task guesses grant no authority here.
{
    namespace Bleed = NativePedBleedingProfile;
    namespace Stay = NativePedStayProfile;
    static_assert(Bleed::FlagsOffset == Stay::FlagsOffset && (Bleed::BleedingMask & Stay::StayMask) == 0);
    for (const auto& pin : Bleed::Code)
        admit(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
    for (const auto& pin : Stay::Code)
        admit(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
    ClockPedBehaviorOwnerTest::bleeding = {};
    ClockPedBehaviorOwnerTest::stay = {};
    // This local owner is available at this include point. Later SDK fixture
    // declarations must not become hidden prerequisites for default refusal.
    struct DefaultPedBehaviorOwner final : NativeGameTimingAccess
    {
        ClockResult SetTimeScale(float) noexcept override { return ClockResult::NotInitialized; }
        ClockResult SetMinuteDuration(std::uint32_t) noexcept override { return ClockResult::NotInitialized; }
        ClockResult Pause(bool) noexcept override { return ClockResult::NotInitialized; }
    } absentBehavior;
    bool sentinel = true;
    Check(absentBehavior.ReadPedBleeding(context.token, sentinel) == ClockResult::NotInitialized && sentinel &&
              absentBehavior.ReadPedStayInSamePlace(context.token, sentinel) == ClockResult::NotInitialized && sentinel &&
              absentBehavior.SetPedBleeding(context.token, true) == ClockResult::NotInitialized &&
              absentBehavior.SetPedStayInSamePlace(context.token, true) == ClockResult::NotInitialized,
          "default timing owners refuse named behavior properties without fabricating successful false or effects");
    const auto bikeBefore = raw(NativePedBikeFlagsProfile::FlagsOffset), shotBefore = raw(NativePedShotFlagsProfile::FlagsOffset);
    {
        context.operation = 0;
        NativeClockService service;
        BeginOwned(service, fixture, false, true, true, &PedPhysicalScalarsOwnerHandler, &context);
        fixture.invokeTimer();
        Check(context.result == ClockResult::Ok, "behavior owner acquires the same real registered generic ped lifetime");
        const auto invoke = [&]
        {
            Check(service.Renew(runtime.generation.load()), "renew admitted selected-ped behavior callback");
            fixture.invokeTimer();
        };
        context.duckingOutput = true;
        Check(runtime.access.ReadPedBleeding(context.token, context.duckingOutput) != ClockResult::Ok && context.duckingOutput &&
                  runtime.access.ReadPedStayInSamePlace(context.token, context.duckingOutput) != ClockResult::Ok && context.duckingOutput &&
                  runtime.access.SetPedBleeding(context.token, true) != ClockResult::Ok &&
                  runtime.access.SetPedStayInSamePlace(context.token, true) != ClockResult::Ok && !ClockPedBehaviorOwnerTest::bleeding.stores &&
                  !ClockPedBehaviorOwnerTest::stay.stores,
              "retained behavior token between callbacks grants no output or mutation scope");
        ClockResult foreignBleeding = ClockResult::Ok, foreignStay = ClockResult::Ok;
        std::thread other(
            [&]
            {
                foreignBleeding = runtime.access.SetPedBleeding(context.token, true);
                foreignStay = runtime.access.SetPedStayInSamePlace(context.token, true);
            });
        other.join();
        Check(foreignBleeding != ClockResult::Ok && foreignStay != ClockResult::Ok && !ClockPedBehaviorOwnerTest::bleeding.stores &&
                  !ClockPedBehaviorOwnerTest::stay.stores,
              "foreign thread cannot borrow native bleeding/stay callback authority");
        store(Bleed::FlagsOffset, 0xffffffffu);
        context.operation = 12;
        invoke();
        Check(context.result == ClockResult::Ok && context.duckingOutput && !runtime.playerInfo.IsBound(),
              "actual IsBleeding query reads its named bit on the selected generic ped without player-data substitution");
        context.operation = 13;
        context.flagBoolean = false;
        invoke();
        Check(context.result == ClockResult::Ok && raw(Bleed::FlagsOffset) == (0xffffffffu & ~Bleed::BleedingMask) &&
                  ClockPedBehaviorOwnerTest::bleeding.stores == 1,
              "actual bleeding setter clears only its owned bit and preserves stay plus all unowned neighbors");
        context.operation = 14;
        invoke();
        Check(context.result == ClockResult::Ok && context.duckingOutput, "actual stay query remains true when independent bleeding intent clears");
        context.operation = 15;
        context.flagBoolean = false;
        invoke();
        const auto bothClear = 0xffffffffu & ~(Bleed::BleedingMask | Stay::StayMask);
        Check(context.result == ClockResult::Ok && raw(Stay::FlagsOffset) == bothClear && ClockPedBehaviorOwnerTest::stay.stores == 1,
              "actual stay setter clears only named stay storage without changing bleeding or Kinda/unowned bits");
        context.operation = 12;
        invoke();
        Check(context.result == ClockResult::Ok && !context.duckingOutput, "successful false bleeding is distinct from read refusal");
        context.operation = 14;
        invoke();
        Check(context.result == ClockResult::Ok && !context.duckingOutput, "successful false stay is a current fact rather than a synthetic default");
        ClockPedBehaviorOwnerTest::bleeding.neighborOnce = true;
        const auto bleedStores = ClockPedBehaviorOwnerTest::bleeding.stores;
        context.operation = 13;
        context.flagBoolean = true;
        invoke();
        const auto bleedSet = (bothClear ^ std::uint32_t{1}) | Bleed::BleedingMask;
        Check(context.result == ClockResult::Ok && raw(Bleed::FlagsOffset) == bleedSet && ClockPedBehaviorOwnerTest::bleeding.stores == bleedStores + 2 &&
                  !ClockPedBehaviorOwnerTest::bleeding.neighborOnce,
              "only an unaccepted neighbor conflict receives bounded CAS retry and its native neighbor update is preserved");
        const auto valid = context.token;
        ++context.token.version;
        context.operation = 12;
        context.duckingOutput = true;
        invoke();
        Check(context.result != ClockResult::Ok && context.duckingOutput && !runtime.stopping.load(), "stale bleeding token cannot publish fabricated false");
        context.operation = 14;
        invoke();
        Check(context.result != ClockResult::Ok && context.duckingOutput && !runtime.stopping.load(),
              "stale stay token preserves output and unrelated owner usability");
        context.token = valid;
        context.operation = 15;
        context.flagBoolean = true;
        invoke();
        Check(context.result == ClockResult::Ok && raw(Stay::FlagsOffset) == (bleedSet | Stay::StayMask),
              "same admitted owner can deliberately set stay after clean token refusals while retaining accepted bleeding/neighbors");
        const auto bleed = ClockPedBehaviorOwnerTest::bleeding.stores, stay = ClockPedBehaviorOwnerTest::stay.stores;
        retire(service);
        Check(ClockPedBehaviorOwnerTest::bleeding.stores == bleed && ClockPedBehaviorOwnerTest::stay.stores == stay,
              "normal reference retirement does not replay named behavior flags");
    }
    for (unsigned which = 0; which != 2; ++which)
    {
        context.operation = 0;
        NativeClockService service;
        BeginOwned(service, fixture, false, true, true, &PedPhysicalScalarsOwnerHandler, &context);
        fixture.invokeTimer();
        Check(context.result == ClockResult::Ok, "each behavior-failure generation independently acquires its actual token");
        auto&      host = which ? ClockPedBehaviorOwnerTest::stay : ClockPedBehaviorOwnerTest::bleeding;
        const auto mask = which ? Stay::StayMask : Bleed::BleedingMask;
        const auto before = 0x66666666u & ~mask, expected = before | mask;
        store(Bleed::FlagsOffset, before);
        host.cell = ClockPedReferenceOwnerTest::cell;
        host.loseToken = true;
        const auto stores = host.stores, unlinks = ClockPedReferenceOwnerTest::unregisters;
        const auto node = ClockPedReferenceOwnerTest::node;
        context.operation = which ? 15 : 13;
        context.flagBoolean = true;
        Check(service.Renew(runtime.generation.load()), "renew before accepted behavior intent loses registered-token acknowledgement");
        fixture.invokeTimer();
        Check(
            context.result != ClockResult::Ok && runtime.stopping.load() && host.stores == stores + 1 && raw(Bleed::FlagsOffset) == expected && !host.loseToken,
            "accepted named behavior flag followed by token loss stops same owner and retains target plus all unowned bits");
        Check(runtime.pedReferences.IsRetained() && !service.End(), "native node still naming a behavior cell retains address-retirement debt");
        fixture.invokeTimer();
        Check(host.stores == stores + 1 && ClockPedReferenceOwnerTest::unregisters == unlinks && !service.End(),
              "stopping callbacks neither replay behavior CAS nor reclaim still-native-visible cell storage");
        // Supply only the established ResolveReferences address-retirement
        // postcondition, retaining every accepted behavior/neighbor bit.
        ClockPedReferenceOwnerTest::Put(node + 8, std::uintptr_t{0});
        const auto head = ClockPedReferenceOwnerTest::Get<std::uintptr_t>(fixture.image + R::FreeHeadRva);
        ClockPedReferenceOwnerTest::Put(node, head);
        ClockPedReferenceOwnerTest::Put(fixture.image + R::FreeHeadRva, node);
        ClockPedReferenceOwnerTest::Put(reinterpret_cast<std::uintptr_t>(pool.objects) + R::EntityHeadOffset, std::uintptr_t{0});
        fixture.invokeTimer();
        retire(service);
        Check(host.stores == stores + 1 && raw(Bleed::FlagsOffset) == expected, "resolved native debt retires without behavior rollback/replay");
        host.cell = 0;
    }
    Check(raw(NativePedBikeFlagsProfile::FlagsOffset) == bikeBefore && raw(NativePedShotFlagsProfile::FlagsOffset) == shotBefore,
          "named shared-word behavior changes preserve the separately owned bike and shot words");
    std::puts("Production native bleeding callback owner fixture completed");
    std::puts("Production native stay callback owner fixture completed");
}
