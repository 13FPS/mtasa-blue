/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Execute selective native completion through the retained callback
 ****************************************************************************/
// The enclosing fixture already owns the real typed root/world graph. Only
// the native manager, pending records and handle owners are extended here.
{
    namespace PP = NativeStreamingPriorityProfile;
    namespace PD = NativeStreamingPriorityCalls::PriorityDetail;
    for (const auto& pin : PP::Code)
        admit(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
    alignas(std::uintptr_t) std::array<unsigned char, 0x1000>  engine{}, asset{}, assetType{};
    alignas(std::uintptr_t) std::array<unsigned char, 14 * 24> registry{};
    alignas(std::uintptr_t) std::array<PD::Pending, 2>         pending{};
    alignas(std::uintptr_t) std::array<unsigned char, 0x200>   handles{};
    alignas(std::uintptr_t) std::array<unsigned char, 0x40>    controls{};
    alignas(std::uintptr_t) std::array<unsigned char, 16>      loader{};
    const auto                                                 address = [](const auto& value) { return reinterpret_cast<std::uintptr_t>(value.data()); };
    const auto engineAddress = address(engine), assetAddress = address(asset), assetTypeAddress = address(assetType), loaderAddress = address(loader);
    for (const auto rva : {PP::EngineRva, PP::AssetClassRva, PP::LoaderRva})
    {
        const auto previous = fixture.Get<std::uintptr_t>(rva);
        admit(rva, &previous, sizeof(previous), PAGE_READWRITE);
    }
    const auto callback = fixture.image + PP::CallbackRva, destroy = fixture.image + PP::ControlDestroyRva;
    const auto release = fixture.image + PP::ControlDeleteRva, wait = fixture.image + PP::WaitRva;
    admit(PP::CallbackVtableRva + 0x10, &callback, sizeof(callback), PAGE_READONLY);
    admit(PP::ControlVtableRva, &destroy, sizeof(destroy), PAGE_READONLY);
    admit(PP::ControlVtableRva + 8, &release, sizeof(release), PAGE_READONLY);
    admit(PP::ControlVtableRva + 0x30, &wait, sizeof(wait), PAGE_READONLY);
    // Own the entire observed row span so earlier fixtures cannot leave
    // unrelated pending work in a supposedly selective completion test.
    const std::vector<unsigned char> emptyRows(PP::PhysicalRows * PP::RowBytes);
    admit(PP::RowsRva, emptyRows.data(), emptyRows.size(), PAGE_READWRITE);
    const auto registerObject = [&](unsigned char* object, std::int32_t index, std::uintptr_t vtable, std::uintptr_t type)
    {
        store(object, 0, vtable);
        store(object, 8, std::uint32_t{0});
        store(object, 0xc, index);
        store(object, 0x10, type);
        store(object, 0x20, std::uintptr_t{0});
        store(registry.data(), static_cast<std::size_t>(index) * 24, reinterpret_cast<std::uintptr_t>(object));
        store(registry.data(), static_cast<std::size_t>(index) * 24 + 8, std::uint32_t{0});
        store(registry.data(), static_cast<std::size_t>(index) * 24 + 0x10, static_cast<std::uint32_t>(200 + index));
    };
    const auto resetPriority = [&]
    {
        graph.Reset();
        engine.fill(0);
        asset.fill(0);
        assetType.fill(0);
        registry.fill(0);
        pending = {};
        handles.fill(0);
        controls.fill(0);
        loader.fill(0);
        std::memcpy(registry.data(), graph.items.data(), graph.items.size());
        graph.chunks[0] = address(registry);
        fixture.Put(NativeStartupWorldProfile::objectCountRva, std::int32_t{14});
        registerObject(engine.data(), 11, fixture.image + NativeStartupWorldProfile::ownerVtableRva, graph.Class(0));
        registerObject(asset.data(), 12, fixture.image + PP::AssetVtableRva, assetTypeAddress);
        registerObject(assetType.data(), 13, fixture.image + NativeStartupWorldProfile::ownerVtableRva, graph.Class(0));
        store(engine.data(), PP::EngineAssetOffset, assetAddress);
        fixture.Put(PP::EngineRva, engineAddress);
        fixture.Put(PP::AssetClassRva, assetTypeAddress);
        store(loader.data(), 0, fixture.image + PP::ControlVtableRva);
        fixture.Put(PP::LoaderRva, loaderAddress);
        store(owner.data(), PP::CallbackOffset, fixture.image + PP::CallbackVtableRva);
        store(owner.data(), 0x78, static_cast<unsigned char>(0));
        store(owner.data(), 0x80, PD::Array{address(pending), 2, 2});
        for (std::size_t i = 0; i != pending.size(); ++i)
        {
            const auto handleAddress = address(handles) + i * 0x100, controlAddress = address(controls) + i * 0x20;
            auto&      record = pending[i];
            record.boundObject = root;
            record.boundInterface = root + PP::CallbackOffset;
            record.handle = {handleAddress, controlAddress};
            record.paths = {handleAddress + 0xa0, 1, 1};
            record.model = static_cast<std::uint32_t>(17 + i);
            record.originalPriority = i ? 65 : 50;
            store(handles.data() + i * 0x100, 0, handleAddress);
            store(handles.data() + i * 0x100, 8, controlAddress);
            store(handles.data() + i * 0x100, 0x98, assetAddress + PP::AssetStreamableOffset);
            store(controls.data() + i * 0x20, 0, fixture.image + PP::ControlVtableRva);
            store(controls.data() + i * 0x20, 8, std::int32_t{1});
            store(controls.data() + i * 0x20, 12, std::int32_t{1});
            store(controls.data() + i * 0x20, 16, handleAddress);
        }
        fixture.Put(PP::RowsRva + 17 * PP::RowBytes + 12, std::array<unsigned char, 2>{0x10, 3});
        fixture.Put(PP::RowsRva + 18 * PP::RowBytes + 12, std::array<unsigned char, 2>{0, 3});
    };
    ClockStreamingPriorityOwnerTest::image = fixture.image;
    ClockStreamingPriorityOwnerTest::root = root;
    ClockStreamingPriorityOwnerTest::reenterTrue = +[]() noexcept { return runtime.access.LoadStreamingModels(true); };
    ClockStreamingPriorityOwnerTest::reenterFalse = +[]() noexcept { return runtime.access.LoadStreamingModels(false); };
    Check(absent.LoadStreamingModels(true) == ClockResult::NotInitialized, "older owners cannot acknowledge selective completion");
    context.operation = ModelOwnershipOperation::LoadAll;
    context.onlyPriority = true;
    for (unsigned scenario = 0; scenario != 6; ++scenario)
    {
        resetPriority();
        ClockStreamingPriorityOwnerTest::scenario = scenario;
        const auto waits = ClockStreamingPriorityOwnerTest::waits, drains = ClockStreamingPriorityOwnerTest::drains,
                   full = ClockStreamingLoadAllOwnerTest::calls;
        NativeClockService service;
        BeginOwned(service, fixture, false, true, true, &ModelOwnershipOwnerHandler, &context);
        Check(runtime.access.LoadStreamingModels(true) != ClockResult::Ok && ClockStreamingPriorityOwnerTest::waits == waits,
              "retained handle ownership does not grant callback authority outside its actual invocation");
        if (!scenario)
        {
            store(controls.data(), 8, std::int32_t{0});
            fixture.invokeTimer();
            Check(context.result != ClockResult::Ok && !runtime.stopping.load() && ClockStreamingPriorityOwnerTest::waits == waits &&
                      ClockStreamingPriorityOwnerTest::drains == drains,
                  "invalid native strong owner refuses before waiting and leaves retained callback usable");
            store(controls.data(), 8, std::int32_t{1});
            Check(service.Renew(runtime.generation.load()), "renew after clean priority ownership refusal");
        }
        fixture.invokeTimer();
        const bool failed = scenario == 1 || scenario == 2 || scenario == 4;
        Check(ClockStreamingPriorityOwnerTest::waits == waits + 1 &&
                  ClockStreamingPriorityOwnerTest::drains == drains + (scenario == 1 || scenario == 2 ? 0 : 1) &&
                  ClockStreamingPriorityOwnerTest::waitEntry == PP::WaitRva && ClockStreamingPriorityOwnerTest::handle == address(handles) &&
                  ClockStreamingLoadAllOwnerTest::calls == full,
              "actual priority callback selects current flag10 despite stored priority50 and never falls back to global flush");
        Check(failed ? context.result != ClockResult::Ok && runtime.stopping.load() : context.result == ClockResult::Ok && !runtime.stopping.load(),
              "native wait/drain exceptions and root loss retire exact current owner while successful return retains it");
        if (scenario == 3)
            Check(ClockStreamingPriorityOwnerTest::nestedTrue == ClockResult::Busy && ClockStreamingPriorityOwnerTest::nestedFalse == ClockResult::Busy,
                  "shared native completion gate rejects both recursive priority and all-request admission without consuming callback depth");
        if (scenario == 5)
            Check(fixture.Get<unsigned char>(PP::RowsRva + 17 * PP::RowBytes + 13) == 3 && pending[0].handle.object == address(handles),
                  "one returned wait with the same pending handle does not repeat it or invent loaded state1");
        Check(fixture.Get<unsigned char>(PP::RowsRva + 18 * PP::RowBytes + 13) == 3,
              "unselected request is not sent through native Wait or forced loaded by the adapter");
        const auto admittedWaits = ClockStreamingPriorityOwnerTest::waits, admittedDrains = ClockStreamingPriorityOwnerTest::drains;
        retire(service);
        Check(ClockStreamingPriorityOwnerTest::waits == admittedWaits && ClockStreamingPriorityOwnerTest::drains == admittedDrains &&
                  ClockStreamingLoadAllOwnerTest::calls == full,
              "retirement preserves native effects without replaying wait/drain or substituting destructive global loading");
    }
    fixture.Put(Q::RootRva, root);
    store(owner.data(), 0x78, static_cast<unsigned char>(0));
    store(owner.data(), 0x80, PD::Array{});
    fixture.Put(PP::RowsRva + 17 * PP::RowBytes + 12, std::array<unsigned char, 2>{0, 1});
    fixture.Put(PP::RowsRva + 18 * PP::RowBytes + 12, std::array<unsigned char, 2>{0, 1});
    ClockStreamingPriorityOwnerTest::image = ClockStreamingPriorityOwnerTest::root = 0;
    ClockStreamingPriorityOwnerTest::scenario = 0;
    ClockStreamingPriorityOwnerTest::reenterTrue = ClockStreamingPriorityOwnerTest::reenterFalse = nullptr;
    std::puts("Production native streaming priority callback owner fixture completed");
}
