/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Real standalone sequence intents, transfer ACKs and common lifetimes
 ****************************************************************************/
#include "../../Client/mods/deathmatch/logic/CIFPAnimationStorage.h"
namespace StandaloneSequenceOperationsOwnerFixture
{
    namespace Host = NativeCustomAnimSequenceStorageFixture;
    using Status = ECustomAnimationSequenceStatus;
    using Disposition = ECustomAnimationSequenceDisposition;
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
        for (const auto& pin : NativeAnimationSequenceNameProfile::Code)
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
        const auto tick = [&]
        {
            if (runtime.active.load() && !runtime.stopping.load())
                Check(service.Renew(generation), "renew the real standalone operation owner for each bounded action");
            fixture.invokeTimer();
        };
        const auto action = [&](const char* label, const auto& body)
        {
            callback.action = [&](NativeGameTimingAccess&)
            {
                try
                {
                    body();
                }
                catch (const std::exception& error)
                {
                    std::fprintf(stderr, "Standalone action %s refused: %s\n", label, error.what());
                    Check(false, label);
                }
            };
            tick();
            callback.action = [](NativeGameTimingAccess&) {};
        };
        PrivateAnimationFactoryManagerReceiver manager(runtime.access, runtime.access, runtime.access, runtime.access, generation);
        auto&                                  memory = static_cast<NativeAnimationSequenceStorageAccess&>(runtime.access);
        auto&                                  operations = static_cast<NativeCustomAnimationSequenceOperationAccess&>(runtime.access);
        const auto                             drain = [&](CCustomAnimationSequenceOperation& ticket, Disposition expected)
        {
            SCustomAnimationSequenceResult result;
            for (unsigned attempt = 0; attempt != 24; ++attempt)
            {
                if (runtime.active.load() && !runtime.stopping.load())
                    action("resume exact sequence command", [&] { result = ticket.Resume(); });
                else
                    fixture.invokeTimer();
                // Historical Complete is deliberately consumable without a
                // callback; a failed admission cannot invent that receipt.
                result = ticket.Resume();
                if (result.status == Status::Complete)
                    break;
            }
            Check(result.status == Status::Complete && result.disposition == expected,
                  "matching actual operation completes with its exact acknowledged disposition");
        };
        const auto idleUntilGone = [&](std::uintptr_t original)
        {
            callback.action = [](NativeGameTimingAccess&) {};
            for (unsigned attempt = 0; attempt != 24 && host.Contains(original); ++attempt)
                tick();
            Check(!host.Contains(original), "active natural callback drains its declared original resource without generation retirement");
        };
        const auto finish = [&]
        {
            callback.action = [](NativeGameTimingAccess&) {};
            service.End();
            for (unsigned attempt = 0; attempt != 24 && service.IsRetained(); ++attempt)
                fixture.invokeTimer();
            Check(service.End() && !service.IsRetained() && !runtime.customSequences.IsRetained() && !runtime.privateHierarchies.IsRetained(),
                  "qualified no-view pruning permits actual owner and module retirement after all acknowledgements");
        };
        const auto makeSource = [&](std::unique_ptr<CAnimBlendSequence>& source, std::uintptr_t& row)
        {
            action("create explicit standalone source",
                   [&]
                   {
                       source = manager.GetCustomAnimBlendSequence();
                       source->Initialize();
                       row = reinterpret_cast<std::uintptr_t>(source->GetInterface());
                       Check(row && host.Contains(row - NativeCustomAnimSequenceStorageProfile::CookieBytes),
                             "dedicated factory issues a real private standalone allocation and initialized row");
                   });
        };
        const auto attach = [&](CAnimBlendSequence& source, std::uintptr_t& frame)
        {
            action("allocate source frame",
                   [&]
                   {
                       Check(memory.AllocateAnimationKeyFrames(generation, 10, frame) == ClockResult::Ok && frame,
                             "frame originates from the same genuine current allocator owner");
                   });
            action("common accepted frame attachment",
                   [&]
                   {
                       MtaGame::IfpStorage::AttachFrames(manager, source, 1, false, true, reinterpret_cast<BYTE*>(frame));
                       *reinterpret_cast<unsigned char*>(frame) = 0x4a;
                       Check(source.GetKeyFrames() == reinterpret_cast<void*>(frame),
                             "actual common attachment returns only after native binding acknowledgement and permits the real payload view");
                   });
        };
        const auto beginCleanup = [&](CAnimBlendSequence& source) { return manager.BeginCustomAnimationSequenceCleanup({&manager, &source, true}); };
        ClockFreshCompressedSequenceOwnerTest::mode = 0;
        ClockSequenceCopyFrameOwnerTest::mode = 0;
        if (!scenario)
        {
            const auto initialMallocs = host.mallocCalls;
            action("preallocate standalone CPU facade",
                   [&]
                   {
                       bool failed = false;
                       CustomAnimationCleanupCpuAllocation::failNext.store(true);
                       try
                       {
                           static_cast<void>(manager.GetCustomAnimBlendSequence());
                       }
                       catch (const std::bad_alloc&)
                       {
                           failed = true;
                       }
                       const bool unused = CustomAnimationCleanupCpuAllocation::failNext.exchange(false);
                       Check(failed && !unused && host.mallocCalls == initialMallocs,
                             "CPU facade allocation failure precedes standalone native allocation and cannot strand a new record");
                   });
            std::uintptr_t array = 0;
            action("array role is not standalone authority",
                   [&]
                   {
                       Check(memory.AllocateAnimationSequences(generation, 1, array) == ClockResult::Ok, "ordinary capacity-one array is genuinely issued");
                       auto row = manager.GetCustomAnimBlendSequenceAt(reinterpret_cast<CAnimBlendSequenceSAInterface*>(array), 0);
                       bool refused = false;
                       try
                       {
                           static_cast<void>(beginCleanup(*row));
                       }
                       catch (const std::runtime_error&)
                       {
                           refused = true;
                       }
                       NativeAnimationSequenceReferenceToken untouched{73, 79, 83};
                       const auto                            status = operations.ResolvePrivateStandaloneAnimationSequence(generation, array, untouched);
                       Check(refused && status == ClockResult::BindingChanged && untouched.ownerGeneration == 73 && untouched.firstTag == 79 &&
                                 untouched.index == 83,
                             "capacity one and an ordinary borrowed row cannot counterfeit dedicated standalone producer authority");
                       Check(memory.FreeAnimationSequences(generation, array) == ClockResult::Ok,
                             "ordinary array retains its original storage-only release contract after refusal");
                   });
            {
                std::unique_ptr<CAnimBlendSequence> source;
                std::uintptr_t                      row = 0;
                makeSource(source, row);
                action("borrow current standalone",
                       [&]
                       {
                           auto borrowed = manager.GetCustomAnimBlendSequence(reinterpret_cast<CAnimBlendSequenceSAInterface*>(row));
                           Check(borrowed && reinterpret_cast<std::uintptr_t>(borrowed->GetInterface()) == row,
                                 "standalone raw factory resolves only its current explicit producer without adoption");
                           source->SetName("IFP_STANDALONE");
                           source->SetBoneTag(22);
                       });
                std::uintptr_t frame = 0;
                attach(*source, frame);
                const auto                                                        before = host.freeCalls;
                std::unique_ptr<MtaGame::IfpStorage::SequenceOwner<CAnimManager>> owned(
                    new MtaGame::IfpStorage::SequenceOwner<CAnimManager>(manager, std::move(source), true));
                owned->MarkInitialized();
                owned.reset();
                Check(host.freeCalls == before && host.Contains(frame) && host.Contains(row - 8),
                      "actual SequenceOwner destructor outside callback persists cleanup intent without void frees or native getters");
                idleUntilGone(row - 8);
                Check(!host.Contains(frame) && host.freeCalls == before + 2 && runtime.active.load() && !runtime.stopping.load(),
                      "active pump releases the abandoned standalone original frame and interface once while unrelated timing stays active");
            }
            {
                std::unique_ptr<CAnimBlendSequence>  source, destination;
                std::unique_ptr<CAnimBlendHierarchy> hierarchy;
                std::uintptr_t                       row = 0, frame = 0, target = 0;
                makeSource(source, row);
                attach(*source, frame);
                action("prepare common transfer target",
                       [&]
                       {
                           Check(memory.AllocateAnimationSequences(generation, 1, target) == ClockResult::Ok, "transfer target is its original owned array");
                           hierarchy = manager.GetCustomAnimBlendHierarchy();
                           destination = manager.GetCustomAnimBlendSequenceAt(reinterpret_cast<CAnimBlendSequenceSAInterface*>(target), 0);
                           source->SetName("TRANSFER_EXACT");
                           source->SetBoneTag(31);
                       });
                const auto                                       copies = ClockSequenceCopyFrameOwnerTest::copies;
                const auto                                       frees = host.freeCalls;
                MtaGame::IfpStorage::SequenceOwner<CAnimManager> owned(manager, std::move(source), true);
                owned.MarkInitialized();
                action("real common ownership transfer",
                       [&]
                       {
                           owned.TransferTo(*destination);
                           Check(!owned.Get() && destination->GetBoneTag() == 31 && destination->GetKeyFrames() == reinterpret_cast<void*>(frame),
                                 "common transfer consumes source only after actual copy and source-interface release, preserving target fields");
                       });
                Check(ClockSequenceCopyFrameOwnerTest::copies == copies + 1 && host.freeCalls == frees + 1 && !host.Contains(row - 8) && host.Contains(frame),
                      "accepted Copy transfers its original frame obligation and retires source metadata without freeing the target buffer");
                auto cleanup =
                    manager.BeginCustomAnimationCleanup({&manager, hierarchy.get(), reinterpret_cast<CAnimBlendSequenceSAInterface*>(target), 1, false});
                for (unsigned attempt = 0; attempt != 16 && host.Contains(target - 8); ++attempt)
                    action("retire transferred target group", [&] { static_cast<void>(cleanup->Resume()); });
                Check(!host.Contains(frame) && !host.Contains(target - 8) && host.freeCalls == frees + 3,
                      "target cleanup later acknowledges that same original frame and array exactly once");
                cleanup.reset();
                hierarchy.reset();
                destination.reset();
            }
            {
                std::unique_ptr<CAnimBlendSequence>  source, destination;
                std::unique_ptr<CAnimBlendHierarchy> hierarchy;
                std::uintptr_t                       row = 0, frame = 0, target = 0;
                makeSource(source, row);
                attach(*source, frame);
                action("prepare genuine shared participant",
                       [&]
                       {
                           Check(memory.AllocateAnimationSequences(generation, 1, target) == ClockResult::Ok,
                                 "second actual array participates in frame ownership");
                           hierarchy = manager.GetCustomAnimBlendHierarchy();
                           destination = manager.GetCustomAnimBlendSequenceAt(reinterpret_cast<CAnimBlendSequenceSAInterface*>(target), 0);
                           destination->CopySequenceProperties(source->GetInterface());
                       });
                const auto frees = host.freeCalls;
                auto       cleanupSource = beginCleanup(*source);
                for (unsigned attempt = 0; attempt != 3; ++attempt)
                    action("shared source cleanup waits", [&] { static_cast<void>(cleanupSource->Resume()); });
                Check(host.Contains(frame) && !host.Contains(row - 8) && runtime.active.load() && !runtime.stopping.load(),
                      "related live array delays only the shared buffer while source metadata can retire without poisoning unrelated work");
                auto cleanupTarget =
                    manager.BeginCustomAnimationCleanup({&manager, hierarchy.get(), reinterpret_cast<CAnimBlendSequenceSAInterface*>(target), 1, false});
                for (unsigned attempt = 0; attempt != 16 && host.Contains(target - 8); ++attempt)
                    action("shared target cleanup", [&] { static_cast<void>(cleanupTarget->Resume()); });
                drain(*cleanupSource, Disposition::SourceReleased);
                Check(!host.Contains(frame) && !host.Contains(target - 8) && host.freeCalls == frees + 3,
                      "both participants share one actual returned Frame acknowledgement rather than duplicate frees");
                cleanupTarget.reset();
                cleanupSource.reset();
                hierarchy.reset();
                destination.reset();
                source.reset();
            }
            {
                std::unique_ptr<CAnimBlendSequence> source, destination;
                std::uintptr_t                      row = 0, frame = 0, target = 0;
                makeSource(source, row);
                attach(*source, frame);
                action("prepare unentered cancellation",
                       [&]
                       {
                           Check(memory.AllocateAnimationSequences(generation, 1, target) == ClockResult::Ok, "cancellation target is current owned storage");
                           destination = manager.GetCustomAnimBlendSequenceAt(reinterpret_cast<CAnimBlendSequenceSAInterface*>(target), 0);
                       });
                const auto copies = ClockSequenceCopyFrameOwnerTest::copies;
                auto       transfer = manager.BeginCustomAnimationSequenceTransfer({&manager, source.get(), destination.get()});
                auto       cancellation = beginCleanup(*source);
                drain(*transfer, Disposition::SourceReleased);
                Check(ClockSequenceCopyFrameOwnerTest::copies == copies && !host.Contains(frame) && !host.Contains(row - 8),
                      "explicit pre-entry cancellation uses original transfer journal to release source without entering Copy");
                action("cancelled destination remains unchanged",
                       [&]
                       {
                           Check(destination->GetKeyFrames() == nullptr && memory.FreeAnimationSequences(generation, target) == ClockResult::Ok,
                                 "cancelled copy never publishes its source buffer or changes target ownership");
                       });
                cancellation.reset();
                transfer.reset();
                source.reset();
                destination.reset();
            }
            {
                std::unique_ptr<CAnimBlendSequence> source;
                std::uintptr_t                      row = 0, frame = 0;
                makeSource(source, row);
                action("allocate pending bind original",
                       [&] { Check(memory.AllocateAnimationKeyFrames(generation, 10, frame) == ClockResult::Ok, "pending assignment owns original frame"); });
                const auto                                       writes = ClockFreshCompressedSequenceOwnerTest::writes;
                MtaGame::IfpStorage::SequenceOwner<CAnimManager> owned(manager, std::move(source), true);
                owned.MarkInitialized();
                bool pending = false;
                try
                {
                    MtaGame::IfpStorage::AttachFrames(manager, *owned.Get(), 1, false, true, reinterpret_cast<BYTE*>(frame), &owned.FrameOperation());
                }
                catch (const std::runtime_error&)
                {
                    pending = true;
                }
                Check(pending && owned.FrameOperation() && host.Contains(frame),
                      "creator-side accepted bind is retained when native admission is unavailable, with no getter-derived free");
                owned = {};
                idleUntilGone(row - 8);
                Check(!host.Contains(frame) && ClockFreshCompressedSequenceOwnerTest::writes == writes && runtime.active.load() && !runtime.stopping.load(),
                      "separate destructor cleanup intent cancels unentered bind and releases original resources without writing row fields");
            }
            {
                std::unique_ptr<CAnimBlendSequence> source;
                std::uintptr_t                      row = 0, first = 0, second = 0;
                makeSource(source, row);
                attach(*source, first);
                action("source Initialize retains native debt", [&] { source->Initialize(); });
                attach(*source, second);
                const auto frees = host.freeCalls;
                auto       cleanup = beginCleanup(*source);
                drain(*cleanup, Disposition::SourceReleased);
                Check(host.freeCalls == frees + 3 && !host.Contains(first) && !host.Contains(second) && !host.Contains(row - 8),
                      "Initialize and frame-field replacement preserve historical original-buffer obligations through actual cleanup ACKs");
                cleanup.reset();
                source.reset();
            }
            // Completed creator-side views may be pruned only on an admitted
            // callback, independent of the real timing owner's continued use.
            callback.action = [](NativeGameTimingAccess&) {};
            tick();
            Check(host.allocationCount < host.allocations.size() && runtime.active.load() && !runtime.stopping.load(),
                  "bounded standalone cases use actual current owner and stay below the reusable allocator fixture capacity");
            finish();
            std::puts("Production standalone sequence operations success verified");
        }
        else
        {
            std::unique_ptr<CAnimBlendSequence> source, destination;
            std::uintptr_t                      row = 0, frame = 0, target = 0;
            makeSource(source, row);
            if (scenario == 4)
            {
                attach(*source, frame);
                action("prepare copy acknowledgement target",
                       [&]
                       {
                           Check(memory.AllocateAnimationSequences(generation, 1, target) == ClockResult::Ok,
                                 "copy acknowledgement owns original target array");
                           destination = manager.GetCustomAnimBlendSequenceAt(reinterpret_cast<CAnimBlendSequenceSAInterface*>(target), 0);
                       });
            }
            else
                action("allocate typed assignment original",
                       [&]
                       {
                           Check(memory.AllocateAnimationKeyFrames(generation, 10, frame) == ClockResult::Ok,
                                 "typed assignment receives its original issued frame");
                       });
            const auto                                         initialFrees = host.freeCalls;
            NativeCustomAnimationSequenceOperationBeginReceipt foreign;
            const auto foreignStatus = operations.BeginPrivateAnimationSequenceCleanup({generation + 1, row, 0}, foreign);
            Check(foreignStatus == ClockResult::OwnerConflict && !foreign.accepted && !foreign.view.IsIssued() && host.freeCalls == initialFrees,
                  "foreign generation cannot accept standalone native release intent or fabricate completion");
            std::unique_ptr<CCustomAnimationSequenceOperation> ticket;
            if (scenario == 4)
                ticket = manager.BeginCustomAnimationSequenceTransfer({&manager, source.get(), destination.get()});
            else
                ticket = manager.BeginCustomAnimationFrameAttachment({&manager, source.get(), 1, false, true, reinterpret_cast<void*>(frame)});
            Check(ticket && host.freeCalls == initialFrees, "creator-side operation Begin retains immutable intent before native admission");
            const auto beforeWrites = scenario == 4 ? ClockSequenceCopyFrameOwnerTest::copies : ClockFreshCompressedSequenceOwnerTest::writes;
            if (scenario == 1)
                ClockFreshCompressedSequenceOwnerTest::mode = 1;
            else if (scenario == 3)
                ClockFreshCompressedSequenceOwnerTest::mode = 2;
            else if (scenario == 4)
                ClockSequenceCopyFrameOwnerTest::mode = 2;
            SCustomAnimationSequenceResult result;
            action("enter actual owned row operation once", [&] { result = ticket->Resume(); });
            ClockFreshCompressedSequenceOwnerTest::mode = 0;
            ClockSequenceCopyFrameOwnerTest::mode = 0;
            const auto afterWrites = scenario == 4 ? ClockSequenceCopyFrameOwnerTest::copies : ClockFreshCompressedSequenceOwnerTest::writes;
            Check(afterWrites == beforeWrites + 1, "typed service delegates exactly once to the real guarded CPU writer");
            if (scenario == 2)
            {
                Check(result.status == Status::Complete && result.disposition == Disposition::FramesAttached && host.Contains(frame),
                      "foreign-drop scenario first obtains a genuine binding acknowledgement");
                ticket.reset();
                auto cleanup = beginCleanup(*source);
                drain(*cleanup, Disposition::SourceReleased);
                Check(host.freeCalls == initialFrees + 2 && !host.Contains(frame) && !host.Contains(row - 8) && manager.HasPendingCustomAnimationCleanup(),
                      "completed live cleanup view keeps actual provider code after native original releases");
                Check(!service.End(), "live completed standalone ticket prevents module retirement before its destructor returns");
                std::thread worker([owned = std::move(cleanup)]() mutable { owned.reset(); });
                worker.join();
                source.reset();
                for (unsigned attempt = 0; attempt != 4; ++attempt)
                    fixture.invokeTimer();
                Check(!service.End() && service.IsRetained() && runtime.customSequences.IsRetained() && manager.HasPendingCustomAnimationCleanup() &&
                          host.freeCalls == initialFrees + 2,
                      "off-creator terminal view drop quarantines module lifetime without replaying acknowledged native cleanup");
                std::puts("Production standalone sequence operations foreign ticket drop quarantined");
            }
            else if (scenario == 1)
            {
                saved.Global(NativeBorrowedGcProfile::markerRva, LONG{0});
                const auto unknownWrites = ClockFreshCompressedSequenceOwnerTest::writes;
                ticket.reset();
                source.reset();
                service.End();
                for (unsigned attempt = 0; attempt != 5; ++attempt)
                    fixture.invokeTimer();
                Check(!service.End() && service.IsRetained() && runtime.customSequences.IsRetained() && manager.HasPendingCustomAnimationCleanup() &&
                          ClockFreshCompressedSequenceOwnerTest::writes == unknownWrites && host.freeCalls == initialFrees && host.Contains(frame) &&
                          host.Contains(row - 8),
                      "entered unknown row write remains claimed without a second writer, inferred acknowledgement or native free");
                std::puts("Production standalone sequence operations unknown write retained");
            }
            else
            {
                Check(result.status == Status::Refused && runtime.stopping.load() && host.Contains(frame),
                      "accepted physical write with lost owner acknowledgement stops ordinary use while its real journal remains");
                saved.Global(NativeBorrowedGcProfile::markerRva, LONG{0});
                std::unique_ptr<CCustomAnimationSequenceOperation> sourceCleanup;
                if (scenario == 3)
                    sourceCleanup = beginCleanup(*source);
                Check(!service.End(), "known accepted effect and live ticket keep actual cleanup callback available");
                for (unsigned attempt = 0; attempt != 24; ++attempt)
                {
                    fixture.invokeTimer();
                    result = ticket->Resume();
                    if (result.status == Status::Complete && !host.Contains(frame) && !host.Contains(row - 8) && (!target || !host.Contains(target - 8)))
                        break;
                }
                const auto writes = scenario == 4 ? ClockSequenceCopyFrameOwnerTest::copies : ClockFreshCompressedSequenceOwnerTest::writes;
                Check(result.status == Status::Complete &&
                          result.disposition == (scenario == 4 ? Disposition::SourceTransferred : Disposition::FramesAttached) && writes == afterWrites &&
                          !host.Contains(frame) && !host.Contains(row - 8) && (!target || !host.Contains(target - 8)) &&
                          host.freeCalls == initialFrees + (scenario == 4 ? 3u : 2u),
                      "natural recovery requalifies exact accepted bytes and original free ACKs without replaying the writer or losing aliases");
                const auto cached = ticket->Resume();
                Check(cached.status == Status::Complete && cached.disposition == result.disposition,
                      "same completed operation receipt remains consumable outside callback after creator close");
                if (sourceCleanup)
                {
                    const auto released = sourceCleanup->Resume();
                    Check(released.status == Status::Complete && released.disposition == Disposition::SourceReleased,
                          "separate cleanup intent releases the source after its prior accepted Bind acknowledgement");
                }
                sourceCleanup.reset();
                ticket.reset();
                source.reset();
                destination.reset();
                finish();
                std::puts(scenario == 3 ? "Production standalone sequence operations bind lost acknowledgement verified"
                                        : "Production standalone sequence operations copy lost acknowledgement verified");
            }
        }
        // Unknown/foreign-drop scenarios deliberately end their fresh process
        // with retained production debt. Test allocation teardown is not an ACK.
        Host::active = nullptr;
        ClockReaderTestHost::image = previousImage;
        saved.Restore();
    }
}
