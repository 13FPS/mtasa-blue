/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Only associated-fire presence over actual owned ped pool/image pages
 ****************************************************************************/
#pragma push_macro("main")
#undef main
#define main NativePedFireHistoricalAreaMain
#include "NativePedAreaTests.cpp"
#undef main
#pragma pop_macro("main")
#include "../../Client/game_de/NativePedFireMemory.h"
#include "../../Client/game_de/NativePedFireProfile.h"
#define ReadProcessMemory PoolTestHost::Read
#include "../../Client/game_de/NativePedFireMemory.cpp"
#undef ReadProcessMemory
namespace
{
    namespace FireP = NativePedFireProfile;
    using FireMemory = NativePedFireMemory;
    struct FireFixture : AreaFixture
    {
        explicit FireFixture(const std::filesystem::path& path) : AreaFixture(path)
        {
            for (const auto& pin : FireP::Code)
                transform.image.Patch(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
            const auto group = transform.image.image + FireP::GroupRva;
            const auto selected = static_cast<std::uint8_t>(FireP::Selector);
            const auto branch = static_cast<std::uint32_t>(FireP::BranchRva);
            transform.image.Patch(FireP::GroupSlotRva, &group, sizeof(group), PAGE_READONLY);
            transform.image.Patch(FireP::RemapRva + FireP::OpcodeIndex, &selected, 1, PAGE_EXECUTE_READ);
            transform.image.Patch(FireP::DispatchRva + FireP::Selector * 4, &branch, sizeof(branch), PAGE_EXECUTE_READ);
            Reset();
        }
        void Reset()
        {
            AreaFixture::Reset();
            Store(0, FireP::FireOffset, std::uintptr_t{0});
            Store(8, FireP::FireOffset, std::uintptr_t{1});
        }
        std::uintptr_t Address(std::size_t slot = 0)
        {
            return reinterpret_cast<std::uintptr_t>(transform.pool.objects) + slot * NativePedPoolProfile::ObjectStride + FireP::FireOffset;
        }
    };
    FireFixture* changingFire = nullptr;
    void         ReplaceFire()
    {
        changingFire->Store(0, FireP::FireOffset, std::uintptr_t{0xdeadbeef});
    }
    void RetireFirePed()
    {
        changingFire->transform.pool.flags[0] = 2;
    }
}
int main(int argc, char** argv)
{
    Check(argc == 2 && std::filesystem::path(argv[1]).is_absolute(), "fire evidence uses explicit workspace output");
    FireMemory::Binding wrong{123};
    Check(!FireMemory::Qualify(wrong) && wrong.image == 123, "wrong executable cannot qualify native fire association");
    FireFixture fireFixture(argv[1]);
    changingFire = &fireFixture;
    PoolTestHost::image = fireFixture.transform.image.image;
    const FireMemory::Binding fireBinding{PoolTestHost::image};
    FireMemory                fire(fireBinding, &PoolTestHost::Owner), unownedFire(fireBinding, nullptr);
    Check(FireMemory::Revalidate(fireBinding), "named0326 producer, direct target clear and commonped initializer qualify associated-fire role");
    auto firePed = fireFixture.Ped();
    AreaHost::Reset();
    bool present = true;
    Check(unownedFire.ReadOnFire(firePed, present) == ClockResult::NotInitialized && present, "native pointer presence is not callback authority");
    for (const auto handle : {std::uint32_t{1}, std::uint32_t{0x87f}})
        for (const auto bits : {std::uintptr_t{0}, std::uintptr_t{1}, std::uintptr_t{0xdeadbeef}, std::uintptr_t{0x8000000000000000ull},
                                (std::numeric_limits<std::uintptr_t>::max)()})
        {
            fireFixture.Reset();
            firePed = fireFixture.Ped(handle);
            AreaHost::Reset();
            fireFixture.Store(firePed.slot, FireP::FireOffset, bits);
            present = bits == 0;
            Check(fire.ReadOnFire(firePed, present) == ClockResult::Ok && present == (bits != 0) && !AreaHost::exchanges,
                  "actual constCPed semantics preserve pointer presence on both classes, including non-dereferenceable nonzero bits, without fire mutation");
        }
    fireFixture.Reset();
    firePed = fireFixture.Ped();
    AreaHost::Reset();
    present = true;
    Check(fire.ReadOnFire(firePed, present) == ClockResult::Ok && !present, "measure only current fire observation boundaries");
    const auto fireReads = PoolTestHost::reads, fireOwners = PoolTestHost::ownerCalls;
    for (unsigned fail = 1; fail <= fireReads; ++fail)
    {
        fireFixture.Reset();
        firePed = fireFixture.Ped();
        AreaHost::Reset();
        PoolTestHost::failAt = fail;
        present = true;
        Check(fire.ReadOnFire(firePed, present) != ClockResult::Ok && present && !AreaHost::exchanges,
              "every failed actual read preserves output rather than inventing no fire");
    }
    for (unsigned revoke = 1; revoke <= fireOwners; ++revoke)
    {
        fireFixture.Reset();
        firePed = fireFixture.Ped();
        AreaHost::Reset();
        PoolTestHost::revokeAt = revoke;
        present = true;
        Check(fire.ReadOnFire(firePed, present) == ClockResult::WrongThread && present,
              "each actual owner boundary may revoke without publishing stale association");
    }
    for (const auto action : {&ReplaceFire, &RetireFirePed})
    {
        fireFixture.Reset();
        firePed = fireFixture.Ped();
        AreaHost::Reset();
        PoolTestHost::watched = fireFixture.Address();
        PoolTestHost::after = action;
        present = true;
        Check(fire.ReadOnFire(firePed, present) != ClockResult::Ok && present && !PoolTestHost::after,
              "association replacement or occupied ped retirement refuses mixed pointer identity");
    }
    fireFixture.Reset();
    firePed = fireFixture.Ped();
    AreaHost::Reset();
    fireFixture.Store(0, FireP::FireOffset, std::uintptr_t{1});
    PoolTestHost::watched = fireFixture.Address();
    PoolTestHost::after = &ReplaceFire;
    present = false;
    Check(fire.ReadOnFire(firePed, present) == ClockResult::OwnerConflict && !present,
          "nonnull-to-nonnull replacement is still a changed association, not a stable true fallback");
    for (const auto state : {std::uint32_t{6}, std::uint32_t{7}, std::uint32_t{8}})
    {
        fireFixture.Reset();
        firePed = fireFixture.Ped();
        AreaHost::Reset();
        fireFixture.Global(NativeGameStateProfile::stateRva, state);
        present = true;
        Check(fire.ReadOnFire(firePed, present) == ClockResult::NotInitialized && present, "frontend/loading state cannot borrow ped fire association");
    }
    fireFixture.Reset();
    firePed = fireFixture.Ped();
    AreaHost::Reset();
    fireFixture.Global(NativeBorrowedGcProfile::markerRva, LONG{1});
    present = true;
    Check(fire.ReadOnFire(firePed, present) != ClockResult::Ok && present, "actual native GC excludes fire observation");
    fireFixture.Reset();
    firePed = fireFixture.Ped();
    AreaHost::Reset();
    ClockResult fireForeign = ClockResult::Ok;
    present = true;
    std::thread fireThread([&] { fireForeign = fire.ReadOnFire(firePed, present); });
    fireThread.join();
    Check(fireForeign == ClockResult::WrongThread && present, "callback cannot replace native game-thread affinity");
    for (const auto& pin : FireP::Code)
    {
        fireFixture.Reset();
        firePed = fireFixture.Ped();
        AreaHost::Reset();
        const auto changed = static_cast<unsigned char>(pin.bytes[0] ^ 1u);
        fireFixture.transform.image.Patch(pin.rva, &changed, 1, PAGE_EXECUTE_READ);
        present = true;
        Check(fire.ReadOnFire(firePed, present) != ClockResult::Ok && present, "every qualified producer/clear/init code pin is required");
        fireFixture.transform.image.Patch(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
    }
    fireFixture.Reset();
    firePed = fireFixture.Ped();
    AreaHost::Reset();
    const auto changed = static_cast<std::uint8_t>(FireP::Selector + 1);
    fireFixture.transform.image.Patch(FireP::RemapRva + FireP::OpcodeIndex, &changed, 1, PAGE_EXECUTE_READ);
    present = true;
    Check(fire.ReadOnFire(firePed, present) != ClockResult::Ok && present, "changed exact named sparse selection refuses fire-field identity");
    const auto selected = static_cast<std::uint8_t>(FireP::Selector);
    fireFixture.transform.image.Patch(FireP::RemapRva + FireP::OpcodeIndex, &selected, 1, PAGE_EXECUTE_READ);
    fireFixture.Reset();
    firePed = fireFixture.Ped();
    AreaHost::Reset();
    PoolTestHost::image = 0;
    present = true;
    Check(fire.ReadOnFire(firePed, present) != ClockResult::Ok && present, "main-image loss never fabricates fire absence");
    std::puts("Production native ped fire association predicate fixture completed");
    std::printf("Passed %u native ped fire query checks (64-bit; no native engine code executed)\n", checks);
    return 0;
}
