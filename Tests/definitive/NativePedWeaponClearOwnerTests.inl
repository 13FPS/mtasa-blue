/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: One native single-weapon clear on the actual registered-ped owner
 *****************************************************************************/
namespace PedWeaponClearOwnerFixture
{
    void Run(ClockImageFixture& fixture)
    {
        namespace P = NativePedWeaponClearProfile;
        namespace R = NativePedReferenceProfile;
        namespace Ped = NativePedPoolProfile;
        namespace Host = ClockPedWeaponClearOwnerTest;
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
        NativePedWeaponClearFixture::Install(fixture.image, &WorldGroundOwnerFixture::Writes::Apply, &writes);
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
        // Reuse the actual registered-ped and weapon-reader setup only.
        // Branch-specific row/model admission belongs to the genuine clear
        // primitive; this fixture supplies no wrapper bank or model request.
        const auto priorReaderImage = ClockReaderTestHost::image;
        const auto priorReferenceImage = ClockPedReferenceOwnerTest::image;
        ClockReaderTestHost::image = ClockPedReferenceOwnerTest::image = fixture.image;
        ClockPedReferenceOwnerTest::partialUnlink = false;
        ClockPedReferenceOwnerTest::unregisterResult = ClockResult::Ok;
        Check(ClockReaderTestHost::Module(nullptr) == reinterpret_cast<HMODULE>(fixture.image),
              "weapon-clear scope selects the owned native image before thread/world/ped checks");
        Host::image = fixture.image;
        Host::ped = reinterpret_cast<std::uintptr_t>(pool.objects);
        Host::calls = Host::effects = 0;
        Host::scenario = 0;
        Host::callerToken = nullptr;
        Host::reenter = nullptr;
        const std::uint32_t priorType = 23;
        std::memcpy(pool.objects + NativePedWeaponProfile::ArrayOffset + 2 * NativePedWeaponProfile::Stride, &priorType, sizeof(priorType));
        pool.objects[NativePedCurrentWeaponSlotProfile::CurrentOffset] = 2;
        struct DefaultClearAccess final : NativePedWeaponClearAccess
        {
        };
        DefaultClearAccess absent;
        Check(absent.ClearPedWeapon({}, WEAPONTYPE_PISTOL) == ClockResult::NotInitialized,
              "older clear owner refuses instead of pretending the requested native weapon was removed");
        ped_clear_weapon_fixture::GameBase     game;
        ped_clear_weapon_fixture::TaskScenario tasks;
        ped_clear_weapon_fixture::WeaponLookup lookup;
        const auto                             makePed = [&](const NativePedReferenceToken& token)
        {
            auto& access = runtime.access;
            return std::make_unique<ped_clear_weapon_fixture::PedClearWeaponFixture>(
                game, access, access, access, access, access, access, access, access, access, access, access, access, access, access, access, access, access,
                access, access, access, access, access, access, access, access, token, 0x12345678, tasks, lookup);
        };
        const auto retire = [&](NativeClockService& service)
        {
            WorldGroundOwnerFixture::Retire(service, fixture);
            Check(!runtime.pedReferences.IsRetained(), "weapon-clear retirement acknowledges real registered cells before owner release");
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
                  "single-weapon clear borrows the sole registered-ped controller");
            Host::selected = token;
            Host::cell = ClockPedReferenceOwnerTest::cell;
            Check(runtime.access.ClearPedWeapon(token, WEAPONTYPE_PISTOL) != ClockResult::Ok && !Host::calls,
                  "retained ped token does not lend clear authority outside its current native callback");
            std::thread foreign([&] { result = runtime.access.ClearPedWeapon(token, WEAPONTYPE_PISTOL); });
            foreign.join();
            Check(result != ClockResult::Ok && !Host::calls && !runtime.stopping.load(),
                  "foreign clear refuses without native entry or stopping the actual owner");
            callback.action = [&](NativeGameTimingAccess& access) { result = access.ClearPedWeapon(token, WEAPONTYPE_PISTOL); };
            Check(NativeStreamingCompletionContext::TryEnter(), "hold actual shared native gate before clear intent");
            fixture.invokeTimer();
            NativeStreamingCompletionContext::Leave();
            Check(result == ClockResult::Busy && !Host::calls && !runtime.stopping.load() && runtime.weaponWriterReader.IsBound(),
                  "clear shares the existing bound reader and native gate without replacing either owner");
            {
                NativePedWeaponContext::Mutation held;
                Check(static_cast<bool>(held), "hold actual ped weapon mutation scope before clear intent");
                fixture.invokeTimer();
                Check(result == ClockResult::Busy && !Host::calls && !runtime.stopping.load(),
                      "clear refuses independent weapon mutation overlap before genuine native entry");
            }
            callback.action = [&](NativeGameTimingAccess& access) { result = access.ClearPedWeapon(token, static_cast<eWeaponType>(0xff)); };
            fixture.invokeTimer();
            Check(result == ClockResult::InvalidArgument && !Host::calls && !runtime.stopping.load(),
                  "full SDK enum reaches owner qualification without clamping an unsupported native type");
            callback.action = [&](NativeGameTimingAccess& access) { result = access.ClearPedWeapon(token, WEAPONTYPE_PISTOL); };
            Check(service.Renew(runtime.generation.load()), "renew genuine no-matching-row single-weapon clear");
            fixture.invokeTimer();
            Check(result == ClockResult::Ok && Host::calls == 1 && Host::effects == 1 && Host::entry == fixture.image + P::EntryRva &&
                      Host::receivedPed == Host::ped && Host::receivedType == 22 && !runtime.stopping.load(),
                  "no matching inline weapon still invokes the genuine clear once with exact receiver and full DWORD type");
            const std::uint32_t matchingType = 22;
            std::memcpy(pool.objects + NativePedWeaponProfile::ArrayOffset + 2 * NativePedWeaponProfile::Stride, &matchingType, sizeof(matchingType));
            static ClockResult nestedClear = ClockResult::NotInitialized, nestedAmmo = ClockResult::NotInitialized;
            static bool        nestedStopping = false;
            Host::reenter = +[]() noexcept
            {
                nestedClear = runtime.access.ClearPedWeapon(Host::selected, WEAPONTYPE_PISTOL);
                nestedAmmo = runtime.access.SetPedWeaponAmmoTotal(Host::selected, 2, 9);
                nestedStopping = runtime.stopping.load();
            };
            const auto fixedToken = token;
            Host::callerToken = &token;
            Check(service.Renew(runtime.generation.load()), "renew matching clear with caller-token and native reentry observation");
            fixture.invokeTimer();
            Check(result == ClockResult::Ok && Host::calls == 2 && token.version == fixedToken.version + 1 && nestedClear == ClockResult::Busy &&
                      nestedAmmo == ClockResult::Busy && !nestedStopping,
                  "clear fixes original receiver before callbacks and excludes nested clear/ammo effects without replay");
            token = fixedToken;
            Host::reenter = nullptr;
            auto       sdk = makePed(token);
            bool       caught = false;
            const auto beforeSdk = Host::calls;
            callback.action = [&](NativeGameTimingAccess&)
            {
                try
                {
                    CPed& ped = *sdk;
                    ped.ClearWeapon(WEAPONTYPE_PISTOL);
                }
                catch (const std::exception& error)
                {
                    caught = true;
                    std::fprintf(stderr, "Actual clear SDK refused: %s\n", error.what());
                }
            };
            Check(service.Renew(runtime.generation.load()), "renew actual CPed single-weapon SDK dispatch");
            fixture.invokeTimer();
            Check(!caught && Host::calls == beforeSdk + 1 && Host::receivedType == 22 && !tasks.eventCount && !lookup.count && !runtime.stopping.load(),
                  "actual void CPed ClearWeapon reaches native once without weapon lookup, preparation, equip or a return-pointer wrapper");
            // A refused native read may revoke token publication. Put the GC
            // fault after every positive use and retire this relation next.
            const auto beforeGc = Host::calls;
            writes.Global(NativeBorrowedGcProfile::markerRva, LONG{1});
            callback.action = [&](NativeGameTimingAccess& access) { result = access.ClearPedWeapon(token, WEAPONTYPE_PISTOL); };
            fixture.invokeTimer();
            writes.Global(NativeBorrowedGcProfile::markerRva, LONG{0});
            Check(result == ClockResult::Busy && Host::calls == beforeGc && !runtime.stopping.load(),
                  "actual GC guard refuses before clear entry without replaying prior effects or stopping a valid owner");
            retire(service);
        }
        // Entered failures get independent owners; no happy case borrows a
        // relation whose publication was revoked by a prior native refusal.
        for (unsigned scenario = 1; scenario != 3; ++scenario)
        {
            graph.Reset();
            Host::scenario = scenario;
            Host::callerToken = nullptr;
            Host::reenter = nullptr;
            NativeClockService service;
            callback.action = [&](NativeGameTimingAccess& access) { result = access.AcquirePedReference(1, token); };
            BeginOwned(service, fixture, false, true, true, &WorldGroundOwnerFixture::Callback::Invoke, &callback);
            fixture.invokeTimer();
            Check(result == ClockResult::Ok, "fresh entered-clear failure owner acquires its own registered relation");
            Host::cell = ClockPedReferenceOwnerTest::cell;
            const auto node = ClockPedReferenceOwnerTest::node;
            const auto before = Host::calls;
            callback.action = [&](NativeGameTimingAccess& access) { result = access.ClearPedWeapon(token, WEAPONTYPE_PISTOL); };
            Check(service.Renew(runtime.generation.load()), "renew one-shot entered clear failure callback");
            fixture.invokeTimer();
            Check(result != ClockResult::Ok && runtime.stopping.load() && Host::calls == before + 1,
                  "entered native exception or lost final registered relation Stops after one clear without rollback or retry");
            if (scenario == 1)
                Check(result == ClockResult::OriginalException, "genuine clear exception status survives owner retirement without substitution");
            if (scenario == 2)
            {
                const auto unlinks = ClockPedReferenceOwnerTest::unregisters;
                Check(!service.End() && runtime.pedReferences.IsRetained(), "lost clear weak word retains the native node that still names its owned cell");
                fixture.invokeTimer();
                Check(runtime.pedReferences.IsRetained() && ClockPedReferenceOwnerTest::unregisters == unlinks && Host::calls == before + 1,
                      "uncertain reference cleanup never unregisters blindly or repeats the weapon clear");
                // The owned fixture supplies the exact native retirement
                // postcondition; production cleanup still owns acknowledgement.
                ClockPedReferenceOwnerTest::Put(node + 8, std::uintptr_t{0});
                const auto freeHead = ClockPedReferenceOwnerTest::Get<std::uintptr_t>(fixture.image + R::FreeHeadRva);
                ClockPedReferenceOwnerTest::Put(node, freeHead);
                ClockPedReferenceOwnerTest::Put(fixture.image + R::FreeHeadRva, node);
                ClockPedReferenceOwnerTest::Put(Host::ped + R::EntityHeadOffset, std::uintptr_t{0});
                fixture.invokeTimer();
            }
            retire(service);
            Check(Host::calls == before + 1, "clear retirement neither repeats native effects nor rewrites their result");
        }
        Host::image = Host::ped = Host::cell = 0;
        Host::scenario = 0;
        Host::callerToken = nullptr;
        Host::reenter = nullptr;
        writes.Restore();
        ClockReaderTestHost::image = priorReaderImage;
        ClockPedReferenceOwnerTest::image = priorReferenceImage;
        std::puts("Production native registered weapon-clear callback owner fixture completed");
        std::puts("Production actual CPed ClearWeapon SDK callback fixture completed");
    }
}
