/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Genuine scene admission over owned typed world/settings allocations
 ****************************************************************************/
#pragma push_macro("main")
#undef main
#define main NativeStreamingSceneHistoricalLoadAllMain
#include "NativeStreamingLoadAllTests.cpp"
#undef main
#pragma pop_macro("main")
#include "../../Client/game_de/NativeStreamingSceneCalls.h"
#include "../../Client/game_de/NativeStreamingSceneProfile.h"
namespace
{
    mta::game_de::ClockResult InvokeScene(mta::game_de::NativeStreamingSceneCalls::NativeFunction, const CVector*, std::uint32_t&) noexcept;
}
// The actual completion gate, owner, image and UObject guards above compile
// once. Only copied-byte faults and the final native scene entry are replaced.
#define GetModuleHandleW                     PoolTestHost::Module
#define ReadProcessMemory                    PoolTestHost::Read
#define MTA_DE_STREAMING_SCENE_NATIVE_INVOKE ::InvokeScene
#include "../../Client/game_de/NativeStreamingSceneCalls.cpp"
#undef MTA_DE_STREAMING_SCENE_NATIVE_INVOKE
#undef ReadProcessMemory
#undef GetModuleHandleW
namespace
{
    namespace SC = NativeStreamingSceneCalls;
    namespace SCP = NativeStreamingSceneProfile;
    namespace DC = NativeStreamingCompletionContext;
    struct SceneFixture : LoadFixture
    {
        alignas(std::uintptr_t) std::array<unsigned char, 0x278> persistent{};
        alignas(std::uintptr_t) std::array<unsigned char, 0x490> settings{};
        alignas(std::uintptr_t) std::array<unsigned char, 0x40> settingsClass{};
        alignas(std::uintptr_t) std::array<unsigned char, SCP::TargetBytes> target{};
        alignas(std::uintptr_t) std::array<unsigned char, 14 * 24> registry{};
        std::array<std::uintptr_t, 1> sceneChunks{}, settingsChain{};
        CVector                       position{123.25f, -456.5f, 78.75f};
        explicit SceneFixture(const std::filesystem::path& directory) : LoadFixture(directory)
        {
            for (const auto& pin : SCP::Pins)
                transform.image.Patch(pin.rva, pin.bytes, pin.size, pin.code ? PAGE_EXECUTE_READ : PAGE_READONLY);
            Reset();
        }
        void Reset()
        {
            LoadFixture::Reset();
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
                Put(object, 0, transform.image.image + NativeStartupWorldProfile::ownerVtableRva);
                Put(object, 0xc, static_cast<std::int32_t>(11 + index));
                Put(object, 0x10, graph.Class(0));
                Put(registry.data(), (11 + index) * 24, Address(object));
                Put(registry.data(), (11 + index) * 24 + 0x10, static_cast<std::uint32_t>(111 + index));
            }
            // PersistentLevel is intentionally distinct from the GameMode's
            // outer Level. Native LoadScene follows UWorld's current pointer.
            Put(persistent.data(), 0x10, graph.Class(5));
            Put(persistent.data(), SCP::LevelSettingsOffset, Address(settings.data()));
            Put(settings.data(), 0x10, Address(settingsClass.data()));
            Put(settings.data(), SCP::SettingsTargetOffset, Address(target.data()));
            settingsChain[0] = Address(settingsClass.data()) + 0x30;
            Put(settingsClass.data(), 0x30, reinterpret_cast<std::uintptr_t>(settingsChain.data()));
            Put(settingsClass.data(), 0x38, std::int32_t{0});
            Put(settingsClass.data(), 8, std::uint32_t{0x10});
            Put(graph.world.data(), SCP::WorldLevelOffset, Address(persistent.data()));
            sceneChunks[0] = Address(registry.data());
            Global(NativeStartupWorldProfile::objectChunksRva, reinterpret_cast<std::uintptr_t>(sceneChunks.data()));
            Global(NativeStartupWorldProfile::objectCountRva, std::int32_t{14});
            Global(SCP::SettingsClassRva, Address(settingsClass.data()));
            position = CVector(123.25f, -456.5f, 78.75f);
        }
    };
    SceneFixture*      activeScene = nullptr;
    unsigned           sceneCalls = 0, sceneEffects = 0, sceneScenario = 0;
    SC::NativeFunction passedScene = nullptr;
    CVector            passedPosition;
    ClockResult        nestedScene = ClockResult::Ok, nestedLoad = ClockResult::Ok;
    ClockResult        InvokeScene(SC::NativeFunction function, const CVector* position, std::uint32_t& exception) noexcept
    {
        ++sceneCalls;
        ++sceneEffects;
        passedScene = function;
        passedPosition = *position;
        auto& f = *activeScene;
        // Model one irreversible native scene effect without implementing the
        // loader. Refusal after entry must preserve it, never retry or rewind.
        f.target.back() = 0x6b;
        if (f.graph.world[LPF::WorldModeOffset] == 2)
            f.graph.world[LPF::WorldModeOffset] = 1;
        switch (sceneScenario)
        {
            case 1:
                exception = EXCEPTION_ACCESS_VIOLATION;
                return ClockResult::OriginalException;
            case 2:
                f.Global(LPF::RootRva, std::uintptr_t{0});
                break;
            case 3:
                f.Put(f.settings.data(), SCP::SettingsTargetOffset, std::uintptr_t{0});
                break;
            case 4:
                f.Put(f.registry.data(), 12 * 24 + 0x10, std::uint32_t{999});
                break;
            case 5:
                PoolTestHost::ownerResult = ClockResult::Expired;
                break;
            case 6:
                f.Global(NativeBorrowedGcProfile::markerRva, LONG{1});
                break;
            case 7:
            {
                SC::Receipt nested;
                LC::Receipt load;
                nestedScene = SC::Load(f.transform.image.image, &f.position, &PoolTestHost::Owner, &nested);
                nestedLoad = LC::Load(f.transform.image.image, false, &PoolTestHost::Owner, &load);
                if (nested.entered || load.entered)
                    std::abort();
                break;
            }
            case 8:
                f.Global(NativeGameStateProfile::stateRva, std::uint32_t{7});
                break;
            case 9:
                f.Put(f.graph.world.data(), SCP::WorldLevelOffset, f.graph.Level());
                break;
            case 10:
                f.Global(SCP::SettingsClassRva, f.graph.Class(2));
                break;
            case 11:
                f.position.fX = 999;
                break;
        }
        return ClockResult::Ok;
    }
    void ChangeSceneTarget()
    {
        activeScene->Put(activeScene->settings.data(), SCP::SettingsTargetOffset, std::uintptr_t{0});
    }
    void ChangeScenePosition()
    {
        activeScene->position.fY += 1;
    }
}
int main(int argc, char** argv)
{
    Check(argc == 2 && std::filesystem::path(argv[1]).is_absolute(), "scene evidence uses an explicit workspace-owned directory");
    SceneFixture fixture(argv[1]);
    activeScene = &fixture;
    activeLoad = &fixture;
    const auto image = fixture.transform.image.image;
    PoolTestHost::image = image;
    SC::Receipt receipt;
    const auto  reset = [&]
    {
        fixture.Reset();
        sceneCalls = sceneEffects = sceneScenario = loadCalls = 0;
        passedScene = nullptr;
        passedPosition = CVector();
        nestedScene = nestedLoad = ClockResult::Ok;
    };
    reset();
    Check(SC::Load(image, nullptr, &PoolTestHost::Owner, &receipt) == ClockResult::InvalidArgument && !receipt.entered && !sceneCalls,
          "missing vector refuses before any scene effect");
    Check(SC::Load(image, &fixture.position, nullptr, &receipt) == ClockResult::NotInitialized && !receipt.entered,
          "current initialized game does not substitute for actual callback authority");
    for (unsigned char mode = 0; mode != 3; ++mode)
    {
        reset();
        fixture.graph.world[LPF::WorldModeOffset] = mode;
        const auto original = fixture.position;
        Check(SC::Load(image, &fixture.position, &PoolTestHost::Owner, &receipt) == ClockResult::Ok && receipt.entered && receipt.returned && sceneCalls == 1 &&
                  sceneEffects == 1 && reinterpret_cast<std::uintptr_t>(passedScene) == image + SCP::EntryRva &&
                  !std::memcmp(&passedPosition, &original, sizeof(original)) && receipt.position[0] == original.fX && receipt.position[1] == original.fY &&
                  receipt.position[2] == original.fZ && receipt.root == fixture.Address(fixture.application) && receipt.gameMode == fixture.graph.Game() &&
                  receipt.level == fixture.graph.Level() && receipt.persistentLevel == fixture.Address(fixture.persistent.data()) &&
                  receipt.world == fixture.graph.World() && receipt.settings == fixture.Address(fixture.settings.data()) &&
                  receipt.settingsClass == fixture.Address(fixture.settingsClass.data()) && receipt.target == fixture.Address(fixture.target.data()) &&
                  receipt.modeBefore == mode && receipt.modeAfter == (mode == 2 ? 1 : mode) && fixture.target.back() == 0x6b,
              "genuine vector entry receives exact XYZ once under current typed PersistentLevel/settings and legitimate world-mode progress");
    }
    for (unsigned scenario = 1; scenario <= 10; ++scenario)
    {
        if (scenario == 7)
            continue;
        reset();
        sceneScenario = scenario;
        Check(SC::Load(image, &fixture.position, &PoolTestHost::Owner, &receipt) != ClockResult::Ok && receipt.entered && receipt.returned == (scenario != 1) &&
                  sceneCalls == 1 && sceneEffects == 1 && fixture.target.back() == 0x6b &&
                  (scenario != 1 || receipt.exceptionCode == EXCEPTION_ACCESS_VIOLATION),
              "entered exception, owner/world/settings/member/GC/state loss preserves effects with no retry or snapshot restoration");
        reset();
        Check(SC::Load(image, &fixture.position, &PoolTestHost::Owner, &receipt) == ClockResult::Ok && sceneCalls == 1,
              "shared loading gate releases even after entered failure");
    }
    reset();
    sceneScenario = 7;
    Check(SC::Load(image, &fixture.position, &PoolTestHost::Owner, &receipt) == ClockResult::Ok && nestedScene == ClockResult::Busy &&
              nestedLoad == ClockResult::Busy && sceneCalls == 1 && !loadCalls,
          "native callback cannot recursively enter Scene or existing blocking-all through an independent gate");
    reset();
    Check(DC::TryEnter(), "borrow the existing completion scope rather than inventing a scene-only gate");
    {
        DC::Scope held;
        Check(SC::Load(image, &fixture.position, &PoolTestHost::Owner, &receipt) == ClockResult::Busy && !receipt.entered && !sceneCalls,
              "another genuine completion already in progress refuses Scene before entry");
    }
    for (unsigned failure = 0; failure != 15; ++failure)
    {
        reset();
        switch (failure)
        {
            case 0:
                fixture.Global(LPF::RootRva, std::uintptr_t{0});
                break;
            case 1:
                fixture.Put(fixture.graph.world.data(), SCP::WorldLevelOffset, std::uintptr_t{0});
                break;
            case 2:
                fixture.Put(fixture.persistent.data(), SCP::LevelSettingsOffset, std::uintptr_t{0});
                break;
            case 3:
                fixture.Put(fixture.settings.data(), SCP::SettingsTargetOffset, std::uintptr_t{1});
                break;
            case 4:
                fixture.Global(SCP::SettingsClassRva, std::uintptr_t{0});
                break;
            case 5:
                fixture.Put(fixture.settings.data(), 0x10, fixture.graph.Class(3));
                break;
            case 6:
                fixture.Put(fixture.persistent.data(), 0x10, fixture.graph.Class(3));
                break;
            case 7:
                fixture.Put(fixture.registry.data(), 12 * 24 + 8, std::uint32_t{0x10000000});
                break;
            case 8:
                fixture.Put(fixture.registry.data(), 13 * 24 + 8, std::uint32_t{0x10000000});
                break;
            case 9:
                fixture.Global(NativeStartGameProfile::ThreadIdRva, static_cast<DWORD>(GetCurrentThreadId() + 1));
                break;
            case 10:
                fixture.Global(NativeBorrowedGcProfile::markerRva, LONG{1});
                break;
            case 11:
                fixture.Global(NativeGameStateProfile::stateRva, std::uint32_t{7});
                break;
            case 12:
                fixture.graph.world[LPF::WorldModeOffset] = 3;
                break;
            case 13:
                PoolTestHost::ownerResult = ClockResult::Expired;
                break;
            case 14:
                fixture.settingsChain[0] = fixture.graph.Class(2) + 0x30;
                break;
        }
        Check(SC::Load(image, &fixture.position, &PoolTestHost::Owner, &receipt) != ClockResult::Ok && !receipt.entered && !sceneCalls,
              "missing/stale/type-invalid current scene prerequisites refuse before native effects");
    }
    for (const auto value : {(std::numeric_limits<float>::quiet_NaN)(), (std::numeric_limits<float>::infinity)(), -(std::numeric_limits<float>::infinity)(),
                             (std::numeric_limits<float>::max)()})
        for (unsigned axis = 0; axis != 3; ++axis)
        {
            reset();
            float* coordinate = axis == 0 ? &fixture.position.fX : axis == 1 ? &fixture.position.fY : &fixture.position.fZ;
            *coordinate = value;
            Check(SC::Load(image, &fixture.position, &PoolTestHost::Owner, &receipt) == ClockResult::InvalidArgument && !receipt.entered && !sceneCalls,
                  "nonfinite or native scaling overflow refuses without invented coordinate limits");
        }
    reset();
    fixture.position = CVector(-0.0f, 0.0f, 1000000.0f);
    Check(SC::Load(image, &fixture.position, &PoolTestHost::Owner, &receipt) == ClockResult::Ok &&
              !std::memcmp(&passedPosition, &fixture.position, sizeof(passedPosition)),
          "representable coordinates and signed zero are passed unchanged without a guessed map clamp");
    reset();
    sceneScenario = 11;
    Check(SC::Load(image, &fixture.position, &PoolTestHost::Owner, &receipt) == ClockResult::Ok && passedPosition.fX == 123.25f && fixture.position.fX == 999,
          "native callback receives the admitted local vector rather than a caller buffer reused after entry");
    for (const auto& pin : SCP::Pins)
    {
        reset();
        const auto bad = static_cast<unsigned char>(pin.bytes[0] ^ 1);
        fixture.transform.image.Patch(pin.rva, &bad, 1, pin.code ? PAGE_EXECUTE_READ : PAGE_READONLY);
        Check(SC::Load(image, &fixture.position, &PoolTestHost::Owner, &receipt) != ClockResult::Ok && !receipt.entered && !sceneCalls,
              "each genuine scene/settings/named-branch/coordinate provenance pin is required before entry");
        fixture.transform.image.Patch(pin.rva, pin.bytes, 1, pin.code ? PAGE_EXECUTE_READ : PAGE_READONLY);
    }
    reset();
    PoolTestHost::watched = fixture.Address(fixture.settings.data()) + SCP::SettingsTargetOffset;
    PoolTestHost::after = &ChangeSceneTarget;
    Check(SC::Load(image, &fixture.position, &PoolTestHost::Owner, &receipt) != ClockResult::Ok && !receipt.entered && !sceneCalls,
          "target recycled between current graph observations refuses before native entry");
    reset();
    PoolTestHost::watched = reinterpret_cast<std::uintptr_t>(&fixture.position);
    PoolTestHost::after = &ChangeScenePosition;
    Check(SC::Load(image, &fixture.position, &PoolTestHost::Owner, &receipt) == ClockResult::BindingChanged && !receipt.entered && !sceneCalls,
          "caller XYZ changing between actual copies is never admitted as a coherent position");
    for (unsigned failure = 1; failure <= 2; ++failure)
    {
        reset();
        PoolTestHost::failAt = failure;
        Check(SC::Load(image, &fixture.position, &PoolTestHost::Owner, &receipt) == ClockResult::ReadFailure && !receipt.entered && !sceneCalls,
              "either owned vector-copy failure refuses before loading authority is acquired");
    }
    reset();
    ClockResult foreign = ClockResult::Ok;
    std::thread other([&] { foreign = SC::Load(image, &fixture.position, &PoolTestHost::Owner, &receipt); });
    other.join();
    Check(foreign != ClockResult::Ok && !receipt.entered && !sceneCalls, "foreign thread cannot borrow current scene loading authority");
    reset();
    Check(SC::Load(image, &fixture.position, &PoolTestHost::Owner, &receipt) == ClockResult::Ok && sceneCalls == 1,
          "scene admission remains usable after all pre-entry refusal classes");
    std::puts("Production native streaming scene primitive fixture completed");
    std::printf("Passed %u native streaming-scene checks (64-bit; no native engine code executed)\n", checks);
    return 0;
}
