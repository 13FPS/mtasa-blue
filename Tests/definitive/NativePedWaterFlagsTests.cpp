/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual two-water-bit storage over genuine owned generic/player peds
 ****************************************************************************/
#pragma push_macro("main")
#undef main
#define main NativePedWaterFlagsHistoricalDuckingMain
#include "NativePedDuckingTests.cpp"
#undef main
#pragma pop_macro("main")
#include "../../Client/game_de/NativePedWaterFlagsMemory.h"
#include "../../Client/game_de/NativePedWaterFlagsProfile.h"
#include <functional>
#include <intrin.h>
namespace WaterHost
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
        LONG old = 0, desired = 0;
        std::memcpy(&old, &expected, sizeof(old));
        std::memcpy(&desired, &target, sizeof(desired));
        const auto previous = _InterlockedCompareExchange(reinterpret_cast<volatile LONG*>(at), desired, old);
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
#define ReadProcessMemory               WaterHost::Read
#define MTA_DE_PED_WATER_FLAGS_EXCHANGE WaterHost::Exchange
#include "../../Client/game_de/NativePedWaterFlagsMemory.cpp"
#undef MTA_DE_PED_WATER_FLAGS_EXCHANGE
#undef ReadProcessMemory
#undef GetModuleHandleW

namespace
{
    namespace W = NativePedWaterFlagsProfile;
    struct WaterFixture : DuckingFixture
    {
        explicit WaterFixture(const std::filesystem::path& directory) : DuckingFixture(directory)
        {
            for (const auto& pin : W::Code)
                transform.image.Patch(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
            Reset();
        }
        void Reset()
        {
            DuckingFixture::Reset();
            for (const auto slot : {std::size_t{0}, std::size_t{8}})
                Store(slot, W::FlagsOffset, std::uint32_t{0x15a632e7} & ~W::WaterMask);
            WaterHost::Reset();
            PoolTestHost::ownerResult = ClockResult::Ok;
            PoolTestHost::reads = PoolTestHost::failAt = PoolTestHost::ownerCalls = PoolTestHost::revokeAt = 0;
        }
        std::uint32_t Bits(std::size_t slot = 0) const
        {
            std::uint32_t value = 0;
            std::memcpy(&value, transform.pool.objects + slot * NativePedPoolProfile::ObjectStride + W::FlagsOffset, sizeof(value));
            return value;
        }
    };
}
int main(int argc, char** argv)
{
    using Memory = NativePedWaterFlagsMemory;
    Check(argc == 2 && std::filesystem::path(argv[1]).is_absolute(), "water evidence uses its explicit workspace directory");
    Memory::Binding bad{123};
    Check(!Memory::Qualify(bad) && bad.image == 123, "ordinary host cannot acquire shipping native water authority");
    WaterFixture fixture(argv[1]);
    PoolTestHost::image = fixture.transform.image.image;
    const Memory::Binding binding{fixture.transform.image.image};
    Memory                memory(binding, &PoolTestHost::Owner), absent(binding, nullptr);
    Memory::Receipt       receipt;
    Check(Memory::Revalidate(binding), "exact native producer, constructor and independent submerged consumer qualify storage");
    struct Default final : NativePedWaterFlagsAccess
    {
    } defaultAccess;
    Check(defaultAccess.SetPedInWaterFlags({}, true) == ClockResult::NotInitialized, "timing-only facade grants no physical water setter");
    const auto reset = [&]
    {
        fixture.Reset();
        auto ped = fixture.Ped();
        WaterHost::Reset();
        return ped;
    };
    for (const auto handle : {1u, 0x87fu})
        for (const auto pair : {0u, W::SubmergedMask, W::TouchingMask, W::WaterMask})
            for (const bool enabled : {false, true})
            {
                fixture.Reset();
                const std::size_t slot = handle >> 8;
                const auto        original = (std::uint32_t{0x95a632e7} & ~W::WaterMask) | pair;
                fixture.Store(slot, W::FlagsOffset, original);
                const auto                                                    ped = fixture.Ped(handle);
                std::array<unsigned char, NativePedPoolProfile::ObjectStride> before{};
                std::memcpy(before.data(), reinterpret_cast<void*>(ped.nativePed), before.size());
                Check(memory.SetInWaterFlags(ped, enabled, &receipt) == ClockResult::Ok && receipt.entered && receipt.accepted && receipt.verified &&
                          receipt.attempts == 1 && WaterHost::exchanges == 1 && WaterHost::acceptedWrites == 1 &&
                          fixture.Bits(slot) == ((original & ~W::WaterMask) | (enabled ? W::WaterMask : 0)),
                      "both actual physical water bits receive one bool intent on generic/player without conflating task-based IsInWater");
                for (std::size_t i = 0; i != before.size(); ++i)
                    if (i < W::FlagsOffset || i >= W::FlagsOffset + sizeof(std::uint32_t))
                        Check(before[i] == reinterpret_cast<const unsigned char*>(ped.nativePed)[i], "water setter preserves every other ped byte");
            }
    auto ped = reset();
    Check(absent.SetInWaterFlags(ped, true, &receipt) == ClockResult::NotInitialized && !receipt.entered, "unowned water mutation refuses before storage");
    WaterHost::before = [&]
    {
        if (WaterHost::exchanges == 1)
            fixture.Store(0, W::FlagsOffset, fixture.Bits() ^ std::uint32_t{1});
    };
    Check(memory.SetInWaterFlags(ped, true, &receipt) == ClockResult::Ok && receipt.attempts == 2 && WaterHost::acceptedWrites == 1 &&
              (fixture.Bits() & W::WaterMask) == W::WaterMask && (fixture.Bits() & 1) == 0,
          "only an unaccepted independent-neighbor conflict may reacquire current DWORD before one accepted pair store");
    for (const auto owned : {W::SubmergedMask, W::TouchingMask})
    {
        ped = reset();
        WaterHost::before = [&] { fixture.Store(0, W::FlagsOffset, fixture.Bits() ^ owned); };
        Check(memory.SetInWaterFlags(ped, true, &receipt) == ClockResult::OwnerConflict && receipt.entered && !receipt.accepted && WaterHost::exchanges == 1 &&
                  fixture.Bits() == ((std::uint32_t{0x15a632e7} & ~W::WaterMask) ^ owned),
              "either concurrently changed owned water bit is preserved without CAS retry");
    }
    ped = reset();
    WaterHost::before = [&] { fixture.Store(0, W::FlagsOffset, fixture.Bits() ^ std::uint32_t{1}); };
    Check(memory.SetInWaterFlags(ped, true, &receipt) == ClockResult::OwnerConflict && receipt.entered && !receipt.accepted &&
              WaterHost::exchanges == W::MaximumAttempts && WaterHost::acceptedWrites == 0,
          "perpetually changing neighbors have a fixed work bound without an accepted effect");
    for (const bool after : {false, true})
    {
        ped = reset();
        WaterHost::failBefore = !after;
        WaterHost::failAfter = after;
        const auto original = fixture.Bits();
        Check(memory.SetInWaterFlags(ped, true, &receipt) == ClockResult::WriteFailure && receipt.entered && !receipt.accepted && WaterHost::exchanges == 1 &&
                  fixture.Bits() == (after ? original | W::WaterMask : original),
              "unknown final-store acknowledgement remains entered and never retries or restores a possibly accepted water pair");
    }
    for (unsigned loss = 0; loss != 4; ++loss)
    {
        ped = reset();
        WaterHost::after = [&]
        {
            if (loss == 0)
                fixture.transform.pool.flags[0] ^= 1;
            if (loss == 1)
                PoolTestHost::ownerResult = ClockResult::Expired;
            if (loss == 2)
                fixture.Global(NativeBorrowedGcProfile::markerRva, LONG{1});
            if (loss == 3)
                fixture.Store(0, W::FlagsOffset, fixture.Bits() ^ W::TouchingMask);
        };
        Check(memory.SetInWaterFlags(ped, true, &receipt) != ClockResult::Ok && receipt.entered && receipt.accepted && !receipt.verified &&
                  WaterHost::exchanges == 1 && (fixture.Bits() & W::SubmergedMask) != 0,
              "post-store identity/owner/GC/owned-bit loss retains acknowledged effects and never replays the accepted pair");
    }
    ped = reset();
    WaterHost::before = [&]
    {
        Memory::Receipt nested;
        Check(memory.SetInWaterFlags(ped, false, &nested) == ClockResult::Busy && !nested.entered,
              "recursive pair mutation cannot borrow the current setter intent");
    };
    Check(memory.SetInWaterFlags(ped, true, &receipt) == ClockResult::Ok, "recursive refusal leaves the admitted outer pair store valid");
    ped = reset();
    PoolTestHost::ownerResult = ClockResult::Expired;
    Check(memory.SetInWaterFlags(ped, true, &receipt) == ClockResult::Expired && !receipt.entered && !WaterHost::exchanges,
          "expired owner cannot turn old native pool observation into water write authority");
    ped = reset();
    ClockResult foreign = ClockResult::Ok;
    std::thread worker([&] { foreign = memory.SetInWaterFlags(ped, true, &receipt); });
    worker.join();
    Check(foreign == ClockResult::WrongThread && !receipt.entered && !WaterHost::exchanges, "actual native GameThread guard still controls setter entry");
    ped = reset();
    const auto& pin = W::Code[5];
    const auto  changed = static_cast<unsigned char>(pin.bytes[0] ^ 1);
    fixture.transform.image.Patch(pin.rva, &changed, 1, PAGE_EXECUTE_READ);
    Check(memory.SetInWaterFlags(ped, true, &receipt) == ClockResult::BindingChanged && !receipt.entered && !WaterHost::exchanges,
          "changed genuine pair producer refuses before any water storage mutation");
    fixture.transform.image.Patch(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
    std::puts("Production native physical water flags primitive fixture completed");
    std::printf("Passed %u native ped water-flags checks (64-bit; no native engine code executed)\n", checks);
    return 0;
}
