/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: One native ignored-entity journal on the actual registered-ped owner
 *****************************************************************************/
namespace WorldIgnoreEntityOwnerFixture
{
    void Run(ClockImageFixture& fixture)
    {
        namespace P = NativeWorldIgnoredEntityProfile;
        namespace R = NativePedReferenceProfile;
        namespace Ped = NativePedPoolProfile;
        namespace Host = ClockWorldIgnoreOwnerTest;
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
        NativeWorldIgnoredEntityFixture::Install(fixture.image, &WorldGroundOwnerFixture::Writes::Apply, &writes);
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
        // Only current native ped identity and typed world admission apply.
        // No sector/matrix or fake native entity/interface graph is needed.
        const auto priorReaderImage = ClockReaderTestHost::image;
        const auto priorReferenceImage = ClockPedReferenceOwnerTest::image;
        ClockReaderTestHost::image = ClockPedReferenceOwnerTest::image = fixture.image;
        ClockPedReferenceOwnerTest::partialUnlink = false;
        ClockPedReferenceOwnerTest::unregisterResult = ClockResult::Ok;
        Check(ClockReaderTestHost::Module(nullptr) == reinterpret_cast<HMODULE>(fixture.image),
              "ignored-entity scope selects the owned native image before thread/world/ped checks");
        Host::image = fixture.image;
        Host::calls = 0;
        Host::scenario = 0;
        Host::reenter = nullptr;
        const auto firstPed = reinterpret_cast<std::uintptr_t>(pool.objects);
        const auto secondPed = firstPed + Ped::ObjectStride;
        pool.flags[1] = 1;
        std::memcpy(pool.objects + Ped::ObjectStride, &vtable, sizeof(vtable));
        std::memcpy(pool.objects + Ped::ObjectStride + NativePedMetadataProfile::ModelOffset, &modelIndex, sizeof(modelIndex));
        pool.objects[Ped::ObjectStride + R::TypeOffset] = 3;
        const auto global = fixture.image + P::GlobalRva;
        const auto field = [&] { return ClockPedReferenceOwnerTest::Get<std::uintptr_t>(global); };
        struct DefaultIgnoreAccess final : NativeWorldIgnoreEntityAccess
        {
        };
        DefaultIgnoreAccess absent;
        Check(absent.SetWorldIgnoredPed({}) == ClockResult::NotInitialized && absent.ClearWorldIgnoredEntity() == ClockResult::NotInitialized,
              "older world owner cannot report successful ignored-pointer state");
        WorldGroundOwnerFixture::Callback callback;
        ClockResult                       result = ClockResult::NotInitialized;
        NativePedReferenceToken           token, secondToken;
        const auto                        retire = [&](NativeClockService& service)
        {
            WorldGroundOwnerFixture::Retire(service, fixture);
            Check(!WorldIgnoreRetained() && !runtime.pedReferences.IsRetained(), "journal/filter acknowledgement precedes final native reference retirement");
        };
        {
            NativeClockService service;
            callback.action = [&](NativeGameTimingAccess& access) { result = access.AcquirePedReference(1, token); };
            BeginOwned(service, fixture, false, true, true, &WorldGroundOwnerFixture::Callback::Invoke, &callback);
            fixture.invokeTimer();
            Check(result == ClockResult::Ok && token.version, "ignored-pointer scope reuses the genuine registered ped controller");
            Check(runtime.access.SetWorldIgnoredPed(token) != ClockResult::Ok && !Host::calls && !WorldIgnoreRetained(),
                  "an issued token outside its callback cannot install a world pointer");
            std::thread foreign([&] { result = runtime.access.SetWorldIgnoredPed(token); });
            foreign.join();
            Check(result != ClockResult::Ok && !Host::calls && !runtime.stopping.load(),
                  "foreign setter cannot mutate the native filter or Stop its actual owner");
            callback.action = [&](NativeGameTimingAccess& access) { result = access.SetWorldIgnoredPed(token); };
            Check(NativeStreamingCompletionContext::TryEnter(), "hold existing shared effect gate for ignored-pointer refusal");
            fixture.invokeTimer();
            NativeStreamingCompletionContext::Leave();
            Check(result == ClockResult::Busy && !Host::calls && !WorldIgnoreRetained() && !runtime.stopping.load(),
                  "shared-gate refusal restores bookkeeping before any native pointer intent can enter");
            world_ignore_entity_fixture::Pools registry(runtime.access);
            world_ignore_entity_fixture::Ped   wrapper(runtime.access, token);
            CPoolsDE::BorrowedPedRegistration  registration;
            callback.action = [&](NativeGameTimingAccess&) { result = registry.BindBorrowedPed(wrapper, nullptr, registration); };
            Check(service.Renew(runtime.generation.load()), "renew controlled existing SDK registry association");
            fixture.invokeTimer();
            Check(result == ClockResult::Ok && registration.IsBound(), "actual world target has one existing controlled SDK wrapper");
            world_ignore_entity_fixture::World world(runtime.access, runtime.access, runtime.access, runtime.access, registry, runtime.access);
            bool                               caught = false;
            const auto                         registersBefore = ClockPedReferenceOwnerTest::registers;
            callback.action = [&](NativeGameTimingAccess&)
            {
                try
                {
                    CWorld& sdk = world;
                    sdk.IgnoreEntity(&wrapper);
                }
                catch (const std::exception& error)
                {
                    caught = true;
                    std::fprintf(stderr, "Actual IgnoreEntity SDK refused: %s\n", error.what());
                }
            };
            Check(service.Renew(runtime.generation.load()), "renew actual CWorld ignored-entity dispatch");
            fixture.invokeTimer();
            Check(!caught && field() == firstPed && WorldIgnoreRetained() && !runtime.worldIgnoreRecovery.load() && Host::calls == 1 &&
                      Host::address == global && Host::expected == 0 && Host::desired == firstPed && ClockPedReferenceOwnerTest::registers == registersBefore &&
                      registry.GetPedCount() == 1,
                  "actual CWorld resolves the existing CPU registry token then installs one native ped address without another reference or interface");
            static ClockResult nestedSet = ClockResult::NotInitialized, nestedClear = ClockResult::NotInitialized, nestedRelease = ClockResult::NotInitialized;
            static bool        nestedStop = true;
            Host::selected = token;
            Host::reenter = +[]() noexcept
            {
                nestedSet = runtime.access.SetWorldIgnoredPed(Host::selected);
                nestedClear = runtime.access.ClearWorldIgnoredEntity();
                nestedRelease = runtime.access.ReleasePedReference(Host::selected);
                nestedStop = runtime.stopping.load();
            };
            callback.action = [&](NativeGameTimingAccess& access) { result = access.SetWorldIgnoredPed(token); };
            Check(service.Renew(runtime.generation.load()), "renew isolated ignored-pointer reentry observation");
            fixture.invokeTimer();
            Host::reenter = nullptr;
            Check(result == ClockResult::Ok && nestedSet == ClockResult::Busy && nestedClear == ClockResult::Busy && nestedRelease == ClockResult::Busy &&
                      !nestedStop && field() == firstPed && Host::calls == 2,
                  "reentry cannot replace journal bookkeeping or release its selected relation while one CAS is in flight");
            callback.action = [&](NativeGameTimingAccess& access) { result = access.AcquirePedReference(0x101, secondToken); };
            Check(service.Renew(runtime.generation.load()), "renew second genuine ped relation for owned pointer replacement");
            fixture.invokeTimer();
            Check(result == ClockResult::Ok && secondToken.version, "second address uses the same reference controller and native pool");
            callback.action = [&](NativeGameTimingAccess& access) { result = access.SetWorldIgnoredPed(secondToken); };
            Check(service.Renew(runtime.generation.load()), "renew owned-global replace intent");
            fixture.invokeTimer();
            Check(result == ClockResult::Ok && field() == secondPed && Host::expected == firstPed && runtime.worldIgnore.committed == secondPed,
                  "acknowledged replacement uses only the owner's prior comparand and commits the new selected token");
            const auto beforeOldRelease = Host::calls;
            callback.action = [&](NativeGameTimingAccess& access) { result = access.ReleasePedReference(token); };
            Check(service.Renew(runtime.generation.load()), "renew now-unselected relation retirement");
            fixture.invokeTimer();
            Check(result == ClockResult::Ok && field() == secondPed && Host::calls == beforeOldRelease && !runtime.stopping.load(),
                  "releasing superseded relation does not clear the currently selected different native ped");
            callback.action = [&](NativeGameTimingAccess&)
            {
                try
                {
                    CWorld& sdk = world;
                    sdk.IgnoreEntity(nullptr);
                }
                catch (const std::exception& error)
                {
                    caught = true;
                    std::fprintf(stderr, "Actual null clear refused: %s\n", error.what());
                }
            };
            Check(service.Renew(runtime.generation.load()), "renew actual CWorld explicit null clear");
            fixture.invokeTimer();
            Check(!caught && !field() && !WorldIgnoreRetained() && registry.GetPedCount() == 1,
                  "actual null world clear acknowledges the global without consulting the now-released first wrapper token");
            callback.action = [&](NativeGameTimingAccess& access) { result = access.SetWorldIgnoredPed(secondToken); };
            Check(service.Renew(runtime.generation.load()), "renew target before matching release ordering");
            fixture.invokeTimer();
            Check(result == ClockResult::Ok && field() == secondPed, "matching-release test owns a real installed pointer");
            NativePedReferenceToken survivorToken;
            callback.action = [&](NativeGameTimingAccess& access) { result = access.AcquirePedReference(1, survivorToken); };
            Check(service.Renew(runtime.generation.load()), "renew other genuine relation for release-token snapshot observation");
            fixture.invokeTimer();
            Check(result == ClockResult::Ok && survivorToken.version, "release snapshot test retains another actual controller relation");
            const auto                      originalSelected = secondToken;
            static NativePedReferenceToken* callerToken = nullptr;
            static NativePedReferenceToken  replacementToken;
            callerToken = &secondToken;
            replacementToken = survivorToken;
            Host::reenter = +[]() noexcept { *callerToken = replacementToken; };
            const auto unlinks = ClockPedReferenceOwnerTest::unregisters;
            callback.action = [&](NativeGameTimingAccess& access) { result = access.ReleasePedReference(secondToken); };
            Check(service.Renew(runtime.generation.load()), "renew matching relation release with filter debt");
            fixture.invokeTimer();
            Host::reenter = nullptr;
            callerToken = nullptr;
            Check(result == ClockResult::Ok && !field() && !WorldIgnoreRetained() && Host::unregistersAtCas == unlinks &&
                      ClockPedReferenceOwnerTest::unregisters == unlinks + 1 && runtime.pedReferences.IsRetained() &&
                      SameWorldIgnoreToken(secondToken, survivorToken),
                  "clear CAS can mutate original caller token but release still acknowledges the selected relation before genuine unregister");
            ClockResult                selectedResult = ClockResult::Ok, survivorResult = ClockResult::NotInitialized;
            NativePedPoolReader::Value selectedValue, survivorValue;
            callback.action = [&](NativeGameTimingAccess& access)
            {
                selectedResult = access.ResolvePedReference(originalSelected, selectedValue);
                survivorResult = access.ResolvePedReference(survivorToken, survivorValue);
            };
            Check(service.Renew(runtime.generation.load()), "renew independent selected/survivor relation acknowledgement");
            fixture.invokeTimer();
            Check(selectedResult == ClockResult::NotInitialized && survivorResult == ClockResult::Ok && survivorValue.nativePed == firstPed &&
                      ClockPedReferenceOwnerTest::unregisters == unlinks + 1,
                  "snapshot release retires original selected token while supplied replacement relation remains genuinely resolvable");
            callback.action = [&](NativeGameTimingAccess& access) { result = access.ReleasePedReference(survivorToken); };
            Check(service.Renew(runtime.generation.load()), "renew surviving relation's own explicit retirement");
            fixture.invokeTimer();
            Check(result == ClockResult::Ok && !runtime.pedReferences.IsRetained(),
                  "replacement relation retires only when separately requested after snapshot acknowledgement");
            Check(registration.Reset() == ClockResult::Ok && !registry.GetPedCount(), "controlled CPU association withdraws before its wrapper dies");
            retire(service);
        }
        // Each failure owns fresh tokens. Read refusal revokes publication,
        // so no later positive SDK case tries to reuse that failed relation.
        for (unsigned scenario = 1; scenario != 5; ++scenario)
        {
            graph.Reset();
            writes.Global(NativeGameStateProfile::stateRva, std::uint32_t{9});
            writes.Global(P::GlobalRva, std::uintptr_t{0});
            Host::scenario = 0;
            Host::protectionChanged = false;
            NativeClockService service;
            callback.action = [&](NativeGameTimingAccess& access) { result = access.AcquirePedReference(1, token); };
            BeginOwned(service, fixture, false, true, true, &WorldGroundOwnerFixture::Callback::Invoke, &callback);
            fixture.invokeTimer();
            Check(result == ClockResult::Ok, "fresh ignored-pointer failure owner acquires its own registered relation");
            if (scenario == 4)
            {
                callback.action = [&](NativeGameTimingAccess& access) { result = access.AcquirePedReference(0x101, secondToken); };
                Check(service.Renew(runtime.generation.load()), "renew uncertain replace second relation acquisition");
                fixture.invokeTimer();
                Check(result == ClockResult::Ok, "uncertain replace holds both actual controller relations");
            }
            callback.action = [&](NativeGameTimingAccess& access) { result = access.SetWorldIgnoredPed(token); };
            if (scenario != 3 && scenario != 4)
                Host::scenario = scenario;
            const auto callsBeforeIntent = Host::calls;
            const auto unlinksBeforeIntent = ClockPedReferenceOwnerTest::unregisters;
            Check(service.Renew(runtime.generation.load()), "renew owned ignored-pointer effect/failure case");
            fixture.invokeTimer();
            if (scenario == 4)
            {
                Check(result == ClockResult::Ok && field() == firstPed, "replace uncertainty begins from one acknowledged owned address");
                Host::scenario = 1;
                callback.action = [&](NativeGameTimingAccess& access) { result = access.SetWorldIgnoredPed(secondToken); };
                Check(service.Renew(runtime.generation.load()), "renew uncertain replace into second address");
                fixture.invokeTimer();
                Check(runtime.worldIgnore.committed == firstPed && runtime.worldIgnore.pending == secondPed,
                      "interrupted replace retains exactly committed and pending address/token candidates");
            }
            if (scenario == 2)
            {
                // The real timer tail observes Stop immediately after this
                // handler. Current-image null recovery intentionally survives
                // lost state9 and can finish in this very same callback.
                std::fprintf(
                    stderr,
                    "Ignore scenario2 result=%u failure=%u stop=%u generation=%llu committed=%llx pending=%llx recovery=%u refs=%u refRecovery=%u "
                    "refGeneration=%llu filter=%llx calls=%u before=%u unregisters=%u before=%u state=%u active=%u ready=%u timerReady=%u restored=%u\n",
                    static_cast<unsigned>(result), static_cast<unsigned>(runtime.failure.load()), static_cast<unsigned>(runtime.stopping.load()),
                    static_cast<unsigned long long>(runtime.worldIgnoreGeneration.load()), static_cast<unsigned long long>(runtime.worldIgnore.committed),
                    static_cast<unsigned long long>(runtime.worldIgnore.pending), static_cast<unsigned>(runtime.worldIgnoreRecovery.load()),
                    static_cast<unsigned>(runtime.pedReferences.IsRetained()), static_cast<unsigned>(runtime.pedReferences.RecoveryPending()),
                    static_cast<unsigned long long>(runtime.referenceGeneration.load()), static_cast<unsigned long long>(field()), Host::calls,
                    callsBeforeIntent, ClockPedReferenceOwnerTest::unregisters, unlinksBeforeIntent,
                    ClockPedReferenceOwnerTest::Get<std::uint32_t>(fixture.image + NativeGameStateProfile::stateRva),
                    static_cast<unsigned>(runtime.active.load()), static_cast<unsigned>(runtime.ready.load()), static_cast<unsigned>(runtime.timerReady.load()),
                    static_cast<unsigned>(runtime.restored.load()));
                Check(result == ClockResult::BindingChanged && runtime.failure.load() == ClockResult::BindingChanged && runtime.stopping.load() &&
                          !WorldIgnoreRetained() && !runtime.worldIgnoreRecovery.load() && !field() && Host::calls == callsBeforeIntent + 2 &&
                          runtime.pedReferences.IsRetained() && runtime.pedReferences.RecoveryPending() &&
                          ClockPedReferenceOwnerTest::unregisters == unlinksBeforeIntent,
                      "unknown state0 fails binding while narrow filter cleanup finishes and genuine reference cleanup retains its independent debt");
                const auto completedCalls = Host::calls;
                Check(!service.End() && service.IsRetained() && runtime.pedReferences.IsRetained(),
                      "cleared filter alone cannot authorize producer End to discard native reference debt");
                // The existing reference cleanup requires a known semantic
                // state even when state9 is not required. Supply proved native
                // frontend7, not loaded-world admission or a forced release.
                writes.Global(NativeGameStateProfile::stateRva, std::uint32_t{7});
                Host::scenario = 0;
                retire(service);
                Check(Host::calls == completedCalls && ClockPedReferenceOwnerTest::unregisters == unlinksBeforeIntent + 1 && !field(),
                      "proved frontend state enables one existing unregister without replaying the already-acknowledged filter CAS");
                continue;
            }
            const auto unlinks = ClockPedReferenceOwnerTest::unregisters;
            if (scenario == 3)
            {
                Check(result == ClockResult::Ok && field() == firstPed, "foreign replacement case begins from actual owned state");
                writes.Global(P::GlobalRva, std::uintptr_t{0x12345678});
            }
            else
                Check(result != ClockResult::Ok && runtime.stopping.load() && WorldIgnoreRetained() && runtime.pedReferences.IsRetained(),
                      "post-CAS unreadable acknowledgement or lost loaded-world state retains pointer and native cell obligations");
            const auto callsBeforeEnd = Host::calls;
            Check(!service.End(), "producer End cannot clear native global or release reference cells itself");
            if (scenario == 1 || scenario == 4)
            {
                fixture.invokeTimer();
                Check(WorldIgnoreRetained() && runtime.pedReferences.IsRetained() && ClockPedReferenceOwnerTest::unregisters == unlinks &&
                          Host::calls == callsBeforeEnd && Host::protectionChanged,
                      "unreadable global retains callback/module/reference debt without unregister, blind CAS or replay");
                DWORD ignored = 0;
                Check(VirtualProtect(reinterpret_cast<void*>(global), sizeof(void*), Host::priorProtection, &ignored) != FALSE,
                      "restore only the owned fixture global's prior page protection");
            }
            Host::scenario = 0;
            retire(service);
            if (scenario == 3)
                Check(field() == 0x12345678 && Host::calls == callsBeforeEnd,
                      "stable foreign replacement acknowledges our absence without a write or leaked reference debt");
            else
                Check(!field() && Host::calls == callsBeforeEnd + 1,
                      "one qualified null CAS clears current candidate before End retires relations, even after world/state9 loss");
        }
        Host::image = 0;
        Host::reenter = nullptr;
        writes.Restore();
        ClockReaderTestHost::image = priorReaderImage;
        ClockPedReferenceOwnerTest::image = priorReferenceImage;
        std::puts("Production native ignored-entity journal owner fixture completed");
        std::puts("Production actual CWorld IgnoreEntity SDK owner fixture completed");
    }
}
