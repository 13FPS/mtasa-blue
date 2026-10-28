/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Joint loaded-world Initialize prerequisites on the actual runtime
 ****************************************************************************/
{
    namespace G = NativeNamedGarageProfile;
    namespace N = NativeNumericGarageProfile;
    namespace Menu = NativeStartupMenuProfile;
    namespace Frontend = NativeFrontendMenuProfile;
    namespace Allocator = NativeCustomAnimSequenceStorageFixture;
    using Writes = WorldGroundOwnerFixture::Writes;
    resetSpecial();
    Writes writes{fixture, {}};
    StartupNumericGaragesRuntimeFixture::AdmitCountPage(fixture, writes);
    for (const auto& pin : G::Code)
        Writes::Apply(&writes, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
    for (const auto& pin : NativeNamedGarageGeometryProfile::Code)
        Writes::Apply(&writes, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
    Writes::Apply(&writes, G::FoldRva, G::Fold, sizeof(G::Fold), PAGE_READONLY);
    writes.Global(G::CountRva, N::CertifiedPublishedCount);
    for (std::uint32_t index = 0; index != N::CertifiedPublishedCount; ++index)
    {
        std::array<unsigned char, G::RowBytes> empty{};
        Writes::Apply(&writes, G::BankRva + static_cast<std::uintptr_t>(index) * G::RowBytes, empty.data(), empty.size(), PAGE_READWRITE);
    }
    for (std::size_t slot = 0; slot != N::Descriptors.size(); ++slot)
    {
        const auto& descriptor = N::Descriptors[slot];
        const auto  garage = StartupNumericGaragesRuntimeFixture::RowRva(slot);
        Writes::Apply(&writes, garage, descriptor.geometry.words.data(), sizeof(descriptor.geometry.words), PAGE_READWRITE);
        Writes::Apply(&writes, garage + G::NameOffset, descriptor.name.data(), descriptor.name.size(), PAGE_READWRITE);
        writes.Global(garage + G::TypeOffset, static_cast<std::uint8_t>(0x80u + slot));
        writes.Global(garage + G::DoorStateOffset, std::uint8_t{0});
    }
    // Compose real prerequisites in the same provider/world/heap graph. The
    // allocator fixture restores complete native malloc/free code over K's
    // nonexecuted entry stub; no allocation is used as a readiness probe.
    NativePedClothesFixture::Install(fixture.image, &Writes::Apply, &writes);
    NativePedClothesFixture::NameState names;
    names.Reset(fixture.image, &Writes::Apply, &writes);
    Allocator::State host;
    host.Reset(fixture.image, &Writes::Apply, &writes);
    const auto previousAllocator = Allocator::active;
    Allocator::active = &host;
    PedMetadataLifecycleFixture::DefaultData defaults;
    defaults.Seed(fixture, writes);
    Writes::Apply(&writes, NativeModelObjectProfile::CollisionConsumerRva, NativeModelObjectProfile::CollisionConsumer.data(),
                  NativeModelObjectProfile::CollisionConsumer.size(), PAGE_EXECUTE_READ);

    // Reuse the menu fixture's exact InitializePins profile loops only. Its
    // Reset would replace the registered streaming provider graph, so qualify
    // the genuine empty-widget/empty-stack route on the existing owned root.
    for (const auto& pin : Frontend::pins)
        Writes::Apply(&writes, pin.rva, pin.bytes, pin.size, pin.code ? PAGE_EXECUTE_READ : PAGE_READONLY);
    // This nested fixture shares its scope with the special-model setup;
    // a distinct local keeps the combined fixture valid under /WX.
    const unsigned char initializationReturn = 0xc3;
    for (const auto& target : Frontend::vtableTargets)
    {
        const auto pointer = fixture.image + target.targetRva;
        Writes::Apply(&writes, target.targetRva, &initializationReturn, 1, PAGE_EXECUTE_READ);
        writes.Global(target.slotRva, pointer, PAGE_READONLY);
    }
    for (const auto& target : Frontend::imagePointers)
        writes.Global(target.slotRva, fixture.image + target.targetRva, PAGE_READONLY);
    for (const auto& pin : Menu::Pins)
        Writes::Apply(&writes, pin.rva, pin.bytes, pin.size, pin.code ? PAGE_EXECUTE_READ : PAGE_READONLY);
    store(owner.data(), Menu::DependencyOffset, std::uintptr_t{0});
    store(owner.data(), Frontend::widgetOffset, std::uintptr_t{0});
    store(owner.data(), Menu::AuxiliaryOffset, std::uint32_t{0});
    writes.Global(Menu::FlagRva, std::uint8_t{0});
    const std::array<unsigned char, 24> emptyStack{};
    Writes::Apply(&writes, Frontend::menuHeaderRva, emptyStack.data(), emptyStack.size(), PAGE_READWRITE);

    ClockNamedGarageOwnerTest::stores = ClockNamedGarageOwnerTest::action = ClockNamedGarageOwnerTest::geometryReadAction = 0;
    ClockStartupMenuInvokeTest::calls = ClockStartupCollisionAtomicTest::calls = 0;
    Check(specialAbsent.RequireStartupInitializationReady() == ClockResult::NotInitialized &&
              runtime.access.RequireStartupInitializationReady() != ClockResult::Ok,
          "default and unbound owners cannot claim loaded-world initialization readiness");
    const auto nativeSnapshot = [&]
    {
        std::vector<unsigned char> bytes;
        const auto                 append = [&](std::uintptr_t address, std::size_t size)
        {
            const auto start = bytes.size();
            bytes.resize(start + size);
            SIZE_T copied = 0;
            Check(::ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<const void*>(address), bytes.data() + start, size, &copied) && copied == size,
                  "joint readiness snapshots only its bounded owned fixture bytes");
        };
        append(root, owner.size());
        append(providerAddress, provider.size());
        append(modelAddress, model.size());
        append(fixture.image + G::CountRva, sizeof(std::uint32_t));
        append(fixture.image + G::BankRva, N::CertifiedPublishedCount * G::RowBytes);
        append(fixture.image + NativeModelBankProfile::BankRva, NativeModelBankProfile::BankBytes);
        append(fixture.image + X::NamePoolRva, X::NamePoolBytes);
        append(fixture.image + X::CacheRva, X::CacheHeaderBytes);
        append(fixture.image + X::ReverseCacheRva, X::CacheHeaderBytes);
        append(reinterpret_cast<std::uintptr_t>(names.heap.data()), names.heap.size());
        append(reinterpret_cast<std::uintptr_t>(names.nameBlock.data()), names.nameBlock.size());
        append(reinterpret_cast<std::uintptr_t>(names.shardHashes.data()), names.shardHashes.size());
        append(reinterpret_cast<std::uintptr_t>(names.cacheRows.data()), names.cacheRows.size());
        append(reinterpret_cast<std::uintptr_t>(defaults.bytes), 0x3000);
        append(fixture.image + NativePedModelMetadataProfile::DefaultCollisionRva, 0x38);
        append(fixture.image + Menu::FlagRva, 1);
        append(fixture.image + Frontend::menuHeaderRva, emptyStack.size());
        return bytes;
    };
    const auto effects = [&]
    {
        return std::make_tuple(host.mallocCalls, host.freeCalls, ClockSpecialModelOwnerTest::calls, ClockStreamingRequestOwnerTest::calls,
                               ClockNamedGarageOwnerTest::stores, ClockStartupMenuInvokeTest::calls, ClockStartupCollisionAtomicTest::calls);
    };
    NativeClockService service;
    const auto         invoke = [&]
    {
        Check(service.Renew(runtime.generation.load()), "renew joint readiness fixture between independent callback phases");
        fixture.invokeTimer();
    };
    const auto unchanged = [&](auto&& action)
    {
        const StartupPreparationReadinessRuntimeFixture::Snapshot journals(host);
        const auto                                                before = nativeSnapshot();
        const auto                                                beforeEffects = effects();
        const auto menuJournal = std::make_tuple(runtime.startupMenuGeneration.load(), runtime.startupMenuBusy.load(), runtime.startupMenuRetained.load(),
                                                 runtime.startupMenuCloses.size());
        action();
        journals.Unchanged(host);
        Check(nativeSnapshot() == before && effects() == beforeEffects && !runtime.stopping.load() &&
                  menuJournal == std::make_tuple(runtime.startupMenuGeneration.load(), runtime.startupMenuBusy.load(), runtime.startupMenuRetained.load(),
                                                 runtime.startupMenuCloses.size()),
              "aggregate readiness preserves garage/model/name/default/menu bytes and all native effect counts");
    };
    unsigned callbacks = 0;
    specialCall.action = [&](NativeGameTimingAccess& access)
    {
        ++callbacks;
        unchanged(
            [&]
            {
                Check(access.RequireStartupNumericGaragesReady() == ClockResult::Ok && access.RequireStartupPedModelCreationReady() == ClockResult::Ok &&
                          access.RequireStartupStreamingReceiverReady() == ClockResult::Ok && access.RequireStartupCollisionCopyReady() == ClockResult::Ok &&
                          access.RequireStartupNativeMenuReady() == ClockResult::Ok,
                      "one actual retained callback qualifies all genuine initialization dependencies in the same live image");
                Check(access.RequireStartupInitializationReady() == ClockResult::Ok && access.RequireStartupInitializationReady() == ClockResult::Ok,
                      "the actual NativeAccess aggregate accepts joint prerequisites without invoking an Initialize effect or SDK getter");
            });
    };
    BeginOwned(service, fixture, false, true, true, &SpecialCallback::Invoke, &specialCall);
    invoke();
    Check(callbacks == 1 && effects() == std::make_tuple(0u, 0u, 0u, 0u, 0u, 0u, 0u),
          "joint positive readiness used zero native allocations, requests, garage writes, collision CAS or menu entries");
    std::puts("Production startup initialization joint readiness actual owner completed");
    Check(runtime.access.RequireStartupInitializationReady() != ClockResult::Ok && !runtime.stopping.load(),
          "retained objects alone cannot grant between-callback initialization readiness");
    ClockResult foreignResult = ClockResult::Ok;
    std::thread foreign([&] { foreignResult = runtime.access.RequireStartupInitializationReady(); });
    foreign.join();
    Check(foreignResult != ClockResult::Ok && !runtime.stopping.load(), "foreign aggregate reads cannot borrow or stop the actual admitted owner");

    specialCall.action = [&](NativeGameTimingAccess& access)
    {
        PedModelCreationScope busy;
        Check(busy.held, "joint refusal fixture owns the genuine temporary creation exclusion");
        unchanged(
            [&]
            {
                const auto expected = access.RequireStartupNumericGaragesReady();
                Check(expected != ClockResult::Ok && expected != ClockResult::Busy && access.RequireStartupInitializationReady() == expected,
                      "actual aggregate returns the earlier genuine garage refusal before the simultaneously busy creation dependency");
            });
    };
    writes.Global(G::CountRva, N::CertifiedPublishedCount - 1);
    invoke();
    writes.Global(G::CountRva, N::CertifiedPublishedCount);
    specialCall.action = [&](NativeGameTimingAccess& access)
    {
        PedModelCreationScope busy;
        Check(busy.held, "joint creation contention retains the original scope");
        unchanged(
            [&] { Check(access.RequireStartupInitializationReady() == ClockResult::Busy, "current creation contention freshly refuses aggregate readiness"); });
    };
    invoke();
    specialCall.action = [&](NativeGameTimingAccess& access)
    {
        StartupCollisionScope busy;
        Check(busy.held, "joint streaming refusal keeps the genuine later collision exclusion busy");
        unchanged(
            [&]
            {
                const auto expected = access.RequireStartupStreamingReceiverReady();
                Check(expected != ClockResult::Ok && expected != ClockResult::Busy && access.RequireStartupInitializationReady() == expected,
                      "actual aggregate forwards the fresh native name refusal before the later busy collision prerequisite");
            });
    };
    fixture.Put(X::NameInitializedRva, std::uint8_t{0});
    invoke();
    fixture.Put(X::NameInitializedRva, initializedNames);
    specialCall.action = [&](NativeGameTimingAccess& access)
    {
        StartupCollisionScope busy;
        Check(busy.held, "joint collision contention owns its existing temporary exclusion");
        unchanged(
            [&]
            { Check(access.RequireStartupInitializationReady() == ClockResult::Busy, "current collision contention freshly refuses aggregate readiness"); });
    };
    invoke();
    specialCall.action = [&](NativeGameTimingAccess& access)
    {
        unchanged(
            [&]
            {
                const auto expected = access.RequireStartupNativeMenuReady();
                Check(expected != ClockResult::Ok && access.RequireStartupInitializationReady() == expected,
                      "the actual aggregate freshly refuses the real nonempty native frontend stack");
            });
    };
    writes.Global(Frontend::menuHeaderRva + 8, std::int32_t{1});
    invoke();
    writes.Global(Frontend::menuHeaderRva + 8, std::int32_t{0});
    specialCall.action = [&](NativeGameTimingAccess& access)
    {
        unchanged(
            [&]
            {
                const auto generation = runtime.generation.load();
                ClockStartupInitializationReadinessTest::watched = fixture.image + Menu::FlagRva;
                ClockStartupInitializationReadinessTest::afterRead = +[]() noexcept { runtime.generation.fetch_add(1); };
                const auto result = access.RequireStartupInitializationReady();
                const bool consumed = !ClockStartupInitializationReadinessTest::afterRead;
                ClockStartupInitializationReadinessTest::watched = 0;
                ClockStartupInitializationReadinessTest::afterRead = nullptr;
                runtime.generation.store(generation);
                Check(consumed && result != ClockResult::Ok,
                      "actual aggregate rejects generation loss during final native-menu prerequisite reads without inventing an owner result");
                Check(access.RequireStartupInitializationReady() == ClockResult::Ok,
                      "a subsequent same-generation call freshly requalifies the restored genuine prerequisites");
            });
    };
    invoke();
    std::puts("Production startup initialization current refusals and owner bookends completed");
    retireSpecial(service);
    Check(runtime.access.RequireStartupInitializationReady() != ClockResult::Ok && effects() == std::make_tuple(0u, 0u, 0u, 0u, 0u, 0u, 0u) &&
              runtime.pedModelCreations.empty() && runtime.startupCollisionCopies.empty(),
          "clean retirement publishes no aggregate readiness, appends no startup journal and executes no native initialization effects");
    Allocator::active = previousAllocator;
    writes.Restore();
    std::puts("Production startup initialization readiness no-effects and clean retirement completed");
    std::puts("Production startup initialization readiness runtime fixture completed");
}
