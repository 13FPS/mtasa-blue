/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Only new immediate borrowed-name tests on actual owned image rows
 ****************************************************************************/
#pragma push_macro("main")
#undef main
#define main AnimationBlockMetadataHistoricalScaffoldMain
#include "NativeAnimationBlockTests.cpp"
#undef main
#pragma pop_macro("main")
namespace AnimationBlockNameIdentityHost
{
    NativeAnimationBlockIdentity* original = nullptr;
    unsigned                      mutateAt = 0, kind = 0;
    ClockResult                   Owner() noexcept
    {
        const auto result = PoolTestHost::Owner();
        if (result == ClockResult::Ok && original && PoolTestHost::ownerCalls == mutateAt)
        {
            // Mutate caller-owned CPU identity at the final real owner fence,
            // after native row/index/name validation has already completed.
            if (kind == 0)
                original->index = 0xffffffffu;
            else if (kind == 1)
                original->name[0] ^= 0x40u;
            else
                original->ownerGeneration ^= 0x8000000000000000ull;
            original = nullptr;
        }
        return result;
    }
}
int main(int argc, char** argv)
{
    Check(argc == 2 && std::filesystem::path(argv[1]).is_absolute(), "borrowed name tests use explicit workspace output");
    BlockFixture fixture(argv[1]);
    changingBlock = &fixture;
    PoolTestHost::image = fixture.bank.transform.image.image;
    const BlockMemory::Binding binding{PoolTestHost::image};
    BlockMemory                reader(binding, &PoolTestHost::Owner), unowned(binding, nullptr);
    auto                       identity = Identity();
    char*                      name = nullptr;
    for (const auto index : {std::uint32_t{0}, std::uint32_t{1}, A::ObservationLimit - 1})
        for (const auto loaded : {std::uint8_t{0}, std::uint8_t{1}})
        {
            fixture.Reset();
            fixture.Cell(A::CountRva, static_cast<std::int32_t>(A::ObservationLimit));
            fixture.Seed(index, loaded, -1, 0xffffffffu);
            identity = Identity(index);
            name = nullptr;
            Check(reader.ReadName(identity, name) == ClockResult::Ok && name == reinterpret_cast<char*>(fixture.Row(index)) &&
                      std::memcmp(name, identity.name.data(), 16) == 0,
                  "actual native image-bank address is returned for loaded and unloaded rows, not copied CPU text");
            name[0] = 'X';
            NativeAnimationBlockValues value = OutputSentinel();
            Check(*reinterpret_cast<const unsigned char*>(fixture.Row(index)) == 'X' && reader.Read(identity, value) == ClockResult::BindingChanged &&
                      UntouchedBlock(value),
                  "mutable borrowed SDK intent changes genuine row and invalidates old copied identity; no stable-name claim");
        }
    fixture.Reset();
    identity = Identity();
    const auto sentinel = reinterpret_cast<char*>(std::uintptr_t{0x1231});
    name = sentinel;
    Check(unowned.ReadName(identity, name) == ClockResult::NotInitialized && name == sentinel, "unowned image address cannot become a native borrow");
    fixture.Reset();
    identity = Identity(0xffffffffu);
    name = sentinel;
    Check(reader.ReadName(identity, name) == ClockResult::InvalidArgument && name == sentinel && !PoolTestHost::reads && !PoolTestHost::ownerCalls,
          "invalid fullDWORD index preserves pointer before native observations");
    fixture.Reset();
    identity = Identity();
    identity.name.fill('x');
    name = sentinel;
    Check(reader.ReadName(identity, name) == ClockResult::InvalidArgument && name == sentinel && !PoolTestHost::reads,
          "unterminated input cannot publish a native char pointer");
    for (const auto action : {&ChangeBlockName, &ChangeBlockCount, &ChangeBlockMetadata})
    {
        fixture.Reset();
        identity = Identity();
        PoolTestHost::watched = fixture.Row(1);
        PoolTestHost::after = action;
        name = sentinel;
        Check(reader.ReadName(identity, name) == ClockResult::BindingChanged && name == sentinel && !PoolTestHost::after,
              "changed native name/count/row refuses pointer publication without restoring native storage");
    }
    fixture.Reset();
    identity = Identity();
    PoolTestHost::ownerResult = ClockResult::WrongThread;
    name = sentinel;
    Check(reader.ReadName(identity, name) == ClockResult::WrongThread && name == sentinel, "owner refusal preserves pointer");
    fixture.Reset();
    fixture.Cell(NativeBorrowedGcProfile::markerRva, LONG{1});
    name = sentinel;
    Check(reader.ReadName(identity, name) != ClockResult::Ok && name == sentinel, "native GC refuses borrowed pointer");
    fixture.Reset();
    fixture.Cell(NativeGameStateProfile::stateRva, std::uint32_t{8});
    name = sentinel;
    Check(reader.ReadName(identity, name) == ClockResult::NotInitialized && name == sentinel, "native frontend cannot borrow a gameplay block name");
    fixture.Reset();
    PoolTestHost::failAt = 1;
    name = sentinel;
    Check(reader.ReadName(identity, name) != ClockResult::Ok && name == sentinel, "actual failed OS read preserves pointer");
    fixture.Reset();
    name = sentinel;
    ClockResult foreign = ClockResult::Ok;
    std::thread other([&] { foreign = reader.ReadName(identity, name); });
    other.join();
    Check(foreign == ClockResult::WrongThread && name == sentinel, "native GameThread affinity cannot be replaced by a permissive callback");
    fixture.Reset();
    DWORD old = 0, ignored = 0;
    Check(VirtualProtect(reinterpret_cast<void*>(fixture.Row(1)), A::RowBytes, PAGE_READONLY, &old) != FALSE, "restrict owned native-name data page");
    name = sentinel;
    Check(reader.ReadName(identity, name) != ClockResult::Ok && name == sentinel, "readonly data cannot satisfy mutable SDK name intent");
    Check(VirtualProtect(reinterpret_cast<void*>(fixture.Row(1)), A::RowBytes, old, &ignored) != FALSE, "restore owned native-name data page");
    fixture.Reset();
    identity = Identity();
    name = sentinel;
    BlockMemory callbackReader(binding, &AnimationBlockNameIdentityHost::Owner);
    AnimationBlockNameIdentityHost::original = nullptr;
    Check(callbackReader.ReadName(identity, name) == ClockResult::Ok && name == reinterpret_cast<char*>(fixture.Row(1)),
          "measure final real owner callback for name-identity publication");
    const auto finalOwnerCall = PoolTestHost::ownerCalls;
    Check(finalOwnerCall > 0, "the name borrow has an actual final owner boundary");
    for (unsigned kind = 0; kind != 3; ++kind)
    {
        fixture.Reset();
        identity = Identity();
        name = sentinel;
        const auto before = identity;
        AnimationBlockNameIdentityHost::original = &identity;
        AnimationBlockNameIdentityHost::mutateAt = finalOwnerCall;
        AnimationBlockNameIdentityHost::kind = kind;
        Check(callbackReader.ReadName(identity, name) == ClockResult::BindingChanged && name == sentinel && PoolTestHost::ownerCalls == finalOwnerCall &&
                  !AnimationBlockNameIdentityHost::original,
              "final owner mutation of caller index/name/generation cannot redirect or publish a native pointer");
        Check((kind == 0 && identity.index != before.index) || (kind == 1 && identity.name != before.name) ||
                  (kind == 2 && identity.ownerGeneration != before.ownerGeneration),
              "the fixture actually changed the selected CPU identity field at the final callback");
    }
    AnimationBlockNameIdentityHost::original = nullptr;
    fixture.Reset();
    PoolTestHost::image = 0;
    name = sentinel;
    Check(reader.ReadName(identity, name) != ClockResult::Ok && name == sentinel, "main module loss cannot publish a stale native name pointer");
    std::puts("Production native animation block borrowed name primitive fixture completed");
    std::printf("Passed %u native animation block borrowed name checks (64-bit; no native engine code executed)\n", checks);
    // This entry is also compiled under a non-main name by focused fixtures.
    return 0;
}
