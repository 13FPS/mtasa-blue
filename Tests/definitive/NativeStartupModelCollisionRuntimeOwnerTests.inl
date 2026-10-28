/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Prove actual callback debt retention without rewinding clock cleanup
 ****************************************************************************/
namespace StartupCollisionRuntimeFixture
{
    namespace Atomic = ClockStartupCollisionAtomicTest;
    namespace P = NativeModelObjectProfile;
    using Writes = WorldGroundOwnerFixture::Writes;
    constexpr std::uint32_t  Target = 17, Source = 18;
    constexpr std::uintptr_t Borrowed = 0xfedcba9876543210;

    void Run(ClockImageFixture& fixture, unsigned scenario)
    {
        Check(scenario < 4, "only four new collision-runtime scenarios are admitted in isolated child processes");
        Writes     writes{fixture, {}};
        const auto admit = [&](const auto& pins)
        {
            for (const auto& pin : pins)
                Writes::Apply(&writes, pin.rva, pin.bytes, pin.size, pin.code ? PAGE_EXECUTE_READ : PAGE_READONLY);
        };
        for (const auto& pin : NativeStreamingLoadAllProfile::Code)
            Writes::Apply(&writes, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        admit(NativeStartupOwnerProfile::pins);
        admit(NativeStartupWorldProfile::pins);
        admit(NativeGameStateProfile::pins);
        for (const auto& pin : NativeUObjectMembershipProfile::pins)
            Writes::Apply(&writes, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : NativeBorrowedGcProfile::pins)
            Writes::Apply(&writes, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : NativePedPoolProfile::Code)
            Writes::Apply(&writes, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : NativeModelBankProfile::Code)
            Writes::Apply(&writes, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : NativeStartGameProfile::Code)
            if (pin.rva == 0xab879c || pin.rva == 0xab92f0 || pin.rva == 0x1af4ef0)
                Writes::Apply(&writes, pin.rva, pin.bytes.data(), pin.size, PAGE_EXECUTE_READ);
        Writes::Apply(&writes, P::CollisionConsumerRva, P::CollisionConsumer.data(), P::CollisionConsumer.size(), PAGE_EXECUTE_READ);
        alignas(std::uintptr_t) std::array<unsigned char, 0x180> root{};
        NativeStreamingLoadAllFixture::Graph                     graph(fixture.image, reinterpret_cast<std::uintptr_t>(root.data()), &Writes::Apply, &writes);
        graph.Reset();
        alignas(std::uintptr_t) std::array<unsigned char, P::CollisionPrefixBytes> target{}, source{};
        std::memcpy(source.data() + P::CollisionOffset, &Borrowed, sizeof(Borrowed));
        writes.Global(NativeModelBankProfile::BankRva + Target * 8, reinterpret_cast<std::uintptr_t>(target.data()));
        writes.Global(NativeModelBankProfile::BankRva + Source * 8, reinterpret_cast<std::uintptr_t>(source.data()));
        writes.Global(NativeGameStateProfile::stateRva, std::uint32_t{9});
        writes.Global(NativeStartGameProfile::ThreadInitializedRva, static_cast<unsigned char>(1));
        writes.Global(NativeStartGameProfile::ThreadIdRva, GetCurrentThreadId());
        writes.Global(NativeBorrowedGcProfile::markerRva, LONG{0});
        const auto previousImage = ClockReaderTestHost::image;
        ClockReaderTestHost::image = fixture.image;
        Atomic::image = fixture.image;
        Atomic::scenario = scenario;
        Atomic::calls = 0;
        WorldGroundOwnerFixture::Callback callback;
        NativeClockService                service;
        ClockResult                       copied = ClockResult::NotInitialized;
        auto                              beforeRestore = ClockTestFaults::clockDataWrites.load();
        callback.action = [&](NativeGameTimingAccess& access)
        {
            Check(runtime.clock.TrySet(18, 20) == ClockResult::Ok && access.SetTimeScale(2.0f) == ClockResult::Ok,
                  "actual callback owns independent clock/timing effects before collision disposition");
            // Stop can restore independent timing in this same natural callback.
            // Measure after our writes, before Copy may trigger that recovery.
            beforeRestore = ClockTestFaults::clockDataWrites.load();
            copied = access.CopyStartupModelCollision(Target, Source);
        };
        BeginOwned(service, fixture, true, true, true, &WorldGroundOwnerFixture::Callback::Invoke, &callback);
        const auto generation = runtime.generation.load();
        fixture.invokeTimer();
        callback.action = [](NativeGameTimingAccess&) {};
        std::uintptr_t current = 0;
        std::memcpy(&current, target.data() + P::CollisionOffset, sizeof(current));
        Check(Atomic::calls == 1 && current == (scenario == 1 ? std::uintptr_t{0} : Borrowed) && !runtime.startupCollisionBusy.load(),
              "actual optional-owner dispatch reaches exactly one guarded CPU CAS without querying the borrowed pointee");
        if (!scenario)
        {
            Check(copied == ClockResult::Ok && runtime.startupCollisionCopies.empty() && !runtime.startupCollisionRetained.load() &&
                      !runtime.startupCollisionGeneration.load(),
                  "a verified borrowed copy discards acknowledgement-only storage without inventing resource ownership");
            WorldGroundOwnerFixture::Retire(service, fixture);
            std::memcpy(&current, target.data() + P::CollisionOffset, sizeof(current));
            Check(current == Borrowed && Atomic::calls == 1 && fixture.Get<std::uint8_t>(hoursRva) == 12 && fixture.Get<std::uint8_t>(minutesRva) == 34 &&
                      fixture.Get<float>(NativeGameTimingProfile::timeScaleRva) == 1.0f,
                  "successful End restores unrelated timing and retains the accepted collision pointer without rollback");
            ClockReaderTestHost::image = previousImage;
            writes.Restore();
            std::puts("Production startup collision runtime verified copy retired without rollback");
            return;
        }
        Check(copied != ClockResult::Ok && runtime.stopping.load() && runtime.startupCollisionCopies.size() == 1 &&
                  runtime.startupCollisionCopies.front()->HasDebt() && runtime.startupCollisionRetained.load() &&
                  runtime.startupCollisionGeneration.load() == generation,
              "actual NativeAccess retains unknown or accepted-but-unverified entered work before Stop can lose its journal");
        if (scenario == 3)
            writes.Global(NativeBorrowedGcProfile::markerRva, LONG{0});
        Check(!service.End() && service.IsRetained(), "producer End cannot acknowledge away an uncertain borrowed collision write");

        fixture.invokeTimer();
        Check(!runtime.restored.load() && !runtime.nativeTouched.load() && !runtime.dataRecovery.load() && !runtime.timingDirty.load() &&
                  !runtime.timingRecovery.load() && !runtime.timingCaptured.load() && !runtime.timingGeneration.load() &&
                  runtime.startupCollisionRetained.load() && fixture.Get<std::uint8_t>(hoursRva) == 12 && fixture.Get<std::uint8_t>(minutesRva) == 34 &&
                  fixture.Get<float>(NativeGameTimingProfile::timeScaleRva) == 1.0f && ClockTestFaults::clockDataWrites.load() > beforeRestore,
              "real clock/timing restoration completes once while overall collision acknowledgement and module retirement remain incomplete");
        fixture.Put(hoursRva, std::uint8_t{7});
        fixture.Put(minutesRva, std::uint8_t{8});
        fixture.Put(secondsRva, std::uint16_t{9});
        fixture.Put(lastChangeRva, std::uint32_t{19900});
        fixture.Put(nativeTimerRva, std::uint32_t{20000});
        const auto afterRestore = ClockTestFaults::clockDataWrites.load();
        fixture.invokeTimer();
        std::memcpy(&current, target.data() + P::CollisionOffset, sizeof(current));
        Check(ClockTestFaults::clockDataWrites.load() == afterRestore && fixture.Get<std::uint8_t>(hoursRva) == 7 &&
                  fixture.Get<std::uint8_t>(minutesRva) == 8 && fixture.Get<std::uint16_t>(secondsRva) == 9 &&
                  fixture.Get<std::uint32_t>(lastChangeRva) == 19900 && Atomic::calls == 1 && current == (scenario == 1 ? std::uintptr_t{0} : Borrowed),
              "later callbacks neither replay the collision copy nor rewind acknowledged clock restoration");
        Check(!service.End() && service.IsRetained() && runtime.startupCollisionCopies.size() == 1 && runtime.startupCollisionCopies.front()->HasDebt() &&
                  NativeLifetimeRetained(),
              "the real End/module lifetime gate retains its exact uncertain CPU journal without deletion or adoption");
        NativeClockService  replacement;
        ClockServiceStateV1 refused;
        Check(!replacement.BeginGameTiming({}, refused) && runtime.generation.load() == generation &&
                  NativeServiceAdmission::IsClaimed(NativeServiceAdmission::Category::Clock),
              "a retained collision acknowledgement blocks replacement Begin and owner-generation rotation");
        const auto state = State(service);
        Check(!state.restored && state.hooked && state.dataRecoveryPending && state.stopping,
              "published real service state exposes pending collision debt after independent restoration succeeded");
        std::puts(scenario == 1   ? "Production startup collision runtime unknown-before retained without replay"
                  : scenario == 2 ? "Production startup collision runtime unknown-after retained without rollback"
                                  : "Production startup collision runtime post-owner-loss retained without rewind");
        std::printf("Passed %u production clock service checks\n", checks.load());
        std::fflush(stdout);
        // A retained process exits normally through the fixture's isolated
        // child boundary. Never erase real runtime state to fake successful End.
        std::_Exit(0);
    }
}
