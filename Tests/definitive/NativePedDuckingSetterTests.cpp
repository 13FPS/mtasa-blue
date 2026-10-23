/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: New raw ducking setter cases over the actual occupied-ped fixture
 ****************************************************************************/
#include <cstdint>
namespace DuckingSetterHost
{
    bool Exchange(std::uintptr_t, std::uint32_t, std::uint32_t, std::uint32_t&, std::uint32_t&) noexcept;
}
// Compile the existing real read/pool/metadata implementation once. Its old
// main is not executed; only the final atomic-write boundary is substituted.
#define MTA_DE_PED_DUCKING_EXCHANGE DuckingSetterHost::Exchange
#pragma push_macro("main")
#undef main
#define main NativePedDuckingSetterHistoricalReadMain
#include "NativePedDuckingTests.cpp"
#undef main
#pragma pop_macro("main")
#undef MTA_DE_PED_DUCKING_EXCHANGE
#include <functional>
#include <intrin.h>
namespace DuckingSetterHost
{
    unsigned              attempts = 0, accepted = 0;
    bool                  failBefore = false, failAfter = false;
    std::function<void()> before, after;
    bool Exchange(std::uintptr_t address, std::uint32_t expected, std::uint32_t target, std::uint32_t& observed, std::uint32_t& exception) noexcept
    {
        ++attempts;
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
        const auto result = _InterlockedCompareExchange(reinterpret_cast<volatile LONG*>(address), desired, old);
        std::memcpy(&observed, &result, sizeof(observed));
        if (observed == expected)
            ++accepted;
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
        attempts = accepted = 0;
        failBefore = failAfter = false;
        before = after = {};
    }
}
int main(int argc, char** argv)
{
    using Memory = mta::game_de::NativePedDuckingMemory;
    namespace Host = DuckingSetterHost;
    Check(argc == 2 && std::filesystem::path(argv[1]).is_absolute(), "new ducking setter uses explicit workspace evidence");
    DuckingFixture fixture(argv[1]);
    PoolTestHost::image = fixture.transform.image.image;
    const Memory::Binding binding{fixture.transform.image.image};
    Memory                memory(binding, &PoolTestHost::Owner), unavailable(binding, nullptr);
    Memory::Receipt       receipt;
    const auto            reset = [&](std::size_t slot = 0)
    {
        fixture.Reset();
        Host::Reset();
        return fixture.Ped(slot == 0 ? 1 : (8u << 8) | 127u);
    };
    for (const auto slot : {std::size_t{0}, std::size_t{8}})
        for (const auto original : {std::uint32_t{0}, 0xffffffffu, 0xa1b2c3d4u, 0x04000000u, 0x80000000u})
            for (bool enabled : {false, true})
            {
                const auto ped = reset(slot);
                fixture.Store(slot, DP::FlagsOffset, original);
                const auto target = (original & ~DP::DuckingMask) | (enabled ? DP::DuckingMask : 0);
                Check(memory.SetDucking(ped, enabled, &receipt) == ClockResult::Ok && receipt.entered && receipt.accepted && receipt.verified &&
                          receipt.attempts == 1 && Host::attempts == 1 && Host::accepted == 1 && fixture.Flags(slot) == target,
                      "actual generic/player ducking bool preserves every neighboring DWORD bit with one CAS");
                bool observed = !enabled;
                Check(memory.ReadDucking(ped, observed) == ClockResult::Ok && observed == enabled && Host::attempts == 1,
                      "focused existing read sanity observes the accepted bit without a second write");
            }
    auto ped = reset();
    Check(unavailable.SetDucking(ped, true, &receipt) == ClockResult::NotInitialized && !receipt.entered && !Host::attempts,
          "readable private ped memory never grants callback write authority");
    for (const auto racing : {std::uint32_t{1}, DP::DuckingMask})
    {
        ped = reset();
        Host::before = [&] { fixture.Store(0, DP::FlagsOffset, fixture.Flags() ^ racing); };
        Check(memory.SetDucking(ped, true, &receipt) == ClockResult::OwnerConflict && receipt.entered && !receipt.accepted && receipt.attempts == 1 &&
                  Host::attempts == 1 && !Host::accepted && fixture.Flags() == racing,
              "neighbor or owned-bit races refuse once, retaining the engine word without retry");
    }
    ped = reset();
    Host::after = [&] { fixture.Store(0, DP::FlagsOffset, fixture.Flags() ^ std::uint32_t{0x80000001}); };
    Check(memory.SetDucking(ped, true, &receipt) == ClockResult::Ok && receipt.accepted && receipt.verified &&
              fixture.Flags() == (DP::DuckingMask | 0x80000001) && Host::attempts == 1,
          "accepted ducking change does not own or repair subsequently changed neighboring flags");
    for (unsigned mode = 0; mode != 6; ++mode)
    {
        ped = reset();
        if (mode == 0)
            Host::failBefore = true;
        if (mode == 1)
            Host::failAfter = true;
        if (mode == 2)
            Host::after = [&] { PoolTestHost::ownerResult = ClockResult::Expired; };
        if (mode == 3)
            Host::after = [&] { fixture.transform.pool.flags[0] = 2; };
        if (mode == 4)
            Host::after = [&] { fixture.Global(NativeBorrowedGcProfile::markerRva, LONG{1}); };
        if (mode == 5)
            Host::after = [&] { fixture.Store(0, DP::FlagsOffset, fixture.Flags() ^ DP::DuckingMask); };
        const auto result = memory.SetDucking(ped, true, &receipt);
        Check(result != ClockResult::Ok && receipt.entered && !receipt.verified && receipt.attempts == 1 && Host::attempts == 1 &&
                  Host::accepted == (mode == 0 ? 0u : 1u) && fixture.Flags() == (mode == 0 || mode == 5 ? 0u : DP::DuckingMask),
              "entered failure retains accepted or unknown effects without rollback, replay or a second CAS");
    }
    ped = reset();
    DuckingHost::failFlagRead = 1;
    Check(memory.SetDucking(ped, true, &receipt) == ClockResult::ReadFailure && !receipt.entered && !Host::attempts,
          "uncopied source flags refuse before entry");
    ped = reset();
    Host::before = [&]
    {
        Memory::Receipt nested;
        Check(memory.SetDucking(ped, false, &nested) == ClockResult::Busy && !nested.entered, "same-operation recursion refuses before nested entry");
    };
    Check(memory.SetDucking(ped, true, &receipt) == ClockResult::Ok && Host::attempts == 1, "outer accepted write survives refused recursion");
    for (const auto& pin : DP::Code)
    {
        ped = reset();
        const auto bad = static_cast<unsigned char>(pin.bytes[0] ^ 1);
        fixture.transform.image.Patch(pin.rva, &bad, 1, PAGE_EXECUTE_READ);
        Check(memory.SetDucking(ped, true, &receipt) != ClockResult::Ok && !receipt.entered && !Host::attempts,
              "genuine named read and common constructor remain mandatory setter identity proofs");
        fixture.transform.image.Patch(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
    }
    ped = reset();
    ClockResult foreign = ClockResult::Ok;
    std::thread thread([&] { foreign = memory.SetDucking(ped, true); });
    thread.join();
    Check(foreign != ClockResult::Ok && !Host::attempts, "foreign thread cannot acquire native ducking write authority");
    std::puts("Production native ped ducking setter fixture completed");
    std::printf("Passed %u native ped-ducking-setter checks (64-bit; no native engine code executed)\n", checks);
    return 0;
}
