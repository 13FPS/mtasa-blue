/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual named collision-use mask over genuine owned generic/player peds
 ****************************************************************************/
#pragma push_macro("main")
#undef main
#define main NativePedCollisionHistoricalDuckingMain
#include "NativePedDuckingTests.cpp"
#undef main
#pragma pop_macro("main")
#include "../../Client/game_de/NativePedCollisionMemory.h"
#include "../../Client/game_de/NativePedCollisionProfile.h"
#include <functional>
#include <intrin.h>
namespace CollisionHost
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
#define GetModuleHandleW              PoolTestHost::Module
#define ReadProcessMemory             CollisionHost::Read
#define MTA_DE_PED_COLLISION_EXCHANGE CollisionHost::Exchange
#include "../../Client/game_de/NativePedCollisionMemory.cpp"
#undef MTA_DE_PED_COLLISION_EXCHANGE
#undef ReadProcessMemory
#undef GetModuleHandleW
namespace
{
    namespace SF = NativePedCollisionProfile;
    struct CollisionFixture : DuckingFixture
    {
        explicit CollisionFixture(const std::filesystem::path& directory) : DuckingFixture(directory)
        {
            for (const auto& pin : SF::Code)
                transform.image.Patch(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
            const auto group = transform.image.image + SF::GroupRva;
            transform.image.Patch(SF::GroupSlotRva, &group, sizeof(group), PAGE_READONLY);
            const auto branch = static_cast<std::uint32_t>(SF::CaseRva);
            transform.image.Patch(SF::TableRva + SF::CaseIndex * 4, &branch, sizeof(branch), PAGE_EXECUTE_READ);
            Reset();
        }
        void Reset()
        {
            DuckingFixture::Reset();
            for (const auto slot : {std::size_t{0}, std::size_t{8}})
                Store(slot, SF::FlagsOffset, std::uint32_t{0xa1123400});
            CollisionHost::Reset();
        }
        std::uint32_t CollisionBits(std::size_t slot = 0) const
        {
            std::uint32_t bits = 0;
            std::memcpy(&bits, transform.pool.objects + slot * NativePedPoolProfile::ObjectStride + SF::FlagsOffset, sizeof(bits));
            return bits;
        }
    };
}
int main(int argc, char** argv)
{
    using Memory = NativePedCollisionMemory;
    Check(argc == 2 && std::filesystem::path(argv[1]).is_absolute(), "collision-use evidence uses an explicit workspace-owned directory");
    Memory::Binding refused{123};
    Check(!Memory::Qualify(refused) && refused.image == 123, "actual unsupported executable never becomes an admitted collision-use binding");
    CollisionFixture fixture(argv[1]);
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
    struct DefaultAccess final : NativePedCollisionAccess
    {
    } defaultAccess;
    Check(defaultAccess.SetPedUsesCollision(NativePedReferenceToken{}, true) == ClockResult::NotInitialized,
          "unbound registered-token access refuses the actual setter rather than accepting a fake native effect");
    const std::array<std::uint32_t, 6> seeds{0, 0xffffffff, SF::UsesCollisionMask, ~SF::UsesCollisionMask, 0x13579bdf, 0x2468ace0};
    for (const auto slot : {std::size_t{0}, std::size_t{8}})
        for (const auto bits : seeds)
            for (bool enabled : {false, true})
            {
                fixture.Reset();
                fixture.Store(slot, SF::FlagsOffset, bits);
                const auto ped = fixture.Ped(slot == 0 ? 1 : (8u << 8) | 127u);
                const auto target = (bits & ~SF::UsesCollisionMask) | (enabled ? SF::UsesCollisionMask : 0);
                Check(memory.SetUsesCollision(ped, enabled, &receipt) == ClockResult::Ok && receipt.entered && receipt.accepted && receipt.verified &&
                          receipt.observedValid && receipt.original == bits && receipt.target == target && receipt.observed == target &&
                          receipt.attempts == 1 && receipt.handle == ped.handle && receipt.nativePed == ped.nativePed &&
                          receipt.address == ped.nativePed + SF::FlagsOffset && fixture.CollisionBits(slot) == target && CollisionHost::exchanges == 1 &&
                          CollisionHost::acceptedWrites == 1,
                      "one genuine masked DWORD CAS implements the named bool while every other bit is preserved");
            }
    auto ped = reset();
    Check(unowned.SetUsesCollision(ped, true, &receipt) == ClockResult::NotInitialized && !receipt.entered && !CollisionHost::exchanges,
          "readable native ped memory cannot replace callback authority for collision use");
    ped = reset();
    const auto original = fixture.CollisionBits();
    CollisionHost::before = [&]
    {
        if (CollisionHost::exchanges == 1)
            fixture.Store(0, SF::FlagsOffset, fixture.CollisionBits() ^ std::uint32_t{2});
    };
    Check(memory.SetUsesCollision(ped, true, &receipt) == ClockResult::OwnerConflict && receipt.entered && !receipt.accepted && !receipt.verified &&
              receipt.attempts == 1 && CollisionHost::exchanges == 1 && !CollisionHost::acceptedWrites && fixture.CollisionBits() == (original ^ 2),
          "racing neighboring flags preserve the engine word and refuse this intent after one CAS without retry");
    ped = reset();
    CollisionHost::after = [&] { fixture.Store(0, SF::FlagsOffset, fixture.CollisionBits() ^ std::uint32_t{0x80000002}); };
    Check(memory.SetUsesCollision(ped, true, &receipt) == ClockResult::Ok && receipt.accepted && receipt.verified && receipt.attempts == 1 &&
              CollisionHost::acceptedWrites == 1 && (fixture.CollisionBits() & SF::UsesCollisionMask) && fixture.CollisionBits() != receipt.target,
          "post-acceptance changes to unowned flags remain intact and do not trigger an accepted-write replay");
    ped = reset();
    CollisionHost::before = [&] { fixture.Store(0, SF::FlagsOffset, fixture.CollisionBits() ^ SF::UsesCollisionMask); };
    Check(memory.SetUsesCollision(ped, true, &receipt) == ClockResult::OwnerConflict && receipt.entered && !receipt.accepted && receipt.attempts == 1 &&
              !CollisionHost::acceptedWrites && (fixture.CollisionBits() & SF::UsesCollisionMask),
          "competing change to the intended flag is never overwritten or retried");
    ped = reset();
    CollisionHost::before = [&] { fixture.Store(0, SF::FlagsOffset, fixture.CollisionBits() ^ std::uint32_t{2}); };
    Check(memory.SetUsesCollision(ped, true, &receipt) == ClockResult::OwnerConflict && receipt.entered && !receipt.accepted &&
              receipt.attempts == SF::MaximumAttempts && CollisionHost::exchanges == SF::MaximumAttempts && !CollisionHost::acceptedWrites,
          "repeated unaccepted neighbor changes are bounded instead of an endless setter loop");
    for (unsigned failure = 0; failure != 6; ++failure)
    {
        ped = reset();
        if (failure == 0)
            CollisionHost::failBefore = true;
        if (failure == 1)
            CollisionHost::failAfter = true;
        if (failure == 2)
            CollisionHost::after = [&] { PoolTestHost::ownerResult = ClockResult::Expired; };
        if (failure == 3)
            CollisionHost::after = [&] { fixture.transform.pool.flags[0] = 2; };
        if (failure == 4)
            CollisionHost::after = [&] { fixture.Global(NativeBorrowedGcProfile::markerRva, LONG{1}); };
        if (failure == 5)
            CollisionHost::after = [&] { fixture.Store(0, SF::FlagsOffset, fixture.CollisionBits() & ~SF::UsesCollisionMask); };
        Check(memory.SetUsesCollision(ped, true, &receipt) != ClockResult::Ok && receipt.entered && !receipt.verified && receipt.attempts == 1 &&
                  CollisionHost::exchanges == 1 && CollisionHost::acceptedWrites == (failure == 0 ? 0u : 1u) &&
                  ((failure >= 1 && failure <= 4) ? (fixture.CollisionBits() & SF::UsesCollisionMask) != 0 : true),
              "entered exception, accepted mutation or lost acknowledgement retains effects and never restores or retries");
    }
    ped = reset();
    Memory::Receipt nested;
    ClockResult     nestedResult = ClockResult::Ok;
    CollisionHost::before = [&] { nestedResult = memory.SetUsesCollision(ped, false, &nested); };
    Check(memory.SetUsesCollision(ped, true, &receipt) == ClockResult::Ok && nestedResult == ClockResult::Busy && !nested.entered &&
              CollisionHost::exchanges == 1 && CollisionHost::acceptedWrites == 1,
          "a nested setter cannot borrow the same live native property-write admission");
    for (bool neighborOnly : {false, true})
    {
        ped = reset();
        unsigned seen = 0;
        CollisionHost::afterRead = [&](std::uintptr_t at, std::size_t bytes)
        {
            if (at == ped.nativePed + SF::FlagsOffset && bytes == 4 && ++seen == 1)
                fixture.Store(0, SF::FlagsOffset, fixture.CollisionBits() ^ (neighborOnly ? std::uint32_t{2} : SF::UsesCollisionMask));
        };
        const auto result = memory.SetUsesCollision(ped, true, &receipt);
        Check(neighborOnly ? (result == ClockResult::Ok && receipt.accepted && (fixture.CollisionBits() & 2))
                           : (result == ClockResult::OwnerConflict && !receipt.entered && !CollisionHost::exchanges),
              "preflight snapshot coherence applies to intended flag without claiming independent neighbors");
    }
    for (const auto& pin : SF::Code)
    {
        ped = reset();
        const auto bad = static_cast<unsigned char>(pin.bytes[0] ^ 1);
        fixture.transform.image.Patch(pin.rva, &bad, 1, PAGE_EXECUTE_READ);
        Check(memory.SetUsesCollision(ped, true, &receipt) != ClockResult::Ok && !receipt.entered && !CollisionHost::exchanges,
              "named producer/prefix/return, default constructor and actual contact consumer all remain required before property admission");
        fixture.transform.image.Patch(pin.rva, pin.bytes, 1, PAGE_EXECUTE_READ);
    }
    ped = reset();
    ClockResult foreign = ClockResult::Ok;
    std::thread other([&] { foreign = memory.SetUsesCollision(ped, true, &receipt); });
    other.join();
    Check(foreign != ClockResult::Ok && !receipt.entered && !CollisionHost::exchanges, "foreign thread cannot mutate the native ped property");
    ped = reset();
    Check(memory.SetUsesCollision(ped, true, &receipt) == ClockResult::Ok && receipt.verified, "write scope releases after every bounded failure path");
    std::puts("Production native ped uses-collision primitive fixture completed");
    std::printf("Passed %u native ped-uses-collision checks (64-bit; no native engine code executed)\n", checks);
    return 0;
}
