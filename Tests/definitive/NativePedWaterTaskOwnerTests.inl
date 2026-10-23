/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual task-based SDK water predicate on the sole native ped token
 ****************************************************************************/
{
    namespace Q = NativePedWaterTaskProfile;
    namespace IQ = NativePedIntelligenceProfile;
    namespace TQ = NativeTaskPoolProfile;
    for (const auto& pin : IQ::Code)
        admit(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
    for (const auto& pin : TQ::Code)
        admit(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
    for (const auto& pin : Q::Code)
        admit(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
    for (const auto& slot : TQ::Slots)
        for (const auto& method : std::array<std::pair<std::size_t, std::uintptr_t>, 3>{
                 {{0, slot.deletingDestructor}, {TQ::SimpleSlot, slot.isSimple}, {TQ::TypeSlot, slot.getType}}})
        {
            const auto target = fixture.image + method.second;
            admit(slot.vtable + method.first, &target, sizeof(target), PAGE_READONLY);
        }
    const auto eventType = fixture.image + Q::EventTypeRva;
    admit(Q::EventVtableRva + Q::EventTypeSlot, &eventType, sizeof(eventType), PAGE_READONLY);
    const std::uintptr_t nullRoot = 0;
    admit(IQ::RootRva, &nullRoot, sizeof(nullRoot), PAGE_READWRITE);
    admit(TQ::RootRva, &nullRoot, sizeof(nullRoot), PAGE_READWRITE);
    ClockTeleportPool queryIntelligence(fixture, IQ::RootRva, IQ::NativeCapacity, IQ::ObjectStride);
    ClockTeleportPool queryTasks(fixture, TQ::RootRva, TQ::NativeCapacity, TQ::ObjectStride);
    queryIntelligence.Publish();
    queryTasks.Publish();
    queryIntelligence.flags[0] = 3;
    const auto nativeIntel = reinterpret_cast<std::uintptr_t>(queryIntelligence.objects);
    const auto nativePed = reinterpret_cast<std::uintptr_t>(pool.objects);
    std::memcpy(pool.objects + IQ::IntelligenceOffset, &nativeIntel, sizeof(nativeIntel));
    std::array<std::uintptr_t, 13> queryPrefix{};
    queryPrefix[0] = queryPrefix[12] = nativePed;
    const auto publishPrefix = [&] { std::memcpy(queryIntelligence.objects, queryPrefix.data(), sizeof(queryPrefix)); };
    publishPrefix();
    std::array<std::uintptr_t, TQ::Slots.size()> queryTaskAddresses{};
    for (std::size_t index = 0; index != TQ::Slots.size(); ++index)
    {
        const auto queryVtable = fixture.image + TQ::Slots[index].vtable;
        queryTaskAddresses[index] = reinterpret_cast<std::uintptr_t>(queryTasks.objects + index * TQ::ObjectStride);
        queryTasks.flags[index] = static_cast<unsigned char>(index + 1);
        std::memcpy(reinterpret_cast<void*>(queryTaskAddresses[index]), &queryVtable, sizeof(queryVtable));
    }
    bool queryOutput = true;
    Check(absent.ReadPedInWater({}, queryOutput) == ClockResult::NotInitialized && queryOutput &&
              runtime.access.ReadPedInWater({}, queryOutput) == ClockResult::NotInitialized && queryOutput,
          "default/unbound owners preserve water output and grant no task predicate");
    {
        NativeClockService      service;
        NativePedReferenceToken token;
        ClockResult             result = ClockResult::NotInitialized;
        call.action = [&](NativeGameTimingAccess& access) { result = access.AcquirePedReference(1, token); };
        BeginOwned(service, fixture, false, true, true, &WaterCallback::Invoke, &call);
        fixture.invokeTimer();
        Check(result == ClockResult::Ok, "task water query uses the same sole registered reference controller");
        Check(runtime.access.ReadPedInWater(token, queryOutput) != ClockResult::Ok && queryOutput,
              "retained ped token does not grant a between-callback water query");
        std::thread foreign([&] { result = runtime.access.ReadPedInWater(token, queryOutput); });
        foreign.join();
        Check(result != ClockResult::Ok && queryOutput && !runtime.stopping.load(),
              "foreign-thread water query cannot publish false or stop another admitted runtime");
        const auto invokeQuery = [&]
        {
            Check(service.Renew(runtime.generation.load()), "renew the actual water-task callback");
            fixture.invokeTimer();
        };
        call.action = [&](NativeGameTimingAccess& access)
        {
            auto stale = token;
            ++stale.version;
            Check(access.ReadPedInWater(stale, queryOutput) != ClockResult::Ok && queryOutput && !runtime.stopping.load(),
                  "stale initial token admission preserves output and leaves the current owner usable");
        };
        invokeQuery();
        const std::array<std::uintptr_t, 4> selected{0, queryTaskAddresses[0], queryTaskAddresses[1], queryTaskAddresses[2]};
        for (const auto address : selected)
            for (const bool previous : {false, true})
            {
                queryPrefix[Q::PrimaryIndex + 1] = address;
                publishPrefix();
                queryOutput = previous;
                call.action = [&](NativeGameTimingAccess& access)
                {
                    result = access.ReadPedInWater(token, queryOutput);
                    Check(result == ClockResult::Ok && queryOutput == (address == queryTaskAddresses[2]),
                          "same-owner raw query observes primary2 null/known203/305 false and genuine268 true without native calls");
                    ped_water_fixture::PedWaterFixture ped(game, access, access, access, access, access, access, access, access, access, access, access, access,
                                                           access, access, access, access, token, 0x12345678);
                    try
                    {
                        const CPed& sdk = ped;
                        Check(sdk.IsInWater() == (address == queryTaskAddresses[2]),
                              "actual const CPed IsInWater reads the genuine task predicate through the immutable token");
                    }
                    catch (const std::exception& error)
                    {
                        Check(false, error.what());
                    }
                };
                invokeQuery();
                Check(!runtime.stopping.load(), "successful task observations never retire a valid water owner");
            }
        queryPrefix[Q::PrimaryIndex + 1] = 0;
        queryPrefix[1] = queryTaskAddresses[2];
        queryPrefix[6] = queryTaskAddresses[2];
        publishPrefix();
        store(W::FlagsOffset, raw(W::FlagsOffset) | W::WaterMask);
        call.action = [&](NativeGameTimingAccess& access)
        {
            queryOutput = true;
            Check(access.ReadPedInWater(token, queryOutput) == ClockResult::Ok && !queryOutput,
                  "physical water flags and other task priorities do not replace the actual SDK primary2 contract");
        };
        invokeQuery();
        queryPrefix[1] = queryPrefix[6] = 0;
        queryPrefix[Q::PrimaryIndex + 1] = queryTaskAddresses[2];
        publishPrefix();
        const auto unknownVtable = fixture.image + 0x431c5b0;
        std::memcpy(reinterpret_cast<void*>(queryTaskAddresses[2]), &unknownVtable, sizeof(unknownVtable));
        call.action = [&](NativeGameTimingAccess& access)
        {
            for (const bool previous : {false, true})
            {
                queryOutput = previous;
                Check(access.ReadPedInWater(token, queryOutput) == ClockResult::UnsupportedBuild && queryOutput == previous && !runtime.stopping.load(),
                      "unknown/deleting task class remains an explicit refusal, never successful false");
            }
            ped_water_fixture::PedWaterFixture ped(game, access, access, access, access, access, access, access, access, access, access, access, access, access,
                                                   access, access, access, token, 0x12345678);
            bool                               refused = false;
            try
            {
                (void)ped.IsInWater();
            }
            catch (const std::runtime_error&)
            {
                refused = true;
            }
            Check(refused && !runtime.stopping.load(), "actual SDK preserves unknown-class refusal without retiring an unrelated service");
        };
        invokeQuery();
        const auto waterVtable = fixture.image + TQ::Slots[2].vtable;
        std::memcpy(reinterpret_cast<void*>(queryTaskAddresses[2]), &waterVtable, sizeof(waterVtable));
        call.action = [&](NativeGameTimingAccess& access) { result = access.ReadPedInWater(token, queryOutput); };
        invokeQuery();
        Check(result == ClockResult::Ok && queryOutput && !runtime.stopping.load(), "read-only refusal does not poison the later same-owner water query");
        retire(service);
    }
    std::puts("Production actual task-based water SDK callback fixture completed");
    {
        NativeClockService      service;
        NativePedReferenceToken token;
        ClockResult             result = ClockResult::NotInitialized;
        call.action = [&](NativeGameTimingAccess& access) { result = access.AcquirePedReference(1, token); };
        BeginOwned(service, fixture, false, true, true, &WaterCallback::Invoke, &call);
        fixture.invokeTimer();
        Check(result == ClockResult::Ok, "late relation-loss water query token admission");
        const auto node = ClockPedReferenceOwnerTest::node;
        const auto unlinks = ClockPedReferenceOwnerTest::unregisters;
        ClockPedIntelligenceOwnerTest::address = nativeIntel;
        ClockPedIntelligenceOwnerTest::cell = ClockPedReferenceOwnerTest::cell;
        ClockPedIntelligenceOwnerTest::clearOnRead = true;
        const auto losses = ClockPedIntelligenceOwnerTest::invalidations;
        queryOutput = false;
        call.action = [&](NativeGameTimingAccess& access) { result = access.ReadPedInWater(token, queryOutput); };
        Check(service.Renew(runtime.generation.load()), "renew before a real intelligence prefix read loses its registered cell acknowledgement");
        fixture.invokeTimer();
        Check(result != ClockResult::Ok && !queryOutput && runtime.stopping.load() && ClockPedIntelligenceOwnerTest::invalidations == losses + 1 &&
                  runtime.pedReferences.IsRetained() && !service.End(),
              "post-success token loss preserves caller bool and stops only the same admitted runtime with original cell debt retained");
        fixture.invokeTimer();
        Check(ClockPedReferenceOwnerTest::unregisters == unlinks && !service.End(), "read-only query retirement cannot replay uncertain reference cleanup");
        ClockPedReferenceOwnerTest::Put(node + 8, std::uintptr_t{0});
        const auto head = ClockPedReferenceOwnerTest::Get<std::uintptr_t>(fixture.image + R::FreeHeadRva);
        ClockPedReferenceOwnerTest::Put(node, head);
        ClockPedReferenceOwnerTest::Put(fixture.image + R::FreeHeadRva, node);
        ClockPedReferenceOwnerTest::Put(nativePed + R::EntityHeadOffset, std::uintptr_t{0});
        fixture.invokeTimer();
        retire(service);
        ClockPedIntelligenceOwnerTest::clearOnRead = false;
    }
    std::memcpy(pool.objects + IQ::IntelligenceOffset, &nullRoot, sizeof(nullRoot));
    std::puts("Production native ped primary water task callback owner fixture completed");
}
