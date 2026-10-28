/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual native menu callback debt without clock recovery replay
 ****************************************************************************/
namespace ClockStartupMenuInvokeTest
{
    mta::game_de::ClockResult Invoke(Calls::NativeFunction function, std::uintptr_t root, bool& result, std::uint32_t& exception) noexcept
    {
        ++calls;
        Check(menu && root == menu->world.root && function == reinterpret_cast<Calls::NativeFunction>(image + NativeStartupMenuFixture::P::EntryRva),
              "real NativeAccess reaches the exact concrete native bool(this) close with its current root");
        if (scenario == 2)
        {
            exception = 0xe0570241;
            return ClockResult::OriginalException;
        }
        menu->Effect(result);
        if (scenario == 3)
        {
            exception = 0xe0570242;
            return ClockResult::OriginalException;
        }
        if (scenario == 4)
            ::InterlockedExchange(reinterpret_cast<volatile LONG*>(image + mta::game_de::NativeBorrowedGcProfile::markerRva), LONG{1});
        if (scenario < 2)
            menu->RetireWidget();
        return ClockResult::Ok;
    }
}

namespace StartupMenuRuntimeFixture
{
    namespace Host = ClockStartupMenuInvokeTest;
    using Writes = WorldGroundOwnerFixture::Writes;
    void Run(ClockImageFixture& fixture, unsigned scenario)
    {
        Check(scenario < 5, "only five new native menu runtime dispositions run in isolated owned children");
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
        for (const auto& pin : NativeStartGameProfile::Code)
            if (pin.rva == 0xab879c || pin.rva == 0xab92f0 || pin.rva == 0x1af4ef0)
                Writes::Apply(&writes, pin.rva, pin.bytes.data(), pin.size, PAGE_EXECUTE_READ);
        alignas(std::uintptr_t) std::array<unsigned char, 0x360> root{};
        NativeStreamingLoadAllFixture::Graph                     world(fixture.image, reinterpret_cast<std::uintptr_t>(root.data()), &Writes::Apply, &writes);
        NativeStartupMenuFixture::Graph                          menu(world);
        menu.InitializePins();
        menu.Reset(scenario != 1);
        writes.Global(NativeGameStateProfile::stateRva, std::uint32_t{9});
        writes.Global(NativeStartGameProfile::ThreadInitializedRva, static_cast<unsigned char>(1));
        writes.Global(NativeStartGameProfile::ThreadIdRva, GetCurrentThreadId());
        writes.Global(NativeBorrowedGcProfile::markerRva, LONG{0});
        const auto previousImage = ClockReaderTestHost::image;
        ClockReaderTestHost::image = fixture.image;
        Host::image = fixture.image;
        Host::scenario = scenario;
        Host::calls = 0;
        Host::menu = &menu;
        WorldGroundOwnerFixture::Callback callback;
        NativeClockService                service;
        ClockResult                       closed = ClockResult::NotInitialized;
        auto                              beforeRestore = ClockTestFaults::clockDataWrites.load();
        callback.action = [&](NativeGameTimingAccess& access)
        {
            Check(access.RequireStartupNativeMenuReady() == ClockResult::Ok && !Host::calls,
                  "actual optional menu readiness qualifies the current borrowed frame without entering engine effects");
            Check(runtime.clock.TrySet(18, 20) == ClockResult::Ok && access.SetTimeScale(2.0f) == ClockResult::Ok,
                  "actual callback owns independent clock/timing mutations before menu disposition");
            beforeRestore = ClockTestFaults::clockDataWrites.load();
            closed = access.HideStartupNativeMenu();
        };
        BeginOwned(service, fixture, true, true, true, &WorldGroundOwnerFixture::Callback::Invoke, &callback);
        const auto generation = runtime.generation.load();
        fixture.invokeTimer();
        callback.action = [](NativeGameTimingAccess&) {};
        Check(Host::calls == 1 && menu.Root<std::uintptr_t>(0x348) == (scenario == 2 ? menu.Widget() : std::uintptr_t{0}) && !runtime.startupMenuBusy.load(),
              "actual optional-owner dispatch reaches exactly one typed native close with the expected surviving effect prefix");
        if (scenario < 2)
        {
            Check(closed == ClockResult::Ok && runtime.startupMenuCloses.empty() && !runtime.startupMenuRetained.load() &&
                      !runtime.startupMenuGeneration.load() && menu.Root<std::uint32_t>(0x350) == (scenario == 1 ? 79u : 0u),
                  "native widget removal and genuine false/no-widget results both retire acknowledgement-only records");
            WorldGroundOwnerFixture::Retire(service, fixture);
            Check(!menu.Root<std::uintptr_t>(0x348) && Host::calls == 1 && fixture.Get<std::uint8_t>(hoursRva) == 12 &&
                      fixture.Get<std::uint8_t>(minutesRva) == 34 && fixture.Get<float>(NativeGameTimingProfile::timeScaleRva) == 1.0f,
                  "successful real End restores unrelated timing without reopening the accepted native menu");
            Host::menu = nullptr;
            ClockReaderTestHost::image = previousImage;
            writes.Restore();
            std::puts(scenario ? "Production startup menu runtime false no-widget retired" : "Production startup menu runtime verified close retired");
            return;
        }
        Check(closed != ClockResult::Ok && runtime.stopping.load() && runtime.startupMenuCloses.size() == 1 && runtime.startupMenuCloses.front()->HasDebt() &&
                  runtime.startupMenuRetained.load() && runtime.startupMenuGeneration.load() == generation,
              "actual runtime journals unknown-before/after or post-owner-loss native effects before stopping the same owner");
        if (scenario == 4)
            writes.Global(NativeBorrowedGcProfile::markerRva, LONG{0});
        Check(!service.End() && service.IsRetained(), "producer End cannot erase an uncertain native menu close acknowledgement");
        fixture.invokeTimer();
        Check(!runtime.restored.load() && !runtime.nativeTouched.load() && !runtime.dataRecovery.load() && !runtime.timingDirty.load() &&
                  !runtime.timingRecovery.load() && !runtime.timingCaptured.load() && !runtime.timingGeneration.load() && runtime.startupMenuRetained.load() &&
                  fixture.Get<std::uint8_t>(hoursRva) == 12 && fixture.Get<std::uint8_t>(minutesRva) == 34 &&
                  fixture.Get<float>(NativeGameTimingProfile::timeScaleRva) == 1.0f && ClockTestFaults::clockDataWrites.load() > beforeRestore,
              "independent clock/timing restoration completes once while menu acknowledgement and module retirement stay pending");
        fixture.Put(hoursRva, std::uint8_t{7});
        fixture.Put(minutesRva, std::uint8_t{8});
        fixture.Put(secondsRva, std::uint16_t{9});
        fixture.Put(lastChangeRva, std::uint32_t{19900});
        fixture.Put(nativeTimerRva, std::uint32_t{20000});
        const auto afterRestore = ClockTestFaults::clockDataWrites.load();
        fixture.invokeTimer();
        Check(ClockTestFaults::clockDataWrites.load() == afterRestore && fixture.Get<std::uint8_t>(hoursRva) == 7 &&
                  fixture.Get<std::uint8_t>(minutesRva) == 8 && fixture.Get<std::uint16_t>(secondsRva) == 9 &&
                  fixture.Get<std::uint32_t>(lastChangeRva) == 19900 && Host::calls == 1 &&
                  menu.Root<std::uintptr_t>(0x348) == (scenario == 2 ? menu.Widget() : std::uintptr_t{0}),
              "later callbacks neither replay menu input/audio effects nor rewind already acknowledged clock cleanup");
        Check(!service.End() && service.IsRetained() && runtime.startupMenuCloses.size() == 1 && runtime.startupMenuCloses.front()->HasDebt() &&
                  NativeLifetimeRetained(),
              "actual End and module lifetime gates retain the exact unresolved journal without adoption or rollback");
        NativeClockService  replacement;
        ClockServiceStateV1 refused;
        Check(!replacement.BeginGameTiming({}, refused) && runtime.generation.load() == generation &&
                  NativeServiceAdmission::IsClaimed(NativeServiceAdmission::Category::Clock),
              "retained native menu debt blocks replacement Begin and owner-generation rotation");
        const auto state = State(service);
        Check(!state.restored && state.hooked && state.dataRecoveryPending && state.stopping,
              "published actual service state exposes menu debt after independent restoration succeeded");
        std::puts(scenario == 2   ? "Production startup menu runtime unknown-before retained without replay"
                  : scenario == 3 ? "Production startup menu runtime unknown-after retained without rollback"
                                  : "Production startup menu runtime post-owner-loss retained without rewind");
        std::printf("Passed %u production clock service checks\n", checks.load());
        std::fflush(stdout);
        // Preserve the real retained journal for this isolated child lifetime;
        // resetting runtime state would falsely certify successful End.
        std::_Exit(0);
    }
}
