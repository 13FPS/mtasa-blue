/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Exact original group-copy admission and existing issued retirement
 ****************************************************************************/
namespace ClockGroupCopyAnimationInvokeTest
{
    namespace Host = NativeCustomAnimSequenceStorageFixture;
    namespace PrivateHook = ClockPrivateAnimationAssociationTest;
    namespace Calls = NativeAnimationAssociationCalls;
    std::uintptr_t                                image = 0, parent = 0, counter = 0;
    unsigned                                      calls = 0, mode = 0;
    bool                                          targetsValid = true, nestedBusy = false, gateBusy = false, lifetimeBusy = false;
    NativeAnimationAssociationGroupReferenceToken group{};
    std::uintptr_t                                Allocate(std::uintptr_t heap, std::size_t bytes, std::uint32_t alignment) noexcept
    {
        std::uintptr_t returned = 0;
        std::uint32_t  exception = 0;
        if (!Host::active || Host::active->Malloc(PrivateHook::originalAllocate, heap, bytes, alignment, returned, exception) != ClockResult::Ok)
            std::abort();
        // Only the real FF1120 outer-allocation null branch is injected. The
        // constructor's unchecked inner allocation must remain nonnull.
        if (!returned && mode != 1 && mode != 2)
            std::abort();
        return returned;
    }
    ClockResult Invoke(Calls::GroupCopyFunction function, std::int32_t physicalGroup, std::uint32_t animation, std::uintptr_t& returned,
                       std::uint32_t& exception) noexcept
    {
        ++calls;
        targetsValid = targetsValid && reinterpret_cast<std::uintptr_t>(function) == image + 0xff1120 && physicalGroup == 0 && animation == 0;
        std::uintptr_t nested = 0;
        nestedBusy = runtime.access.CopyAnimationAssociationGroup(group, 0, nested) == ClockResult::Busy;
        const bool entered = NativeStreamingCompletionContext::TryEnter();
        gateBusy = !entered;
        if (entered)
            NativeStreamingCompletionContext::Leave();
        lifetimeBusy = runtime.privateAssociations.IsRetained() && NativeLifetimeRetained();
        // The whole copy entry performs its first allocation internally, so
        // unlike Create+Construct there is no prior host Malloc to record the
        // receiver. Route only the actual current qualified native heap.
        NativeModelReferenceHeap::Value heap;
        if (!Host::active || NativeModelReferenceHeap::Capture(image, heap) != ClockResult::Ok)
            return ClockResult::BindingChanged;
        Host::active->heap = heap.current;
        PrivateHook::HeapRoute route;
        if (!targetsValid || !route.Begin())
            return ClockResult::WriteFailure;
        PrivateHook::Store(route.table + 0x10, reinterpret_cast<std::uintptr_t>(&Allocate));
        Host::active->nullMalloc = mode == 1 || mode == 2;
        const auto result = AnimationAssociationStorageDetail::InvokeGroupCopy(function, physicalGroup, animation, returned, exception);
        Host::active->nullMalloc = false;
        if (!route.Restore() || PrivateHook::heapFailed)
            return ClockResult::WriteFailure;
        if (result != ClockResult::Ok)
            return result;
        if (mode == 3 && returned)
            PrivateHook::Store(returned + 0x18, std::uint16_t{7});
        if (mode == 4 && returned)
            PrivateHook::Store(counter, std::uint16_t{17});
        if (mode == 5)
        {
            exception = EXCEPTION_ACCESS_VIOLATION;
            return ClockResult::OriginalException;
        }
        if (mode == 2 || mode == 6)
            PrivateHook::Store(image + NativeBorrowedGcProfile::markerRva, LONG{1});
        if (mode == 7)
            PrivateHook::Store(parent + 0x14, std::uint32_t{1});
        return result;
    }
}
namespace AnimationAssociationGroupCopyOwnerFixture
{
    namespace Hook = ClockGroupCopyAnimationInvokeTest;
    namespace PrivateHook = ClockPrivateAnimationAssociationTest;
    namespace Host = NativeCustomAnimSequenceStorageFixture;
    namespace Calls = NativeAnimationAssociationCalls;
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
        if (scenario)
        {
            // Each retained original effect owns a fresh process. Never reset
            // sticky module pins or native debt to fake another pristine Begin.
            Hook::mode = scenario + 1;
            std::uintptr_t          output = 0x1234;
            Calls::GroupCopyReceipt receipt;
            action(
                [&]
                {
                    const auto result = runtime.access.CopyAnimationAssociationGroup(group, 0, output);
                    receipt = runtime.animationGroupCopy;
                    Check(result != ClockResult::Ok && output == 0x1234 && receipt.entered && !receipt.verified && Hook::calls == 1,
                          "entered refused copy preserves caller output and exact original-call receipt");
                });
            PrivateHook::Store(fixture.image + NativeBorrowedGcProfile::markerRva, LONG{0});
            Check(Hook::targetsValid && Hook::nestedBusy && Hook::gateBusy && Hook::lifetimeBusy,
                  "copy uses the actual serialized controller, shared native gate and retained module owner");
            const auto copies = Hook::calls, frees = host.freeCalls;
            if (scenario == 1)
            {
                Check(receipt.returned && receipt.returnedNull && !receipt.returnedInterface && !receipt.resources.allocated &&
                          !receipt.resources.constructed && !runtime.privateAssociations.IsRetained() && PrivateHook::Load<std::uint16_t>(source.counter) == 17,
                      "acknowledged nullable original return owns no resource even after an owner acknowledgement fails");
                // Hook teardown is separate from native allocation debt.
                (void)service.End();
                fixture.invokeTimer();
                Check(service.End() && !service.IsRetained() && host.freeCalls == frees && Hook::calls == copies,
                      "null owner loss may finish ordinary hook teardown without replay or native free");
                std::puts("Production group copy animation nullable authority fixture completed");
            }
            else
            {
                Check(runtime.privateAssociations.IsRetained() && runtime.privateAssociations.RecoveryPending() && NativeLifetimeRetained() && !service.End() &&
                          host.freeCalls == frees && PrivateHook::destructorCalls == 0,
                      "unknown copy or unverified returned nonnull keeps the original outer/node/ref journal and blocks unconstructed free");
                std::uintptr_t untouched = 0x5678;
                Check(runtime.access.CopyAnimationAssociationGroup(group, 0, untouched) != ClockResult::Ok && untouched == 0x5678 && Hook::calls == copies &&
                          host.freeCalls == frees,
                      "a stopped retained copy never replays its original call or frees an unverified allocation");
                if (scenario == 4)
                    Check(!receipt.returned && !receipt.returnedNull && !receipt.resources.constructed,
                          "missing original return acknowledgement does not infer null or constructor completion");
                else
                    Check(receipt.returned && receipt.returnedInterface && receipt.resources.allocated && !receipt.returnedNull,
                          "causally returned nonnull is journaled before layout, reference, source or owner postchecks");
                std::puts("Production group copy animation retained failures without replay fixture completed");
            }
        }
        else
        {
            // Refusals precede the sole native entry and own no new resource.
            action(
                [&]
                {
                    const auto calls = Hook::calls;
                    auto       wrong = group;
                    wrong.ownerGeneration = 0;
                    std::uintptr_t output = 0x1234;
                    Check(runtime.access.CopyAnimationAssociationGroup(wrong, 0, output) != ClockResult::Ok && Hook::calls == calls && output == 0x1234,
                          "wrong group owner is refused before original copy entry");
                    PrivateHook::Store(source.hierarchy + 0x18, std::uint32_t{0});
                    Check(runtime.access.CopyAnimationAssociationGroup(group, 0, output) == ClockResult::OwnerConflict && Hook::calls == calls &&
                              output == 0x1234,
                          "unqualified compressed hierarchy work is refused rather than decompressed or guessed");
                    PrivateHook::Store(source.hierarchy + 0x18, std::uint32_t{0x3f800000});
                    PrivateHook::Store(source.children + 0xe, std::uint16_t{0x4010});
                    Check(runtime.access.CopyAnimationAssociationGroup(group, 0, output) == ClockResult::InvalidArgument && Hook::calls == calls,
                          "a source with an existing ownership flag cannot fabricate another reference increment");
                    PrivateHook::Store(source.children + 0xe, std::uint16_t{0x10});
                });
            action(
                [&]
                {
                    Hook::mode = 1;
                    const auto     allocations = host.allocationCount;
                    std::uintptr_t output = 0x1234;
                    Check(runtime.access.CopyAnimationAssociationGroup(group, 0, output) == ClockResult::Ok && !output &&
                              runtime.animationGroupCopy.returnedNull && runtime.animationGroupCopy.verified && !runtime.privateAssociations.IsRetained() &&
                              host.allocationCount == allocations && PrivateHook::Load<std::uint16_t>(source.counter) == 17,
                          "the exact original null allocation branch is a successful typed nullable result with no outer, nodes or ref");
                    Hook::mode = 0;
                });
            Calls::GroupCopyReceipt receipt;
            std::uintptr_t          output = 0;
            action(
                [&]
                {
                    AnimationAssociationGroupReceiver receiver(runtime.access, group);
                    output = reinterpret_cast<std::uintptr_t>(receiver.CopyAnimation(0));
                    receipt = runtime.animationGroupCopy;
                    Check(output && receipt.returnedInterface == output && receipt.issued.ownerGeneration == generation && receipt.issued.serial &&
                              receipt.selected.locator == source.children && receipt.rawIdOffset == 0 && receipt.sourcePrepared && receipt.hierarchyWarm &&
                              receipt.entered && receipt.returned && !receipt.returnedNull && receipt.verified && receipt.resources.constructed &&
                              receipt.resources.refVerified && receipt.resources.allocated && runtime.privateAssociations.IsRetained() &&
                              !runtime.privateAssociations.RecoveryPending() && PrivateHook::Load<std::uint16_t>(source.counter) == 18,
                          "actual SDK raw interface transport reuses one original copied allocation and the existing issued native owner");
                    Check(host.Contains(output) && host.Contains(receipt.resources.nodeRaw) && receipt.resources.nodeCount == 5 &&
                              receipt.resources.nodeRequestBytes == 320 && receipt.resources.nodes % 64 == 0 &&
                              PrivateHook::Load<std::uintptr_t>(receipt.resources.nodes - 8) == receipt.resources.nodeRaw,
                          "original constructor and node helper produce the current heap-backed aligned node header");
                    Check(PrivateHook::Load<unsigned char>(output + 0x1c) == 0xa5 && PrivateHook::Load<unsigned char>(output + 0x50) == 0xa5 &&
                              PrivateHook::Load<unsigned char>(output + 0x61) == 0xa5,
                          "copy acceptance preserves constructor-undefined padding and callback payload instead of zeroing native storage");
                    Check(Hook::targetsValid && Hook::nestedBusy && Hook::gateBusy && Hook::lifetimeBusy,
                          "the real copy call is admitted once under existing native scope and serialized controller ownership");
                });
            action(
                [&]
                {
                    NativeAnimationAssociationValues values;
                    Check(runtime.access.ReadPrivateAnimationAssociation(receipt.issued, values) == ClockResult::Ok && values.interfaceAddress == output &&
                              values.animId == -32768 && values.animGroup == -1 && values.blendAmountBits == 0x3f800000 && values.speedBits == 0x3f800000,
                          "causal copied return joins the existing metadata observer without a new raw-pointer factory");
                    Check(runtime.access.SetPrivateAnimationAssociationAnimID(receipt.issued, 321) == ClockResult::Ok &&
                              runtime.access.ReadPrivateAnimationAssociation(receipt.issued, values) == ClockResult::Ok && values.animId == 321,
                          "verified copied return uses the existing issued scalar mutation path");
                    const auto frees = host.freeCalls;
                    Check(runtime.access.RetirePrivateAnimationAssociation(receipt.issued) == ClockResult::Ok && !runtime.privateAssociations.IsRetained() &&
                              host.freeCalls == frees + 2 && PrivateHook::Load<std::uint16_t>(source.counter) == 17,
                          "existing exact deleting destructor retires copied outer and original node header once and balances its reference");
                });
            // A second successful zero-count copy exercises the original
            // mandatory64-byte node-header allocation and End's existing drain.
            action(
                [&]
                {
                    source.ResetRow(0);
                    std::uintptr_t zero = 0;
                    Check(runtime.access.CopyAnimationAssociationGroup(group, 0, zero) == ClockResult::Ok && zero &&
                              runtime.animationGroupCopy.resources.nodeCount == 0 && runtime.animationGroupCopy.resources.nodeRequestBytes == 64,
                          "zero-count copy keeps the actual original aligned node-header allocation");
                });
            const auto copies = Hook::calls, frees = host.freeCalls;
            Check(!service.End(), "outside-callback End cannot invent a copied native destructor acknowledgement");
            fixture.invokeTimer();
            Check(service.End() && !service.IsRetained() && !runtime.privateAssociations.IsRetained() && host.freeCalls == frees + 2 && Hook::calls == copies &&
                      PrivateHook::Load<std::uint16_t>(source.counter) == 17,
                  "the same runtime retirement epoch drains an untransferred copied record before ending hooks");
            std::puts("Production group copy animation actual owner fixture completed");
            std::puts("Production group copy animation actual SDK raw transport fixture completed");
            std::puts("Production group copy animation nullable authority fixture completed");
        }
        ClockReaderTestHost::image = previousImage;
        Hook::image = PrivateHook::image = 0;
        Host::active = nullptr;
        saved.Restore();
    }
}
