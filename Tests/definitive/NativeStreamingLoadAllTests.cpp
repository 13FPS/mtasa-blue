/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Real blocking-all qualification on owned images and typed world graphs
 ****************************************************************************/
#pragma push_macro("main")
#undef main
#define main NativeStreamingLoadAllHistoricalRequestMain
#include "NativeStreamingRequestTests.cpp"
#undef main
#pragma pop_macro("main")
#include "NativeStreamingLoadAllGraphFixture.h"
#include "../../Client/game_de/NativeStreamingLoadAllCalls.h"
namespace
{
    mta::game_de::ClockResult InvokeLoadAll(mta::game_de::NativeStreamingLoadAllCalls::NativeFunction, std::uint32_t&) noexcept;
}
#define GetModuleHandleW                       PoolTestHost::Module
#define ReadProcessMemory                      PoolTestHost::Read
#define MTA_DE_STREAMING_LOADALL_NATIVE_INVOKE ::InvokeLoadAll
#include "../../Client/game_de/NativeStreamingLoadAllCalls.cpp"
#undef MTA_DE_STREAMING_LOADALL_NATIVE_INVOKE
#undef ReadProcessMemory
#undef GetModuleHandleW
namespace
{
    namespace LC = NativeStreamingLoadAllCalls;
    namespace LPF = NativeStreamingLoadAllProfile;
    struct LoadFixture : RequestFixture
    {
        static void Write(void* host, std::uintptr_t rva, const void* bytes, std::size_t size, DWORD protection)
        {
            static_cast<LoadFixture*>(host)->transform.image.Patch(rva, bytes, size, protection);
        }
        NativeStreamingLoadAllFixture::Graph graph;
        explicit LoadFixture(const std::filesystem::path& directory)
            : RequestFixture(directory), graph(transform.image.image, Address(application), &Write, this)
        {
            for (const auto& pin : LPF::Code)
                transform.image.Patch(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
            for (const auto& pin : NativeStartupOwnerProfile::pins)
                transform.image.Patch(pin.rva, pin.bytes, pin.size, pin.code ? PAGE_EXECUTE_READ : PAGE_READONLY);
            for (const auto& pin : NativeStartupWorldProfile::pins)
                transform.image.Patch(pin.rva, pin.bytes, pin.size, pin.code ? PAGE_EXECUTE_READ : PAGE_READONLY);
        }
        void Reset()
        {
            RequestFixture::Reset();
            graph.Reset();
        }
    };
    LoadFixture*       activeLoad = nullptr;
    unsigned           loadCalls = 0, loadEffects = 0, loadScenario = 0;
    LC::NativeFunction passedLoad = nullptr;
    ClockResult        nestedResult = ClockResult::Ok;
    ClockResult        InvokeLoadAll(LC::NativeFunction function, std::uint32_t& exception) noexcept
    {
        ++loadCalls;
        ++loadEffects;
        passedLoad = function;
        auto& f = *activeLoad;
        if (f.graph.world[LPF::WorldModeOffset] == 2)
            f.graph.world[LPF::WorldModeOffset] = 1;
        f.application[0x78] = 0;
        if (loadScenario == 1)
        {
            exception = EXCEPTION_ACCESS_VIOLATION;
            return ClockResult::OriginalException;
        }
        if (loadScenario == 2)
            f.Global(LPF::RootRva, std::uintptr_t{0});
        if (loadScenario == 3)
            f.Put(f.graph.level.data(), 0xd0, std::uintptr_t{0});
        if (loadScenario == 4)
            f.Put(f.graph.items.data(), 0x10, std::uint32_t{999});
        if (loadScenario == 5)
            PoolTestHost::ownerResult = ClockResult::Expired;
        if (loadScenario == 6)
            f.Global(NativeBorrowedGcProfile::markerRva, LONG{1});
        if (loadScenario == 7)
        {
            LC::Receipt nested;
            nestedResult = LC::Load(f.transform.image.image, false, &PoolTestHost::Owner, &nested);
            if (nested.entered)
                std::abort();
        }
        return ClockResult::Ok;
    }
}
int main(int argc, char** argv)
{
    Check(argc == 2 && std::filesystem::path(argv[1]).is_absolute(), "load-all evidence uses workspace-owned explicit directory");
    LoadFixture fixture(argv[1]);
    activeLoad = &fixture;
    const auto image = fixture.transform.image.image;
    PoolTestHost::image = image;
    LC::Receipt receipt;
    const auto  reset = [&]
    {
        fixture.Reset();
        loadCalls = loadEffects = loadScenario = 0;
        passedLoad = nullptr;
        nestedResult = ClockResult::Ok;
    };
    reset();
    Check(LC::Load(0, true, nullptr, &receipt) == ClockResult::UnsupportedBuild && !receipt.entered && !loadCalls,
          "priority-only intent refuses before native ABI or owner admission rather than loading everything");
    Check(LC::Load(image, false, nullptr, &receipt) == ClockResult::NotInitialized && !receipt.entered, "unbound loader is not an admitted engine operation");
    for (unsigned char mode = 0; mode != 3; ++mode)
    {
        reset();
        fixture.graph.world[LPF::WorldModeOffset] = mode;
        fixture.application[0x78] = 1;
        Check(LC::Load(image, false, &PoolTestHost::Owner, &receipt) == ClockResult::Ok && receipt.entered && receipt.returned && loadCalls == 1 &&
                  reinterpret_cast<std::uintptr_t>(passedLoad) == image + LPF::EntryRva && receipt.root == fixture.Address(fixture.application) &&
                  receipt.gameMode == fixture.graph.Game() && receipt.level == fixture.graph.Level() && receipt.world == fixture.graph.World() &&
                  receipt.modeBefore == mode && receipt.modeAfter == (mode == 2 ? 1 : mode) && fixture.application[0x78] == 0,
              "actual no-argument blocking-all entry acknowledges same typed world and legitimate mode2-to1/queue progress");
    }
    for (unsigned scenario = 1; scenario <= 6; ++scenario)
    {
        reset();
        loadScenario = scenario;
        Check(LC::Load(image, false, &PoolTestHost::Owner, &receipt) != ClockResult::Ok && receipt.entered && loadCalls == 1 && loadEffects == 1,
              "entered fault, lost root/world/member/owner/GC acknowledgement preserves effects and never replays");
    }
    reset();
    loadScenario = 7;
    Check(LC::Load(image, false, &PoolTestHost::Owner, &receipt) == ClockResult::Ok && nestedResult == ClockResult::Busy && loadCalls == 1,
          "synchronous native callback cannot recursively enter blocking-all or consume the outer receipt");
    loadScenario = 0;
    Check(LC::Load(image, false, &PoolTestHost::Owner, &receipt) == ClockResult::Ok && loadCalls == 2, "scoped entry gate releases after normal return");
    for (unsigned failure = 0; failure != 9; ++failure)
    {
        reset();
        switch (failure)
        {
            case 0:
                fixture.Global(LPF::RootRva, std::uintptr_t{0});
                break;
            case 1:
                fixture.Global(NativeStartupOwnerProfile::gameModeClassCacheRva, std::uintptr_t{0});
                break;
            case 2:
                fixture.Put(fixture.graph.game.data(), 0x2c0, std::uintptr_t{0});
                break;
            case 3:
                fixture.Put(fixture.graph.level.data(), 0xd0, std::uintptr_t{0});
                break;
            case 4:
                fixture.graph.world[LPF::WorldModeOffset] = 3;
                break;
            case 5:
                fixture.Global(NativeGameStateProfile::stateRva, std::uint32_t{7});
                break;
            case 6:
                fixture.Global(NativeBorrowedGcProfile::markerRva, LONG{1});
                break;
            case 7:
                fixture.Put(fixture.graph.items.data(), 4 * 24 + 8, std::uint32_t{0x10000000});
                break;
            case 8:
                fixture.Global(NativeStartGameProfile::ThreadIdRva, static_cast<DWORD>(GetCurrentThreadId() + 1));
                break;
        }
        Check(LC::Load(image, false, &PoolTestHost::Owner, &receipt) != ClockResult::Ok && !receipt.entered && !loadCalls,
              "missing typed prerequisites, unsupported mode, wrong thread or retired raw member refuse before native entry");
    }
    reset();
    ClockResult foreign = ClockResult::Ok;
    std::thread other([&] { foreign = LC::Load(image, false, &PoolTestHost::Owner, &receipt); });
    other.join();
    Check(foreign != ClockResult::Ok && !loadCalls && !receipt.entered, "foreign thread cannot borrow the actual callback's loading authority");
    for (const auto& pin : LPF::Code)
    {
        reset();
        const auto bad = static_cast<unsigned char>(pin.bytes[0] ^ 1);
        fixture.transform.image.Patch(pin.rva, &bad, 1, PAGE_EXECUTE_READ);
        Check(LC::Load(image, false, &PoolTestHost::Owner, &receipt) != ClockResult::Ok && !receipt.entered && !loadCalls,
              "native loading/root/world/callback code pin changes refuse before entry");
        fixture.transform.image.Patch(pin.rva, pin.bytes, 1, PAGE_EXECUTE_READ);
    }
    // Cover actual copy failure before admission and after accepted effects;
    // the broad request scaffold's historical failures are not rerun here.
    reset();
    PoolTestHost::failAt = 1;
    Check(LC::Load(image, false, &PoolTestHost::Owner, &receipt) != ClockResult::Ok && !receipt.entered && !loadCalls, "first guarded copy refuses cleanly");
    reset();
    Check(LC::Load(image, false, &PoolTestHost::Owner, &receipt) == ClockResult::Ok, "entry gate also releases after preflight failure");
    std::puts("Production native streaming load-all primitive fixture completed");
    std::printf("Passed %u native streaming-loadall checks (64-bit; no native engine code executed)\n", checks);
    return 0;
}
