/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Original DE JumpCut with ordered behind-byte and retained prefixes
 ****************************************************************************/
namespace CameraJumpCutOwnerFixture
{
    namespace P = NativeCameraDataProfile;
    namespace R = NativePedReferenceProfile;
    namespace C = NativeCameraRestoreProfile;
    struct Node
    {
        std::uintptr_t next = 0, cell = 0;
    };
    static_assert(sizeof(Node) == R::ReferenceBytes);
    void Run(ClockImageFixture& fixture, unsigned scenario)
    {
        fixture.AdmitApplicationPins();
        WorldGroundOwnerFixture::Writes writes{fixture, {}};
        const auto                      apply = [&](std::uintptr_t rva, const void* data, std::size_t bytes, DWORD protection)
        { WorldGroundOwnerFixture::Writes::Apply(&writes, rva, data, bytes, protection); };
        const auto admit = [&](const auto& pins)
        {
            for (const auto& pin : pins)
                apply(pin.rva, pin.bytes, pin.size, pin.code ? PAGE_EXECUTE_READ : PAGE_READONLY);
        };
        for (const auto& pin : NativeStreamingLoadAllProfile::Code)
            apply(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        admit(NativeStartupOwnerProfile::pins);
        admit(NativeStartupWorldProfile::pins);
        admit(NativeGameStateProfile::pins);
        for (const auto& pin : NativeUObjectMembershipProfile::pins)
            apply(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : NativeBorrowedGcProfile::pins)
            apply(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : NativeStartGameProfile::Code)
            if (pin.rva == 0xab879c || pin.rva == 0xab92f0 || pin.rva == 0x1af4ef0)
                apply(pin.rva, pin.bytes.data(), pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : NativePedPoolProfile::Code)
            apply(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : R::Code)
            apply(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& slot : R::Slots)
            writes.Global(slot.rva, fixture.image + slot.targetRva, PAGE_READONLY);
        for (const auto& pin : NativePlayerInfoProfile::Code)
            apply(pin.rva, pin.bytes.data(), pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : NativePlayerInfoExtraProfile::Code)
            apply(pin.rva, pin.bytes.data(), pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : P::Pins)
            apply(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& group : P::Groups)
        {
            writes.Global(group.slotRva, fixture.image + group.entryRva, PAGE_READONLY);
            writes.Global(group.tableSlotRva, static_cast<std::uint32_t>(group.branchRva), PAGE_EXECUTE_READ);
        }
        writes.Global(P::CrtPointerRva, fixture.image + P::CrtWrapperRva, PAGE_READONLY);
        apply(C::EntryRva, C::Restore.data(), C::Restore.size(), PAGE_EXECUTE_READ);
        for (const auto& pin : C::JumpCutPins)
            apply(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        admit(NativeStartupProvidersProfile::pins);
        fixture.Seed();
        NativeStreamingLoadAllFixture::Graph graph(fixture.image, reinterpret_cast<std::uintptr_t>(fixture.frameOwner.data()),
                                                   &WorldGroundOwnerFixture::Writes::Apply, &writes);
        fixture.SetFrameReceiver(fixture.frameOwner.data());
        writes.Global(NativeGameStateProfile::stateRva, std::uint32_t{9});
        writes.Global(NativeStartGameProfile::ThreadInitializedRva, static_cast<unsigned char>(1));
        writes.Global(NativeStartGameProfile::ThreadIdRva, GetCurrentThreadId());
        writes.Global(NativeBorrowedGcProfile::markerRva, LONG{0});
        ClockTeleportPool peds(fixture, NativePedPoolProfile::RootRva, NativePedPoolProfile::NativeCapacity, NativePedPoolProfile::ObjectStride);
        peds.Publish();
        peds.flags[0] = 1;
        const auto ped = reinterpret_cast<std::uintptr_t>(peds.objects);
        const auto putPed = [&](std::size_t offset, const auto& value) { std::memcpy(peds.objects + offset, &value, sizeof(value)); };
        putPed(0, fixture.image + NativePlayerInfoProfile::PlayerVtableRva);
        putPed(0x5f0, fixture.image + NativePlayerInfoProfile::BankRva + 8);
        putPed(0x7e0, std::int32_t{0});
        putPed(0x6a, static_cast<unsigned char>(3));
        putPed(0x634, std::uint32_t{0});
        writes.Global(NativePlayerInfoProfile::BankRva, ped);
        writes.Global(NativePlayerInfoProfile::PlayerIndexRva, static_cast<unsigned char>(0));
        writes.Global(NativePlayerInfoProfile::PlayerVtableRva + 0x50, fixture.image + NativePlayerInfoProfile::PlayerControlRva, PAGE_READONLY);
        // The reference profile supplies the exact first virtual target;
        // preserve it while adding the independently qualified control slot.
        const auto oldReaderImage = ClockReaderTestHost::image;
        ClockReaderTestHost::image = ClockCameraRestoreOwnerTest::image = ClockCameraJumpCutOwnerTest::image = fixture.image;
        ClockCameraRestoreOwnerTest::calls = 0;
        ClockCameraRestoreOwnerTest::scenario = 0;
        const auto nodeAddress = [&](std::size_t index) { return fixture.image + R::ReferencesRva + index * sizeof(Node); };
        const auto seed = [&](std::uint32_t mode, bool primary, bool secondary)
        {
            std::array<unsigned char, C::CameraBytes> camera;
            camera.fill(0x5a);
            const auto put = [&](std::size_t offset, const auto& value) { std::memcpy(camera.data() + offset, &value, sizeof(value)); };
            put(0, fixture.image + P::CameraVtableRva);
            put(0x33, std::uint8_t{0x7f});
            put(0x22, std::uint8_t{0});
            put(0x47, std::uint8_t{0});
            put(0x7f8, primary ? ped : std::uintptr_t{0});
            put(0x800, secondary ? ped : std::uintptr_t{0});
            apply(C::CameraRva, camera.data(), camera.size(), PAGE_READWRITE);
            std::array<Node, R::ReferenceCount> bank{};
            std::size_t                         used = 0;
            if (primary)
                bank[used++].cell = fixture.image + C::CameraRva + 0x7f8;
            if (secondary)
                bank[used++].cell = fixture.image + C::CameraRva + 0x800;
            for (std::size_t i = 0; i != used; ++i)
                bank[i].next = i + 1 < used ? nodeAddress(i + 1) : 0;
            for (std::size_t i = used; i != bank.size(); ++i)
                bank[i].next = i + 1 < bank.size() ? nodeAddress(i + 1) : 0;
            apply(R::ReferencesRva, bank.data(), sizeof(bank), PAGE_READWRITE);
            writes.Global(R::FreeHeadRva, nodeAddress(used));
            putPed(R::EntityHeadOffset, used ? nodeAddress(0) : std::uintptr_t{0});
            putPed(0x5fc, mode);
        };
        seed(0, true, true);
        // Extend the same typed world registry, rather than supplying a fake
        // predicate. The whole native viewport/class lookup and AFC130 then
        // consume these current UObject rows and take their real false branch.
        namespace T = NativeStartupProvidersProfile;
        alignas(std::uintptr_t) std::array<unsigned char, 0x788>               engine{};
        alignas(std::uintptr_t) std::array<unsigned char, 0x368>               viewport{};
        alignas(std::uintptr_t) std::array<unsigned char, 0x290>               member{};
        alignas(std::uintptr_t) std::array<unsigned char, 0x40>                manager{}, controller{};
        alignas(std::uintptr_t) std::array<std::array<unsigned char, 0x40>, 6> classes{};
        std::array<std::array<std::uintptr_t, 3>, 3>                           chains{};
        std::array<unsigned char, 22 * 24>                                     items{};
        std::array<std::uintptr_t, 1>                                          chunks{};
        std::array<std::uint32_t, 2>                                           weak{15, 115};
        std::memcpy(items.data(), graph.items.data(), graph.items.size());
        const auto address = [](const auto& object) { return reinterpret_cast<std::uintptr_t>(object.data()); };
        const auto put = [](auto& object, std::size_t offset, const auto& value) { std::memcpy(object.data() + offset, &value, sizeof(value)); };
        const auto registerObject = [&](auto& object, std::size_t index, std::uintptr_t type)
        {
            put(object, 0, fixture.image + NativeStartupWorldProfile::ownerVtableRva);
            put(object, 0xc, static_cast<std::int32_t>(index));
            put(object, 0x10, type);
            put(items, index * 24, address(object));
            put(items, index * 24 + 0x10, static_cast<std::uint32_t>(100 + index));
        };
        registerObject(engine, 11, graph.Class(0));
        registerObject(viewport, 12, address(classes[1]));
        registerObject(manager, 13, address(classes[3]));
        registerObject(member, 14, graph.Class(0));
        registerObject(controller, 15, address(classes[5]));
        for (std::size_t i = 0; i != classes.size(); ++i)
            registerObject(classes[i], 16 + i, graph.Class(0));
        for (std::size_t i = 0; i != chains.size(); ++i)
        {
            auto& base = classes[2 * i];
            auto& derived = classes[2 * i + 1];
            chains[i] = {0, address(base) + 0x30, address(derived) + 0x30};
            put(base, 8, std::uint32_t{0x10});
            put(base, 0x38, std::int32_t{1});
            put(derived, 0x30, address(chains[i]));
            put(derived, 0x38, std::int32_t{2});
        }
        put(engine, T::engineViewportOffset, address(viewport));
        put(viewport, T::viewportManagerOffset, address(manager));
        put(manager, 0, fixture.image + T::managerVtableRva);
        put(manager, 0x28, address(member));
        NativeStreamingLoadAllFixture::Graph::Put(reinterpret_cast<void*>(graph.World()), T::worldControllerDataOffset, address(weak));
        NativeStreamingLoadAllFixture::Graph::Put(reinterpret_cast<void*>(graph.World()), T::worldControllerCountOffset, std::int32_t{1});
        chunks[0] = address(items);
        writes.Global(T::objectChunksRva, address(chunks));
        writes.Global(T::objectCountRva, std::int32_t{22});
        writes.Global(T::engineRva, address(engine));
        writes.Global(T::viewportClassCacheRva, address(classes[0]));
        writes.Global(T::managerClassCacheRva, address(classes[2]));
        writes.Global(T::playerControllerClassCacheRva, address(classes[4]));
        ClockCameraJumpCutOwnerTest::member = address(member);
        ClockCameraJumpCutOwnerTest::calls = ClockCameraJumpCutOwnerTest::writes = ClockCameraJumpCutOwnerTest::events = 0;
        ClockCameraJumpCutOwnerTest::scenario = 0;

        CameraBasicsObserver camera(runtime.access);
        CCamera&             sdk = camera;
        ClockResult          result = ClockResult::NotInitialized;
        Check(runtime.access.RestoreCameraWithJumpCut() != ClockResult::Ok && !ClockCameraJumpCutOwnerTest::calls && !ClockCameraJumpCutOwnerTest::writes,
              "an unbound JumpCut facade supplies neither the ordered behind write nor a native call");
        WorldGroundOwnerFixture::Callback callback;
        NativeClockService                service;
        ClockServiceRequestV1             request;
        request.reserved[0] = NativeClockService::GameTimingAfterApplicationFrameV1;
        ClockServiceStateV1 rejected, state;
        Check(!service.BeginGameTiming(request, rejected, &WorldGroundOwnerFixture::Callback::Invoke, &callback) &&
                  rejected.failure == ClockResult::UnsupportedBuild && !rejected.modulePinned,
              "JumpCut preserves public wrong-executable admission instead of manually arming an owner");
        ClockNumericAssociationIssuedTest::admittedImage = fixture.image;
        const bool began = service.BeginGameTiming(request, state, &WorldGroundOwnerFixture::Callback::Invoke, &callback);
        ClockNumericAssociationIssuedTest::admittedImage = 0;
        Check(began && state.bound && state.hooked && state.modulePinned && runtime.frameHooked.load() && !runtime.timerHooked.load(),
              "actual public Begin lends only the natural post-original application frame to JumpCut");
        const auto pulse = [&]
        {
            Check(service.Renew(state.generation), "renew the same actual JumpCut owner generation");
            StaticAssociationInitializationOwnerFixture::ProducerPulse producer(service, state.generation);
            fixture.invokeFrame(fixture.frameOwner.data(), 0.25f);
            producer.Stop();
            Check(!producer.unexpectedFailures.load(), "independent producer preserves the actual JumpCut frame handoff");
        };

        if (!scenario)
        {
            std::thread foreign([&] { result = runtime.access.RestoreCameraWithJumpCut(); });
            foreign.join();
            Check(result != ClockResult::Ok && !runtime.stopping.load() && !ClockCameraJumpCutOwnerTest::calls && !ClockCameraJumpCutOwnerTest::writes,
                  "a foreign or retained caller cannot borrow the JumpCut write phase");
            callback.action = [&](NativeGameTimingAccess& access)
            {
                const auto refusal = [&](const char* why)
                {
                    const auto before = fixture.Get<std::array<unsigned char, C::CameraBytes>>(C::CameraRva);
                    result = access.RestoreCameraWithJumpCut();
                    Check(result != ClockResult::Ok && !ClockCameraJumpCutOwnerTest::calls && !ClockCameraJumpCutOwnerTest::writes &&
                              before == fixture.Get<std::array<unsigned char, C::CameraBytes>>(C::CameraRva) && !runtime.cameraRestore.IsRetained() &&
                              !runtime.stopping.load(),
                          why);
                };
                fixture.Put(C::CameraRva + 0x47, std::uint8_t{1});
                refusal("camera47 must exclude the unqualified two-player JumpCut branch before either effect");
                fixture.Put(C::CameraRva + 0x47, std::uint8_t{0});
                put(member, 0x288, std::uint8_t{1});
                refusal("the real native predicate's first member must be current with byte288zero before the behind prefix");
                put(member, 0x288, std::uint8_t{0});
                put(viewport, T::viewportManagerOffset, std::uintptr_t{0});
                refusal("a missing cached manager refuses instead of entering its allocating native initializer");
                put(viewport, T::viewportManagerOffset, address(manager));
                fixture.Put(T::viewportClassCacheRva, std::uintptr_t{0});
                refusal("the viewport StaticClass must already be cached before any camera effect");
                fixture.Put(T::viewportClassCacheRva, address(classes[0]));
                weak[1] = 116;
                refusal("a stale controller weak serial cannot be replaced by an unrelated current viewport");
                weak[1] = 115;
                put(items, 14 * 24 + 8, std::uint32_t{0x20000000});
                refusal("rejected current widget item flags exclude the source's alternate reflected mutation branch");
                put(items, 14 * 24 + 8, std::uint32_t{0});
                const auto selected = chains[1][1];
                chains[1][1] = address(classes[0]) + 0x30;
                refusal("manager candidate type and selected base-chain entry must agree jointly");
                chains[1][1] = selected;
                const auto serial = frameCallbackSerial;
                --frameCallbackSerial;
                refusal("an older frame serial lends no JumpCut prefix authority");
                frameCallbackSerial = serial;
                fixture.Put(NativeBorrowedGcProfile::markerRva, LONG{1});
                refusal("native GC excludes both JumpCut stages before the first write");
                fixture.Put(NativeBorrowedGcProfile::markerRva, LONG{0});
                const auto original = C::TransitionPredicate[0];
                const auto changed = static_cast<unsigned char>(original ^ 1);
                fixture.Patch(C::TransitionPredicateRva, &changed, 1);
                refusal("the whole genuine transition predicate must remain exact, never a substituted boolean");
                fixture.Patch(C::TransitionPredicateRva, &original, 1);
            };
            pulse();
            Check(NativeStreamingCompletionContext::TryEnter(), "hold the same shared native effect gate before JumpCut");
            callback.action = [&](NativeGameTimingAccess& access) { result = access.RestoreCameraWithJumpCut(); };
            pulse();
            NativeStreamingCompletionContext::Leave();
            Check(result == ClockResult::Busy && !ClockCameraJumpCutOwnerTest::calls && !ClockCameraJumpCutOwnerTest::writes,
                  "JumpCut cannot bypass an existing streaming/camera operation gate");
            std::puts("Production camera JumpCut current owner refusals fixture completed");
            for (const auto mode : {0u, 0x35u, 0x39u, 0x3bu, 0x3du})
                for (const auto behind : {std::uint8_t{0}, std::uint8_t{1}, std::uint8_t{2}})
                {
                    seed(mode, true, true);
                    fixture.Put(C::CameraRva + 0x22, behind);
                    ClockCameraJumpCutOwnerTest::calls = ClockCameraJumpCutOwnerTest::writes = ClockCameraJumpCutOwnerTest::events = 0;
                    callback.action = [&](NativeGameTimingAccess&)
                    {
                        Check(CheckApplicationTimingOwner() == ClockResult::Ok && !timingTimerScope,
                              "actual JumpCut SDK dispatch retains the current post-original frame owner");
                        sdk.RestoreWithJumpCut();
                    };
                    pulse();
                    const auto& receipt = runtime.cameraRestoreReceipt;
                    Check(ClockCameraJumpCutOwnerTest::calls == 1 && ClockCameraJumpCutOwnerTest::writes == (behind == 1 ? 0u : 1u) &&
                              ClockCameraJumpCutOwnerTest::orderVerified && receipt.operation == NativeCameraRestoreCalls::Operation::JumpCut &&
                              receipt.entered && receipt.returned && receipt.behindVerified && receipt.effectsVerified && !receipt.debt &&
                              fixture.Get<std::uint8_t>(C::CameraRva + 0x22) == 1 && fixture.Get<std::uint16_t>(C::CameraRva + 0xaf4) == 2 &&
                              fixture.Get<std::uint16_t>(C::CameraRva + 0x35) == 1 &&
                              fixture.Get<std::uint16_t>(C::CameraRva + 0xaf0) == (mode == 0x35 || mode == 0x39 || mode == 0x3b ? 0x12 : 4) &&
                              fixture.Get<std::uintptr_t>(C::CameraRva + 0x7f8) == ped && fixture.Get<std::uintptr_t>(C::CameraRva + 0x800) == ped &&
                              fixture.Get<std::uint8_t>(C::CameraRva + 0x37) == 0x5a && fixture.Get<std::uint32_t>(C::CameraRva + 0x174) == 0x5a5a5a5a &&
                              fixture.Get<std::uint8_t>(C::CameraRva + 0x33) == 0x7f && member[0x288] == 0 && !runtime.stopping.load(),
                          "actual SDK executes the conditional named behind byte first and whole original JumpCut second with its real cached "
                          "predicate/reference helpers");
                }
            Check(service.End() && !service.IsRetained() && !runtime.cameraRestore.IsRetained() && !runtime.frameHooked.load(),
                  "acknowledged ordered JumpCut retires without a second native call, camera rewind or permanent-cell cleanup");
            writes.Restore();
            ClockReaderTestHost::image = oldReaderImage;
            ClockCameraJumpCutOwnerTest::image = ClockCameraJumpCutOwnerTest::member = 0;
            std::puts("Production camera JumpCut ordered prefix fixture completed");
            std::puts("Production camera JumpCut whole original false provider fixture completed");
            std::puts("Production camera JumpCut actual SDK owner fixture completed");
            return;
        }

        ClockCameraJumpCutOwnerTest::scenario = scenario;
        bool threw = false;
        callback.action = [&](NativeGameTimingAccess&)
        {
            try
            {
                sdk.RestoreWithJumpCut();
            }
            catch (const std::exception&)
            {
                threw = true;
            }
        };
        pulse();
        const auto& receipt = runtime.cameraRestoreReceipt;
        if (scenario == 1)
        {
            Check(threw && receipt.result == ClockResult::OwnerConflict && receipt.behindWriteEntered && receipt.behindWriteReturned &&
                      !receipt.behindAccepted && !receipt.effectsEntered && !receipt.entered && !receipt.debt && !runtime.cameraRestore.IsRetained() &&
                      !runtime.stopping.load() && !ClockCameraJumpCutOwnerTest::calls && fixture.Get<std::uint8_t>(C::CameraRva + 0x22) == 0x7f,
                  "a known rejected behind CAS preserves the foreign byte, skips JumpCut and creates no entered-effect debt");
            Check(service.End() && !service.IsRetained(), "known no-effect CAS conflict retires without repairing another writer's byte");
            writes.Restore();
            ClockReaderTestHost::image = oldReaderImage;
            std::puts("Production camera JumpCut known rejected prefix fixture completed");
            return;
        }
        const bool wholeEntered = scenario >= 5;
        Check(threw && receipt.effectsEntered && receipt.debt && receipt.entered == wholeEntered && runtime.stopping.load() &&
                  runtime.cameraRestore.RecoveryPending() && service.IsRetained() && runtime.frameHooked.load() &&
                  ClockCameraJumpCutOwnerTest::calls == (wholeEntered ? 1u : 0u),
              "an uncertain prefix, changed prefix dependency or unknown whole original retains its exact entered stage and native journal");
        if (scenario >= 3)
            Check(fixture.Get<std::uint8_t>(C::CameraRva + 0x22) == 1 && receipt.behindAccepted,
                  "accepted behind prefix remains in native storage when later JumpCut acknowledgement is lost");
        const auto calls = ClockCameraJumpCutOwnerTest::calls, exchanges = ClockCameraJumpCutOwnerTest::writes;
        Check(runtime.cameraRestore.RestoreWithJumpCut(fixture.image, runtime.playerInfo, &CheckTimingOwner, &CheckApplicationTimingOwner) ==
                  ClockResult::RestoreFailure,
              "retained JumpCut debt cannot accept a second operation or replay an earlier successful prefix");
        ClockCameraJumpCutOwnerTest::scenario = 0;
        Check(!service.End() && service.IsRetained(), "producer End cannot erase accepted or unknown camera prefix effects");
        fixture.invokeFrame(fixture.frameOwner.data(), 0.25f);
        Check(!service.End() && service.IsRetained() && runtime.cameraRestore.RecoveryPending() && ClockCameraJumpCutOwnerTest::calls == calls &&
                  ClockCameraJumpCutOwnerTest::writes == exchanges,
              "stopped native frames retain the exact JumpCut journal without retry, rollback or behind-byte repair");
        const char* names[] = {"",           "",           "behind-before",   "behind-after",  "prefix-drift", "jump-before",
                               "jump-after", "owner-loss", "effect-mismatch", "provider-drift"};
        std::printf("Production camera JumpCut %s retained debt fixture completed\n", names[scenario]);
    }
}
