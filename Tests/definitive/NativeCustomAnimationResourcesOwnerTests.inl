/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual callback service owns storage through natural End recovery
 ****************************************************************************/
namespace CustomAnimationResourcesOwnerFixture
{
    namespace Host = NativeCustomAnimSequenceStorageFixture;
    using Service = NativeAnimationSequenceStorageAccess;
    constexpr std::uintptr_t Sentinel = 0x12345678;
    struct Effect
    {
        WorldGroundOwnerFixture::Writes* writes = nullptr;
        std::uint64_t*                   callerGeneration = nullptr;
        std::uint32_t*                   callerBytes = nullptr;
        bool                             loseScope = false, retainedAtEntry = false;
    };
    void AfterMalloc(Host::State& host) noexcept
    {
        auto& effect = *static_cast<Effect*>(host.context);
        effect.retainedAtEntry = runtime.customSequences.IsRetained() && runtime.customSequenceGeneration.load() == runtime.generation.load();
        if (effect.callerGeneration)
            ++*effect.callerGeneration;
        if (effect.callerBytes)
            *effect.callerBytes = 65535;
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
            Check(captured->AllocateAnimationKeyFrames(generation, 41, output) != ClockResult::Ok && output == Sentinel && !host.mallocCalls,
                  "optional frame provider refuses a producer without native callback authority");
            std::thread foreign([&] { result = captured->AllocateAnimationKeyFrames(generation, 41, output); });
            foreign.join();
            Check(result != ClockResult::Ok && output == Sentinel && !host.mallocCalls && !runtime.stopping.load(),
                  "foreign frame request cannot enter native allocation or stop the valid owner");
            callback.action = [&](NativeGameTimingAccess&) { result = captured->AllocateAnimationKeyFrames(generation + 1, 41, output); };
            invoke();
            Check(result == ClockResult::OwnerConflict && output == Sentinel && !host.mallocCalls && !runtime.stopping.load(),
                  "frame request keeps the current runtime generation before native entry");
            host.nullMalloc = true;
            callback.action = [&](NativeGameTimingAccess&) { result = captured->AllocateAnimationKeyFrames(generation, 41, output); };
            invoke();
            Check(result == ClockResult::Ok && !output && !runtime.stopping.load() && !runtime.customSequences.IsRetained(),
                  "qualified frame OOM remains a nullable successful MTA allocation with no cleanup resource");
            host.nullMalloc = false;
            std::uint64_t callerGeneration = generation;
            std::uint32_t callerBytes = 41;
            effect.callerGeneration = &callerGeneration;
            effect.callerBytes = &callerBytes;
            host.afterMalloc = &AfterMalloc;
            callback.action = [&](NativeGameTimingAccess&) { result = captured->AllocateAnimationKeyFrames(callerGeneration, callerBytes, output); };
            invoke();
            Check(result == ClockResult::Ok && output == host.base && host.bytes == 41 && callerGeneration == generation + 1 && callerBytes == 65535 &&
                      effect.retainedAtEntry && runtime.customSequences.IsRetained(),
                  "actual frame service copies generation and byte size and journals raw base before pointer publication");
            host.afterMalloc = nullptr;
            effect.callerGeneration = nullptr;
            effect.callerBytes = nullptr;
            const auto frames = output;
            auto*      payload = reinterpret_cast<unsigned char*>(frames);
            bool       preserved = true;
            for (std::size_t index = 0; index != 41; ++index)
                preserved = preserved && payload[index] == 0xa5;
            std::memset(payload, 0x63, 41);
            payload[0] = 0x11;
            payload[40] = 0x7e;
            Check(preserved && payload[0] == 0x11 && payload[40] == 0x7e,
                  "frame payload is caller-owned through its first and final bytes with no array cookie initialization");
            callback.action = [&](NativeGameTimingAccess&) { result = captured->AllocateAnimationSequences(generation, 2, output); };
            invoke();
            const auto array = output;
            Check(result == ClockResult::Ok && array == host.base + 8 && array != frames && runtime.customSequenceGeneration.load() == generation,
                  "one runtime journal owns raw frames and a separately initialized sequence array together");
            AnimationSequenceStorageManagerReceiver sdk(runtime.access, *captured, generation);
            bool                                    sdkRouted = false;
            callback.action = [&](NativeGameTimingAccess&)
            {
                auto* sdkFrames = sdk.AllocateKeyFramesMemory(17);
                auto* sdkArray = sdk.AllocateCustomAnimSequences(1);
                Check(sdkFrames && sdkArray, "actual SDK storage methods issue both kinds through their same native owner");
                sdkFrames[0] = 0x22;
                sdkFrames[16] = 0x7c;
                sdk.FreeCustomAnimSequences(sdkArray);
                sdk.FreeKeyFramesMemory(sdkFrames);
                sdkRouted = true;
            };
            invoke();
            Check(sdkRouted && !runtime.stopping.load() && runtime.customSequences.IsRetained(),
                  "all four actual CAnimManagerDE SDK storage overrides dispatch to the callback's shared controller without a complete CGame");
            ClockResult wrongArray = ClockResult::Ok, wrongFrames = ClockResult::Ok;
            callback.action = [&](NativeGameTimingAccess&)
            {
                wrongArray = captured->FreeAnimationSequences(generation, frames);
                wrongFrames = captured->FreeAnimationKeyFrames(generation, array);
            };
            const auto mixedFreeCalls = host.freeCalls;
            invoke();
            Check(wrongArray == ClockResult::BindingChanged && wrongFrames == ClockResult::BindingChanged && host.freeCalls == mixedFreeCalls &&
                      !runtime.stopping.load() && runtime.customSequences.IsRetained(),
                  "wrong free APIs refuse before entry without consuming either kind or stopping the actual owner");
            Check(!service.End() && service.IsRetained() && runtime.timerHooked.load() && host.freeCalls == mixedFreeCalls,
                  "producer End retains one callback owner with both resource kinds and performs no producer-side native free");
            fixture.invokeTimer();
            Check(host.freeCalls == mixedFreeCalls + 2 && !runtime.customSequences.IsRetained() && !runtime.customSequenceGeneration.load(),
                  "natural End cleanup dispatches both frame and array frees, including changed frame payload first bytes");
            Check(service.End() && !service.IsRetained() && !runtime.timerHooked.load() &&
                      !NativeServiceAdmission::IsClaimed(NativeServiceAdmission::Category::Clock),
                  "completed mixed cleanup permits hook and admission retirement without a second manager");

            resetContext();
            callback.action = [&](NativeGameTimingAccess& access)
            {
                captured = dynamic_cast<Service*>(&access);
                result = captured->AllocateAnimationKeyFrames(runtime.generation.load(), 41, output);
            };
            effect.loseScope = true;
            host.afterMalloc = &AfterMalloc;
            output = Sentinel;
            BeginOwned(service, fixture, false, true, true, &WorldGroundOwnerFixture::Callback::Invoke, &callback);
            fixture.invokeTimer();
            Check(result != ClockResult::Ok && output == Sentinel && runtime.stopping.load() && runtime.customSequences.IsRetained() &&
                      runtime.customSequences.RecoveryPending(),
                  "known returned frame allocation with lost acknowledgement retains its private original base and stops the runtime");
            const auto failedFreeCalls = host.freeCalls;
            Check(!service.End() && service.IsRetained() && host.freeCalls == failedFreeCalls,
                  "native GC refusal retains known frame debt across producer End");
            resetContext();
            effect.loseScope = false;
            host.afterMalloc = nullptr;
            fixture.invokeTimer();
            Check(host.freeCalls == failedFreeCalls + 1 && !runtime.customSequences.IsRetained() && service.End() && !service.IsRetained(),
                  "the same natural cleanup generation recovers only its known unpublished frame allocation");

            resetContext();
            callback.action = [&](NativeGameTimingAccess& access)
            {
                captured = dynamic_cast<Service*>(&access);
                result = captured->AllocateAnimationKeyFrames(runtime.generation.load(), 41, output);
            };
            BeginOwned(service, fixture, false, true, true, &WorldGroundOwnerFixture::Callback::Invoke, &callback);
            fixture.invokeTimer();
            Check(result == ClockResult::Ok, "prepare raw frame free acknowledgement loss through the actual runtime");
            const auto freeGeneration = runtime.generation.load();
            const auto returnedFreeCalls = host.freeCalls;
            host.afterFree = &AfterFree;
            callback.action = [&](NativeGameTimingAccess&) { result = captured->FreeAnimationKeyFrames(freeGeneration, output); };
            Check(service.Renew(freeGeneration), "renew the one actual frame-free attempt");
            fixture.invokeTimer();
            Check(result != ClockResult::Ok && runtime.stopping.load() && host.freeCalls == returnedFreeCalls + 1 && !runtime.customSequences.IsRetained() &&
                      !runtime.customSequenceGeneration.load(),
                  "known returned frame free is consumed before failed acknowledgement without replayable journal debt");
            resetContext();
            host.afterFree = nullptr;
            WorldGroundOwnerFixture::Retire(service, fixture);
            Check(host.freeCalls == returnedFreeCalls + 1, "natural End never repeats a consumed frame free after scope recovery");
            Host::active = nullptr;
            ClockReaderTestHost::image = previousImage;
            writes.Restore();
            std::puts("Production mixed custom animation resources callback owner fixture completed");
            std::puts("Production mixed custom animation resources natural End fixture completed");
            std::puts("Production actual custom animation storage SDK callback fixture completed");
            return;
        }

        // Each unknown-effect scenario runs in its own ordinary test process.
        // Never reset/discard the real global owner to make another case fit.
        if (irreversibleScenario == 1)
            host.mallocResult = ClockResult::OriginalException;
        callback.action = [&](NativeGameTimingAccess&) { result = captured->AllocateAnimationKeyFrames(generation, 41, output); };
        invoke();
        if (irreversibleScenario == 2)
        {
            Check(result == ClockResult::Ok, "prepare actual issued storage before an unknown entered-free effect");
            host.freeResult = ClockResult::OriginalException;
            host.consumeFailedFree = true;
            callback.action = [&](NativeGameTimingAccess&) { result = captured->FreeAnimationKeyFrames(generation, output); };
            invoke();
        }
        Check(result == ClockResult::OriginalException && runtime.stopping.load() && runtime.customSequences.IsRetained() &&
                  runtime.customSequences.RecoveryPending() && runtime.customSequenceGeneration.load() == generation && service.IsRetained(),
              "unknown entered frame allocation/free effects retain the actual runtime owner and original generation");
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
        std::puts(irreversibleScenario == 1 ? "Production mixed custom animation resources unknown frame allocation lifetime retained"
                                            : "Production mixed custom animation resources unknown frame free lifetime retained");
        std::printf("Passed %u production clock service checks\n", checks.load());
        std::fflush(stdout);
        // The host process ends with the retained owner intact. Destructing or
        // reconstructing it here would hide precisely the debt under test.
        std::_Exit(0);
    }
}
