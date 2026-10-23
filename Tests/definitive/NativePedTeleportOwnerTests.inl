/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: One genuine retained callback for native teleport entry and failure
 ****************************************************************************/
namespace
{
    struct ClockTeleportPool
    {
        ClockImageFixture& fixture;
        std::uintptr_t     root;
        unsigned char *    header = nullptr, *objects = nullptr, *flags = nullptr;
        ClockTeleportPool(ClockImageFixture& fixture, std::uintptr_t root, std::size_t count, std::size_t stride) : fixture(fixture), root(root)
        {
            header = static_cast<unsigned char*>(::VirtualAlloc(nullptr, 32, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
            objects = static_cast<unsigned char*>(::VirtualAlloc(nullptr, count * stride, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
            flags = static_cast<unsigned char*>(::VirtualAlloc(nullptr, count, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
            Check(header && objects && flags, "own initialized native-style pool/header/flag banks for one real callback action");
            const auto put = [&](std::size_t offset, const auto& value) { std::memcpy(header + offset, &value, sizeof(value)); };
            put(0, reinterpret_cast<std::uintptr_t>(objects));
            put(8, reinterpret_cast<std::uintptr_t>(flags));
            put(0x10, static_cast<std::int32_t>(count));
            put(0x14, std::int32_t{-1});
            put(0x18, static_cast<unsigned char>(1));
            std::memset(flags, 0x80, count);
        }
        void Publish() const { fixture.Put(root, reinterpret_cast<std::uintptr_t>(header)); }
        ~ClockTeleportPool()
        {
            ::VirtualFree(flags, 0, MEM_RELEASE);
            ::VirtualFree(objects, 0, MEM_RELEASE);
            ::VirtualFree(header, 0, MEM_RELEASE);
        }
    };
    struct PedTeleportOwnerContext
    {
        ClockImageFixture* fixture = nullptr;
        ClockTeleportPool *peds = nullptr, *intelligence = nullptr;
        CVector            target{123.25f, -456.5f, 789.75f};
        ClockResult        result = ClockResult::NotInitialized;
    };
    // Both the raw primitive regression and actual SDK completion need the
    // identical short-lived actor graph. Sharing its admission prevents a
    // second, weaker synthetic actor path in the placement owner fixture.
    template <class Action>
    void WithPedTeleportActor(ClockImageFixture& fixture, ClockTeleportPool& peds, Action&& action)
    {
        namespace P = NativePedTeleportProfile;
        // The existing graph already registers each raw object and class. Add
        // only the proved skeletal actor/root associations for this borrowed
        // call; no UObject or class snapshot escapes the callback.
        StartGameGraph graph(fixture);
        graph.chains[1][2] = StartGameGraph::Address(graph.gameClass) + 0x30;
        graph.chains[5][2] = StartGameGraph::Address(graph.managerClass) + 0x30;
        graph.chains[2][2] = StartGameGraph::Address(graph.levelClass) + 0x30;
        graph.chains[0][0] = StartGameGraph::Address(graph.worldClass) + 0x30;
        StartGameGraph::Store(graph.worldClass, 0x30, StartGameGraph::Address(graph.chains[0]));
        StartGameGraph::Store(graph.worldClass, 0x38, std::int32_t{0});
        StartGameGraph::Store(graph.game, 0, fixture.image + P::ActorVtableRva);
        StartGameGraph::Store(graph.game, P::ActorRootOffset, StartGameGraph::Address(graph.manager));
        StartGameGraph::Store(graph.game, P::ActorMeshOffset, StartGameGraph::Address(graph.manager));
        StartGameGraph::Store(graph.manager, P::ComponentOuterOffset, StartGameGraph::Address(graph.game));
        fixture.Put(P::ActorClassCacheRva, StartGameGraph::Address(graph.gameClass));
        fixture.Put(P::ComponentClassCacheRva, StartGameGraph::Address(graph.managerClass));
        fixture.Put(NativeStartupWorldProfile::levelClassCacheRva, StartGameGraph::Address(graph.levelClass));
        const auto actor = StartGameGraph::Address(graph.game);
        std::memcpy(peds.objects + P::ActorOffset, &actor, sizeof(actor));
        action();
        const std::uintptr_t zero = 0;
        std::memcpy(peds.objects + P::ActorOffset, &zero, sizeof(zero));
        fixture.Put(P::ActorClassCacheRva, zero);
        fixture.Put(P::ComponentClassCacheRva, zero);
    }
    void PedTeleportOwnerHandler(NativeGameTimingAccess& access, void* opaque)
    {
        auto& context = *static_cast<PedTeleportOwnerContext*>(opaque);
        WithPedTeleportActor(*context.fixture, *context.peds, [&] { context.result = access.TeleportPed(1, context.target); });
    }
    void NativePedTeleportOwnedSdk(ClockImageFixture& fixture)
    {
        namespace P = NativePedTeleportProfile;
        namespace T = NativePedTransformProfile;
        namespace Ped = NativePedPoolProfile;
        struct Saved
        {
            std::uintptr_t             rva;
            std::vector<unsigned char> bytes;
        };
        std::vector<Saved> saved;
        const auto         admit = [&](std::uintptr_t rva, const void* bytes, std::size_t size, DWORD protection)
        {
            Saved  prior{rva, std::vector<unsigned char>(size)};
            SIZE_T copied = 0;
            Check(::ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(fixture.image + rva), prior.bytes.data(), size, &copied) && copied == size,
                  "save only bounded owned image spans before the native teleport owner cohort");
            saved.push_back(std::move(prior));
            fixture.Patch(rva, bytes, size);
            DWORD previous = 0;
            Check(::VirtualProtect(reinterpret_cast<void*>(fixture.image + rva), size, protection, &previous) != FALSE,
                  "native teleport retains real executable/read-only/data page policy in the actual Clock fixture");
        };
        for (const auto& pin : Ped::Code)
            admit(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : T::Code)
            admit(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : P::Code)
            admit(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : P::Names)
            admit(pin.rva, pin.bytes, pin.size, PAGE_READONLY);
        for (const auto& pin : NativeGameStateProfile::pins)
            admit(pin.rva, pin.bytes, pin.size, pin.code ? PAGE_EXECUTE_READ : PAGE_READONLY);
        for (const auto& pin : NativeBorrowedGcProfile::pins)
            admit(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : NativeStartGameProfile::Code)
            if (pin.rva == 0xab879c || pin.rva == 0xab92f0 || pin.rva == 0x1af4ef0)
                admit(pin.rva, pin.bytes.data(), pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : NativeUObjectMembershipProfile::pins)
            admit(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : NativeStartupWorldProfile::pins)
            admit(pin.rva, pin.bytes, pin.size, pin.code ? PAGE_EXECUTE_READ : PAGE_READONLY);
        for (const auto& slot : P::Slots)
        {
            const auto target = fixture.image + slot.targetRva;
            admit(slot.rva, &target, sizeof(target), PAGE_READONLY);
        }
        const auto method = fixture.image + Ped::Code[1].rva;
        admit(T::PlayerVtableRva, &method, sizeof(method), PAGE_READONLY);
        admit(T::PedVtableRva, &method, sizeof(method), PAGE_READONLY);
        admit(T::ScriptVelocityInScaleRva, &T::ScriptVelocityInScaleBits, 4, PAGE_READONLY);
        admit(T::ScriptVelocityOutScaleRva, &T::ScriptVelocityOutScaleBits, 4, PAGE_READONLY);
        const auto getter = fixture.image + NativeStartupWorldProfile::getterRva;
        admit(NativeStartupWorldProfile::ownerGetWorldSlotRva, &getter, sizeof(getter), PAGE_READONLY);
        const std::uintptr_t zero = 0;
        for (const auto rva : {Ped::RootRva, P::IntelligenceRootRva, P::PointerRootRva, P::EntryRootRva, P::ActorClassCacheRva, P::ComponentClassCacheRva,
                               NativeStartupWorldProfile::objectChunksRva, NativeUiClassMapProfile::uiTransitionClassCacheRva,
                               NativeStartupOwnerProfile::gameModeClassCacheRva, NativeStartupWorldProfile::levelClassCacheRva,
                               NativeStartupProvidersProfile::playerControllerClassCacheRva, NativeStartupProvidersProfile::viewportClassCacheRva,
                               NativeStartupProvidersProfile::managerClassCacheRva, NativeStartupProvidersProfile::engineRva})
            admit(rva, &zero, sizeof(zero), PAGE_READWRITE);
        const std::uint32_t state = 9, count = 0;
        const auto          thread = GetCurrentThreadId();
        const unsigned char initialized = 1;
        admit(NativeStartupWorldProfile::objectCountRva, &count, sizeof(count), PAGE_READWRITE);
        admit(NativeGameStateProfile::stateRva, &state, sizeof(state), PAGE_READWRITE);
        admit(NativeStartGameProfile::ThreadInitializedRva, &initialized, 1, PAGE_READWRITE);
        admit(NativeStartGameProfile::ThreadIdRva, &thread, sizeof(thread), PAGE_READWRITE);
        admit(NativeBorrowedGcProfile::markerRva, &count, sizeof(count), PAGE_READWRITE);
        ClockTeleportPool peds(fixture, Ped::RootRva, Ped::NativeCapacity, Ped::ObjectStride);
        ClockTeleportPool intelligence(fixture, P::IntelligenceRootRva, P::IntelligenceCapacity, P::IntelligenceStride);
        ClockTeleportPool pointers(fixture, P::PointerRootRva, P::PointerCapacity, P::PointerStride);
        ClockTeleportPool entries(fixture, P::EntryRootRva, P::EntryCapacity, P::EntryStride);
        peds.Publish();
        intelligence.Publish();
        pointers.Publish();
        entries.Publish();
        peds.flags[0] = intelligence.flags[0] = 1;
        auto* matrix = static_cast<unsigned char*>(::VirtualAlloc(nullptr, 0x60, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        Check(matrix != nullptr, "own one native-style matrix without asking the engine to allocate a wrapper");
        const auto nativePed = reinterpret_cast<std::uintptr_t>(peds.objects);
        const auto matrixAddress = reinterpret_cast<std::uintptr_t>(matrix);
        const auto vtable = fixture.image + T::PlayerVtableRva;
        const auto data = reinterpret_cast<std::uintptr_t>(intelligence.objects);
        std::memcpy(peds.objects, &vtable, sizeof(vtable));
        std::memcpy(peds.objects + T::MatrixPointerOffset, &matrixAddress, sizeof(matrixAddress));
        std::memcpy(peds.objects + P::IntelligenceOffset, &data, sizeof(data));
        peds.objects[P::EntityTypeOffset] = 3;
        std::memcpy(intelligence.objects, &nativePed, sizeof(nativePed));
        std::memcpy(intelligence.objects + P::IntelligenceOwnerOffset, &nativePed, sizeof(nativePed));
        std::memcpy(matrix + T::MatrixOwnerOffset, &nativePed, sizeof(nativePed));
        PedTeleportOwnerContext context{&fixture, &peds, &intelligence};
        ClockReaderTestHost::image = ClockPedTeleportOwnerTest::image = fixture.image;
        ClockPedTeleportOwnerTest::calls = 0;
        ClockPedTeleportOwnerTest::result = ClockResult::Ok;
        ClockPedTeleportOwnerTest::effect = true;
        Check(runtime.access.TeleportPed(1, context.target) == ClockResult::NotInitialized && !ClockPedTeleportOwnerTest::calls,
              "unbound Runtime cannot turn a qualified native handle into teleport authority");
        const auto retire = [&](NativeClockService& service)
        {
            if (!service.End())
            {
                fixture.invokeTimer();
                Check(service.End(), "native teleport owner drains real pending callback work");
            }
            Check(!service.IsRetained() && !runtime.timingHandler && !runtime.timingContext &&
                      !NativeServiceAdmission::IsClaimed(NativeServiceAdmission::Category::Clock),
                  "native teleport effects create no invented rollback debt after actual owner retirement");
        };
        {
            NativeClockService service;
            BeginOwned(service, fixture, false, true, true, &PedTeleportOwnerHandler, &context);
            Check(runtime.access.TeleportPed(1, context.target) != ClockResult::Ok && !ClockPedTeleportOwnerTest::calls,
                  "between-callback producer access cannot enter native teleport under a matching thread ID");
            bool        refused = false;
            std::thread foreign([&] { refused = runtime.access.TeleportPed(1, context.target) != ClockResult::Ok; });
            foreign.join();
            Check(refused && !ClockPedTeleportOwnerTest::calls, "foreign-thread access cannot borrow the actual native callback");
            fixture.invokeTimer();
            Check(context.result == ClockResult::Ok && ClockPedTeleportOwnerTest::calls == 1 && ClockPedTeleportOwnerTest::receiver == nativePed &&
                      ClockPedTeleportOwnerTest::entry == T::NativeTeleportRva && ClockPedTeleportOwnerTest::argument,
                  "one admitted real callback reaches the qualified native teleport entry with its actual receiver and true argument");
            retire(service);
            CVector position;
            std::memcpy(&position, matrix + T::MatrixPositionOffset, sizeof(position));
            Check(position.fX == context.target.fX && position.fY == context.target.fY && position.fZ == context.target.fZ,
                  "End preserves accepted native teleport effects without restoring an old position snapshot");
        }
        {
            NativeClockService service;
            ClockPedTeleportOwnerTest::result = ClockResult::OriginalException;
            const auto calls = ClockPedTeleportOwnerTest::calls;
            context.target.fX += 10;
            BeginOwned(service, fixture, false, true, true, &PedTeleportOwnerHandler, &context);
            fixture.invokeTimer();
            Check(context.result == ClockResult::OriginalException && runtime.stopping.load() && ClockPedTeleportOwnerTest::calls == calls + 1,
                  "an entered native failure stops the same real retained owner after exactly one action");
            Check(runtime.access.TeleportPed(1, context.target) != ClockResult::Ok && ClockPedTeleportOwnerTest::calls == calls + 1,
                  "the failed owner never retries an already-entered native teleport action");
            retire(service);
            CVector position;
            std::memcpy(&position, matrix + T::MatrixPositionOffset, sizeof(position));
            Check(position.fX == context.target.fX, "entered native effects survive failure and terminal owner cleanup honestly");
        }
        ClockPedTeleportOwnerTest::result = ClockResult::Ok;
        ClockReaderTestHost::image = ClockPedTeleportOwnerTest::image = 0;
        for (auto item = saved.rbegin(); item != saved.rend(); ++item)
            fixture.Patch(item->rva, item->bytes.data(), item->bytes.size());
        Check(::VirtualFree(matrix, 0, MEM_RELEASE) != FALSE, "release only the owned matrix after callback retirement and native selector restoration");
        std::puts("Production native ped Teleport callback owner fixture completed");
    }
}
