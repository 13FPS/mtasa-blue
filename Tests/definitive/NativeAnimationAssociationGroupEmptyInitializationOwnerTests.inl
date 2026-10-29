/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual reverse physical initialization with source skip semantics
 ****************************************************************************/
namespace ClockGroupEmptyInitializationInvokeTest
{
    using Setup = NativeStaticAssociationInitializeFixture::State;
    bool                                          active = false, uncertain = false, nestedBusy = false, gateBusy = false, lifetimeBusy = false;
    unsigned                                      calls = 0, events = 0;
    std::uintptr_t                                rows = 0;
    Setup*                                        selected = nullptr;
    NativeAnimationAssociationGroupReferenceToken group;
    using NodeRows = std::array<std::array<std::uintptr_t, 2>, 3>;
    NodeRows*                  nodeRows = nullptr;
    std::vector<std::uint32_t> order;
    ClockResult Invoke(mta::game_de::NativeStaticAssociationInitializeCalls::NativeFunction function, std::uintptr_t association, std::uintptr_t clump,
                       std::uintptr_t hierarchy, std::uint32_t& exception) noexcept
    {
        if (!active)
            return ClockStaticAssociationInitializationInvokeTest::Invoke(function, association, clump, hierarchy, exception);
        ++calls;
        if (!selected || !nodeRows || reinterpret_cast<std::uintptr_t>(function) != runtime.image + NativeStaticAssociationInitializeProfile::EntryRva ||
            association < rows || (association - rows) % 32 || (association - rows) / 32 >= 3 || clump != selected->Clump() || hierarchy != selected->hierarchy)
            return ClockResult::BindingChanged;
        const auto index = static_cast<std::uint32_t>((association - rows) / 32);
        order.push_back(index);
        nestedBusy = runtime.access.InitializeEmptyAnimationAssociationGroup(group, clump) == ClockResult::Busy;
        const bool entered = NativeStreamingCompletionContext::TryEnter();
        gateBusy = !entered;
        if (entered)
            NativeStreamingCompletionContext::Leave();
        lifetimeBusy = runtime.associationGroupInitializationBusy.load() && NativeLifetimeRetained();
        // Only this existing final native seam injects task-owned effects.
        // Production still qualifies current registry, hierarchy, frame map,
        // row and native heap, then calls one genuine FEB5F0 entry per row.
        (*nodeRows)[index] = {selected->Sequence(0), selected->Sequence(1)};
        NativeStaticAssociationInitializeFixture::Store(reinterpret_cast<void*>(association), 8, std::uint16_t{2});
        NativeStaticAssociationInitializeFixture::Store(reinterpret_cast<void*>(association), 0x10,
                                                        reinterpret_cast<std::uintptr_t>((*nodeRows)[index].data()));
        ++events;
        if (uncertain && index == 0)
        {
            exception = EXCEPTION_ACCESS_VIOLATION;
            return ClockResult::WriteFailure;
        }
        return ClockResult::Ok;
    }
}
namespace AnimationAssociationGroupEmptyInitializationOwnerFixture
{
    unsigned scenario = 0;
    namespace ReadHook = ClockAnimationManagerOwnerTest;
    std::uintptr_t parent = 0, childCookie = 0;
    unsigned       transition = 0;
    bool           changed = false;
    void           RevokeAfterVerifiedPrefix() noexcept
    {
        // The mutation is deliberately after the complete original-entry
        // helper verified one row. Earlier mutation would correctly test an
        // entered unknown effect instead of an unentered next-row refusal.
        if (runtime.associationGroupInitialization.initialized != 1)
        {
            ReadHook::afterRead = &RevokeAfterVerifiedPrefix;
            return;
        }
        if (transition == 0)
        {
            const std::uint64_t invalid = 0;
            std::memcpy(reinterpret_cast<void*>(childCookie), &invalid, 8);
        }
        else
        {
            const std::uint32_t count = 1;
            std::memcpy(reinterpret_cast<void*>(parent + 0x10), &count, 4);
        }
        changed = true;
    }
    void RunPrepared(ClockImageFixture& fixture)
    {
        namespace R = NativePedReferenceProfile;
        namespace Ped = NativePedPoolProfile;
        namespace Request = NativeStreamingRequestProfile;
        namespace B = NativeAnimationBlockProfile;
        namespace H = NativeAnimationBlockHierarchyProfile;
        namespace G = NativeAnimationGroupArrayProfile;
        namespace Hook = ClockGroupEmptyInitializationInvokeTest;
        using Setup = NativeStaticAssociationInitializeFixture::State;
        WorldGroundOwnerFixture::Writes writes{fixture, {}};
        const auto                      admit = [&](const auto& pins)
        {
            for (const auto& pin : pins)
                WorldGroundOwnerFixture::Writes::Apply(&writes, pin.rva, pin.bytes, pin.size, pin.code ? PAGE_EXECUTE_READ : PAGE_READONLY);
        };
        for (const auto& pin : Ped::Code)
            WorldGroundOwnerFixture::Writes::Apply(&writes, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : R::Code)
            WorldGroundOwnerFixture::Writes::Apply(&writes, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& slot : R::Slots)
            writes.Global(slot.rva, fixture.image + slot.targetRva, PAGE_READONLY);
        for (const auto& pin : NativeStreamingLoadAllProfile::Code)
            WorldGroundOwnerFixture::Writes::Apply(&writes, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        admit(NativeStartupOwnerProfile::pins);
        admit(NativeStartupWorldProfile::pins);
        admit(NativeGameStateProfile::pins);
        for (const auto& pin : NativeUObjectMembershipProfile::pins)
            WorldGroundOwnerFixture::Writes::Apply(&writes, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : NativeBorrowedGcProfile::pins)
            WorldGroundOwnerFixture::Writes::Apply(&writes, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : NativeStartGameProfile::Code)
            if (pin.rva == 0xab879c || pin.rva == 0xab92f0 || pin.rva == 0x1af4ef0)
                WorldGroundOwnerFixture::Writes::Apply(&writes, pin.rva, pin.bytes.data(), pin.size, PAGE_EXECUTE_READ);
        alignas(std::uintptr_t) std::array<unsigned char, 0x180> root{};
        NativeStreamingLoadAllFixture::Graph graph(fixture.image, reinterpret_cast<std::uintptr_t>(root.data()), &WorldGroundOwnerFixture::Writes::Apply,
                                                   &writes);
        NativePedClothesFixture::Install(fixture.image, &WorldGroundOwnerFixture::Writes::Apply, &writes);
        NativePedClothesFixture::NameState names;
        names.Reset(fixture.image, &WorldGroundOwnerFixture::Writes::Apply, &writes);
        NativeCustomAnimSequenceStorageFixture::State heap;
        heap.Reset(fixture.image, &WorldGroundOwnerFixture::Writes::Apply, &writes);
        NativeCustomAnimSequenceStorageFixture::active = &heap;
        NativeAnimationBlockFactoryFixture::Install(fixture.image, &WorldGroundOwnerFixture::Writes::Apply, &writes);
        for (const auto& pin : NativeAnimationHierarchyReadProfile::Code)
            WorldGroundOwnerFixture::Writes::Apply(&writes, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : NativeAnimationBlockRefProfile::Code)
            WorldGroundOwnerFixture::Writes::Apply(&writes, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : G::Code)
            WorldGroundOwnerFixture::Writes::Apply(&writes, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : NativeAnimationStaticAssociationProfile::Code)
            WorldGroundOwnerFixture::Writes::Apply(&writes, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        // The actual owner reacquires and reads the physical row before native
        // entry. Seed those read leaves too; selector/retire pins alone only
        // admitted factory resolution and hid this fixture omission.
        for (const auto& pin : NativeAnimationStaticAssociationProfile::ReadCode)
            WorldGroundOwnerFixture::Writes::Apply(&writes, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        WorldGroundOwnerFixture::Writes::Apply(&writes, 0xfec67a, private_hierarchy_fixture::CacheProof, sizeof(private_hierarchy_fixture::CacheProof),
                                               PAGE_EXECUTE_READ);
        writes.Global(NativeGameStateProfile::stateRva, std::uint32_t{9});
        writes.Global(NativeStartGameProfile::ThreadInitializedRva, static_cast<unsigned char>(1));
        writes.Global(NativeStartGameProfile::ThreadIdRva, GetCurrentThreadId());
        writes.Global(NativeBorrowedGcProfile::markerRva, LONG{0});
        writes.Global(NativePlayerInfoProfile::BankRva, std::uintptr_t{0});
        std::vector<unsigned char> references(R::ReferenceCount * R::ReferenceBytes);
        for (std::size_t i = 0; i + 1 < R::ReferenceCount; ++i)
        {
            const auto next = fixture.image + R::ReferencesRva + (i + 1) * R::ReferenceBytes;
            std::memcpy(references.data() + i * R::ReferenceBytes, &next, 8);
        }
        WorldGroundOwnerFixture::Writes::Apply(&writes, R::ReferencesRva, references.data(), references.size(), PAGE_READWRITE);
        writes.Global(R::FreeHeadRva, fixture.image + R::ReferencesRva);
        ClockTeleportPool pool(fixture, Ped::RootRva, Ped::NativeCapacity, Ped::ObjectStride);
        pool.Publish();
        pool.flags[0] = 1;
        const auto ped = reinterpret_cast<std::uintptr_t>(pool.objects);
        NativeStaticAssociationInitializeFixture::Store(pool.objects, 0, fixture.image + R::PedVtableRva);
        pool.objects[R::TypeOffset] = 3;
        NativeStaticAssociationInitializeFixture::Store(pool.objects, NativePedMetadataProfile::ModelOffset, std::uint16_t{0});
        alignas(std::uintptr_t) std::array<unsigned char, Request::ProviderCountOffset + 8> provider{};
        alignas(std::uintptr_t) std::array<unsigned char, Request::ProviderRowStride>       providerRows{};
        alignas(std::uintptr_t) std::array<unsigned char, Request::ModelPrefixBytes>        model{};
        alignas(std::uintptr_t) std::array<unsigned char, 12 * 24>                          baseItems{};
        std::memcpy(baseItems.data(), graph.items.data(), graph.items.size());
        const auto providerAddress = reinterpret_cast<std::uintptr_t>(provider.data());
        NativeStaticAssociationInitializeFixture::Store(provider.data(), 0, fixture.image + Request::RootVtableRva);
        NativeStaticAssociationInitializeFixture::Store(provider.data(), 0x0c, std::int32_t{11});
        NativeStaticAssociationInitializeFixture::Store(provider.data(), 0x10, graph.Class(0));
        NativeStaticAssociationInitializeFixture::Store(provider.data(), Request::ProviderRowsOffset, reinterpret_cast<std::uintptr_t>(providerRows.data()));
        NativeStaticAssociationInitializeFixture::Store(provider.data(), Request::ProviderCountOffset, std::int32_t{1});
        NativeStaticAssociationInitializeFixture::Store(root.data(), Request::ProviderOffset, providerAddress);
        NativeStaticAssociationInitializeFixture::Store(baseItems.data(), 11 * 24, providerAddress);
        NativeStaticAssociationInitializeFixture::Store(baseItems.data(), 11 * 24 + 0x10, std::uint32_t{111});
        NativeStaticAssociationInitializeFixture::Store(model.data(), 0, fixture.image + 0x4301000);
        for (const auto& pin : NativeModelBankProfile::Code)
            WorldGroundOwnerFixture::Writes::Apply(&writes, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        std::array<std::uintptr_t, 0x80 / 8> modelTable{};
        modelTable[Request::TypeMethodSlot / 8] = fixture.image + NativeStaticAssociationInitializeFixture::ModelTypeRva;
        modelTable[Request::AnimationMethodSlot / 8] = fixture.image + NativeStaticAssociationInitializeFixture::EventRva;
        WorldGroundOwnerFixture::Writes::Apply(&writes, 0x4301000, modelTable.data(), sizeof(modelTable), PAGE_READONLY);
        writes.Global(NativeModelBankProfile::BankRva, reinterpret_cast<std::uintptr_t>(model.data()));
        writes.Global(NativeModelLoadedProfile::ArrayRva + NativeModelLoadedProfile::StateOffset, static_cast<unsigned char>(1));
        alignas(std::uintptr_t) std::array<unsigned char, 8 + 2 * 32> parents{};
        alignas(std::uintptr_t) std::array<unsigned char, 8 + 3 * 32> children{};
        NativeStaticAssociationInitializeFixture::Store(parents.data(), 0, std::uint64_t{2});
        NativeStaticAssociationInitializeFixture::Store(children.data(), 0, std::uint64_t{3});
        const auto groups = reinterpret_cast<std::uintptr_t>(parents.data()) + 8;
        const auto association = reinterpret_cast<std::uintptr_t>(children.data()) + 8;
        const auto block = fixture.image + B::BankRva + B::RowBytes;
        const auto hierarchy = fixture.image + H::BankRva + H::RowBytes;
        NativeStaticAssociationInitializeFixture::Store(reinterpret_cast<void*>(groups + 32), 0, block);
        NativeStaticAssociationInitializeFixture::Store(reinterpret_cast<void*>(groups + 32), 8, association);
        NativeStaticAssociationInitializeFixture::Store(reinterpret_cast<void*>(groups + 32), 0x10, std::uint32_t{3});
        NativeStaticAssociationInitializeFixture::Store(reinterpret_cast<void*>(groups + 32), 0x14, std::uint32_t{17});
        NativeStaticAssociationInitializeFixture::Store(reinterpret_cast<void*>(groups + 32), 0x18, std::uint32_t{1});
        writes.Global(G::GlobalRva, groups);
        writes.Global(G::CountRva, std::int32_t{2});
        writes.Global(B::CountRva, std::int32_t{2});
        writes.Global(NativeAnimationManagerProfile::AnimationCountRva, std::int32_t{2});
        const NativeAnimationBlockName name{'b', 'a', 'n', 'k', 0};
        writes.Global(B::BankRva + B::RowBytes, name);
        writes.Global(B::BankRva + B::RowBytes + B::LoadedOffset, static_cast<unsigned char>(1));
        writes.Global(B::BankRva + B::RowBytes + B::IdOffset, std::int32_t{1});
        writes.Global(B::BankRva + B::RowBytes + B::AnimationCountOffset, std::uint32_t{1});
        Setup setup;
        setup.Reset(fixture.image, ped, association, hierarchy, &WorldGroundOwnerFixture::Writes::Apply, &writes);
        setup.EnableSkeletonInputs();
        Check(setup.AttachRegistry(baseItems.data(), 12, graph.Class(0), &WorldGroundOwnerFixture::Writes::Apply, &writes),
              "append actual clump/class/component/skeleton members to the existing world/provider fixture registry");
        const auto previousImage = ClockReaderTestHost::image, previousReferences = ClockPedReferenceOwnerTest::image;
        ClockReaderTestHost::image = ClockPedReferenceOwnerTest::image = fixture.image;
        ClockPedReferenceOwnerTest::registers = ClockPedReferenceOwnerTest::unregisters = 0;
        ClockPedReferenceOwnerTest::partialUnlink = false;
        ClockPedReferenceOwnerTest::unregisterResult = ClockResult::Ok;
        WorldGroundOwnerFixture::Callback callback;
        callback.action = [](NativeGameTimingAccess&) {};
        NativeClockService service;
        BeginOwned(service, fixture, false, true, true, &WorldGroundOwnerFixture::Callback::Invoke, &callback);
        HMODULE module = nullptr;
        Check(GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN, reinterpret_cast<LPCWSTR>(&HookClockContinuation),
                                 &module) &&
                  module,
              "pin actual callback code while registered references and native journal may remain");
        runtime.modulePinned.store(true);
        const auto generation = runtime.generation.load();
        const auto action = [&](const auto& body)
        {
            callback.action = [&](NativeGameTimingAccess&)
            {
                try
                {
                    body();
                }
                catch (const std::exception& error)
                {
                    // Require deliberately throws on native refusal. Report it
                    // inside this noexcept callback so failure evidence keeps
                    // the native result instead of only a terminate exit code.
                    const auto& failed = runtime.staticAssociationInitialization;
                    const auto  now = GetTickCount64();
                    std::fprintf(stderr,
                                 "Static association initialization action refused: %s; calls=%u entered=%d returned=%d verified=%d; serviceAge=%llu ms "
                                 "renewalAge=%llu ms\n",
                                 error.what(), Hook::calls, static_cast<int>(failed.entered), static_cast<int>(failed.returned),
                                 static_cast<int>(failed.verified), static_cast<unsigned long long>(now - runtime.started),
                                 static_cast<unsigned long long>(now - runtime.refreshed.load()));
                    std::fflush(stderr);
                    Check(false, "actual static association initialization action must not throw");
                }
            };
            Check(service.Renew(generation), "renew one focused current native static initialization action");
            const auto                                                 started = GetTickCount64();
            StaticAssociationInitializationOwnerFixture::ProducerPulse pulse(service, generation);
            Check(pulse.renewals.load() && !pulse.unexpectedFailures.load(), "independent actual producer is admitted before the engine callback begins");
            fixture.invokeTimer();
            pulse.Stop();
            Check(pulse.renewals.load() && !pulse.unexpectedFailures.load() && runtime.producerThread == GetCurrentThreadId(),
                  "real producer renewals preserve policy and join before ordinary producer ownership returns");
            std::printf("Static initialization action elapsed=%llu ms independent producer renewals=%u\n",
                        static_cast<unsigned long long>(GetTickCount64() - started), pulse.renewals.load());
            callback.action = [](NativeGameTimingAccess&) {};
        };
        namespace F = NativeStaticAssociationInitializeFixture;
        // Keep injected native-shaped nodes in private stack storage, matching
        // the existing positive initializer fixture's actual page contract.
        Hook::NodeRows nodeRows{};
        Hook::nodeRows = &nodeRows;
        Hook::order.reserve(3);
        std::array<unsigned char, 32> seedRow{};
        std::memcpy(seedRow.data(), reinterpret_cast<void*>(association), seedRow.size());
        const auto restoreChildren = [&]
        {
            for (std::size_t i = 0; i != 3; ++i)
                std::memcpy(reinterpret_cast<void*>(association + i * 32), seedRow.data(), 32);
            F::Store(children.data(), 0, std::uint64_t{3});
            F::Store(reinterpret_cast<void*>(groups + 32), 8, association);
            F::Store(reinterpret_cast<void*>(groups + 32), 0x10, std::uint32_t{3});
            // Source skips every nonzero pointer before touching its target
            // or hierarchy. A deliberately opaque pointer1 proves that path.
            F::Store(reinterpret_cast<void*>(association + 64), 0x10, std::uintptr_t{1});
            F::Store(reinterpret_cast<void*>(association + 64), 0x18, std::uintptr_t{0});
            writes.Global(B::BankRva + B::RowBytes + B::LoadedOffset, static_cast<unsigned char>(1));
            Hook::calls = Hook::events = 0;
            Hook::order.clear();
            Hook::nestedBusy = Hook::gateBusy = Hook::lifetimeBusy = false;
        };
        Hook::selected = &setup;
        Hook::rows = association;
        Hook::active = true;
        Hook::uncertain = scenario != 0;
        parent = groups + 32;
        childCookie = reinterpret_cast<std::uintptr_t>(children.data());
        NativeAnimationAssociationGroupReferenceToken group;
        action(
            [&]
            {
                Check(runtime.access.ResolveAnimationAssociationGroup(generation, 1, group) == ClockResult::Ok && group.locator == parent,
                      "actual runtime stamps current group before borrowing physical loop ownership");
            });
        Hook::group = group;
        AnimationAssociationGroupReceiver receiver(runtime.access, group);
        restoreChildren();
        if (!scenario)
        {
            action(
                [&]
                {
                    F::Store(reinterpret_cast<void*>(parent), 0x10, std::uint32_t{0});
                    F::Store(reinterpret_cast<void*>(parent), 8, std::uintptr_t{1});
                    receiver.InitEmptyAssociations(nullptr);
                    const auto& record = runtime.associationGroupInitialization;
                    Check(record.completed && record.result == ClockResult::Ok && !record.count && !record.visited && record.steps.empty() && !Hook::calls &&
                              !runtime.associationGroupInitializationBusy.load() && !runtime.staticAssociationInitializationRecovery.load(),
                          "count0 captures and completes without a child cookie, clump or native initializer entry");
                    F::Store(reinterpret_cast<void*>(parent), 8, association);
                    F::Store(reinterpret_cast<void*>(parent), 0x10, std::uint32_t{3});
                });
            action(
                [&]
                {
                    for (std::size_t index = 0; index != 3; ++index)
                    {
                        F::Store(reinterpret_cast<void*>(association + index * 32), 0x10, static_cast<std::uintptr_t>(index + 1));
                        F::Store(reinterpret_cast<void*>(association + index * 32), 0x18, std::uintptr_t{0});
                    }
                    writes.Global(B::BankRva + B::RowBytes + B::LoadedOffset, static_cast<unsigned char>(0));
                    receiver.InitEmptyAssociations(nullptr);
                    const auto& record = runtime.associationGroupInitialization;
                    Check(record.completed && record.count == 3 && record.visited == 3 && record.skipped == 3 && !record.initialized &&
                              record.steps.size() == 3 && record.steps[0].index == 2 && record.steps[1].index == 1 && record.steps[2].index == 0 &&
                              !Hook::calls && !runtime.staticAssociationInitializationRecovery.load(),
                          "all mapped rows skip in reverse order despite opaque node targets, null hierarchies, unloaded bank and null clump");
                });
            restoreChildren();
        }
        NativePedReferenceToken pedToken;
        action(
            [&]
            {
                Check(runtime.access.AcquirePedReference(1, pedToken) == ClockResult::Ok,
                      "issue the real registered ped before any reached original clump entry");
            });
        restoreChildren();
        ClockResult result = ClockResult::NotInitialized;
        action(
            [&]
            {
                if (scenario)
                    result = runtime.access.InitializeEmptyAnimationAssociationGroup(group, setup.Clump());
                else
                {
                    receiver.InitEmptyAssociations(reinterpret_cast<RpClump*>(setup.Clump()));
                    result = ClockResult::Ok;
                }
            });
        Check(Hook::order == std::vector<std::uint32_t>({1, 0}) && Hook::calls == 2 && Hook::events == 2 && Hook::nestedBusy && Hook::gateBusy &&
                  Hook::lifetimeBusy && !runtime.associationGroupInitializationBusy.load() && !runtime.staticAssociationInitializationBusy.load(),
              "actual reverse loop skips2, enters original1 then0 once, and shares existing native and outer-runtime busy admission");
        const auto& initial = runtime.associationGroupInitialization;
        Check(initial.count == 3 && initial.visited == 3 && initial.skipped == 1 && initial.steps.size() == 3 && initial.steps[0].index == 2 &&
                  initial.steps[0].skipped && initial.steps[1].index == 1 && initial.steps[1].initialized && initial.steps[1].native.entered &&
                  initial.steps[1].native.verified && initial.steps[2].index == 0 && initial.steps[2].native.entered &&
                  initial.steps[2].native.observedNodes == reinterpret_cast<std::uintptr_t>((*Hook::nodeRows)[0].data()),
              "production group journal preserves physical reverse steps and exact per-row original-call receipts");
        if (scenario)
        {
            Check(result != ClockResult::Ok && !initial.completed && initial.partialAccepted && initial.enteredUnknown && initial.initialized == 1 &&
                      initial.failedIndex == 0 && runtime.staticAssociationInitializationRecovery.load() && runtime.stopping.load() && service.IsRetained(),
                  "entered unknown final row keeps the previously verified prefix separate from real original allocation uncertainty");
            const auto calls = Hook::calls;
            Check(runtime.access.InitializeEmptyAnimationAssociationGroup(group, setup.Clump()) != ClockResult::Ok && Hook::calls == calls && !service.End() &&
                      service.IsRetained() && NativeLifetimeRetained() && !heap.freeCalls,
                  "unknown original effect cannot replay, free or release the actual native Clock lifetime");
            std::puts("Production group empty initialization entered debt retained without replay");
        }
        else
        {
            Check(result == ClockResult::Ok && initial.completed && initial.initialized == 2 && !initial.partialAccepted && !initial.enteredUnknown &&
                      !runtime.staticAssociationInitializationRecovery.load() && !heap.mallocCalls && !heap.freeCalls,
                  "verified original row effects stay engine-owned with no invented loop allocation or private cleanup debt");
            restoreChildren();
            action(
                [&]
                {
                    F::Store(reinterpret_cast<void*>(association), 0x18, std::uintptr_t{0});
                    result = runtime.access.InitializeEmptyAnimationAssociationGroup(group, setup.Clump());
                    const auto& record = runtime.associationGroupInitialization;
                    Check(result != ClockResult::Ok && !record.completed && record.count == 3 && record.initialized == 1 && record.skipped == 1 &&
                              record.partialAccepted && !record.enteredUnknown && record.failedIndex == 0 && Hook::order == std::vector<std::uint32_t>({1}) &&
                              !runtime.staticAssociationInitializationRecovery.load() && !runtime.stopping.load(),
                          "verified prefix plus later unentered null hierarchy remains recorded without reusing stale global entered receipt as new debt");
                });
            for (transition = 0; transition != 2; ++transition)
            {
                restoreChildren();
                action(
                    [&]
                    {
                        changed = false;
                        ReadHook::watched = fixture.image + G::GlobalRva;
                        ReadHook::watchedBytes = 8;
                        ReadHook::reads = 0;
                        ReadHook::afterRead = &RevokeAfterVerifiedPrefix;
                        result = runtime.access.InitializeEmptyAnimationAssociationGroup(group, setup.Clump());
                        ReadHook::afterRead = nullptr;
                        F::Store(children.data(), 0, std::uint64_t{3});
                        F::Store(reinterpret_cast<void*>(parent), 0x10, std::uint32_t{3});
                        const auto& record = runtime.associationGroupInitialization;
                        Check(changed && result != ClockResult::Ok && record.count == 3 && record.initialized == 1 && record.skipped == 1 &&
                                  record.partialAccepted && !record.completed && !record.enteredUnknown && Hook::order == std::vector<std::uint32_t>({1}) &&
                                  !runtime.staticAssociationInitializationRecovery.load() && !runtime.stopping.load(),
                              "current cookie or count revocation after a verified prefix refuses the next entry without reselecting captured count or "
                              "inventing unknown debt");
                    });
            }
            ReadHook::afterRead = nullptr;
            ReadHook::watched = 0;
            ReadHook::watchedBytes = sizeof(std::int32_t);
            ReadHook::failNext = false;
            Check(!service.End(), "registered reference teardown still needs its genuine owner callback");
            fixture.invokeTimer();
            Check(service.End() && !service.IsRetained() && !runtime.pedReferences.IsRetained() && !runtime.associationGroupInitializationBusy.load() &&
                      !runtime.staticAssociationInitializationRecovery.load() && !heap.freeCalls,
                  "normal End retires only CPU registration/hooks and leaves verified engine static nodes untouched");
            std::puts("Production group empty initialization actual owner and SDK fixture completed");
            std::puts("Production group empty initialization phase and verified prefix fixture completed");
        }
        Hook::active = false;
        Hook::selected = nullptr;
        Hook::rows = 0;
        Hook::nodeRows = nullptr;
        Hook::order.clear();
        parent = childCookie = 0;
        NativeCustomAnimSequenceStorageFixture::active = nullptr;
        ClockReaderTestHost::image = previousImage;
        ClockPedReferenceOwnerTest::image = previousReferences;
        writes.Restore();
    }
}
