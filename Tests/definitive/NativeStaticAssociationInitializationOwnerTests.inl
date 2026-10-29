/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual registered-clump/static-row ownership and entered debt
 ****************************************************************************/
namespace ClockStaticAssociationInitializationInvokeTest
{
    unsigned                                         calls = 0, events = 0;
    bool                                             uncertain = false, nestedBusy = false, gateBusy = false;
    NativeStaticAssociationInitializeFixture::State* selected = nullptr;
    NativeAnimationStaticAssociationReferenceToken   row;
    ClockResult Invoke(Calls::NativeFunction function, std::uintptr_t association, std::uintptr_t clump, std::uintptr_t hierarchy,
                       std::uint32_t& exception) noexcept
    {
        ++calls;
        if (!selected || reinterpret_cast<std::uintptr_t>(function) != runtime.image + NativeStaticAssociationInitializeProfile::EntryRva ||
            association != selected->association || clump != selected->Clump() || hierarchy != selected->hierarchy)
            return ClockResult::BindingChanged;
        nestedBusy = runtime.access.InitializeAnimationStaticAssociation(row, clump, hierarchy) == ClockResult::Busy;
        const bool entered = NativeStreamingCompletionContext::TryEnter();
        gateBusy = !entered;
        if (entered)
            NativeStreamingCompletionContext::Leave();
        // These are injected effects only. Production still captures original
        // bone/name rows and enters exactly the one original FEB5F0 function;
        // this fixture never executes a CPU name matcher or game instructions.
        selected->AcceptInjectedResult();
        ++events;
        if (uncertain)
        {
            exception = EXCEPTION_ACCESS_VIOLATION;
            return ClockResult::WriteFailure;
        }
        return ClockResult::Ok;
    }
}
namespace StaticAssociationInitializationOwnerFixture
{
    unsigned scenario = 0;
    // Shipping keeps producer renewal independent of the engine callback.
    // BeginOwned deliberately collapses both onto the test thread; expensive
    // genuine pre/post native qualification can then consume its 500ms lease.
    // Model the real producer split through the unchanged public Renew API,
    // without changing time, duration, freshness checks or engine admission.
    struct ProducerPulse
    {
        NativeClockService&   service;
        std::uint64_t         generation;
        DWORD                 previousProducer;
        std::atomic<bool>     ready{false}, stopping{false};
        std::atomic<unsigned> renewals{0}, unexpectedFailures{0};
        std::thread           worker;
        ProducerPulse(NativeClockService& owner, std::uint64_t current)
            : service(owner),
              generation(current),
              previousProducer(runtime.producerThread),
              worker(
                  [this]
                  {
                      // The release/acquire handshake publishes this one plain
                      // owner-field write before the engine callback can read it.
                      runtime.producerThread = GetCurrentThreadId();
                      do
                      {
                          if (service.Renew(generation))
                              renewals.fetch_add(1);
                          else if (!runtime.stopping.load())
                              unexpectedFailures.fetch_add(1);
                          ready.store(true, std::memory_order_release);
                          Sleep(10);
                      } while (!stopping.load(std::memory_order_acquire));
                  })
        {
            while (!ready.load(std::memory_order_acquire))
                Sleep(1);
        }
        void Stop() noexcept
        {
            stopping.store(true, std::memory_order_release);
            if (worker.joinable())
            {
                worker.join();
                // No producer/API operation remains when the engine thread
                // resumes its ordinary service Read/End producer duties.
                runtime.producerThread = previousProducer;
            }
        }
        ~ProducerPulse() { Stop(); }
        ProducerPulse(const ProducerPulse&) = delete;
        ProducerPulse& operator=(const ProducerPulse&) = delete;
    };
    void RunPrepared(ClockImageFixture& fixture)
    {
        namespace R = NativePedReferenceProfile;
        namespace Ped = NativePedPoolProfile;
        namespace Request = NativeStreamingRequestProfile;
        namespace B = NativeAnimationBlockProfile;
        namespace H = NativeAnimationBlockHierarchyProfile;
        namespace G = NativeAnimationGroupArrayProfile;
        namespace Hook = ClockStaticAssociationInitializationInvokeTest;
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
        alignas(std::uintptr_t) std::array<unsigned char, 8 + 32>     children{};
        NativeStaticAssociationInitializeFixture::Store(parents.data(), 0, std::uint64_t{2});
        NativeStaticAssociationInitializeFixture::Store(children.data(), 0, std::uint64_t{1});
        const auto groups = reinterpret_cast<std::uintptr_t>(parents.data()) + 8;
        const auto association = reinterpret_cast<std::uintptr_t>(children.data()) + 8;
        const auto block = fixture.image + B::BankRva + B::RowBytes;
        const auto hierarchy = fixture.image + H::BankRva + H::RowBytes;
        NativeStaticAssociationInitializeFixture::Store(reinterpret_cast<void*>(groups + 32), 0, block);
        NativeStaticAssociationInitializeFixture::Store(reinterpret_cast<void*>(groups + 32), 8, association);
        NativeStaticAssociationInitializeFixture::Store(reinterpret_cast<void*>(groups + 32), 0x10, std::uint32_t{1});
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
            const auto    started = GetTickCount64();
            ProducerPulse pulse(service, generation);
            Check(pulse.renewals.load() && !pulse.unexpectedFailures.load(), "independent actual producer is admitted before the engine callback begins");
            fixture.invokeTimer();
            pulse.Stop();
            Check(pulse.renewals.load() && !pulse.unexpectedFailures.load() && runtime.producerThread == GetCurrentThreadId(),
                  "real producer renewals preserve policy and join before ordinary producer ownership returns");
            std::printf("Static initialization action elapsed=%llu ms independent producer renewals=%u\n",
                        static_cast<unsigned long long>(GetTickCount64() - started), pulse.renewals.load());
            callback.action = [](NativeGameTimingAccess&) {};
        };
        NativePedReferenceToken pedToken;
        action(
            [&]
            { Check(runtime.access.AcquirePedReference(1, pedToken) == ClockResult::Ok, "issue actual registered current ped before borrowing its clump"); });
        NativeAnimationStaticAssociationReferenceToken row;
        action(
            [&]
            {
                Check(runtime.access.ResolveAnimationStaticAssociationById(generation, 1, 17, row) == ClockResult::Ok && row.ownerGeneration == generation &&
                          row.groupIndex == 1 && row.index == 0 && row.locator == association,
                      "actual numeric factory resolver keeps native modulo-offset physical row identity");
                NativeAnimationStaticAssociationReferenceToken raw;
                Check(runtime.access.ResolveAnimationStaticAssociationInterface(generation, association, raw) == ClockResult::Ok &&
                          raw.locator == association && raw.groupIndex == 1 && raw.index == 0,
                      "actual raw factory resolver reacquires the same physical native row");
            });
        Hook::selected = &setup;
        Hook::row = row;
        Hook::calls = Hook::events = 0;
        Hook::uncertain = scenario != 0;
        Check(runtime.access.InitializeAnimationStaticAssociation(row, setup.Clump(), hierarchy) != ClockResult::Ok && !Hook::calls,
              "valid row and registered clump tags do not impersonate a natural callback");
        action(
            [&]
            {
                auto stale = row;
                --stale.ownerGeneration;
                Check(runtime.access.InitializeAnimationStaticAssociation(stale, setup.Clump(), hierarchy) == ClockResult::OwnerConflict && !Hook::calls,
                      "stale static owner generation refuses before native entry");
                Check(runtime.access.InitializeAnimationStaticAssociation(row, setup.Clump() + 8, hierarchy) == ClockResult::NotInitialized && !Hook::calls,
                      "unregistered clump has no pool-scan or pointer adoption fallback");
                Check(runtime.access.InitializeAnimationStaticAssociation(row, setup.Clump(), hierarchy + H::RowBytes) != ClockResult::Ok && !Hook::calls,
                      "cross-bank or replacement hierarchy has no static initializer authority");
                Check(NativeStreamingCompletionContext::TryEnter(), "hold existing native completion gate for a genuine refusal");
                const auto result = runtime.access.InitializeAnimationStaticAssociation(row, setup.Clump(), hierarchy);
                NativeStreamingCompletionContext::Leave();
                Check(result == ClockResult::Busy && !Hook::calls && !runtime.staticAssociationInitializationRecovery.load(),
                      "shared native gate refusal is unentered and creates no journal debt");
            });
        Check(NativeStaticAssociationInitializeFixture::Load<std::uint16_t>(reinterpret_cast<void*>(association), 8) == 0xcafe,
              "the genuine fresh phase ignores undefined nonzero old count while node pointer is zero");
        ClockResult result = ClockResult::NotInitialized;
        action(
            [&]
            {
                if (scenario)
                    result = runtime.access.InitializeAnimationStaticAssociation(row, setup.Clump(), hierarchy);
                else
                {
                    // The successful path crosses the real SDK virtual before
                    // reaching the same NativeAccess/current-frame owner boundary.
                    AnimationStaticAssociationReceiver facade(runtime.access, row);
                    facade.Initialize(reinterpret_cast<RpClump*>(setup.Clump()), reinterpret_cast<CAnimBlendHierarchySAInterface*>(hierarchy));
                    result = ClockResult::Ok;
                }
            });
        const auto& receipt = runtime.staticAssociationInitialization;
        Check(Hook::calls == 1 && Hook::events == 1 && Hook::nestedBusy && Hook::gateBusy && receipt.entered && receipt.rowObserved &&
                  receipt.observedNodes == setup.Nodes() && receipt.observedCount == 2 && !runtime.staticAssociationInitializationBusy.load(),
              "one original typed entry runs with actual runtime/shared gate and preserves published count/nodes from nonzero prior count");
        if (scenario)
        {
            Check(result != ClockResult::Ok && runtime.staticAssociationInitializationRecovery.load() && runtime.stopping.load() && service.IsRetained(),
                  "entered lost acknowledgement stops the actual owner and retains durable allocation observations");
            Check(State(service).dataRecoveryPending && !State(service).ready, "published Clock readiness denies uncertain static allocation debt");
            Check(runtime.access.InitializeAnimationStaticAssociation(row, setup.Clump(), hierarchy) != ClockResult::Ok && Hook::calls == 1 && !service.End() &&
                      service.IsRetained() && NativeLifetimeRetained() && !heap.freeCalls,
                  "uncertain native allocation cannot replay, free, rewind or release the actual Clock lifetime");
            std::puts("Production static association initialization entered debt retained without replay");
        }
        else
        {
            Check(result == ClockResult::Ok && receipt.returned && receipt.verified && !runtime.staticAssociationInitializationRecovery.load(),
                  "verified current same-bank static row initialization is accepted by actual NativeAccess");
            action(
                [&]
                {
                    AnimationStaticAssociationReceiver facade(runtime.access, row);
                    Check(facade.GetNumBlendNodes() == 2 && facade.GetAnimHierarchyInterface() == reinterpret_cast<CAnimBlendHierarchySAInterface*>(hierarchy),
                          "complete real static SDK sees original accepted node count and current hierarchy");
                    Check(runtime.access.InitializeAnimationStaticAssociation(row, setup.Clump(), hierarchy) != ClockResult::Ok && Hook::calls == 1,
                          "already mapped row cannot replay allocation or invent free-plus-clear reinitialization");
                });
            const auto nodes = NativeStaticAssociationInitializeFixture::Load<std::uintptr_t>(reinterpret_cast<void*>(association), 0x10);
            Check(!heap.freeCalls && !heap.mallocCalls, "CPU facades and transport do not add private allocation or extra bank counter effects");
            Check(!service.End(), "registered cell cleanup still requires original owner callback");
            fixture.invokeTimer();
            Check(service.End() && !service.IsRetained() && !runtime.pedReferences.IsRetained() && !runtime.staticAssociationInitializationRecovery.load() &&
                      NativeStaticAssociationInitializeFixture::Load<std::uintptr_t>(reinterpret_cast<void*>(association), 0x10) == nodes && !heap.freeCalls,
                  "normal CPU owner retirement frees only its registered reference, preserving engine static node ownership");
            std::puts("Production static association initialization actual owner and SDK fixture completed");
            std::puts("Production static association initialization registered same bank refusals completed");
        }
        Hook::selected = nullptr;
        NativeCustomAnimSequenceStorageFixture::active = nullptr;
        ClockReaderTestHost::image = previousImage;
        ClockPedReferenceOwnerTest::image = previousReferences;
        writes.Restore();
    }
}
