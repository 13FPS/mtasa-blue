/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Physical water intent through the actual SDK and sole ped reference owner
 ****************************************************************************/
{
    namespace W = NativePedWaterFlagsProfile;
    for (const auto& pin : W::Code)
        admit(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
    for (const auto& pin : NativePedMetadataProfile::Code)
        admit(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
    struct DefaultWaterOwner final : NativeGameTimingAccess
    {
        ClockResult SetTimeScale(float) noexcept override { return ClockResult::NotInitialized; }
        ClockResult SetMinuteDuration(std::uint32_t) noexcept override { return ClockResult::NotInitialized; }
        ClockResult Pause(bool) noexcept override { return ClockResult::NotInitialized; }
    } absent;
    Check(absent.SetPedInWaterFlags({}, true) == ClockResult::NotInitialized && runtime.access.SetPedInWaterFlags({}, false) == ClockResult::NotInitialized,
          "default timing and unbound runtime grant no physical water intent");
    struct WaterCallback
    {
        std::function<void(NativeGameTimingAccess&)> action;
        static void Invoke(NativeGameTimingAccess& access, void* opaque) { static_cast<WaterCallback*>(opaque)->action(access); }
    } call;
    ped_water_fixture::Game game;
    const auto              reset = [&]
    {
        store(W::FlagsOffset, std::uint32_t{0xa5003002} & ~W::WaterMask);
        ClockPedPhysicalScalarsOwnerTest::stores = ClockPedPhysicalScalarsOwnerTest::failAt = ClockPedPhysicalScalarsOwnerTest::loseAt = 0;
        ClockPedPhysicalScalarsOwnerTest::cell = 0;
        ClockPedPhysicalScalarsOwnerTest::loseToken = ClockPedWaterOwnerTest::failAfter = false;
    };
    {
        reset();
        NativeClockService      service;
        NativePedReferenceToken token;
        ClockResult             result = ClockResult::NotInitialized;
        call.action = [&](NativeGameTimingAccess& access) { result = access.AcquirePedReference(1, token); };
        BeginOwned(service, fixture, false, true, true, &WaterCallback::Invoke, &call);
        fixture.invokeTimer();
        Check(result == ClockResult::Ok, "physical water setter uses the actual sole registered native ped controller");
        Check(runtime.access.SetPedInWaterFlags(token, true) != ClockResult::Ok && !ClockPedPhysicalScalarsOwnerTest::stores,
              "retained token cannot authorize between-callback water writes");
        std::thread other([&] { result = runtime.access.SetPedInWaterFlags(token, true); });
        other.join();
        Check(result != ClockResult::Ok && !ClockPedPhysicalScalarsOwnerTest::stores && !runtime.stopping.load(),
              "foreign water admission refuses without retiring another runtime");
        const auto invoke = [&]
        {
            Check(service.Renew(runtime.generation.load()), "renew one real physical water callback");
            fixture.invokeTimer();
        };
        call.action = [&](NativeGameTimingAccess& access)
        {
            auto stale = token;
            ++stale.version;
            Check(access.SetPedInWaterFlags(stale, true) != ClockResult::Ok && !runtime.stopping.load(),
                  "stale reference token cannot mutate physical water storage or stop a different admitted owner");
        };
        invoke();
        for (const auto originalPair : {0u, W::SubmergedMask, W::TouchingMask, W::WaterMask})
            for (const bool enabled : {false, true})
            {
                const auto original = (std::uint32_t{0xa5003002} & ~W::WaterMask) | originalPair;
                store(W::FlagsOffset, original);
                const auto writes = ClockPedPhysicalScalarsOwnerTest::stores;
                call.action = [&](NativeGameTimingAccess& access)
                {
                    ped_water_fixture::PedWaterFixture ped(game, access, access, access, access, access, access, access, access, access, access, access, access,
                                                           access, access, access, token, 0x12345678);
                    CPed&                              sdk = ped;
                    try
                    {
                        sdk.SetInWaterFlags(enabled);
                    }
                    catch (const std::exception& error)
                    {
                        // The native callback boundary is noexcept. Report a
                        // refused SDK operation here rather than losing its
                        // cause to the CRT terminate handler outside the test.
                        Check(false, error.what());
                    }
                    Check(sdk.GetType() == 0x12345678 && !sdk.GetCustomMoveAnim() && !ped.repairCalls,
                          "new water SDK stays in the actual Stats/Pose chain without querying the borrowed game or fabricating physics repair");
                };
                invoke();
                Check(raw(W::FlagsOffset) == ((original & ~W::WaterMask) | (enabled ? W::WaterMask : 0)) &&
                          ClockPedPhysicalScalarsOwnerTest::stores == writes + 1 && !runtime.stopping.load() && !runtime.playerInfo.IsBound(),
                      "actual CPed virtual commits exactly both physical water bits, preserves every neighbor and never substitutes the current player");
            }
        const auto writes = ClockPedPhysicalScalarsOwnerTest::stores;
        call.action = [&](NativeGameTimingAccess& access)
        {
            // A changed native identity permanently withdraws this published
            // borrow. Test that terminal case after the successful SDK cases;
            // restoring its raw type cannot resurrect the old CPU token.
            pool.objects[NativePedMetadataProfile::TypeOffset] = 2;
            result = access.SetPedInWaterFlags(token, true);
            pool.objects[NativePedMetadataProfile::TypeOffset] = 3;
        };
        invoke();
        Check(result != ClockResult::Ok && ClockPedPhysicalScalarsOwnerTest::stores == writes && !runtime.stopping.load(),
              "an unsupported native identity preserves water storage without stopping unrelated owner services");
        call.action = [&](NativeGameTimingAccess& access) { result = access.SetPedInWaterFlags(token, true); };
        invoke();
        Check(result == ClockResult::NotInitialized && ClockPedPhysicalScalarsOwnerTest::stores == writes && !runtime.stopping.load(),
              "restoring a raw native type never republishes an invalidated borrow or replays water intent");
        retire(service);
    }
    std::puts("Production actual physical water SDK callback fixture completed");
    {
        reset();
        NativeClockService      service;
        NativePedReferenceToken token;
        ClockResult             result = ClockResult::NotInitialized;
        call.action = [&](NativeGameTimingAccess& access) { result = access.AcquirePedReference(1, token); };
        BeginOwned(service, fixture, false, true, true, &WaterCallback::Invoke, &call);
        fixture.invokeTimer();
        Check(result == ClockResult::Ok, "unknown-ack water token admission");
        ClockPedWaterOwnerTest::failAfter = true;
        call.action = [&](NativeGameTimingAccess& access) { result = access.SetPedInWaterFlags(token, true); };
        Check(service.Renew(runtime.generation.load()), "renew before one uncertain physical pair acknowledgement");
        fixture.invokeTimer();
        Check(result == ClockResult::WriteFailure && runtime.stopping.load() && (raw(W::FlagsOffset) & W::WaterMask) == W::WaterMask &&
                  ClockPedPhysicalScalarsOwnerTest::stores == 1,
              "an unknown final-CAS acknowledgement stops the same retained owner with possibly accepted physical flags preserved");
        fixture.invokeTimer();
        Check(ClockPedPhysicalScalarsOwnerTest::stores == 1, "stopping callback cannot retry the uncertain physical pair store");
        retire(service);
    }
    {
        reset();
        NativeClockService      service;
        NativePedReferenceToken token;
        ClockResult             result = ClockResult::NotInitialized;
        call.action = [&](NativeGameTimingAccess& access) { result = access.AcquirePedReference(1, token); };
        BeginOwned(service, fixture, false, true, true, &WaterCallback::Invoke, &call);
        fixture.invokeTimer();
        Check(result == ClockResult::Ok, "post-store water reference-loss token admission");
        const auto node = ClockPedReferenceOwnerTest::node;
        const auto unlinks = ClockPedReferenceOwnerTest::unregisters;
        ClockPedPhysicalScalarsOwnerTest::cell = ClockPedReferenceOwnerTest::cell;
        ClockPedPhysicalScalarsOwnerTest::loseToken = true;
        call.action = [&](NativeGameTimingAccess& access) { result = access.SetPedInWaterFlags(token, true); };
        Check(service.Renew(runtime.generation.load()), "renew before physical water acknowledgement loses its original weak token");
        fixture.invokeTimer();
        Check(result != ClockResult::Ok && runtime.stopping.load() && (raw(W::FlagsOffset) & W::WaterMask) == W::WaterMask &&
                  ClockPedPhysicalScalarsOwnerTest::stores == 1 && runtime.pedReferences.IsRetained() && !service.End(),
              "post-CAS token loss preserves the accepted water pair and original native-visible cell debt");
        fixture.invokeTimer();
        Check(ClockPedPhysicalScalarsOwnerTest::stores == 1 && ClockPedReferenceOwnerTest::unregisters == unlinks && !service.End(),
              "water retirement neither retries accepted mutation nor replays uncertain native reference cleanup");
        // Supply only the existing ResolveReferences full-bank absence
        // postcondition. Accepted physical flags are not restoration data.
        ClockPedReferenceOwnerTest::Put(node + 8, std::uintptr_t{0});
        const auto head = ClockPedReferenceOwnerTest::Get<std::uintptr_t>(fixture.image + R::FreeHeadRva);
        ClockPedReferenceOwnerTest::Put(node, head);
        ClockPedReferenceOwnerTest::Put(fixture.image + R::FreeHeadRva, node);
        ClockPedReferenceOwnerTest::Put(reinterpret_cast<std::uintptr_t>(pool.objects) + R::EntityHeadOffset, std::uintptr_t{0});
        fixture.invokeTimer();
        retire(service);
        Check((raw(W::FlagsOffset) & W::WaterMask) == W::WaterMask && ClockPedPhysicalScalarsOwnerTest::stores == 1 &&
                  ClockPedReferenceOwnerTest::unregisters == unlinks,
              "proved native address retirement completes without changing accepted physical water state");
    }
    ClockPedWaterOwnerTest::failAfter = false;
    ClockPedPhysicalScalarsOwnerTest::failAt = ClockPedPhysicalScalarsOwnerTest::loseAt = 0;
    std::puts("Production native ped physical water callback owner fixture completed");
}
