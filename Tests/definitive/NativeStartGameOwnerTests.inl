/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual synchronous StartGame owner without executing GTA instructions
 *****************************************************************************/

// Included inside the existing production owner fixture's anonymous namespace.
// All graph objects live only in one handler invocation. The actual bounded
// readers, GUObjectArray membership fences and native frame admission run.
class StartGameGraph final
{
public:
    using Object = std::array<unsigned char, 0x40>;
    ClockImageFixture& fixture;
    alignas(std::uintptr_t) Object meta{}, uiBase{}, gameBase{}, levelBase{}, controllerBase{}, viewportBase{}, managerBase{};
    alignas(std::uintptr_t) Object uiClass{}, gameClass{}, levelClass{}, controllerClass{}, viewportClass{}, managerClass{}, worldClass{}, engineClass{};
    alignas(std::uintptr_t) Object controller{}, manager{};
    alignas(std::uintptr_t) std::array<unsigned char, 0x2c8> game{};
    alignas(std::uintptr_t) std::array<unsigned char, 0xd8> level{};
    alignas(std::uintptr_t) std::array<unsigned char, 0x1d0> world{};
    alignas(std::uintptr_t) std::array<unsigned char, 0x788> engine{};
    alignas(std::uintptr_t) std::array<unsigned char, 0x368> viewport{};
    std::array<std::array<std::uintptr_t, 3>, 6>                               chains{};
    std::array<unsigned char, NativeStartupWorldProfile::objectItemBytes * 32> items{};
    std::array<std::uintptr_t, 1>                                              chunks{};
    std::array<unsigned char, 8>                                               weak{};
    std::size_t                                                                count = 0, managerIndex = 0, uiIndex = 0;

    template <typename T>
    static std::uintptr_t Address(const T& value) noexcept
    {
        return reinterpret_cast<std::uintptr_t>(value.data());
    }
    template <typename T, typename V>
    static void Store(T& object, std::size_t offset, const V& value) noexcept
    {
        std::memcpy(reinterpret_cast<unsigned char*>(object.data()) + offset, &value, sizeof(value));
    }
    template <typename T>
    std::size_t Register(T& object, std::uintptr_t type, std::uint32_t serial = 0)
    {
        const auto index = count++;
        Check(index < 32, "the callback-local startup graph fits its explicit owned registry evidence budget");
        Store(object, 0, fixture.image + NativeStartupOwnerProfile::gameModeVtableRva);
        Store(object, NativeStartupWorldProfile::objectIndexOffset, static_cast<std::int32_t>(index));
        Store(object, NativeStartupWorldProfile::objectClassOffset, type);
        Store(items, index * NativeStartupWorldProfile::objectItemBytes, Address(object));
        Store(items, index * NativeStartupWorldProfile::objectItemBytes + NativeStartupProvidersProfile::itemSerialOffset, serial);
        return index;
    }
    void Type(Object& base, Object& derived, std::size_t index)
    {
        Store(base, NativeUiClassMapProfile::classChainIndexOffset, std::int32_t{1});
        chains[index][1] = Address(base) + NativeUiClassMapProfile::classChainOffset;
        Store(derived, NativeUiClassMapProfile::classChainOffset, Address(chains[index]));
        Store(derived, NativeUiClassMapProfile::classChainIndexOffset, std::int32_t{2});
        Store(base, NativeStartupWorldProfile::objectFlagsOffset, std::uint32_t{0x10});
    }
    explicit StartGameGraph(ClockImageFixture& owned) : fixture(owned)
    {
        namespace W = NativeStartupWorldProfile;
        namespace O = NativeStartupOwnerProfile;
        namespace P = NativeStartupProvidersProfile;
        namespace U = NativeUiClassMapProfile;
        Register(level, Address(levelClass));
        Register(controller, Address(controllerClass), 17);
        Register(meta, Address(meta));
        Register(game, Address(gameClass));
        Register(world, Address(worldClass));
        Register(viewport, Address(viewportClass));
        managerIndex = Register(manager, Address(managerClass));
        Register(engine, Address(engineClass));
        for (auto* object : {&uiBase, &gameBase, &levelBase, &controllerBase, &viewportBase, &managerBase, &uiClass, &gameClass, &levelClass, &controllerClass,
                             &viewportClass, &managerClass, &worldClass, &engineClass})
        {
            const auto index = Register(*object, Address(meta));
            if (object == &uiClass)
                uiIndex = index;
        }
        Type(uiBase, uiClass, 0);
        Type(gameBase, gameClass, 1);
        Type(levelBase, levelClass, 2);
        Type(controllerBase, controllerClass, 3);
        Type(viewportBase, viewportClass, 4);
        Type(managerBase, managerClass, 5);
        Store(game, O::objectOuterOffset, Address(level));
        Store(game, O::gameModeInterfaceOffset, Address(fixture.frameOwner));
        Store(level, W::levelWorldOffset, Address(world));
        Store(manager, 0, fixture.image + P::managerVtableRva);
        Store(viewport, P::viewportManagerOffset, Address(manager));
        Store(engine, P::engineViewportOffset, Address(viewport));
        Store(world, P::worldControllerDataOffset, Address(weak));
        Store(world, P::worldControllerCountOffset, std::int32_t{1});
        Store(weak, P::weakIndexOffset, std::int32_t{1});
        Store(weak, P::weakSerialOffset, std::uint32_t{17});
        Store(fixture.frameOwner, O::objectOuterOffset, Address(game));
        Store(fixture.uiEntries, U::valueOffset, Address(uiClass));
        Store(fixture.uiEntries, U::entryBytes + U::valueOffset, Address(uiClass));
        chunks[0] = Address(items);
        fixture.Put(W::objectChunksRva, Address(chunks));
        fixture.Put(W::objectCountRva, static_cast<std::int32_t>(count));
        fixture.Put(U::uiTransitionClassCacheRva, Address(uiBase));
        fixture.Put(O::gameModeClassCacheRva, Address(gameBase));
        fixture.Put(W::levelClassCacheRva, Address(levelBase));
        fixture.Put(P::playerControllerClassCacheRva, Address(controllerBase));
        fixture.Put(P::viewportClassCacheRva, Address(viewportBase));
        fixture.Put(P::managerClassCacheRva, Address(managerBase));
        fixture.Put(P::engineRva, Address(engine));
    }
    ~StartGameGraph()
    {
        // No image cache or natural receiver outlives these stack objects.
        for (const auto rva : {NativeStartupWorldProfile::objectChunksRva, NativeUiClassMapProfile::uiTransitionClassCacheRva,
                               NativeStartupOwnerProfile::gameModeClassCacheRva, NativeStartupWorldProfile::levelClassCacheRva,
                               NativeStartupProvidersProfile::playerControllerClassCacheRva, NativeStartupProvidersProfile::viewportClassCacheRva,
                               NativeStartupProvidersProfile::managerClassCacheRva, NativeStartupProvidersProfile::engineRva})
            fixture.Put(rva, std::uintptr_t{0});
        fixture.Put(NativeStartupWorldProfile::objectCountRva, std::int32_t{0});
        Store(fixture.frameOwner, NativeStartupOwnerProfile::objectOuterOffset, std::uintptr_t{0});
        Store(fixture.uiEntries, NativeUiClassMapProfile::valueOffset, std::uintptr_t{0});
        Store(fixture.uiEntries, NativeUiClassMapProfile::entryBytes + NativeUiClassMapProfile::valueOffset, std::uintptr_t{0});
    }
    void Fault(unsigned kind)
    {
        if (kind == 1)
            Store(viewport, NativeStartupProvidersProfile::viewportManagerOffset, std::uintptr_t{0});
        if (kind == 2)
            Store(items, uiIndex * NativeStartupWorldProfile::objectItemBytes + NativeStartupProvidersProfile::itemFlagsOffset, std::uint32_t{0x30000000});
        if (kind == 3)
            Store(manager, NativeStartupWorldProfile::objectClassOffset, Address(uiClass));
        if (kind == 4)
            Store(weak, NativeStartupProvidersProfile::weakSerialOffset, std::uint32_t{18});
    }
};

struct StartGameOwnerContext
{
    ApplicationFrameContext              observer;
    ClockImageFixture*                   fixture = nullptr;
    unsigned                             calls = 0, refusal = 0, graphFault = 0, operation = 0;
    NativeStartupCompletionAccess::Value progress{0xa5, 0x5a, 0xa5};
    void*                                expectedReceiver = nullptr;
    ClockResult                          result = ClockResult::NotInitialized, duplicate = ClockResult::NotInitialized;
    bool                                 tryDuplicate = false;
};

void StartGameOwnerHandler(NativeGameTimingAccess& access, const NativeApplicationFrame& frame, void* opaque)
{
    auto& context = *static_cast<StartGameOwnerContext*>(opaque);
    ++context.calls;
    Check(callbackDepth == 1 && runtime.inFlight.load() == 1 && frame.generation == runtime.generation.load() &&
              frameCallbackReceiver == (context.expectedReceiver ? context.expectedReceiver : context.fixture->frameOwner.data()) && !timingTimerScope &&
              !runtime.timerReady.load() && !runtime.captured.load(),
          "the actual typed startup handler borrows the original normal frame without CJ, timer readiness or a clock snapshot");
    const auto generation = frameCallbackGeneration;
    const auto admission = runtime.admissionGeneration.load();
    const auto refreshed = runtime.refreshed.load();
    if (context.refusal == 1)
        frameCallbackGeneration = generation + 1;
    if (context.refusal == 2)
        runtime.admissionGeneration.store(admission + 1);
    if (context.refusal == 3)
        ++callbackDepth;
    if (context.refusal == 4)
        runtime.refreshed.store(GetTickCount64() - ClockServiceLeaseMsV1 - 1);
    if (context.refusal == 5)
        context.fixture->Put(NativeStartGameProfile::ThreadIdRva, GetCurrentThreadId() + DWORD{1});
    if (context.refusal == 6)
        context.fixture->Put(NativeBorrowedGcProfile::markerRva, std::uint32_t{1});
    if (context.operation == 1)
        context.result = access.ReadStartupProgress(context.progress);
    else
    {
        StartGameGraph graph(*context.fixture);
        graph.Fault(context.graphFault);
        context.result = access.StartGame();
        if (context.tryDuplicate)
            context.duplicate = access.StartGame();
    }
    context.fixture->Put(NativeStartGameProfile::ThreadIdRva, GetCurrentThreadId());
    context.fixture->Put(NativeBorrowedGcProfile::markerRva, std::uint32_t{0});
    runtime.refreshed.store(refreshed);
    if (context.refusal == 3)
        --callbackDepth;
    runtime.admissionGeneration.store(admission);
    frameCallbackGeneration = generation;
}

void NativeStartGameOwnedFrame(ClockImageFixture& fixture)
{
    namespace P = NativeStartGameProfile;
    fixture.AdmitUiClassMapPins();
    fixture.AdmitStartupPins();
    fixture.SeedUiClassMap();
    std::array<std::array<unsigned char, 94>, NativeGameStateProfile::pins.size()> statePins{};
    for (std::size_t index = 0; index < statePins.size(); ++index)
        statePins[index] = fixture.Get<std::array<unsigned char, 94>>(NativeGameStateProfile::pins[index].rva);
    fixture.AdmitSystemStatePins();
    const auto admit = [&](std::uintptr_t rva, const void* bytes, std::size_t size, DWORD protection)
    {
        DWORD previous = 0, ignored = 0;
        Check(::VirtualProtect(reinterpret_cast<void*>(fixture.image + rva), size, PAGE_READWRITE, &previous) != FALSE,
              "populate only bounded StartGame/GC/membership proof fields in the owned image");
        std::memcpy(reinterpret_cast<void*>(fixture.image + rva), bytes, size);
        Check(::VirtualProtect(reinterpret_cast<void*>(fixture.image + rva), size, protection, &ignored) != FALSE &&
                  ::FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(fixture.image + rva), size),
              "restore independent native startup code RX, pointer/name RO and protocol-cell RW protection");
    };
    for (const auto& pin : P::Code)
        admit(pin.rva, pin.bytes.data(), pin.size, PAGE_EXECUTE_READ);
    for (const auto& pin : NativeBorrowedGcProfile::pins)
        admit(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
    for (const auto& pin : NativeUObjectMembershipProfile::pins)
        admit(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
    for (const auto& pin : NativeStartupCompletionProfile::pins)
        admit(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
    for (const auto& pointer : P::Pointers)
    {
        const auto target = fixture.image + pointer.targetRva;
        admit(pointer.slotRva, &target, sizeof(target), PAGE_READONLY);
    }
    admit(P::NameRva, P::Name, sizeof(P::Name), PAGE_READONLY);
    const std::uint32_t zero = 0;
    const unsigned char zeroByte = 0;
    admit(P::RequestRva, &zeroByte, sizeof(zeroByte), PAGE_READWRITE);
    admit(P::SelectionRva, &zero, sizeof(zero), PAGE_READWRITE);
    admit(NativeGameStateProfile::stateRva, &zero, sizeof(zero), PAGE_READWRITE);
    admit(NativeBorrowedGcProfile::markerRva, &zero, sizeof(zero), PAGE_READWRITE);
    const auto thread = GetCurrentThreadId();
    admit(P::ThreadIdRva, &thread, sizeof(thread), PAGE_READWRITE);
    const unsigned char initialized = 1;
    admit(P::ThreadInitializedRva, &initialized, sizeof(initialized), PAGE_READWRITE);
    ClockReaderTestHost::image = ClockStartGameOwnerTest::image = fixture.image;
    ClockStartGameOwnerTest::calls = 0;
    ClockStartGameOwnerTest::result = ClockResult::Ok;
    const auto prepare = [&]
    {
        fixture.Put(NativeGameStateProfile::stateRva, std::uint32_t{7});
        fixture.Put(P::RequestRva, static_cast<unsigned char>(0));
        fixture.Put(P::SelectionRva, std::uint32_t{17});
        fixture.Put(NativeGamePauseProfile::userPauseRva, static_cast<unsigned char>(1));
        StartGameGraph::Store(fixture.frameOwner, NativeStartupCompletionProfile::PumpOffset, static_cast<unsigned char>(0));
    };
    Check(runtime.access.StartGame() == ClockResult::NotInitialized && !ClockStartGameOwnerTest::calls,
          "an unbound producer cannot obtain startup call authority from a complete native profile");
    {
        NativeClockService      observer;
        ApplicationFrameContext context;
        BeginApplicationOwned(observer, fixture, context);
        fixture.invokeFrame(fixture.frameOwner.data(), 0.125f);
        Check(runtime.access.StartGame() == ClockResult::NotInitialized && !runtime.startupMode && !runtime.startupAttempted.load(),
              "the unchanged V1-style observation mode cannot consume a native startup attempt");
        FinishApplicationOwned(observer);
    }
    {
        NativeClockService    service;
        StartGameOwnerContext context;
        context.fixture = &fixture;
        BeginApplicationOwned(service, fixture, context.observer, true, &StartGameOwnerHandler, &context);
        const auto invoke = [&]
        {
            Check(service.Renew(runtime.generation.load()), "renew the current startup owner between separate genuine frames");
            fixture.invokeFrame(fixture.frameOwner.data(), 0.125f);
        };
        prepare();
        HookApplicationFrame(fixture.frameOwner.data(), 0.125f);
        // The earlier frame-owner suite exercises the exact excluded loading
        // origin. Its benign owned return suffix overlaps the independently
        // pinned native pump-wait bytes here, so do not execute that suffix
        // after admitting this separate observation profile.
        Check(!context.calls && !ClockStartGameOwnerTest::calls && !runtime.startupAttempted.load(),
              "an unknown call origin still forwards once but cannot enter the typed startup handler");
        Check(runtime.access.StartGame() != ClockResult::Ok && !ClockStartGameOwnerTest::calls,
              "producer-side startup calls cannot borrow a future admitted native frame");
        context.operation = 1;
        fixture.Put(NativeGameStateProfile::stateRva, std::uint32_t{9});
        invoke();
        Check(context.result == ClockResult::NotInitialized && context.progress.nativeState == 0xa5 && !runtime.startupRequestGeneration.load() &&
                  !context.progress.PumpRetired(),
              "the native retired tuple before any entered published request cannot manufacture same-generation completion history");
        context.operation = 0;
        prepare();
        for (unsigned refusal = 1; refusal <= 6; ++refusal)
        {
            context.refusal = refusal;
            invoke();
            Check(context.result != ClockResult::Ok && !runtime.startupAttempted.load() && !ClockStartGameOwnerTest::calls && !runtime.stopping.load(),
                  "stale generation/admission, nesting, expiry, foreign native GameThread and active scoped GC refuse before claiming entry");
        }
        context.refusal = 0;
        for (unsigned fault = 1; fault <= 4; ++fault)
        {
            context.graphFault = fault;
            invoke();
            Check(context.result == ClockResult::NotInitialized && !runtime.startupAttempted.load() && !ClockStartGameOwnerTest::calls,
                  "missing manager, rejected configured UClass membership, wrong manager type and stale weak-controller serial cannot consume startup");
        }
        context.graphFault = 0;
        context.tryDuplicate = true;
        ClockStartGameOwnerTest::retireGraph = true;
        invoke();
        Check(context.result == ClockResult::Ok && context.duplicate == ClockResult::Busy && runtime.startupAttempted.load() &&
                  runtime.startupReceipt.entered && runtime.startupReceipt.returned && ClockStartGameOwnerTest::calls == 1 &&
                  ClockStartGameOwnerTest::entry == P::EntryRva && ClockStartGameOwnerTest::receiver == fixture.frameOwner.data() &&
                  runtime.startupReceipt.nativeStateBefore == 7 && runtime.startupReceipt.nativeStateAfter == 8 && runtime.startupReceipt.requestAfter == 1 &&
                  !runtime.captured.load() && !runtime.timerHooked.load(),
              "the real post-original owner invokes the exact native SA receiver once, accepts asynchronous native progress and never requires the old graph "
              "after return");
        Check(runtime.startupRequestGeneration.load() == runtime.generation.load() &&
                  runtime.startupRequestReceiver.load() == reinterpret_cast<std::uintptr_t>(fixture.frameOwner.data()),
              "only the observed real entered and published request records this generation's asynchronous status authority");
        context.operation = 1;
        context.tryDuplicate = false;
        const auto pump = [&](unsigned char value) { StartGameGraph::Store(fixture.frameOwner, NativeStartupCompletionProfile::PumpOffset, value); };
        const auto untouched = [&] { return context.progress.nativeState == 0xa5 && context.progress.request == 0x5a && context.progress.pumpActive == 0xa5; };
        invoke();
        Check(context.result == ClockResult::Ok && context.progress.nativeState == 8 && context.progress.request == 1 && context.progress.pumpActive == 1 &&
                  !context.progress.PumpRetired(),
              "the same real startup callback observes queued asynchronous native facts after the previous UI/provider graph retired");
        fixture.Put(NativeGameStateProfile::stateRva, std::uint32_t{9});
        invoke();
        Check(context.result == ClockResult::Ok && !context.progress.PumpRetired(),
              "native state9 alone is distinct from request consumption and pump retirement");
        fixture.Put(P::RequestRva, static_cast<unsigned char>(0));
        invoke();
        Check(context.result == ClockResult::Ok && context.progress.pumpActive == 1 && !context.progress.PumpRetired(),
              "a consumed native request with an active pump still remains asynchronous work");
        pump(0);
        invoke();
        Check(context.result == ClockResult::Ok && context.progress.PumpRetired() && ClockStartGameOwnerTest::calls == 1,
              "only the matching entered-request generation can publish the native state9/request0/pump0 retirement facts without repeating StartGame");
        for (unsigned refusal = 1; refusal <= 6; ++refusal)
        {
            context.refusal = refusal;
            context.progress = {0xa5, 0x5a, 0xa5};
            invoke();
            Check(context.result != ClockResult::Ok && untouched(),
                  "read-only startup progress preserves output for stale generation/admission, nesting, expiry, wrong native thread and scoped GC");
        }
        context.refusal = 0;
        const auto publishedGeneration = runtime.startupRequestGeneration.load();
        runtime.startupRequestGeneration.store(publishedGeneration + 1);
        invoke();
        Check(context.result == ClockResult::NotInitialized && untouched(),
              "an earlier or unrelated generation's entered receipt cannot attribute the current native retired tuple");
        runtime.startupRequestGeneration.store(publishedGeneration);
        {
            auto replacement = fixture.frameOwner;
            StartGameGraph::Store(replacement, NativeStartupCompletionProfile::PumpOffset, static_cast<unsigned char>(0));
            fixture.SetFrameReceiver(replacement.data());
            context.expectedReceiver = replacement.data();
            Check(service.Renew(runtime.generation.load()), "renew before a separately admitted replacement application's frame");
            fixture.invokeFrame(replacement.data(), 0.125f);
            Check(context.result == ClockResult::BindingChanged && untouched() &&
                      runtime.startupRequestReceiver.load() == reinterpret_cast<std::uintptr_t>(fixture.frameOwner.data()),
                  "a fresh normal frame and retired tuple on a replacement SA application cannot inherit the old receiver's request attribution");
            fixture.SetFrameReceiver(fixture.frameOwner.data());
            context.expectedReceiver = nullptr;
        }
        pump(2);
        invoke();
        Check(context.result == ClockResult::BindingChanged && untouched(), "an unproved native pump byte refuses instead of becoming a completion boolean");
        pump(0);
        const auto& completionPin = NativeStartupCompletionProfile::pins[0];
        const auto  changedPin = static_cast<unsigned char>(completionPin.bytes[0] ^ 1);
        fixture.Patch(completionPin.rva, &changedPin, sizeof(changedPin));
        invoke();
        Check(context.result == ClockResult::BindingChanged && untouched(),
              "the actual progress owner requires its independently pinned native pump producer/consumer lifecycle");
        fixture.Patch(completionPin.rva, completionPin.bytes, completionPin.size);
        invoke();
        Check(context.result == ClockResult::Ok && context.progress.PumpRetired(),
              "restoring the owned profile recovers read-only status without replaying a request or writing native state");
        NativeStartupCompletionAccess::Value outside{0xa5, 0x5a, 0xa5};
        Check(runtime.access.ReadStartupProgress(outside) != ClockResult::Ok && outside.nativeState == 0xa5 && outside.request == 0x5a &&
                  outside.pumpActive == 0xa5,
              "producer-side progress cannot reuse a previous frame's native receiver or status lease");
        bool        foreignRefused = false;
        std::thread foreign([&] { foreignRefused = runtime.access.StartGame() != ClockResult::Ok; });
        foreign.join();
        Check(foreignRefused && ClockStartGameOwnerTest::calls == 1, "a foreign producer cannot replay a returned native startup transaction");
        FinishApplicationOwned(service);
        Check(fixture.Get<unsigned char>(P::RequestRva) == 0 && fixture.Get<std::uint32_t>(NativeGameStateProfile::stateRva) == 9 &&
                  !runtime.startupRequestGeneration.load() && !runtime.startupRequestReceiver.load(),
              "End retires generation history and hooks while preserving native pump retirement without a world/player readiness claim");
    }
    for (unsigned failure = 0; failure < 2; ++failure)
    {
        NativeClockService    service;
        StartGameOwnerContext context;
        context.fixture = &fixture;
        ClockStartGameOwnerTest::retireGraph = false;
        ClockStartGameOwnerTest::result = failure ? ClockResult::Ok : ClockResult::OriginalException;
        if (failure)
            ClockStartGameOwnerTest::after = [] { runtime.admissionGeneration.fetch_add(1); };
        BeginApplicationOwned(service, fixture, context.observer, true, &StartGameOwnerHandler, &context);
        prepare();
        const auto calls = ClockStartGameOwnerTest::calls;
        fixture.invokeFrame(fixture.frameOwner.data(), 0.125f);
        Check(context.result == (failure ? ClockResult::OwnerConflict : ClockResult::OriginalException) && runtime.startupReceipt.entered &&
                  runtime.startupAttempted.load() && runtime.stopping.load() && ClockStartGameOwnerTest::calls == calls + 1 &&
                  runtime.access.StartGame() != ClockResult::Ok,
              "an entered exception or failed post-return owner bookend consumes the attempt and stops instead of retrying partial native effects");
        FinishApplicationOwned(service);
        Check(fixture.Get<unsigned char>(P::RequestRva) == 1 && fixture.Get<std::uint32_t>(NativeGameStateProfile::stateRva) == 8 &&
                  ClockStartGameOwnerTest::calls == calls + 1,
              "failed entered startup effects persist through complete hook retirement without retry or rollback");
    }
    ClockStartGameOwnerTest::after = nullptr;
    ClockStartGameOwnerTest::result = ClockResult::Ok;
    ClockStartGameOwnerTest::retireGraph = false;
    ClockReaderTestHost::image = ClockStartGameOwnerTest::image = 0;
    for (std::size_t index = 0; index < statePins.size(); ++index)
        fixture.Patch(NativeGameStateProfile::pins[index].rva, statePins[index].data(), NativeGameStateProfile::pins[index].size);
    std::puts("Production native synchronous StartGame frame owner fixture completed");
    std::puts("Production native asynchronous startup completion owner fixture completed");
}
