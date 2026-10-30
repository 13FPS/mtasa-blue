namespace ClockNumericAssociationIssuedTest
{
    std::uintptr_t admittedImage = 0;
    HMODULE        Module(LPCWSTR name) noexcept
    {
        return !name && admittedImage ? reinterpret_cast<HMODULE>(admittedImage) : ::GetModuleHandleW(name);
    }
}
namespace mta::game_de::NativeImageAccess
{
    bool NumericClockFixtureExecutable() noexcept
    {
        return ::ClockNumericAssociationIssuedTest::admittedImage != 0 || CheckExecutableBuild();
    }
}
namespace ClockNumericAssociationIssuedTest
{
    namespace Host = NativeCustomAnimSequenceStorageFixture;
    namespace PrivateHook = ClockPrivateAnimationAssociationTest;
    namespace Calls = NativeAnimationAssociationCalls;
    bool                                          active = false, targetsValid = true, nestedBusy = false, gateBusy = false, lifetimeBusy = false;
    unsigned                                      calls = 0, mode = 0;
    std::uintptr_t                                image = 0, parent = 0, counter = 0;
    constexpr std::uint32_t                       Animation = 0x80000011u;
    NativeAnimationAssociationGroupReferenceToken group{};
    ClockResult Invoke(Calls::GroupCopyFunction function, std::int32_t physicalGroup, std::uint32_t animation, std::uintptr_t& returned,
                       std::uint32_t& exception) noexcept
    {
        if (!active)
            return ClockGroupCopyAnimationInvokeTest::Invoke(function, physicalGroup, animation, returned, exception);
        ++calls;
        targetsValid = targetsValid && reinterpret_cast<std::uintptr_t>(function) == image + 0xff1120 && physicalGroup == 1 && animation == Animation;
        NativeAnimationAssociationIssuedCopy nested;
        nestedBusy = runtime.access.CopyPrivateAnimationAssociationFromGroup(group, Animation, nested) == ClockResult::Busy;
        const bool entered = NativeStreamingCompletionContext::TryEnter();
        gateBusy = !entered;
        if (entered)
            NativeStreamingCompletionContext::Leave();
        lifetimeBusy = runtime.privateAssociations.HasPendingNative() && NativeLifetimeRetained();
        NativeModelReferenceHeap::Value heap;
        if (!Host::active || NativeModelReferenceHeap::Capture(image, heap) != ClockResult::Ok)
            return ClockResult::BindingChanged;
        Host::active->heap = heap.current;
        PrivateHook::HeapRoute route;
        if (!targetsValid || !route.Begin())
            return ClockResult::WriteFailure;
        PrivateHook::Store(route.table + 0x10, reinterpret_cast<std::uintptr_t>(&ClockGroupCopyAnimationInvokeTest::Allocate));
        Host::active->nullMalloc = mode == 1;
        ClockGroupCopyAnimationInvokeTest::mode = mode == 1 ? 1 : 0;
        // Execute the existing whole FF1120 body with the full requested ID.
        // Only its established qualified heap leaf is routed to owned memory.
        const auto result = AnimationAssociationStorageDetail::InvokeGroupCopy(function, physicalGroup, animation, returned, exception);
        Host::active->nullMalloc = false;
        ClockGroupCopyAnimationInvokeTest::mode = 0;
        if (!route.Restore() || PrivateHook::heapFailed)
            return ClockResult::WriteFailure;
        if (result != ClockResult::Ok)
            return result;
        if (mode == 2)
        {
            exception = EXCEPTION_ACCESS_VIOLATION;
            return ClockResult::OriginalException;
        }
        if (mode == 3)
            PrivateHook::Store(image + NativeBorrowedGcProfile::markerRva, LONG{1});
        if (mode == 4)
            runtime.privateAssociations.CloseFacadeIssuance(runtime.generation.load());
        return result;
    }
}

namespace NumericAssociationOwnerFixture
{
    namespace Hook = ClockNumericAssociationIssuedTest;
    namespace PrivateHook = ClockPrivateAnimationAssociationTest;
    namespace Host = NativeCustomAnimSequenceStorageFixture;
    unsigned scenario = 0;
    void     RunPrepared(ClockImageFixture& fixture)
    {
        WorldGroundOwnerFixture::Writes saved{fixture, {}};
        const auto                      admit = [&](const auto& pins)
        {
            for (const auto& pin : pins)
                WorldGroundOwnerFixture::Writes::Apply(&saved, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        };
        admit(NativeStreamingLoadAllProfile::Code);
        for (const auto& pin : NativeStartupOwnerProfile::pins)
            WorldGroundOwnerFixture::Writes::Apply(&saved, pin.rva, pin.bytes, pin.size, pin.code ? PAGE_EXECUTE_READ : PAGE_READONLY);
        for (const auto& pin : NativeStartupWorldProfile::pins)
            WorldGroundOwnerFixture::Writes::Apply(&saved, pin.rva, pin.bytes, pin.size, pin.code ? PAGE_EXECUTE_READ : PAGE_READONLY);
        admit(NativeUObjectMembershipProfile::pins);
        for (const auto& pin : NativeGameStateProfile::pins)
            WorldGroundOwnerFixture::Writes::Apply(&saved, pin.rva, pin.bytes, pin.size, pin.code ? PAGE_EXECUTE_READ : PAGE_READONLY);
        admit(NativeBorrowedGcProfile::pins);
        for (const auto& pin : NativeStartGameProfile::Code)
            if (pin.rva == 0xab879c || pin.rva == 0xab92f0 || pin.rva == 0x1af4ef0)
                WorldGroundOwnerFixture::Writes::Apply(&saved, pin.rva, pin.bytes.data(), pin.size, PAGE_EXECUTE_READ);
        alignas(std::uintptr_t) std::array<unsigned char, 0x180> root{};
        NativeStreamingLoadAllFixture::Graph graph(fixture.image, reinterpret_cast<std::uintptr_t>(root.data()), &WorldGroundOwnerFixture::Writes::Apply,
                                                   &saved);
        NativePedClothesFixture::Install(fixture.image, &WorldGroundOwnerFixture::Writes::Apply, &saved);
        NativePedClothesFixture::NameState names;
        names.Reset(fixture.image, &WorldGroundOwnerFixture::Writes::Apply, &saved);
        Host::State host;
        host.Reset(fixture.image, &WorldGroundOwnerFixture::Writes::Apply, &saved);
        Host::active = &host;
        // The direct fixture does not run the old AnimationBlocks wrapper.
        // Its read-only bank foundation still validates these exact pool pins.
        admit(NativePedPoolProfile::Code);
        NativeAnimationBlockFactoryFixture::Install(fixture.image, &WorldGroundOwnerFixture::Writes::Apply, &saved);
        admit(NativeAnimationBlockProfile::Code);
        // These genuine dispatch cells share executable text pages. Match the
        // established bank fixture preparation without replaying its cases.
        saved.Global(NativeAnimationBlockProfile::RemapRva + NativeAnimationBlockProfile::LoadedOpcodeIndex,
                     static_cast<unsigned char>(NativeAnimationBlockProfile::LoadedSelector), PAGE_EXECUTE_READ);
        saved.Global(NativeAnimationBlockProfile::DispatchRva + NativeAnimationBlockProfile::LoadedSelector * 4,
                     static_cast<std::uint32_t>(NativeAnimationBlockProfile::LoadedBranchRva), PAGE_EXECUTE_READ);
        saved.Global(NativeAnimationBlockProfile::GroupSlotRva, fixture.image + NativeAnimationBlockProfile::GroupRva, PAGE_READONLY);
        admit(NativeAnimationBlockRefProfile::Code);
        admit(NativeAnimationGroupArrayProfile::Code);
        admit(NativeAnimationStaticAssociationProfile::Code);
        admit(NativeAnimationStaticAssociationProfile::ReadCode);
        admit(NativeAnimationAssociationProfile::Code);
        // The independent H facade uses its own saved read-profile pins.
        admit(NativeAnimationHierarchyReadProfile::Code);
        saved.Global(NativeAnimationAssociationProfile::VtableRva, fixture.image + NativeAnimationAssociationProfile::DestructorRva, PAGE_READONLY);
        PrivateAnimationAssociationOwnerFixture::SourceBank source(fixture, saved);
        admit(NativeAnimationAssociationGroupCopyProfile::Code);
        saved.Global(source.hierarchy - fixture.image + 0x13, static_cast<unsigned char>(1));
        saved.Global(source.hierarchy - fixture.image + 0x18, std::uint32_t{0x3f800000});
        graph.Reset();
        saved.Global(NativeGameStateProfile::stateRva, std::uint32_t{9});
        saved.Global(NativeStartGameProfile::ThreadInitializedRva, static_cast<unsigned char>(1));
        saved.Global(NativeStartGameProfile::ThreadIdRva, GetCurrentThreadId());
        saved.Global(NativeBorrowedGcProfile::markerRva, LONG{0});
        const auto previousImage = ClockReaderTestHost::image;
        ClockReaderTestHost::image = fixture.image;
        PrivateHook::image = fixture.image;
        Hook::image = fixture.image;
        Hook::parent = source.groups;
        Hook::counter = source.counter;
        Hook::calls = 0;
        Hook::mode = 0;
        Hook::targetsValid = true;
        PrivateHook::constructorCalls = PrivateHook::destructorCalls = PrivateHook::exchangeCalls = PrivateHook::heapAllocations = PrivateHook::heapFrees = 0;
        PrivateHook::targetsValid = PrivateHook::snapshotsCopied = true;
        PrivateHook::heapFailed = false;
        PrivateHook::mode = 0;
        WorldGroundOwnerFixture::Callback callback;
        NativeClockService                service;
        callback.action = [](NativeGameTimingAccess&) {};
        ClockServiceStateV1 rejected, admitted;
        Check(!service.BeginGameTiming({}, rejected, &WorldGroundOwnerFixture::Callback::Invoke, &callback) &&
                  rejected.failure == ClockResult::UnsupportedBuild && !rejected.bound && !rejected.hooked &&
                  !NativeServiceAdmission::IsClaimed(NativeServiceAdmission::Category::Clock),
              "default shipping Begin rejects this executable and releases its failed epoch");
        fixture.Seed();
        // The rejected generation is permanently closed. A second actual
        // public Begin issues the new epoch; no whitebox flag reopens it.
        Hook::admittedImage = fixture.image;
        const bool began = service.BeginGameTiming({}, admitted, &WorldGroundOwnerFixture::Callback::Invoke, &callback);
        Hook::admittedImage = 0;
        Check(began && admitted.generation > rejected.generation && admitted.bound && admitted.modulePinned &&
                  NativeServiceAdmission::IsClaimed(NativeServiceAdmission::Category::Clock),
              "fresh public Begin owns the qualified synthetic image, real hook transaction and new admission epoch");
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
                    std::fprintf(stderr, "Group copy action exception: %s; copies=%u entered=%u returned=%u verified=%u\n", error.what(), Hook::calls,
                                 runtime.animationGroupCopy.entered, runtime.animationGroupCopy.returned, runtime.animationGroupCopy.verified);
                    Check(false, "actual group copy callback must not throw");
                }
            };
            Check(service.Renew(generation), "renew one actual group-copy owner callback");
            StaticAssociationInitializationOwnerFixture::ProducerPulse pulse(service, generation);
            Check(pulse.renewals.load() && !pulse.unexpectedFailures.load(), "copy callback has a real independent producer before native entry");
            fixture.invokeTimer();
            pulse.Stop();
            Check(!pulse.unexpectedFailures.load() && runtime.producerThread == GetCurrentThreadId(),
                  "copy callbacks preserve the unchanged producer/engine handoff policy");
            callback.action = [](NativeGameTimingAccess&) {};
        };
        NativeAnimationAssociationGroupReferenceToken group;
        action(
            [&]
            {
                Check(runtime.access.ResolveAnimationAssociationGroup(generation, 0, group) == ClockResult::Ok && group.locator == source.groups,
                      "actual runtime issues the current physical group despite its zero Created count");
                Hook::group = group;
            });
        Hook::active = true;
        // The physical group1 shares genuine source row0. Its DWORD offset
        // makes the high-bit request select that row without truncating IDs or
        // requiring the row's independent WORD animation metadata to equal it.
        std::memcpy(reinterpret_cast<void*>(source.groups + 32), reinterpret_cast<void*>(source.groups), 32);
        PrivateHook::Store(source.groups + 32 + 0x14, Hook::Animation);
        PrivateHook::Store(source.children + 0x0a, std::int16_t{17});
        saved.Global(NativeAnimationGroupArrayProfile::CountRva, std::int32_t{2});
        action(
            [&]
            {
                Check(runtime.access.ResolveAnimationAssociationGroup(generation, 1, group) == ClockResult::Ok && group.locator == source.groups + 32,
                      "numeric factory uses a real current physical group1 with unchanged Created0 and row0 extent");
                Hook::group = group;
            });
        AnimationAssociationRawFactoryManagerReceiver manager(runtime.access, generation);
        CAnimManager&                                 sdk = manager;
        std::unique_ptr<CAnimBlendAssociation>        product;
        std::unique_ptr<CAnimBlendHierarchy>          child;
        const auto                                    refuses = [&](const auto& operation, const char* label)
        {
            bool rejected = false;
            try
            {
                operation();
            }
            catch (const std::runtime_error&)
            {
                rejected = true;
            }
            Check(rejected, label);
        };
        const auto finish = [&]
        {
            bool done = service.End();
            for (unsigned attempt = 0; attempt != 5 && !done; ++attempt)
            {
                fixture.invokeTimer();
                done = service.End();
            }
            return done;
        };
        if (scenario == 0)
        {
            // Raw static construction needs one unambiguous parent interval.
            // The numeric regression below deliberately exposes two aliases
            // and selects its group explicitly; restore that source afterward.
            saved.Global(NativeAnimationGroupArrayProfile::CountRva, std::int32_t{1});
            NativeAnimationAssociationReferenceToken rawToken;
            std::uintptr_t                           rawAddress = 0;
            std::unique_ptr<CAnimBlendAssociation>   firstRaw, secondRaw;
            const auto                               freesBeforeRaw = host.freeCalls;
            action(
                [&]
                {
                    Check(!sdk.GetAnimBlendAssociation(nullptr), "raw factory preserves source null without native entry");
                    Check(runtime.access.CreatePrivateAnimationAssociation(generation, rawToken) == ClockResult::Ok && rawToken.serial,
                          "actual owner issues one acknowledged unconstructed receiver before raw factory use");
                    Check(runtime.access.ReadPrivateAnimationAssociationInterface(rawToken, rawAddress) == ClockResult::Ok && rawAddress,
                          "identity channel returns the acknowledged unconstructed outer allocation");
                    // Uninitialized native bytes are not a vptr or metadata
                    // contract. The ledger's allocation ACK supplies identity.
                    PrivateHook::Store(rawAddress, std::uintptr_t{0xbad00bad});
                    const auto mallocs = host.mallocCalls;
                    firstRaw = sdk.GetAnimBlendAssociation(reinterpret_cast<CAnimBlendAssociationSAInterface*>(rawAddress));
                    secondRaw = sdk.GetAnimBlendAssociation(reinterpret_cast<CAnimBlendAssociationSAInterface*>(rawAddress));
                    Check(firstRaw && secondRaw && firstRaw.get() != secondRaw.get() &&
                              reinterpret_cast<std::uintptr_t>(firstRaw->GetInterface()) == rawAddress &&
                              reinterpret_cast<std::uintptr_t>(secondRaw->GetInterface()) == rawAddress && host.mallocCalls == mallocs && !Hook::calls,
                          "two actual raw facades share the unconstructed Entry without native allocation or copy");
                    refuses([&] { (void)firstRaw->GetAnimID(); }, "unconstructed identity does not authorize scalar reads");
                    refuses([&] { (void)sdk.GetAnimBlendAssociation(reinterpret_cast<CAnimBlendAssociationSAInterface*>(std::uintptr_t{1})); },
                            "unreadable foreign raw tag refuses without dereference or adoption");
                    NativeAnimationAssociationIssuedCopy occupied;
                    occupied.interfaceAddress = 0x76543210;
                    Check(runtime.access.ResolvePrivateAnimationAssociationInterface(generation, rawAddress, occupied) == ClockResult::InvalidArgument &&
                              occupied.interfaceAddress == 0x76543210 && !occupied.token.serial && !occupied.code.IsIssued(),
                          "occupied caller output is preserved without another code view");
                    NativeAnimationAssociationIssuedCopy stale;
                    Check(runtime.access.ResolvePrivateAnimationAssociationInterface(generation - 1, rawAddress, stale) == ClockResult::OwnerConflict &&
                              !stale.interfaceAddress && !stale.token.serial && !stale.code.IsIssued(),
                          "current allocation cannot renew an obsolete runtime generation");
                    const auto returned = firstRaw->Constructor(*reinterpret_cast<CAnimBlendStaticAssociationSAInterface*>(source.children));
                    Check(reinterpret_cast<std::uintptr_t>(returned) == rawAddress && static_cast<short>(secondRaw->GetAnimID()) == 17 &&
                              PrivateHook::Load<std::uint16_t>(source.counter) == 18,
                          "real copied constructor initializes the same Entry observed through the second facade");
                    Check(runtime.access.RetirePrivateAnimationAssociation(rawToken) == ClockResult::Ok && host.freeCalls == freesBeforeRaw + 2 &&
                              PrivateHook::Load<std::uint16_t>(source.counter) == 17 && !runtime.privateAssociations.HasPendingNative() &&
                              runtime.privateAssociations.HasPendingFacadeCode(),
                          "explicit retirement frees native resources once while raw facades retain code");
                    refuses([&] { (void)firstRaw->GetInterface(); }, "retired facade cannot reuse its old serial as current identity");
                    refuses([&] { (void)sdk.GetAnimBlendAssociation(reinterpret_cast<CAnimBlendAssociationSAInterface*>(rawAddress)); },
                            "retired outer address cannot issue a new facade");
                });
            const auto freesAfterRaw = host.freeCalls;
            firstRaw.reset();
            secondRaw.reset();
            Check(host.freeCalls == freesAfterRaw && runtime.privateAssociations.HasPendingFacadeCode(),
                  "raw destructors drop code counts only, without pruning active destructor frames");
            action([] {});
            Check(!runtime.privateAssociations.HasPendingFacadeCode() && !runtime.privateAssociations.HasPendingNative() && host.freeCalls == freesAfterRaw,
                  "later creator callback prunes raw code debt without another native destructor");
            saved.Global(NativeAnimationGroupArrayProfile::CountRva, std::int32_t{2});
            std::puts("Production issued raw association actual factory and unconstructed lifetime fixture completed");
        }
        if (scenario == 0 || scenario == 6 || scenario == 7)
        {
            action(
                [&]
                {
                    product = sdk.CreateAnimAssociation(1, Hook::Animation);
                    Check(product && dynamic_cast<CAnimBlendAssociationDE*>(product.get()) && static_cast<short>(product->GetAnimID()) == 17,
                          "actual MTA numeric factory returns its real concrete facade for the same whole-copy issued record");
                    const auto address = reinterpret_cast<std::uintptr_t>(product->GetInterface());
                    const auto aliasesBefore = host.mallocCalls;
                    auto       alias = sdk.GetAnimBlendAssociation(reinterpret_cast<CAnimBlendAssociationSAInterface*>(address));
                    Check(alias && alias.get() != product.get() && alias->GetInterface() == product->GetInterface() &&
                              static_cast<short>(alias->GetAnimID()) == 17 && host.mallocCalls == aliasesBefore && Hook::calls == 1,
                          "raw factory aliases the numeric product's existing Entry without copy or native reference");
                    Check(address && host.Contains(address) && runtime.animationGroupCopy.returnedInterface == address &&
                              runtime.animationGroupCopy.rawIdOffset == Hook::Animation && runtime.animationGroupCopy.animationId == Hook::Animation,
                          "full32 animation request reaches original copy while independent source WORD metadata remains17");
                    Check(Hook::calls == 1 && Hook::targetsValid && Hook::nestedBusy && Hook::gateBusy && Hook::lifetimeBusy &&
                              runtime.privateAssociations.HasPendingNative() && runtime.privateAssociations.HasPendingFacadeCode() &&
                              PrivateHook::Load<std::uint16_t>(source.counter) == 18,
                          "same actual Entry reserves native and CPU code authority before the single original call");
                    if (scenario != 7)
                    {
                        child = product->GetAnimHierarchy();
                        Check(child && reinterpret_cast<std::uintptr_t>(child->GetInterface()) == source.hierarchy && child->GetTotalTime() == 1.0f,
                              "independent actual H child inherits code lifetime without acquiring another native reference");
                    }
                });
            const auto frees = host.freeCalls;
            if (scenario == 6)
            {
                child.reset();
                std::thread foreign([owned = std::move(product)]() mutable { owned.reset(); });
                foreign.join();
            }
            else
            {
                product.reset();
            }
            Check(host.freeCalls == frees && PrivateHook::Load<std::uint16_t>(source.counter) == 18 && runtime.privateAssociations.HasPendingNative() &&
                      runtime.privateAssociations.HasPendingFacadeCode(),
                  "base unique_ptr deletion drops CPU code bookkeeping only and never frees original nodes or reference");
            if (scenario == 7)
            {
                Check(finish() && !service.IsRetained() && host.freeCalls == frees + 2 && PrivateHook::Load<std::uint16_t>(source.counter) == 17,
                      "creator pump independently drains last CPU view and retires the still-live native record once");
            }
            else
            {
                Check(!finish() && !runtime.privateAssociations.HasPendingNative() && runtime.privateAssociations.HasPendingFacadeCode() &&
                          service.IsRetained() && host.freeCalls == frees + 2 && PrivateHook::Load<std::uint16_t>(source.counter) == 17,
                      "Stop retires native storage once while independent child or foreign quarantine keeps the code fence");
                const auto retiredFrees = host.freeCalls;
                if (scenario == 0)
                {
                    refuses([&] { (void)child->GetInterface(); }, "a retained H CPU object cannot bypass the ended native owner");
                    child.reset();
                    Check(runtime.privateAssociations.HasPendingFacadeCode() && host.freeCalls == retiredFrees,
                          "last hierarchy destructor cannot prune its own active code frame or replay native retirement");
                    Check(finish() && !service.IsRetained() && !runtime.privateAssociations.HasPendingFacadeCode() && host.freeCalls == retiredFrees,
                          "later actual creator callback releases the final code fence without a second native destructor");
                }
                else
                {
                    Check(!finish() && runtime.privateAssociations.HasPendingFacadeCode() && host.freeCalls == retiredFrees,
                          "joined foreign deletion remains conservatively quarantined without inventing a code-quiescence ACK");
                }
            }
            std::puts(scenario == 6   ? "Production numeric association foreign code quarantine fixture completed"
                      : scenario == 7 ? "Production numeric association CPU-first lifetime fixture completed"
                                      : "Production numeric association actual manager and independent child lifetime fixture completed");
        }
        else if (scenario == 1)
        {
            Hook::mode = 1;
            action([&] { product = sdk.CreateAnimAssociation(1, Hook::Animation); });
            Check(!product && Hook::calls == 1 && runtime.animationGroupCopy.returnedNull && !runtime.privateAssociations.HasPendingNative() &&
                      !runtime.privateAssociations.HasPendingFacadeCode() && PrivateHook::Load<std::uint16_t>(source.counter) == 17 && !host.freeCalls,
                  "actual original allocation-null is a null MTA product with no fabricated token or CPU code view");
            Check(finish(), "known-null numeric factory leaves no native or code debt at End");
            std::puts("Production numeric association actual nullable factory fixture completed");
        }
        else if (scenario == 4)
        {
            runtime.privateAssociations.CloseFacadeIssuance(generation);
            action(
                [&]
                {
                    refuses([&] { product = sdk.CreateAnimAssociation(1, Hook::Animation); }, "closed generation refuses public factory before original entry");
                    refuses([&] { (void)sdk.GetAnimBlendAssociation(reinterpret_cast<CAnimBlendAssociationSAInterface*>(std::uintptr_t{1})); },
                            "closed generation cannot mint a raw alias");
                });
            Check(!product && !Hook::calls && !host.mallocCalls && !runtime.privateAssociations.IsRetained(),
                  "monotonic issuance closure has no allocation, token adoption or native side effect");
            Check(finish(), "closed empty generation retires normally");
            std::puts("Production numeric association pre-entry closure fixture completed");
        }
        else
        {
            Hook::mode = scenario == 2 ? 2 : scenario == 3 ? 3 : 4;
            const auto frees = host.freeCalls;
            action(
                [&]
                {
                    refuses([&] { product = sdk.CreateAnimAssociation(1, Hook::Animation); }, "entered failure or closure cannot publish a numeric facade");
                    Check(!product && Hook::calls == 1 && runtime.privateAssociations.HasPendingNative() &&
                              !runtime.privateAssociations.HasPendingFacadeCode() && PrivateHook::Load<std::uint16_t>(source.counter) == 18,
                          "inside the original callback the unpublished copy still belongs to its actual native ledger without a CPU view");
                });
            if (scenario == 3)
                PrivateHook::Store(fixture.image + NativeBorrowedGcProfile::markerRva, LONG{0});
            if (scenario == 5)
            {
                // The post-handler cleanup may already retire this verified
                // copy. Compare with the pre-call count, not a stale pending
                // assumption after returning from the actual callback.
                Check(finish() && !product && host.freeCalls == frees + 2 && PrivateHook::Load<std::uint16_t>(source.counter) == 17,
                      "closure during original Copy retires the known unpublished resource exactly once");
                std::puts("Production numeric association in-flight closure fixture completed");
            }
            else
            {
                Check(!product && !finish() && service.IsRetained() && host.freeCalls == frees && runtime.privateAssociations.HasPendingNative(),
                      "unknown native entry or lost owner ACK never guesses cleanup or publishes a code view");
                std::puts("Production numeric association entered uncertainty fixture completed");
            }
        }
        Hook::active = false;
        ClockReaderTestHost::image = previousImage;
        Hook::image = PrivateHook::image = 0;
        Host::active = nullptr;
        saved.Restore();
    }
}
