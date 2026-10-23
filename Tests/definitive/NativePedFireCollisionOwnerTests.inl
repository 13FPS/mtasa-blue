/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual fire query and collision intent on the sole ped token owner
 ****************************************************************************/
{
    namespace Fire = NativePedFireProfile;
    namespace Collision = NativePedCollisionProfile;
    for (const auto& pin : NativePedMetadataProfile::Code)
        admit(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
    for (const auto& pin : Fire::Code)
        admit(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
    for (const auto& pin : Collision::Code)
        admit(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
    const auto fireGroupEntry = fixture.image + Fire::GroupRva;
    const auto fireBranch = static_cast<std::uint32_t>(Fire::BranchRva);
    const auto fireSelector = static_cast<std::uint8_t>(Fire::Selector);
    admit(Fire::GroupSlotRva, &fireGroupEntry, sizeof(fireGroupEntry), PAGE_READONLY);
    admit(Fire::RemapRva + Fire::OpcodeIndex, &fireSelector, sizeof(fireSelector), PAGE_EXECUTE_READ);
    admit(Fire::DispatchRva + Fire::Selector * 4, &fireBranch, sizeof(fireBranch), PAGE_EXECUTE_READ);
    const auto collisionGroupEntry = fixture.image + Collision::GroupRva;
    const auto collisionBranch = static_cast<std::uint32_t>(Collision::CaseRva);
    admit(Collision::GroupSlotRva, &collisionGroupEntry, sizeof(collisionGroupEntry), PAGE_READONLY);
    admit(Collision::TableRva + Collision::CaseIndex * 4, &collisionBranch, sizeof(collisionBranch), PAGE_EXECUTE_READ);
    struct FireCollisionCallback
    {
        std::function<void(NativeGameTimingAccess&)> action;
        static void Invoke(NativeGameTimingAccess& access, void* opaque) { static_cast<FireCollisionCallback*>(opaque)->action(access); }
    } flagsCall;
    struct DefaultFireCollisionOwner final : NativeGameTimingAccess
    {
        ClockResult SetTimeScale(float) noexcept override { return ClockResult::NotInitialized; }
        ClockResult SetMinuteDuration(std::uint32_t) noexcept override { return ClockResult::NotInitialized; }
        ClockResult Pause(bool) noexcept override { return ClockResult::NotInitialized; }
    } flagsAbsent;
    ped_collision_fixture::Game flagsGame;
    const auto                  fireAddress = reinterpret_cast<std::uintptr_t>(pool.objects) + Fire::FireOffset;
    const auto                  collisionAddress = reinterpret_cast<std::uintptr_t>(pool.objects) + Collision::FlagsOffset;
    const auto                  resetFlagHosts = [&]
    {
        ClockPedFireOwnerTest::watched = fireAddress;
        ClockPedFireOwnerTest::reads = ClockPedFireOwnerTest::changeAfter = 0;
        ClockPedFireOwnerTest::cell = ClockPedFireOwnerTest::replacement = 0;
        ClockPedFireOwnerTest::failNext = ClockPedFireOwnerTest::loseToken = false;
        ClockPedCollisionOwnerTest::stores = 0;
        ClockPedCollisionOwnerTest::cell = 0;
        ClockPedCollisionOwnerTest::neighborRace = ClockPedCollisionOwnerTest::loseToken = ClockPedCollisionOwnerTest::failAfter = false;
        const std::uintptr_t noFire = 0;
        std::memcpy(reinterpret_cast<void*>(fireAddress), &noFire, sizeof(noFire));
        const std::uint32_t initialCollision = 0xdeadbeeeu;
        std::memcpy(reinterpret_cast<void*>(collisionAddress), &initialCollision, sizeof(initialCollision));
    };
    const auto finishLostFlagsCell = [&](NativeClockService& selectedService, std::uintptr_t registeredNode, unsigned unregistersBefore)
    {
        Check(!selectedService.End() && runtime.pedReferences.IsRetained(), "lost flags token retains native-address-visible cell debt");
        fixture.invokeTimer();
        Check(ClockPedReferenceOwnerTest::unregisters == unregistersBefore && runtime.pedReferences.IsRetained(),
              "stopping flags callbacks neither repeat intent nor unsafe-unlink a still-named cell");
        // Only established native bank progress supplies address retirement.
        // This is not a new production cleanup operation or forced debt release.
        ClockPedReferenceOwnerTest::Put(registeredNode + 8, std::uintptr_t{0});
        const auto freeFlagsNode = ClockPedReferenceOwnerTest::Get<std::uintptr_t>(fixture.image + R::FreeHeadRva);
        ClockPedReferenceOwnerTest::Put(registeredNode, freeFlagsNode);
        ClockPedReferenceOwnerTest::Put(fixture.image + R::FreeHeadRva, registeredNode);
        ClockPedReferenceOwnerTest::Put(reinterpret_cast<std::uintptr_t>(pool.objects) + R::EntityHeadOffset, std::uintptr_t{0});
        fixture.invokeTimer();
    };
    if (fireCollisionOnly == 1)
    {
        bool fireOutput = true;
        Check(flagsAbsent.ReadPedOnFire({}, fireOutput) == ClockResult::NotInitialized && fireOutput,
              "older owner cannot fabricate a successful no-fire result");
        resetFlagHosts();
        NativeClockService      fireService;
        NativePedReferenceToken fireToken;
        ClockResult             fireResult = ClockResult::NotInitialized;
        flagsCall.action = [&](NativeGameTimingAccess& access) { fireResult = access.AcquirePedReference(1, fireToken); };
        BeginOwned(fireService, fixture, false, true, true, &FireCollisionCallback::Invoke, &flagsCall);
        fixture.invokeTimer();
        Check(fireResult == ClockResult::Ok && fireToken.version && runtime.pedReferences.IsRetained(), "fire query uses sole real registered-ped controller");
        Check(runtime.access.ReadPedOnFire(fireToken, fireOutput) != ClockResult::Ok && fireOutput, "between-callback fire query preserves caller bool");
        std::thread fireForeign([&] { fireResult = runtime.access.ReadPedOnFire(fireToken, fireOutput); });
        fireForeign.join();
        Check(fireResult != ClockResult::Ok && fireOutput && !runtime.stopping.load(), "foreign fire reader never Stops the genuine owner or invents absence");
        const auto invokeFire = [&]
        {
            Check(fireService.Renew(runtime.generation.load()), "renew actual fire callback");
            fixture.invokeTimer();
        };
        flagsCall.action = [&](NativeGameTimingAccess& access)
        {
            auto staleFire = fireToken;
            ++staleFire.version;
            Check(access.ReadPedOnFire(staleFire, fireOutput) != ClockResult::Ok && fireOutput && !runtime.stopping.load(),
                  "stale initial fire relation refuses without false or Stop");
        };
        invokeFire();
        for (const auto association : {std::uintptr_t{0}, std::uintptr_t{1}, (std::numeric_limits<std::uintptr_t>::max)()})
        {
            std::memcpy(reinterpret_cast<void*>(fireAddress), &association, sizeof(association));
            ClockPedFireOwnerTest::reads = 0;
            flagsCall.action = [&](NativeGameTimingAccess& access)
            {
                ped_collision_fixture::PedCollisionFixture firePed(flagsGame, access, access, access, access, access, access, access, access, access, access,
                                                                   access, access, access, access, access, access, access, access, access, access, fireToken,
                                                                   0x12345678);
                const CPed&                                fireSdk = firePed;
                CEntity&                                   baseEntityFireSdk = firePed;
                const auto                                 readsBeforeFallback = ClockPedFireOwnerTest::reads;
                Check(!baseEntityFireSdk.IsOnFire() && ClockPedFireOwnerTest::reads == readsBeforeFallback,
                      "actual shared nonconst CEntity fallback returns false without changing the distinct native const CPed query");
                try
                {
                    fireOutput = fireSdk.IsOnFire();
                    fireResult = ClockResult::Ok;
                }
                catch (const std::exception& error)
                {
                    Check(false, error.what());
                }
            };
            invokeFire();
            Check(fireResult == ClockResult::Ok && fireOutput == (association != 0) && ClockPedFireOwnerTest::reads == 2,
                  "actual const CPed fire virtual reads association presence twice without dereferencing even an invalid target");
        }
        flagsCall.action = [&](NativeGameTimingAccess& access) { fireResult = access.ReadPedOnFire(fireToken, fireOutput); };
        fireOutput = true;
        ClockPedFireOwnerTest::reads = 0;
        ClockPedFireOwnerTest::failNext = true;
        invokeFire();
        Check(fireResult == ClockResult::ReadFailure && fireOutput && !runtime.stopping.load(),
              "native fire read failure preserves true without Stop or successful absence");
        const std::uintptr_t initiallyClear = 0;
        std::memcpy(reinterpret_cast<void*>(fireAddress), &initiallyClear, sizeof(initiallyClear));
        ClockPedFireOwnerTest::reads = 0;
        ClockPedFireOwnerTest::changeAfter = 1;
        ClockPedFireOwnerTest::replacement = 1;
        invokeFire();
        Check(fireResult == ClockResult::OwnerConflict && fireOutput && !runtime.stopping.load(),
              "transitioning association refuses coherent bool while retaining valid registered owner");
        ClockPedFireOwnerTest::changeAfter = 0;
        const auto&                firePin = Fire::Code[0];
        std::vector<unsigned char> changedFireCode(firePin.bytes, firePin.bytes + firePin.size);
        changedFireCode[0] ^= 1;
        fixture.Patch(firePin.rva, changedFireCode.data(), changedFireCode.size());
        invokeFire();
        Check(fireResult != ClockResult::Ok && fireOutput && !runtime.stopping.load(), "unknown fire code refuses instead of reporting no fire");
        fixture.Patch(firePin.rva, firePin.bytes, firePin.size);
        ClockPedFireOwnerTest::reads = 0;
        invokeFire();
        Check(fireResult == ClockResult::Ok && fireOutput, "read-only preflight refusal leaves the same valid callback usable");
        const auto fireNode = ClockPedReferenceOwnerTest::node;
        const auto fireUnregisters = ClockPedReferenceOwnerTest::unregisters;
        std::memcpy(reinterpret_cast<void*>(fireAddress), &initiallyClear, sizeof(initiallyClear));
        fireOutput = true;
        ClockPedFireOwnerTest::reads = 0;
        ClockPedFireOwnerTest::cell = ClockPedReferenceOwnerTest::cell;
        ClockPedFireOwnerTest::loseToken = true;
        invokeFire();
        Check(fireResult != ClockResult::Ok && fireOutput && runtime.stopping.load() && ClockPedFireOwnerTest::reads == 2,
              "successful raw absence followed by token loss Stops only admitted runtime and preserves caller bool");
        finishLostFlagsCell(fireService, fireNode, fireUnregisters);
        retire(fireService);
        std::puts("Production native registered fire callback owner fixture completed");
        std::puts("Production actual const ped fire SDK callback fixture completed");
    }
    else
    {
        Check(flagsAbsent.SetPedUsesCollision({}, true) == ClockResult::NotInitialized, "older owner cannot fabricate collision intent acceptance");
        resetFlagHosts();
        {
            NativeClockService      collisionService;
            NativePedReferenceToken collisionToken;
            ClockResult             collisionResult = ClockResult::NotInitialized;
            flagsCall.action = [&](NativeGameTimingAccess& access) { collisionResult = access.AcquirePedReference(1, collisionToken); };
            BeginOwned(collisionService, fixture, false, true, true, &FireCollisionCallback::Invoke, &flagsCall);
            fixture.invokeTimer();
            Check(collisionResult == ClockResult::Ok && collisionToken.version, "collision setter uses sole actual registered relation");
            Check(runtime.access.SetPedUsesCollision(collisionToken, true) != ClockResult::Ok && !ClockPedCollisionOwnerTest::stores,
                  "retained token does not grant collision write between callbacks");
            std::thread collisionForeign([&] { collisionResult = runtime.access.SetPedUsesCollision(collisionToken, true); });
            collisionForeign.join();
            Check(collisionResult != ClockResult::Ok && !ClockPedCollisionOwnerTest::stores && !runtime.stopping.load(),
                  "foreign collision intent cannot Stop another owner");
            const auto invokeCollision = [&]
            {
                Check(collisionService.Renew(runtime.generation.load()), "renew actual collision callback");
                fixture.invokeTimer();
            };
            flagsCall.action = [&](NativeGameTimingAccess& access)
            {
                auto staleCollision = collisionToken;
                ++staleCollision.version;
                Check(access.SetPedUsesCollision(staleCollision, true) != ClockResult::Ok && !ClockPedCollisionOwnerTest::stores && !runtime.stopping.load(),
                      "stale initial collision token refuses before CAS without stopping valid owner");
            };
            invokeCollision();
            for (const bool requestedCollision : {true, false, false})
            {
                std::uint32_t previousCollision = 0;
                std::memcpy(&previousCollision, reinterpret_cast<void*>(collisionAddress), sizeof(previousCollision));
                const auto previousAttempts = ClockPedCollisionOwnerTest::stores;
                flagsCall.action = [&](NativeGameTimingAccess& access)
                {
                    ped_collision_fixture::PedCollisionFixture collisionPed(flagsGame, access, access, access, access, access, access, access, access, access,
                                                                            access, access, access, access, access, access, access, access, access, access,
                                                                            access, collisionToken, 0x12345678);
                    CEntity&                                   collisionSdk = collisionPed;
                    try
                    {
                        collisionSdk.SetUsesCollision(requestedCollision);
                        collisionResult = ClockResult::Ok;
                    }
                    catch (const std::exception& error)
                    {
                        Check(false, error.what());
                    }
                };
                invokeCollision();
                std::uint32_t currentCollision = 0;
                std::memcpy(&currentCollision, reinterpret_cast<void*>(collisionAddress), sizeof(currentCollision));
                const auto expectedCollision = (previousCollision & ~Collision::UsesCollisionMask) | (requestedCollision ? Collision::UsesCollisionMask : 0u);
                Check(collisionResult == ClockResult::Ok && currentCollision == expectedCollision && ClockPedCollisionOwnerTest::stores == previousAttempts + 1,
                      "actual CEntity setter changes only collision bit0 using one CAS even for same-value intent");
            }
            retire(collisionService);
        }
        for (const unsigned collisionFailure : {1u, 2u, 3u})
        {
            resetFlagHosts();
            NativeClockService      collisionService;
            NativePedReferenceToken collisionToken;
            ClockResult             collisionResult = ClockResult::NotInitialized;
            flagsCall.action = [&](NativeGameTimingAccess& access) { collisionResult = access.AcquirePedReference(1, collisionToken); };
            BeginOwned(collisionService, fixture, false, true, true, &FireCollisionCallback::Invoke, &flagsCall);
            fixture.invokeTimer();
            Check(collisionResult == ClockResult::Ok, "fresh collision fault owner acquires genuine retained relation");
            const auto collisionNode = ClockPedReferenceOwnerTest::node;
            const auto collisionUnregisters = ClockPedReferenceOwnerTest::unregisters;
            ClockPedCollisionOwnerTest::cell = ClockPedReferenceOwnerTest::cell;
            ClockPedCollisionOwnerTest::neighborRace = collisionFailure == 1;
            ClockPedCollisionOwnerTest::failAfter = collisionFailure == 2;
            ClockPedCollisionOwnerTest::loseToken = collisionFailure == 3;
            flagsCall.action = [&](NativeGameTimingAccess& access) { collisionResult = access.SetPedUsesCollision(collisionToken, true); };
            Check(collisionService.Renew(runtime.generation.load()), "renew before collision word conflict or acknowledgement loss");
            fixture.invokeTimer();
            std::uint32_t collisionAfterFailure = 0;
            std::memcpy(&collisionAfterFailure, reinterpret_cast<void*>(collisionAddress), sizeof(collisionAfterFailure));
            Check(collisionResult != ClockResult::Ok && runtime.stopping.load() && ClockPedCollisionOwnerTest::stores == 1 &&
                      collisionAfterFailure == (collisionFailure == 1 ? 0xdeadbeecu : 0xdeadbeefu),
                  "one competing-word attempt or persistent collision acknowledgement loss Stops without retry, repair or neighbor overwrite");
            if (collisionFailure == 3)
                finishLostFlagsCell(collisionService, collisionNode, collisionUnregisters);
            retire(collisionService);
            Check(ClockPedCollisionOwnerTest::stores == 1, "collision retirement cannot replay an entered CAS");
        }
        std::puts("Production native registered collision callback owner fixture completed");
        std::puts("Production actual ped collision SDK callback fixture completed");
    }
    ClockPedFireOwnerTest::watched = ClockPedFireOwnerTest::cell = 0;
    ClockPedFireOwnerTest::failNext = ClockPedFireOwnerTest::loseToken = false;
    ClockPedCollisionOwnerTest::cell = 0;
    ClockPedCollisionOwnerTest::neighborRace = ClockPedCollisionOwnerTest::loseToken = ClockPedCollisionOwnerTest::failAfter = false;
}
