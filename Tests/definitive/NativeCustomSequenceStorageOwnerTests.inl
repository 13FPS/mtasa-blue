/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual callback service owns storage through natural End recovery
 ****************************************************************************/
namespace CustomSequenceStorageOwnerFixture
{
    namespace Host = NativeCustomAnimSequenceStorageFixture;
    using Service = NativeAnimationSequenceStorageAccess;
    constexpr std::uintptr_t Sentinel = 0x12345678;
    struct Effect
    {
        WorldGroundOwnerFixture::Writes* writes = nullptr;
        std::uint64_t*                   callerGeneration = nullptr;
        std::size_t*                     callerCapacity = nullptr;
        bool                             loseScope = false, retainedAtEntry = false;
    };
    void AfterMalloc(Host::State& host) noexcept
    {
        auto& effect = *static_cast<Effect*>(host.context);
        effect.retainedAtEntry = runtime.customSequences.IsRetained() && runtime.customSequenceGeneration.load() == runtime.generation.load();
        if (effect.callerGeneration)
            ++*effect.callerGeneration;
        if (effect.callerCapacity)
            *effect.callerCapacity = 65536;
        if (effect.loseScope)
            effect.writes->Global(NativeBorrowedGcProfile::markerRva, LONG{1});
    }
    void AfterFree(Host::State& host) noexcept
    {
        auto& effect = *static_cast<Effect*>(host.context);
        effect.writes->Global(NativeBorrowedGcProfile::markerRva, LONG{1});
    }
    void Run(ClockImageFixture& fixture, unsigned irreversibleScenario)
    {
        WorldGroundOwnerFixture::Writes writes{fixture, {}};
        const auto                      admit = [&](const auto& pins)
        {
            for (const auto& pin : pins)
                WorldGroundOwnerFixture::Writes::Apply(&writes, pin.rva, pin.bytes, pin.size, pin.code ? PAGE_EXECUTE_READ : PAGE_READONLY);
        };
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
        Host::State host;
        host.Reset(fixture.image, &WorldGroundOwnerFixture::Writes::Apply, &writes);
        Host::active = &host;
        Effect effect{&writes};
        host.context = &effect;
        const auto resetContext = [&]
        {
            graph.Reset();
            writes.Global(NativeGameStateProfile::stateRva, std::uint32_t{9});
            writes.Global(NativeStartGameProfile::ThreadInitializedRva, static_cast<unsigned char>(1));
            writes.Global(NativeStartGameProfile::ThreadIdRva, GetCurrentThreadId());
            writes.Global(NativeBorrowedGcProfile::markerRva, LONG{0});
        };
        resetContext();
        const auto previousImage = ClockReaderTestHost::image;
        ClockReaderTestHost::image = fixture.image;
        WorldGroundOwnerFixture::Callback callback;
        NativeClockService                service;
        ClockResult                       result = ClockResult::NotInitialized;
        std::uintptr_t                    output = Sentinel;
        Service*                          captured = nullptr;
        callback.action = [&](NativeGameTimingAccess& access) { captured = dynamic_cast<Service*>(&access); };
        BeginOwned(service, fixture, false, true, true, &WorldGroundOwnerFixture::Callback::Invoke, &callback);
        fixture.invokeTimer();
        Check(captured == static_cast<Service*>(&runtime.access),
              "actual natural timing callback exposes the same owner's optional storage service by cross-cast");
        const auto generation = runtime.generation.load();
        const auto invoke = [&]
        {
            Check(service.Renew(generation), "renew only the admitted custom sequence storage callback owner");
            fixture.invokeTimer();
        };
        if (!irreversibleScenario)
        {
            Check(captured->AllocateAnimationSequences(generation, 1, output) != ClockResult::Ok && output == Sentinel && !host.mallocCalls,
                  "borrowed optional service grants no allocation authority between callbacks");
            std::thread foreign([&] { result = captured->AllocateAnimationSequences(generation, 1, output); });
            foreign.join();
            Check(result != ClockResult::Ok && output == Sentinel && !host.mallocCalls && !runtime.stopping.load(),
                  "foreign producer cannot allocate or stop the valid current owner");
            callback.action = [&](NativeGameTimingAccess&) { result = captured->AllocateAnimationSequences(generation + 1, 1, output); };
            invoke();
            Check(result == ClockResult::OwnerConflict && output == Sentinel && !host.mallocCalls && !runtime.stopping.load(),
                  "wrong requested generation refuses before allocation without stopping the current owner");
            callback.action = [&](NativeGameTimingAccess&) { result = captured->AllocateAnimationSequences(generation, 0, output); };
            invoke();
            Check(
                result == ClockResult::Ok && !output && !host.mallocCalls && !runtime.customSequences.IsRetained() && !runtime.customSequenceGeneration.load(),
                "admitted zero-capacity call returns null without a retained native resource");
            std::uint64_t callerGeneration = generation;
            std::size_t   callerCapacity = 2;
            effect.callerGeneration = &callerGeneration;
            effect.callerCapacity = &callerCapacity;
            host.afterMalloc = &AfterMalloc;
            callback.action = [&](NativeGameTimingAccess&) { result = captured->AllocateAnimationSequences(callerGeneration, callerCapacity, output); };
            invoke();
            Check(result == ClockResult::Ok && output == host.base + 8 && host.bytes == 8 + 2 * 80 && callerGeneration == generation + 1 &&
                      callerCapacity == 65536 && effect.retainedAtEntry && runtime.customSequences.IsRetained() &&
                      runtime.customSequenceGeneration.load() == generation,
                  "actual service copies scalar intent, retains journal before entry and commits one fully initialized first tag");
            host.afterMalloc = nullptr;
            effect.callerGeneration = nullptr;
            effect.callerCapacity = nullptr;
            Check(captured->FreeAnimationSequences(generation, output) != ClockResult::Ok && !host.freeCalls && runtime.customSequences.IsRetained(),
                  "producer cannot retire native storage outside the exact natural callback");
            callback.action = [&](NativeGameTimingAccess&) { result = captured->FreeAnimationSequences(generation, output); };
            invoke();
            Check(result == ClockResult::Ok && host.freeCalls == 1 && !runtime.customSequences.IsRetained() && !runtime.customSequenceGeneration.load(),
                  "actual optional service releases its current issued allocation only through the callback owner");
            output = Sentinel;
            callback.action = [&](NativeGameTimingAccess&) { result = captured->AllocateAnimationSequences(generation, 1, output); };
            invoke();
            Check(result == ClockResult::Ok && runtime.customSequences.IsRetained(), "prepare successful unpublished storage for End");
            const auto beforeEnd = host.freeCalls;
            Check(!service.End() && service.IsRetained() && runtime.timerHooked.load() && runtime.customSequences.IsRetained() && host.freeCalls == beforeEnd,
                  "producer End retains callback hooks and storage without invoking free on the producer thread");
            fixture.invokeTimer();
            Check(host.freeCalls == beforeEnd + 1 && !runtime.customSequences.IsRetained() && !runtime.customSequenceGeneration.load(),
                  "next actual natural timer drains known unpublished storage before owner hook retirement");
            Check(service.End() && !service.IsRetained() && !runtime.timerHooked.load() && !runtime.timingHandler && !runtime.timingContext &&
                      !NativeServiceAdmission::IsClaimed(NativeServiceAdmission::Category::Clock),
                  "completed natural cleanup permits End to release exact callback and shared service admission");

            // Lost acknowledgement after malloc must retain the known base.
            // Keep native GC closed until its original callback can recover.
            resetContext();
            callback.action = [&](NativeGameTimingAccess& access)
            {
                captured = dynamic_cast<Service*>(&access);
                result = captured->AllocateAnimationSequences(runtime.generation.load(), 2, output);
            };
            effect.loseScope = true;
            effect.retainedAtEntry = false;
            host.afterMalloc = &AfterMalloc;
            output = Sentinel;
            BeginOwned(service, fixture, false, true, true, &WorldGroundOwnerFixture::Callback::Invoke, &callback);
            fixture.invokeTimer();
            const auto failedGeneration = runtime.generation.load();
            Check(result != ClockResult::Ok && output == Sentinel && runtime.stopping.load() && runtime.customSequences.IsRetained() &&
                      runtime.customSequences.RecoveryPending() && runtime.customSequenceGeneration.load() == failedGeneration,
                  "entered known allocation acknowledgement loss stops the runtime and retains private cleanup authority without publishing output");
            const auto failedFreeCalls = host.freeCalls;
            Check(!service.End() && service.IsRetained() && runtime.timerHooked.load() && host.freeCalls == failedFreeCalls,
                  "GC refusal keeps known unpublished storage and hooks retained across producer End");
            resetContext();
            effect.loseScope = false;
            host.afterMalloc = nullptr;
            fixture.invokeTimer();
            Check(host.freeCalls == failedFreeCalls + 1 && !runtime.customSequences.IsRetained() && service.End() && !service.IsRetained(),
                  "same original generation naturally recovers known allocation debt once GC scope returns");

            resetContext();
            callback.action = [&](NativeGameTimingAccess& access)
            {
                captured = dynamic_cast<Service*>(&access);
                result = captured->AllocateAnimationSequences(runtime.generation.load(), 1, output);
            };
            BeginOwned(service, fixture, false, true, true, &WorldGroundOwnerFixture::Callback::Invoke, &callback);
            fixture.invokeTimer();
            Check(result == ClockResult::Ok, "prepare current issued allocation for returned-free acknowledgement loss");
            const auto freedGeneration = runtime.generation.load();
            const auto returnedFreeCalls = host.freeCalls;
            host.afterFree = &AfterFree;
            callback.action = [&](NativeGameTimingAccess&) { result = captured->FreeAnimationSequences(freedGeneration, output); };
            Check(service.Renew(freedGeneration), "renew the one actual storage-free attempt");
            fixture.invokeTimer();
            Check(result != ClockResult::Ok && runtime.stopping.load() && host.freeCalls == returnedFreeCalls + 1 && !runtime.customSequences.IsRetained() &&
                      !runtime.customSequenceGeneration.load(),
                  "known returned free is consumed before failed acknowledgement and never becomes reusable cleanup debt");
            resetContext();
            host.afterFree = nullptr;
            WorldGroundOwnerFixture::Retire(service, fixture);
            Check(host.freeCalls == returnedFreeCalls + 1, "End never repeats a known returned native free after acknowledgement recovery");
            Host::active = nullptr;
            ClockReaderTestHost::image = previousImage;
            writes.Restore();
            std::puts("Production native custom sequence storage callback owner fixture completed");
            std::puts("Production native custom sequence storage natural End fixture completed");
            return;
        }

        // Each unknown-effect scenario runs in its own ordinary test process.
        // Never reset/discard the real global owner to make another case fit.
        if (irreversibleScenario == 1)
            host.mallocResult = ClockResult::OriginalException;
        callback.action = [&](NativeGameTimingAccess&) { result = captured->AllocateAnimationSequences(generation, 1, output); };
        invoke();
        if (irreversibleScenario == 2)
        {
            Check(result == ClockResult::Ok, "prepare actual issued storage before an unknown entered-free effect");
            host.freeResult = ClockResult::OriginalException;
            host.consumeFailedFree = true;
            callback.action = [&](NativeGameTimingAccess&) { result = captured->FreeAnimationSequences(generation, output); };
            invoke();
        }
        Check(result == ClockResult::OriginalException && runtime.stopping.load() && runtime.customSequences.IsRetained() &&
                  runtime.customSequences.RecoveryPending() && runtime.customSequenceGeneration.load() == generation && service.IsRetained(),
              "unknown entered allocation/free effects retain the actual runtime owner and original generation");
        const auto mallocCalls = host.mallocCalls, freeCalls = host.freeCalls;
        Check(!service.End() && runtime.timerHooked.load() && runtime.timingHandler == &WorldGroundOwnerFixture::Callback::Invoke &&
                  runtime.timingContext == &callback && NativeServiceAdmission::IsClaimed(NativeServiceAdmission::Category::Clock),
              "End cannot detach original hooks, handler/context or admission while native allocation effects remain unknown");
        fixture.invokeTimer();
        Check(!service.End() && host.mallocCalls == mallocCalls && host.freeCalls == freeCalls && runtime.customSequences.IsRetained(),
              "natural recovery neither retries unknown malloc nor issues a second native free");
        NativeClockService  replacement;
        ClockServiceStateV1 replacementState;
        Check(!replacement.BeginGameTiming({}, replacementState) && runtime.generation.load() == generation && runtime.customSequences.IsRetained() &&
                  service.IsRetained(),
              "outstanding native journal blocks replacement Begin without rotating or erasing the cleanup generation");
        const auto state = State(service);
        Check(state.stopping && !state.restored && state.hooked && state.dataRecoveryPending,
              "actual published state keeps cleanup debt visible to the module lifetime caller");
        std::puts(irreversibleScenario == 1 ? "Production native custom sequence storage unknown allocation lifetime retained"
                                            : "Production native custom sequence storage unknown free lifetime retained");
        std::printf("Passed %u production clock service checks\n", checks.load());
        std::fflush(stdout);
        // The host process ends with the retained owner intact. Destructing or
        // reconstructing it here would hide precisely the debt under test.
        std::_Exit(0);
    }
}
