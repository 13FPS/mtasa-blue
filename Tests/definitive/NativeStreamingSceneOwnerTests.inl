/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Genuine scene entry and persistent effects use the retained owner
 ****************************************************************************/
// Extend the existing current root/game-mode/world fixture. The persistent
// level and GTAWorldSettings are independent registered objects, not aliases
// inferred from an existing model pointer or a synthetic collision provider.
{
    namespace Scene = NativeStreamingSceneProfile;
    for (const auto& pin : Scene::Pins)
        admit(pin.rva, pin.bytes, pin.size, pin.code ? PAGE_EXECUTE_READ : PAGE_READONLY);
    const auto oldSettingsClass = fixture.Get<std::uintptr_t>(Scene::SettingsClassRva);
    admit(Scene::SettingsClassRva, &oldSettingsClass, sizeof(oldSettingsClass), PAGE_READWRITE);
    alignas(std::uintptr_t) std::array<unsigned char, 0x278>              persistent{};
    alignas(std::uintptr_t) std::array<unsigned char, 0x490>              settings{};
    alignas(std::uintptr_t) std::array<unsigned char, 0x40>               settingsClass{};
    alignas(std::uintptr_t) std::array<unsigned char, Scene::TargetBytes> target{};
    alignas(std::uintptr_t) std::array<unsigned char, 14 * 24>            registry{};
    std::array<std::uintptr_t, 1>                                         sceneChunks{}, settingsChain{};
    const auto address = [](const auto& object) { return reinterpret_cast<std::uintptr_t>(object.data()); };
    const auto resetScene = [&]
    {
        graph.Reset();
        graph.world[Q::WorldModeOffset] = 2;
        persistent.fill(0);
        settings.fill(0);
        settingsClass.fill(0);
        target.fill(0);
        registry.fill(0);
        std::memcpy(registry.data(), graph.items.data(), graph.items.size());
        const std::array<unsigned char*, 3> extra{persistent.data(), settings.data(), settingsClass.data()};
        for (std::size_t index = 0; index != extra.size(); ++index)
        {
            auto* object = extra[index];
            store(object, 0, fixture.image + NativeStartupWorldProfile::ownerVtableRva);
            store(object, 0xc, static_cast<std::int32_t>(11 + index));
            store(object, 0x10, graph.Class(0));
            store(registry.data(), (11 + index) * 24, reinterpret_cast<std::uintptr_t>(object));
            store(registry.data(), (11 + index) * 24 + 0x10, static_cast<std::uint32_t>(111 + index));
        }
        store(persistent.data(), 0x10, graph.Class(5));
        store(persistent.data(), Scene::LevelSettingsOffset, address(settings));
        store(settings.data(), 0x10, address(settingsClass));
        store(settings.data(), Scene::SettingsTargetOffset, address(target));
        settingsChain[0] = address(settingsClass) + 0x30;
        store(settingsClass.data(), 0x30, reinterpret_cast<std::uintptr_t>(settingsChain.data()));
        store(settingsClass.data(), 0x38, std::int32_t{0});
        store(settingsClass.data(), 8, std::uint32_t{0x10});
        store(graph.world.data(), Scene::WorldLevelOffset, address(persistent));
        sceneChunks[0] = address(registry);
        fixture.Put(NativeStartupWorldProfile::objectChunksRva, reinterpret_cast<std::uintptr_t>(sceneChunks.data()));
        fixture.Put(NativeStartupWorldProfile::objectCountRva, std::int32_t{14});
        fixture.Put(Scene::SettingsClassRva, address(settingsClass));
        ClockStreamingSceneOwnerTest::image = fixture.image;
        ClockStreamingSceneOwnerTest::target = address(target);
        ClockStreamingSceneOwnerTest::world = graph.World();
        ClockStreamingSceneOwnerTest::level = graph.Level();
    };
    CVector                            position;
    const std::array<std::uint32_t, 3> positionBits{0x80000000, 0x3f800001, 0xc3964000};
    static_assert(sizeof(position) == sizeof(positionBits));
    std::memcpy(&position, positionBits.data(), sizeof(position));
    context.operation = ModelOwnershipOperation::Scene;
    context.scenePosition = &position;
    ClockStreamingSceneOwnerTest::source = &position;
    ClockStreamingSceneOwnerTest::reenter = +[]() noexcept
    {
        using namespace ClockStreamingSceneOwnerTest;
        nested[0] = runtime.access.RequestStreamingModel(3, 6);
        nested[1] = runtime.access.RequestStreamingUpgrade(1000, 6);
        nested[2] = runtime.access.LoadStreamingModels(false);
        nested[3] = runtime.access.LoadStreamingModels(true);
        nested[4] = runtime.access.LoadStreamingScene(source);
    };
    DefaultPedIntelligenceOwner absentScene;
    Check(absentScene.LoadStreamingScene(&position) == ClockResult::NotInitialized,
          "older native timing owner refuses scene completion without fabricated acceptance");
    for (unsigned scenario = 0; scenario != 4; ++scenario)
    {
        resetScene();
        ClockStreamingSceneOwnerTest::scenario = scenario;
        const auto calls = ClockStreamingSceneOwnerTest::calls, effects = ClockStreamingSceneOwnerTest::effects;
        const auto requests = ClockStreamingRequestOwnerTest::calls, upgrades = ClockStreamingUpgradeOwnerTest::calls;
        const auto full = ClockStreamingLoadAllOwnerTest::calls, waits = ClockStreamingPriorityOwnerTest::waits,
                   drains = ClockStreamingPriorityOwnerTest::drains;
        NativeClockService service;
        BeginOwned(service, fixture, false, true, true, &ModelOwnershipOwnerHandler, &context);
        const auto invoke = [&]
        {
            Check(service.Renew(runtime.generation.load()), "renew actual owner before independent scene intent");
            fixture.invokeTimer();
        };
        Check(runtime.access.LoadStreamingScene(&position) != ClockResult::Ok && ClockStreamingSceneOwnerTest::calls == calls,
              "retained owner alone cannot lend current scene callback authority");
        if (!scenario)
        {
            ClockResult foreign = ClockResult::Ok;
            std::thread other([&] { foreign = runtime.access.LoadStreamingScene(&position); });
            other.join();
            Check(foreign != ClockResult::Ok && ClockStreamingSceneOwnerTest::calls == calls,
                  "foreign thread cannot inherit native world/settings scene admission");
            context.scenePosition = nullptr;
            invoke();
            Check(context.result == ClockResult::InvalidArgument && !runtime.stopping.load() && ClockStreamingSceneOwnerTest::calls == calls,
                  "null scene input refuses before engine entry without consuming the owner");
            context.scenePosition = &position;
            store(settings.data(), Scene::SettingsTargetOffset, std::uintptr_t{0});
            invoke();
            Check(context.result != ClockResult::Ok && !runtime.stopping.load() && ClockStreamingSceneOwnerTest::calls == calls,
                  "missing current settings target refuses cleanly before persistent scene effects");
            store(settings.data(), Scene::SettingsTargetOffset, address(target));
        }
        invoke();
        Check(ClockStreamingSceneOwnerTest::calls == calls + 1 && ClockStreamingSceneOwnerTest::effects == effects + 1 &&
                  ClockStreamingSceneOwnerTest::entry == Scene::EntryRva && ClockStreamingSceneOwnerTest::copied &&
                  std::memcmp(&ClockStreamingSceneOwnerTest::received, positionBits.data(), sizeof(position)) == 0 &&
                  std::memcmp(&position, positionBits.data(), sizeof(position)) == 0 && target.back() == 0x6b && graph.world[Q::WorldModeOffset] == 1,
              "actual callback enters genuine named scene function once with private bit-preserving finite XYZ and persistent effects");
        const bool failed = scenario == 1 || scenario == 2;
        Check(failed ? context.result != ClockResult::Ok && runtime.stopping.load() : context.result == ClockResult::Ok && !runtime.stopping.load(),
              "entered scene exception or old-world loss stops same owner; normal completion acknowledges current graph");
        if (scenario == 3)
        {
            for (const auto result : ClockStreamingSceneOwnerTest::nested)
                Check(result == ClockResult::Busy, "same completion scope refuses recursive scene/request/upgrade/full/selective adapter admission");
            Check(ClockStreamingRequestOwnerTest::calls == requests && ClockStreamingUpgradeOwnerTest::calls == upgrades &&
                      ClockStreamingLoadAllOwnerTest::calls == full && ClockStreamingPriorityOwnerTest::waits == waits &&
                      ClockStreamingPriorityOwnerTest::drains == drains && ClockStreamingSceneOwnerTest::calls == calls + 1,
                  "reentrant Busy calls do not enter another native operation or corrupt actual callback depth");
            // A subsequent deliberate request is a new admission. Exercise
            // the shared scope's release using the already qualified graph.
            context.operation = ModelOwnershipOperation::LoadAll;
            context.onlyPriority = false;
            ClockStreamingLoadAllOwnerTest::scenario = 0;
            invoke();
            Check(context.result == ClockResult::Ok && ClockStreamingLoadAllOwnerTest::calls == full + 1,
                  "scene scope releases after callbacks so later deliberate all-request loading can enter");
            context.operation = ModelOwnershipOperation::Scene;
        }
        const auto admitted = ClockStreamingSceneOwnerTest::calls;
        retire(service);
        Check(ClockStreamingSceneOwnerTest::calls == admitted && target.back() == 0x6b,
              "retirement never repeats scene loading or restores stale native scene target contents");
    }
    graph.Reset();
    ClockStreamingSceneOwnerTest::image = ClockStreamingSceneOwnerTest::target = ClockStreamingSceneOwnerTest::world = ClockStreamingSceneOwnerTest::level = 0;
    ClockStreamingSceneOwnerTest::source = nullptr;
    ClockStreamingSceneOwnerTest::reenter = nullptr;
    ClockStreamingSceneOwnerTest::scenario = 0;
    context.scenePosition = nullptr;
    std::puts("Production native scene callback owner fixture completed");
    std::puts("Production native streaming shared reentry callback owner fixture completed");
}
