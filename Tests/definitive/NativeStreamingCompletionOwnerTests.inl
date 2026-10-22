/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Execute native acquisition, resource requests and blocking-all on the real owner
 ****************************************************************************/
// Included inside the existing initialized ownership fixture so this
// batch exercises the retained production callback, not a second owner.
{
    namespace A = NativeModelReferenceTransactionProfile;
    for (const auto& pin : A::Code)
        admit(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
    admit(A::ClassSizesRva, A::ClassSizes.data(), sizeof(A::ClassSizes), PAGE_READONLY);
    alignas(std::uintptr_t) std::array<unsigned char, A::HeapBytes> heap{};
    const auto                                                      heapAddress = reinterpret_cast<std::uintptr_t>(heap.data());
    store(heap.data(), 0, fixture.image + A::HeapVtableRva);
    admit(A::HeapRva, &heapAddress, sizeof(heapAddress), PAGE_READWRITE);
    admit(A::HeapAuthorityRva, &heapAddress, sizeof(heapAddress), PAGE_READWRITE);
    const auto reallocate = fixture.image + A::HeapReallocateRva, quantize = fixture.image + A::HeapQuantizeRva;
    admit(A::HeapVtableRva + 0x20, &reallocate, sizeof(reallocate), PAGE_READONLY);
    admit(A::HeapVtableRva + 0x38, &quantize, sizeof(quantize), PAGE_READONLY);
    std::array<unsigned char, A::LookupBytes> lookup{};
    std::array<std::uint16_t, A::ClassCount>  reverse{};
    std::size_t                               category = 0;
    for (std::size_t i = 0; i != lookup.size(); ++i)
    {
        while (category < A::ClassSizes.size() && A::ClassSizes[category] < i * 16)
            ++category;
        lookup[i] = static_cast<unsigned char>(category);
    }
    for (std::size_t i = 0; i != reverse.size(); ++i)
        reverse[i] = A::ClassSizes[A::ClassSizes.size() - i - 1];
    admit(A::LookupRva, lookup.data(), lookup.size(), PAGE_READWRITE);
    admit(A::ReverseSizesRva, reverse.data(), sizeof(reverse), PAGE_READWRITE);
    ClockModelAcquireOwnerTest::image = fixture.image;
    const auto resetMap = [&]
    {
        referenceRows = {};
        reference.key = 3;
        reference.count = 2;
        reference.next = -1;
        reference.bucket = 0;
        store(map, 8, std::int32_t{1});
        store(map, 0xc, std::int32_t{4});
        store(map, 0x10, std::uint32_t{1});
        store(map, 0x28, std::int32_t{1});
        store(map, 0x2c, std::int32_t{128});
        store(map, 0x30, std::int32_t{-1});
        store(map, 0x34, std::int32_t{0});
        store(map, 0x38, std::int32_t{0});
        store(map, 0x48, std::int32_t{1});
        fixture.Put(P::RootRva, root);
        seed(4, 1);
    };
    // Reference ownership belongs to the selected native model key;
    // only Request/HasLoaded use MTA's paired upgrade dispatch.
    resetMap();
    reference.key = 1000;
    reference.next = 1;
    referenceRows[1].key = 1001;
    referenceRows[1].count = 5;
    referenceRows[1].next = -1;
    referenceRows[1].bucket = 0;
    store(map, 8, std::int32_t{2});
    store(map, 0x28, std::int32_t{2});
    store(map, 0x10, std::uint32_t{3});
    seed(1000, 1);
    seed(1001, 1);
    context.operation = ModelOwnershipOperation::Reference;
    context.index = 1000;
    context.delta = 1;
    {
        const auto         reserves = ClockModelAcquireOwnerTest::reserves, paired = ClockStreamingUpgradeOwnerTest::calls;
        NativeClockService service;
        BeginOwned(service, fixture, false, true, true, &ModelOwnershipOwnerHandler, &context);
        fixture.invokeTimer();
        Check(context.result == ClockResult::Ok && reference.count == 3 && referenceRows[1].count == 5 && ClockModelAcquireOwnerTest::reserves == reserves &&
                  ClockStreamingUpgradeOwnerTest::calls == paired,
              "actual upgrade-model reference increments selected key only and leaves companion counter/request unchanged");
        retire(service);
    }
    context.operation = ModelOwnershipOperation::Reference;
    context.index = 4;
    context.delta = 1;
    for (unsigned scenario = 0; scenario != 3; ++scenario)
    {
        resetMap();
        ClockModelAcquireOwnerTest::scenario = scenario;
        const auto         reserves = ClockModelAcquireOwnerTest::reserves, publishes = ClockModelAcquireOwnerTest::publishes;
        NativeClockService service;
        BeginOwned(service, fixture, false, true, true, &ModelOwnershipOwnerHandler, &context);
        Check(runtime.access.AdjustModelReference(4, 1) != ClockResult::Ok && ClockModelAcquireOwnerTest::reserves == reserves,
              "missing-key insertion cannot borrow a retained owner outside its actual callback");
        fixture.invokeTimer();
        Check(ClockModelAcquireOwnerTest::reserves == reserves + 1 && ClockModelAcquireOwnerTest::publishes == publishes + (scenario == 1 ? 0 : 1),
              "actual owner uses exactly one genuine reserve and at most one publish for an absent model key");
        if (!scenario)
        {
            Check(context.result == ClockResult::Ok && !runtime.stopping.load() && referenceRows[1].key == 4 && referenceRows[1].count == 1 &&
                      referenceRows[1].next == 0 && referenceRows[1].bucket == 0 && reference.count == 2,
                  "native publish plus conditional first count creates only the selected row and preserves existing owners");
            Check(service.Renew(runtime.generation.load()), "renew admitted acquisition owner for existing-key reuse");
            const auto exchanges = ClockModelReferenceOwnerTest::calls;
            fixture.invokeTimer();
            Check(context.result == ClockResult::Ok && referenceRows[1].count == 2 && ClockModelReferenceOwnerTest::calls == exchanges + 1 &&
                      ClockModelAcquireOwnerTest::reserves == reserves + 1 && ClockModelAcquireOwnerTest::publishes == publishes + 1,
                  "subsequent acquisition reuses actual current-key count operation without another allocator transaction");
        }
        else
        {
            Check(context.result != ClockResult::Ok && runtime.stopping.load() && (scenario == 1 || referenceRows[1].count == 1),
                  "reserved exception or published lost-root acknowledgement stops same owner and preserves native effects");
        }
        retire(service);
        Check(ClockModelAcquireOwnerTest::reserves == reserves + 1 && ClockModelAcquireOwnerTest::publishes == publishes + (scenario == 1 ? 0 : 1),
              "acquisition retirement never repeats reserve or publish, including unpublished native reservation");
    }
    resetMap();
    ClockModelAcquireOwnerTest::scenario = 0;
    ClockModelAcquireOwnerTest::image = 0;
    std::puts("Production native model acquisition callback owner fixture completed");
}
{
    namespace X = NativeStreamingResourceProfile;
    for (const auto& pin : X::Pins)
        admit(pin.rva, pin.bytes, pin.size, pin.code ? PAGE_EXECUTE_READ : PAGE_READONLY);
    const auto arrayAddress = fixture.image + L::ArrayRva;
    admit(X::ArrayPointerRva, &arrayAddress, sizeof(arrayAddress), PAGE_READWRITE);
    const std::array<std::uintptr_t, 4> pointerRvas{X::LoadedHeadRva, X::LoadedTailRva, X::RequestHeadRva, X::RequestTailRva};
    for (std::size_t i = 0; i != pointerRvas.size(); ++i)
    {
        const auto address = arrayAddress + (X::ResourceEnd + i) * L::RowStride;
        admit(pointerRvas[i], &address, sizeof(address), PAGE_READWRITE);
    }
    const std::int32_t zeroCount = 0;
    admit(X::RequestCountRva, &zeroCount, sizeof(zeroCount), PAGE_READWRITE);
    admit(X::PriorityCountRva, &zeroCount, sizeof(zeroCount), PAGE_READWRITE);
    const auto resetResource = [&]
    {
        fixture.Put(P::RootRva, root);
        seed(X::ResourceFirst, 0);
        fixture.Put(L::ArrayRva + X::ResourceFirst * L::RowStride + 8, std::array<std::int16_t, 2>{-1, -1});
        for (std::uint32_t pair = 0; pair != 4; pair += 2)
        {
            const auto head = static_cast<std::int16_t>(X::ResourceEnd + pair), tail = static_cast<std::int16_t>(head + 1);
            fixture.Put(L::ArrayRva + static_cast<std::uintptr_t>(head) * L::RowStride + 8, std::array<std::int16_t, 2>{tail, -1});
            fixture.Put(L::ArrayRva + static_cast<std::uintptr_t>(tail) * L::RowStride + 8, std::array<std::int16_t, 2>{-1, head});
        }
    };
    ClockStreamingResourceOwnerTest::image = fixture.image;
    context.operation = ModelOwnershipOperation::ResourceRequest;
    context.index = X::ResourceFirst;
    context.flags = 0x80000016;
    for (unsigned scenario = 0; scenario != 3; ++scenario)
    {
        resetResource();
        ClockStreamingResourceOwnerTest::scenario = scenario;
        const auto         calls = ClockStreamingResourceOwnerTest::calls;
        NativeClockService service;
        BeginOwned(service, fixture, false, true, true, &ModelOwnershipOwnerHandler, &context);
        Check(runtime.access.RequestStreamingModel(X::ResourceFirst, 6) != ClockResult::Ok && ClockStreamingResourceOwnerTest::calls == calls,
              "category resource requests cannot borrow a retained owner outside its current callback");
        if (!scenario)
        {
            // A broken initialized native list refuses before any
            // resource effect, even when root and raw row are valid.
            fixture.Put(L::ArrayRva + (X::ResourceEnd + 1) * L::RowStride + 8, std::array<std::int16_t, 2>{-1, -1});
            fixture.invokeTimer();
            Check(context.result != ClockResult::Ok && !runtime.stopping.load() && ClockStreamingResourceOwnerTest::calls == calls,
                  "resource owner preserves real sentinel/list prerequisites instead of accepting raw-state wiring alone");
            resetResource();
            Check(service.Renew(runtime.generation.load()), "renew resource owner after clean preflight refusal");
        }
        fixture.invokeTimer();
        Check(ClockStreamingResourceOwnerTest::calls == calls + 1 && ClockStreamingResourceOwnerTest::entry == NativeStreamingRequestProfile::RequestRva &&
                  ClockStreamingResourceOwnerTest::index == X::ResourceFirst && ClockStreamingResourceOwnerTest::flags == context.flags &&
                  fixture.Get<unsigned char>(L::ArrayRva + X::ResourceFirst * L::RowStride + L::StateOffset) == 1,
              "actual callback dispatches non-DFF first load through genuine full-DWORD RequestModel once");
        Check(!scenario ? context.result == ClockResult::Ok && !runtime.stopping.load() : context.result != ClockResult::Ok && runtime.stopping.load(),
              "resource entry exception or lost root stops current owner; normal return keeps its callback usable");
        retire(service);
        Check(ClockStreamingResourceOwnerTest::calls == calls + 1 &&
                  fixture.Get<unsigned char>(L::ArrayRva + X::ResourceFirst * L::RowStride + L::StateOffset) == 1,
              "resource cleanup neither retries genuine first load nor rewinds accepted decoder effects");
    }
    fixture.Put(P::RootRva, root);
    ClockStreamingResourceOwnerTest::image = 0;
    ClockStreamingResourceOwnerTest::scenario = 0;
    std::puts("Production native streaming resource callback owner fixture completed");
}
{
    namespace Q = NativeStreamingLoadAllProfile;
    for (const auto& pin : Q::Code)
        admit(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
    for (const auto& pin : NativeStartupOwnerProfile::pins)
        admit(pin.rva, pin.bytes, pin.size, pin.code ? PAGE_EXECUTE_READ : PAGE_READONLY);
    for (const auto& pin : NativeStartupWorldProfile::pins)
        admit(pin.rva, pin.bytes, pin.size, pin.code ? PAGE_EXECUTE_READ : PAGE_READONLY);
    const auto graphWrite = +[](void* opaque, std::uintptr_t rva, const void* bytes, std::size_t size, DWORD protection)
    {
        auto* target = static_cast<ClockImageFixture*>(opaque);
        target->Patch(rva, bytes, size);
        DWORD before = 0;
        Check(::VirtualProtect(reinterpret_cast<void*>(target->image + rva), size, protection, &before) != FALSE,
              "world graph globals retain qualified image protections");
    };
    // Save these exact mutable global slots once before the graph
    // writer publishes its owned classes/registry for this operation.
    for (const auto rva : {NativeStartupOwnerProfile::gameModeClassCacheRva, NativeStartupWorldProfile::levelClassCacheRva, Q::RootWorldSlotRva,
                           NativeStartupWorldProfile::ownerGetWorldSlotRva})
    {
        const auto old = fixture.Get<std::uintptr_t>(rva);
        admit(rva, &old, sizeof(old), rva == Q::RootWorldSlotRva || rva == NativeStartupWorldProfile::ownerGetWorldSlotRva ? PAGE_READONLY : PAGE_READWRITE);
    }
    NativeStreamingLoadAllFixture::Graph graph(fixture.image, root, graphWrite, &fixture);
    ClockStreamingLoadAllOwnerTest::image = fixture.image;
    ClockStreamingLoadAllOwnerTest::world = graph.World();
    ClockStreamingLoadAllOwnerTest::level = graph.Level();
    ClockStreamingLoadAllOwnerTest::reenter = +[]() noexcept { return runtime.access.LoadStreamingModels(false); };
    context.operation = ModelOwnershipOperation::LoadAll;
    for (unsigned scenario = 0; scenario != 4; ++scenario)
    {
        graph.Reset();
        graph.world[Q::WorldModeOffset] = 2;
        ClockStreamingLoadAllOwnerTest::scenario = scenario;
        context.onlyPriority = false;
        const auto         calls = ClockStreamingLoadAllOwnerTest::calls, effects = ClockStreamingLoadAllOwnerTest::effects;
        NativeClockService service;
        BeginOwned(service, fixture, false, true, true, &ModelOwnershipOwnerHandler, &context);
        Check(runtime.access.LoadStreamingModels(false) != ClockResult::Ok && ClockStreamingLoadAllOwnerTest::calls == calls,
              "blocking-all operation requires actual borrowed callback rather than retention alone");
        if (!scenario)
        {
            ClockResult foreign = ClockResult::Ok;
            std::thread other([&] { foreign = runtime.access.LoadStreamingModels(false); });
            other.join();
            Check(foreign != ClockResult::Ok && ClockStreamingLoadAllOwnerTest::calls == calls,
                  "foreign thread cannot inherit current native callback completion authority");
            context.onlyPriority = true;
            fixture.invokeTimer();
            Check(context.result == ClockResult::UnsupportedBuild && !runtime.stopping.load() && ClockStreamingLoadAllOwnerTest::calls == calls,
                  "unqualified priority intent refuses without entering all-request native flush or retiring owner");
            context.onlyPriority = false;
            Check(service.Renew(runtime.generation.load()), "renew after explicit priority refusal");
        }
        fixture.invokeTimer();
        Check(ClockStreamingLoadAllOwnerTest::calls == calls + 1 && ClockStreamingLoadAllOwnerTest::effects == effects + 1 &&
                  ClockStreamingLoadAllOwnerTest::entry == Q::EntryRva && graph.world[Q::WorldModeOffset] == 1,
              "actual callback invokes genuine no-argument native flush once and keeps legitimate mode2-to1 progress");
        if (scenario == 1 || scenario == 2)
            Check(context.result != ClockResult::Ok && runtime.stopping.load(), "entered exception or retired old world stops exact current owner");
        else
        {
            Check(context.result == ClockResult::Ok && !runtime.stopping.load(), "successful blocking-all leaves actual callback depth and owner usable");
            if (scenario == 3)
            {
                Check(ClockStreamingLoadAllOwnerTest::nested == ClockResult::Busy,
                      "native completion callbacks cannot recursively enter the blocking operation");
                ClockStreamingLoadAllOwnerTest::scenario = 0;
                Check(service.Renew(runtime.generation.load()), "reentrant refusal preserved actual owner callback depth");
                fixture.invokeTimer();
                Check(context.result == ClockResult::Ok && ClockStreamingLoadAllOwnerTest::calls == calls + 2,
                      "scoped reentry gate releases for later deliberate native operation on same owner");
            }
        }
        const auto admitted = ClockStreamingLoadAllOwnerTest::calls;
        retire(service);
        Check(ClockStreamingLoadAllOwnerTest::calls == admitted && graph.world[Q::WorldModeOffset] == 1,
              "retirement neither repeats loading nor restores stale native mode/world state");
    }
    ClockStreamingLoadAllOwnerTest::image = ClockStreamingLoadAllOwnerTest::world = ClockStreamingLoadAllOwnerTest::level = 0;
    ClockStreamingLoadAllOwnerTest::reenter = nullptr;
    std::puts("Production native streaming load-all callback owner fixture completed");
}
