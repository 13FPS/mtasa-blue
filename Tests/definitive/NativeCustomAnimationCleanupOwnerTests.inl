/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual cleanup tickets retain backend receipts through End
 ****************************************************************************/
namespace CustomAnimationCleanupOwnerFixture
{
    namespace Host = NativeCustomAnimSequenceStorageFixture;
    using View = NativeCustomAnimationCleanupView;
    using CleanupRequest = NativeCustomAnimationCleanupRequest;
    using CleanupBeginReceipt = NativeCustomAnimationCleanupBeginReceipt;
    unsigned scenario = 0;
    void     Run(ClockImageFixture& fixture)
    {
        WorldGroundOwnerFixture::Writes saved{fixture, {}};
        const auto                      admit = [&](const auto& pins)
        {
            for (const auto& pin : pins)
                WorldGroundOwnerFixture::Writes::Apply(&saved, pin.rva, pin.bytes, pin.size, pin.code ? PAGE_EXECUTE_READ : PAGE_READONLY);
        };
        for (const auto& pin : NativeStreamingLoadAllProfile::Code)
            WorldGroundOwnerFixture::Writes::Apply(&saved, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        admit(NativeStartupOwnerProfile::pins);
        admit(NativeStartupWorldProfile::pins);
        for (const auto& pin : NativeUObjectMembershipProfile::pins)
            WorldGroundOwnerFixture::Writes::Apply(&saved, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        admit(NativeGameStateProfile::pins);
        for (const auto& pin : NativeBorrowedGcProfile::pins)
            WorldGroundOwnerFixture::Writes::Apply(&saved, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
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
        for (const auto& pin : NativeAnimationSequenceRowProfile::Code)
            WorldGroundOwnerFixture::Writes::Apply(&saved, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        // Cleanup exercises the real fresh frame producers, so their existing
        // instruction profile must be installed before binding either row.
        for (const auto& pin : NativeAnimationSequenceFrameBindProfile::Code)
            WorldGroundOwnerFixture::Writes::Apply(&saved, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        NativeAnimationBlockFactoryFixture::Install(fixture.image, &WorldGroundOwnerFixture::Writes::Apply, &saved);
        for (const auto& pin : NativeAnimationHierarchyReadProfile::Code)
            WorldGroundOwnerFixture::Writes::Apply(&saved, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        WorldGroundOwnerFixture::Writes::Apply(&saved, 0xfec67a, private_hierarchy_fixture::CacheProof, sizeof(private_hierarchy_fixture::CacheProof),
                                               PAGE_EXECUTE_READ);
        graph.Reset();
        saved.Global(NativeGameStateProfile::stateRva, std::uint32_t{9});
        saved.Global(NativeStartGameProfile::ThreadInitializedRva, static_cast<unsigned char>(1));
        saved.Global(NativeStartGameProfile::ThreadIdRva, GetCurrentThreadId());
        saved.Global(NativeBorrowedGcProfile::markerRva, LONG{0});
        const auto previousImage = ClockReaderTestHost::image;
        ClockReaderTestHost::image = fixture.image;
        WorldGroundOwnerFixture::Callback callback;
        NativeClockService                service;
        callback.action = [](NativeGameTimingAccess&) {};
        BeginOwned(service, fixture, false, true, true, &WorldGroundOwnerFixture::Callback::Invoke, &callback);
        // BeginOwned replaces admission before shipping's module-pin step.
        // This scope tests that lifetime too, so pin the real callback code.
        HMODULE cleanupModule = nullptr;
        Check(GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN, reinterpret_cast<LPCWSTR>(&HookClockContinuation),
                                 &cleanupModule) &&
                  cleanupModule,
              "cleanup fixture obtains the genuine process-lifetime pin for its compiled callback module");
        runtime.modulePinned.store(true);
        const auto generation = runtime.generation.load();
        const bool clearCompletion = scenario == 0 || scenario == 3;
        const auto invoke = [&]
        {
            Check(service.Renew(generation), "renew only the actual private cleanup natural owner");
            fixture.invokeTimer();
        };
        if (!scenario)
            CustomAnimationCleanupControllerFixture::Run(fixture, generation, host, callback, invoke);
        PrivateAnimationFactoryManagerReceiver manager(runtime.access, runtime.access, runtime.access, runtime.access, generation);
        PrivateAnimationFactoryManagerReceiver observer(runtime.access, generation);
        Check(observer.HasPendingCustomAnimationCleanup(), "partial observer cannot certify cleanup provider lifetime");
        auto& memory = static_cast<NativeAnimationSequenceStorageAccess&>(runtime.access);
        if (!scenario)
        {
            std::unique_ptr<CAnimBlendHierarchy> abandoned, unrelated;
            CAnimBlendHierarchySAInterface*      abandonedTag = nullptr;
            CAnimBlendHierarchySAInterface*      unrelatedTag = nullptr;
            std::uintptr_t                       abandonedArray = 0, liveArray = 0, looseFrame = 0;
            callback.action = [&](NativeGameTimingAccess&)
            {
                abandoned = manager.GetCustomAnimBlendHierarchy();
                abandonedTag = abandoned->GetInterface();
                Check(memory.AllocateAnimationSequences(generation, 1, abandonedArray) == ClockResult::Ok,
                      "abandoned constructor candidate owns a real initialized array beside its empty private hierarchy");
            };
            invoke();
            callback.action = [&](NativeGameTimingAccess&)
            {
                unrelated = manager.GetCustomAnimBlendHierarchy();
                unrelatedTag = unrelated->GetInterface();
                Check(memory.AllocateAnimationSequences(generation, 1, liveArray) == ClockResult::Ok &&
                          memory.AllocateAnimationKeyFrames(generation, 7, looseFrame) == ClockResult::Ok,
                      "another hierarchy, owned array and loose frame remain active outside the declared cleanup group");
            };
            invoke();
            callback.action = [&](NativeGameTimingAccess&)
            {
                auto liveRow = manager.GetCustomAnimBlendSequenceAt(reinterpret_cast<CAnimBlendSequenceSAInterface*>(liveArray), 0);
                liveRow->SetBoneTag(47);
                *reinterpret_cast<unsigned char*>(looseFrame) = 0x71;
            };
            invoke();
            const auto                     beforeAbandon = host.freeCalls;
            SCustomAnimationCleanupRequest abandonedRequest;
            abandonedRequest.origin = &manager;
            abandonedRequest.hierarchy = abandoned.get();
            abandonedRequest.originalArray = reinterpret_cast<CAnimBlendSequenceSAInterface*>(abandonedArray);
            abandonedRequest.capacity = 1;
            auto orphan = manager.BeginCustomAnimationCleanup(abandonedRequest);
            Check(orphan && host.freeCalls == beforeAbandon && host.Contains(abandonedArray - 8),
                  "creator-side Begin outside native admission reserves CPU intent without reading or freeing native storage");
            orphan.reset();
            abandoned.reset();
            Check(host.freeCalls == beforeAbandon && runtime.active.load() && !runtime.stopping.load(),
                  "dropping the accepted CPU ticket and facade runs no native cleanup and does not request End");
            callback.action = [](NativeGameTimingAccess&) {};
            for (unsigned attempt = 0; attempt != 8 && host.Contains(abandonedArray - 8); ++attempt)
                invoke();
            Check(host.freeCalls == beforeAbandon + 1 && !host.Contains(abandonedArray - 8) && runtime.active.load() && !runtime.stopping.load(),
                  "bounded admitted empty-handler frames drain the abandoned group while gameplay timing remains active");
            callback.action = [&](NativeGameTimingAccess&)
            {
                bool retired = false;
                try
                {
                    static_cast<void>(manager.GetCustomAnimBlendHierarchy(abandonedTag));
                }
                catch (const std::runtime_error&)
                {
                    retired = true;
                }
                auto liveRow = manager.GetCustomAnimBlendSequenceAt(reinterpret_cast<CAnimBlendSequenceSAInterface*>(liveArray), 0);
                Check(retired && unrelated->GetInterface() == unrelatedTag && liveRow && liveRow->GetBoneTag() == 47 && host.Contains(looseFrame) &&
                          *reinterpret_cast<unsigned char*>(looseFrame) == 0x71 && host.freeCalls == beforeAbandon + 1,
                      "active pump retires the matching outer and preserves unrelated current hierarchy, row and frame authority");
                Check(memory.FreeAnimationSequences(generation, liveArray) == ClockResult::Ok &&
                          memory.FreeAnimationKeyFrames(generation, looseFrame) == ClockResult::Ok,
                      "unrelated original allocations remain explicitly usable and releasable after the pump");
                manager.DeleteCustomAnimHierarchyInterface(unrelatedTag);
            };
            invoke();
            unrelated.reset();
            Check(host.freeCalls == beforeAbandon + 3 && runtime.active.load() && !runtime.stopping.load(),
                  "active abandoned cleanup frees its own array once and never performs generation-wide retirement");
            std::puts("Production private cleanup abandoned intent progressed during active callbacks");
        }
        std::unique_ptr<CAnimBlendHierarchy> hierarchy;
        std::uintptr_t                       array = 0, frames = 0;
        callback.action = [&](NativeGameTimingAccess& access)
        {
            Check(dynamic_cast<NativeCustomAnimationCleanupAccess*>(&access) == static_cast<NativeCustomAnimationCleanupAccess*>(&runtime.access),
                  "cleanup tickets borrow the same actual runtime object as hierarchy and storage");
            hierarchy = manager.GetCustomAnimBlendHierarchy();
            Check(hierarchy && memory.AllocateAnimationSequences(generation, 1, array) == ClockResult::Ok &&
                      memory.AllocateAnimationKeyFrames(generation, 10, frames) == ClockResult::Ok,
                  "actual partial manager and storage service issue a private animation resource set");
        };
        invoke();
        std::unique_ptr<CAnimBlendSequence> row;
        callback.action = [&](NativeGameTimingAccess&)
        {
            row = manager.GetCustomAnimBlendSequenceAt(reinterpret_cast<CAnimBlendSequenceSAInterface*>(array), 0);
            row->SetKeyFrames(1, false, true, reinterpret_cast<void*>(frames));
            hierarchy->SetNumSequences(1);
            hierarchy->SetSequences(reinterpret_cast<CAnimBlendSequenceSAInterface*>(array));
        };
        invoke();
        SCustomAnimationCleanupRequest request;
        request.origin = &manager;
        request.hierarchy = hierarchy.get();
        request.originalArray = reinterpret_cast<CAnimBlendSequenceSAInterface*>(array);
        request.capacity = 1;
        request.cacheWasAttached = true;
        std::unique_ptr<CCustomAnimationCleanup> ticket;
        if (!scenario)
        {
            callback.action = [&](NativeGameTimingAccess&)
            {
                const auto pendingBefore = manager.HasPendingCustomAnimationCleanup();
                auto       wrong = request;
                wrong.origin = nullptr;
                bool refused = false;
                try
                {
                    static_cast<void>(manager.BeginCustomAnimationCleanup(wrong));
                }
                catch (const std::invalid_argument&)
                {
                    refused = true;
                }
                Check(refused && manager.HasPendingCustomAnimationCleanup() == pendingBefore,
                      "foreign source manager is refused without changing its existing retention summary");
                bool allocationFailed = false;
                CustomAnimationCleanupCpuAllocation::failNext.store(true);
                try
                {
                    static_cast<void>(manager.BeginCustomAnimationCleanup(request));
                }
                catch (const std::bad_alloc&)
                {
                    allocationFailed = true;
                }
                const auto unusedFault = CustomAnimationCleanupCpuAllocation::failNext.exchange(false);
                Check(allocationFailed && !unusedFault && manager.HasPendingCustomAnimationCleanup() == pendingBefore && host.Contains(frames) &&
                          host.Contains(array - 8),
                      "actual CPU facade allocation fails before native Begin can accept or lose resource ownership");
            };
            invoke();
        }
        const auto freesBefore = host.freeCalls;
        ticket = manager.BeginCustomAnimationCleanup(request);
        Check(ticket && manager.HasPendingCustomAnimationCleanup() && host.freeCalls == freesBefore,
              "real creator-side manager Begin reserves the ticket without a natural callback or native effects");
        Check(ticket->Resume().status == ECustomAnimationCleanupStatus::Refused && host.freeCalls == freesBefore,
              "CPU ticket retention between callbacks cannot lend native cleanup authority");
        SCustomAnimationCleanupResult result;
        const auto                    armFailure = [&]
        {
            if (scenario == 1)
                host.afterFree = [](Host::State& current) noexcept { *reinterpret_cast<LONG*>(current.image + NativeBorrowedGcProfile::markerRva) = 1; };
            if (scenario == 2)
            {
                host.freeResult = ClockResult::OriginalException;
                host.consumeFailedFree = true;
            }
        };
        callback.action = [&](NativeGameTimingAccess&) { result = ticket->Resume(); };
        armFailure();
        invoke();
        host.afterFree = nullptr;
        for (unsigned attempt = 0; !clearCompletion && attempt != 8 && host.freeCalls == freesBefore; ++attempt)
        {
            callback.action = [&](NativeGameTimingAccess&) { result = ticket->Resume(); };
            armFailure();
            invoke();
            host.afterFree = nullptr;
        }
        if (clearCompletion)
        {
            for (unsigned attempt = 0; attempt != 8 && result.status != ECustomAnimationCleanupStatus::Complete; ++attempt)
            {
                callback.action = [&](NativeGameTimingAccess&) { result = ticket->Resume(); };
                invoke();
            }
            Check(result.status == ECustomAnimationCleanupStatus::Complete && result.disposition == ECustomAnimationCleanupDisposition::PrivateDetached &&
                      host.freeCalls == freesBefore + 2 && !runtime.privateHierarchies.IsRetained() && manager.HasPendingCustomAnimationCleanup(),
                  "actual ticket completes only after child and outer receipts while its live code still keeps the provider");
        }
        else
        {
            Check(result.status == ECustomAnimationCleanupStatus::Refused && host.freeCalls == freesBefore + 1 && runtime.customSequences.IsRetained() &&
                      manager.HasPendingCustomAnimationCleanup() && (scenario != 2 || runtime.stopping.load()),
                  "manual returned-free refusal preserves its exact resumable ACK; unknown native effects also stop ordinary runtime use");
            host.freeResult = ClockResult::Ok;
            host.consumeFailedFree = false;
            saved.Global(NativeBorrowedGcProfile::markerRva, LONG{0});
        }
        if (!scenario)
        {
            const auto returnedFrees = host.freeCalls;
            const auto cached = ticket->Resume();
            Check(cached.status == ECustomAnimationCleanupStatus::Complete && cached.disposition == ECustomAnimationCleanupDisposition::PrivateDetached &&
                      host.freeCalls == returnedFrees,
                  "completed live ticket returns its exact saved CPU receipt outside admission without another native free");
        }
        Check(!service.End() && service.IsRetained(), "End cannot unload callback code while its actual CPU ticket is alive");
        for (unsigned attempt = 0; attempt != 8; ++attempt)
            fixture.invokeTimer();
        ClockServiceStateV1 state;
        Check(service.Read(state) && state.modulePinned && !service.End(),
              "real runtime module pin remains held for completed live views or unknown destructive effects");
        if (scenario == 3)
        {
            std::thread closer([owned = std::move(ticket)]() mutable { owned.reset(); });
            closer.join();
            hierarchy.reset();
            row.reset();
            for (unsigned attempt = 0; attempt != 3; ++attempt)
                fixture.invokeTimer();
            Check(!service.End() && service.IsRetained() && runtime.customSequences.IsRetained() && manager.HasPendingCustomAnimationCleanup() &&
                      host.freeCalls == freesBefore + 2 && !host.Contains(frames) && !host.Contains(array - 8),
                  "worker terminal ticket drop quarantines code lifetime even after actual resources were acknowledged and the worker joined");
            std::puts("Production private cleanup foreign thread ticket drop retained provider lifetime");
        }
        else if (scenario == 2)
        {
            const auto unknownFrees = host.freeCalls;
            ticket.reset();
            hierarchy.reset();
            row.reset();
            for (unsigned attempt = 0; attempt != 3; ++attempt)
                fixture.invokeTimer();
            Check(!service.End() && service.IsRetained() && runtime.customSequences.IsRetained() && manager.HasPendingCustomAnimationCleanup() &&
                      host.freeCalls == unknownFrees && host.Contains(array - 8),
                  "absence of the consumed frame cannot forge a free ACK or let generic retirement bypass the cleanup claim");
            std::puts("Production private cleanup unknown acknowledgement retained without replay");
        }
        else
        {
            Check(host.freeCalls == freesBefore + 2 && !host.Contains(frames) && !host.Contains(array - 8),
                  "natural cleanup before generic retirement consumes each known original exactly once");
            ticket.reset();
            hierarchy.reset();
            row.reset();
            for (unsigned attempt = 0; attempt != 8 && service.IsRetained(); ++attempt)
                fixture.invokeTimer();
            Check(service.End() && !service.IsRetained() && !runtime.customSequences.IsRetained() && !runtime.privateHierarchies.IsRetained(),
                  "next qualified no-view prune permits actual End and module retirement after all resource ACKs");
            if (scenario == 1)
                std::puts("Production private cleanup returned acknowledgement recovered without replay");
            else
            {
                std::puts("Production actual private cleanup runtime ticket fixture completed");
                std::puts("Production private cleanup live ticket End gate fixture completed");
            }
        }
        Host::active = nullptr;
        ClockReaderTestHost::image = previousImage;
        saved.Restore();
    }
}
