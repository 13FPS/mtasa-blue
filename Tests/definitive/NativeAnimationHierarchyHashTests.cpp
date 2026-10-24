/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Fresh hierarchy-bank hash fields without fabricating a hierarchy
 ****************************************************************************/
#pragma push_macro("main")
#undef main
#define main HistoricalAnimationHierarchyHashHostMain
#include "NativeAnimationBlockFactoryTests.cpp"
#undef main
#pragma pop_macro("main")
#include "../../Client/game_de/NativeAnimationHierarchyHashMemory.h"
namespace AnimationHierarchyHashTestHost
{
    std::uintptr_t  hashAddress = 0;
    std::uintptr_t* caller = nullptr;
    unsigned        hashReads = 0, failHashAt = 0, hashChecks = 0;
    void (*afterHash)() = nullptr;
    void Verify(bool condition, const char* message)
    {
        ++hashChecks;
        Check(condition, message);
    }
    void ChangeHash()
    {
        *reinterpret_cast<std::uint32_t*>(hashAddress) ^= 0x01020304;
    }
    void ChangeOwner()
    {
        *reinterpret_cast<std::uint32_t*>(AnimationBlockFactoryHost::Row(6) + AnimationBlockFactoryHost::Hierarchy::OwningBlockOffset) = 0;
    }
    void ChangeRange()
    {
        AnimationBlockFactoryHost::fixture->Seed(1, 1, 7, 1);
    }
    void ChangeHighwater()
    {
        AnimationBlockFactoryHost::fixture->Cell(AnimationBlockFactoryHost::Manager::AnimationCountRva, std::int32_t{13});
    }
    void LoseOwner()
    {
        PoolTestHost::ownerResult = ClockResult::WrongThread;
    }
    void ChangeCaller()
    {
        *caller = AnimationBlockFactoryHost::Row(7);
    }
}
namespace mta::game_de::NativeImageAccess
{
    bool HierarchyHashFixtureReadImage(std::uintptr_t image, std::uintptr_t address, void* destination, std::size_t bytes, bool writable = false,
                                       bool executable = false) noexcept
    {
        using namespace AnimationHierarchyHashTestHost;
        const bool selected = address == hashAddress && bytes == sizeof(std::uint32_t);
        if (selected && ++hashReads == failHashAt)
            return false;
        if (!ReadImage(image, address, destination, bytes, writable, executable))
            return false;
        if (selected && afterHash)
        {
            const auto action = afterHash;
            afterHash = nullptr;
            action();
        }
        return true;
    }
}
#define ReadImage HierarchyHashFixtureReadImage
#include "../../Client/game_de/NativeAnimationHierarchyHashMemory.cpp"
#undef ReadImage
int main(int argc, char** argv)
{
    using namespace AnimationHierarchyHashTestHost;
    namespace Host = AnimationBlockFactoryHost;
    using Memory = mta::game_de::NativeAnimationHierarchyHashMemory;
    Verify(argc == 2 && std::filesystem::path(argv[1]).is_absolute(), "hash evidence stays in its explicit owned output");
    BlockFixture owned(argv[1]);
    Host::fixture = &owned;
    const auto reset = [&]
    {
        Host::Reset();
        hashAddress = Host::Row(6);
        hashReads = failHashAt = 0;
        afterHash = nullptr;
        caller = nullptr;
    };
    reset();
    const Memory  memory({PoolTestHost::image}, &PoolTestHost::Owner);
    std::uint32_t output = 0xdeadbeef;
    for (const std::uint32_t value : {0u, 0x80000000u, 0xffffffffu, 0x13579bdfu})
    {
        reset();
        std::memcpy(reinterpret_cast<void*>(hashAddress), &value, sizeof(value));
        Verify(memory.Read(Host::Row(6), output) == ClockResult::Ok && output == value,
               "current native bank hash preserves the complete DWORD rather than a signed conversion or synthetic gateway classification");
    }
    reset();
    owned.Seed(1, 0, 5, 3);
    std::memset(reinterpret_cast<void*>(owned.Row(1)), 0x5a, 16);
    Verify(memory.Read(Host::Row(6), output) == ClockResult::Ok && output == 0x13579bdf,
           "unloaded block with surviving current range and raw nonterminated name still admits its source-preserved hash field");
    for (const std::uintptr_t locator : {std::uintptr_t{0}, Host::Row(6) + 1, Host::Row(6) + 4, Host::Row(0) - Host::Hierarchy::RowBytes, Host::Row(12),
                                         Host::Row(6) + (std::uintptr_t{1} << 32), (std::numeric_limits<std::uintptr_t>::max)()})
    {
        reset();
        output = 0xdeadbeef;
        Verify(memory.Read(locator, output) != ClockResult::Ok && output == 0xdeadbeef,
               "null, interior, before-bank, highwater, full-width foreign and overflowing locators preserve output rather than adopting a custom hierarchy");
    }
    for (unsigned refusal = 0; refusal != 6; ++refusal)
    {
        reset();
        if (refusal == 0)
            owned.Cell(Host::Manager::AnimationCountRva, std::int32_t{-1});
        if (refusal == 1)
            owned.Cell(A::CountRva, std::int32_t{1});
        if (refusal == 2)
            *reinterpret_cast<std::uint32_t*>(Host::Row(6) + Host::Hierarchy::OwningBlockOffset) = A::ObservationLimit;
        if (refusal == 3)
            owned.Seed(1, 1, -1, 3);
        if (refusal == 4)
            owned.Seed(1, 1, 5, 0);
        if (refusal == 5)
            owned.Seed(1, 1, 7, 1);
        output = 0xdeadbeef;
        Verify(memory.Read(Host::Row(6), output) != ClockResult::Ok && output == 0xdeadbeef,
               "current highwater, owning block and actual start/count containment are required to qualify field origin");
    }
    for (const unsigned failedRead : {1u, 2u})
    {
        reset();
        failHashAt = failedRead;
        output = 0xdeadbeef;
        Verify(memory.Read(Host::Row(6), output) != ClockResult::Ok && output == 0xdeadbeef,
               "first and final hash-field observation failures cannot publish a partial DWORD");
    }
    for (const auto transition : {&ChangeHash, &ChangeOwner, &ChangeRange, &ChangeHighwater, &LoseOwner})
    {
        reset();
        afterHash = transition;
        output = 0xdeadbeef;
        Verify(memory.Read(Host::Row(6), output) != ClockResult::Ok && output == 0xdeadbeef,
               "hash, row owner, range, highwater or callback loss between snapshots preserves the caller value");
    }
    reset();
    auto locator = Host::Row(6);
    caller = &locator;
    afterHash = &ChangeCaller;
    Verify(memory.Read(locator, output) == ClockResult::Ok && output == 0x13579bdf && locator == Host::Row(7),
           "reader keeps its copied full-width locator when caller state changes during synchronous observation");
    reset();
    ClockResult foreign = ClockResult::Ok;
    output = 0xdeadbeef;
    std::thread thread([&] { foreign = memory.Read(Host::Row(6), output); });
    thread.join();
    Verify(foreign != ClockResult::Ok && output == 0xdeadbeef, "a CPU callback cannot substitute for actual native GameThread admission");
    reset();
    DWORD previous = 0, ignored = 0;
    Verify(VirtualProtect(reinterpret_cast<void*>(Host::Row(6)), Host::Hierarchy::RowBytes, PAGE_READONLY, &previous) != FALSE,
           "restrict only the owned hierarchy row");
    output = 0xdeadbeef;
    Verify(memory.Read(Host::Row(6), output) != ClockResult::Ok && output == 0xdeadbeef,
           "readonly hierarchy-shaped storage is not adopted as the current native RW bank");
    Verify(VirtualProtect(reinterpret_cast<void*>(Host::Row(6)), Host::Hierarchy::RowBytes, previous, &ignored) != FALSE,
           "restore owned hierarchy row protection");
    std::puts("Production native animation hierarchy hash fixture completed");
    std::printf("Passed %u native animation hierarchy hash checks (64-bit; no native engine code executed)\n", hashChecks);
    return 0;
}
