/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Real rebuild admission over owned native-shaped pages and readers
 ****************************************************************************/
#include <windows.h>
namespace PedRebuildReadTrace
{
    BOOL WINAPI Read(HANDLE, LPCVOID, LPVOID, SIZE_T, SIZE_T*);
}
// Instrument only the inherited host's final OS read. Its original fixture
// intentionally replaces this macro with PoolTestHost::Read immediately
// after defining that host; the one expected redefinition is test-local.
#pragma warning(push)
#pragma warning(disable : 4005)
#define ReadProcessMemory PedRebuildReadTrace::Read
#pragma push_macro("main")
#undef main
#define main HistoricalPedClothesPrimitiveDefinitionsMain
#include "NativePedClothesCallsTests.cpp"
#undef main
#pragma pop_macro("main")
#undef ReadProcessMemory
#pragma warning(pop)
#include "NativePedRebuildFixture.h"
#include "NativePedRebuildTestHost.h"
#include <cmath>
#include <thread>
#include <map>
#include <set>
#include <tuple>

namespace PedRebuildReadTrace
{
    using Key = std::tuple<std::uintptr_t, std::size_t, bool>;
    bool                                         active = false, entered = false;
    unsigned                                     observations = 0;
    std::map<Key, std::pair<unsigned, unsigned>> ordinals;
    BOOL WINAPI                                  Read(HANDLE process, LPCVOID input, LPVOID output, SIZE_T bytes, SIZE_T* copied)
    {
        if (active)
        {
            ++observations;
            auto& selected = ordinals[Key{reinterpret_cast<std::uintptr_t>(input), bytes, entered}];
            if (!selected.first)
                selected.first = PoolTestHost::reads;
            selected.second = PoolTestHost::reads;
        }
        // Preserve every real OS copy and all production page/identity guards.
        return ::ReadProcessMemory(process, input, output, bytes, copied);
    }
}

namespace
{
    ULONGLONG rebuildPhaseStart = 0;
    void      RebuildPhase(const char* phase, unsigned current = 0, unsigned total = 0) noexcept
    {
        // Expose bounded fixture phases and selected observation bookends
        // even when a deadline terminates buffered child output.
        std::printf("rebuild-phase=%s current=%u total=%u reads=%u owners=%u elapsed-ms=%llu\n", phase, current, total, PoolTestHost::reads,
                    PoolTestHost::ownerCalls, static_cast<unsigned long long>(GetTickCount64() - rebuildPhaseStart));
        std::fflush(stdout);
    }
    namespace RB = mta::game_de::NativePedRebuildCalls;
    namespace RBP = mta::game_de::NativePedRebuildProfile;
    namespace RBF = NativePedRebuildFixture;
    struct RebuildFixture : ClothesFixture
    {
        RBF::State state;
        explicit RebuildFixture(const std::filesystem::path& directory) : ClothesFixture(directory)
        {
            Check(state.Ready(), "own real-shaped intelligence/task storage independently of the native ped and descriptor");
            Reset();
        }
        void Reset()
        {
            ClothesFixture::Reset();
            // The inherited real model-reference map already has positive
            // key0 row0. Reuse that current owner and model/context graph.
            state.Reset(transform.image.image, Address(transform.pool.objects), Address(metadata), Address(items), &SpecialFixture::Patch, this);
            PoolTestHost::reads = PoolTestHost::failAt = PoolTestHost::ownerCalls = PoolTestHost::revokeAt = 0;
            PoolTestHost::ownerResult = ClockResult::Ok;
            PoolTestHost::watched = 0;
            PoolTestHost::after = nullptr;
        }
    };
    RebuildFixture* activeRebuild = nullptr;
    unsigned        rebuildCalls = 0, rebuildEffects = 0, rebuildScenario = 0;
    ClockResult     nestedRebuild = ClockResult::Ok, nestedClothesRebuild = ClockResult::Ok;
    RB::Ped*        mutableRebuildCaller = nullptr;
    ClockResult     MutateRebuildOwner() noexcept
    {
        if (mutableRebuildCaller)
            *mutableRebuildCaller = {};
        return PoolTestHost::Owner();
    }
    ClockResult RebuildEntry(RB::NativeFunction function, std::uintptr_t ped, std::uint32_t& exception) noexcept
    {
        PedRebuildReadTrace::entered = true;
        auto& f = *activeRebuild;
        Check(reinterpret_cast<std::uintptr_t>(function) == f.transform.image.image + RBP::EntryRva &&
                  ped == reinterpret_cast<std::uintptr_t>(f.transform.pool.objects),
              "one genuine native rebuild receives exactly the current ped argument");
        ++rebuildCalls;
        ++rebuildEffects;
        f.state.Recreate(f.transform.image.image, ped);
        std::memset(f.clothes, 0x31, RBP::ClothesBytes);
        std::array<unsigned char, RBP::ClothesBytes> copied{};
        copied.fill(0x52);
        f.Global(RBP::GlobalClothesRva, copied);
        f.Put(f.metadata, RBP::ModelReferenceOffset, std::uint16_t{2});
        f.ReferenceFixture::Row(0, 4, std::uint32_t{8});
        f.Global(RBP::FirstStatRva, 17.5f);
        f.Global(RBP::SecondStatRva, 9.25f);
        f.Put(f.state.statRecord.data(), 0x20, std::uint32_t{123});
        f.PedAt(RBP::RetainedObjectOffset, reinterpret_cast<std::uintptr_t>(f.state.render[1 - f.state.active].data()));
        // Native task abort, descriptor/model/stat changes and object
        // replacement remain effects, never values restored by the adapter.
        f.Put(f.state.intelligenceObjects, 0x58, std::uintptr_t{0});
        if (rebuildScenario == 1)
        {
            exception = EXCEPTION_ACCESS_VIOLATION;
            return ClockResult::OriginalException;
        }
        if (rebuildScenario == 2)
            PoolTestHost::ownerResult = ClockResult::Expired;
        if (rebuildScenario == 3)
            f.transform.pool.flags[0] ^= 1;
        if (rebuildScenario == 4)
            f.Global(RBP::ClothesSlotRva, std::uintptr_t{0});
        if (rebuildScenario == 5)
            f.Global(SP::RootRva, std::uintptr_t{0});
        if (rebuildScenario == 6)
            f.Put(f.state.head.data(), RBP::HeadBacklinkOffset, std::uintptr_t{0});
        if (rebuildScenario == 7)
            f.PedAt(RBP::RenderOffset, std::uintptr_t{0});
        if (rebuildScenario == 8)
            f.Global(RBP::FirstStatRva, std::numeric_limits<float>::quiet_NaN());
        if (rebuildScenario == 9)
        {
            RB::Receipt next;
            const auto  current = f.Ped();
            nestedRebuild = RB::Rebuild(f.transform.image.image, current, f.playerInfo, &PoolTestHost::Owner, &next);
            Check(!next.entered, "recursive rebuild cannot enter a second native operation");
            PC::Receipt clothesReceipt;
            nestedClothesRebuild = PC::Set(f.transform.image.image, current, f.playerInfo, nullptr, nullptr, 0, &PoolTestHost::Owner, &clothesReceipt);
            Check(!clothesReceipt.entered, "clothes mutation shares the same current native completion gate");
        }
        if (rebuildScenario == 10)
            PoolTestHost::failAt = PoolTestHost::reads + 1;
        if (rebuildScenario == 11)
        {
            const auto& pin = RBP::Pins[0];
            const auto  changed = static_cast<unsigned char>(pin.bytes[0] ^ 1);
            f.transform.image.Patch(pin.rva, &changed, 1, PAGE_EXECUTE_READ);
        }
        return ClockResult::Ok;
    }
}
namespace NativePedRebuildTestHost
{
    HMODULE WINAPI Module(LPCWSTR name)
    {
        return PoolTestHost::Module(name);
    }
    BOOL WINAPI Read(HANDLE process, LPCVOID input, LPVOID output, SIZE_T bytes, SIZE_T* copied)
    {
        return PoolTestHost::Read(process, input, output, bytes, copied);
    }
    mta::game_de::ClockResult Invoke(mta::game_de::NativePedRebuildCalls::NativeFunction function, std::uintptr_t ped, std::uint32_t& exception) noexcept
    {
        return RebuildEntry(function, ped, exception);
    }
}

int main(int argc, char** argv)
{
    rebuildPhaseStart = GetTickCount64();
    RebuildPhase("before-fixture");
    Check(argc == 2 && std::filesystem::path(argv[1]).is_absolute(), "rebuild evidence belongs to the workspace-owned output directory");
    RebuildFixture fixture{std::filesystem::path(argv[1])};
    activeRebuild = &fixture;
    RebuildPhase("fixture-ready");
    const auto reset = [&]
    {
        fixture.Reset();
        rebuildCalls = rebuildEffects = rebuildScenario = 0;
        nestedRebuild = nestedClothesRebuild = ClockResult::Ok;
        mutableRebuildCaller = nullptr;
    };
    const auto run = [&](RB::Receipt& receipt)
    {
        const auto ped = fixture.Ped();
        return RB::Rebuild(fixture.transform.image.image, ped, fixture.playerInfo, &PoolTestHost::Owner, &receipt);
    };
    reset();
    RB::Receipt receipt;
    RebuildPhase("first-entry-before");
    Check(run(receipt) == ClockResult::Ok && receipt.entered && receipt.returned && rebuildCalls == 1 && rebuildEffects == 1 &&
              receipt.beforeRender != receipt.afterRender && receipt.beforeContainer != receipt.afterContainer &&
              receipt.afterRender == fixture.state.Render() && receipt.afterContainer == fixture.state.Container() &&
              receipt.preservedHead == fixture.state.Head(),
          "actual native primitive accepts a legitimate new render/container while retaining the existing native list head");
    RebuildPhase("first-entry-after");
    Check(fixture.clothes[0] == 0x31 && fixture.Get<std::uint16_t>(fixture.metadata, RBP::ModelReferenceOffset) == 2 &&
              fixture.ReferenceFixture::Count(0) == 8 && fixture.Get<std::uint32_t>(fixture.state.statRecord.data(), 0x20) == 123,
          "the native descriptor, model reference and stat effects remain changed without adapter rollback");
    reset();
    fixture.state.Secondary(fixture.transform.image.image);
    Check(run(receipt) == ClockResult::Ok && rebuildCalls == 1 && !fixture.Get<std::uintptr_t>(fixture.state.intelligenceObjects, 0x58),
          "actual supported secondary-task admission accepts its native removal during rebuild");

    RebuildPhase("invalid-prerequisites");
    for (unsigned fault = 0; fault != 14; ++fault)
    {
        reset();
        if (fault == 0)
            fixture.PedAt(NativePedMetadataProfile::ModelOffset, std::uint16_t{7});
        if (fault == 1)
            fixture.Global(IP::PlayerIndexRva, static_cast<unsigned char>(1));
        if (fault == 2)
            fixture.PedAt(0x5f0, reinterpret_cast<std::uintptr_t>(fixture.clothes));
        if (fault == 3)
            fixture.Global(NativeModelLoadedProfile::ArrayRva + NativeModelLoadedProfile::StateOffset, static_cast<unsigned char>(0));
        if (fault == 4)
            fixture.Global(NativeModelBankProfile::BankRva, std::uintptr_t{0});
        if (fault == 5)
            fixture.Put(fixture.metadata, RBP::ModelReferenceOffset, std::uint16_t{0});
        if (fault == 6)
            fixture.ReferenceFixture::Row(0, 4, std::uint32_t{0});
        if (fault == 7)
            fixture.Put(fixture.state.render[fixture.state.active].data(), RBP::ContainerOffset, std::uintptr_t{0});
        if (fault == 8)
            fixture.Put(fixture.state.containers[fixture.state.active].data(), RBP::ContainerOwnerOffset, std::uintptr_t{0});
        if (fault == 9)
            fixture.Put(fixture.state.head.data(), RBP::HeadBacklinkOffset, std::uintptr_t{0});
        if (fault == 10)
            fixture.Global(RBP::FirstStatRva, std::numeric_limits<float>::infinity());
        if (fault == 11)
            fixture.state.intelligenceFlags[0] |= 0x80;
        if (fault == 12)
        {
            fixture.state.Secondary(fixture.transform.image.image);
            fixture.state.taskFlags[0] |= 0x80;
        }
        if (fault == 13)
            fixture.playerInfo.Reset();
        Check(run(receipt) != ClockResult::Ok && !receipt.entered && !rebuildCalls && !rebuildEffects,
              "a missing local/model/loaded/reference/intelligence/task or concrete render-list prerequisite refuses before native effects");
    }
    reset();
    DWORD oldProtection = 0;
    Check(VirtualProtect(fixture.clothes, 4096, PAGE_READONLY, &oldProtection) != FALSE, "make the separately owned descriptor read-only");
    Check(run(receipt) != ClockResult::Ok && !receipt.entered && !rebuildCalls, "read-only descriptor storage cannot authorize a mutating native rebuild");
    Check(VirtualProtect(fixture.clothes, 4096, oldProtection, &oldProtection) != FALSE, "restore the descriptor's owned writable protection");

    RebuildPhase("code-pins");
    for (const auto& pin : RBP::Pins)
    {
        reset();
        const auto changed = static_cast<unsigned char>(pin.bytes[0] ^ 1);
        fixture.transform.image.Patch(pin.rva, &changed, 1, PAGE_EXECUTE_READ);
        Check(run(receipt) != ClockResult::Ok && !receipt.entered && !rebuildCalls, "changed genuine rebuild/caller/producer code refuses before entry");
    }
    RebuildPhase("entered-scenarios");
    for (unsigned scenario = 1; scenario <= 11; ++scenario)
    {
        reset();
        rebuildScenario = scenario;
        const auto result = run(receipt);
        if (scenario == 9)
            Check(result == ClockResult::Ok && nestedRebuild == ClockResult::Busy && nestedClothesRebuild == ClockResult::Busy && rebuildCalls == 1,
                  "the existing shared completion gate prevents recursive rebuild and clothes native entry");
        else
            Check(result != ClockResult::Ok && receipt.entered && rebuildCalls == 1 && rebuildEffects == 1 && fixture.clothes[0] == 0x31,
                  "entered native exception or failed postflight retains one prefix and native effects with no retry or rollback");
    }

    RebuildPhase("measure");
    reset();
    auto ped = fixture.Ped();
    PoolTestHost::reads = PoolTestHost::ownerCalls = 0;
    PedRebuildReadTrace::ordinals.clear();
    PedRebuildReadTrace::observations = 0;
    PedRebuildReadTrace::entered = false;
    PedRebuildReadTrace::active = true;
    Check(RB::Rebuild(fixture.transform.image.image, ped, fixture.playerInfo, &PoolTestHost::Owner, &receipt) == ClockResult::Ok,
          "measure only this new primitive's complete successful read and owner sequence");
    PedRebuildReadTrace::active = false;
    const auto         reads = PoolTestHost::reads, ownerCalls = PoolTestHost::ownerCalls;
    std::set<unsigned> readFaults;
    for (const auto& observed : PedRebuildReadTrace::ordinals)
    {
        readFaults.insert(observed.second.first);
        readFaults.insert(observed.second.second);
    }
    Check(PedRebuildReadTrace::observations == reads && !readFaults.empty(),
          "the test transport records every real baseline read before choosing distinct address/size/entry-phase bookends");
    std::printf("rebuild-read-schedule baseline=%u keys=%zu faults=%zu owners=%u\n", reads, PedRebuildReadTrace::ordinals.size(), readFaults.size(),
                ownerCalls);
    std::fflush(stdout);
    // Unchanged dependency readers have their own historical exhaustive
    // matrices. This new primitive exercises the first/last consumption of
    // each distinct native observation in both pre-entry and post-entry phases,
    // instead of replaying every identical nested dependency prefix.
    unsigned selectedFault = 0;
    RebuildPhase("read-sweep", 0, static_cast<unsigned>(readFaults.size()));
    for (const auto fault : readFaults)
    {
        ++selectedFault;
        if (selectedFault == 1 || selectedFault % 64 == 0 || selectedFault == readFaults.size())
            RebuildPhase("read-sweep", selectedFault, static_cast<unsigned>(readFaults.size()));
        reset();
        ped = fixture.Ped();
        PoolTestHost::reads = PoolTestHost::ownerCalls = 0;
        PoolTestHost::failAt = fault;
        Check(RB::Rebuild(fixture.transform.image.image, ped, fixture.playerInfo, &PoolTestHost::Owner, &receipt) != ClockResult::Ok && rebuildCalls <= 1 &&
                  rebuildEffects == rebuildCalls,
              "each distinct observation or confirmation-bookend failure refuses without a second native entry or replay");
    }
    RebuildPhase("owner-sweep", 0, ownerCalls);
    for (unsigned fault = 1; fault <= ownerCalls; ++fault)
    {
        if (fault == 1 || fault % 64 == 0 || fault == ownerCalls)
            RebuildPhase("owner-sweep", fault, ownerCalls);
        reset();
        ped = fixture.Ped();
        PoolTestHost::reads = PoolTestHost::ownerCalls = 0;
        PoolTestHost::revokeAt = fault;
        Check(RB::Rebuild(fixture.transform.image.image, ped, fixture.playerInfo, &PoolTestHost::Owner, &receipt) != ClockResult::Ok && rebuildCalls <= 1,
              "each owner refusal stops its current attempt without retrying a completed native prefix");
    }
    reset();
    ped = fixture.Ped();
    RebuildPhase("copied-caller");
    mutableRebuildCaller = &ped;
    Check(RB::Rebuild(fixture.transform.image.image, ped, fixture.playerInfo, &MutateRebuildOwner, &receipt) == ClockResult::Ok && !ped.nativePed &&
              rebuildCalls == 1,
          "the primitive owns its copied ped intent before the first owner callback");
    reset();
    ped = fixture.Ped();
    RebuildPhase("foreign-thread");
    ClockResult foreign = ClockResult::Ok;
    std::thread worker([&] { foreign = RB::Rebuild(fixture.transform.image.image, ped, fixture.playerInfo, &PoolTestHost::Owner, &receipt); });
    worker.join();
    Check(foreign != ClockResult::Ok && !receipt.entered && !rebuildCalls, "a foreign thread cannot borrow current game-thread native ownership");
    RebuildPhase("completed");
    std::puts("Production native ped rebuild primitive fixture completed");
    std::printf("Passed %u native ped-rebuild checks (%zu-bit; no native engine code executed)\n", checks, sizeof(void*) * 8);
    return 0;
}
