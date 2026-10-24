/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Registered local-player clothes through the real callback owner
 ****************************************************************************/
namespace PedClothesOwnerFixture
{
    void Run(ClockImageFixture& fixture)
    {
        namespace P = NativePedClothesProfile;
        namespace R = NativePedReferenceProfile;
        namespace Ped = NativePedPoolProfile;
        namespace Player = NativePlayerInfoProfile;
        namespace Host = ClockPedClothesOwnerTest;
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
        alignas(std::uintptr_t) std::array<unsigned char, 0x180> root{};
        NativeStreamingLoadAllFixture::Graph graph(fixture.image, reinterpret_cast<std::uintptr_t>(root.data()), &WorldGroundOwnerFixture::Writes::Apply,
                                                   &writes);
        NativePedClothesFixture::Install(fixture.image, &WorldGroundOwnerFixture::Writes::Apply, &writes);
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
        Host::callerTexture = nullptr;
        Host::reenter = nullptr;
        Host::afterNative = nullptr;
        Host::afterContext = nullptr;
        Check(ClockReaderTestHost::Module(nullptr) == reinterpret_cast<HMODULE>(fixture.image),
              "clothes scope selects its one owned image before player-info, native thread and registered-ped qualification");
        struct DefaultClothesAccess final : NativePedClothesAccess
        {
        };
        DefaultClothesAccess absent;
        Check(absent.SetPedClothes({}, nullptr, nullptr, 0) == ClockResult::NotInitialized,
              "an older owner refuses rather than pretending that a real clothes setter or player factory exists");
        ped_clothes_fixture::GameBase     game;
        ped_clothes_fixture::TaskScenario tasks;
        ped_clothes_fixture::WeaponLookup lookup;
        const auto                        makePed = [&](const NativePedReferenceToken& token)
        {
            auto& access = runtime.access;
            return std::make_unique<ped_clothes_fixture::PedClothesFixture>(
                game, access, access, access, access, access, access, access, access, access, access, access, access, access, access, access, access, access,
                access, access, access, access, access, access, access, access, access, token, 0x12345678, tasks, lookup);
        };
        const auto retire = [&](NativeClockService& service)
        {
            WorldGroundOwnerFixture::Retire(service, fixture);
            Check(!runtime.pedReferences.IsRetained() && !runtime.playerInfo.IsBound() && !runtime.playerInfoGeneration,
                  "clothes retirement releases the existing registered cells and generation-bound player reader without native rewind");
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
                  "clothes borrows the actual registered local player instead of creating a native ped or private player-data owner");
            Host::selected = token;
            Host::cell = ClockPedReferenceOwnerTest::cell;
            Check(runtime.access.SetPedClothes(token, "vest", "vest", 0) != ClockResult::Ok && !Host::calls,
                  "a retained ped token grants no clothes authority outside the current admitted callback");
            std::thread foreign([&] { result = runtime.access.SetPedClothes(token, "vest", "vest", 0); });
            foreign.join();
            Check(result != ClockResult::Ok && !Host::calls && !runtime.stopping.load(),
                  "foreign clothes intent refuses without native entry, fabricated local identity or stopping the valid owner");
            writes.Global(P::ClothesSlotRva, std::uintptr_t{0});
            callback.action = [&](NativeGameTimingAccess& access)
            { result = access.SetPedClothes(token, reinterpret_cast<const char*>(std::uintptr_t{1}), reinterpret_cast<const char*>(std::uintptr_t{2}), 255); };
            fixture.invokeTimer();
            Check(result == ClockResult::Ok && !Host::calls && runtime.playerInfo.IsBound() && !runtime.stopping.load(),
                  "genuine null clothes succeeds before invalid part and unreadable names, after real local-player and token qualification");
            writes.Global(P::ClothesSlotRva, clothesAddress);
            callback.action = [&](NativeGameTimingAccess& access) { result = access.SetPedClothes(token, nullptr, nullptr, 0); };
            Check(NativeStreamingCompletionContext::TryEnter(), "hold the actual shared native completion gate before clothes intent");
            fixture.invokeTimer();
            NativeStreamingCompletionContext::Leave();
            Check(result == ClockResult::Busy && !Host::calls && !runtime.stopping.load(),
                  "clothes excludes existing native name/streaming work before any setter without replacing its owner");
            Check(service.Renew(runtime.generation.load()), "renew the native null-name clothes clear");
            fixture.invokeTimer();
            Check(result == ClockResult::Ok && Host::calls == 1 && Host::effects == 1 && Host::textureNull && Host::modelNull &&
                      Host::receivedClothes == clothesAddress && Host::receivedPart == 0 && Host::entry == fixture.image + P::EntryRva,
                  "nullable clear calls the genuine descriptor setter once with its exact ABI and no rebuild");
            std::array<char, 6> texture{'v', 'e', 's', 't', '1', '\0'};
            const auto          fixedToken = token;
            static ClockResult  nested = ClockResult::NotInitialized;
            Host::callerToken = &token;
            Host::callerTexture = texture.data();
            Host::reenter = +[]() noexcept { nested = runtime.access.SetPedClothes(Host::selected, "nested", "nested", 1); };
            struct CacheEffect
            {
                NativePedClothesFixture::NameState* names;
                WorldGroundOwnerFixture::Writes*    writes;
                std::uintptr_t                      image;
            } cache{&names, &writes, fixture.image};
            Host::afterContext = &cache;
            Host::afterNative = +[](void* opaque) noexcept
            {
                auto& selected = *static_cast<CacheEffect*>(opaque);
                selected.names->Cache(selected.image, &WorldGroundOwnerFixture::Writes::Apply, selected.writes, true);
            };
            callback.action = [&](NativeGameTimingAccess& access) { result = access.SetPedClothes(token, texture.data(), "model1", 17); };
            Check(service.Renew(runtime.generation.load()), "renew copied names and shared-gate native clothes intent");
            fixture.invokeTimer();
            Check(result == ClockResult::Ok && Host::calls == 2 && Host::effects == 2 && Host::receivedPart == 17 &&
                      std::strcmp(Host::receivedTexture.data(), "vest1") == 0 && std::strcmp(Host::receivedModel.data(), "model1") == 0 &&
                      token.version == fixedToken.version + 1 && texture[0] == '!' && nested == ClockResult::Busy && !runtime.stopping.load(),
                  "setter uses copied token/names, refuses nested native work and accepts committed descriptor/name-cache changes without replay");
            token = fixedToken;
            Host::reenter = nullptr;
            Host::afterNative = nullptr;
            Host::afterContext = nullptr;
            auto sdk = makePed(token);
            bool caught = false;
            callback.action = [&](NativeGameTimingAccess&)
            {
                try
                {
                    CPed& pedSdk = *sdk;
                    pedSdk.SetClothesTextureAndModel("shirt", nullptr, 1);
                }
                catch (const std::exception& error)
                {
                    caught = true;
                    std::fprintf(stderr, "Actual clothes SDK refused: %s\n", error.what());
                }
            };
            const auto beforeSdk = Host::calls;
            Check(service.Renew(runtime.generation.load()), "renew actual CPed clothes SDK dispatch through the real owner");
            fixture.invokeTimer();
            Check(!caught && Host::calls == beforeSdk + 1 && Host::receivedPart == 1 && Host::modelNull && !tasks.eventCount && !lookup.count &&
                      !runtime.stopping.load(),
                  "actual void CPed SetClothesTextureAndModel reaches the genuine setter without a rebuild, model request, task or weapon-wrapper lookup");
            retire(service);
        }
        // Refusal may revoke published tokens, so each guard gets its own
        // actual registered relation. No later happy case reuses that token.
        for (unsigned fault = 0; fault != 14; ++fault)
        {
            graph.Reset();
            bindPlayer();
            pool.flags[0] = 1;
            Host::scenario = 0;
            NativeClockService service;
            callback.action = [&](NativeGameTimingAccess& access) { result = access.AcquirePedReference(1, token); };
            BeginOwned(service, fixture, false, true, true, &WorldGroundOwnerFixture::Callback::Invoke, &callback);
            fixture.invokeTimer();
            Check(result == ClockResult::Ok, "fresh pre-entry clothes refusal owner acquires its own registered relation");
            const auto                            before = Host::calls;
            const auto                            generation = runtime.playerInfoGeneration;
            std::array<char, P::MaximumNameBytes> unterminated{};
            unterminated.fill('a');
            const char   nonAscii[] = {'x', static_cast<char>(0x80), '\0'};
            const char*  name = "shirt";
            std::uint8_t part = 1;
            if (fault == 0)
                writes.Global(NativeGameStateProfile::stateRva, std::uint32_t{7});
            if (fault == 1)
                writes.Global(NativeStartGameProfile::ThreadIdRva, GetCurrentThreadId() + 1);
            if (fault == 2)
                writes.Global(NativeBorrowedGcProfile::markerRva, LONG{1});
            if (fault == 3)
                pool.flags[0] = 2;
            if (fault == 4)
                writes.Global(Player::BankRva, ped + Ped::ObjectStride);
            if (fault == 5)
                store(pool.objects, 0x5f0, privateAddress);
            if (fault == 6)
                store(pool.objects, 0x7e0, std::int32_t{1});
            if (fault == 7)
                writes.Global(P::ClothesSlotRva, std::uintptr_t{1});
            if (fault == 8)
                name = reinterpret_cast<const char*>(std::uintptr_t{1});
            if (fault == 9)
                name = nonAscii;
            if (fault == 10)
                name = unterminated.data();
            if (fault == 11)
                part = 18;
            if (fault == 12)
                runtime.playerInfoGeneration = runtime.generation.load() + 1;
            if (fault == 13)
                writes.Global(Ped::RootRva, std::uintptr_t{0});
            callback.action = [&](NativeGameTimingAccess& access) { result = access.SetPedClothes(token, name, "model", part); };
            Check(service.Renew(runtime.generation.load()), "renew one independent clothes admission guard");
            fixture.invokeTimer();
            Check(result != ClockResult::Ok && Host::calls == before && !runtime.stopping.load(),
                  "state/GT/GC/generation/local-player/data/descriptor/name/part refusal occurs before native entry without retry or Stop");
            if (fault == 12)
                runtime.playerInfoGeneration = generation;
            writes.Global(NativeGameStateProfile::stateRva, std::uint32_t{9});
            writes.Global(NativeStartGameProfile::ThreadIdRva, GetCurrentThreadId());
            writes.Global(NativeBorrowedGcProfile::markerRva, LONG{0});
            pool.flags[0] = 1;
            pool.Publish();
            bindPlayer();
            retire(service);
        }
        for (unsigned fault = 1; fault != 5; ++fault)
        {
            graph.Reset();
            bindPlayer();
            Host::scenario = fault <= 2 ? fault : 0;
            NativeClockService service;
            callback.action = [&](NativeGameTimingAccess& access) { result = access.AcquirePedReference(1, token); };
            BeginOwned(service, fixture, false, true, true, &WorldGroundOwnerFixture::Callback::Invoke, &callback);
            fixture.invokeTimer();
            Check(result == ClockResult::Ok, "fresh entered clothes failure owner acquires its own registered relation");
            Host::cell = ClockPedReferenceOwnerTest::cell;
            const auto node = ClockPedReferenceOwnerTest::node;
            struct PointerEffect
            {
                unsigned char*                   ped;
                std::uintptr_t                   data;
                WorldGroundOwnerFixture::Writes* writes;
                std::uintptr_t                   clothes;
            } change{pool.objects, privateAddress, &writes, reinterpret_cast<std::uintptr_t>(otherClothes.data())};
            Host::afterContext = &change;
            if (fault == 3)
                Host::afterNative = +[](void* opaque) noexcept
                {
                    auto& selected = *static_cast<PointerEffect*>(opaque);
                    std::memcpy(selected.ped + 0x5f0, &selected.data, sizeof(selected.data));
                };
            if (fault == 4)
                Host::afterNative = +[](void* opaque) noexcept
                {
                    auto& selected = *static_cast<PointerEffect*>(opaque);
                    selected.writes->Global(P::ClothesSlotRva, selected.clothes);
                };
            const auto before = Host::calls;
            callback.action = [&](NativeGameTimingAccess& access) { result = access.SetPedClothes(token, "shirt", "model", 1); };
            Check(service.Renew(runtime.generation.load()), "renew one-shot entered clothes fault");
            fixture.invokeTimer();
            Check(result != ClockResult::Ok && runtime.stopping.load() && Host::calls == before + 1,
                  "entered native exception or lost final receiver/data/descriptor relation Stops without another setter or rollback");
            unsigned effect = 0;
            std::memcpy(&effect, clothes.data(), sizeof(effect));
            Check(effect == Host::effects, "an entered clothes effect remains committed while the real owner retires");
            if (fault == 1)
                Check(result == ClockResult::OriginalException, "native clothes exception retains its original status");
            Host::afterNative = nullptr;
            Host::afterContext = nullptr;
            bindPlayer();
            if (fault == 2)
            {
                const auto unlinks = ClockPedReferenceOwnerTest::unregisters;
                Check(!service.End() && runtime.pedReferences.IsRetained(), "lost clothes weak word retains the native node naming its owned cell");
                fixture.invokeTimer();
                Check(runtime.pedReferences.IsRetained() && ClockPedReferenceOwnerTest::unregisters == unlinks && Host::calls == before + 1,
                      "uncertain clothes reference retirement never unlinks blindly or repeats the native setter");
                ClockPedReferenceOwnerTest::Put(node + 8, std::uintptr_t{0});
                const auto head = ClockPedReferenceOwnerTest::Get<std::uintptr_t>(fixture.image + R::FreeHeadRva);
                ClockPedReferenceOwnerTest::Put(node, head);
                ClockPedReferenceOwnerTest::Put(fixture.image + R::FreeHeadRva, node);
                ClockPedReferenceOwnerTest::Put(ped + R::EntityHeadOffset, std::uintptr_t{0});
                fixture.invokeTimer();
            }
            retire(service);
            Check(Host::calls == before + 1, "clothes End acknowledges existing debt without repeating descriptor effects");
        }
        Host::image = Host::ped = Host::cell = 0;
        Host::scenario = 0;
        Host::callerToken = nullptr;
        Host::callerTexture = nullptr;
        Host::reenter = nullptr;
        Host::afterNative = nullptr;
        Host::afterContext = nullptr;
        writes.Restore();
        ClockReaderTestHost::image = priorReaderImage;
        ClockPedReferenceOwnerTest::image = priorReferenceImage;
        std::puts("Production native registered clothes callback owner fixture completed");
        std::puts("Production actual CPed clothes SDK callback fixture completed");
    }
}
