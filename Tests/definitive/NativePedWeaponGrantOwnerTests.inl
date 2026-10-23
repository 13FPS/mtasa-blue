/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: One native weapon grant on the actual registered-ped callback owner
 *****************************************************************************/
namespace PedWeaponGrantOwnerFixture
{
    void Run(ClockImageFixture& fixture)
    {
        namespace P = NativePedWeaponGrantProfile;
        namespace R = NativePedReferenceProfile;
        namespace Ped = NativePedPoolProfile;
        namespace Host = ClockPedWeaponGrantOwnerTest;
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
        NativePedWeaponGrantFixture::Install(fixture.image, &WorldGroundOwnerFixture::Writes::Apply, &writes);
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
        // Native grant consumes real bound weapon rows with -1 model
        // sentinels until a readiness case explicitly supplies a model row.
        // No CModelInfo implementation or private CWeapon bank is published.
        const auto priorReaderImage = ClockReaderTestHost::image;
        const auto priorReferenceImage = ClockPedReferenceOwnerTest::image;
        ClockReaderTestHost::image = ClockPedReferenceOwnerTest::image = fixture.image;
        ClockPedReferenceOwnerTest::partialUnlink = false;
        ClockPedReferenceOwnerTest::unregisterResult = ClockResult::Ok;
        Check(ClockReaderTestHost::Module(nullptr) == reinterpret_cast<HMODULE>(fixture.image),
              "weapon-grant scope selects the owned native image before thread/world/ped checks");
        Host::image = fixture.image;
        Host::ped = reinterpret_cast<std::uintptr_t>(pool.objects);
        Host::calls = Host::effects = 0;
        Host::scenario = 0;
        Host::callerToken = nullptr;
        Host::reenter = nullptr;
        const std::uint32_t priorType = 23;
        std::memcpy(pool.objects + NativePedWeaponProfile::ArrayOffset + 2 * NativePedWeaponProfile::Stride, &priorType, sizeof(priorType));
        pool.objects[P::CurrentSlotOffset] = 2;
        const auto currentSlotBefore = pool.objects[P::CurrentSlotOffset];
        struct DefaultGrantAccess final : NativePedWeaponGrantAccess
        {
        };
        DefaultGrantAccess absent;
        std::uint32_t      slot = 0xdecafbad;
        Check(absent.GrantPedWeapon({}, WEAPONTYPE_PISTOL, 7, slot) == ClockResult::NotInitialized && slot == 0xdecafbad,
              "older weapon grant owner refuses without fabricating native slot0 or mutation success");
        struct Game final : ped_give_weapon_fixture::GameBase
        {
            unsigned     infoCalls = 0, modelCalls = 0;
            CWeaponInfo* GetWeaponInfo(eWeaponType type, eWeaponSkill skill) override
            {
                (void)type;
                (void)skill;
                ++infoCalls;
                return nullptr;
            }
            CModelInfo* GetModelInfo(DWORD model, bool canBeInvalid) override
            {
                (void)model;
                (void)canBeInvalid;
                ++modelCalls;
                return nullptr;
            }
        } game;
        ped_give_weapon_fixture::TaskScenario tasks;
        ped_give_weapon_fixture::WeaponLookup lookup;
        ped_give_weapon_fixture::Weapon       returnedIdentity;
        lookup.bank[0] = &returnedIdentity;
        const auto makePed = [&](const NativePedReferenceToken& token)
        {
            auto& access = runtime.access;
            return std::make_unique<ped_give_weapon_fixture::PedGiveWeaponFixture>(
                game, access, access, access, access, access, access, access, access, access, access, access, access, access, access, access, access, access,
                access, access, access, access, access, access, access, token, 0x12345678, tasks, lookup);
        };
        const auto retire = [&](NativeClockService& service)
        {
            WorldGroundOwnerFixture::Retire(service, fixture);
            Check(!runtime.pedReferences.IsRetained(), "weapon grant retirement acknowledges registered cells and callback admission together");
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
                  "weapon grant borrows the sole genuine registered-ped controller");
            Host::selected = token;
            Host::cell = ClockPedReferenceOwnerTest::cell;
            Check(runtime.access.GrantPedWeapon(token, WEAPONTYPE_PISTOL, 7, slot) != ClockResult::Ok && slot == 0xdecafbad && !Host::calls,
                  "retained SDK token cannot lend native grant authority outside its natural callback");
            std::thread foreign([&] { result = runtime.access.GrantPedWeapon(token, WEAPONTYPE_PISTOL, 7, slot); });
            foreign.join();
            Check(result != ClockResult::Ok && slot == 0xdecafbad && !Host::calls && !runtime.stopping.load(),
                  "foreign grant preserves caller slot and cannot Stop the real owner");
            callback.action = [&](NativeGameTimingAccess& access) { result = access.GrantPedWeapon(token, WEAPONTYPE_PISTOL, 7, slot); };
            Check(NativeStreamingCompletionContext::TryEnter(), "hold existing native effect gate before a grant intent");
            fixture.invokeTimer();
            NativeStreamingCompletionContext::Leave();
            Check(result == ClockResult::Busy && slot == 0xdecafbad && !Host::calls && !runtime.stopping.load() && runtime.weaponWriterReader.IsBound(),
                  "grant reuses real bound weapon reader and existing shared gate without entering native or inventing slot0");
            {
                NativePedWeaponContext::Mutation held;
                Check(static_cast<bool>(held), "hold actual inline-weapon mutation scope for independent grant refusal");
                fixture.invokeTimer();
                Check(result == ClockResult::Busy && slot == 0xdecafbad && !Host::calls && !runtime.stopping.load(),
                      "grant also shares the genuine ped weapon mutation owner instead of a parallel lock");
            }
            NativeWeaponInfoValues originalInfo;
            std::memcpy(&originalInfo, reinterpret_cast<void*>(fixture.image + NativeWeaponInfoProfile::BankRva + 22 * NativeWeaponInfoProfile::RowBytes),
                        sizeof(originalInfo));
            auto needsModel = originalInfo;
            needsModel.model = 3;
            NativePedWeaponGrantFixture::Info(&WorldGroundOwnerFixture::Writes::Apply, &writes, 22, needsModel);
            NativePedWeaponGrantFixture::Model(&WorldGroundOwnerFixture::Writes::Apply, &writes, 3, std::uintptr_t{0x12345678}, false);
            Check(service.Renew(runtime.generation.load()), "renew actual owner before explicit unready-model refusal");
            fixture.invokeTimer();
            Check(result == ClockResult::NotInitialized && slot == 0xdecafbad && !Host::calls && !runtime.stopping.load(),
                  "unloaded required native model refuses before grant without request, wrapper manufacture or Stop");
            NativePedWeaponGrantFixture::Model(&WorldGroundOwnerFixture::Writes::Apply, &writes, 3, std::uintptr_t{0x12345678}, true);
            Host::nextSlot = 0x80000013;
            callback.action = [&](NativeGameTimingAccess& access) { result = access.GrantPedWeapon(token, WEAPONTYPE_PISTOL, 0xffffffffu, slot); };
            Check(service.Renew(runtime.generation.load()), "renew native preloaded grant with full ammo and returned-slot bits");
            fixture.invokeTimer();
            Check(result == ClockResult::Ok && slot == 0x80000013 && Host::calls == 1 && Host::effects == 1 && Host::entry == fixture.image + P::EntryRva &&
                      Host::receivedPed == Host::ped && Host::receivedType == 22 && Host::receivedAmmo == 0xffffffffu && Host::receivedUnused == 1 &&
                      !runtime.stopping.load(),
                  "one native grant preserves exact receiver, full type/ammo, fixed fourth1 and all returned DWORD bits");
            NativePedWeaponGrantFixture::Info(&WorldGroundOwnerFixture::Writes::Apply, &writes, 22, originalInfo);
            NativePedWeaponGrantFixture::Model(&WorldGroundOwnerFixture::Writes::Apply, &writes, 3, 0, false);
            static ClockResult   nestedGrant = ClockResult::NotInitialized, nestedAmmo = ClockResult::NotInitialized;
            static std::uint32_t nestedSlot = 0;
            static bool          nestedStopping = false;
            Host::reenter = +[]() noexcept
            {
                nestedSlot = 0x12345678;
                nestedGrant = runtime.access.GrantPedWeapon(Host::selected, WEAPONTYPE_PISTOL, 3, nestedSlot);
                nestedAmmo = runtime.access.SetPedWeaponAmmoTotal(Host::selected, 2, 9);
                nestedStopping = runtime.stopping.load();
            };
            const auto fixedToken = token;
            Host::callerToken = &token;
            Host::nextSlot = 2;
            slot = 0xdecafbad;
            Check(service.Renew(runtime.generation.load()), "renew before snapshot and shared-weapon reentry observation");
            fixture.invokeTimer();
            Check(result == ClockResult::Ok && slot == 2 && token.version == fixedToken.version + 1 && Host::calls == 2 && nestedGrant == ClockResult::Busy &&
                      nestedAmmo == ClockResult::Busy && nestedSlot == 0x12345678 && !nestedStopping,
                  "grant fixes caller token and excludes nested grant/ammo mutation without replay or stopping the outer owner");
            token = fixedToken;
            Host::reenter = nullptr;
            auto     sdk = makePed(token);
            CWeapon* output = nullptr;
            bool     caught = false;
            Host::nextSlot = 0;
            const auto beforeSdk = Host::calls;
            callback.action = [&](NativeGameTimingAccess&)
            {
                try
                {
                    CPed& ped = *sdk;
                    output = ped.GiveWeapon(WEAPONTYPE_UNARMED, 0x80000001u, WEAPONSKILL_STD);
                }
                catch (const std::exception& error)
                {
                    caught = true;
                    std::fprintf(stderr, "Actual grant SDK refused: %s\n", error.what());
                }
            };
            Check(service.Renew(runtime.generation.load()), "renew actual CPed GiveWeapon callback with its existing test-only wrapper identity");
            fixture.invokeTimer();
            Check(!caught && output == &returnedIdentity && Host::calls == beforeSdk + 1 && Host::receivedType == 0 && Host::receivedAmmo == 0x80000001u &&
                      lookup.count == 1 && lookup.slots[0] == 0 && !game.infoCalls && !game.modelCalls && !tasks.eventCount && !runtime.stopping.load(),
                  "actual CPed unarmed itinerary skips model preparation and returns its existing slot wrapper after one native grant");
            Check(pool.objects[P::CurrentSlotOffset] == currentSlotBefore, "SDK grant adds no implicit equip/current-slot setter around the native operation");
            retire(service);
        }
        // Each entered fault uses a fresh registered relation. A failed native
        // read invalidates publication, so no positive test borrows it afterward.
        for (unsigned scenario = 1; scenario != 3; ++scenario)
        {
            graph.Reset();
            Host::scenario = scenario;
            Host::nextSlot = 2;
            Host::callerToken = nullptr;
            Host::reenter = nullptr;
            NativeClockService service;
            callback.action = [&](NativeGameTimingAccess& access) { result = access.AcquirePedReference(1, token); };
            BeginOwned(service, fixture, false, true, true, &WorldGroundOwnerFixture::Callback::Invoke, &callback);
            fixture.invokeTimer();
            Check(result == ClockResult::Ok, "fresh grant-fault owner acquires its own native reference");
            Host::cell = ClockPedReferenceOwnerTest::cell;
            const auto node = ClockPedReferenceOwnerTest::node;
            const auto before = Host::calls;
            slot = 0xdecafbad;
            callback.action = [&](NativeGameTimingAccess& access) { result = access.GrantPedWeapon(token, WEAPONTYPE_PISTOL, 37, slot); };
            Check(service.Renew(runtime.generation.load()), "renew genuine one-shot entered-fault grant callback");
            fixture.invokeTimer();
            Check(result != ClockResult::Ok && slot == 0xdecafbad && runtime.stopping.load() && Host::calls == before + 1,
                  "entered native exception or lost final registered relation Stops while preserving caller slot without retry");
            if (scenario == 2)
            {
                const auto unlinks = ClockPedReferenceOwnerTest::unregisters;
                Check(!service.End() && runtime.pedReferences.IsRetained(), "lost grant weak word retains a node that still names its owned cell");
                fixture.invokeTimer();
                Check(runtime.pedReferences.IsRetained() && ClockPedReferenceOwnerTest::unregisters == unlinks && Host::calls == before + 1,
                      "grant cleanup retains native cell debt without unsafe unregister or grant replay");
                ClockPedReferenceOwnerTest::Put(node + 8, std::uintptr_t{0});
                const auto freeHead = ClockPedReferenceOwnerTest::Get<std::uintptr_t>(fixture.image + R::FreeHeadRva);
                ClockPedReferenceOwnerTest::Put(node, freeHead);
                ClockPedReferenceOwnerTest::Put(fixture.image + R::FreeHeadRva, node);
                ClockPedReferenceOwnerTest::Put(Host::ped + R::EntityHeadOffset, std::uintptr_t{0});
                fixture.invokeTimer();
            }
            retire(service);
            Check(Host::calls == before + 1, "grant retirement neither repeats native effects nor rolls them back");
        }
        Host::image = Host::ped = Host::cell = 0;
        Host::scenario = 0;
        Host::callerToken = nullptr;
        Host::reenter = nullptr;
        writes.Restore();
        ClockReaderTestHost::image = priorReaderImage;
        ClockPedReferenceOwnerTest::image = priorReferenceImage;
        std::puts("Production native registered weapon-grant callback owner fixture completed");
        std::puts("Production actual CPed GiveWeapon SDK callback fixture completed");
    }
}
