/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Exact named bleeding mask over genuine owned generic/player peds
 ****************************************************************************/
#pragma push_macro("main")
#undef main
#define main NativePedBleedingHistoricalDuckingMain
#include "NativePedDuckingTests.cpp"
#undef main
#pragma pop_macro("main")
#include "../../Client/game_de/NativePedBleedingMemory.h"
#include "../../Client/game_de/NativePedBleedingProfile.h"
#include <functional>
#include <intrin.h>
namespace BleedingHost
{
    unsigned                                         exchanges = 0, acceptedWrites = 0, exchangeRead = 0;
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
        exchangeRead = PoolTestHost::reads;
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
        exchanges = acceptedWrites = exchangeRead = 0;
        failBefore = failAfter = false;
        before = after = {};
        afterRead = {};
    }
}
#define GetModuleHandleW             PoolTestHost::Module
#define ReadProcessMemory            BleedingHost::Read
#define MTA_DE_PED_BLEEDING_EXCHANGE BleedingHost::Exchange
#include "../../Client/game_de/NativePedBleedingMemory.cpp"
#undef MTA_DE_PED_BLEEDING_EXCHANGE
#undef ReadProcessMemory
#undef GetModuleHandleW
namespace
{
    namespace SF = NativePedBleedingProfile;
    struct BleedingFixture : DuckingFixture
    {
        explicit BleedingFixture(const std::filesystem::path& directory) : DuckingFixture(directory)
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
            BleedingHost::Reset();
        }
        std::uint32_t BleedingBits(std::size_t slot = 0) const
        {
            std::uint32_t bits = 0;
            std::memcpy(&bits, transform.pool.objects + slot * NativePedPoolProfile::ObjectStride + SF::FlagsOffset, sizeof(bits));
            return bits;
        }
    };
}
int main(int argc, char** argv)
{
    using Memory = NativePedBleedingMemory;
    Check(argc == 2 && std::filesystem::path(argv[1]).is_absolute(), "bleeding flag evidence uses an explicit workspace-owned directory");
    Memory::Binding refused{123};
    Check(!Memory::Qualify(refused) && refused.image == 123, "actual unsupported executable never becomes an admitted bleeding flag binding");
    BleedingFixture fixture(argv[1]);
    PoolTestHost::image = fixture.transform.image.image;
    const Memory::Binding binding{fixture.transform.image.image};
    Memory                memory(binding, &PoolTestHost::Owner), unowned(binding, nullptr);
    Memory::Receipt       receipt;
    const auto            reset = [&]
    {
        fixture.Reset();
        const auto ped = fixture.Ped();
        // Resolve is setup, not an operation fault boundary.
        PoolTestHost::reads = PoolTestHost::failAt = PoolTestHost::ownerCalls = PoolTestHost::revokeAt = 0;
        return ped;
    };
    Check(Memory::Revalidate(binding), "exact named bool setter and common initializer qualify with inherited ped admission");
    struct DefaultAccess final : NativePedBleedingAccess
    {
    } defaultAccess;
    for (bool prior : {false, true})
    {
        bool value = prior;
        Check(defaultAccess.ReadPedBleeding(NativePedReferenceToken{}, value) == ClockResult::NotInitialized && value == prior &&
                  defaultAccess.SetPedBleeding(NativePedReferenceToken{}, !prior) == ClockResult::NotInitialized,
              "unbound token access neither invents successful false nor accepts a fake setter");
    }
    const std::array<std::uint32_t, 6> seeds{0, 0xffffffff, SF::BleedingMask, ~SF::BleedingMask, 0x13579bdf, 0x2468ace0};
    for (const auto slot : {std::size_t{0}, std::size_t{8}})
        for (const auto bits : seeds)
            for (bool enabled : {false, true})
            {
                fixture.Reset();
                fixture.Store(slot, SF::FlagsOffset, bits);
                const auto ped = fixture.Ped(slot == 0 ? 1 : (8u << 8) | 127u);
                bool       actual = (bits & SF::BleedingMask) == 0;
                Check(memory.ReadBleeding(ped, actual) == ClockResult::Ok && actual == ((bits & SF::BleedingMask) != 0) && fixture.BleedingBits(slot) == bits &&
                          !BleedingHost::exchanges,
                      "both classes read only the named bit across all unrelated flag patterns");
                const auto target = (bits & ~SF::BleedingMask) | (enabled ? SF::BleedingMask : 0);
                Check(memory.SetBleeding(ped, enabled, &receipt) == ClockResult::Ok && receipt.entered && receipt.accepted && receipt.verified &&
                          receipt.observedValid && receipt.original == bits && receipt.target == target && receipt.observed == target &&
                          receipt.attempts == 1 && receipt.handle == ped.handle && receipt.nativePed == ped.nativePed &&
                          receipt.address == ped.nativePed + SF::FlagsOffset && fixture.BleedingBits(slot) == target && BleedingHost::exchanges == 1 &&
                          BleedingHost::acceptedWrites == 1,
                      "one genuine masked DWORD CAS implements the named bool while every other bit is preserved");
            }
    auto ped = reset();
    for (unsigned bit = 0; bit != 32; ++bit)
    {
        const auto neighbor = std::uint32_t{1} << bit;
        if (neighbor & SF::BleedingMask)
            continue;
        ped = reset();
        const auto original = fixture.BleedingBits();
        const auto target = (original ^ neighbor) | SF::BleedingMask;
        BleedingHost::before = [&]
        {
            if (BleedingHost::exchanges == 1)
                fixture.Store(0, SF::FlagsOffset, fixture.BleedingBits() ^ neighbor);
        };
        Check(memory.SetBleeding(ped, true, &receipt) == ClockResult::Ok && receipt.accepted && receipt.verified && receipt.attempts == 2 &&
                  BleedingHost::acceptedWrites == 1 && fixture.BleedingBits() == target,
              "every independent neighbor bit is retained across a failed CAS before the one accepted bleeding intent");
    }
    ped = reset();
    bool actual = true;
    Check(unowned.ReadBleeding(ped, actual) == ClockResult::NotInitialized && actual &&
              unowned.SetBleeding(ped, true, &receipt) == ClockResult::NotInitialized && !receipt.entered && !BleedingHost::exchanges,
          "readable owned ped memory cannot replace callback authority for either property operation");
    ped = reset();
    const auto original = fixture.BleedingBits();
    BleedingHost::before = [&]
    {
        if (BleedingHost::exchanges == 1)
            fixture.Store(0, SF::FlagsOffset, fixture.BleedingBits() ^ std::uint32_t{1});
    };
    Check(memory.SetBleeding(ped, true, &receipt) == ClockResult::Ok && receipt.accepted && receipt.verified && receipt.attempts == 2 &&
              BleedingHost::acceptedWrites == 1 && fixture.BleedingBits() == ((original ^ 1) | SF::BleedingMask),
          "unaccepted neighbor-only CAS conflict is reacquired and merged without losing the independently changed bit");
    ped = reset();
    BleedingHost::after = [&] { fixture.Store(0, SF::FlagsOffset, fixture.BleedingBits() ^ std::uint32_t{0x80000001}); };
    Check(memory.SetBleeding(ped, true, &receipt) == ClockResult::Ok && receipt.accepted && receipt.verified && receipt.attempts == 1 &&
              BleedingHost::acceptedWrites == 1 && (fixture.BleedingBits() & SF::BleedingMask) && fixture.BleedingBits() != receipt.target,
          "post-acceptance changes to unowned flags remain intact and do not trigger an accepted-write replay");
    ped = reset();
    BleedingHost::before = [&] { fixture.Store(0, SF::FlagsOffset, fixture.BleedingBits() ^ SF::BleedingMask); };
    Check(memory.SetBleeding(ped, true, &receipt) == ClockResult::OwnerConflict && receipt.entered && !receipt.accepted && receipt.attempts == 1 &&
              !BleedingHost::acceptedWrites && (fixture.BleedingBits() & SF::BleedingMask),
          "competing change to the intended flag is never overwritten or retried");
    ped = reset();
    BleedingHost::before = [&] { fixture.Store(0, SF::FlagsOffset, fixture.BleedingBits() ^ std::uint32_t{1}); };
    Check(memory.SetBleeding(ped, true, &receipt) == ClockResult::OwnerConflict && receipt.entered && !receipt.accepted &&
              receipt.attempts == SF::MaximumAttempts && BleedingHost::exchanges == SF::MaximumAttempts && !BleedingHost::acceptedWrites,
          "repeated unaccepted neighbor changes are bounded instead of an endless setter loop");
    for (unsigned failure = 0; failure != 6; ++failure)
    {
        ped = reset();
        if (failure == 0)
            BleedingHost::failBefore = true;
        if (failure == 1)
            BleedingHost::failAfter = true;
        if (failure == 2)
            BleedingHost::after = [&] { PoolTestHost::ownerResult = ClockResult::Expired; };
        if (failure == 3)
            BleedingHost::after = [&] { fixture.transform.pool.flags[0] = 2; };
        if (failure == 4)
            BleedingHost::after = [&] { fixture.Global(NativeBorrowedGcProfile::markerRva, LONG{1}); };
        if (failure == 5)
            BleedingHost::after = [&] { fixture.Store(0, SF::FlagsOffset, fixture.BleedingBits() & ~SF::BleedingMask); };
        Check(memory.SetBleeding(ped, true, &receipt) != ClockResult::Ok && receipt.entered && !receipt.verified && receipt.attempts == 1 &&
                  BleedingHost::exchanges == 1 && BleedingHost::acceptedWrites == (failure == 0 ? 0u : 1u) &&
                  ((failure >= 1 && failure <= 4) ? (fixture.BleedingBits() & SF::BleedingMask) != 0 : true),
              "entered exception, accepted mutation or lost acknowledgement retains effects and never restores or retries");
    }
    ped = reset();
    Memory::Receipt nested;
    ClockResult     nestedResult = ClockResult::Ok;
    BleedingHost::before = [&] { nestedResult = memory.SetBleeding(ped, false, &nested); };
    Check(memory.SetBleeding(ped, true, &receipt) == ClockResult::Ok && nestedResult == ClockResult::Busy && !nested.entered && BleedingHost::exchanges == 1 &&
              BleedingHost::acceptedWrites == 1,
          "a nested setter cannot borrow the same live native property-write admission");
    for (bool neighborOnly : {false, true})
    {
        ped = reset();
        unsigned seen = 0;
        BleedingHost::afterRead = [&](std::uintptr_t at, std::size_t bytes)
        {
            if (at == ped.nativePed + SF::FlagsOffset && bytes == 4 && ++seen == 1)
                fixture.Store(0, SF::FlagsOffset, fixture.BleedingBits() ^ (neighborOnly ? std::uint32_t{1} : SF::BleedingMask));
        };
        const auto result = memory.SetBleeding(ped, true, &receipt);
        Check(neighborOnly ? (result == ClockResult::Ok && receipt.accepted && (fixture.BleedingBits() & 1))
                           : (result == ClockResult::OwnerConflict && !receipt.entered && !BleedingHost::exchanges),
              "preflight snapshot coherence applies to intended flag without claiming independent neighbors");
    }
    for (const auto neighborOnly : {false, true})
    {
        ped = reset();
        bool     resultValue = true;
        unsigned seen = 0;
        BleedingHost::afterRead = [&](std::uintptr_t at, std::size_t bytes)
        {
            if (at == ped.nativePed + SF::FlagsOffset && bytes == 4 && ++seen == 1)
                fixture.Store(0, SF::FlagsOffset, fixture.BleedingBits() ^ (neighborOnly ? std::uint32_t{1} : SF::BleedingMask));
        };
        const auto result = memory.ReadBleeding(ped, resultValue);
        Check(neighborOnly ? result == ClockResult::Ok && !resultValue : result == ClockResult::OwnerConflict && resultValue,
              "read coherence covers only the named bleeding bit and never publishes a false value on failure");
    }
    ped = reset();
    Check(memory.SetBleeding(ped, true, &receipt) == ClockResult::Ok, "measure operation owner and read boundaries after Resolve setup");
    const auto owners = PoolTestHost::ownerCalls, reads = PoolTestHost::reads, exchangeRead = BleedingHost::exchangeRead;
    unsigned   acceptedExpiry = 0;
    for (unsigned step = 1; step <= owners; ++step)
    {
        ped = reset();
        const auto initial = fixture.BleedingBits();
        PoolTestHost::revokeAt = step;
        Check(memory.SetBleeding(ped, true, &receipt) != ClockResult::Ok && BleedingHost::exchanges <= 1 &&
                  fixture.BleedingBits() == (receipt.accepted ? initial | SF::BleedingMask : initial),
              "every callback owner revocation retains exactly the acknowledged bleeding effect without replay");
        acceptedExpiry += receipt.accepted ? 1u : 0u;
    }
    Check(acceptedExpiry != 0, "owner fault coverage includes persistent accepted effects");
    for (const auto step : {1u, exchangeRead, exchangeRead + 1, reads})
    {
        ped = reset();
        const auto initial = fixture.BleedingBits();
        PoolTestHost::failAt = step;
        Check(memory.SetBleeding(ped, true, &receipt) != ClockResult::Ok && BleedingHost::exchanges <= 1 &&
                  fixture.BleedingBits() == (receipt.accepted ? initial | SF::BleedingMask : initial),
              "early, pre-CAS and post-CAS copy faults neither replay nor reverse persistent bleeding intent");
    }
    for (const auto& pin : SF::Code)
    {
        ped = reset();
        const auto bad = static_cast<unsigned char>(pin.bytes[0] ^ 1);
        fixture.transform.image.Patch(pin.rva, &bad, 1, PAGE_EXECUTE_READ);
        actual = true;
        Check(memory.ReadBleeding(ped, actual) != ClockResult::Ok && actual && memory.SetBleeding(ped, true, &receipt) != ClockResult::Ok && !receipt.entered &&
                  !BleedingHost::exchanges,
              "named producer, actual bool-select store and common initializer are all required before property admission");
        fixture.transform.image.Patch(pin.rva, pin.bytes, 1, PAGE_EXECUTE_READ);
    }
    ped = reset();
    ClockResult foreign = ClockResult::Ok;
    std::thread other([&] { foreign = memory.SetBleeding(ped, true, &receipt); });
    other.join();
    Check(foreign != ClockResult::Ok && !receipt.entered && !BleedingHost::exchanges, "foreign thread cannot mutate the native ped property");
    ped = reset();
    Check(memory.SetBleeding(ped, true, &receipt) == ClockResult::Ok && receipt.verified, "write scope releases after every bounded failure path");
    std::puts("Production native ped bleeding primitive fixture completed");
    std::printf("Passed %u native ped-bleeding checks (64-bit; no native engine code executed)\n", checks);
    return 0;
}
