/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: One native on-screen operation on the actual registered-ped owner
 *****************************************************************************/
namespace PedOnScreenOwnerFixture
{
    void Run(ClockImageFixture& fixture)
    {
        namespace P = NativePedOnScreenProfile;
        namespace R = NativePedReferenceProfile;
        namespace Ped = NativePedPoolProfile;
        namespace Host = ClockPedOnScreenOwnerTest;
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
        {
            const auto target = fixture.image + slot.targetRva;
            writes.Global(slot.rva, target, PAGE_READONLY);
        }
        for (const auto& pin : NativeStreamingLoadAllProfile::Code)
            WorldGroundOwnerFixture::Writes::Apply(&writes, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        admit(NativeStartupOwnerProfile::pins);
        admit(NativeStartupWorldProfile::pins);
        for (const auto& pin : NativeUObjectMembershipProfile::pins)
            WorldGroundOwnerFixture::Writes::Apply(&writes, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        admit(NativeGameStateProfile::pins);
        for (const auto& pin : NativeBorrowedGcProfile::pins)
            WorldGroundOwnerFixture::Writes::Apply(&writes, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : NativeStartGameProfile::Code)
            if (pin.rva == 0xab879c || pin.rva == 0xab92f0 || pin.rva == 0x1af4ef0)
                WorldGroundOwnerFixture::Writes::Apply(&writes, pin.rva, pin.bytes.data(), pin.size, PAGE_EXECUTE_READ);
        alignas(std::uintptr_t) std::array<unsigned char, 0x180> root{};
        NativeStreamingLoadAllFixture::Graph graph(fixture.image, reinterpret_cast<std::uintptr_t>(root.data()), &WorldGroundOwnerFixture::Writes::Apply,
                                                   &writes);
        NativePedOnScreenFixture::Install(fixture.image, &WorldGroundOwnerFixture::Writes::Apply, &writes);
        writes.Global(Ped::RootRva, std::uintptr_t{0});
        writes.Global(NativeGameStateProfile::stateRva, std::uint32_t{9});
        writes.Global(NativeStartGameProfile::ThreadInitializedRva, static_cast<unsigned char>(1));
        writes.Global(NativeStartGameProfile::ThreadIdRva, GetCurrentThreadId());
        writes.Global(NativeBorrowedGcProfile::markerRva, LONG{0});
        std::vector<unsigned char> nodes(R::ReferenceCount * R::ReferenceBytes);
        for (std::size_t index = 0; index + 1 < R::ReferenceCount; ++index)
        {
            const auto next = fixture.image + R::ReferencesRva + (index + 1) * R::ReferenceBytes;
            std::memcpy(nodes.data() + index * R::ReferenceBytes, &next, sizeof(next));
        }
        WorldGroundOwnerFixture::Writes::Apply(&writes, R::ReferencesRva, nodes.data(), nodes.size(), PAGE_READWRITE);
        writes.Global(R::FreeHeadRva, fixture.image + R::ReferencesRva);
        ClockTeleportPool pool(fixture, Ped::RootRva, Ped::NativeCapacity, Ped::ObjectStride);
        pool.Publish();
        pool.flags[0] = 1;
        const auto          vtable = fixture.image + R::PedVtableRva;
        const std::uint16_t modelIndex = 17;
        std::memcpy(pool.objects, &vtable, sizeof(vtable));
        std::memcpy(pool.objects + NativePedMetadataProfile::ModelOffset, &modelIndex, sizeof(modelIndex));
        pool.objects[R::TypeOffset] = 3;
        // The genuine null model slot is the helper's fallback path. Neither
        // a fake model/asset object nor a separate ground-matrix graph is built.
        const auto priorReaderImage = ClockReaderTestHost::image;
        const auto priorReferenceImage = ClockPedReferenceOwnerTest::image;
        ClockReaderTestHost::image = ClockPedReferenceOwnerTest::image = fixture.image;
        ClockPedReferenceOwnerTest::partialUnlink = false;
        ClockPedReferenceOwnerTest::unregisterResult = ClockResult::Ok;
        Check(ClockReaderTestHost::Module(nullptr) == reinterpret_cast<HMODULE>(fixture.image),
              "on-screen scope selects the owned native image before thread/world/ped checks");
        Host::image = fixture.image;
        Host::ped = reinterpret_cast<std::uintptr_t>(pool.objects);
        Host::calls = {};
        Host::scenario = 0;
        Host::callerToken = nullptr;
        Host::reenter = nullptr;
        const auto visibilityReads = ClockPedVisibilityOwnerTest::reads, visibilityStores = ClockPedVisibilityOwnerTest::stores;
        const auto groundCalls = ClockWorldGroundOwnerTest::calls, lineCalls = ClockWorldLineOwnerTest::calls;
        struct DefaultOnScreenAccess final : NativePedOnScreenAccess
        {
        };
        DefaultOnScreenAccess absent;
        for (const bool prior : {false, true})
        {
            bool value = prior;
            Check(absent.ReadPedOnScreen({}, value) == ClockResult::NotInitialized && value == prior,
                  "older on-screen owner preserves either bool rather than reporting successful off-screen detection");
        }
        ped_on_screen_fixture::Game         game;
        ped_on_screen_fixture::TaskScenario taskScenario;
        const auto                          makePed = [&](const NativePedReferenceToken& token)
        {
            auto& access = runtime.access;
            return std::make_unique<ped_on_screen_fixture::PedOnScreenFixture>(game, access, access, access, access, access, access, access, access, access,
                                                                               access, access, access, access, access, access, access, access, access, access,
                                                                               access, access, access, access, token, 0x12345678, taskScenario);
        };
        const auto retire = [&](NativeClockService& service)
        {
            WorldGroundOwnerFixture::Retire(service, fixture);
            Check(!runtime.pedReferences.IsRetained(), "on-screen retirement also acknowledges the sole registered-cell controller");
        };
        WorldGroundOwnerFixture::Callback                          callback;
        ClockResult                                                result = ClockResult::NotInitialized;
        NativePedReferenceToken                                    token;
        bool                                                       output = true;
        std::unique_ptr<ped_on_screen_fixture::PedOnScreenFixture> oldSdk;
        {
            NativeClockService service;
            callback.action = [&](NativeGameTimingAccess& access) { result = access.AcquirePedReference(1, token); };
            BeginOwned(service, fixture, false, true, true, &WorldGroundOwnerFixture::Callback::Invoke, &callback);
            fixture.invokeTimer();
            Check(result == ClockResult::Ok && token.version && runtime.pedReferences.IsRetained(),
                  "on-screen operation borrows the existing genuine native registered-ped controller");
            Host::cell = ClockPedReferenceOwnerTest::cell;
            Host::selected = token;
            oldSdk = makePed(token);
            Check(Host::calls == std::array<unsigned, 3>{} && !taskScenario.eventCount,
                  "actual on-screen SDK construction copies identity without task, game, model or native query work");
            Check(runtime.access.ReadPedOnScreen(token, output) != ClockResult::Ok && output && Host::calls == std::array<unsigned, 3>{},
                  "retained token cannot lend on-screen callback admission between natural frames");
            std::thread foreign([&] { result = runtime.access.ReadPedOnScreen(token, output); });
            foreign.join();
            Check(result != ClockResult::Ok && output && Host::calls == std::array<unsigned, 3>{} && !runtime.stopping.load(),
                  "foreign on-screen call preserves output without stopping the actual owner");
            callback.action = [&](NativeGameTimingAccess& access)
            {
                auto stale = token;
                ++stale.version;
                result = access.ReadPedOnScreen(stale, output);
            };
            fixture.invokeTimer();
            Check(result != ClockResult::Ok && output && Host::calls == std::array<unsigned, 3>{} && !runtime.stopping.load(),
                  "initial stale receiver refuses before native entry without consuming the owner");
            callback.action = [&](NativeGameTimingAccess& access) { result = access.ReadPedOnScreen(token, output); };
            Check(NativeStreamingCompletionContext::TryEnter(), "hold existing shared native effect gate before on-screen intent");
            fixture.invokeTimer();
            NativeStreamingCompletionContext::Leave();
            Check(result == ClockResult::Busy && output && Host::calls == std::array<unsigned, 3>{} && !runtime.stopping.load(),
                  "on-screen reuses the existing whole-operation gate and refuses before any helper");
            // Native seams are noexcept: collect facts here, then assert in
            // the ordinary outer fixture so a failure remains diagnostic data.
            static ClockResult   observedNested = ClockResult::NotInitialized, observedGround = ClockResult::NotInitialized;
            static bool          observedNestedOutput = false, observedStopping = false;
            static std::uint32_t observedHeight = 0;
            static unsigned      observedReentries = 0;
            observedReentries = 0;
            Host::reenter = +[]() noexcept
            {
                ++observedReentries;
                std::fprintf(stderr, "OnScreen reentry begin: count=%u selectedVersion=%llu selectedGeneration=%llu calls=%u,%u,%u\n", observedReentries,
                             static_cast<unsigned long long>(Host::selected.version), static_cast<unsigned long long>(Host::selected.ownerGeneration),
                             Host::calls[0], Host::calls[1], Host::calls[2]);
                observedNestedOutput = true;
                observedNested = runtime.access.ReadPedOnScreen(Host::selected, observedNestedOutput);
                std::fprintf(stderr, "OnScreen nested receiver returned: result=%u output=%d stopping=%d\n", static_cast<unsigned>(observedNested),
                             static_cast<int>(observedNestedOutput), static_cast<int>(runtime.stopping.load()));
                CVector point;
                float   height = -173.25f;
                observedGround = runtime.access.FindWorldGroundZ(point, height);
                observedHeight = WorldGroundOwnerFixture::Bits(height);
                observedStopping = runtime.stopping.load();
                std::fprintf(stderr, "OnScreen nested ground returned: result=%u height=%08x stopping=%d\n", static_cast<unsigned>(observedGround),
                             observedHeight, static_cast<int>(observedStopping));
            };
            bool normalCaught = false;
            for (const bool requested : {false, true})
            {
                normalCaught = false;
                Host::nativeValue = requested;
                const auto before = Host::calls;
                callback.action = [&](NativeGameTimingAccess&)
                {
                    std::fprintf(stderr, "OnScreen actual SDK callback: owner=%u failure=%u stopping=%d active=%d calls=%u,%u,%u\n",
                                 static_cast<unsigned>(CheckTimingOwner()), static_cast<unsigned>(runtime.failure.load()),
                                 static_cast<int>(runtime.stopping.load()), static_cast<int>(runtime.active.load()), Host::calls[0], Host::calls[1],
                                 Host::calls[2]);
                    try
                    {
                        if (requested)
                        {
                            CPed& sdk = *oldSdk;
                            output = sdk.IsOnScreen();
                        }
                        else
                        {
                            CEntity& sdk = *oldSdk;
                            output = sdk.IsOnScreen();
                        }
                        result = ClockResult::Ok;
                    }
                    catch (const std::exception& error)
                    {
                        normalCaught = true;
                        std::fprintf(stderr, "OnScreen actual SDK refused: error=%s failure=%u stopping=%d active=%d calls=%u,%u,%u\n", error.what(),
                                     static_cast<unsigned>(runtime.failure.load()), static_cast<int>(runtime.stopping.load()),
                                     static_cast<int>(runtime.active.load()), Host::calls[0], Host::calls[1], Host::calls[2]);
                    }
                };
                Check(service.Renew(runtime.generation.load()), "renew actual CEntity/CPed on-screen callback");
                fixture.invokeTimer();
                std::fprintf(stderr, "OnScreen outer return: requested=%d result=%u output=%d stopping=%d calls=%u,%u,%u reentries=%u nested=%u ground=%u\n",
                             static_cast<int>(requested), static_cast<unsigned>(result), static_cast<int>(output), static_cast<int>(runtime.stopping.load()),
                             Host::calls[0], Host::calls[1], Host::calls[2], observedReentries, static_cast<unsigned>(observedNested),
                             static_cast<unsigned>(observedGround));
                Check(observedNested == ClockResult::Busy && observedNestedOutput && observedGround == ClockResult::Busy &&
                          observedHeight == WorldGroundOwnerFixture::Bits(-173.25f) && !observedStopping,
                      "one admitted on-screen operation blocks nested on-screen and ground entry through the same real gate");
                Check(!normalCaught && result == ClockResult::Ok && output == requested && !runtime.stopping.load() &&
                          Host::calls == std::array<unsigned, 3>{before[0] + 1, before[1] + 1, before[2] + 1} && !taskScenario.eventCount,
                      "actual CEntity and CPed dispatch returns native false/true with each bound/camera helper exactly once");
            }
            Host::reenter = nullptr;
            const auto selected = token;
            Host::callerToken = &token;
            callback.action = [&](NativeGameTimingAccess& access) { result = access.ReadPedOnScreen(token, output); };
            Check(service.Renew(runtime.generation.load()), "renew before caller-token mutation in the final native seam");
            fixture.invokeTimer();
            Check(result == ClockResult::Ok && token.version == selected.version + 1 && output && !runtime.stopping.load(),
                  "Runtime fixes the caller token before any helper callback and acknowledges its original registered receiver");
            token = selected;
            // Resolve intentionally revokes publication on even transient native
            // relation loss. Exercise GC after the success cases; this issued
            // token then remains releasable, but must never be borrowed again.
            const auto gcBeforeCalls = Host::calls;
            output = true;
            callback.action = [&](NativeGameTimingAccess& access)
            {
                writes.Global(NativeBorrowedGcProfile::markerRva, LONG{1});
                result = access.ReadPedOnScreen(token, output);
                writes.Global(NativeBorrowedGcProfile::markerRva, LONG{0});
            };
            fixture.invokeTimer();
            Check(result == ClockResult::Busy && output && Host::calls == gcBeforeCalls && !runtime.stopping.load(),
                  "native GC refusal occurs before query entry and cannot Stop or manufacture a bool");
            callback.action = [&](NativeGameTimingAccess& access) { result = access.ReadPedOnScreen(token, output); };
            Check(service.Renew(runtime.generation.load()), "renew current owner after deliberate registered-borrow revocation");
            fixture.invokeTimer();
            Check(result == ClockResult::NotInitialized && output && Host::calls == gcBeforeCalls && !runtime.stopping.load(),
                  "transient native GC loss revokes this issued borrow without a false result, query entry or whole-owner Stop");
            retire(service);
        }
        // Each entered fault has its own real owner generation and retained
        // reference cell. Cleanup cannot replay a helper or undo native effects.
        for (unsigned scenario = 1; scenario != 5; ++scenario)
        {
            graph.Reset();
            Host::scenario = scenario;
            Host::nativeValue = false;
            Host::callerToken = nullptr;
            Host::reenter = nullptr;
            NativeClockService service;
            callback.action = [&](NativeGameTimingAccess& access) { result = access.AcquirePedReference(1, token); };
            BeginOwned(service, fixture, false, true, true, &WorldGroundOwnerFixture::Callback::Invoke, &callback);
            fixture.invokeTimer();
            Check(result == ClockResult::Ok, "fresh entered-fault owner acquires its own genuine registered relation");
            Host::cell = ClockPedReferenceOwnerTest::cell;
            const auto node = ClockPedReferenceOwnerTest::node;
            const auto before = Host::calls;
            if (scenario == 1)
            {
                bool staleCaught = false;
                callback.action = [&](NativeGameTimingAccess&)
                {
                    try
                    {
                        (void)oldSdk->IsOnScreen();
                    }
                    catch (const std::runtime_error& error)
                    {
                        staleCaught = std::string(error.what()).find("IsOnScreen") != std::string::npos;
                    }
                };
                fixture.invokeTimer();
                Check(staleCaught && Host::calls == before && !runtime.stopping.load(),
                      "old SDK cannot borrow a new generation despite identical native pointer reuse");
            }
            auto sdk = makePed(token);
            bool caught = false;
            output = true;
            callback.action = [&](NativeGameTimingAccess&)
            {
                try
                {
                    CEntity& entity = *sdk;
                    output = entity.IsOnScreen();
                }
                catch (const std::runtime_error& error)
                {
                    caught = std::string(error.what()).find("IsOnScreen") != std::string::npos;
                }
            };
            Check(service.Renew(runtime.generation.load()), "renew fresh one-shot entered-fault callback");
            fixture.invokeTimer();
            const std::array<unsigned, 3> expected{before[0] + 1, before[1] + (scenario >= 2), before[2] + (scenario >= 3)};
            Check(caught && output && runtime.stopping.load() && Host::calls == expected,
                  "any entered radius/center/camera fault or final reference loss Stops without a false result, retry or later helper");
            if (scenario == 4)
            {
                const auto unlinks = ClockPedReferenceOwnerTest::unregisters;
                Check(!service.End() && runtime.pedReferences.IsRetained(), "zero weak word cannot free a native node still naming the owned cell");
                fixture.invokeTimer();
                Check(runtime.pedReferences.IsRetained() && ClockPedReferenceOwnerTest::unregisters == unlinks && Host::calls == expected,
                      "stopping retains real native-cell debt without unsafe unregister or query replay");
                // Supply only the already-proved address-retirement outcome
                // on owned fixture nodes; production gains no force-release.
                ClockPedReferenceOwnerTest::Put(node + 8, std::uintptr_t{0});
                const auto freeHead = ClockPedReferenceOwnerTest::Get<std::uintptr_t>(fixture.image + R::FreeHeadRva);
                ClockPedReferenceOwnerTest::Put(node, freeHead);
                ClockPedReferenceOwnerTest::Put(fixture.image + R::FreeHeadRva, node);
                ClockPedReferenceOwnerTest::Put(Host::ped + R::EntityHeadOffset, std::uintptr_t{0});
                fixture.invokeTimer();
            }
            retire(service);
            Check(Host::calls == expected, "native on-screen retirement neither repeats helpers nor rolls back lazy geometry work");
        }
        oldSdk.reset();
        Check(ClockPedVisibilityOwnerTest::reads == visibilityReads && ClockPedVisibilityOwnerTest::stores == visibilityStores &&
                  ClockWorldGroundOwnerTest::calls == groundCalls && ClockWorldLineOwnerTest::calls == lineCalls && !taskScenario.eventCount,
              "actual on-screen query never substitutes raw visibility, another world query, tasks or a fake renderer");
        Host::image = Host::ped = Host::cell = 0;
        Host::scenario = 0;
        Host::callerToken = nullptr;
        Host::reenter = nullptr;
        writes.Restore();
        ClockReaderTestHost::image = priorReaderImage;
        ClockPedReferenceOwnerTest::image = priorReferenceImage;
        std::puts("Production native registered on-screen callback owner fixture completed");
        std::puts("Production actual CEntity CPed on-screen SDK callback fixture completed");
    }
}
