/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Branch-sensitive whole ClearWeapon admission, no native engine execution
 ****************************************************************************/
#pragma push_macro("main")
#undef main
#define main NativeClearHistoricalGrantMain
#include "NativePedWeaponGrantCallsTests.cpp"
#undef main
#pragma pop_macro("main")
#include "NativePedWeaponClearFixture.h"
#include "../../Client/game_de/NativePedWeaponClearCalls.h"
namespace
{
    namespace WC = mta::game_de::NativePedWeaponClearCalls;
    ClockResult ClearEntry(WC::NativeFunction, std::uintptr_t, std::uint32_t, std::uint32_t&) noexcept;
}
#define GetModuleHandleW                      PoolTestHost::Module
#define ReadProcessMemory                     PoolTestHost::Read
#define MTA_DE_PED_WEAPON_CLEAR_NATIVE_INVOKE ::ClearEntry
#include "../../Client/game_de/NativePedWeaponClearCalls.cpp"
#undef MTA_DE_PED_WEAPON_CLEAR_NATIVE_INVOKE
#undef ReadProcessMemory
#undef GetModuleHandleW
namespace
{
    namespace WP = NativePedWeaponClearProfile;
    struct ClearFixture : GrantFixture
    {
        unsigned char* modelPages = static_cast<unsigned char*>(VirtualAlloc(nullptr, 4096, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        explicit ClearFixture(const std::filesystem::path& directory) : GrantFixture(directory)
        {
            Check(modelPages != nullptr, "own immediate native-shaped model WORD storage");
            Reset();
        }
        ~ClearFixture() { VirtualFree(modelPages, 0, MEM_RELEASE); }
        void Reset()
        {
            DWORD previous = 0;
            Check(VirtualProtect(modelPages, 4096, PAGE_READWRITE, &previous) != 0, "restore only owned model-page permissions");
            std::memset(modelPages, 0, 4096);
            GrantFixture::Reset();
            NativePedWeaponClearFixture::Install(transform.image.image, &LoadFixture::Write, this);
            reader.Reset();
            Check(reader.Bind() == Status::Ok, "reuse actual bound weapon reader after Clear fixture installation");
        }
        std::uintptr_t ModelAddress(unsigned n) const { return reinterpret_cast<std::uintptr_t>(modelPages + n * 0x40); }
        void           Word(unsigned n, std::uint16_t value) { std::memcpy(modelPages + n * 0x40 + 0x22, &value, sizeof(value)); }
        std::uint16_t  Word(unsigned n) const
        {
            std::uint16_t value = 0;
            std::memcpy(&value, modelPages + n * 0x40 + 0x22, sizeof(value));
            return value;
        }
    };
    ClearFixture* activeClear = nullptr;
    WC::Ped*      mutableClearCaller = nullptr;
    unsigned      clearCalls = 0, clearEffects = 0, clearScenario = 0;
    std::uint32_t clearType = 22;
    ClockResult   nestedClear = ClockResult::Ok, nestedClearGrant = ClockResult::Ok, nestedClearLoad = ClockResult::Ok;
    bool          nestedClearMutation = true;
    ClockResult   MutatingClearOwner() noexcept
    {
        if (mutableClearCaller)
            *mutableClearCaller = {};
        return PoolTestHost::Owner();
    }
    ClockResult ClearEntry(WC::NativeFunction function, std::uintptr_t ped, std::uint32_t type, std::uint32_t& exception) noexcept
    {
        auto&      f = *activeClear;
        const auto image = f.transform.image.image;
        Check(reinterpret_cast<std::uintptr_t>(function) == image + WP::EntryRva && ped == reinterpret_cast<std::uintptr_t>(f.transform.pool.objects) &&
                  type == clearType,
              "one exact native Clear receives copied registered ped and unchanged full type; no return pointer is published");
        ++clearCalls;
        ++clearEffects;
        f.application[0x79] = static_cast<unsigned char>(clearEffects);
        if (clearScenario == 1)
        {
            exception = EXCEPTION_ACCESS_VIOLATION;
            return ClockResult::OriginalException;
        }
        if (clearScenario == 2)
            PoolTestHost::ownerResult = ClockResult::Expired;
        if (clearScenario == 3)
            f.Global(LPF::RootRva, std::uintptr_t{0});
        if (clearScenario == 4)
            f.reader.Reset();
        if (clearScenario == 5)
            f.transform.pool.flags[0] ^= 1;
        if (clearScenario == 6)
            f.Put(f.graph.items.data(), 0x10, std::uint32_t{999});
        if (clearScenario == 7)
        {
            const auto& pin = WP::Pins[0];
            const auto  bad = static_cast<unsigned char>(pin.bytes[0] ^ 1);
            f.transform.image.Patch(pin.rva, &bad, 1, PAGE_EXECUTE_READ);
        }
        if (clearScenario == 8)
        {
            // This represents the native prefix's legitimate selected-row,
            // current-slot and model-release effects, not adapter emulation.
            f.PedAt(0x7f0 + 2 * 0x28, std::uint32_t{0});
            f.PedAt(WP::CurrentSlotOffset, static_cast<signed char>(0));
            f.Word(0, static_cast<std::uint16_t>(f.Word(0) - 1));
            f.Word(1, static_cast<std::uint16_t>(f.Word(1) - 1));
            f.Model(100, 0, false);
            f.Model(101, 0, false);
            f.graph.world[LPF::WorldModeOffset] = 1;
        }
        if (clearScenario == 9)
        {
            const auto  current = f.Ped();
            WC::Receipt nested;
            nestedClear = WC::Clear(image, current, f.reader, type, &PoolTestHost::Owner, &nested);
            Check(!nested.entered, "recursive Clear has no entered prefix");
            WG::Receipt   grant;
            std::uint32_t slot = 123;
            nestedClearGrant = WG::Grant(image, current, f.reader, type, 1, slot, &PoolTestHost::Owner, &grant);
            Check(!grant.entered && slot == 123, "Grant cannot enter during Clear or publish a slot");
            LC::Receipt load;
            nestedClearLoad = LC::Load(image, false, &PoolTestHost::Owner, &load);
            NativePedWeaponContext::Mutation mutationScope;
            nestedClearMutation = static_cast<bool>(mutationScope);
        }
        if (clearScenario == 10)
            PoolTestHost::failAt = PoolTestHost::reads + 1;
        return ClockResult::Ok;
    }
}
int main(int argc, char** argv)
{
    Check(argc == 2 && std::filesystem::path(argv[1]).is_absolute(), "Clear evidence stays in an explicit workspace-owned directory");
    ClearFixture fixture(argv[1]);
    activeClear = &fixture;
    const auto image = fixture.transform.image.image;
    PoolTestHost::image = image;
    WC::Receipt receipt;
    const auto  reset = [&]
    {
        fixture.Reset();
        clearCalls = clearEffects = clearScenario = 0;
        clearType = 22;
        mutableClearCaller = nullptr;
        nestedClear = nestedClearGrant = nestedClearLoad = ClockResult::Ok;
        nestedClearMutation = true;
    };
    reset();
    auto ped = fixture.Ped();
    Check(WC::Clear(image, ped, fixture.reader, 22, nullptr, &receipt) == ClockResult::NotInitialized && !receipt.entered && !clearCalls,
          "a resolved ped and weapon reader do not create current engine authority");
    for (unsigned path = 0; path != 5; ++path)
    {
        reset();
        ped = fixture.Ped();
        if (path == 0)
        {
            fixture.Info(22, -1, 20000, -2);
            fixture.PedAt(WP::CurrentSlotOffset, static_cast<signed char>(-128));
        }
        if (path >= 2)
            fixture.PedAt(0x7f0 + 2 * 0x28, std::uint32_t{22});
        if (path == 2)
            fixture.PedAt(WP::CurrentSlotOffset, static_cast<signed char>(-1));
        if (path == 4)
            fixture.Info(0, -1);
        Check(WC::Clear(image, ped, fixture.reader, 22, &PoolTestHost::Owner, &receipt) == ClockResult::Ok && receipt.entered && receipt.returned &&
                  clearCalls == 1,
              "STD slot-minus-one, mismatched row, noncurrent match, current match and unarmed-slot-minus-one all enter genuine native exactly once");
        Check(receipt.matched == (path >= 2) && receipt.maySwitch == (path >= 3) && !receipt.mayAttach,
              "receipt reflects only qualified branch readiness, not a fabricated native success result");
    }
    reset();
    ped = fixture.Ped();
    fixture.PedAt(0x7f0 + 2 * 0x28, std::uint32_t{22});
    fixture.PedAt(WP::CurrentSlotOffset, static_cast<signed char>(3));
    fixture.Info(22, 2, 100, 101);
    fixture.Model(100, fixture.ModelAddress(0), false);
    fixture.Model(101, fixture.ModelAddress(1), false);
    fixture.Word(0, 0);
    fixture.Word(1, 65535);
    Check(WC::Clear(image, ped, fixture.reader, 22, &PoolTestHost::Owner, &receipt) == ClockResult::Ok && receipt.matched && !receipt.maySwitch &&
              clearCalls == 1,
          "decrement-only admission accepts actual RW model WORDs with raw zero/ffff refs and unloaded stream state, without clamping or Grant preloads");
    reset();
    ped = fixture.Ped();
    fixture.PedAt(0x7f0 + 2 * 0x28, std::uint32_t{22});
    fixture.Info(22, 2, 100, 101);
    fixture.Model(100, fixture.ModelAddress(0), false);
    fixture.Model(101, fixture.ModelAddress(1), false);
    fixture.Word(0, 0);
    fixture.Word(1, 2);
    clearScenario = 8;
    fixture.graph.world[LPF::WorldModeOffset] = 2;
    Check(WC::Clear(image, ped, fixture.reader, 22, &PoolTestHost::Owner, &receipt) == ClockResult::Ok && receipt.maySwitch && clearCalls == 1 &&
              fixture.Word(0) == 65535 && fixture.Word(1) == 1,
          "legitimate Clear row/current-slot/ref/model-release and world-mode transitions remain successful and preserved after native entry");
    for (bool loaded : {false, true})
    {
        reset();
        ped = fixture.Ped();
        fixture.PedAt(0x7f0 + 2 * 0x28, std::uint32_t{22});
        fixture.PedAt(0x7f0, std::uint32_t{1});
        fixture.Info(1, 0, 100);
        fixture.Model(100, fixture.ModelAddress(0), loaded);
        const auto result = WC::Clear(image, ped, fixture.reader, 22, &PoolTestHost::Owner, &receipt);
        Check(loaded ? result == ClockResult::Ok && receipt.mayAttach && clearCalls == 1 : result != ClockResult::Ok && !receipt.entered && !clearCalls,
              "only a genuine nonzero destination attachment requires its model1 preloaded; admission never requests it");
    }
    for (unsigned scenario : {1u, 2u, 3u, 4u, 5u, 6u, 7u, 10u})
    {
        reset();
        ped = fixture.Ped();
        clearScenario = scenario;
        Check(WC::Clear(image, ped, fixture.reader, 22, &PoolTestHost::Owner, &receipt) != ClockResult::Ok && receipt.entered && clearCalls == 1 &&
                  clearEffects == 1 && fixture.application[0x79] == 1,
              "entered exception or lost authority preserves effects and stops without replay or rollback");
        if (scenario == 1)
            Check(receipt.exceptionCode == EXCEPTION_ACCESS_VIOLATION && !receipt.returned, "native exception receipt is exact");
    }
    reset();
    ped = fixture.Ped();
    clearScenario = 9;
    Check(WC::Clear(image, ped, fixture.reader, 22, &PoolTestHost::Owner, &receipt) == ClockResult::Ok && clearCalls == 1 && nestedClear == ClockResult::Busy &&
              nestedClearGrant == ClockResult::Busy && nestedClearLoad == ClockResult::Busy && !nestedClearMutation,
          "Clear reuses both existing shared native and weapon-mutation gates");
    for (std::uint32_t type : {47u, 0xffffffffu, 0x10016u})
    {
        reset();
        ped = fixture.Ped();
        Check(WC::Clear(image, ped, fixture.reader, type, &PoolTestHost::Owner, &receipt) == ClockResult::InvalidArgument && !receipt.entered && !clearCalls,
              "unsupported full types refuse before signed-low-WORD indexing without aliasing or truncation");
    }
    for (unsigned failure = 0; failure != 12; ++failure)
    {
        reset();
        ped = fixture.Ped();
        fixture.PedAt(0x7f0 + 2 * 0x28, std::uint32_t{22});
        fixture.PedAt(WP::CurrentSlotOffset, static_cast<signed char>(3));
        switch (failure)
        {
            case 0:
                fixture.Global(LPF::RootRva, std::uintptr_t{0});
                break;
            case 1:
                fixture.Global(NativeGameStateProfile::stateRva, std::uint32_t{7});
                break;
            case 2:
                fixture.Global(NativeBorrowedGcProfile::markerRva, LONG{1});
                break;
            case 3:
                PoolTestHost::ownerResult = ClockResult::Expired;
                break;
            case 4:
                fixture.transform.pool.flags[0] ^= 1;
                break;
            case 5:
                fixture.reader.Reset();
                break;
            case 6:
                fixture.Info(22, 13);
                break;
            case 7:
                fixture.Info(22, 2, 20000);
                break;
            case 8:
                fixture.Info(22, 2, -2);
                break;
            case 9:
                fixture.Info(22, 2, 100);
                fixture.Model(100, 0, false);
                break;
            case 10:
                fixture.Info(22, 2, 100);
                fixture.Model(100, 1, false);
                break;
            case 11:
            {
                fixture.Info(22, 2, 100);
                fixture.Model(100, fixture.ModelAddress(0), false);
                DWORD prior = 0;
                Check(VirtualProtect(fixture.modelPages, 4096, PAGE_READONLY, &prior) != 0, "make only owned ref storage readonly");
                break;
            }
        }
        Check(WC::Clear(image, ped, fixture.reader, 22, &PoolTestHost::Owner, &receipt) != ClockResult::Ok && !receipt.entered && !clearCalls,
              "invalid current owner/world/ped/bank/selectedslot/model-index/WORD-storage refuses before effects");
    }
    for (const auto& pin : WP::Pins)
    {
        reset();
        ped = fixture.Ped();
        const auto bad = static_cast<unsigned char>(pin.bytes[0] ^ 1);
        fixture.transform.image.Patch(pin.rva, &bad, 1, PAGE_EXECUTE_READ);
        Check(WC::Clear(image, ped, fixture.reader, 22, &PoolTestHost::Owner, &receipt) == ClockResult::UnsupportedBuild && !receipt.entered && !clearCalls,
              "whole Clear/caller/current-selector and reused immediate helper bodies remain exact executable pins");
    }
    reset();
    ped = fixture.Ped();
    mutableClearCaller = &ped;
    Check(WC::Clear(image, ped, fixture.reader, 22, &MutatingClearOwner, &receipt) == ClockResult::Ok && !ped.nativePed && clearCalls == 1,
          "snapshot resolved caller token before any owner callback mutation");
    reset();
    ped = fixture.Ped();
    ClockResult foreign = ClockResult::Ok;
    std::thread thread([&] { foreign = WC::Clear(image, ped, fixture.reader, 22, &PoolTestHost::Owner, &receipt); });
    thread.join();
    Check(foreign != ClockResult::Ok && !receipt.entered && !clearCalls, "foreign thread cannot borrow Clear authority");
    reset();
    ped = fixture.Ped();
    Check(WC::Clear(image, ped, fixture.reader, 22, &PoolTestHost::Owner, &receipt) == ClockResult::Ok && clearCalls == 1,
          "shared scopes retire on all failure paths and admit fresh work");
    std::puts("Production native ped weapon clear operation fixture completed");
    std::printf("Passed %u native ped-weapon-clear checks (64-bit; no native engine code executed)\n", checks);
    return 0;
}
