/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Raw clip/total DWORD intent on current qualified inline weapon slots
 ****************************************************************************/
#pragma push_macro("main")
#undef main
#define main NativePedWeaponAmmoHistoricalFieldsMain
#include "NativePedWeaponFieldsTests.cpp"
#undef main
#pragma pop_macro("main")
#include "../../Client/game_de/NativePedWeaponAmmoMemory.h"
#include "../../Client/game_de/NativePedWeaponAmmoProfile.h"
#include <functional>
#include <intrin.h>
namespace WeaponAmmoHost
{
    unsigned                                         exchanges = 0, acceptedWrites = 0;
    bool                                             failBefore = false, failAfter = false;
    std::function<void()>                            before, after;
    std::function<void(std::uintptr_t, std::size_t)> afterRead;
    BOOL WINAPI                                      Read(HANDLE process, LPCVOID input, LPVOID output, SIZE_T bytes, SIZE_T* copied)
    {
        const auto result = PoolTestHost::Read(process, input, output, bytes, copied);
        if (result && afterRead)
            afterRead(reinterpret_cast<std::uintptr_t>(input), bytes);
        return result;
    }
    bool Exchange(std::uintptr_t at, std::uint32_t expected, std::uint32_t target, std::uint32_t& observed, std::uint32_t& exception) noexcept
    {
        ++exchanges;
        if (failBefore)
        {
            exception = EXCEPTION_ACCESS_VIOLATION;
            return false;
        }
        if (before)
            before();
        LONG original = 0, next = 0;
        std::memcpy(&original, &expected, sizeof(original));
        std::memcpy(&next, &target, sizeof(next));
        const auto previous = _InterlockedCompareExchange(reinterpret_cast<volatile LONG*>(at), next, original);
        std::memcpy(&observed, &previous, sizeof(observed));
        if (observed == expected)
            ++acceptedWrites;
        if (after)
            after();
        if (failAfter)
        {
            exception = EXCEPTION_ACCESS_VIOLATION;
            return false;
        }
        return true;
    }
    void Reset()
    {
        exchanges = acceptedWrites = 0;
        failBefore = failAfter = false;
        before = after = {};
        afterRead = {};
    }
}
#define GetModuleHandleW                PoolTestHost::Module
#define ReadProcessMemory               WeaponAmmoHost::Read
#define MTA_DE_PED_WEAPON_AMMO_EXCHANGE WeaponAmmoHost::Exchange
#include "../../Client/game_de/NativePedWeaponAmmoMemory.cpp"
#undef MTA_DE_PED_WEAPON_AMMO_EXCHANGE
#undef ReadProcessMemory
#undef GetModuleHandleW
namespace
{
    namespace AP = NativePedWeaponAmmoProfile;
    namespace WP = NativePedWeaponProfile;
    struct WeaponAmmoFixture : WeaponFieldsFixture
    {
        explicit WeaponAmmoFixture(const std::filesystem::path& directory) : WeaponFieldsFixture(directory)
        {
            for (const auto& pin : AP::Code)
                transform.image.Patch(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
            Reset();
        }
        unsigned char* Row(std::uint32_t slot = 0, std::size_t pedSlot = 0) const
        {
            return transform.pool.objects + pedSlot * NativePedPoolProfile::ObjectStride + WP::ArrayOffset + static_cast<std::size_t>(slot) * WP::Stride;
        }
        void Field(std::uint32_t slot, std::size_t field, std::uint32_t value, std::size_t pedSlot = 0)
        {
            std::memcpy(Row(slot, pedSlot) + field, &value, sizeof(value));
        }
        std::uint32_t Field(std::uint32_t slot, std::size_t field, std::size_t pedSlot = 0) const
        {
            std::uint32_t value = 0;
            std::memcpy(&value, Row(slot, pedSlot) + field, sizeof(value));
            return value;
        }
        std::array<unsigned char, WP::Stride> Snapshot(std::uint32_t slot = 0, std::size_t pedSlot = 0) const
        {
            std::array<unsigned char, WP::Stride> bytes{};
            std::memcpy(bytes.data(), Row(slot, pedSlot), bytes.size());
            return bytes;
        }
        void Reset()
        {
            WeaponFieldsFixture::Reset();
            for (const auto pedSlot : {std::size_t{0}, std::size_t{8}})
                for (std::uint32_t slot = 0; slot != WP::Count; ++slot)
                {
                    std::memset(Row(slot, pedSlot), 0x5a, WP::Stride);
                    Field(slot, WP::TypeOffset, 22, pedSlot);
                    Field(slot, WP::StateOffset, 2, pedSlot);
                    Field(slot, AP::ClipOffset, 7, pedSlot);
                    Field(slot, AP::TotalOffset, 39, pedSlot);
                }
            WeaponAmmoHost::Reset();
        }
    };
}
int main(int argc, char** argv)
{
    using Memory = NativePedWeaponAmmoMemory;
    Check(argc == 2 && std::filesystem::path(argv[1]).is_absolute(), "ammo evidence uses the explicit workspace-owned directory");
    Memory::Binding refused{123};
    Check(!Memory::Qualify(refused) && refused.image == 123, "unsupported executable never publishes an ammo binding");
    WeaponAmmoFixture fixture(argv[1]);
    PoolTestHost::image = fixture.transform.image.image;
    const Memory::Binding binding{fixture.transform.image.image};
    Memory                memory(binding, &PoolTestHost::Owner), unowned(binding, nullptr);
    Memory::Receipt       receipt;
    const auto            reset = [&]
    {
        fixture.Reset();
        return fixture.Ped();
    };
    const auto read = [&](const Memory::Ped& ped, std::uint32_t slot, bool clip, std::uint32_t& value)
    { return clip ? memory.ReadAmmoInClip(ped, slot, value) : memory.ReadAmmoTotal(ped, slot, value); };
    const auto write = [&](const Memory::Ped& ped, std::uint32_t slot, bool clip, std::uint32_t value, Memory::Receipt* result)
    { return clip ? memory.SetAmmoInClip(ped, slot, value, result) : memory.SetAmmoTotal(ped, slot, value, result); };
    Check(Memory::Revalidate(binding), "actual native ammo roles qualify with the common inline-slot context");
    struct DefaultAccess final : NativePedWeaponAmmoAccess
    {
    } defaultAccess;
    std::uint32_t actual = 0xabcdef01;
    Check(defaultAccess.ReadPedWeaponAmmoInClip({}, 0, actual) == ClockResult::NotInitialized && actual == 0xabcdef01 &&
              defaultAccess.ReadPedWeaponAmmoTotal({}, 0, actual) == ClockResult::NotInitialized && actual == 0xabcdef01 &&
              defaultAccess.SetPedWeaponAmmoInClip({}, 0, 0xffffffff) == ClockResult::NotInitialized &&
              defaultAccess.SetPedWeaponAmmoTotal({}, 0, 0xffffffff) == ClockResult::NotInitialized,
          "default access cannot invent zero ammo or accept either raw setter");
    for (const auto pedSlot : {std::size_t{0}, std::size_t{8}})
        for (const auto slot : {std::uint32_t{0}, std::uint32_t{12}})
            for (const auto clip : {false, true})
                for (const auto target :
                     {std::uint32_t{0}, std::uint32_t{1}, std::uint32_t{99999}, std::uint32_t{100000}, std::uint32_t{0x80000000}, std::uint32_t{0xffffffff}})
                {
                    fixture.Reset();
                    const auto ped = fixture.Ped(pedSlot == 0 ? 1 : (8u << 8) | 127u);
                    auto       expected = fixture.Snapshot(slot, pedSlot);
                    const auto field = clip ? AP::ClipOffset : AP::TotalOffset;
                    actual = 0xabcdef01;
                    Check(read(ped, slot, clip, actual) == ClockResult::Ok && actual == (clip ? 7u : 39u) && !WeaponAmmoHost::exchanges,
                          "both actual classes read clip/total on first and last explicit slots without requiring current selection");
                    std::memcpy(expected.data() + field, &target, sizeof(target));
                    Check(write(ped, slot, clip, target, &receipt) == ClockResult::Ok && receipt.entered && receipt.accepted && receipt.verified &&
                              receipt.observedValid && receipt.handle == ped.handle && receipt.slot == slot && receipt.rawType == 22 &&
                              receipt.nativePed == ped.nativePed && receipt.nativeWeapon == reinterpret_cast<std::uintptr_t>(fixture.Row(slot, pedSlot)) &&
                              receipt.address == receipt.nativeWeapon + field && receipt.original == (clip ? 7u : 39u) && receipt.target == target &&
                              receipt.observed == target && fixture.Snapshot(slot, pedSlot) == expected && WeaponAmmoHost::exchanges == 1 &&
                              WeaponAmmoHost::acceptedWrites == 1,
                          "full raw DWORD setter preserves every other row byte without script clamp, clip/total coupling or state reset");
                }
    auto ped = reset();
    for (const auto slot : {std::uint32_t{13}, std::uint32_t{0x80000000}, std::uint32_t{0xffffffff}})
    {
        actual = 0xabcdef01;
        Check(memory.ReadAmmoInClip(ped, slot, actual) == ClockResult::InvalidArgument && actual == 0xabcdef01 &&
                  memory.SetAmmoTotal(ped, slot, 1, &receipt) == ClockResult::InvalidArgument && !receipt.entered && !WeaponAmmoHost::exchanges,
              "full DWORD slot bounds reject invalid selectors instead of truncating them into the native array");
    }
    actual = 0xabcdef01;
    Check(unowned.ReadAmmoTotal(ped, 0, actual) == ClockResult::NotInitialized && actual == 0xabcdef01 &&
              unowned.SetAmmoInClip(ped, 0, 1, &receipt) == ClockResult::NotInitialized && !receipt.entered && !WeaponAmmoHost::exchanges,
          "occupied inline memory does not replace the same callback owner");
    ped = reset();
    WeaponAmmoHost::before = [&] { fixture.Field(0, AP::ClipOffset, 8); };
    Check(memory.SetAmmoInClip(ped, 0, 100000, &receipt) == ClockResult::OwnerConflict && receipt.entered && !receipt.accepted &&
              WeaponAmmoHost::exchanges == 1 && !WeaponAmmoHost::acceptedWrites && fixture.Field(0, AP::ClipOffset) == 8,
          "competing ammo is never overwritten, merged or retried");
    ped = reset();
    WeaponAmmoHost::before = [&] { fixture.Field(0, AP::TotalOffset, 0xffffffff); };
    Check(memory.SetAmmoInClip(ped, 0, 100000, &receipt) == ClockResult::Ok && receipt.verified && WeaponAmmoHost::exchanges == 1 &&
              fixture.Field(0, AP::ClipOffset) == 100000 && fixture.Field(0, AP::TotalOffset) == 0xffffffff && fixture.Field(0, WP::StateOffset) == 2,
          "unowned total changes survive an independent clip write without invoking script ammo policy");
    for (unsigned failure = 0; failure != 7; ++failure)
    {
        ped = reset();
        if (failure == 0)
            WeaponAmmoHost::failBefore = true;
        if (failure == 1)
            WeaponAmmoHost::failAfter = true;
        if (failure == 2)
            WeaponAmmoHost::after = [&] { PoolTestHost::ownerResult = ClockResult::Expired; };
        if (failure == 3)
            WeaponAmmoHost::after = [&] { fixture.transform.pool.flags[0] = 2; };
        if (failure == 4)
            WeaponAmmoHost::after = [&] { fixture.Field(0, WP::TypeOffset, 23); };
        if (failure == 5)
            WeaponAmmoHost::after = [&] { fixture.Global(NativeBorrowedGcProfile::markerRva, LONG{1}); };
        if (failure == 6)
            WeaponAmmoHost::after = [&] { fixture.Field(0, AP::ClipOffset, 100001); };
        Check(memory.SetAmmoInClip(ped, 0, 100000, &receipt) != ClockResult::Ok && receipt.entered && !receipt.verified && WeaponAmmoHost::exchanges == 1 &&
                  WeaponAmmoHost::acceptedWrites == (failure == 0 ? 0u : 1u) &&
                  (failure >= 1 && failure <= 5 ? fixture.Field(0, AP::ClipOffset) == 100000 : true),
              "entered exception, possible accepted effect, changed current instance or lost owner never triggers rollback or replay");
    }
    ped = reset();
    Memory::Receipt nested;
    ClockResult     nestedResult = ClockResult::Ok;
    WeaponAmmoHost::before = [&] { nestedResult = memory.SetAmmoTotal(ped, 0, 4, &nested); };
    Check(memory.SetAmmoInClip(ped, 0, 100000, &receipt) == ClockResult::Ok && nestedResult == ClockResult::Busy && !nested.entered &&
              WeaponAmmoHost::exchanges == 1 && fixture.Field(0, AP::TotalOffset) == 39,
          "clip and total setters share the common live weapon mutation admission");
    ped = reset();
    NativePedWeaponFieldsMemory          fields(binding, &PoolTestHost::Owner);
    NativePedWeaponFieldsMemory::Receipt fieldReceipt;
    WeaponAmmoHost::before = [&] { nestedResult = fields.SetType(ped, 0, 23, &fieldReceipt); };
    Check(memory.SetAmmoInClip(ped, 0, 100000, &receipt) == ClockResult::Ok && nestedResult == ClockResult::Busy && !fieldReceipt.entered &&
              !WeaponFieldsHost::exchanges && WeaponAmmoHost::exchanges == 1 && fixture.Field(0, WP::TypeOffset) == 22,
          "actual type mutation refuses entry while an ammo setter owns the shared current-instance scope");
    ped = reset();
    WeaponFieldsHost::before = [&] { nestedResult = memory.SetAmmoTotal(ped, 0, 1, &nested); };
    Check(fields.SetState(ped, 0, 1, &fieldReceipt) == ClockResult::Ok && nestedResult == ClockResult::Busy && !nested.entered &&
              WeaponFieldsHost::exchanges == 1 && !WeaponAmmoHost::exchanges && fixture.Field(0, AP::TotalOffset) == 39,
          "actual state setter and ammo setter use one shared mutation admission in both directions");
    for (const auto independent : {false, true})
    {
        ped = reset();
        unsigned seen = 0;
        WeaponAmmoHost::afterRead = [&](std::uintptr_t address, std::size_t bytes)
        {
            if (address == reinterpret_cast<std::uintptr_t>(fixture.Row()) + AP::ClipOffset && bytes == sizeof(actual) && ++seen == 1)
                fixture.Field(0, independent ? AP::TotalOffset : AP::ClipOffset, 8);
        };
        actual = 0xabcdef01;
        const auto result = memory.ReadAmmoInClip(ped, 0, actual);
        Check(independent ? result == ClockResult::Ok && actual == 7 : result == ClockResult::OwnerConflict && actual == 0xabcdef01,
              "read coherence owns one ammo DWORD and preserves output when that field changes");
    }
    for (const auto& pin : AP::Code)
    {
        ped = reset();
        const auto bad = static_cast<unsigned char>(pin.bytes[0] ^ 1);
        fixture.transform.image.Patch(pin.rva, &bad, 1, PAGE_EXECUTE_READ);
        actual = 0xabcdef01;
        Check(memory.ReadAmmoTotal(ped, 0, actual) != ClockResult::Ok && actual == 0xabcdef01 && memory.SetAmmoInClip(ped, 0, 1, &receipt) != ClockResult::Ok &&
                  !receipt.entered && !WeaponAmmoHost::exchanges,
              "every independent exact native ammo-role pin remains mandatory");
        fixture.transform.image.Patch(pin.rva, pin.bytes, 1, PAGE_EXECUTE_READ);
    }
    ped = reset();
    ClockResult foreign = ClockResult::Ok;
    std::thread other([&] { foreign = memory.SetAmmoTotal(ped, 0, 1, &receipt); });
    other.join();
    Check(foreign != ClockResult::Ok && !receipt.entered && !WeaponAmmoHost::exchanges, "foreign thread cannot write the ped-owned weapon array");
    ped = reset();
    Check(memory.SetAmmoInClip(ped, 0, 0xffffffff, &receipt) == ClockResult::Ok && receipt.verified,
          "common mutation scope releases after every refusal and fault");
    std::puts("Production native ped weapon ammo primitive fixture completed");
    std::printf("Passed %u native ped-weapon-ammo checks (64-bit; no native engine code executed)\n", checks);
    return 0;
}
