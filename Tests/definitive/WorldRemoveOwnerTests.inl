/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual Runtime world unlink ownership with only final originals doubled
 ****************************************************************************/
namespace ClockWorldRemoveOwnerTest
{
    mta::game_de::ClockResult ReadBound(mta::game_de::NativeWorldBoundRectCalls::Entry original, std::uintptr_t entity,
                                        mta::game_de::NativeWorldBoundRectCalls::Rect& output, mta::game_de::NativeWorldBoundRectCalls::Rect*& returned,
                                        std::uint32_t& exception) noexcept
    {
        using namespace mta::game_de;
        exception = 0;
        if (!current || image != current->image || reinterpret_cast<std::uintptr_t>(original) != image + NativeWorldBoundRectProfile::EntryRva ||
            entity != current->Entity() || current->Read<std::uintptr_t>(entity, 0x18) || current->Read<std::uint32_t>(entity, 0x14))
        {
            invalidInvocation = true;
            return ClockResult::InvalidArgument;
        }
        ++bounds;
        // Explicit final-original double only. The real producer has already
        // admitted normalAtomic/null collision, genuine static default box,
        // finite inline +0 placement and its current native owner context.
        std::array<float, 6> box{};
        std::memcpy(box.data(), reinterpret_cast<void*>(image + NativeWorldBoundRectProfile::DefaultBoxRva), sizeof(box));
        const auto x = current->Read<float>(entity, 8), y = current->Read<float>(entity, 0xc);
        output = {(std::min)(box[0], box[3]) + x, (std::max)(box[1], box[4]) + y, (std::max)(box[0], box[3]) + x, (std::min)(box[1], box[4]) + y};
        returned = &output;
        return ClockResult::Ok;
    }
    mta::game_de::ClockResult Invoke(mta::game_de::NativeWorldRemoveCalls::Entry original, void* supplied, std::uint32_t& exception) noexcept
    {
        using namespace mta::game_de;
        exception = 0;
        const auto entity = reinterpret_cast<std::uintptr_t>(supplied);
        if (!current || image != current->image || entity != current->Entity() ||
            reinterpret_cast<std::uintptr_t>(original) != image + NativePedOrientationProfile::RemoveRva)
        {
            invalidInvocation = true;
            return ClockResult::InvalidArgument;
        }
        ++removes;
        // This single whole-original double includes its internal ignored
        // BoundRect read by definition. It independently walks raw native
        // lists, never applies production expected arrays as simulated effects.
        if (!current->Remove(entity))
        {
            invalidInvocation = true;
            return ClockResult::InvalidArgument;
        }
        if (failAfterEffect)
        {
            exception = 0xe05c0001;
            return ClockResult::OriginalException;
        }
        return ClockResult::Ok;
    }
}
namespace WorldRemoveOwnerFixture
{
    namespace RemoveMem = NativeWorldRemoveMemory;
    namespace RemoveW = NativeWorldIplFixture;
    namespace RemoveF = NativeWorldRemoveFixture;
    void Run(ClockImageFixture& fixture, unsigned scenario)
    {
        Check(scenario < 3, "one finite healthy/refused/unknown WorldRemove Runtime lifetime is selected");
        const bool refused = scenario == 1, unknown = scenario == 2;
        fixture.AdmitApplicationPins();
        WorldGroundOwnerFixture::Writes writes{fixture, {}};
        const auto                      apply = [&](std::uintptr_t rva, const void* bytes, std::size_t size, DWORD protection)
        { WorldGroundOwnerFixture::Writes::Apply(&writes, rva, bytes, size, protection); };
        const auto code = [&](const auto& pins)
        {
            for (const auto& pin : pins)
                apply(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        };
        const auto typed = [&](const auto& pins)
        {
            for (const auto& pin : pins)
                apply(pin.rva, pin.bytes, pin.size, pin.code ? PAGE_EXECUTE_READ : PAGE_READONLY);
        };
        code(NativePedPoolProfile::Code);
        code(NativeModelBankProfile::Code);
        code(NativeStreamingLoadAllProfile::Code);
        typed(NativeStartupOwnerProfile::pins);
        typed(NativeStartupWorldProfile::pins);
        typed(NativeGameStateProfile::pins);
        code(NativeUObjectMembershipProfile::pins);
        code(NativeBorrowedGcProfile::pins);
        for (const auto& pin : NativeStartGameProfile::Code)
            if (pin.rva == 0xab879c || pin.rva == 0xab92f0 || pin.rva == 0x1af4ef0)
                apply(pin.rva, pin.bytes.data(), pin.size, PAGE_EXECUTE_READ);
        NativeStreamingLoadAllFixture::Graph graph(fixture.image, reinterpret_cast<std::uintptr_t>(fixture.frameOwner.data()),
                                                   &WorldGroundOwnerFixture::Writes::Apply, &writes);
        RemoveW::State                       world(fixture.image, &WorldGroundOwnerFixture::Writes::Apply, &writes);
        RemoveF::State                       removal(fixture.image, &WorldGroundOwnerFixture::Writes::Apply, &writes);
        Check(world.Ready() && removal.Ready(), "actual typed entities, normalAtomic metadata, static box and both fixed native list pools are owned");
        world.Reset();
        removal.Reset();
        std::vector<unsigned char> providerRows(20 * NativeStreamingRequestProfile::ProviderRowStride);
        NativeWorldBoundRectFixture::PublishProvider(reinterpret_cast<std::uintptr_t>(fixture.frameOwner.data()), graph.Game(),
                                                     reinterpret_cast<std::uintptr_t>(providerRows.data()), 20);
        writes.Global(NativeGameStateProfile::stateRva, std::uint32_t{9});
        writes.Global(NativeStartGameProfile::ThreadInitializedRva, static_cast<unsigned char>(1));
        writes.Global(NativeBorrowedGcProfile::markerRva, LONG{0});
        fixture.frameOwner[0] = fixture.image + NativeApplicationFrameProfile::vtableRva;
        fixture.SetFrameReceiver(fixture.frameOwner.data());
        CameraRowBorrowOwnerFixture::EnginePump engine(fixture);
        writes.Global(NativeStartGameProfile::ThreadIdRva, engine.thread.load());
        const auto previousImage = ClockReaderTestHost::image;
        ClockReaderTestHost::image = fixture.image;
        ClockWorldRemoveOwnerTest::current = &removal;
        ClockWorldRemoveOwnerTest::image = fixture.image;
        ClockWorldRemoveOwnerTest::removes = ClockWorldRemoveOwnerTest::bounds = 0;
        ClockWorldRemoveOwnerTest::failAfterEffect = ClockWorldRemoveOwnerTest::invalidInvocation = false;
        NativeClockService                service;
        WorldGroundOwnerFixture::Callback callback;
        callback.action = [](NativeGameTimingAccess&) {};
        ClockServiceRequestV1 request;
        request.reserved[0] = NativeClockService::GameTimingAfterApplicationFrameV1;
        ClockServiceStateV1 state, rejected;
        Check(!service.BeginGameTiming(request, rejected, &WorldGroundOwnerFixture::Callback::Invoke, &callback) &&
                  rejected.failure == ClockResult::UnsupportedBuild,
              "WorldRemove scope preserves shipping executable refusal before explicit owned-image admission");
        ClockNumericAssociationIssuedTest::admittedImage = fixture.image;
        const bool began = service.BeginGameTiming(request, state, &WorldGroundOwnerFixture::Callback::Invoke, &callback);
        ClockNumericAssociationIssuedTest::admittedImage = 0;
        Check(began && state.bound && runtime.frameHooked.load(), "WorldRemove work uses the real post-application Runtime owner");
        const auto generation = state.generation;
        const auto pulse = [&](const auto& action, bool active = true)
        {
            unsigned called = 0;
            callback.action = [&](NativeGameTimingAccess& owner)
            {
                ++called;
                Check(GetCurrentThreadId() == engine.thread.load() && GetCurrentThreadId() != runtime.producerThread,
                      "WorldRemove input and original execute on the distinct genuinely admitted engine thread");
                action(owner);
            };
            engine.Pulse(service, generation);
            callback.action = [](NativeGameTimingAccess&) {};
            Check(called == 1 && (!active || !runtime.stopping.load()), "one WorldRemove action consumes one current engine callback");
        };
        Check(runtime.access.RemoveWorldEntity(RemoveMem::Kind::Building, 0) != ClockResult::Ok && !runtime.worldRemovals.Pending() &&
                  !ClockWorldRemoveOwnerTest::removes && !ClockWorldRemoveOwnerTest::bounds,
              "producer-thread access cannot borrow an entity or enter either native original");
        const auto raw = [](std::uintptr_t address, std::size_t bytes)
        {
            std::vector<unsigned char> result(bytes);
            std::memcpy(result.data(), reinterpret_cast<void*>(address), bytes);
            return result;
        };
        if (!refused && !unknown)
        {
            pulse(
                [&](NativeGameTimingAccess& owner)
                {
                    auto&      pool = removal.Nodes();
                    const auto next = removal.Read<std::uintptr_t>(pool.Row(0), 8);
                    RemoveW::Put(pool.Row(0), 8, pool.Row(0));
                    Check(owner.CheckWorldEntityRemoveReady(removal.kind, 0) != ClockResult::Ok && !runtime.worldRemovals.Pending() &&
                              !ClockWorldRemoveOwnerTest::removes,
                          "actual current source cycle refuses before original and retains no domain intent");
                    RemoveW::Put(pool.Row(0), 8, next);
                    const auto flag = pool.flags[0];
                    pool.flags[0] |= 0x80;
                    Check(owner.CheckWorldEntityRemoveReady(removal.kind, 0) != ClockResult::Ok && !runtime.worldRemovals.Pending() &&
                              !ClockWorldRemoveOwnerTest::removes,
                          "a free-but-linked native node refuses before original without creating a lease");
                    pool.flags[0] = flag;
                    RemoveW::Put(removal.Entity(), 0x3c, static_cast<unsigned char>(255));
                    Check(owner.CheckWorldEntityRemoveReady(removal.kind, 0) != ClockResult::Ok && !runtime.worldRemovals.Pending() &&
                              !ClockWorldRemoveOwnerTest::removes,
                          "out-of-range cached native cell refuses before resource mutation");
                    RemoveW::Put(removal.Entity(), 0x3c, static_cast<unsigned char>(2));
                    NativeWorldBoundRectCalls::Rect    rectangle{99, 99, 99, 99};
                    NativeWorldBoundRectCalls::Receipt receipt;
                    Check(NativeWorldBoundRectCalls::Read(runtime.image, generation, removal.kind, 0, rectangle, &CheckTimingOwner,
                                                          &CheckApplicationTimingOwner, &receipt) == ClockResult::Ok &&
                              receipt.entered && receipt.returned && !receipt.uncertain && ClockWorldRemoveOwnerTest::bounds == 1,
                          "actual shared native read-only BoundRect producer runs once under the real Runtime with its final original doubled");
                });
            for (unsigned kindCase = 0; kindCase != 3; ++kindCase)
            {
                pulse(
                    [&](NativeGameTimingAccess& owner)
                    {
                        if (kindCase == 0)
                            removal.SetupBuilding();
                        if (kindCase == 1)
                            removal.SetupDummy();
                        if (kindCase == 2)
                            removal.SetupLod();
                        auto& pool = removal.Nodes();
                        if (kindCase == 1)
                        {
                            RemoveW::Put(pool.Row(1), 0x10, pool.Row(4));
                            const auto previous = ClockWorldRemoveOwnerTest::removes;
                            Check(owner.CheckWorldEntityRemoveReady(removal.kind, 0) != ClockResult::Ok && !runtime.worldRemovals.Pending() &&
                                      ClockWorldRemoveOwnerTest::removes == previous,
                                  "actual DoubleLink reciprocal predecessor mismatch refuses before original");
                            RemoveW::Put(pool.Row(1), 0x10, pool.Row(0));
                        }
                        const auto entityStride =
                            removal.kind == RemoveMem::Kind::Building ? removal.entities.buildings.stride : removal.entities.dummies.stride;
                        const auto entity = raw(removal.Entity(), entityStride);
                        const auto freedFirst = raw(pool.Row(1), pool.stride), freedSecond = raw(pool.Row(3), pool.stride);
                        const auto untouchedFlag = pool.flags[0];
                        Check(owner.CheckWorldEntityRemoveReady(removal.kind, 0) == ClockResult::Ok && !runtime.worldRemovals.Pending(),
                              "real current actor-null/default-box/matrix/list admission is ready without preserving a future borrow");
                        const auto previous = ClockWorldRemoveOwnerTest::removes;
                        Check(owner.RemoveWorldEntity(removal.kind, 0) == ClockResult::Ok && ClockWorldRemoveOwnerTest::removes == previous + 1 &&
                                  runtime.worldRemovals.CanDiscard() && !runtime.worldRemovals.Uncertain(),
                              "actual Runtime domain enters one genuine whole WorldRemove double and acknowledges current native resource effects");
                        Check(pool.flags[1] == 0x83 && pool.flags[3] == 0x89 && pool.flags[0] == untouchedFlag &&
                                  removal.Read<std::int32_t>(pool.Address(), 0x14) == 1 && raw(pool.Row(1), pool.stride) == freedFirst &&
                                  raw(pool.Row(3), pool.stride) == freedSecond && removal.Read<std::uintptr_t>(removal.Head(2)) == pool.Row(0) &&
                                  removal.Read<std::uintptr_t>(pool.Row(0), 8) == pool.Row(2) && removal.Read<std::uintptr_t>(removal.Head(3)) == pool.Row(4) &&
                                  raw(removal.Entity(), entity.size()) == entity,
                              "Building/Dummy/LOD release only two selected slots, preserve generations/freed payload/entity and lower cursor exactly");
                    });
            }
        }
        else if (refused)
        {
            pulse(
                [&](NativeGameTimingAccess& owner)
                {
                    RemoveW::Put(removal.Entity(), 0x20, removal.entities.dummies.Row(0));
                    Check(owner.RemoveWorldEntity(removal.kind, 0) != ClockResult::Ok && !runtime.worldRemovals.Pending() &&
                              !runtime.worldRemovals.Uncertain() && !ClockWorldRemoveOwnerTest::removes && !ClockWorldRemoveOwnerTest::bounds,
                          "unqualified actor-present receiver refuses before either original and creates no retained intent");
                    RemoveW::Put(removal.Entity(), 0x20, std::uintptr_t{0});
                },
                false);
        }
        else
        {
            ClockWorldRemoveOwnerTest::failAfterEffect = true;
            pulse(
                [&](NativeGameTimingAccess& owner)
                {
                    Check(owner.RemoveWorldEntity(removal.kind, 0) != ClockResult::Ok && runtime.worldRemovals.Pending() && runtime.worldRemovals.Uncertain() &&
                              ClockWorldRemoveOwnerTest::removes == 1 && removal.singles.flags[1] == 0x83 && removal.singles.flags[3] == 0x89,
                          "entered whole-original failure retains actual native two-node unlink prefix as unknown");
                },
                false);
            const auto nodes = raw(removal.singles.Row(0), 5 * removal.singles.stride);
            const auto flags = raw(reinterpret_cast<std::uintptr_t>(removal.singles.flags), static_cast<std::size_t>(removal.singles.capacity));
            const auto originalCount = ClockWorldRemoveOwnerTest::removes, boundCount = ClockWorldRemoveOwnerTest::bounds;
            Check(!service.End(generation), "unknown WorldRemove domain refuses actual owner End");
            engine.Pulse(service, generation, true);
            NativeClockService  next;
            ClockServiceStateV1 denied;
            ClockNumericAssociationIssuedTest::admittedImage = fixture.image;
            const bool beganAgain = next.BeginGameTiming(request, denied, nullptr, nullptr);
            ClockNumericAssociationIssuedTest::admittedImage = 0;
            Check(!service.End(generation) && !beganAgain && runtime.worldRemovals.Pending() && runtime.worldRemovals.Uncertain() &&
                      ClockWorldRemoveOwnerTest::removes == originalCount && ClockWorldRemoveOwnerTest::bounds == boundCount &&
                      raw(removal.singles.Row(0), nodes.size()) == nodes && raw(reinterpret_cast<std::uintptr_t>(removal.singles.flags), flags.size()) == flags,
                  "stopped cleanup and new Begin retain unknown CPU intent without replay, rollback, node writes or renewed native borrows");
            engine.Stop();
            ClockWorldRemoveOwnerTest::current = nullptr;
            ClockReaderTestHost::image = previousImage;
            std::puts("Production world remove unknown completed");
            return;
        }
        Check(!ClockWorldRemoveOwnerTest::invalidInvocation, "every called native original was the explicit correctly typed final body double");
        if (!service.End(generation))
            engine.Pulse(service, generation, true);
        Check(service.End(generation) && runtime.worldRemovals.CanDiscard(), "healthy ACK or untouched preentry refusal allows actual owner End");
        NativeClockService  next;
        ClockServiceStateV1 nextState;
        ClockNumericAssociationIssuedTest::admittedImage = fixture.image;
        const bool nextBegan = next.BeginGameTiming(request, nextState, &WorldGroundOwnerFixture::Callback::Invoke, &callback);
        ClockNumericAssociationIssuedTest::admittedImage = 0;
        Check(nextBegan && nextState.generation != generation, "actual acknowledged/refused WorldRemove lifetime permits a distinct owner generation");
        engine.Pulse(next, nextState.generation);
        if (!next.End(nextState.generation))
            engine.Pulse(next, nextState.generation, true);
        Check(next.End(nextState.generation), "next no-effect WorldRemove generation retires normally");
        engine.Stop();
        ClockWorldRemoveOwnerTest::current = nullptr;
        ClockReaderTestHost::image = previousImage;
        writes.Restore();
        std::puts(refused ? "Production world remove refused completed" : "Production world remove healthy completed");
    }
}
