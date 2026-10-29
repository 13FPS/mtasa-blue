/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual current group replacement and causal fresh-array publication
 ****************************************************************************/
namespace ClockGroupReplacementOwnerTest
{
    namespace Calls = NativeAnimationAssociationGroupReplacementCalls;
    namespace Host = NativeCustomAnimSequenceStorageFixture;
    namespace PrivateHook = ClockPrivateAnimationAssociationTest;
    namespace Detail = AssociationGroupReplacementDetail;
    std::uintptr_t        image = 0, parent = 0;
    unsigned              retireCalls = 0, invalidations = 0, mode = 0;
    bool                  targetsValid = true, sourceChanged = false;
    std::vector<unsigned> events;
    void                  Invalidate(void*) noexcept
    {
        ++invalidations;
        events.push_back(5);
    }
    void AfterMalloc(Host::State&) noexcept
    {
        events.push_back(4);
    }
    void DriftParent() noexcept
    {
        if (ClockAnimationManagerOwnerTest::reads < 3)
        {
            ClockAnimationManagerOwnerTest::afterRead = &DriftParent;
            return;
        }
        PrivateHook::Store(parent + 0x14, std::uint32_t{73});
        sourceChanged = true;
    }
    bool BeginHeap(PrivateHook::HeapRoute& route) noexcept
    {
        NativeModelReferenceHeap::Value heap;
        if (!Host::active || NativeModelReferenceHeap::Capture(image, heap) != ClockResult::Ok)
            return false;
        // The host records calls but has no heap receiver until allocation.
        // Capture the actual current receiver, never guess from host.heap.
        Host::active->heap = heap.current;
        return route.Begin();
    }
    ClockResult Retire(Calls::RetireFunction function, std::uintptr_t group, std::uint32_t& exception) noexcept
    {
        ++retireCalls;
        events.push_back(0);
        targetsValid = targetsValid && reinterpret_cast<std::uintptr_t>(function) == image + 0xfeb580 && group == parent;
        PrivateHook::HeapRoute route;
        if (!targetsValid || !BeginHeap(route))
            return ClockResult::WriteFailure;
        const auto result = Detail::InvokeOriginal(function, group, exception);
        if (!route.Restore() || PrivateHook::heapFailed)
            return ClockResult::WriteFailure;
        if (result == ClockResult::Ok && mode == 4)
        {
            exception = EXCEPTION_ACCESS_VIOLATION;
            return ClockResult::OriginalException;
        }
        return result;
    }
    ClockResult DestroyFresh(NativeStaticAssociationArrayStorageCalls::DeletingDestructorFunction function, std::uintptr_t rows, std::uint32_t flags,
                             std::uintptr_t& returned, std::uint32_t& exception) noexcept
    {
        PrivateHook::HeapRoute route;
        if (!BeginHeap(route))
            return ClockResult::WriteFailure;
        const auto result = CustomAnimSequenceStorageDetail::NativeStaticArrayDestructor(function, rows, flags, returned, exception);
        if (!route.Restore() || PrivateHook::heapFailed)
            return ClockResult::WriteFailure;
        return result;
    }
    ClockResult Qword(std::uintptr_t address, std::uint64_t before, std::uint64_t target, std::uint64_t& observed, bool& accepted,
                      std::uint32_t& exception) noexcept
    {
        targetsValid = targetsValid && (address == parent || address == parent + 8);
        const bool child = address == parent + 8;
        events.push_back(child ? 2 : 1);
        if (child && mode == 1)
        {
            observed = before ^ 0x10000;
            accepted = false;
            return ClockResult::Ok;
        }
        const auto result = Detail::Qword(address, before, target, observed, accepted, exception);
        if (result == ClockResult::Ok && child && mode == 2)
        {
            exception = EXCEPTION_ACCESS_VIOLATION;
            return ClockResult::OriginalException;
        }
        if (result == ClockResult::Ok && child && mode == 3)
            PrivateHook::Store(image + NativeBorrowedGcProfile::markerRva, LONG{1});
        return result;
    }
    ClockResult Dword(std::uintptr_t address, std::uint32_t before, std::uint32_t target, std::uint32_t& observed, bool& accepted,
                      std::uint32_t& exception) noexcept
    {
        targetsValid = targetsValid && address == parent + 0x10;
        events.push_back(3);
        return Detail::Dword(address, before, target, observed, accepted, exception);
    }
}
namespace AnimationAssociationGroupReplacementOwnerFixture
{
    namespace Hook = ClockGroupReplacementOwnerTest;
    namespace PrivateHook = ClockPrivateAnimationAssociationTest;
    namespace Host = NativeCustomAnimSequenceStorageFixture;
    namespace Calls = NativeAnimationAssociationGroupReplacementCalls;
    namespace Arrays = NativeStaticAssociationArrayStorageCalls;
    using Record = NativeAnimationAssociationGroupReplacement;
    using Storage = NativeCustomAnimSequenceStorage;
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
        saved.Global(NativeAnimationAssociationProfile::VtableRva, fixture.image + NativeAnimationAssociationProfile::DestructorRva, PAGE_READONLY);
        PrivateAnimationAssociationOwnerFixture::SourceBank source(fixture, saved);
        // The reused raw-bank fixture deliberately fills all16 name bytes.
        // This typed factory needs a real terminated name, unlike raw ref reads.
        saved.Global(NativeAnimationBlockProfile::BankRva + 15, static_cast<unsigned char>(0));
        // Initial empty child is fixture arrangement before Begin, not a
        // production adoption or a hand-written native free.
        PrivateHook::Store(source.groups + 8, std::uintptr_t{0});
        PrivateHook::Store(source.groups + 0x10, std::uint32_t{0});
        saved.Global(NativeAnimationBlockProfile::BankRva + NativeAnimationBlockProfile::AnimationCountOffset, std::int32_t{2});
        saved.Global(NativeAnimationManagerProfile::AnimationCountRva, std::int32_t{2});
        WorldGroundOwnerFixture::Writes::Apply(&saved, NativeStaticAssociationArrayProfile::ConstructorRecipeRva,
                                               NativeStaticAssociationArrayProfile::ConstructorRecipeBytes.data(),
                                               NativeStaticAssociationArrayProfile::ConstructorRecipeBytes.size(), PAGE_EXECUTE_READ);
        WorldGroundOwnerFixture::Writes::Apply(&saved, NativeStaticAssociationArrayProfile::DeletingDestructorRva,
                                               NativeStaticAssociationArrayProfile::DeletingDestructorBytes.data(),
                                               NativeStaticAssociationArrayProfile::DeletingDestructorBytes.size(), PAGE_EXECUTE_READ);
        saved.Global(NativeStaticAssociationArrayProfile::VtableRva, fixture.image + NativeStaticAssociationArrayProfile::DeletingDestructorRva, PAGE_READONLY);
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

        Hook::retireCalls = Hook::invalidations = 0;
        Hook::events.clear();
        Hook::events.reserve(64);
        host.afterMalloc = &Hook::AfterMalloc;
        Hook::mode = 0;
        Hook::targetsValid = true;
        PrivateHook::constructorCalls = PrivateHook::destructorCalls = PrivateHook::exchangeCalls = PrivateHook::heapAllocations = PrivateHook::heapFrees = 0;
        PrivateHook::targetsValid = PrivateHook::snapshotsCopied = true;
        PrivateHook::heapFailed = false;
        PrivateHook::mode = 0;
        WorldGroundOwnerFixture::Callback callback;
        NativeClockService                service;
        callback.action = [](NativeGameTimingAccess&) {};
        BeginOwned(service, fixture, false, true, true, &WorldGroundOwnerFixture::Callback::Invoke, &callback);
        HMODULE module = nullptr;
        Check(GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN, reinterpret_cast<LPCWSTR>(&HookClockContinuation),
                                 &module) &&
                  module,
              "private association scope pins the actual callback module once for this process");
        runtime.modulePinned.store(true);
        const auto generation = runtime.generation.load();
        const auto action = [&](const auto& body)
        {
            callback.action = [&](NativeGameTimingAccess&) { body(); };
            Check(service.Renew(generation), "renew actual group replacement owner callback");
            StaticAssociationInitializationOwnerFixture::ProducerPulse pulse(service, generation);
            Check(pulse.renewals.load() && !pulse.unexpectedFailures.load(), "independent actual producer owns the unchanged renewal policy");
            fixture.invokeTimer();
            pulse.Stop();
            Check(!pulse.unexpectedFailures.load() && runtime.producerThread == GetCurrentThreadId(),
                  "replacement callbacks restore ordinary producer ownership after genuine renewal handoff");
            callback.action = [](NativeGameTimingAccess&) {};
        };
        NativeAnimationAssociationGroupReferenceToken               group;
        NativeAnimationBlockIdentity                                block;
        const NativeAnimationAssociationGroupChildBorrowInvalidator invalidator{nullptr, &Hook::Invalidate};
        action(
            [&]
            {
                Check(runtime.access.ResolveAnimationAssociationGroup(generation, 0, group) == ClockResult::Ok && group.locator == source.groups,
                      "actual runtime supplies current physical group identity for replacement");
                Check(NativeAnimationBlockFactoryMemory({fixture.image}, &CheckTimingOwner).ReadIdentity(0, block) == ClockResult::Ok,
                      "replacement receives current original block identity without native lifetime fiction");
                block.ownerGeneration = generation;
            });
        Hook::events.clear();
        if (!scenario)
        {
            action(
                [&]
                {
                    Record wrong;
                    auto   token = group;
                    token.ownerGeneration = 0;
                    Calls::Receipt receipt;
                    Check(Calls::RetireCurrentChildren(fixture.image, wrong, generation, token, &CheckTimingOwner, invalidator, &receipt, &Hook::Retire) ==
                                  ClockResult::OwnerConflict &&
                              !receipt.retireEntered && !Hook::retireCalls && !Hook::invalidations,
                          "wrong generation cannot begin old-child retirement or expire child borrows");
                    ClockAnimationManagerOwnerTest::watched = fixture.image + NativeAnimationGroupArrayProfile::GlobalRva;
                    ClockAnimationManagerOwnerTest::watchedBytes = 8;
                    ClockAnimationManagerOwnerTest::reads = 0;
                    Hook::sourceChanged = false;
                    ClockAnimationManagerOwnerTest::afterRead = &Hook::DriftParent;
                    Record     drift;
                    const auto result =
                        Calls::RetireCurrentChildren(fixture.image, drift, generation, group, &CheckTimingOwner, invalidator, &receipt, &Hook::Retire);
                    ClockAnimationManagerOwnerTest::afterRead = nullptr;
                    PrivateHook::Store(source.groups + 0x14, std::uint32_t{0});
                    Check(Hook::sourceChanged && result == ClockResult::BindingChanged && !receipt.retireEntered && !Hook::retireCalls && !Hook::invalidations,
                          "current parent drift refuses before native retirement or child-borrow invalidation");
                    ClockAnimationManagerOwnerTest::watched = 0;
                });
        }
        Record          first;
        Storage         firstStorage;
        Calls::Receipt  receipt;
        Arrays::Receipt allocation;
        const auto      publish = [&](Record& record, Storage& storage, std::uint32_t count)
        {
            Check(Calls::RetireCurrentChildren(fixture.image, record, generation, group, &CheckTimingOwner, invalidator, &receipt, &Hook::Retire) ==
                      ClockResult::Ok,
                  "replacement retires exactly the current physical old child before changing block or allocating new rows");
            Check(Calls::PublishBlock(fixture.image, record, generation, block, &CheckTimingOwner, &receipt, &Hook::Qword) == ClockResult::Ok &&
                      receipt.blockStore.accepted && receipt.blockStore.verified && receipt.fullCount == count,
                  "source-order block store is acknowledged before fresh native allocation");
            Check(Arrays::AllocateFresh(fixture.image, storage, generation, count, &CheckTimingOwner, &allocation) == ClockResult::Ok,
                  "replacement uses a causal typed fresh32 allocation with the original block full count");
        };
        action([&] { publish(first, firstStorage, 2); });
        if (scenario >= 1 && scenario <= 3)
        {
            Hook::mode = scenario;
            ClockResult result = ClockResult::Ok;
            action(
                [&]
                {
                    result = Calls::PublishChildren(fixture.image, first, generation, firstStorage, &CheckTimingOwner, &receipt, &Hook::Qword);
                    // End only the fixture's transient GC injection after the
                    // failed publication bookend has recorded its exact outcome.
                    // Neither native record state nor unknown effects are reset.
                    PrivateHook::Store(fixture.image + NativeBorrowedGcProfile::markerRva, LONG{0});
                });
            Check(result != ClockResult::Ok && receipt.childStore.entered && !receipt.childStore.verified && first.IsRetained(),
                  "entered unsuccessful child publication keeps its exact record and cannot claim a completed Create");
            PrivateHook::Store(fixture.image + NativeBorrowedGcProfile::markerRva, LONG{0});
            const auto frees = host.freeCalls;
            if (scenario == 1)
            {
                Check(receipt.childStore.returned && !receipt.childStore.accepted && firstStorage.GetState() == Storage::State::LiveUnpublished &&
                          !firstStorage.PublicationAffinity() && PrivateHook::Load<std::uintptr_t>(source.groups + 8) == 0,
                      "causal known child-CAS rejection restores only the original private unpublished allocation");
                action(
                    [&]
                    {
                        Check(Arrays::RetireFresh(fixture.image, firstStorage, generation, &CheckTimingOwner, &allocation, &Hook::DestroyFresh) ==
                                      ClockResult::Ok &&
                                  host.freeCalls == frees + 1,
                              "the rejected unpublished allocation remains retireable only through its typed native destructor");
                    });
            }
            else if (scenario == 2)
            {
                Check(!receipt.childStore.returned && firstStorage.GetState() == Storage::State::PublicationPinned && firstStorage.HasOutstandingResource() &&
                          !firstStorage.CanFreeStorage(),
                      "unacknowledged child store pins the exact allocation instead of inferring transfer from a readable group pointer");
                action(
                    [&]
                    {
                        Check(Arrays::RetireFresh(fixture.image, firstStorage, generation, &CheckTimingOwner, &allocation, &Hook::DestroyFresh) !=
                                      ClockResult::Ok &&
                                  !allocation.entered && host.freeCalls == frees,
                              "unknown child publication cannot privately free its possibly engine-owned rows");
                    });
            }
            else
            {
                Check(receipt.childStore.returned && receipt.childStore.accepted && firstStorage.GetState() == Storage::State::EnginePublished &&
                          firstStorage.CanDiscard() && !firstStorage.CanFreeStorage() &&
                          PrivateHook::Load<std::uintptr_t>(source.groups + 8) == firstStorage.IssuedTag(),
                      "accepted child acknowledgement marks engine ownership before a later lost owner postcheck");
                action(
                    [&]
                    {
                        Check(Arrays::RetireFresh(fixture.image, firstStorage, generation, &CheckTimingOwner, &allocation, &Hook::DestroyFresh) !=
                                      ClockResult::Ok &&
                                  !allocation.entered && host.freeCalls == frees,
                              "accepted engine publication cannot fall back to a private free");
                    });
            }
            const auto events = Hook::events.size();
            action(
                [&]
                {
                    Check(
                        Calls::PublishChildren(fixture.image, first, generation, firstStorage, &CheckTimingOwner, &receipt, &Hook::Qword) != ClockResult::Ok &&
                            Hook::events.size() == events,
                        "failed publication continuation cannot replay its child store");
                });
            std::puts("Production group replacement causal publication outcomes fixture completed");
        }
        else
        {
            action(
                [&]
                {
                    Check(
                        Calls::PublishChildren(fixture.image, first, generation, firstStorage, &CheckTimingOwner, &receipt, &Hook::Qword) == ClockResult::Ok &&
                            firstStorage.GetState() == Storage::State::EnginePublished && !firstStorage.CanFreeStorage(),
                        "actual accepted child store consumes private free authority before full count publication");
                    Check(Calls::PublishFullCount(fixture.image, first, generation, &CheckTimingOwner, &receipt, &Hook::Dword) == ClockResult::Ok &&
                              first.GetState() == Record::State::Published && first.IsRetained() && PrivateHook::Load<std::uint32_t>(source.groups + 0x10) == 2,
                          "full source count is published after child, while the synchronous source continuation keeps its actual CPU witness");
                    Check(Hook::events == std::vector<unsigned>({5, 1, 4, 2, 3}),
                          "first block publication invalidates its old borrow before block, allocation, child and full count");
                    Check(Calls::FinishPublished(fixture.image, first, generation, &CheckTimingOwner, &receipt) == ClockResult::Ok && first.CanDiscard(),
                          "explicit source-loop completion ends the continuation without privately destroying engine-owned rows");
                });
            if (scenario == 4)
            {
                Record unknown;
                Hook::mode = 4;
                action(
                    [&]
                    {
                        Check(
                            Calls::RetireCurrentChildren(fixture.image, unknown, generation, group, &CheckTimingOwner, invalidator, &receipt, &Hook::Retire) ==
                                    ClockResult::OriginalException &&
                                receipt.retireEntered && !receipt.retireReturned && unknown.IsRetained() && Hook::retireCalls == 1 && Hook::invalidations == 2,
                            "missing original retirement acknowledgement retains its entered record after exactly one borrow invalidation");
                    });
                const auto calls = Hook::retireCalls, frees = host.freeCalls;
                action(
                    [&]
                    {
                        Check(Calls::RetireCurrentChildren(fixture.image, unknown, generation, group, &CheckTimingOwner, invalidator, &receipt,
                                                           &Hook::Retire) != ClockResult::Ok &&
                                  Hook::retireCalls == calls && Hook::invalidations == 2 && host.freeCalls == frees,
                              "unknown FEB580 never replays or guesses another free from null parent fields");
                    });
                std::puts("Production group replacement causal publication outcomes fixture completed");
            }
            else
            {
                action(
                    [&]
                    {
                        NativeModelReferenceHeap::Value heap;
                        Check(NativeModelReferenceHeap::Capture(fixture.image, heap) == ClockResult::Ok,
                              "fixture arranges one actual host-issued direct static node");
                        std::uintptr_t node = 0;
                        std::uint32_t  exception = 0;
                        const auto     entry = heap.current == heap.authority ? NativeCustomAnimSequenceStorageProfile::AuthorityMallocRva
                                                                              : NativeCustomAnimSequenceStorageProfile::ProxyMallocRva;
                        Check(Host::Malloc(reinterpret_cast<Host::Calls::MallocFunction>(fixture.image + entry), heap.current, 16, 0, node, exception) ==
                                      ClockResult::Ok &&
                                  node,
                              "node arrangement comes from the same bounded native allocator host");
                        PrivateHook::Store(firstStorage.IssuedTag() + 0x10, node);
                    });
                Hook::events.clear();
                Record     second;
                Storage    zeroStorage;
                const auto frees = host.freeCalls;
                action(
                    [&]
                    {
                        Check(Calls::RetireCurrentChildren(fixture.image, second, generation, group, &CheckTimingOwner, invalidator, &receipt, &Hook::Retire) ==
                                      ClockResult::Ok &&
                                  receipt.retireEntered && receipt.retireReturned && receipt.oldChildrenConsumed && receipt.retireVerified &&
                                  host.freeCalls == frees + 2 && Hook::retireCalls == 1 && Hook::invalidations == 2 &&
                                  !PrivateHook::Load<std::uintptr_t>(source.groups + 8) && !PrivateHook::Load<std::uint32_t>(source.groups + 0x10),
                              "real FEB580 calls the original reverse vector destructor, frees direct node plus cookie, and clears only child/count");
                        Check(Hook::events == std::vector<unsigned>({5, 0}), "child borrows expire once immediately before genuine old-child retirement");
                        saved.Global(NativeAnimationBlockProfile::BankRva + NativeAnimationBlockProfile::AnimationCountOffset, std::int32_t{0});
                        Hook::events.clear();
                        Check(Calls::PublishBlock(fixture.image, second, generation, block, &CheckTimingOwner, &receipt, &Hook::Qword) == ClockResult::Ok &&
                                  receipt.fullCount == 0 &&
                                  Arrays::AllocateFresh(fixture.image, zeroStorage, generation, 0, &CheckTimingOwner, &allocation) == ClockResult::Ok &&
                                  Calls::PublishChildren(fixture.image, second, generation, zeroStorage, &CheckTimingOwner, &receipt, &Hook::Qword) ==
                                      ClockResult::Ok &&
                                  Calls::PublishFullCount(fixture.image, second, generation, &CheckTimingOwner, &receipt, &Hook::Dword) == ClockResult::Ok &&
                                  Calls::FinishPublished(fixture.image, second, generation, &CheckTimingOwner, &receipt) == ClockResult::Ok &&
                                  PrivateHook::Load<std::uintptr_t>(source.groups + 8) == zeroStorage.IssuedTag() &&
                                  !PrivateHook::Load<std::uint32_t>(source.groups + 0x10),
                              "full source countzero still publishes a causal nonnull zero-cookie child without a Created shortcut");
                        Check(Hook::events == std::vector<unsigned>({1, 4, 2, 3}), "countzero preserves the same block allocation child full-count order");
                    });
                Record     zeroRetire;
                const auto zeroFrees = host.freeCalls;
                action(
                    [&]
                    {
                        Check(Calls::RetireCurrentChildren(fixture.image, zeroRetire, generation, group, &CheckTimingOwner, invalidator, &receipt,
                                                           &Hook::Retire) == ClockResult::Ok &&
                                  receipt.oldChildrenConsumed && host.freeCalls == zeroFrees + 1 && Hook::retireCalls == 2 && Hook::invalidations == 3 &&
                                  !PrivateHook::Load<std::uintptr_t>(source.groups + 8),
                              "nonnull child with zero cookie/count uses original FEB580 direct-cookie free, with exactly one new invalidation");
                    });
                Check(service.End(), "ordinary clock hooks can end after the focused external source records own no private native allocation");
                std::puts("Production group replacement actual owner fixture completed");
                std::puts("Production group replacement source ordered publication fixture completed");
                std::puts("Production group replacement original vector and zero cookie retirement fixture completed");
            }
        }
        Check(Hook::targetsValid && !PrivateHook::heapFailed,
              "all original retirement and scalar entries retained exact native addresses and current heap receiver");
        ClockReaderTestHost::image = previousImage;
        Host::active = nullptr;
        Hook::image = PrivateHook::image = 0;
        ClockAnimationManagerOwnerTest::afterRead = nullptr;
        ClockAnimationManagerOwnerTest::watched = 0;
        saved.Restore();
    }
}
