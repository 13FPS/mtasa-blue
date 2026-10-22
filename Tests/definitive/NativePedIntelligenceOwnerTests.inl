/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Intelligence observations share actual token/callback retirement
 ****************************************************************************/
namespace
{
    struct PedIntelligenceOwnerContext
    {
        NativePedReferenceToken            token;
        NativePedIntelligenceReader::Value value, raw;
        ClockResult                        result = ClockResult::NotInitialized, rawResult = ClockResult::NotInitialized;
        unsigned                           operation = 0;
        bool                               alsoRaw = false;
    };
    void PedIntelligenceOwnerHandler(NativeGameTimingAccess& access, void* opaque)
    {
        auto& context = *static_cast<PedIntelligenceOwnerContext*>(opaque);
        context.result =
            context.operation == 0 ? access.AcquirePedReference(1, context.token) : access.ReadReferencedPedIntelligence(context.token, context.value);
        if (context.alsoRaw)
        {
            NativePedPoolReader        pool({runtime.image}, CheckTimingOwner);
            NativePedPoolReader::Value ped;
            context.rawResult = pool.Resolve(1, ped);
            if (context.rawResult == ClockResult::Ok)
                context.rawResult = NativePedIntelligenceReader({runtime.image}, CheckTimingOwner).Read(ped, context.raw);
        }
    }
    struct DefaultPedIntelligenceOwner final : NativeGameTimingAccess
    {
        ClockResult SetTimeScale(float) noexcept override { return ClockResult::NotInitialized; }
        ClockResult SetMinuteDuration(std::uint32_t) noexcept override { return ClockResult::NotInitialized; }
        ClockResult Pause(bool) noexcept override { return ClockResult::NotInitialized; }
    };
    void NativePedIntelligenceOwnedSdk(ClockImageFixture& fixture)
    {
        namespace R = NativePedReferenceProfile;
        namespace Ped = NativePedPoolProfile;
        namespace I = NativePedIntelligenceProfile;
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
                  "save bounded intelligence/reference proof in the actual retained callback image");
            saved.push_back(std::move(prior));
            fixture.Patch(rva, bytes, size);
            DWORD previous = 0;
            Check(::VirtualProtect(reinterpret_cast<void*>(fixture.image + rva), size, protection, &previous) != FALSE,
                  "intelligence owner uses real executable pins, relocated slots and mutable native banks");
        };
        for (const auto& pin : Ped::Code)
            admit(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : R::Code)
            admit(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : I::Code)
            admit(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : NativeGameStateProfile::pins)
            admit(pin.rva, pin.bytes, pin.size, pin.code ? PAGE_EXECUTE_READ : PAGE_READONLY);
        for (const auto& pin : NativeBorrowedGcProfile::pins)
            admit(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : NativeStartGameProfile::Code)
            if (pin.rva == 0xab879c || pin.rva == 0xab92f0 || pin.rva == 0x1af4ef0)
                admit(pin.rva, pin.bytes.data(), pin.size, PAGE_EXECUTE_READ);
        for (const auto& slot : R::Slots)
        {
            const auto target = fixture.image + slot.targetRva;
            admit(slot.rva, &target, sizeof(target), PAGE_READONLY);
        }
        const std::uintptr_t zero = 0;
        const std::uint32_t  state = 9, marker = 0;
        const auto           thread = GetCurrentThreadId();
        const unsigned char  initialized = 1;
        admit(Ped::RootRva, &zero, sizeof(zero), PAGE_READWRITE);
        admit(I::RootRva, &zero, sizeof(zero), PAGE_READWRITE);
        admit(NativeGameStateProfile::stateRva, &state, sizeof(state), PAGE_READWRITE);
        admit(NativeStartGameProfile::ThreadInitializedRva, &initialized, 1, PAGE_READWRITE);
        admit(NativeStartGameProfile::ThreadIdRva, &thread, sizeof(thread), PAGE_READWRITE);
        admit(NativeBorrowedGcProfile::markerRva, &marker, sizeof(marker), PAGE_READWRITE);
        std::vector<unsigned char> nodes(R::ReferenceCount * R::ReferenceBytes);
        for (std::size_t i = 0; i + 1 < R::ReferenceCount; ++i)
        {
            const auto next = fixture.image + R::ReferencesRva + (i + 1) * R::ReferenceBytes;
            std::memcpy(nodes.data() + i * R::ReferenceBytes, &next, sizeof(next));
        }
        admit(R::ReferencesRva, nodes.data(), nodes.size(), PAGE_READWRITE);
        const auto first = fixture.image + R::ReferencesRva;
        admit(R::FreeHeadRva, &first, sizeof(first), PAGE_READWRITE);
        ClockTeleportPool peds(fixture, Ped::RootRva, Ped::NativeCapacity, Ped::ObjectStride);
        ClockTeleportPool intelligence(fixture, I::RootRva, I::NativeCapacity, I::ObjectStride);
        peds.Publish();
        intelligence.Publish();
        peds.flags[0] = 1;
        intelligence.flags[3] = 19;
        const auto ped = reinterpret_cast<std::uintptr_t>(peds.objects);
        const auto nativeIntelligence = reinterpret_cast<std::uintptr_t>(intelligence.objects + 3 * I::ObjectStride);
        const auto vtable = fixture.image + I::PedVtableRva;
        std::memcpy(peds.objects, &vtable, sizeof(vtable));
        peds.objects[I::TypeOffset] = 3;
        std::memcpy(peds.objects + I::IntelligenceOffset, &nativeIntelligence, sizeof(nativeIntelligence));
        std::array<std::uintptr_t, 13> prefix{};
        prefix[0] = prefix[12] = ped;
        // These are intentionally opaque/unreadable task addresses. The real
        // Intelligence reader must never manufacture a task wrapper or call.
        prefix[1] = 1;
        prefix[5] = 0x111;
        prefix[6] = 0x222;
        prefix[11] = 0x333;
        std::memcpy(reinterpret_cast<void*>(nativeIntelligence), prefix.data(), sizeof(prefix));
        ClockReaderTestHost::image = ClockPedReferenceOwnerTest::image = fixture.image;
        ClockPedReferenceOwnerTest::registers = ClockPedReferenceOwnerTest::unregisters = 0;
        ClockPedReferenceOwnerTest::partialUnlink = false;
        ClockPedReferenceOwnerTest::unregisterResult = ClockResult::Ok;
        PedIntelligenceOwnerContext context;
        context.value.nativeIntelligence = 123;
        context.value.primaryTasks[0] = 456;
        DefaultPedIntelligenceOwner absent;
        Check(absent.ReadReferencedPedIntelligence(context.token, context.value) == ClockResult::NotInitialized && context.value.nativeIntelligence == 123 &&
                  context.value.primaryTasks[0] == 456 &&
                  runtime.access.ReadReferencedPedIntelligence(context.token, context.value) == ClockResult::NotInitialized,
              "default and unbound Intelligence observations preserve caller output without fabricated services");
        NativeClockService service;
        BeginOwned(service, fixture, false, true, true, &PedIntelligenceOwnerHandler, &context);
        fixture.invokeTimer();
        Check(context.result == ClockResult::Ok && runtime.pedReferences.IsRetained(), "intelligence observation starts with actual native-cell registration");
        Check(runtime.access.ReadReferencedPedIntelligence(context.token, context.value) != ClockResult::Ok && context.value.nativeIntelligence == 123,
              "a producer with a live token still cannot read intelligence between admitted native callbacks");
        context.operation = 1;
        const auto invoke = [&]
        {
            Check(service.Renew(runtime.generation.load()), "renew the same native owner between bounded Intelligence observations");
            fixture.invokeTimer();
        };
        invoke();
        Check(context.result == ClockResult::Ok && context.value.ped.nativePed == ped && context.value.nativeIntelligence == nativeIntelligence &&
                  context.value.slot == 3 && context.value.generation == 19 && context.value.taskManager == nativeIntelligence + I::ManagerOffset &&
                  context.value.primaryTasks.front() == 1 && context.value.primaryTasks.back() == 0x111 && context.value.secondaryTasks.front() == 0x222 &&
                  context.value.secondaryTasks.back() == 0x333,
              "actual callback publishes the independently occupied intelligence row and real embedded arrays without task dereference");
        const auto token = context.token;
        ++context.token.version;
        context.value.nativeIntelligence = 123;
        invoke();
        Check(context.result != ClockResult::Ok && context.value.nativeIntelligence == 123 && !runtime.stopping.load(),
              "a stale CPU token cannot publish a still-readable native intelligence snapshot");
        context.token = token;
        ClockResult                        foreign = ClockResult::Ok;
        NativePedIntelligenceReader::Value foreignOutput;
        foreignOutput.nativeIntelligence = 789;
        std::thread other([&] { foreign = runtime.access.ReadReferencedPedIntelligence(context.token, foreignOutput); });
        other.join();
        Check(foreign != ClockResult::Ok && foreignOutput.nativeIntelligence == 789, "a foreign thread cannot borrow the current native Intelligence grant");
        ClockPedIntelligenceOwnerTest::address = nativeIntelligence;
        ClockPedIntelligenceOwnerTest::cell = ClockPedReferenceOwnerTest::cell;
        ClockPedIntelligenceOwnerTest::clearOnRead = true;
        const auto invalidations = ClockPedIntelligenceOwnerTest::invalidations;
        context.alsoRaw = true;
        invoke();
        Check(context.result != ClockResult::Ok && context.value.nativeIntelligence == 123 && context.rawResult == ClockResult::Ok &&
                  context.raw.nativeIntelligence == nativeIntelligence && ClockPedIntelligenceOwnerTest::invalidations == invalidations + 1 &&
                  !runtime.stopping.load(),
              "fresh raw Intelligence remains observable but post-read token loss prevents publishing its unacknowledged snapshot");
        // Restore only the owned host word for genuine native unlink. The
        // revoked caller token is never republished or used to call a task.
        ClockPedReferenceOwnerTest::Put(ClockPedReferenceOwnerTest::cell, ped);
        const auto unlinks = ClockPedReferenceOwnerTest::unregisters;
        Check(!service.End() && runtime.pedReferences.IsRetained(), "Intelligence End retains registered storage until natural callback retirement");
        fixture.invokeTimer();
        Check(service.End() && !service.IsRetained() && !runtime.pedReferences.IsRetained() && !runtime.timingHandler &&
                  ClockPedReferenceOwnerTest::unregisters == unlinks + 1 && !NativeServiceAdmission::IsClaimed(NativeServiceAdmission::Category::Clock),
              "Intelligence observations retire through the same exact reference-cell and module lifetime gate");
        Check(std::memcmp(reinterpret_cast<void*>(nativeIntelligence), prefix.data(), sizeof(prefix)) == 0,
              "observation and retirement do not reset intelligence, tasks or the embedded manager");
        context.value.nativeIntelligence = 123;
        Check(runtime.access.ReadReferencedPedIntelligence(token, context.value) != ClockResult::Ok && context.value.nativeIntelligence == 123,
              "retired generations expose no intelligence or task snapshot");
        ClockPedIntelligenceOwnerTest::clearOnRead = false;
        ClockPedIntelligenceOwnerTest::address = ClockPedIntelligenceOwnerTest::cell = 0;
        ClockReaderTestHost::image = ClockPedReferenceOwnerTest::image = 0;
        for (auto item = saved.rbegin(); item != saved.rend(); ++item)
            fixture.Patch(item->rva, item->bytes.data(), item->bytes.size());
        std::puts("Production native ped intelligence callback owner fixture completed");
    }
}
