/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Source-ordered collision completion using actual owned world guards
 ****************************************************************************/
#pragma push_macro("main")
#undef main
#define main NativeCollisionSceneHistoricalSceneMain
#include "NativeStreamingSceneTests.cpp"
#undef main
#pragma pop_macro("main")
#include "../../Client/game_de/NativeStreamingCollisionSceneCalls.h"
#include "../../Client/game_de/NativeStreamingCollisionSceneProfile.h"

namespace CollisionFixtureHost
{
    namespace Calls = mta::game_de::NativeStreamingCollisionSceneCalls;
    mta::game_de::ClockResult Store(Calls::StoreFunction, CVector*, bool, std::uint32_t&) noexcept;
    mta::game_de::ClockResult Finish(Calls::LoadAllFunction, std::uint32_t&) noexcept;
}
// Existing current-image, GT/GC, owner, registry and shared completion guards
// compile unchanged. Only the three final native stages are substituted.
#define GetModuleHandleW                                PoolTestHost::Module
#define ReadProcessMemory                               PoolTestHost::Read
#define MTA_DE_STREAMING_COLLISION_SCENE_STORE_INVOKE   ::CollisionFixtureHost::Store
#define MTA_DE_STREAMING_COLLISION_SCENE_LOADALL_INVOKE ::CollisionFixtureHost::Finish
#include "../../Client/game_de/NativeStreamingCollisionSceneCalls.cpp"
#undef MTA_DE_STREAMING_COLLISION_SCENE_LOADALL_INVOKE
#undef MTA_DE_STREAMING_COLLISION_SCENE_STORE_INVOKE
#undef ReadProcessMemory
#undef GetModuleHandleW

namespace CollisionFixtureHost
{
    using mta::game_de::ClockResult;
    namespace Layout = mta::game_de::NativeStreamingCollisionSceneProfile;
    struct Fixture final : SceneFixture
    {
        explicit Fixture(const std::filesystem::path& path) : SceneFixture(path)
        {
            for (const auto& pin : Layout::Code)
                transform.image.Patch(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        }
    };
    Fixture*                                    active = nullptr;
    unsigned                                    calls = 0, failStage = 3, mode = 0, profileEvents = 0;
    std::array<unsigned, 3>                     effects{}, order{};
    std::array<std::uintptr_t, 3>               functions{};
    std::array<std::uintptr_t, 2>               arguments{};
    std::array<std::array<std::uint32_t, 3>, 2> positions{};
    std::array<bool, 2>                         passedTrue{};
    ClockResult                                 nestedCollision = ClockResult::Ok, nestedScene = ClockResult::Ok, nestedLoad = ClockResult::Ok;

    void ExpireAtProfile()
    {
        PoolTestHost::ownerResult = ClockResult::Expired;
    }
    void ExpireAtFinalPreentryProfile()
    {
        // These are the initial and final collision-profile phases, rather
        // than a dependency on the total number of owner checks in Capture.
        if (++profileEvents == 2)
            ExpireAtProfile();
        else
            PoolTestHost::after = &ExpireAtFinalPreentryProfile;
    }

    ClockResult EnterStage(unsigned stage, std::uint32_t& exception) noexcept
    {
        Check(calls < order.size(), "one collision operation cannot execute more than three native stages");
        order[calls++] = stage;
        ++effects[stage];
        active->target[stage] = static_cast<unsigned char>(0x91 + stage);
        // A legitimate engine mode transition is progress, not rollback data.
        if (active->graph.world[LPF::WorldModeOffset] == 2)
            active->graph.world[LPF::WorldModeOffset] = 1;
        if (mode == 4 && stage == 0)
        {
            Calls::Receipt collision;
            SC::Receipt    scene;
            LC::Receipt    load;
            const auto     image = active->transform.image.image;
            nestedCollision = Calls::Load(image, &active->position, &PoolTestHost::Owner, &collision);
            nestedScene = SC::Load(image, &active->position, &PoolTestHost::Owner, &scene);
            nestedLoad = LC::Load(image, false, &PoolTestHost::Owner, &load);
            Check(!collision.entered && !scene.entered && !load.entered, "reentry cannot consume a later native stage through another API");
        }
        if (stage != failStage)
            return ClockResult::Ok;
        if (mode == 1)
        {
            exception = EXCEPTION_ACCESS_VIOLATION;
            return ClockResult::OriginalException;
        }
        if (mode == 2)
            PoolTestHost::ownerResult = ClockResult::Expired;
        if (mode == 3)
            active->Put(active->registry.data(), 3 * 24 + 0x10, std::uint32_t{999});
        if (mode == 5)
            active->Global(NativeBorrowedGcProfile::markerRva, LONG{1});
        if (mode == 7)
        {
            // Lose admission during the last profile copy, after the earlier
            // acknowledgement owner check has passed. The closing owner
            // bookend must stop the next stage (or final success).
            PoolTestHost::watched = active->transform.image.image + Layout::IplRva;
            PoolTestHost::after = &ExpireAtProfile;
        }
        return ClockResult::Ok;
    }
    ClockResult Store(Calls::StoreFunction function, CVector* position, bool ignorePlayerVehicle, std::uint32_t& exception) noexcept
    {
        const auto     image = active->transform.image.image;
        const auto     address = reinterpret_cast<std::uintptr_t>(function);
        const unsigned stage = address == image + Layout::CollisionRva ? 0u : 1u;
        Check(address == image + (stage == 0 ? Layout::CollisionRva : Layout::IplRva), "store dispatch uses only the exact COL and IPL entrypoints");
        functions[stage] = address;
        arguments[stage] = reinterpret_cast<std::uintptr_t>(position);
        std::memcpy(positions[stage].data(), position, sizeof(CVector));
        passedTrue[stage] = ignorePlayerVehicle;
        // Native store parameters are by value in source. Deliberately change
        // this pointer-local copy: IPL must receive a separate pristine copy.
        const std::array<std::uint32_t, 3> changed{{0x441fc000, 0xc45e0000, 0x43a68000}};
        std::memcpy(position, changed.data(), sizeof(CVector));
        if (mode == 6 && stage == 0)
            active->position = CVector(9, 8, 7);
        return EnterStage(stage, exception);
    }
    ClockResult Finish(Calls::LoadAllFunction function, std::uint32_t& exception) noexcept
    {
        functions[2] = reinterpret_cast<std::uintptr_t>(function);
        Check(functions[2] == active->transform.image.image + Layout::LoadAllRva,
              "final stage is the raw no-argument native LoadAll entry under the held gate");
        return EnterStage(2, exception);
    }
    void ChangePosition()
    {
        active->position.fY += 1;
    }
}

int main(int argc, char** argv)
{
    namespace H = CollisionFixtureHost;
    namespace Calls = mta::game_de::NativeStreamingCollisionSceneCalls;
    namespace Layout = mta::game_de::NativeStreamingCollisionSceneProfile;
    Check(argc == 2 && std::filesystem::path(argv[1]).is_absolute(), "collision evidence uses an explicit workspace-owned directory");
    H::Fixture fixture(argv[1]);
    H::active = &fixture;
    activeScene = &fixture;
    activeLoad = &fixture;
    const auto image = fixture.transform.image.image;
    PoolTestHost::image = image;
    Calls::Receipt receipt;
    const auto     reset = [&]
    {
        fixture.Reset();
        H::calls = 0;
        H::failStage = 3;
        H::mode = H::profileEvents = 0;
        H::effects.fill(0);
        H::order.fill(99);
        H::functions.fill(0);
        H::arguments.fill(0);
        H::passedTrue.fill(false);
        for (auto& position : H::positions)
            position.fill(0);
        H::nestedCollision = H::nestedScene = H::nestedLoad = ClockResult::Ok;
        sceneCalls = loadCalls = sceneEffects = loadEffects = sceneScenario = loadScenario = 0;
    };
    const auto run = [&] { return Calls::Load(image, &fixture.position, &PoolTestHost::Owner, &receipt); };
    reset();
    Check(Calls::Load(image, nullptr, &PoolTestHost::Owner, &receipt) == ClockResult::InvalidArgument && !receipt.entered && !H::calls,
          "null collision position refuses before a native entry");
    Check(Calls::Load(image, &fixture.position, nullptr, &receipt) == ClockResult::NotInitialized && !receipt.entered && !H::calls,
          "a loaded world cannot replace the required owner callback");
    for (unsigned char initialMode = 0; initialMode != 3; ++initialMode)
    {
        reset();
        fixture.graph.world[LPF::WorldModeOffset] = initialMode;
        const auto source = fixture.position;
        Check(run() == ClockResult::Ok && receipt.entered && receipt.returned && receipt.verified && H::calls == 3 &&
                  H::order == std::array<unsigned, 3>{{0, 1, 2}} && H::effects == std::array<unsigned, 3>{{1, 1, 1}},
              "actual COLtrue then IPLtrue then raw LoadAll run exactly once in source order");
        Check(receipt.collision.entered && receipt.collision.returned && receipt.ipl.entered && receipt.ipl.returned && receipt.loadAll.entered &&
                  receipt.loadAll.returned && receipt.collision.entryRva == Layout::CollisionRva && receipt.ipl.entryRva == Layout::IplRva &&
                  receipt.loadAll.entryRva == Layout::LoadAllRva && receipt.root == fixture.Address(fixture.application) &&
                  receipt.gameMode == fixture.graph.Game() && receipt.level == fixture.graph.Level() && receipt.world == fixture.graph.World() &&
                  receipt.modeBefore == initialMode && receipt.modeAfter == (initialMode == 2 ? 1 : initialMode),
              "per-stage receipts and current native graph bookends retain legitimate mode progress");
        Check(H::passedTrue[0] && H::passedTrue[1] && H::arguments[0] != H::arguments[1] &&
                  H::arguments[0] != reinterpret_cast<std::uintptr_t>(&fixture.position) &&
                  H::arguments[1] != reinterpret_cast<std::uintptr_t>(&fixture.position) && !std::memcmp(H::positions[0].data(), &source, 12) &&
                  !std::memcmp(H::positions[1].data(), &source, 12) && !std::memcmp(receipt.position.data(), &source, 12) &&
                  !std::memcmp(&fixture.position, &source, 12),
              "COL and IPL receive independent original XYZ by-value copies; their local mutation cannot rewrite caller intent");
    }
    reset();
    const std::array<std::uint32_t, 3> raw{{0x7fc01234, 0x80000000, 0x7f800000}};
    std::memcpy(&fixture.position, raw.data(), 12);
    Check(run() == ClockResult::Ok && H::positions[0] == raw && H::positions[1] == raw && !std::memcmp(receipt.position.data(), raw.data(), 12),
          "raw NaN payload, signed zero and infinity are forwarded without full-scene scaling or a finite/clamp filter");
    reset();
    H::mode = 6;
    const auto original = fixture.position;
    Check(run() == ClockResult::Ok && fixture.position.fX == 9 && !std::memcmp(H::positions[1].data(), &original, 12),
          "caller buffer reused during COL does not tear the already copied IPL argument");
    for (unsigned stage = 0; stage != 3; ++stage)
        for (unsigned fault : {1u, 2u, 3u, 5u, 7u})
        {
            reset();
            H::failStage = stage;
            H::mode = fault;
            const auto                               result = run();
            const std::array<Calls::StageReceipt, 3> stages{{receipt.collision, receipt.ipl, receipt.loadAll}};
            Check(result != ClockResult::Ok && receipt.entered && !receipt.verified && H::calls == stage + 1 && receipt.returned == (stage == 2 && fault != 1),
                  "entered failure exposes the exact native prefix for Runtime Stop without retrying later stages");
            for (unsigned index = 0; index != 3; ++index)
                Check(stages[index].entered == (index <= stage) && stages[index].returned == (index < stage || (index == stage && fault != 1)) &&
                          H::effects[index] == (index <= stage ? 1u : 0u) &&
                          fixture.target[index] == (index <= stage ? static_cast<unsigned char>(0x91 + index) : 0),
                      "exception or lost owner/member/GC acknowledgement preserves each completed effect and leaves all later stages untouched");
            Check(fault != 1 || stages[stage].exceptionCode == EXCEPTION_ACCESS_VIOLATION, "the actual failing stage retains its exception receipt");
        }
    reset();
    PoolTestHost::watched = image + Layout::IplRva;
    PoolTestHost::after = &H::ExpireAtFinalPreentryProfile;
    Check(run() == ClockResult::Expired && H::profileEvents == 2 && !receipt.entered && !H::calls,
          "owner expiry in the final preentry profile is refused by the closing callback bookend before COL");
    reset();
    H::mode = 4;
    Check(run() == ClockResult::Ok && H::nestedCollision == ClockResult::Busy && H::nestedScene == ClockResult::Busy && H::nestedLoad == ClockResult::Busy &&
              H::calls == 3 && !sceneCalls && !loadCalls,
          "all three APIs share one existing completion gate; nested calls cannot execute a separate stage");
    reset();
    Check(DC::TryEnter(), "hold the existing completion gate for an independent pre-entry refusal");
    {
        DC::Scope scope;
        Check(run() == ClockResult::Busy && !receipt.entered && !H::calls, "an already held gate refuses the complete collision operation");
    }
    for (unsigned failure = 0; failure != 6; ++failure)
    {
        reset();
        switch (failure)
        {
            case 0:
                fixture.Global(LPF::RootRva, std::uintptr_t{0});
                break;
            case 1:
                fixture.Global(NativeBorrowedGcProfile::markerRva, LONG{1});
                break;
            case 2:
                fixture.Global(NativeGameStateProfile::stateRva, std::uint32_t{7});
                break;
            case 3:
                PoolTestHost::ownerResult = ClockResult::Expired;
                break;
            case 4:
                fixture.Put(fixture.registry.data(), 3 * 24 + 8, std::uint32_t{0x10000000});
                break;
            case 5:
                fixture.Global(NativeStartGameProfile::ThreadIdRva, static_cast<DWORD>(GetCurrentThreadId() + 1));
                break;
        }
        Check(run() != ClockResult::Ok && !receipt.entered && !H::calls, "current owner, game thread, GC and typed graph admission refuse before effects");
    }
    for (const auto& pin : Layout::Code)
    {
        reset();
        const auto changed = static_cast<unsigned char>(pin.bytes[0] ^ 1);
        fixture.transform.image.Patch(pin.rva, &changed, 1, PAGE_EXECUTE_READ);
        Check(run() == ClockResult::UnsupportedBuild && !receipt.entered && !H::calls,
              "both complete native store bodies are independently required before entry");
        fixture.transform.image.Patch(pin.rva, pin.bytes, 1, PAGE_EXECUTE_READ);
    }
    reset();
    PoolTestHost::watched = reinterpret_cast<std::uintptr_t>(&fixture.position);
    PoolTestHost::after = &H::ChangePosition;
    Check(run() == ClockResult::BindingChanged && !receipt.entered && !H::calls, "torn source XYZ is refused before native loading");
    for (unsigned failure = 1; failure <= 2; ++failure)
    {
        reset();
        PoolTestHost::failAt = failure;
        Check(run() == ClockResult::ReadFailure && !receipt.entered && !H::calls, "either actual position copy can refuse without entering a stage");
    }
    reset();
    ClockResult foreign = ClockResult::Ok;
    std::thread other([&] { foreign = run(); });
    other.join();
    Check(foreign == ClockResult::WrongThread && !receipt.entered && !H::calls,
          "a foreign thread cannot borrow the native GT even with a nominal owner callback");
    reset();
    Check(run() == ClockResult::Ok && H::calls == 3, "the real shared gate remains usable after scoped refusal checks");
    std::puts("Production native streaming collision scene owner fixture completed");
    std::printf("Passed %u native streaming-collision-scene checks (64-bit; no native engine code executed)\n", checks);
    return 0;
}
