/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Admit one real complete constructor-service bundle, without effects
 *****************************************************************************/
namespace ConstructorServicesFixture
{
    using Game = constructor_services_fixture::GameConstructorFixtureBase;

    class Legacy final : public NativeGameTimingAccess
    {
    public:
        ClockResult SetTimeScale(float) noexcept override { return ClockResult::NotInitialized; }
        ClockResult SetMinuteDuration(std::uint32_t) noexcept override { return ClockResult::NotInitialized; }
        ClockResult Pause(bool) noexcept override { return ClockResult::NotInitialized; }
    };

    struct Context
    {
        NativeGameConstructorServices bundle;
        NativeGameTimingReader        timing;
        NativeStreamingFileIdsReader  streaming;
        NativeGameStateReader         state;
        std::unique_ptr<Game>         game;
        ClockResult                   result = ClockResult::NotInitialized;
        unsigned                      operation = 0;
        bool                          threw = false;
    };

    template <class Operation>
    bool Refuses(Operation operation)
    {
        try
        {
            operation();
        }
        catch (const std::runtime_error&)
        {
            return true;
        }
        return false;
    }

    void Handler(NativeGameTimingAccess& access, void* opaque)
    {
        auto& context = *static_cast<Context*>(opaque);
        context.threw = false;
        if (context.operation == 0)
            context.result = access.CaptureConstructorServices(&runtime.clock, context.bundle);
        else if (context.operation == 1)
        {
            context.game = std::make_unique<Game>(context.timing, runtime.clock, access, context.streaming, context.state, context.bundle);
            CGame& game = *context.game;
            Check(game.GetClock() == &runtime.clock && game.GetStats() == &runtime.stats,
                  "actual prepared CGame consumes the captured clock and freshly qualified selected stats service");
            DWORD previous = 0, ignored = 0;
            void* lastRow = reinterpret_cast<void*>(runtime.image + NativeWeaponInfoProfile::BankRva +
                                                    (NativeWeaponInfoProfile::RowCount - 1) * NativeWeaponInfoProfile::RowBytes);
            Check(::VirtualProtect(lastRow, NativeWeaponInfoProfile::RowBytes, PAGE_NOACCESS, &previous) != FALSE,
                  "owned fixture temporarily denies only an unrelated final weapon-row page");
            Check(game.GetStats() == &runtime.stats,
                  "prepared selected getter validates CPU identities and its own service without rereading the whole weapon bank");
            Check(::VirtualProtect(lastRow, NativeWeaponInfoProfile::RowBytes, previous, &ignored) != FALSE,
                  "restore unrelated owned weapon-row pages before another whole-bundle capture");
        }
        else if (context.operation == 2)
        {
            const auto supplied = context.bundle;
            auto       altered = supplied;
            ++altered.generation;
            Check(Refuses([&] { Game game(context.timing, runtime.clock, access, context.streaming, context.state, altered); }),
                  "supplied stale generation refuses even while every SDK pointer is the current runtime address");
            altered = supplied;
            altered.weapons[22] = supplied.weapons[23];
            Check(Refuses([&] { Game game(context.timing, runtime.clock, access, context.streaming, context.state, altered); }),
                  "a complete-looking tampered bank identity cannot construct a prepared game");
            altered = supplied;
            altered.stats = nullptr;
            Check(Refuses([&] { Game game(context.timing, runtime.clock, access, context.streaming, context.state, altered); }),
                  "a partial caller bundle cannot substitute for an actual full-service capture");
            Check(access.ValidateConstructorServices(supplied) == ClockResult::Ok, "readonly rejected bundles leave the original admitted bundle usable");
            Check(access.ValidateConstructorServices(altered) == ClockResult::OwnerConflict,
                  "runtime validation refuses partial producer-made values without overwriting them");
            auto prior = context.bundle;
            context.result = access.CaptureConstructorServices(nullptr, context.bundle);
            Check(context.result == ClockResult::InvalidArgument && context.bundle.Same(prior), "foreign clock refuses before any output publication");
            const auto callback = timingCallbackGeneration;
            ++timingCallbackGeneration;
            context.result = access.CaptureConstructorServices(&runtime.clock, context.bundle);
            timingCallbackGeneration = callback;
            Check(context.result == ClockResult::OwnerConflict && context.bundle.Same(prior) && !runtime.stopping.load(),
                  "foreign callback generation preserves output and does not stop the unrelated admitted runtime");
        }
        else if (context.operation == 3)
        {
            context.threw = Refuses([&] { context.game->GetStats(); });
            Check(context.threw && !runtime.stopping.load(), "an old prepared game cannot reselect same-address services in a later admitted generation");
            NativeGameConstructorServices current;
            Check(access.CaptureConstructorServices(&runtime.clock, current) == ClockResult::Ok && current.generation != context.bundle.generation &&
                      current.stats == context.bundle.stats && current.clock == context.bundle.clock,
                  "a new admitted generation can separately capture the reused runtime addresses");
            Check(Refuses([&] { Game stale(context.timing, runtime.clock, access, context.streaming, context.state, context.bundle); }),
                  "stale caller bundle cannot construct a new prepared game after End and Begin");
            context.bundle = current;
            // The deliberately stale negative-test object owns no native cleanup.
            // Typed destruction occurs here before the new owner is retired.
            context.game.reset();
            context.game = std::make_unique<Game>(context.timing, runtime.clock, access, context.streaming, context.state, current);
            Check(context.game->GetStats() == current.stats, "a separately prepared new game consumes only the new captured generation");
        }
        else
            std::abort();
    }

    void Prepare(NativeGameTimingAccess& access, const NativeApplicationFrame&, void* opaque)
    {
        auto& progress = *static_cast<NativeWeaponConstructorProgress*>(opaque);
        Check(access.PrepareConstructorWeapons(progress) == ClockResult::Ok,
              "new bundle fixture advances its genuine constructor prerequisite once per real normal frame");
        if (progress.completedRows == 1)
        {
            NativeGameConstructorServices untouched;
            untouched.generation = 99;
            Check(access.CaptureConstructorServices(&runtime.clock, untouched) != ClockResult::Ok && untouched.generation == 99,
                  "an admitted early constructor frame is not a loaded timing-owner service bundle");
        }
    }

    void SeedServices(ClockImageFixture& fixture)
    {
        const auto admit = [&](std::uintptr_t rva, const void* bytes, std::size_t size, DWORD protection)
        {
            fixture.Patch(rva, bytes, size);
            DWORD ignored = 0;
            Check(::VirtualProtect(reinterpret_cast<void*>(fixture.image + rva), size, protection, &ignored) != FALSE,
                  "constructor bundle fixture owns only the exact service code and data pages");
        };
        const auto writable = [&](std::uintptr_t rva, std::size_t bytes)
        {
            DWORD ignored = 0;
            Check(::VirtualProtect(reinterpret_cast<void*>(fixture.image + rva), bytes, PAGE_READWRITE, &ignored) != FALSE,
                  "owned constructor service data remains genuine non-executable writable storage");
        };
        fixture.AdmitSystemStatePins();
        fixture.AdmitStatsPins();
        fixture.AdmitWeatherPins();
        fixture.AdmitWeatherScalarPins();
        fixture.AdmitRainPins();
        for (const auto& pin : NativePlayerInfoProfile::Code)
            admit(pin.rva, pin.bytes.data(), pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : NativePlayerInfoExtraProfile::Code)
            admit(pin.rva, pin.bytes.data(), pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : NativeWantedProfile::Code)
            admit(pin.rva, pin.bytes.data(), pin.size, PAGE_EXECUTE_READ);
        namespace P = NativePlayerInfoProfile;
        namespace W = NativeWantedProfile;
        writable(P::BankRva, P::RowBytes);
        writable(P::PlayerIndexRva, 1);
        writable(W::MaximumLevelRva, 8);
        const auto store = [](auto& bytes, std::size_t offset, const auto& value) { std::memcpy(bytes.data() + offset, &value, sizeof(value)); };
        fixture.statsPed.fill(0);
        fixture.statsData.fill(0);
        store(fixture.statsPed, 0, fixture.image + P::PlayerVtableRva);
        store(fixture.statsPed, 0x5f0, fixture.image + P::BankRva + 8);
        store(fixture.statsPed, 0x7e0, std::int32_t{0});
        fixture.Put(P::BankRva, reinterpret_cast<std::uintptr_t>(fixture.statsPed.data()));
        fixture.Put(W::WantedRootRva, reinterpret_cast<std::uintptr_t>(fixture.statsData.data()));
        fixture.Put(P::PlayerIndexRva, std::uint8_t{0});
        fixture.Put(W::MaximumLevelRva, std::uint32_t{6});
        fixture.Put(W::MaximumChaosRva, std::int32_t{6900});
        const auto control = fixture.image + P::PlayerControlRva;
        admit(P::PlayerVtableRva + 0x50, &control, sizeof(control), PAGE_READONLY);
        namespace R = NativeRopesProfile;
        for (const auto& pin : R::Code)
            admit(pin.rva, pin.bytes.data(), pin.size, PAGE_EXECUTE_READ);
        const auto destructor = fixture.image + R::HookDestructorRva;
        const auto remove = fixture.image + R::HookRemoveRva;
        admit(R::HookVtableRva, &destructor, sizeof(destructor), PAGE_READONLY);
        admit(R::HookVtableRva + 0x20, &remove, sizeof(remove), PAGE_READONLY);
        const std::array<unsigned char, R::RowCount * R::RowBytes> emptyRows{};
        std::vector<unsigned char>                                 emptyReferences(R::ReferenceCount * R::ReferenceBytes);
        admit(R::TableRva, emptyRows.data(), emptyRows.size(), PAGE_READWRITE);
        admit(R::ReferencesRva, emptyReferences.data(), emptyReferences.size(), PAGE_READWRITE);
        const std::uint32_t zero = 0;
        admit(R::CounterRva, &zero, sizeof(zero), PAGE_READWRITE);
        writable(NativeGameStateProfile::stateRva, 4);
        fixture.Put(NativeGameStateProfile::stateRva, std::uint32_t{9});
        ClockReaderTestHost::image = fixture.image;
    }

    void Retire(NativeClockService& service, ClockImageFixture& fixture)
    {
        // This timing-only scope never captured CClock or entered a write.
        // End can therefore detach immediately; the common clock-mutation
        // helper intentionally requires asynchronous debt that does not exist
        // here. If retirement is retained, only its actual timer may drain it.
        Check(runtime.timingMode && !runtime.captured.load() && !runtime.nativeTouched.load() && !runtime.timingDirty.load() &&
                  !runtime.timingRecovery.load() && !runtime.weaponVectorRecovery.load() && !runtime.weatherRecovery.load(),
              "readonly constructor bundle has no captured clock or native mutation to restore");
        if (!service.End())
        {
            fixture.invokeTimer();
            Check(service.End(), "only the retained timing callback may acknowledge any pending readonly owner retirement");
        }
        Check(!service.IsRetained() && !runtime.bound.load() && !runtime.ready.load() && !runtime.timerReady.load() && !runtime.hooked.load() &&
                  !runtime.timerHooked.load() && !runtime.frameHooked.load() && !runtime.captured.load() && !runtime.timingHandler && !runtime.timingContext &&
                  !runtime.weaponWriterReader.IsBound() && !runtime.weaponWriterGeneration && !runtime.statsReader.IsBound() && !runtime.statsGeneration &&
                  !runtime.playerInfo.IsBound() && !runtime.playerInfoGeneration && !runtime.ropesMemory.IsBound() && !runtime.ropesGeneration &&
                  !runtime.weatherOverrideGeneration.load() && !runtime.inFlight.load() && !runtime.admissionSerial.load() &&
                  !runtime.admissionGeneration.load() && !NativeServiceAdmission::IsClaimed(NativeServiceAdmission::Category::Clock) && CheckBinding(true),
              "readonly retirement detaches hooks and releases every borrowed reader and exact service admission");
    }

    void Run(ClockImageFixture& fixture)
    {
        static_assert(std::is_abstract_v<CGameDE> && !std::is_abstract_v<Game>);
        Context context;
        context.bundle.generation = 777;
        const auto untouched = context.bundle;
        Legacy     legacy;
        Check(legacy.CaptureConstructorServices(&runtime.clock, context.bundle) == ClockResult::NotInitialized && context.bundle.Same(untouched) &&
                  legacy.ValidateConstructorServices(context.bundle) == ClockResult::NotInitialized,
              "older timing owners explicitly refuse full-bundle capture and validation without fabricating output");
        Game historical(context.timing, runtime.clock, legacy, context.streaming, context.state);
        Check(historical.GetClock() == &runtime.clock, "historical unprepared construction keeps its existing noexcept borrowed-clock identity getter");
        Check(Refuses([&] { Game refused(context.timing, runtime.clock, legacy, context.streaming, context.state, context.bundle); }),
              "historical constructor success cannot be mistaken for full-service preparation");

        alignas(std::uintptr_t) std::array<unsigned char, 8> model{};
        ConstructorWeaponFixture::Seed(fixture, model);
        ApplicationFrameContext         observer;
        NativeClockService              service;
        NativeWeaponConstructorProgress progress;
        BeginApplicationOwned(service, fixture, observer, true, &Prepare, &progress);
        for (std::size_t row = 0; row < NativeWeaponInfoProfile::RowCount; ++row)
        {
            Check(service.Renew(runtime.generation.load()), "renew only between distinct prerequisite preparation frames");
            fixture.invokeFrame(fixture.frameOwner.data(), 0.125f);
        }
        Check(progress.complete && !progress.failed && progress.completedRows == NativeWeaponInfoProfile::RowCount,
              "actual shared constructor prerequisite is complete before the read-only bundle test starts");
        FinishApplicationOwned(service);
        SeedServices(fixture);
        BeginOwned(service, fixture, false, true, true, &Handler, &context);
        const auto invoke = [&]
        {
            Check(service.Renew(runtime.generation.load()), "renew only before a separate admitted full-service read callback");
            fixture.invokeTimer();
        };
        const auto weaponWrites = ClockTestFaults::weaponExchanges.load();
        const auto clockWrites = ClockTestFaults::clockDataWrites.load();
        const auto statsCalls = ClockStatsOwnerTest::calls;
        const auto wantedCalls = ClockWantedOwnerTest::calls;
        const auto ropesCreated = ClockRopesOwnerTest::creates;
        const auto ropesRemoved = ClockRopesOwnerTest::removes;
        const auto ropePin = NativeRopesProfile::Code[0];
        const auto changed = static_cast<unsigned char>(ropePin.bytes[0] ^ 1);
        fixture.Patch(ropePin.rva, &changed, 1);
        invoke();
        Check(context.result != ClockResult::Ok && context.bundle.Same(untouched) && runtime.statsGeneration == runtime.generation.load(),
              "a failed partial capture never publishes any subset of services");
        fixture.Patch(ropePin.rva, ropePin.bytes.data(), ropePin.size);
        invoke();
        Check(context.result == ClockResult::Ok && context.bundle.Complete() && context.bundle.generation == runtime.generation.load() &&
                  context.bundle.stats == &runtime.stats && context.bundle.playerInfo == &runtime.playerInfoSdk && context.bundle.wanted == &runtime.wanted &&
                  context.bundle.ropes == &runtime.ropesSdk && context.bundle.weather == &runtime.weatherSdk &&
                  context.bundle.weaponManager == &runtime.weaponStatManager && !runtime.stopping.load(),
              "one actual current runtime atomically publishes every complete service and its generation");
        for (std::size_t slot = 0; slot < context.bundle.weapons.size(); ++slot)
            Check(context.bundle.weapons[slot] == runtime.weaponBank.GetSlot(slot), "all 80 captured weapon identities are the existing owned SDK wrappers");
        context.operation = 1;
        invoke();
        context.operation = 2;
        invoke();
        const auto prior = context.bundle;
        Check(runtime.access.CaptureConstructorServices(&runtime.clock, context.bundle) != ClockResult::Ok && context.bundle.Same(prior),
              "outside a callback even a current bundle carries no capture authority");
        Retire(service, fixture);
        Check(context.game->GetClock() == &runtime.clock,
              "GetClock keeps the existing noexcept identity contract and makes no cross-generation native lease claim");
        Check(Refuses([&] { context.game->GetStats(); }), "prepared native service getters refuse after their actual owner End");
        BeginOwned(service, fixture, false, true, true, &Handler, &context);
        context.operation = 3;
        invoke();
        Check(ClockTestFaults::weaponExchanges.load() == weaponWrites && ClockTestFaults::clockDataWrites.load() == clockWrites &&
                  ClockStatsOwnerTest::calls == statsCalls && ClockWantedOwnerTest::calls == wantedCalls && ClockRopesOwnerTest::creates == ropesCreated &&
                  ClockRopesOwnerTest::removes == ropesRemoved && !runtime.nativeTouched.load(),
              "capture, validation, prepared construction and selected reads never replay native defaults or mutate services");
        context.game.reset();
        Retire(service, fixture);
        ClockReaderTestHost::image = 0;
        std::puts("Production native constructor service bundle callback owner fixture completed");
        std::puts("Production actual prepared constructor service bundle SDK fixture completed");
    }
}
