/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Registered local-player render rebuild through the real callback owner
 ****************************************************************************/
namespace PedRebuildOwnerFixture
{
    void Run(ClockImageFixture& fixture)
    {
        namespace P = NativePedRebuildProfile;
        namespace R = NativePedReferenceProfile;
        namespace Ped = NativePedPoolProfile;
        namespace Player = NativePlayerInfoProfile;
        namespace Host = ClockPedRebuildOwnerTest;
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
        for (const auto& pin : NativePedMetadataProfile::Code)
            WorldGroundOwnerFixture::Writes::Apply(&writes, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& slot : R::Slots)
            writes.Global(slot.rva, fixture.image + slot.targetRva, PAGE_READONLY);
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
        alignas(std::uintptr_t) std::array<unsigned char, NativeModelReferenceProfile::MapOffset + NativeModelReferenceProfile::HeaderBytes> root{};
        NativeStreamingLoadAllFixture::Graph graph(fixture.image, reinterpret_cast<std::uintptr_t>(root.data()), &WorldGroundOwnerFixture::Writes::Apply,
                                                   &writes);
        NativePedRebuildFixture::Install(fixture.image, &WorldGroundOwnerFixture::Writes::Apply, &writes);
        NativePedClothesFixture::NameState names;
        names.Reset(fixture.image, &WorldGroundOwnerFixture::Writes::Apply, &writes);
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
        alignas(std::uintptr_t) std::array<unsigned char, P::ClothesBytes>  clothes{}, otherClothes{};
        alignas(std::uintptr_t) std::array<unsigned char, Player::RowBytes> privateData{};
        const auto                                                          ped = reinterpret_cast<std::uintptr_t>(pool.objects);
        const auto                                                          clothesAddress = reinterpret_cast<std::uintptr_t>(clothes.data());
        const auto                                                          privateAddress = reinterpret_cast<std::uintptr_t>(privateData.data());
        const auto store = [](unsigned char* bytes, std::size_t offset, const auto& value) { std::memcpy(bytes + offset, &value, sizeof(value)); };
        const auto bindPlayer = [&]
        {
            const auto table = fixture.image + Player::PlayerVtableRva;
            const auto data = fixture.image + Player::BankRva + 8;
            for (unsigned slot = 0; slot != 2; ++slot)
            {
                auto* object = pool.objects + slot * Ped::ObjectStride;
                store(object, 0, table);
                store(object, 0x5f0, data);
                store(object, 0x7e0, std::int32_t{0});
                object[R::TypeOffset] = 3;
            }
            writes.Global(Player::PlayerVtableRva + 0x50, fixture.image + Player::PlayerControlRva, PAGE_READONLY);
            writes.Global(Player::PlayerIndexRva, static_cast<unsigned char>(0));
            writes.Global(Player::BankRva, ped);
            writes.Global(P::ClothesSlotRva, clothesAddress);
        };
        bindPlayer();
        namespace Request = NativeStreamingRequestProfile;
        namespace Ref = NativeModelReferenceProfile;
        namespace U = NativeStartupProvidersProfile;
        alignas(std::uintptr_t) std::array<unsigned char, Request::ProviderCountOffset + sizeof(std::int32_t)> provider{};
        alignas(std::uintptr_t) std::array<unsigned char, Request::ProviderRowStride>                          providerRows{};
        alignas(std::uintptr_t) std::array<unsigned char, Request::ModelPrefixBytes>                           model{};
        alignas(std::uintptr_t) std::array<unsigned char, 12 * U::objectItemBytes>                             liveItems{};
        std::array<std::uintptr_t, 1>                                                                          liveChunks{};
        struct alignas(std::uintptr_t) RefRow
        {
            std::uint32_t key = 0, count = 7;
            std::int32_t  next = -1, bucket = 0;
        };
        std::array<RefRow, 4>          references{};
        const auto                     modelAddress = reinterpret_cast<std::uintptr_t>(model.data());
        NativePedRebuildFixture::State state;
        Check(state.Ready(), "own actual-sized intelligence/task pools and separate current render receivers for this rebuild");
        const auto reset = [&]
        {
            graph.Reset();
            bindPlayer();
            pool.flags[0] = 1;
            pool.Publish();
            provider.fill(0);
            providerRows.fill(0);
            model.fill(0);
            liveItems.fill(0);
            std::memcpy(liveItems.data(), graph.items.data(), graph.items.size());
            const auto providerAddress = reinterpret_cast<std::uintptr_t>(provider.data());
            store(provider.data(), 0, fixture.image + Request::RootVtableRva);
            store(provider.data(), NativeStartupWorldProfile::objectIndexOffset, std::int32_t{11});
            store(provider.data(), NativeStartupWorldProfile::objectClassOffset, graph.Class(0));
            store(provider.data(), Request::ProviderRowsOffset, reinterpret_cast<std::uintptr_t>(providerRows.data()));
            store(provider.data(), Request::ProviderCountOffset, std::int32_t{1});
            store(root.data(), Request::ProviderOffset, providerAddress);
            store(liveItems.data(), 11 * U::objectItemBytes, providerAddress);
            store(liveItems.data(), 11 * U::objectItemBytes + 0x10, std::uint32_t{111});
            liveChunks[0] = reinterpret_cast<std::uintptr_t>(liveItems.data());
            writes.Global(U::objectChunksRva, reinterpret_cast<std::uintptr_t>(liveChunks.data()));
            writes.Global(U::objectCountRva, std::int32_t{12});
            // This is the existing reference fixture's one occupied sparse-map
            // row, keyed to current model0; it creates no production registry.
            references = {};
            auto* map = root.data() + Ref::MapOffset;
            store(map, 0, reinterpret_cast<std::uintptr_t>(references.data()));
            store(map, 8, std::int32_t{1});
            store(map, 0xc, std::int32_t{4});
            store(map, 0x10, std::uint32_t{1});
            store(map, 0x28, std::int32_t{1});
            store(map, 0x2c, std::int32_t{128});
            store(map, 0x30, std::int32_t{-1});
            store(map, 0x34, std::int32_t{0});
            store(map, 0x38, std::int32_t{0});
            store(map, 0x40, std::uintptr_t{0});
            store(map, 0x48, std::int32_t{1});
            store(model.data(), 0, fixture.image + 0x4301000);
            for (const auto& pin : NativeModelBankProfile::Code)
                WorldGroundOwnerFixture::Writes::Apply(&writes, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
            writes.Global(0x4301000 + Request::AnimationMethodSlot, fixture.image + NativePedRebuildFixture::RenderEventRva, PAGE_READONLY);
            state.Reset(fixture.image, ped, modelAddress, reinterpret_cast<std::uintptr_t>(liveItems.data()), &WorldGroundOwnerFixture::Writes::Apply, &writes);
            names.Reset(fixture.image, &WorldGroundOwnerFixture::Writes::Apply, &writes);
        };
        reset();
        const auto priorReaderImage = ClockReaderTestHost::image;
        const auto priorReferenceImage = ClockPedReferenceOwnerTest::image;
        ClockReaderTestHost::image = ClockPedReferenceOwnerTest::image = fixture.image;
        ClockPedReferenceOwnerTest::partialUnlink = false;
        ClockPedReferenceOwnerTest::unregisterResult = ClockResult::Ok;
        Host::image = fixture.image;
        Host::ped = ped;
        Host::calls = Host::effects = 0;
        Host::scenario = 0;
        Host::callerToken = nullptr;
        Host::reenter = nullptr;
        Host::afterNative = nullptr;
        Host::afterContext = nullptr;
        Check(ClockReaderTestHost::Module(nullptr) == reinterpret_cast<HMODULE>(fixture.image),
              "rebuild scope selects its one owned image before player-info, native thread and registered-ped qualification");
        struct DefaultRebuildAccess final : NativePedRebuildAccess
        {
        };
        DefaultRebuildAccess absent;
        Check(absent.RebuildPed({}) == ClockResult::NotInitialized, "an older owner does not invent a completed native rebuild");
        ped_rebuild_fixture::GameBase     game;
        ped_rebuild_fixture::TaskScenario tasks;
        ped_rebuild_fixture::WeaponLookup lookup;
        const auto                        makePed = [&](const NativePedReferenceToken& token)
        {
            auto& access = runtime.access;
            return std::make_unique<ped_rebuild_fixture::PedRebuildFixture>(
                game, access, access, access, access, access, access, access, access, access, access, access, access, access, access, access, access, access,
                access, access, access, access, access, access, access, access, access, access, token, 0x12345678, tasks, lookup);
        };
        const auto retire = [&](NativeClockService& service)
        {
            WorldGroundOwnerFixture::Retire(service, fixture);
            Check(!runtime.pedReferences.IsRetained() && !runtime.playerInfo.IsBound() && !runtime.playerInfoGeneration,
                  "rebuild retirement acknowledges existing registered cells and local-player reader without undoing native render effects");
        };
        struct Effect
        {
            NativePedRebuildFixture::State*  state;
            WorldGroundOwnerFixture::Writes* writes;
            std::uintptr_t                   image, ped;
            unsigned char*                   model;
            RefRow*                          row;
        } effect{&state, &writes, fixture.image, ped, model.data(), references.data()};
        const auto recreate = +[](void* opaque) noexcept
        {
            auto& selected = *static_cast<Effect*>(opaque);
            selected.state->Recreate(selected.image, selected.ped);
            ++selected.row->count;
            std::uint16_t count = 0;
            std::memcpy(&count, selected.model + P::ModelReferenceOffset, sizeof(count));
            ++count;
            std::memcpy(selected.model + P::ModelReferenceOffset, &count, sizeof(count));
            const float changedStat = 30.0f;
            selected.writes->Global(P::FirstStatRva, changedStat);
            // The genuine entry may abort/remove its old secondary task. A
            // current empty slot is an accepted effect, not a cached old task.
            const std::uintptr_t empty = 0;
            std::memcpy(selected.state->intelligenceObjects + 0x58, &empty, sizeof(empty));
        };
        WorldGroundOwnerFixture::Callback callback;
        NativePedReferenceToken           token;
        ClockResult                       result = ClockResult::NotInitialized;
        {
            NativeClockService service;
            callback.action = [&](NativeGameTimingAccess& access) { result = access.AcquirePedReference(1, token); };
            BeginOwned(service, fixture, false, true, true, &WorldGroundOwnerFixture::Callback::Invoke, &callback);
            fixture.invokeTimer();
            Check(result == ClockResult::Ok && token.version && runtime.pedReferences.IsRetained(),
                  "rebuild borrows the same actual registered local CJ instead of creating render or player lifetime authority");
            Host::selected = token;
            Host::cell = ClockPedReferenceOwnerTest::cell;
            Check(runtime.access.RebuildPed(token) != ClockResult::Ok && !Host::calls, "retained token does not admit rebuild outside its current callback");
            std::thread foreign([&] { result = runtime.access.RebuildPed(token); });
            foreign.join();
            Check(result != ClockResult::Ok && !Host::calls && !runtime.stopping.load(),
                  "foreign rebuild refuses before native entry without stopping the actual owner");
            callback.action = [&](NativeGameTimingAccess& access) { result = access.RebuildPed(token); };
            Check(NativeStreamingCompletionContext::TryEnter(), "hold the actual shared native completion gate before rebuild intent");
            fixture.invokeTimer();
            NativeStreamingCompletionContext::Leave();
            Check(result == ClockResult::Busy && !Host::calls && !runtime.stopping.load(),
                  "rebuild shares existing native exclusion before its whole genuine call");
            const auto oldRender = state.Render(), oldContainer = state.Container(), preservedHead = state.Head();
            state.Secondary(fixture.image, 0);
            Host::afterNative = recreate;
            Host::afterContext = &effect;
            Host::callerToken = &token;
            const auto         copied = token;
            static ClockResult nestedRebuild = ClockResult::NotInitialized, nestedClothes = ClockResult::NotInitialized;
            Host::reenter = +[]() noexcept
            {
                nestedRebuild = runtime.access.RebuildPed(Host::selected);
                nestedClothes = runtime.access.SetPedClothes(Host::selected, nullptr, nullptr, 0);
            };
            Check(service.Renew(runtime.generation.load()), "renew whole genuine rebuild with current model/render/secondary-task state");
            fixture.invokeTimer();
            Check(result == ClockResult::Ok && Host::calls == 1 && Host::entry == fixture.image + P::EntryRva && Host::receivedPed == ped &&
                      state.Render() != oldRender && state.Container() != oldContainer && state.Head() == preservedHead &&
                      token.version == copied.version + 1 && nestedRebuild == ClockResult::Busy && nestedClothes == ClockResult::Busy &&
                      !runtime.stopping.load(),
                  "one genuine single-ped ABI entry accepts fresh render/container and committed reference/stat/task effects with copied token and no nested "
                  "replay");
            token = copied;
            Host::reenter = nullptr;
            auto       sdk = makePed(token);
            bool       caught = false;
            const auto beforeSdk = Host::calls;
            callback.action = [&](NativeGameTimingAccess&)
            {
                try
                {
                    CPed& pedSdk = *sdk;
                    pedSdk.RebuildPlayer();
                }
                catch (const std::exception& error)
                {
                    caught = true;
                    std::fprintf(stderr, "Actual rebuild SDK refused: %s\n", error.what());
                }
            };
            Check(service.Renew(runtime.generation.load()), "renew actual CPed RebuildPlayer dispatch through the current native owner");
            fixture.invokeTimer();
            Check(
                !caught && Host::calls == beforeSdk + 1 && Host::receivedPed == ped && !tasks.eventCount && !lookup.count && !runtime.stopping.load(),
                "actual void CPed RebuildPlayer reaches the whole DE entry once without a fabricated second classic argument, task service or wrapper factory");
            Host::afterNative = nullptr;
            Host::afterContext = nullptr;
            retire(service);
        }
        for (unsigned fault = 0; fault != 6; ++fault)
        {
            reset();
            NativeClockService service;
            callback.action = [&](NativeGameTimingAccess& access) { result = access.AcquirePedReference(1, token); };
            BeginOwned(service, fixture, false, true, true, &WorldGroundOwnerFixture::Callback::Invoke, &callback);
            fixture.invokeTimer();
            Check(result == ClockResult::Ok, "fresh rebuild refusal owner acquires only its existing registered relation");
            const auto before = Host::calls;
            const auto priorGeneration = runtime.playerInfoGeneration;
            if (fault == 0)
                writes.Global(NativeGameStateProfile::stateRva, std::uint32_t{7});
            if (fault == 1)
                writes.Global(NativeStartGameProfile::ThreadIdRva, GetCurrentThreadId() + 1);
            if (fault == 2)
                writes.Global(NativeBorrowedGcProfile::markerRva, LONG{1});
            if (fault == 3)
                pool.flags[0] = 2;

            if (fault == 4)
                runtime.playerInfoGeneration = runtime.generation.load() + 1;
            if (fault == 5)
                writes.Global(Ped::RootRva, std::uintptr_t{0});
            callback.action = [&](NativeGameTimingAccess& access) { result = access.RebuildPed(token); };
            Check(service.Renew(runtime.generation.load()), "renew one independent native rebuild admission guard");
            fixture.invokeTimer();
            Check(result != ClockResult::Ok && Host::calls == before && !runtime.stopping.load(),
                  "state/GT/GC/pool/current-generation refusal precedes native entry with no Stop or replay");
            if (fault == 4)
                runtime.playerInfoGeneration = priorGeneration;
            writes.Global(NativeGameStateProfile::stateRva, std::uint32_t{9});
            writes.Global(NativeStartGameProfile::ThreadIdRva, GetCurrentThreadId());
            writes.Global(NativeBorrowedGcProfile::markerRva, LONG{0});
            reset();
            retire(service);
        }
        for (unsigned fault = 1; fault != 5; ++fault)
        {
            reset();
            NativeClockService service;
            callback.action = [&](NativeGameTimingAccess& access) { result = access.AcquirePedReference(1, token); };
            BeginOwned(service, fixture, false, true, true, &WorldGroundOwnerFixture::Callback::Invoke, &callback);
            fixture.invokeTimer();
            Check(result == ClockResult::Ok, "fresh entered rebuild fault acquires its own registered relation");
            Host::cell = ClockPedReferenceOwnerTest::cell;
            const auto node = ClockPedReferenceOwnerTest::node;
            const auto before = Host::calls;
            const auto oldRender = state.Render();
            Host::scenario = fault <= 2 ? fault : 0;
            struct FaultEffect
            {
                Effect*        effect;
                unsigned       fault;
                std::uintptr_t privateData, otherClothes;
            } change{&effect, fault, privateAddress, reinterpret_cast<std::uintptr_t>(otherClothes.data())};
            Host::afterContext = &change;
            Host::afterNative = +[](void* opaque) noexcept
            {
                auto& selected = *static_cast<FaultEffect*>(opaque);
                auto& effect = *selected.effect;
                effect.state->Recreate(effect.image, effect.ped);
                if (selected.fault == 3)
                    std::memcpy(reinterpret_cast<void*>(effect.ped + 0x5f0), &selected.privateData, sizeof(selected.privateData));
                if (selected.fault == 4)
                    effect.writes->Global(P::ClothesSlotRva, selected.otherClothes);
            };
            callback.action = [&](NativeGameTimingAccess& access) { result = access.RebuildPed(token); };
            Check(service.Renew(runtime.generation.load()), "renew one-shot entered rebuild fault");
            fixture.invokeTimer();
            Check(result != ClockResult::Ok && runtime.stopping.load() && Host::calls == before + 1 && state.Render() != oldRender,
                  "entered exception/lost weak word/data/descriptor Stops once while preserving already recreated render state");
            if (fault == 1)
                Check(result == ClockResult::OriginalException, "genuine rebuild exception status is not replaced during owner Stop");
            Host::afterNative = nullptr;
            Host::afterContext = nullptr;
            bindPlayer();
            if (fault == 2)
            {
                const auto unlinks = ClockPedReferenceOwnerTest::unregisters;
                Check(!service.End() && runtime.pedReferences.IsRetained(), "uncertain rebuilt weak word retains the genuine native node and owned cell");
                fixture.invokeTimer();
                Check(runtime.pedReferences.IsRetained() && ClockPedReferenceOwnerTest::unregisters == unlinks && Host::calls == before + 1,
                      "rebuild retirement does not blindly unlink debt or replay native render effects");
                ClockPedReferenceOwnerTest::Put(node + 8, std::uintptr_t{0});
                const auto head = ClockPedReferenceOwnerTest::Get<std::uintptr_t>(fixture.image + R::FreeHeadRva);
                ClockPedReferenceOwnerTest::Put(node, head);
                ClockPedReferenceOwnerTest::Put(fixture.image + R::FreeHeadRva, node);
                ClockPedReferenceOwnerTest::Put(ped + R::EntityHeadOffset, std::uintptr_t{0});
                fixture.invokeTimer();
            }
            retire(service);
            Check(Host::calls == before + 1, "End acknowledges existing rebuild retirement without another native entry");
            Host::scenario = 0;
        }
        Host::image = Host::ped = Host::cell = 0;
        Host::scenario = 0;
        Host::callerToken = nullptr;
        Host::reenter = nullptr;
        Host::afterNative = nullptr;
        Host::afterContext = nullptr;
        writes.Restore();
        ClockReaderTestHost::image = priorReaderImage;
        ClockPedReferenceOwnerTest::image = priorReferenceImage;
        std::puts("Production native registered rebuild callback owner fixture completed");
        std::puts("Production actual CPed rebuild SDK callback fixture completed");
    }
}
