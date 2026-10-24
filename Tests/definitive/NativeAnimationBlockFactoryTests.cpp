/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Only current block identity/opaque hierarchy borrow checks
 ****************************************************************************/
#pragma push_macro("main")
#undef main
#define main NativeAnimationBlockFactoryHistoricalMetadataMain
#include "NativeAnimationBlockTests.cpp"
#undef main
#pragma pop_macro("main")
#include "../../Client/game_de/NativeAnimationBlockFactoryMemory.h"
#include "NativeAnimationBlockFactoryFixture.h"
#define ReadProcessMemory PoolTestHost::Read
#include "../../Client/game_de/NativeAnimationManagerMemory.cpp"
#include "../../Client/game_de/NativeAnimationBlockFactoryMemory.cpp"
#undef ReadProcessMemory
namespace AnimationBlockFactoryHost
{
    using FactoryMemory = mta::game_de::NativeAnimationBlockFactoryMemory;
    namespace Hierarchy = mta::game_de::NativeAnimationBlockHierarchyProfile;
    namespace Manager = mta::game_de::NativeAnimationManagerProfile;
    BlockFixture*                 fixture = nullptr;
    NativeAnimationBlockIdentity* caller = nullptr;
    void                          Write(void* host, std::uintptr_t rva, const void* bytes, std::size_t size, DWORD protection)
    {
        static_cast<BlockFixture*>(host)->bank.transform.image.Patch(rva, bytes, size, protection);
    }
    std::uintptr_t Row(std::uint32_t index)
    {
        return fixture->bank.transform.image.image + Hierarchy::BankRva + static_cast<std::uintptr_t>(index) * Hierarchy::RowBytes;
    }
    void Seed(std::uint32_t index, std::uint32_t block)
    {
        NativeAnimationBlockFactoryFixture::SeedHierarchy(fixture->bank.transform.image.image, &Write, fixture, index, block);
    }
    void Reset()
    {
        fixture->Reset();
        PoolTestHost::image = fixture->bank.transform.image.image;
        NativeAnimationBlockFactoryFixture::Install(PoolTestHost::image, &Write, fixture);
        fixture->Cell(Manager::AnimationCountRva, std::int32_t{12});
        fixture->Seed(1, 1, 5, 3);
        for (std::uint32_t index = 5; index != 8; ++index)
            Seed(index, 1);
        Seed(4, 0);
        Seed(8, 2);
        caller = nullptr;
        PoolTestHost::reads = PoolTestHost::ownerCalls = 0;
    }
    NativeAnimationBlockIdentity FactorySentinel()
    {
        NativeAnimationBlockIdentity value;
        value.index = 0xdeadbeef;
        value.name.fill(0xa5);
        value.ownerGeneration = 0x12345678;
        return value;
    }
    bool FactoryUntouched(const NativeAnimationBlockIdentity& value)
    {
        const auto expected = FactorySentinel();
        return value.index == expected.index && value.name == expected.name && value.ownerGeneration == expected.ownerGeneration;
    }
    void ChangeNativeName()
    {
        *reinterpret_cast<unsigned char*>(fixture->Row(1)) = 'X';
    }
    void ChangeCaller()
    {
        caller->index = 0xffffffffu;
        caller->name.fill('X');
        caller->ownerGeneration = 0;
    }
    unsigned mutation = 0;
    void     ChangeBorrowState()
    {
        if (mutation == 0)
            *reinterpret_cast<std::uint8_t*>(fixture->Row(1) + A::LoadedOffset) = 0;
        else if (mutation == 1)
            *reinterpret_cast<std::int32_t*>(fixture->Row(1) + A::IdOffset) = 6;
        else if (mutation == 2)
            fixture->Cell(Manager::AnimationCountRva, std::int32_t{13});
        else if (mutation == 3)
            *reinterpret_cast<std::uint32_t*>(Row(6) + Hierarchy::OwningBlockOffset) = 2;
        else if (mutation == 4)
            ChangeNativeName();
        else
            PoolTestHost::ownerResult = ClockResult::WrongThread;
    }
}
int main(int argc, char** argv)
{
    using namespace AnimationBlockFactoryHost;
    Check(argc == 2 && std::filesystem::path(argv[1]).is_absolute(), "factory proof uses its explicit workspace output");
    BlockFixture owned(argv[1]);
    fixture = &owned;
    Reset();
    const FactoryMemory::Binding binding{PoolTestHost::image};
    const FactoryMemory          memory(binding, &PoolTestHost::Owner), unowned(binding, nullptr);
    auto                         observed = FactorySentinel();
    Check(unowned.ReadIdentity(1, observed) == ClockResult::NotInitialized && FactoryUntouched(observed),
          "native row presence does not issue callback affinity");
    for (const auto index : {0u, 1u, A::ObservationLimit - 1})
        for (const auto loaded : {std::uint8_t{0}, std::uint8_t{1}})
        {
            Reset();
            owned.Cell(A::CountRva, static_cast<std::int32_t>(A::ObservationLimit));
            owned.Seed(index, loaded, -17, 0xffffffffu);
            auto actual = Identity(index).name;
            actual[0] = 'p';
            actual[2] = 0xe9;
            actual[3] = 0;
            actual[4] = 0x81;
            std::memcpy(reinterpret_cast<void*>(owned.Row(index)), actual.data(), actual.size());
            observed = FactorySentinel();
            Check(
                memory.ReadIdentity(index, observed) == ClockResult::Ok && observed.index == index && observed.name == actual && observed.ownerGeneration == 0,
                "factory copies complete actual native spelling and no CPU generation even for unloaded first/last rows");
            observed = FactorySentinel();
            Check(memory.ResolveInterface(owned.Row(index), observed) == ClockResult::Ok && observed.index == index && observed.name == actual &&
                      observed.ownerGeneration == 0,
                  "opaque locator selects exact current row and reacquires native identity without an issuance registry");
        }
    for (const auto invalid : {A::ObservationLimit, 0xffffffffu})
    {
        Reset();
        observed = FactorySentinel();
        Check(memory.ReadIdentity(invalid, observed) == ClockResult::InvalidArgument && FactoryUntouched(observed) && !PoolTestHost::reads,
              "full-width invalid index never aliases a qualified row");
    }
    Reset();
    for (const auto invalid : {std::uintptr_t{0}, owned.Row(0) - 1, owned.Row(0) + 1, owned.Row(1) + 31, owned.Row(A::ObservationLimit)})
    {
        observed = FactorySentinel();
        Check(memory.ResolveInterface(invalid, observed) == ClockResult::InvalidArgument && FactoryUntouched(observed),
              "null/foreign/misaligned/out-of-domain locator never becomes block authority");
    }
    observed = FactorySentinel();
    Check(memory.ResolveInterface(owned.Row(3), observed) == ClockResult::NotInitialized && FactoryUntouched(observed),
          "aligned locator still requires a currently registered native row");
    Reset();
    std::memset(reinterpret_cast<void*>(owned.Row(1)), 'x', 16);
    observed = FactorySentinel();
    Check(memory.ReadIdentity(1, observed) == ClockResult::BindingChanged && FactoryUntouched(observed), "unterminated native row cannot become SDK identity");
    Reset();
    PoolTestHost::watched = owned.Row(1);
    PoolTestHost::after = &ChangeNativeName;
    observed = FactorySentinel();
    Check(memory.ReadIdentity(1, observed) != ClockResult::Ok && FactoryUntouched(observed), "native rename between observations preserves factory output");
    Reset();
    auto           identity = Identity();
    std::uintptr_t output = 0x1234;
    for (const auto loaded : {std::uint8_t{1}, std::uint8_t{0xff}})
        for (const auto index : {std::size_t{0}, std::size_t{2}})
        {
            Reset();
            owned.Seed(1, loaded, 5, 3);
            output = 0x1234;
            const auto result = memory.ReadHierarchy(identity, index, output);
            if (result != ClockResult::Ok)
            {
                std::fprintf(stderr, "hierarchy loaded=%u index=%zu result=%u reads=%u owners=%u\n", static_cast<unsigned>(loaded), index,
                             static_cast<unsigned>(result), PoolTestHost::reads, PoolTestHost::ownerCalls);
                std::fflush(stderr);
            }
            Check(result == ClockResult::Ok && output == Row(static_cast<std::uint32_t>(5 + index)),
                  "loaded first/last owned hierarchy rows use actual40-byte bank and block ownership");
        }
    Reset();
    owned.Seed(1, 0, (std::numeric_limits<std::int32_t>::min)(), 0xffffffffu);
    owned.Cell(Manager::AnimationCountRva, std::int32_t{-1});
    const auto& unused = Hierarchy::Code[1];
    const auto  changed = static_cast<unsigned char>(unused.bytes[0] ^ 1u);
    owned.bank.transform.image.Patch(unused.rva, &changed, 1, PAGE_EXECUTE_READ);
    output = 0x1234;
    Check(memory.ReadHierarchy(identity, (std::numeric_limits<std::size_t>::max)(), output) == ClockResult::Ok && output == 0,
          "unloaded returns null before SIZE_MAX/native range/highwater or unused hierarchy profile checks");
    owned.Seed(1, 1, 5, 3);
    owned.Cell(Manager::AnimationCountRva, std::int32_t{12});
    output = 0x1234;
    Check(memory.ReadHierarchy(identity, 1, output) == ClockResult::BindingChanged && output == 0x1234,
          "the same altered producer pin refuses a loaded hierarchy borrow before pointer publication");
    for (const auto index : {std::size_t{3}, std::size_t{1} << 32, (std::numeric_limits<std::size_t>::max)()})
    {
        Reset();
        output = 0x1234;
        Check(memory.ReadHierarchy(identity, index, output) == ClockResult::InvalidArgument && output == 0x1234,
              "loaded request retains full size_t and cannot truncate into a neighbor hierarchy row");
    }
    for (unsigned invalid = 0; invalid != 7; ++invalid)
    {
        Reset();
        if (invalid == 0)
            owned.Seed(1, 1, -1, 3);
        if (invalid == 1)
            owned.Seed(1, 1, 5, 0);
        if (invalid == 2)
            owned.Seed(1, 1, 5, 0xffffffffu);
        if (invalid == 3)
            owned.Cell(Manager::AnimationCountRva, std::int32_t{7});
        if (invalid == 4)
            owned.Cell(Manager::AnimationCountRva, std::int32_t{0});
        if (invalid == 5)
        {
            owned.Seed(1, 1, (std::numeric_limits<std::int32_t>::max)() - 1, 1);
            owned.Cell(Manager::AnimationCountRva, (std::numeric_limits<std::int32_t>::max)());
        }
        if (invalid == 6)
            Seed(6, 2);
        output = 0x1234;
        Check(memory.ReadHierarchy(identity, invalid == 5 ? 0 : 1, output) == ClockResult::BindingChanged && output == 0x1234,
              "bad native signed range/highwater/image arithmetic or actual row owner never publishes a pointer");
    }
    for (AnimationBlockFactoryHost::mutation = 0; AnimationBlockFactoryHost::mutation != 6; ++AnimationBlockFactoryHost::mutation)
    {
        Reset();
        PoolTestHost::watched = Row(6);
        PoolTestHost::after = &ChangeBorrowState;
        output = 0x1234;
        Check(memory.ReadHierarchy(identity, 1, output) != ClockResult::Ok && output == 0x1234,
              "unload/start/highwater/row owner/name/runtime loss during borrow preserves raw output");
    }
    Reset();
    caller = &identity;
    PoolTestHost::watched = Row(6);
    PoolTestHost::after = &ChangeCaller;
    output = 0x1234;
    Check(memory.ReadHierarchy(identity, 1, output) == ClockResult::Ok && output == Row(6) && identity.index == 0xffffffffu,
          "complete input identity is copied before callbacks and later caller storage changes cannot redirect lookup");
    Reset();
    identity = Identity();
    Check(memory.ReadIdentity(1, observed) == ClockResult::Ok, "measure identity read boundaries once");
    const auto identityReads = PoolTestHost::reads, identityOwners = PoolTestHost::ownerCalls;
    for (const auto failure : {1u, identityReads})
    {
        Reset();
        PoolTestHost::failAt = failure;
        observed = FactorySentinel();
        Check(memory.ReadIdentity(1, observed) != ClockResult::Ok && FactoryUntouched(observed), "first/last distinct identity read failure preserves output");
    }
    for (const auto failure : {1u, identityOwners})
    {
        Reset();
        PoolTestHost::revokeAt = failure;
        observed = FactorySentinel();
        Check(memory.ReadIdentity(1, observed) == ClockResult::WrongThread && FactoryUntouched(observed), "first/last identity owner failure preserves output");
    }
    Reset();
    Check(memory.ReadHierarchy(identity, 1, output) == ClockResult::Ok, "measure hierarchy read boundaries once");
    const auto hierarchyReads = PoolTestHost::reads, hierarchyOwners = PoolTestHost::ownerCalls;
    for (const auto failure : {1u, hierarchyReads})
    {
        Reset();
        PoolTestHost::failAt = failure;
        output = 0x1234;
        Check(memory.ReadHierarchy(identity, 1, output) != ClockResult::Ok && output == 0x1234, "first/last distinct hierarchy read failure preserves output");
    }
    for (const auto failure : {1u, hierarchyOwners})
    {
        Reset();
        PoolTestHost::revokeAt = failure;
        output = 0x1234;
        Check(memory.ReadHierarchy(identity, 1, output) == ClockResult::WrongThread && output == 0x1234, "first/last hierarchy owner failure preserves output");
    }
    Reset();
    DWORD old = 0, ignored = 0;
    Check(VirtualProtect(reinterpret_cast<void*>(Row(6)), Hierarchy::RowBytes, PAGE_READONLY, &old) != FALSE, "restrict selected owned hierarchy page");
    output = 0x1234;
    Check(memory.ReadHierarchy(identity, 1, output) != ClockResult::Ok && output == 0x1234, "readonly page cannot substitute for current writable native row");
    Check(VirtualProtect(reinterpret_cast<void*>(Row(6)), Hierarchy::RowBytes, old, &ignored) != FALSE, "restore selected owned hierarchy page");
    Reset();
    Check(memory.ReadHierarchy(identity, 2, output) == ClockResult::Ok, "observe current pre-unload borrowed address");
    const auto previous = output;
    owned.Seed(1, 0, 5, 3);
    Check(memory.ReadHierarchy(identity, 2, output) == ClockResult::Ok && output == 0, "unloaded block cannot revive a previously borrowed row");
    owned.Seed(1, 1, 10, 2);
    Seed(10, 1);
    Seed(11, 1);
    Check(memory.ReadHierarchy(identity, 1, output) == ClockResult::Ok && output == Row(11) && output != previous,
          "new operation after reload reacquires current native range without a retained hierarchy lease");
    Reset();
    ClockResult foreign = ClockResult::Ok;
    output = 0x1234;
    std::thread other([&] { foreign = memory.ReadHierarchy(identity, 1, output); });
    other.join();
    Check(foreign == ClockResult::WrongThread && output == 0x1234, "a callback cannot substitute for native GameThread qualification");
    std::puts("Production native animation block factory primitive fixture completed");
    std::printf("Passed %u native animation block factory checks (64-bit; no native engine code executed)\n", checks);
    return 0;
}
