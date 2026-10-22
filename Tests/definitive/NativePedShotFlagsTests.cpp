/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Exact named vehicle-shot mask over genuine owned generic/player peds
 ****************************************************************************/
#pragma push_macro("main")
#undef main
#define main NativePedShotFlagsHistoricalDuckingMain
#include "NativePedDuckingTests.cpp"
#undef main
#pragma pop_macro("main")
#include "../../Client/game_de/NativePedShotFlagsMemory.h"
#include "../../Client/game_de/NativePedShotFlagsProfile.h"
#include <functional>
#include <intrin.h>
namespace ShotHost
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
#define GetModuleHandleW               PoolTestHost::Module
#define ReadProcessMemory              ShotHost::Read
#define MTA_DE_PED_SHOT_FLAGS_EXCHANGE ShotHost::Exchange
#include "../../Client/game_de/NativePedShotFlagsMemory.cpp"
#undef MTA_DE_PED_SHOT_FLAGS_EXCHANGE
#undef ReadProcessMemory
#undef GetModuleHandleW
namespace
{
    namespace SF = NativePedShotFlagsProfile;
    struct ShotFixture : DuckingFixture
    {
        explicit ShotFixture(const std::filesystem::path& directory) : DuckingFixture(directory)
        {
            for (const auto& pin : SF::Code)
                transform.image.Patch(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
            Reset();
        }
        void Reset()
        {
            DuckingFixture::Reset();
            for (const auto slot : {std::size_t{0}, std::size_t{8}})
                Store(slot, SF::FlagsOffset, std::uint32_t{0xa1123400});
            ShotHost::Reset();
        }
        std::uint32_t ShotBits(std::size_t slot = 0) const
        {
            std::uint32_t bits = 0;
            std::memcpy(&bits, transform.pool.objects + slot * NativePedPoolProfile::ObjectStride + SF::FlagsOffset, sizeof(bits));
            return bits;
        }
    };
}
int main(int argc, char** argv)
{
    using Memory = NativePedShotFlagsMemory;
    Check(argc == 2 && std::filesystem::path(argv[1]).is_absolute(), "shot flag evidence uses an explicit workspace-owned directory");
    Memory::Binding refused{123};
    Check(!Memory::Qualify(refused) && refused.image == 123, "actual unsupported executable never becomes an admitted shot flag binding");
    ShotFixture fixture(argv[1]);
    PoolTestHost::image = fixture.transform.image.image;
    const Memory::Binding binding{fixture.transform.image.image};
    Memory                memory(binding, &PoolTestHost::Owner), unowned(binding, nullptr);
    Memory::Receipt       receipt;
    const auto            reset = [&]
    {
        fixture.Reset();
        return fixture.Ped();
    };
    Check(Memory::Revalidate(binding), "exact named bool setter and common initializer qualify with inherited ped admission");
    struct DefaultAccess final : NativePedShotFlagsAccess
    {
    } defaultAccess;
    for (bool prior : {false, true})
    {
        bool value = prior;
        Check(defaultAccess.ReadPedCanBeShotInVehicle(NativePedReferenceToken{}, value) == ClockResult::NotInitialized && value == prior &&
                  defaultAccess.SetPedCanBeShotInVehicle(NativePedReferenceToken{}, !prior) == ClockResult::NotInitialized,
              "unbound token access neither invents successful false nor accepts a fake setter");
    }
    const std::array<std::uint32_t, 6> seeds{0, 0xffffffff, SF::CanBeShotMask, ~SF::CanBeShotMask, 0x13579bdf, 0x2468ace0};
    for (const auto slot : {std::size_t{0}, std::size_t{8}})
        for (const auto bits : seeds)
            for (bool enabled : {false, true})
            {
                fixture.Reset();
                fixture.Store(slot, SF::FlagsOffset, bits);
                const auto ped = fixture.Ped(slot == 0 ? 1 : (8u << 8) | 127u);
                bool       actual = (bits & SF::CanBeShotMask) == 0;
                Check(memory.ReadCanBeShotInVehicle(ped, actual) == ClockResult::Ok && actual == ((bits & SF::CanBeShotMask) != 0) &&
                          fixture.ShotBits(slot) == bits && !ShotHost::exchanges,
                      "both classes read only the named bit across all unrelated flag patterns");
                const auto target = (bits & ~SF::CanBeShotMask) | (enabled ? SF::CanBeShotMask : 0);
                Check(memory.SetCanBeShotInVehicle(ped, enabled, &receipt) == ClockResult::Ok && receipt.entered && receipt.accepted && receipt.verified &&
                          receipt.observedValid && receipt.original == bits && receipt.target == target && receipt.observed == target &&
                          receipt.attempts == 1 && receipt.handle == ped.handle && receipt.nativePed == ped.nativePed &&
                          receipt.address == ped.nativePed + SF::FlagsOffset && fixture.ShotBits(slot) == target && ShotHost::exchanges == 1 &&
                          ShotHost::acceptedWrites == 1,
                      "one genuine masked DWORD CAS implements the named bool while every other bit is preserved");
            }
    auto ped = reset();
    bool actual = true;
    Check(unowned.ReadCanBeShotInVehicle(ped, actual) == ClockResult::NotInitialized && actual &&
              unowned.SetCanBeShotInVehicle(ped, true, &receipt) == ClockResult::NotInitialized && !receipt.entered && !ShotHost::exchanges,
          "readable owned ped memory cannot replace callback authority for either property operation");
    ped = reset();
    const auto original = fixture.ShotBits();
    ShotHost::before = [&]
    {
        if (ShotHost::exchanges == 1)
            fixture.Store(0, SF::FlagsOffset, fixture.ShotBits() ^ std::uint32_t{1});
    };
    Check(memory.SetCanBeShotInVehicle(ped, true, &receipt) == ClockResult::Ok && receipt.accepted && receipt.verified && receipt.attempts == 2 &&
              ShotHost::acceptedWrites == 1 && fixture.ShotBits() == ((original ^ 1) | SF::CanBeShotMask),
          "unaccepted neighbor-only CAS conflict is reacquired and merged without losing the independently changed bit");
    ped = reset();
    ShotHost::after = [&] { fixture.Store(0, SF::FlagsOffset, fixture.ShotBits() ^ std::uint32_t{0x80000001}); };
    Check(memory.SetCanBeShotInVehicle(ped, true, &receipt) == ClockResult::Ok && receipt.accepted && receipt.verified && receipt.attempts == 1 &&
              ShotHost::acceptedWrites == 1 && (fixture.ShotBits() & SF::CanBeShotMask) && fixture.ShotBits() != receipt.target,
          "post-acceptance changes to unowned flags remain intact and do not trigger an accepted-write replay");
    ped = reset();
    ShotHost::before = [&] { fixture.Store(0, SF::FlagsOffset, fixture.ShotBits() ^ SF::CanBeShotMask); };
    Check(memory.SetCanBeShotInVehicle(ped, true, &receipt) == ClockResult::OwnerConflict && receipt.entered && !receipt.accepted && receipt.attempts == 1 &&
              !ShotHost::acceptedWrites && (fixture.ShotBits() & SF::CanBeShotMask),
          "competing change to the intended flag is never overwritten or retried");
    ped = reset();
    ShotHost::before = [&] { fixture.Store(0, SF::FlagsOffset, fixture.ShotBits() ^ std::uint32_t{1}); };
    Check(memory.SetCanBeShotInVehicle(ped, true, &receipt) == ClockResult::OwnerConflict && receipt.entered && !receipt.accepted &&
              receipt.attempts == SF::MaximumAttempts && ShotHost::exchanges == SF::MaximumAttempts && !ShotHost::acceptedWrites,
          "repeated unaccepted neighbor changes are bounded instead of an endless setter loop");
    for (unsigned failure = 0; failure != 6; ++failure)
    {
        ped = reset();
        if (failure == 0)
            ShotHost::failBefore = true;
        if (failure == 1)
            ShotHost::failAfter = true;
        if (failure == 2)
            ShotHost::after = [&] { PoolTestHost::ownerResult = ClockResult::Expired; };
        if (failure == 3)
            ShotHost::after = [&] { fixture.transform.pool.flags[0] = 2; };
        if (failure == 4)
            ShotHost::after = [&] { fixture.Global(NativeBorrowedGcProfile::markerRva, LONG{1}); };
        if (failure == 5)
            ShotHost::after = [&] { fixture.Store(0, SF::FlagsOffset, fixture.ShotBits() & ~SF::CanBeShotMask); };
        Check(memory.SetCanBeShotInVehicle(ped, true, &receipt) != ClockResult::Ok && receipt.entered && !receipt.verified && receipt.attempts == 1 &&
                  ShotHost::exchanges == 1 && ShotHost::acceptedWrites == (failure == 0 ? 0u : 1u) &&
                  ((failure >= 1 && failure <= 4) ? (fixture.ShotBits() & SF::CanBeShotMask) != 0 : true),
              "entered exception, accepted mutation or lost acknowledgement retains effects and never restores or retries");
    }
    ped = reset();
    Memory::Receipt nested;
    ClockResult     nestedResult = ClockResult::Ok;
    ShotHost::before = [&] { nestedResult = memory.SetCanBeShotInVehicle(ped, false, &nested); };
    Check(memory.SetCanBeShotInVehicle(ped, true, &receipt) == ClockResult::Ok && nestedResult == ClockResult::Busy && !nested.entered &&
              ShotHost::exchanges == 1 && ShotHost::acceptedWrites == 1,
          "a nested setter cannot borrow the same live native property-write admission");
    for (bool neighborOnly : {false, true})
    {
        ped = reset();
        unsigned seen = 0;
        ShotHost::afterRead = [&](std::uintptr_t at, std::size_t bytes)
        {
            if (at == ped.nativePed + SF::FlagsOffset && bytes == 4 && ++seen == 1)
                fixture.Store(0, SF::FlagsOffset, fixture.ShotBits() ^ (neighborOnly ? std::uint32_t{1} : SF::CanBeShotMask));
        };
        const auto result = memory.SetCanBeShotInVehicle(ped, true, &receipt);
        Check(neighborOnly ? (result == ClockResult::Ok && receipt.accepted && (fixture.ShotBits() & 1))
                           : (result == ClockResult::OwnerConflict && !receipt.entered && !ShotHost::exchanges),
              "preflight snapshot coherence applies to intended flag without claiming independent neighbors");
    }
    for (const auto& pin : SF::Code)
    {
        ped = reset();
        const auto bad = static_cast<unsigned char>(pin.bytes[0] ^ 1);
        fixture.transform.image.Patch(pin.rva, &bad, 1, PAGE_EXECUTE_READ);
        actual = true;
        Check(memory.ReadCanBeShotInVehicle(ped, actual) != ClockResult::Ok && actual && memory.SetCanBeShotInVehicle(ped, true, &receipt) != ClockResult::Ok &&
                  !receipt.entered && !ShotHost::exchanges,
              "named producer, actual bool-select store and common initializer are all required before property admission");
        fixture.transform.image.Patch(pin.rva, pin.bytes, 1, PAGE_EXECUTE_READ);
    }
    ped = reset();
    ClockResult foreign = ClockResult::Ok;
    std::thread other([&] { foreign = memory.SetCanBeShotInVehicle(ped, true, &receipt); });
    other.join();
    Check(foreign != ClockResult::Ok && !receipt.entered && !ShotHost::exchanges, "foreign thread cannot mutate the native ped property");
    ped = reset();
    Check(memory.SetCanBeShotInVehicle(ped, true, &receipt) == ClockResult::Ok && receipt.verified, "write scope releases after every bounded failure path");
    std::puts("Production native ped shot-flags primitive fixture completed");
    std::printf("Passed %u native ped-shot-flags checks (64-bit; no native engine code executed)\n", checks);
    return 0;
}
