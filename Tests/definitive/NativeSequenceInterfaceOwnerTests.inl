/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual owned-row borrows and source-faithful interface-only release
 ****************************************************************************/
namespace SequenceInterfaceOwnerFixture
{
    namespace Host = NativeCustomAnimSequenceStorageFixture;
    unsigned scenario = 0;
    void     RunPrepared(ClockImageFixture& fixture)
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
        const auto                             generation = runtime.generation.load();
        PrivateAnimationFactoryManagerReceiver manager(runtime.access, runtime.access, runtime.access, runtime.access, generation);
        CAnimManager&                          sdk = manager;
        auto&                                  memory = static_cast<NativeAnimationSequenceStorageAccess&>(runtime.access);
        auto&                                  operations = static_cast<NativeCustomAnimationSequenceOperationAccess&>(runtime.access);
        const auto                             action = [&](const char* label, const auto& body)
        {
            callback.action = [&](NativeGameTimingAccess&)
            {
                try
                {
                    body();
                }
                catch (const std::exception& error)
                {
                    std::fprintf(stderr, "Sequence interface action %s refused: %s\n", label, error.what());
                    Check(false, label);
                }
            };
            Check(service.Renew(generation), "renew the actual interface owner for each bounded native action");
            fixture.invokeTimer();
            callback.action = [](NativeGameTimingAccess&) {};
        };
        const auto makeSource = [&](std::unique_ptr<CAnimBlendSequence>& source, std::uintptr_t& tag)
        {
            action("create explicit standalone interface",
                   [&]
                   {
                       source = sdk.GetCustomAnimBlendSequence();
                       source->Initialize();
                       tag = reinterpret_cast<std::uintptr_t>(source->GetInterface());
                       Check(tag && host.Contains(tag - NativeCustomAnimSequenceStorageProfile::CookieBytes),
                             "dedicated factory supplies the actual standalone producer before raw compatibility use");
                   });
        };
        const auto bind = [&](CAnimBlendSequence& source, std::uintptr_t& frame)
        {
            action("allocate raw interface original frame",
                   [&]
                   {
                       Check(memory.AllocateAnimationKeyFrames(generation, 10, frame) == ClockResult::Ok && frame,
                             "raw interface frame is an original same-owner allocation");
                   });
            action("bind original through real SDK", [&] { source.SetKeyFrames(1, false, true, reinterpret_cast<void*>(frame)); });
        };
        const auto same = [](const NativeAnimationSequenceResolvedIdentity& a, const NativeAnimationSequenceResolvedIdentity& b)
        {
            return a.reference.ownerGeneration == b.reference.ownerGeneration && a.reference.firstTag == b.reference.firstTag &&
                   a.reference.index == b.reference.index && a.interfaceAddress == b.interfaceAddress;
        };
        const NativeAnimationSequenceResolvedIdentity sentinel{{0x71, 0x73, 0x79}, 0x83};
        if (!scenario)
        {
            std::unique_ptr<CAnimBlendSequence> empty;
            std::uintptr_t                      emptyTag = 0;
            makeSource(empty, emptyTag);
            const auto firstFrees = host.freeCalls;
            action("last empty standalone metadata release",
                   [&]
                   {
                       sdk.DeleteCustomAnimSequenceInterface(reinterpret_cast<CAnimBlendSequenceSAInterface*>(emptyTag));
                       Check(host.freeCalls == firstFrees + 1 && !host.Contains(emptyTag - 8) && !runtime.customSequences.IsRetained() &&
                                 !runtime.customSequenceGeneration.load() && runtime.active.load() && !runtime.stopping.load(),
                             "last empty interface deletes once and succeeds after retained generation clears, without inventing a cleanup result");
                   });
            empty.reset();
            sdk.DeleteCustomAnimSequenceInterface(nullptr);
            Check(!sdk.GetCustomAnimBlendSequence(nullptr) && host.freeCalls == firstFrees + 1,
                  "source-compatible null raw getter/delete is a CPU no-resource operation");
            std::uintptr_t                      array = 0, single = 0;
            std::unique_ptr<CAnimBlendSequence> last, singleRow;
            constexpr std::size_t               capacity = 65537;
            action("allocate real full-capacity private arrays",
                   [&]
                   {
                       Check(memory.AllocateAnimationSequences(generation, capacity, array) == ClockResult::Ok &&
                                 memory.AllocateAnimationSequences(generation, 1, single) == ClockResult::Ok,
                             "generic raw borrow uses actual original array/cookie authority, including a capacity-one Array role");
                   });
            const auto lastTag = array + std::uintptr_t{65536} * NativeCustomAnimSequenceStorageProfile::RowBytes;
            action("resolve full-width interior row identity",
                   [&]
                   {
                       auto identity = sentinel;
                       Check(operations.ResolvePrivateAnimationSequenceReference(generation, lastTag, identity) == ClockResult::Ok &&
                                 identity.reference.ownerGeneration == generation && identity.reference.firstTag == array &&
                                 identity.reference.index == 65536 && identity.interfaceAddress == lastTag,
                             "native resolver returns exact physical interface identity and full index without client stride inference");
                       const auto allocations = host.mallocCalls;
                       last = sdk.GetCustomAnimBlendSequence(reinterpret_cast<CAnimBlendSequenceSAInterface*>(lastTag));
                       singleRow = sdk.GetCustomAnimBlendSequence(reinterpret_cast<CAnimBlendSequenceSAInterface*>(single));
                       Check(last && singleRow && reinterpret_cast<std::uintptr_t>(last->GetInterface()) == lastTag &&
                                 reinterpret_cast<std::uintptr_t>(singleRow->GetInterface()) == single && host.mallocCalls == allocations,
                             "actual raw SDK factory borrows current array rows with no native allocation or standalone promotion");
                   });
            std::array<unsigned char, 80> foreign{};
            action("generic raw refusal preserves identity output",
                   [&]
                   {
                       auto identity = sentinel;
                       Check(operations.ResolvePrivateAnimationSequenceReference(generation, lastTag + 1, identity) == ClockResult::BindingChanged &&
                                 same(identity, sentinel),
                             "misaligned row pointer cannot borrow arbitrary array bytes");
                       Check(operations.ResolvePrivateAnimationSequenceReference(generation, reinterpret_cast<std::uintptr_t>(foreign.data()), identity) ==
                                     ClockResult::BindingChanged &&
                                 same(identity, sentinel),
                             "readable foreign CPU storage cannot become an issued row");
                       Check(operations.ResolvePrivateAnimationSequenceReference(generation, fixture.image + NativeAnimationBlockHierarchyProfile::BankRva,
                                                                                 identity) == ClockResult::BindingChanged &&
                                 same(identity, sentinel),
                             "a native bank pointer cannot be adopted by the private row factory");
                       Check(operations.ResolvePrivateAnimationSequenceReference(generation + 1, single, identity) == ClockResult::OwnerConflict &&
                                 same(identity, sentinel),
                             "foreign owner generation refuses every output field");
                       const auto frees = host.freeCalls;
                       Check(operations.DeletePrivateStandaloneAnimationSequenceInterface({generation, single, 0}) == ClockResult::BindingChanged,
                             "capacity one does not grant standalone metadata delete authority");
                       bool refused = false;
                       try
                       {
                           sdk.DeleteCustomAnimSequenceInterface(reinterpret_cast<CAnimBlendSequenceSAInterface*>(single));
                       }
                       catch (const std::runtime_error&)
                       {
                           refused = true;
                       }
                       Check(refused && host.freeCalls == frees && host.Contains(single - 8),
                             "actual SDK raw Delete refuses ordinary Array role before native effects");
                   });
            action(
                "ordinary original array release remains storage only", [&]
                { Check(memory.FreeAnimationSequences(generation, array) == ClockResult::Ok, "original array releases once through its actual array API"); });
            action("consumed row cannot create raw factory",
                   [&]
                   {
                       auto identity = sentinel;
                       Check(operations.ResolvePrivateAnimationSequenceReference(generation, lastTag, identity) == ClockResult::BindingChanged &&
                                 same(identity, sentinel),
                             "absent consumed allocation never supplies a success receipt or probes freed row bytes");
                       Check(memory.FreeAnimationSequences(generation, single) == ClockResult::Ok,
                             "capacity-one Array role remains correctly releasable as an array");
                   });
            singleRow.reset();
            {
                std::unique_ptr<CAnimBlendSequence> source, borrow;
                std::uintptr_t                      tag = 0, frame = 0;
                makeSource(source, tag);
                bind(*source, frame);
                const auto frees = host.freeCalls;
                action("metadata-only delete preserves unbound original frame",
                       [&]
                       {
                           borrow = sdk.GetCustomAnimBlendSequence(reinterpret_cast<CAnimBlendSequenceSAInterface*>(tag));
                           Check(borrow && reinterpret_cast<std::uintptr_t>(borrow->GetInterface()) == tag,
                                 "generic raw borrow also resolves explicit Standalone role");
                           sdk.DeleteCustomAnimSequenceInterface(reinterpret_cast<CAnimBlendSequenceSAInterface*>(tag));
                           Check(host.freeCalls == frees + 1 && !host.Contains(tag - 8) && host.Contains(frame),
                                 "raw source-interface release acknowledges only metadata, leaving its original frame allocated");
                       });
                action("caller explicitly releases the now-unbound frame",
                       [&]
                       {
                           Check(memory.FreeAnimationKeyFrames(generation, frame) == ClockResult::Ok && host.freeCalls == frees + 2,
                                 "original frame remains independently owned/releasable after interface-only delete");
                       });
                borrow.reset();
                source.reset();
            }
            {
                std::unique_ptr<CAnimBlendSequence> source, target;
                std::uintptr_t                      tag = 0, frame = 0, targetArray = 0;
                makeSource(source, tag);
                bind(*source, frame);
                action("real accepted copy holds another array reference",
                       [&]
                       {
                           Check(memory.AllocateAnimationSequences(generation, 1, targetArray) == ClockResult::Ok,
                                 "shared participant owns its original array");
                           target = sdk.GetCustomAnimBlendSequenceAt(reinterpret_cast<CAnimBlendSequenceSAInterface*>(targetArray), 0);
                           target->CopySequenceProperties(source->GetInterface());
                       });
                const auto frees = host.freeCalls;
                action("delete only source side of actual shared frame",
                       [&]
                       {
                           sdk.DeleteCustomAnimSequenceInterface(reinterpret_cast<CAnimBlendSequenceSAInterface*>(tag));
                           Check(host.freeCalls == frees + 1 && host.Contains(frame) && target->GetKeyFrames() == reinterpret_cast<void*>(frame),
                                 "source metadata ACK removes only its own frame obligation and preserves actual related row use");
                           Check(memory.FreeAnimationKeyFrames(generation, frame) == ClockResult::OwnerConflict && host.freeCalls == frees + 1,
                                 "other actual array ownership still prevents an unrelated frame free");
                       });
                action("retire remaining metadata then original shared frame",
                       [&]
                       {
                           Check(memory.FreeAnimationSequences(generation, targetArray) == ClockResult::Ok && host.Contains(frame),
                                 "array storage-only release itself frees no shared payload");
                           Check(memory.FreeAnimationKeyFrames(generation, frame) == ClockResult::Ok && host.freeCalls == frees + 3,
                                 "last actual owner removal makes original frame release possible exactly once");
                       });
                target.reset();
                source.reset();
            }
            {
                std::unique_ptr<CAnimBlendSequence> source;
                std::uintptr_t                      tag = 0, frame = 0;
                makeSource(source, tag);
                action("allocate pending assignment payload",
                       [&]
                       {
                           Check(memory.AllocateAnimationKeyFrames(generation, 10, frame) == ClockResult::Ok,
                                 "pending bind retains its genuine original buffer");
                       });
                auto       ticket = sdk.BeginCustomAnimationFrameAttachment({&sdk, source.get(), 1, false, true, reinterpret_cast<void*>(frame)});
                const auto frees = host.freeCalls;
                const auto writes = ClockFreshCompressedSequenceOwnerTest::writes;
                action("pending bind cannot authorize raw interface release",
                       [&]
                       {
                           bool refused = false;
                           try
                           {
                               sdk.DeleteCustomAnimSequenceInterface(reinterpret_cast<CAnimBlendSequenceSAInterface*>(tag));
                           }
                           catch (const std::runtime_error&)
                           {
                               refused = true;
                           }
                           Check(refused && operations.DeletePrivateStandaloneAnimationSequenceInterface({generation, tag, 0}) == ClockResult::OwnerConflict &&
                                     host.freeCalls == frees && ClockFreshCompressedSequenceOwnerTest::writes == writes,
                                 "pending source rejects metadata deletion with no free or writer effect from that request");
                       });
                SCustomAnimationSequenceResult bound;
                for (unsigned attempt = 0; attempt != 8; ++attempt)
                {
                    action("resume the original pending bind", [&] { bound = ticket->Resume(); });
                    if (bound.status == ECustomAnimationSequenceStatus::Complete)
                        break;
                }
                Check(bound.status == ECustomAnimationSequenceStatus::Complete && bound.disposition == ECustomAnimationSequenceDisposition::FramesAttached,
                      "only the original typed operation supplies binding completion");
                action("resolved bind permits metadata-only delete",
                       [&]
                       {
                           sdk.DeleteCustomAnimSequenceInterface(reinterpret_cast<CAnimBlendSequenceSAInterface*>(tag));
                           Check(host.Contains(frame) && memory.FreeAnimationKeyFrames(generation, frame) == ClockResult::Ok,
                                 "metadata-only release after binding acknowledgement still leaves frame free explicit");
                       });
                ticket.reset();
                source.reset();
            }
            {
                std::unique_ptr<CAnimBlendSequence> source, target;
                std::uintptr_t                      tag = 0, frame = 0, targetArray = 0;
                makeSource(source, tag);
                bind(*source, frame);
                action("prepare pending transfer row",
                       [&]
                       {
                           Check(memory.AllocateAnimationSequences(generation, 1, targetArray) == ClockResult::Ok,
                                 "pending transfer destination is actual array storage");
                           target = sdk.GetCustomAnimBlendSequenceAt(reinterpret_cast<CAnimBlendSequenceSAInterface*>(targetArray), 0);
                       });
                auto       ticket = sdk.BeginCustomAnimationSequenceTransfer({&sdk, source.get(), target.get()});
                const auto frees = host.freeCalls;
                const auto copies = ClockSequenceCopyFrameOwnerTest::copies;
                action("pending transfer cannot be bypassed by raw delete",
                       [&]
                       {
                           Check(operations.DeletePrivateStandaloneAnimationSequenceInterface({generation, tag, 0}) == ClockResult::OwnerConflict &&
                                     host.freeCalls == frees && ClockSequenceCopyFrameOwnerTest::copies == copies,
                                 "raw metadata release cannot consume the source of an accepted pending transfer");
                       });
                SCustomAnimationSequenceResult transferred;
                for (unsigned attempt = 0; attempt != 8; ++attempt)
                {
                    action("resume exact original transfer", [&] { transferred = ticket->Resume(); });
                    if (transferred.status == ECustomAnimationSequenceStatus::Complete)
                        break;
                }
                Check(transferred.status == ECustomAnimationSequenceStatus::Complete &&
                          transferred.disposition == ECustomAnimationSequenceDisposition::SourceTransferred && host.Contains(frame) && !host.Contains(tag - 8),
                      "typed SourceTransferred remains a separate real receipt, never fabricated by raw interface-only success");
                action("finish actual transfer target resources",
                       [&]
                       {
                           Check(memory.FreeAnimationSequences(generation, targetArray) == ClockResult::Ok &&
                                     memory.FreeAnimationKeyFrames(generation, frame) == ClockResult::Ok,
                                 "remaining original target and frame release through their actual resource APIs");
                       });
                ticket.reset();
                source.reset();
                target.reset();
            }
            callback.action = [](NativeGameTimingAccess&) {};
            Check(service.Renew(generation), "renew to prune only completed creator-dropped operation views");
            fixture.invokeTimer();
            Check(host.allocationCount < host.allocations.size() && !runtime.customSequences.IsRetained() && !runtime.privateHierarchies.IsRetained(),
                  "bounded interface cases leave no private ownership or CPU operation view debt");
            WorldGroundOwnerFixture::Retire(service, fixture);
            NativeClockService next;
            bool               staleFacade = false;
            callback.action = [&](NativeGameTimingAccess&)
            {
                auto identity = sentinel;
                Check(operations.ResolvePrivateAnimationSequenceReference(generation, lastTag, identity) == ClockResult::OwnerConflict &&
                          same(identity, sentinel),
                      "a new runtime generation refuses the old raw resolver owner before touching consumed row storage");
                try
                {
                    static_cast<void>(last->GetInterface());
                }
                catch (const std::runtime_error&)
                {
                    staleFacade = true;
                }
            };
            // The first epoch obtained a real process-lifetime Windows PIN.
            // Shipping rejection must preserve that pin; the shared first-epoch
            // helper instead assumes no earlier PIN exists in this process.
            Check(runtime.modulePinned.load(), "first retired owner retains its genuine process-lifetime module pin");
            ClockServiceStateV1 rejected;
            Check(!next.BeginGameTiming({}, rejected, &WorldGroundOwnerFixture::Callback::Invoke, &callback) &&
                      rejected.failure == ClockResult::UnsupportedBuild && !rejected.bound && !rejected.hooked && rejected.modulePinned &&
                      rejected.generation > generation && !NativeServiceAdmission::IsClaimed(NativeServiceAdmission::Category::Clock),
                  "shipping second Begin refuses the actual test executable before binding/hooks while preserving its existing Windows pin");
            fixture.Seed();
            NativeServiceAdmission::Token claim;
            Check(NativeServiceAdmission::TryAcquire(NativeServiceAdmission::Category::Clock, claim),
                  "second owned epoch acquires real shared clock admission after the first fully retired");
            runtime.admissionSerial.store(claim.serial);
            runtime.admissionGeneration.store(runtime.generation.load());
            Check(!ReleaseAdmission(generation) && NativeServiceAdmission::IsClaimed(NativeServiceAdmission::Category::Clock),
                  "retired first epoch cannot release the second epoch's actual admission");
            runtime.image = fixture.image;
            runtime.failure.store(ClockResult::Ok);
            runtime.nativeTouched.store(false);
            runtime.stopping.store(false);
            runtime.bound.store(true);
            runtime.active.store(true);
            runtime.started = GetTickCount64();
            runtime.refreshed.store(runtime.started);
            runtime.originalContinuation = reinterpret_cast<ContinuationFunction>(fixture.image + NativeClockProfile::hookEntryRva);
            runtime.originalTimer = reinterpret_cast<ContinuationFunction>(fixture.image + NativeGamePauseProfile::entryRva);
            runtime.timingHandler = &WorldGroundOwnerFixture::Callback::Invoke;
            runtime.timingContext = &callback;
            Check(CheckBinding(true), "second owned MEM_IMAGE epoch passes the same exact binding checks before hook attachment");
            Check(ChangeHooks(true), "second owner attaches actual Detours only after current native binding validation");
            runtime.hooked.store(true);
            fixture.invokeTimer();
            Check(staleFacade && !runtime.stopping.load(), "old borrowed facade refuses under a different real callback generation without stopping its owner");
            WorldGroundOwnerFixture::Retire(next, fixture);
            last.reset();
            std::puts("Production sequence interface compatibility owner fixture completed");
            std::puts("Production sequence interface compatibility SDK fixture completed");
        }
        else
        {
            std::unique_ptr<CAnimBlendSequence> source;
            std::uintptr_t                      tag = 0, frame = 0;
            makeSource(source, tag);
            bind(*source, frame);
            const auto frees = host.freeCalls;
            host.freeResult = ClockResult::OriginalException;
            host.consumeFailedFree = true;
            action("unknown original metadata free",
                   [&]
                   {
                       bool refused = false;
                       try
                       {
                           sdk.DeleteCustomAnimSequenceInterface(reinterpret_cast<CAnimBlendSequenceSAInterface*>(tag));
                       }
                       catch (const std::runtime_error&)
                       {
                           refused = true;
                       }
                       Check(refused && host.freeCalls == frees + 1 && !host.Contains(tag - 8) && host.Contains(frame) && runtime.stopping.load(),
                             "unknown returned status preserves source claims without pretending metadata or frame acknowledgement");
                   });
            host.freeResult = ClockResult::Ok;
            host.consumeFailedFree = false;
            source.reset();
            service.End();
            for (unsigned attempt = 0; attempt != 6; ++attempt)
                fixture.invokeTimer();
            Check(!service.End() && service.IsRetained() && runtime.customSequences.IsRetained() && host.freeCalls == frees + 1 && host.Contains(frame),
                  "actual recovery/generic retirement never replays unknown metadata free or releases its protected frame obligation");
            std::puts("Production sequence interface unknown metadata free retained");
        }
        // The isolated unknown scenario intentionally retains production debt.
        // Clearing task-owned buffers at process teardown is no native ACK.
        Host::active = nullptr;
        ClockReaderTestHost::image = previousImage;
        saved.Restore();
    }
}
