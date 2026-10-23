/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Only current goggles association-presence cases on owned ped pages
 ****************************************************************************/
#pragma push_macro("main")
#undef main
#define main NativePedGogglesHistoricalDuckingMain
#include "NativePedDuckingTests.cpp"
#undef main
#pragma pop_macro("main")
#include "../../Client/game_de/NativePedGogglesMemory.h"
#include "../../Client/game_de/NativePedGogglesProfile.h"
#include <functional>
namespace GogglesHost
{
    std::uintptr_t                associationAddress = 0, forbiddenTarget = 0;
    unsigned                      reads = 0, failRead = 0, targetReads = 0;
    std::function<void(unsigned)> after;
    BOOL WINAPI                   Read(HANDLE process, LPCVOID input, LPVOID output, SIZE_T bytes, SIZE_T* copied)
    {
        const auto at = reinterpret_cast<std::uintptr_t>(input);
        if (forbiddenTarget && at == forbiddenTarget)
        {
            ++targetReads;
            if (copied)
                *copied = 0;
            return FALSE;
        }
        const bool association = at == associationAddress && bytes == sizeof(std::uintptr_t);
        if (association && ++reads == failRead)
        {
            if (copied)
                *copied = 0;
            return FALSE;
        }
        const auto result = PoolTestHost::Read(process, input, output, bytes, copied);
        if (result && association && after)
            after(reads);
        return result;
    }
    void Reset()
    {
        reads = failRead = targetReads = 0;
        forbiddenTarget = 0;
        after = {};
    }
}
#define GetModuleHandleW  PoolTestHost::Module
#define ReadProcessMemory GogglesHost::Read
#include "../../Client/game_de/NativePedGogglesMemory.cpp"
#undef ReadProcessMemory
#undef GetModuleHandleW
namespace
{
    namespace GP = NativePedGogglesProfile;
    struct GogglesFixture : DuckingFixture
    {
        explicit GogglesFixture(const std::filesystem::path& directory) : DuckingFixture(directory)
        {
            for (const auto& pin : GP::Code)
                transform.image.Patch(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
            Reset();
        }
        void Reset()
        {
            DuckingFixture::Reset();
            GogglesHost::Reset();
            for (const auto slot : {std::size_t{0}, std::size_t{8}})
            {
                Store(slot, GP::AssociationOffset - 8, std::uintptr_t{0x123456789abcdef0ull});
                Store(slot, GP::AssociationOffset, std::uintptr_t{0});
                Store(slot, GP::AssociationOffset + 8, std::uintptr_t{0xfedcba9876543210ull});
            }
            GogglesHost::associationAddress = reinterpret_cast<std::uintptr_t>(transform.pool.objects) + GP::AssociationOffset;
        }
    };
}
int main(int argc, char** argv)
{
    using Memory = NativePedGogglesMemory;
    Check(argc == 2 && std::filesystem::path(argv[1]).is_absolute(), "goggles evidence uses an explicit workspace-owned directory");
    Memory::Binding unsupported{123};
    Check(!Memory::Qualify(unsupported) && unsupported.image == 123, "actual unsupported executable cannot qualify as the supported image");
    GogglesFixture fixture(argv[1]);
    PoolTestHost::image = fixture.transform.image.image;
    const Memory::Binding binding{fixture.transform.image.image};
    Memory                memory(binding, &PoolTestHost::Owner), unavailable(binding, nullptr);
    const auto            reset = [&](std::size_t slot = 0)
    {
        fixture.Reset();
        GogglesHost::associationAddress += slot * NativePedPoolProfile::ObjectStride;
        return fixture.Ped(slot == 0 ? 1 : (8u << 8) | 127u);
    };
    struct DefaultAccess : NativePedGogglesAccess
    {
    } oldOwner;
    bool output = true;
    Check(oldOwner.ReadPedWearingGoggles(NativePedReferenceToken{}, output) == ClockResult::NotInitialized && output,
          "an absent named goggles owner refuses without fabricating false");
    for (const auto slot : {std::size_t{0}, std::size_t{8}})
        for (const auto bits : std::array<std::uintptr_t, 6>{0, 1, 0x10000, 0xffffffffffffffffull, 0x8000000000000000ull, 0x123456789abcdef0ull})
        {
            const auto ped = reset(slot);
            fixture.Store(slot, GP::AssociationOffset, bits);
            GogglesHost::forbiddenTarget = bits;
            output = bits == 0;
            Check(memory.ReadWearingGoggles(ped, output) == ClockResult::Ok && output == (bits != 0) && GogglesHost::reads == 2 && !GogglesHost::targetReads,
                  "actual generic/player getter observes only association presence; even invalid target bits are never dereferenced");
        }
    auto ped = reset();
    output = true;
    Check(unavailable.ReadWearingGoggles(ped, output) == ClockResult::NotInitialized && output && !GogglesHost::reads,
          "readable native memory cannot replace retained callback authority");
    for (unsigned fail = 1; fail <= 2; ++fail)
    {
        ped = reset();
        GogglesHost::failRead = fail;
        output = true;
        Check(memory.ReadWearingGoggles(ped, output) == ClockResult::ReadFailure && output,
              "either uncopied pointer snapshot refuses while preserving the caller bool");
    }
    for (unsigned mode = 0; mode != 6; ++mode)
    {
        ped = reset();
        output = true;
        if (mode == 0)
            fixture.Store(0, GP::AssociationOffset, std::uintptr_t{1});
        GogglesHost::after = [&](unsigned reads)
        {
            if (reads != 1)
                return;
            if (mode == 0)
                fixture.Store(0, GP::AssociationOffset, std::uintptr_t{2});
            if (mode == 1)
                fixture.Store(0, GP::AssociationOffset, std::uintptr_t{1});
            if (mode == 2)
                PoolTestHost::ownerResult = ClockResult::Expired;
            if (mode == 3)
                fixture.transform.pool.flags[0] = 2;
            if (mode == 4)
                fixture.Global(NativeBorrowedGcProfile::markerRva, LONG{1});
            if (mode == 5)
                fixture.Store(0, 0x6a, static_cast<unsigned char>(0xfa));
        };
        Check(memory.ReadWearingGoggles(ped, output) != ClockResult::Ok && output,
              "changed full association, occupied lifetime, owner, GC or metadata refuses without cached/false fallback");
    }
    for (const auto& pin : GP::Code)
    {
        ped = reset();
        output = true;
        const auto changed = static_cast<unsigned char>(pin.bytes[0] ^ 1);
        fixture.transform.image.Patch(pin.rva, &changed, 1, PAGE_EXECUTE_READ);
        Check(memory.ReadWearingGoggles(ped, output) != ClockResult::Ok && output && !GogglesHost::reads,
              "named goggles consumer, direct clear and common initialization all remain mandatory before reads");
        fixture.transform.image.Patch(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
    }
    ped = reset();
    output = true;
    ClockResult foreign = ClockResult::Ok;
    std::thread thread([&] { foreign = memory.ReadWearingGoggles(ped, output); });
    thread.join();
    Check(foreign != ClockResult::Ok && output && !GogglesHost::reads, "foreign thread cannot borrow the association query");
    std::puts("Production native ped goggles query fixture completed");
    std::printf("Passed %u native ped-goggles checks (64-bit; no native engine code executed)\n", checks);
    return 0;
}
