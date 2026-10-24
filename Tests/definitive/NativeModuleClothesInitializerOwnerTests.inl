/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Module clothes storage through the existing admitted callback owner
 ****************************************************************************/
namespace ModuleClothesInitializerOwnerFixture
{
    void Run(ClockImageFixture& fixture)
    {
        namespace Host = ClockModuleClothesInitializerTest;
        namespace P = NativeClothesDescriptorProfile;
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
        // Reuse the qualified shared heap/name state. This initializer owns no
        // ped, player-data slot or local-player fallback to set up or retire.
        NativePedClothesFixture::Install(fixture.image, &WorldGroundOwnerFixture::Writes::Apply, &writes);
        NativeClothesDescriptorFixture::Install(fixture.image, &WorldGroundOwnerFixture::Writes::Apply, &writes);
        NativePedClothesFixture::NameState names;
        names.Reset(fixture.image, &WorldGroundOwnerFixture::Writes::Apply, &writes);
        const auto resetContext = [&]
        {
            graph.Reset();
            writes.Global(NativeGameStateProfile::stateRva, std::uint32_t{9});
            writes.Global(NativeStartGameProfile::ThreadInitializedRva, static_cast<unsigned char>(1));
            writes.Global(NativeStartGameProfile::ThreadIdRva, GetCurrentThreadId());
            writes.Global(NativeBorrowedGcProfile::markerRva, LONG{0});
        };
        resetContext();
        const auto priorImage = ClockReaderTestHost::image;
        ClockReaderTestHost::image = fixture.image;
        Host::image = fixture.image;
        Host::calls = Host::effects = Host::scenario = 0;
        Host::nested = nullptr;
        Host::reenter = +[](NativeClothesDescriptorStorage& value) noexcept { return runtime.access.InitializePedClothesDescriptor(value); };
        Host::after = nullptr;
        Host::opaque = nullptr;
        struct CacheEffect
        {
            NativePedClothesFixture::NameState* names;
            WorldGroundOwnerFixture::Writes*    writes;
            std::uintptr_t                      image;
        } cache{&names, &writes, fixture.image};
        const auto cacheEffect = +[](void* opaque) noexcept
        {
            auto& current = *static_cast<CacheEffect*>(opaque);
            current.names->Cache(current.image, &WorldGroundOwnerFixture::Writes::Apply, current.writes, true);
        };
        struct DefaultTimingAccess final : NativeGameTimingAccess
        {
            ClockResult SetTimeScale(float) noexcept override { return ClockResult::NotInitialized; }
            ClockResult SetMinuteDuration(std::uint32_t) noexcept override { return ClockResult::NotInitialized; }
            ClockResult Pause(bool) noexcept override { return ClockResult::NotInitialized; }
        } absent;
        NativeClothesDescriptorStorage defaultStorage;
        Check(absent.InitializePedClothesDescriptor(defaultStorage) == ClockResult::NotInitialized && !defaultStorage.IsInitialized() && !Host::calls,
              "older timing owner cannot invent initialization or expose module storage as a native lease");
        WorldGroundOwnerFixture::Callback callback;
        ClockResult                       result = ClockResult::NotInitialized;
        {
            NativeClockService             service;
            NativeClothesDescriptorStorage storage, nestedStorage;
            callback.action = [](NativeGameTimingAccess&) {};
            BeginOwned(service, fixture, false, true, true, &WorldGroundOwnerFixture::Callback::Invoke, &callback);
            fixture.invokeTimer();
            Check(runtime.access.InitializePedClothesDescriptor(storage) != ClockResult::Ok && !storage.IsInitialized() && !Host::calls,
                  "CPU ownership alone grants no initializer authority between native callbacks");
            std::thread foreign([&] { result = runtime.access.InitializePedClothesDescriptor(storage); });
            foreign.join();
            Check(result != ClockResult::Ok && !storage.IsInitialized() && !Host::calls && !runtime.stopping.load(),
                  "foreign thread refuses before the exact initializer and does not stop the valid owner");
            callback.action = [&](NativeGameTimingAccess& access) { result = access.InitializePedClothesDescriptor(storage); };
            Check(NativeStreamingCompletionContext::TryEnter(), "hold the existing shared native name/completion gate");
            Check(service.Renew(runtime.generation.load()), "renew current owner for a pre-entry shared-gate refusal");
            fixture.invokeTimer();
            NativeStreamingCompletionContext::Leave();
            Check(result == ClockResult::Busy && !storage.IsInitialized() && !Host::calls && !runtime.stopping.load(),
                  "initializer respects shared native exclusion without consuming fresh module storage");
            writes.Global(NativeGameStateProfile::stateRva, std::uint32_t{7});
            Check(service.Renew(runtime.generation.load()), "renew native frontend refusal without entering the initializer");
            fixture.invokeTimer();
            Check(result != ClockResult::Ok && !storage.IsInitialized() && !Host::calls && !runtime.stopping.load(),
                  "known frontend state refuses before native work without turning CPU ownership into engine admission");
            writes.Global(NativeGameStateProfile::stateRva, std::uint32_t{9});
            Host::nested = &nestedStorage;
            Host::after = cacheEffect;
            Host::opaque = &cache;
            Check(service.Renew(runtime.generation.load()), "renew one genuine initializer attempt on caller-owned storage");
            fixture.invokeTimer();
            Check(result == ClockResult::Ok && storage.IsInitialized() && Host::calls == 1 && Host::effects == 1 &&
                      Host::entry == fixture.image + P::InitializeRva && Host::descriptor && Host::nestedResult == ClockResult::Busy &&
                      !nestedStorage.IsInitialized() && !runtime.stopping.load() && !runtime.pedReferences.IsRetained() && !runtime.playerInfo.IsBound(),
                  "same owner calls the exact initializer once, accepts native cache growth and exposes no ambient player or native allocation debt");
            Host::nested = nullptr;
            Host::after = nullptr;
            Host::opaque = nullptr;
            Check(service.Renew(runtime.generation.load()), "renew the initialized-storage one-shot refusal");
            fixture.invokeTimer();
            Check(result == ClockResult::Busy && storage.IsInitialized() && Host::calls == 1 && !runtime.stopping.load(),
                  "initialized CPU storage cannot be initialized again or trigger another name-cache operation");
            WorldGroundOwnerFixture::Retire(service, fixture);
            Check(storage.IsInitialized() && !runtime.pedReferences.IsRetained(),
                  "ordinary owner retirement does not destroy caller CPU storage or invent native descriptor release debt");
        }
        // An entered failure stops the existing owner exactly once. Keep the
        // initializer's cache effects; only owned fixture context is restored
        // for normal End, with no native retry or descriptor rollback.
        for (unsigned scenario = 1; scenario != 3; ++scenario)
        {
            resetContext();
            Host::scenario = scenario;
            Host::after = cacheEffect;
            Host::opaque = &cache;
            const auto                     before = Host::calls;
            NativeClockService             service;
            NativeClothesDescriptorStorage storage;
            callback.action = [&](NativeGameTimingAccess& access) { result = access.InitializePedClothesDescriptor(storage); };
            BeginOwned(service, fixture, false, true, true, &WorldGroundOwnerFixture::Callback::Invoke, &callback);
            fixture.invokeTimer();
            Check(result != ClockResult::Ok && !storage.IsInitialized() && Host::calls == before + 1 && runtime.stopping.load() &&
                      !runtime.pedReferences.IsRetained() && !runtime.playerInfo.IsBound(),
                  "entered native exception or lost postflight scope stops the same owner without publishing initialized storage");
            if (scenario == 1)
                Check(result == ClockResult::OriginalException, "entered native fault preserves its real exception result");
            const auto entered = Host::calls;
            Check(runtime.access.InitializePedClothesDescriptor(storage) != ClockResult::Ok && Host::calls == entered,
                  "stopped owner never retries a failed descriptor or repeats shared native side effects");
            Host::after = nullptr;
            Host::opaque = nullptr;
            writes.Global(NativeBorrowedGcProfile::markerRva, LONG{0});
            WorldGroundOwnerFixture::Retire(service, fixture);
            Check(!storage.IsInitialized() && !runtime.pedReferences.IsRetained(),
                  "failed unattached module storage needs no native destructor, registry or retained descriptor debt after End");
        }
        Host::image = Host::entry = Host::descriptor = 0;
        Host::scenario = 0;
        Host::nested = nullptr;
        Host::reenter = nullptr;
        ClockReaderTestHost::image = priorImage;
        writes.Restore();
        std::puts("Production native module-owned clothes initializer callback owner fixture completed");
    }
}
