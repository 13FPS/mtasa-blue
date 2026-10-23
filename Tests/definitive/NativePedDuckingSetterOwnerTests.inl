/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual ducking setter through the sole registered-ped owner
 ****************************************************************************/
{
    namespace Duck = NativePedDuckingProfile;
    for (const auto& pin : NativePedMetadataProfile::Code)
        admit(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
    for (const auto& pin : Duck::Code)
        admit(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
    struct DuckingCallback
    {
        std::function<void(NativeGameTimingAccess&)> action;
        static void Invoke(NativeGameTimingAccess& access, void* opaque) { static_cast<DuckingCallback*>(opaque)->action(access); }
    } duckCall;
    struct DefaultDuckingSetterOwner final : NativeGameTimingAccess
    {
        ClockResult SetTimeScale(float) noexcept override { return ClockResult::NotInitialized; }
        ClockResult SetMinuteDuration(std::uint32_t) noexcept override { return ClockResult::NotInitialized; }
        ClockResult Pause(bool) noexcept override { return ClockResult::NotInitialized; }
    } duckAbsent;
    Check(duckAbsent.SetPedDucking({}, true) == ClockResult::NotInitialized, "older owners cannot fabricate accepted ducking intent");
    ped_tasks_fixture::Game         duckGame;
    ped_tasks_fixture::TaskScenario duckScenario;
    const auto                      duckAddress = reinterpret_cast<std::uintptr_t>(pool.objects) + Duck::FlagsOffset;
    constexpr std::uint32_t         duckOriginal = 0xa1a55aa5u;
    static_assert((duckOriginal & Duck::DuckingMask) == 0);
    const auto resetDuck = [&]
    {
        ClockPedDuckingSetterOwnerTest::stores = 0;
        ClockPedDuckingSetterOwnerTest::cell = 0;
        ClockPedDuckingSetterOwnerTest::callerToken = nullptr;
        ClockPedDuckingSetterOwnerTest::neighborRace = ClockPedDuckingSetterOwnerTest::failAfter = ClockPedDuckingSetterOwnerTest::loseToken = false;
        std::memcpy(reinterpret_cast<void*>(duckAddress), &duckOriginal, sizeof(duckOriginal));
    };
    resetDuck();
    {
        NativeClockService      duckService;
        NativePedReferenceToken duckToken;
        ClockResult             duckResult = ClockResult::NotInitialized;
        duckCall.action = [&](NativeGameTimingAccess& access) { duckResult = access.AcquirePedReference(1, duckToken); };
        BeginOwned(duckService, fixture, false, true, true, &DuckingCallback::Invoke, &duckCall);
        fixture.invokeTimer();
        Check(duckResult == ClockResult::Ok && duckToken.version && runtime.pedReferences.IsRetained(),
              "duck setter uses sole actual registered-ped controller");
        Check(runtime.access.SetPedDucking(duckToken, true) != ClockResult::Ok && !ClockPedDuckingSetterOwnerTest::stores,
              "retained token gives no duck write between callbacks");
        std::thread duckForeign([&] { duckResult = runtime.access.SetPedDucking(duckToken, true); });
        duckForeign.join();
        Check(duckResult != ClockResult::Ok && !ClockPedDuckingSetterOwnerTest::stores && !runtime.stopping.load(),
              "foreign duck intent cannot Stop another Runtime");
        const auto invokeDuck = [&]
        {
            Check(duckService.Renew(runtime.generation.load()), "renew actual duck setter callback");
            fixture.invokeTimer();
        };
        duckCall.action = [&](NativeGameTimingAccess& access)
        {
            auto staleDuck = duckToken;
            ++staleDuck.version;
            Check(access.SetPedDucking(staleDuck, true) != ClockResult::Ok && !ClockPedDuckingSetterOwnerTest::stores && !runtime.stopping.load(),
                  "stale initial duck token refuses before CAS without stopping genuine owner");
        };
        invokeDuck();
        const auto&                duckPin = Duck::Code[0];
        std::vector<unsigned char> changedDuckCode(duckPin.bytes, duckPin.bytes + duckPin.size);
        changedDuckCode[0] ^= 1;
        fixture.Patch(duckPin.rva, changedDuckCode.data(), changedDuckCode.size());
        duckCall.action = [&](NativeGameTimingAccess& access) { duckResult = access.SetPedDucking(duckToken, true); };
        invokeDuck();
        Check(duckResult != ClockResult::Ok && !ClockPedDuckingSetterOwnerTest::stores && !runtime.stopping.load(),
              "unknown native duck profile refuses before entry and keeps owner usable");
        fixture.Patch(duckPin.rva, duckPin.bytes, duckPin.size);
        for (const bool requestedDuck : {false, true, true, false})
        {
            const auto attemptsBefore = ClockPedDuckingSetterOwnerTest::stores;
            duckCall.action = [&](NativeGameTimingAccess& access)
            {
                ped_tasks_fixture::PedTasksFixture duckPed(duckGame, access, access, access, access, access, access, access, access, access, access, access,
                                                           access, access, access, access, access, access, access, access, access, duckToken, 0x12345678,
                                                           duckScenario);
                CPed&                              duckSdk = duckPed;
                try
                {
                    duckSdk.SetDucking(requestedDuck);
                    Check(duckSdk.IsDucking() == requestedDuck, "unchanged actual duck reader observes accepted bit");
                    duckResult = ClockResult::Ok;
                }
                catch (const std::exception& error)
                {
                    Check(false, error.what());
                }
            };
            invokeDuck();
            std::uint32_t observedDuck = 0;
            std::memcpy(&observedDuck, reinterpret_cast<void*>(duckAddress), sizeof(observedDuck));
            Check(duckResult == ClockResult::Ok && observedDuck == (duckOriginal | (requestedDuck ? Duck::DuckingMask : 0u)) &&
                      ClockPedDuckingSetterOwnerTest::stores == attemptsBefore + 1 && duckScenario.eventCount == 0,
                  "actual inherited SDK SetDucking uses one CAS including same-value intent, preserves neighbors and never invokes task interfaces");
        }
        const auto selectedDuck = duckToken;
        ClockPedDuckingSetterOwnerTest::callerToken = &duckToken;
        duckCall.action = [&](NativeGameTimingAccess& access) { duckResult = access.SetPedDucking(duckToken, true); };
        invokeDuck();
        Check(duckResult == ClockResult::Ok && duckToken.version == selectedDuck.version + 1 && !runtime.stopping.load(),
              "Runtime snapshots caller token before final boundary and acknowledges only that original relation");
        duckToken = selectedDuck;
        retire(duckService);
    }
    for (const unsigned duckFailure : {1u, 2u, 3u})
    {
        resetDuck();
        NativeClockService      duckService;
        NativePedReferenceToken duckToken;
        ClockResult             duckResult = ClockResult::NotInitialized;
        duckCall.action = [&](NativeGameTimingAccess& access) { duckResult = access.AcquirePedReference(1, duckToken); };
        BeginOwned(duckService, fixture, false, true, true, &DuckingCallback::Invoke, &duckCall);
        fixture.invokeTimer();
        Check(duckResult == ClockResult::Ok, "fresh duck fault owner acquires genuine registered relation");
        const auto duckNode = ClockPedReferenceOwnerTest::node;
        const auto duckUnregisters = ClockPedReferenceOwnerTest::unregisters;
        ClockPedDuckingSetterOwnerTest::cell = ClockPedReferenceOwnerTest::cell;
        ClockPedDuckingSetterOwnerTest::neighborRace = duckFailure == 1;
        ClockPedDuckingSetterOwnerTest::failAfter = duckFailure == 2;
        ClockPedDuckingSetterOwnerTest::loseToken = duckFailure == 3;
        duckCall.action = [&](NativeGameTimingAccess& access) { duckResult = access.SetPedDucking(duckToken, true); };
        Check(duckService.Renew(runtime.generation.load()), "renew before entered duck conflict or acknowledgement loss");
        fixture.invokeTimer();
        std::uint32_t duckAfterFailure = 0;
        std::memcpy(&duckAfterFailure, reinterpret_cast<void*>(duckAddress), sizeof(duckAfterFailure));
        Check(duckResult != ClockResult::Ok && runtime.stopping.load() && ClockPedDuckingSetterOwnerTest::stores == 1 &&
                  duckAfterFailure == (duckFailure == 1 ? (duckOriginal ^ 2u) : (duckOriginal | Duck::DuckingMask)),
              "single competing word or persistent duck acknowledgement loss Stops without retry, restore or neighbor overwrite");
        if (duckFailure == 3)
        {
            Check(!duckService.End() && runtime.pedReferences.IsRetained(), "zero weak word cannot free a cell still named by native bank");
            fixture.invokeTimer();
            Check(
                ClockPedReferenceOwnerTest::unregisters == duckUnregisters && runtime.pedReferences.IsRetained() && ClockPedDuckingSetterOwnerTest::stores == 1,
                "stopping callbacks retain duck cell debt without replay or unsafe unregister");
            // Supply only the already-established native address-retirement
            // postcondition; no production force-release or debt forgiveness.
            ClockPedReferenceOwnerTest::Put(duckNode + 8, std::uintptr_t{0});
            const auto freeDuckNode = ClockPedReferenceOwnerTest::Get<std::uintptr_t>(fixture.image + R::FreeHeadRva);
            ClockPedReferenceOwnerTest::Put(duckNode, freeDuckNode);
            ClockPedReferenceOwnerTest::Put(fixture.image + R::FreeHeadRva, duckNode);
            ClockPedReferenceOwnerTest::Put(reinterpret_cast<std::uintptr_t>(pool.objects) + R::EntityHeadOffset, std::uintptr_t{0});
            fixture.invokeTimer();
        }
        retire(duckService);
        Check(ClockPedDuckingSetterOwnerTest::stores == 1, "duck retirement never repeats an entered CAS");
    }
    ClockPedDuckingSetterOwnerTest::cell = 0;
    ClockPedDuckingSetterOwnerTest::callerToken = nullptr;
    ClockPedDuckingSetterOwnerTest::neighborRace = ClockPedDuckingSetterOwnerTest::failAfter = ClockPedDuckingSetterOwnerTest::loseToken = false;
    std::puts("Production native registered ducking setter callback owner fixture completed");
    std::puts("Production actual ped ducking setter SDK callback fixture completed");
}
