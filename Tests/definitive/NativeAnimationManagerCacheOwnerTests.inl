/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual manager cache overloads share the qualified private hierarchy owner
 ****************************************************************************/
namespace AnimationManagerCacheOwnerFixture
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
                    std::fprintf(stderr, "Manager cache action %s refused: %s\n", label, error.what());
                    Check(false, label);
                }
            };
            Check(service.Renew(generation), "renew the real owner separately for each manager-cache operation");
            fixture.invokeTimer();
            callback.action = [](NativeGameTimingAccess&) {};
        };
        const auto refuses = [](const auto& call)
        {
            try
            {
                call();
            }
            catch (const std::exception&)
            {
                return true;
            }
            return false;
        };
        std::unique_ptr<CAnimBlendHierarchy>   hierarchy;
        CAnimBlendHierarchySAInterface*        tag = nullptr;
        NativeAnimationHierarchyReferenceToken token;
        action("create actual private cache0 hierarchy",
               [&]
               {
                   hierarchy = sdk.GetCustomAnimBlendHierarchy();
                   tag = hierarchy->GetInterface();
                   Check(tag && runtime.access.ResolvePrivateAnimationHierarchyReference(generation, reinterpret_cast<std::uintptr_t>(tag), token) ==
                                    ClockResult::Ok,
                         "manager cache overloads use the same actually issued private hierarchy and current owner token");
               });
        action("define private scalar neighbours",
               [&]
               {
                   hierarchy->SetName("Manager-Cache-Owner");
                   hierarchy->SetNumSequences(0xffff);
                   hierarchy->SetRunningCompressed(true);
                   hierarchy->SetAnimationBlockID((std::numeric_limits<int>::min)());
               });
        const auto snapshot = [](std::uintptr_t address)
        {
            std::array<unsigned char, 40> value{};
            std::memcpy(value.data(), reinterpret_cast<void*>(address), value.size());
            return value;
        };
        const auto before = snapshot(token.locator);
        const auto initialMalloc = host.mallocCalls, initialFree = host.freeCalls;
        action("typed manager cache0 operation",
               [&]
               {
                   sdk.RemoveFromUncompressedCache(hierarchy.get());
                   Check(snapshot(token.locator) == before, "typed same-provider facade cache0 qualification changes none of the actual40 bytes");
               });
        action("raw manager cache0 operation",
               [&]
               {
                   sdk.RemoveFromUncompressedCache(tag);
                   Check(snapshot(token.locator) == before && host.mallocCalls == initialMalloc && host.freeCalls == initialFree,
                         "raw same-owner resolver/cache0 qualification performs no native allocation, free or field/cache store");
               });
        if (!scenario)
        {
            NativeAnimationHierarchyAccess                          foreignProvider;
            animation_hierarchy_fixture::AnimationHierarchyReceiver foreign(foreignProvider, token);
            animation_hierarchy_fixture::AnimationHierarchyReceiver stale(runtime.access, {generation + 1, token.locator});
            animation_hierarchy_fixture::AnimationHierarchyReceiver bank(runtime.access,
                                                                         {generation, fixture.image + NativeAnimationBlockHierarchyProfile::BankRva});
            std::array<unsigned char, 40>                           unrelated{};
            action("manager facade ownership refusal",
                   [&]
                   {
                       Check(refuses([&] { sdk.RemoveFromUncompressedCache(static_cast<CAnimBlendHierarchy*>(nullptr)); }) &&
                                 refuses([&] { sdk.RemoveFromUncompressedCache(&foreign); }) && refuses([&] { sdk.RemoveFromUncompressedCache(&stale); }) &&
                                 refuses([&] { sdk.RemoveFromUncompressedCache(&bank); }),
                             "typed null, different complete provider, stale generation and bank locator grant no private cache authority");
                   });
            action("raw locator ownership refusal",
                   [&]
                   {
                       Check(refuses([&] { sdk.RemoveFromUncompressedCache(static_cast<CAnimBlendHierarchySAInterface*>(nullptr)); }) &&
                                 refuses([&] { sdk.RemoveFromUncompressedCache(reinterpret_cast<CAnimBlendHierarchySAInterface*>(unrelated.data())); }) &&
                                 refuses(
                                     [&]
                                     {
                                         sdk.RemoveFromUncompressedCache(
                                             reinterpret_cast<CAnimBlendHierarchySAInterface*>(fixture.image + NativeAnimationBlockHierarchyProfile::BankRva));
                                     }),
                             "raw null, foreign readable storage and native bank rows cannot be adopted by the private cache overload");
                       Check(
                           snapshot(token.locator) == before && host.mallocCalls == initialMalloc && host.freeCalls == initialFree && !runtime.stopping.load(),
                           "ownership refusals preserve every legitimate private field and perform no release effects");
                   });
            Check(refuses([&] { sdk.RemoveFromUncompressedCache(hierarchy.get()); }) && refuses([&] { sdk.RemoveFromUncompressedCache(tag); }) &&
                      !runtime.stopping.load(),
                  "both actual manager overloads refuse outside the qualified native callback");
            auto wrongProof = std::array<unsigned char, sizeof(private_hierarchy_fixture::CacheProof)>{};
            std::memcpy(wrongProof.data(), private_hierarchy_fixture::CacheProof, wrongProof.size());
            wrongProof[0] ^= 1;
            WorldGroundOwnerFixture::Writes::Apply(&saved, 0xfec67a, wrongProof.data(), wrongProof.size(), PAGE_EXECUTE_READ);
            action("missing exact cache proof refuses",
                   [&]
                   {
                       Check(refuses([&] { sdk.RemoveFromUncompressedCache(hierarchy.get()); }) && refuses([&] { sdk.RemoveFromUncompressedCache(tag); }) &&
                                 snapshot(token.locator) == before && !runtime.stopping.load(),
                             "neither manager route invents cache0 admission when its actual saved instruction proof is missing");
                   });
            WorldGroundOwnerFixture::Writes::Apply(&saved, 0xfec67a, private_hierarchy_fixture::CacheProof, sizeof(private_hierarchy_fixture::CacheProof),
                                                   PAGE_EXECUTE_READ);
            action("restored genuine cache proof admits again", [&] { sdk.RemoveFromUncompressedCache(hierarchy.get()); });
            std::uintptr_t array = 0, frame = 0;
            action("allocate actual linked array",
                   [&]
                   {
                       Check(memory.AllocateAnimationSequences(generation, 2, array) == ClockResult::Ok && array,
                             "linked manager cache borrows a genuine current original array");
                   });
            action("allocate original linked frame",
                   [&]
                   {
                       Check(memory.AllocateAnimationKeyFrames(generation, 10, frame) == ClockResult::Ok && frame,
                             "linked parent frame retains its actual original native heap record");
                   });
            action("prepare real typed frame binding",
                   [&]
                   {
                       Check(runtime.access.PrepareFreshCompressedAnimationSequence({generation, array, 0}, 1, false, frame) == ClockResult::Ok,
                             "prepared frame authority comes from the actual private transaction");
                   });
            action("attach actual private parent",
                   [&]
                   {
                       hierarchy->SetNumSequences(1);
                       hierarchy->SetSequences(reinterpret_cast<CAnimBlendSequenceSAInterface*>(array));
                   });
            const auto                     linked = snapshot(token.locator);
            std::array<unsigned char, 160> rows{};
            std::array<unsigned char, 10>  payload{};
            std::memcpy(rows.data(), reinterpret_cast<void*>(array), rows.size());
            std::memcpy(payload.data(), reinterpret_cast<void*>(frame), payload.size());
            const auto linkedMalloc = host.mallocCalls, linkedFree = host.freeCalls;
            const auto unchanged = [&]
            {
                return snapshot(token.locator) == linked && !std::memcmp(rows.data(), reinterpret_cast<void*>(array), rows.size()) &&
                       !std::memcmp(payload.data(), reinterpret_cast<void*>(frame), payload.size()) && host.mallocCalls == linkedMalloc &&
                       host.freeCalls == linkedFree;
            };
            action("typed manager linked cache0 epoch",
                   [&]
                   {
                       sdk.RemoveFromUncompressedCache(hierarchy.get());
                       Check(unchanged(), "typed linked cache0 preserves parent, actual rows, payload and native heap counters");
                   });
            action("raw manager linked cache0 epoch",
                   [&]
                   {
                       sdk.RemoveFromUncompressedCache(tag);
                       Check(unchanged() && memory.FreeAnimationSequences(generation, array) == ClockResult::OwnerConflict &&
                                 memory.FreeAnimationKeyFrames(generation, frame) == ClockResult::OwnerConflict && unchanged(),
                             "raw linked cache0 qualifies the held actual epoch without releasing array/frame dependencies");
                   });
            action("acknowledge real parent detach", [&] { hierarchy->SetSequences(nullptr); });
            action("release original children after actual detach",
                   [&]
                   {
                       Check(memory.FreeAnimationSequences(generation, array) == ClockResult::Ok &&
                                 memory.FreeAnimationKeyFrames(generation, frame) == ClockResult::Ok,
                             "cache checks do not replace the separate parent detach and original storage release acknowledgements");
                   });
            action("retire original CPU hierarchy", [&] { sdk.DeleteCustomAnimHierarchyInterface(tag); });
            action("consumed private locator cannot reenter cache channel",
                   [&]
                   {
                       Check(refuses([&] { sdk.RemoveFromUncompressedCache(tag); }) && refuses([&] { sdk.RemoveFromUncompressedCache(hierarchy.get()); }),
                             "old raw and facade locators refuse after actual private record retirement");
                   });
            hierarchy.reset();
            WorldGroundOwnerFixture::Retire(service, fixture);
            std::puts("Production animation manager cache owner fixture completed");
            std::puts("Production animation manager cache refusal ownership completed");
        }
        else
        {
            // An observed nonzero native cache has no qualified splice or
            // detached acknowledgement. Clearing its visible cell cannot
            // undo the existing owner's irreversible publication bookkeeping.
            const std::uintptr_t foreignCache = 1;
            std::memcpy(reinterpret_cast<void*>(token.locator + 0x20), &foreignCache, sizeof(foreignCache));
            const auto changed = snapshot(token.locator);
            action("observed foreign cache is retained",
                   [&]
                   {
                       Check(refuses([&] { sdk.RemoveFromUncompressedCache(hierarchy.get()); }) && refuses([&] { sdk.RemoveFromUncompressedCache(tag); }) &&
                                 snapshot(token.locator) == changed && runtime.privateHierarchies.IsRetained() && runtime.privateHierarchies.RecoveryPending(),
                             "actual nonzero cache observation conservatively refuses and monotonically retains its private owner");
                   });
            const std::uintptr_t zero = 0;
            std::memcpy(reinterpret_cast<void*>(token.locator + 0x20), &zero, sizeof(zero));
            hierarchy.reset();
            service.End();
            for (unsigned attempt = 0; attempt != 6; ++attempt)
                fixture.invokeTimer();
            Check(!service.End() && service.IsRetained() && runtime.privateHierarchies.IsRetained() && host.mallocCalls == initialMalloc &&
                      host.freeCalls == initialFree,
                  "visible cache clearing is no ownership receipt and cannot permit CPU deletion or native cleanup replay");
            std::puts("Production animation manager cache foreign state retained");
        }
        Host::active = nullptr;
        ClockReaderTestHost::image = previousImage;
        saved.Restore();
    }
}
