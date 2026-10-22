/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Raw current-slot BYTE reads over actual owned ped/image fixture pages
 ****************************************************************************/
#pragma push_macro("main")
#undef main
#define main NativeCurrentWeaponHistoricalFieldsMain
#include "NativePedWeaponFieldsTests.cpp"
#undef main
#pragma pop_macro("main")
#include "../../Client/game_de/NativePedCurrentWeaponSlotMemory.h"
#include "../../Client/game_de/NativePedCurrentWeaponSlotProfile.h"
#define GetModuleHandleW  PoolTestHost::Module
#define ReadProcessMemory WeaponFieldsHost::Read
#include "../../Client/game_de/NativePedCurrentWeaponSlotMemory.cpp"
#undef ReadProcessMemory
#undef GetModuleHandleW
int main(int argc, char** argv)
{
    using Memory = NativePedCurrentWeaponSlotMemory;
    namespace S = NativePedCurrentWeaponSlotProfile;
    Check(argc == 2 && std::filesystem::path(argv[1]).is_absolute(), "current-slot evidence stays in the explicit workspace directory");
    Memory::Binding refused{123};
    Check(!Memory::Qualify(refused) && refused.image == 123, "shipping current-slot binding refuses the actual non-GTA test host");
    WeaponFieldsFixture f(argv[1]);
    for (const auto& pin : S::Code)
        f.transform.image.Patch(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
    PoolTestHost::image = f.transform.image.image;
    const Memory::Binding binding{f.transform.image.image};
    Memory                memory(binding, &PoolTestHost::Owner), unowned(binding, nullptr);
    const auto            reset = [&](std::uint32_t handle = 1)
    {
        f.Reset();
        const auto ped = f.Ped(handle);
        WeaponFieldsHost::Reset();
        return ped;
    };
    auto ped = reset();
    Check(Memory::Revalidate(binding), "named query and independent constructor current-slot BYTE pins qualify");
    struct Defaults final : NativePedCurrentWeaponSlotAccess
    {
    } defaults;
    std::uint32_t output = 0xdeadbeef;
    Check(defaults.ReadPedCurrentWeaponSlot({}, output) == ClockResult::NotInitialized && output == 0xdeadbeef,
          "default timing access does not invent an unarmed slot");
    for (const auto pedSlot : {std::size_t{0}, std::size_t{8}})
        for (const auto current : {0u, 1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u, 9u, 10u, 11u, 12u, 13u, 14u, 15u, 127u, 128u, 255u})
        {
            ped = reset(pedSlot == 0 ? 1u : 0x87fu);
            f.Store(pedSlot, S::CurrentOffset, static_cast<std::uint8_t>(current));
            const auto before = WeaponFieldsSnapshot(f, pedSlot);
            unsigned   inlineReads = 0;
            WeaponFieldsHost::afterRead = [&](std::uintptr_t at, std::size_t)
            {
                if (at >= ped.nativePed + NativePedWeaponProfile::ArrayOffset &&
                    at < ped.nativePed + NativePedWeaponProfile::ArrayOffset + NativePedWeaponProfile::Count * NativePedWeaponProfile::Stride)
                    ++inlineReads;
            };
            output = 0xdeadbeef;
            Check(
                memory.ReadCurrentSlot(ped, output) == ClockResult::Ok && output == current && inlineReads == 0 && WeaponFieldsHost::exchanges == 0 &&
                    WeaponFieldsSnapshot(f, pedSlot) == before,
                "both classes preserve unsigned current BYTE including signed-invalid and legal-enum sentinel values without native array access or mutation");
        }
    ped = reset();
    Check(unowned.ReadCurrentSlot(ped, output) == ClockResult::NotInitialized, "native bytes cannot replace callback ownership");
    auto stale = ped;
    stale.generation ^= 1;
    output = 0xdeadbeef;
    Check(memory.ReadCurrentSlot(stale, output) != ClockResult::Ok && output == 0xdeadbeef, "stale ped generation preserves prior output");
    for (const auto& pin : S::Code)
    {
        ped = reset();
        const auto changed = static_cast<unsigned char>(pin.bytes[0] ^ 1);
        f.transform.image.Patch(pin.rva, &changed, 1, PAGE_EXECUTE_READ);
        output = 0xdeadbeef;
        Check(memory.ReadCurrentSlot(ped, output) == ClockResult::BindingChanged && output == 0xdeadbeef, "each exact current-byte proof pin is mandatory");
        f.transform.image.Patch(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
    }
    ped = reset();
    f.Global(NativeBorrowedGcProfile::markerRva, LONG{1});
    output = 0xdeadbeef;
    Check(memory.ReadCurrentSlot(ped, output) != ClockResult::Ok && output == 0xdeadbeef, "native GC refuses the current-slot read");
    ped = reset();
    f.Global(NativeStartGameProfile::ThreadIdRva, GetCurrentThreadId() + 1);
    output = 0xdeadbeef;
    Check(memory.ReadCurrentSlot(ped, output) != ClockResult::Ok && output == 0xdeadbeef, "foreign native GameThread identity grants no slot observation");
    ped = reset();
    bool changed = false;
    WeaponFieldsHost::afterRead = [&](std::uintptr_t at, std::size_t bytes)
    {
        if (!changed && at == ped.nativePed + S::CurrentOffset && bytes == 1)
        {
            changed = true;
            f.Store(0, S::CurrentOffset, static_cast<std::uint8_t>(12));
        }
    };
    output = 0xdeadbeef;
    Check(memory.ReadCurrentSlot(ped, output) == ClockResult::OwnerConflict && output == 0xdeadbeef,
          "selection transition cannot publish a mixed snapshot or a guessed slot");
    ped = reset();
    WeaponFieldsHost::afterRead = [&](std::uintptr_t at, std::size_t bytes)
    {
        if (at == ped.nativePed + S::CurrentOffset && bytes == 1)
            PoolTestHost::ownerResult = ClockResult::NotInitialized;
    };
    output = 0xdeadbeef;
    Check(memory.ReadCurrentSlot(ped, output) != ClockResult::Ok && output == 0xdeadbeef, "owner loss after an observed byte preserves caller output");
    ped = reset();
    PoolTestHost::failAt = 1;
    output = 0xdeadbeef;
    Check(memory.ReadCurrentSlot(ped, output) != ClockResult::Ok && output == 0xdeadbeef, "read failure has no zero or current-player fallback");
    std::puts("Production native current weapon slot reader fixture completed");
    std::printf("Passed %u native ped-current-weapon-slot checks (64-bit; no native engine code executed)\n", checks);
    return 0;
}
