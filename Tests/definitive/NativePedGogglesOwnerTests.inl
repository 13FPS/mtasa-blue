/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual goggles presence on the sole registered-ped callback owner
 ****************************************************************************/
{
    namespace Goggles = NativePedGogglesProfile;
    for (const auto& pin : NativePedMetadataProfile::Code)
        admit(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
    for (const auto& pin : Goggles::Code)
        admit(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
    struct GogglesCallback
    {
        std::function<void(NativeGameTimingAccess&)> action;
        static void Invoke(NativeGameTimingAccess& access, void* opaque) { static_cast<GogglesCallback*>(opaque)->action(access); }
    } gogglesCall;
    struct DefaultGogglesOwner final : NativeGameTimingAccess
    {
        ClockResult SetTimeScale(float) noexcept override { return ClockResult::NotInitialized; }
        ClockResult SetMinuteDuration(std::uint32_t) noexcept override { return ClockResult::NotInitialized; }
        ClockResult Pause(bool) noexcept override { return ClockResult::NotInitialized; }
    } gogglesAbsent;
    for (bool originalGogglesOutput : {false, true})
    {
        bool result = originalGogglesOutput;
        Check(gogglesAbsent.ReadPedWearingGoggles({}, result) == ClockResult::NotInitialized && result == originalGogglesOutput,
              "older owner refuses goggles observation without fabricating absence or changing either bool");
    }
    ped_goggles_fixture::Game         gogglesGame;
    ped_goggles_fixture::TaskScenario gogglesScenario;
    const auto                        gogglesAddress = reinterpret_cast<std::uintptr_t>(pool.objects) + Goggles::AssociationOffset;
    const std::uintptr_t              adjacentGogglesState = (std::numeric_limits<std::uintptr_t>::max)();
    std::memcpy(reinterpret_cast<void*>(gogglesAddress + Goggles::PointerBytes), &adjacentGogglesState, sizeof(adjacentGogglesState));
    ClockPedGogglesOwnerTest::watched = gogglesAddress;
    const auto resetGogglesRead = [&]
    {
        ClockPedGogglesOwnerTest::reads = ClockPedGogglesOwnerTest::changeAfter = 0;
        ClockPedGogglesOwnerTest::failNext = ClockPedGogglesOwnerTest::loseToken = false;
        ClockPedGogglesOwnerTest::cell = ClockPedGogglesOwnerTest::replacement = 0;
        ClockPedGogglesOwnerTest::callerToken = nullptr;
    };
    const auto makeGogglesPed = [&](const NativePedReferenceToken& selectedGogglesReference)
    {
        auto& gogglesOwnerAccess = runtime.access;
        return std::make_unique<ped_goggles_fixture::PedGogglesFixture>(
            gogglesGame, gogglesOwnerAccess, gogglesOwnerAccess, gogglesOwnerAccess, gogglesOwnerAccess, gogglesOwnerAccess, gogglesOwnerAccess,
            gogglesOwnerAccess, gogglesOwnerAccess, gogglesOwnerAccess, gogglesOwnerAccess, gogglesOwnerAccess, gogglesOwnerAccess, gogglesOwnerAccess,
            gogglesOwnerAccess, gogglesOwnerAccess, gogglesOwnerAccess, gogglesOwnerAccess, gogglesOwnerAccess, gogglesOwnerAccess, gogglesOwnerAccess,
            gogglesOwnerAccess, selectedGogglesReference, 0x12345678, gogglesScenario);
    };
    std::unique_ptr<ped_goggles_fixture::PedGogglesFixture> oldGogglesSdk;
    bool                                                    gogglesOutput = true;
    ClockResult                                             gogglesResult = ClockResult::NotInitialized;
    resetGogglesRead();
    {
        NativeClockService      gogglesService;
        NativePedReferenceToken gogglesToken;
        gogglesCall.action = [&](NativeGameTimingAccess& access) { gogglesResult = access.AcquirePedReference(1, gogglesToken); };
        BeginOwned(gogglesService, fixture, false, true, true, &GogglesCallback::Invoke, &gogglesCall);
        fixture.invokeTimer();
        Check(gogglesResult == ClockResult::Ok && gogglesToken.version && runtime.pedReferences.IsRetained(),
              "goggles uses sole real registered-ped controller");
        oldGogglesSdk = makeGogglesPed(gogglesToken);
        Check(!ClockPedGogglesOwnerTest::reads && gogglesScenario.eventCount == 0,
              "SDK construction copies borrowed identity without native query or task availability lookup");
        Check(runtime.access.ReadPedWearingGoggles(gogglesToken, gogglesOutput) != ClockResult::Ok && gogglesOutput,
              "between-callback goggles query preserves caller bool");
        std::thread gogglesForeign([&] { gogglesResult = runtime.access.ReadPedWearingGoggles(gogglesToken, gogglesOutput); });
        gogglesForeign.join();
        Check(gogglesResult != ClockResult::Ok && gogglesOutput && !runtime.stopping.load(),
              "foreign goggles read never Stops another Runtime or returns successful absence");
        const auto invokeGoggles = [&]
        {
            Check(gogglesService.Renew(runtime.generation.load()), "renew actual goggles callback");
            fixture.invokeTimer();
        };
        gogglesCall.action = [&](NativeGameTimingAccess& access)
        {
            auto staleGoggles = gogglesToken;
            ++staleGoggles.version;
            Check(access.ReadPedWearingGoggles(staleGoggles, gogglesOutput) != ClockResult::Ok && gogglesOutput && !runtime.stopping.load(),
                  "stale initial goggles token refuses without bool or Stop");
        };
        invokeGoggles();
        for (const auto association : {std::uintptr_t{0}, std::uintptr_t{1}, (std::numeric_limits<std::uintptr_t>::max)()})
        {
            std::memcpy(reinterpret_cast<void*>(gogglesAddress), &association, sizeof(association));
            resetGogglesRead();
            gogglesCall.action = [&](NativeGameTimingAccess&)
            {
                try
                {
                    gogglesOutput = static_cast<const CPed&>(*oldGogglesSdk).IsWearingGoggles();
                    gogglesResult = ClockResult::Ok;
                }
                catch (const std::exception& error)
                {
                    Check(false, error.what());
                }
            };
            invokeGoggles();
            std::uintptr_t afterState = 0;
            std::memcpy(&afterState, reinterpret_cast<void*>(gogglesAddress + Goggles::PointerBytes), sizeof(afterState));
            Check(gogglesResult == ClockResult::Ok && gogglesOutput == (association != 0) && ClockPedGogglesOwnerTest::reads == 2 &&
                      afterState == adjacentGogglesState && gogglesScenario.eventCount == 0,
                  "actual SDK observes association presence twice without target/vision/task access, including inaccessible nonnull target bits");
        }
        gogglesCall.action = [&](NativeGameTimingAccess& access) { gogglesResult = access.ReadPedWearingGoggles(gogglesToken, gogglesOutput); };
        for (bool priorGogglesOutput : {false, true})
        {
            resetGogglesRead();
            ClockPedGogglesOwnerTest::failNext = true;
            gogglesOutput = priorGogglesOutput;
            invokeGoggles();
            Check(gogglesResult == ClockResult::ReadFailure && gogglesOutput == priorGogglesOutput && !runtime.stopping.load(),
                  "native read refusal preserves either prior bool and valid Runtime");
        }
        for (const auto firstAssociation : {std::uintptr_t{0}, std::uintptr_t{1}})
        {
            resetGogglesRead();
            std::memcpy(reinterpret_cast<void*>(gogglesAddress), &firstAssociation, sizeof(firstAssociation));
            ClockPedGogglesOwnerTest::replacement = firstAssociation + 1;
            ClockPedGogglesOwnerTest::changeAfter = 1;
            gogglesOutput = true;
            invokeGoggles();
            Check(gogglesResult == ClockResult::OwnerConflict && gogglesOutput && !runtime.stopping.load(),
                  "changing association refuses exact pointer coherence even if both observed values are nonnull");
        }
        resetGogglesRead();
        const auto&                gogglesPin = Goggles::Code[0];
        std::vector<unsigned char> changedGogglesCode(gogglesPin.bytes, gogglesPin.bytes + gogglesPin.size);
        changedGogglesCode[0] ^= 1;
        fixture.Patch(gogglesPin.rva, changedGogglesCode.data(), changedGogglesCode.size());
        invokeGoggles();
        Check(gogglesResult != ClockResult::Ok && gogglesOutput && !runtime.stopping.load(),
              "unknown native goggles profile means refusal rather than successful no-goggles");
        fixture.Patch(gogglesPin.rva, gogglesPin.bytes, gogglesPin.size);
        const auto selectedGoggles = gogglesToken;
        resetGogglesRead();
        ClockPedGogglesOwnerTest::callerToken = &gogglesToken;
        invokeGoggles();
        Check(gogglesResult == ClockResult::Ok && gogglesOutput && gogglesToken.version == selectedGoggles.version + 1 && !runtime.stopping.load(),
              "Runtime fixes selected token before native owner callbacks and never redirects to caller's edited token");
        gogglesToken = selectedGoggles;
        retire(gogglesService);
    }
    // The old SDK receiver outlives its first Runtime only in this controlled
    // fixture. Its immutable token must remain stale, never become a lease.
    resetGogglesRead();
    {
        NativeClockService      gogglesService;
        NativePedReferenceToken freshGogglesToken;
        gogglesCall.action = [&](NativeGameTimingAccess& access) { gogglesResult = access.AcquirePedReference(1, freshGogglesToken); };
        BeginOwned(gogglesService, fixture, false, true, true, &GogglesCallback::Invoke, &gogglesCall);
        fixture.invokeTimer();
        Check(gogglesResult == ClockResult::Ok, "new goggles Runtime acquires its own current registered relation");
        const auto invokeGoggles = [&]
        {
            Check(gogglesService.Renew(runtime.generation.load()), "renew new-generation goggles callback");
            fixture.invokeTimer();
        };
        bool staleSdkRefused = false;
        gogglesCall.action = [&](NativeGameTimingAccess&)
        {
            try
            {
                (void)static_cast<const CPed&>(*oldGogglesSdk).IsWearingGoggles();
            }
            catch (const std::runtime_error& error)
            {
                staleSdkRefused = std::string(error.what()).find("IsWearingGoggles") != std::string::npos;
            }
        };
        invokeGoggles();
        Check(staleSdkRefused && !ClockPedGogglesOwnerTest::reads && !runtime.stopping.load(),
              "old SDK refuses after End/newBegin despite same native index/actor pointer, without stopping current owner");
        auto                 freshGogglesSdk = makeGogglesPed(freshGogglesToken);
        const std::uintptr_t noAssociation = 0;
        std::memcpy(reinterpret_cast<void*>(gogglesAddress), &noAssociation, sizeof(noAssociation));
        gogglesCall.action = [&](NativeGameTimingAccess&)
        {
            try
            {
                gogglesOutput = static_cast<const CPed&>(*freshGogglesSdk).IsWearingGoggles();
                gogglesResult = ClockResult::Ok;
            }
            catch (const std::exception& error)
            {
                Check(false, error.what());
            }
        };
        invokeGoggles();
        Check(gogglesResult == ClockResult::Ok && !gogglesOutput && ClockPedGogglesOwnerTest::reads == 2 && gogglesScenario.eventCount == 0,
              "fresh SDK can report genuine absence on current generation without task lookup");
        resetGogglesRead();
        gogglesOutput = true;
        const auto gogglesNode = ClockPedReferenceOwnerTest::node;
        const auto gogglesUnregisters = ClockPedReferenceOwnerTest::unregisters;
        ClockPedGogglesOwnerTest::cell = ClockPedReferenceOwnerTest::cell;
        ClockPedGogglesOwnerTest::loseToken = true;
        gogglesCall.action = [&](NativeGameTimingAccess& access) { gogglesResult = access.ReadPedWearingGoggles(freshGogglesToken, gogglesOutput); };
        invokeGoggles();
        Check(gogglesResult != ClockResult::Ok && gogglesOutput && runtime.stopping.load() && ClockPedGogglesOwnerTest::reads == 2,
              "successful raw absence followed by token loss Stops admitted Runtime and preserves caller bool");
        Check(!gogglesService.End() && runtime.pedReferences.IsRetained(), "lost goggles weak word cannot free a cell still named by native bank");
        fixture.invokeTimer();
        Check(ClockPedReferenceOwnerTest::unregisters == gogglesUnregisters && runtime.pedReferences.IsRetained(),
              "stopping query retains native-address cell debt without unsafe unregister");
        // Supply only the established native bank address-retirement outcome.
        // No read-only operation grants a force-release or target lifetime.
        ClockPedReferenceOwnerTest::Put(gogglesNode + 8, std::uintptr_t{0});
        const auto freeGogglesNode = ClockPedReferenceOwnerTest::Get<std::uintptr_t>(fixture.image + R::FreeHeadRva);
        ClockPedReferenceOwnerTest::Put(gogglesNode, freeGogglesNode);
        ClockPedReferenceOwnerTest::Put(fixture.image + R::FreeHeadRva, gogglesNode);
        ClockPedReferenceOwnerTest::Put(reinterpret_cast<std::uintptr_t>(pool.objects) + R::EntityHeadOffset, std::uintptr_t{0});
        fixture.invokeTimer();
        retire(gogglesService);
    }
    ClockPedGogglesOwnerTest::watched = ClockPedGogglesOwnerTest::cell = 0;
    ClockPedGogglesOwnerTest::callerToken = nullptr;
    ClockPedGogglesOwnerTest::failNext = ClockPedGogglesOwnerTest::loseToken = false;
    std::puts("Production native registered goggles callback owner fixture completed");
    std::puts("Production actual ped goggles SDK callback fixture completed");
}
