/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Execute only new block metadata over the actual owned image scaffold
 ****************************************************************************/
#pragma push_macro("main")
#undef main
#define main NativeAnimationBlockHistoricalBankMain
#include "NativeModelBankTests.cpp"
#undef main
#pragma pop_macro("main")
#include "../../Client/game_de/NativeAnimationBlockMemory.h"
#include "../../Client/game_de/NativeAnimationBlockProfile.h"
#define ReadProcessMemory PoolTestHost::Read
#include "../../Client/game_de/NativeAnimationBlockMemory.cpp"
#undef ReadProcessMemory
namespace
{
    namespace A = NativeAnimationBlockProfile;
    using BlockMemory = NativeAnimationBlockMemory;
    NativeAnimationBlockIdentity Identity(std::uint32_t index = 1)
    {
        NativeAnimationBlockIdentity identity;
        identity.index = index;
        identity.name[0] = 'P';
        identity.name[1] = 'E';
        identity.name[2] = 'D';
        identity.ownerGeneration = 0x123456789abcdef0ull;
        return identity;
    }
    NativeAnimationBlockValues OutputSentinel()
    {
        return {0xdeadbeefu, -123, 0xabcdef01u, 0x7f};
    }
    bool UntouchedBlock(const NativeAnimationBlockValues& value)
    {
        const auto expected = OutputSentinel();
        return value.index == expected.index && value.idOffset == expected.idOffset && value.animationCount == expected.animationCount &&
               value.loaded == expected.loaded;
    }
    struct BlockFixture
    {
        BankFixture bank;
        explicit BlockFixture(const std::filesystem::path& path) : bank(path)
        {
            for (const auto& pin : A::Code)
                bank.transform.image.Patch(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
            const auto group = bank.transform.image.image + A::GroupRva;
            bank.transform.image.Patch(A::GroupSlotRva, &group, sizeof(group), PAGE_READONLY);
            const auto selector = static_cast<unsigned char>(A::LoadedSelector);
            bank.transform.image.Patch(A::RemapRva + A::LoadedOpcodeIndex, &selector, 1, PAGE_EXECUTE_READ);
            const auto branch = static_cast<std::uint32_t>(A::LoadedBranchRva);
            bank.transform.image.Patch(A::DispatchRva + A::LoadedSelector * 4, &branch, 4, PAGE_EXECUTE_READ);
            Reset();
        }
        std::uintptr_t Row(std::uint32_t index) const { return bank.transform.image.image + A::BankRva + static_cast<std::uintptr_t>(index) * A::RowBytes; }
        template <class T>
        void Cell(std::uintptr_t rva, const T& value)
        {
            bank.Global(rva, value);
        }
        void Seed(std::uint32_t index, std::uint8_t loaded, std::int32_t offset, std::uint32_t count)
        {
            std::array<unsigned char, A::RowBytes> row{};
            const auto                             identity = Identity(index);
            std::memcpy(row.data(), identity.name.data(), identity.name.size());
            row[A::LoadedOffset] = loaded;
            std::memcpy(row.data() + A::IdOffset, &offset, 4);
            std::memcpy(row.data() + A::AnimationCountOffset, &count, 4);
            // Distinct neighboring assoc-group bits detect an accidental host
            // size_t read across the genuine32-bit animation count.
            row[0x1c] = 0xa5;
            row[0x1d] = 0x5a;
            row[0x1e] = 0xc3;
            row[0x1f] = 0x3c;
            bank.transform.image.Patch(A::BankRva + static_cast<std::uintptr_t>(index) * A::RowBytes, row.data(), row.size(), PAGE_READWRITE);
        }
        void Reset()
        {
            bank.Reset();
            Cell(A::CountRva, std::int32_t{3});
            Seed(0, 0, 0, 0);
            Seed(1, 1, 17, 33);
            Seed(A::ObservationLimit - 1, 1, -1, 0xffffffffu);
            PoolTestHost::watched = 0;
            PoolTestHost::after = nullptr;
            PoolTestHost::reads = PoolTestHost::ownerCalls = 0;
        }
    };
    BlockFixture* changingBlock = nullptr;
    void          ChangeBlockName()
    {
        *reinterpret_cast<unsigned char*>(changingBlock->Row(1)) = 'X';
    }
    void ChangeBlockCount()
    {
        changingBlock->Cell(A::CountRva, std::int32_t{2});
    }
    void ChangeBlockMetadata()
    {
        *reinterpret_cast<std::uint32_t*>(changingBlock->Row(1) + A::AnimationCountOffset) = 99;
    }
}
int main(int argc, char** argv)
{
    Check(argc == 2 && std::filesystem::path(argv[1]).is_absolute(), "block evidence uses explicit workspace output");
    BlockMemory::Binding wrongHost{123};
    Check(!BlockMemory::Qualify(wrongHost) && wrongHost.image == 123, "wrong host cannot acquire native block binding/output");
    BlockFixture fixture(argv[1]);
    changingBlock = &fixture;
    PoolTestHost::image = fixture.bank.transform.image.image;
    const BlockMemory::Binding binding{PoolTestHost::image};
    BlockMemory                reader(binding, &PoolTestHost::Owner), unowned(binding, nullptr);
    auto                       identity = Identity();
    auto                       value = OutputSentinel();
    Check(BlockMemory::Revalidate(binding), "actual named query/loader/index pins qualify block data independently from association groups");
    Check(unowned.Read(identity, value) == ClockResult::NotInitialized && UntouchedBlock(value), "native bank presence alone is not callback authority");
    for (const auto index : {std::uint32_t{0}, std::uint32_t{1}, A::ObservationLimit - 1})
        for (const auto loaded : {std::uint8_t{0}, std::uint8_t{1}, std::uint8_t{0xff}})
            for (const auto offset :
                 {std::int32_t{0}, std::int32_t{-1}, (std::numeric_limits<std::int32_t>::min)(), (std::numeric_limits<std::int32_t>::max)()})
                for (const auto count : {std::uint32_t{0}, std::uint32_t{1}, std::uint32_t{0x80000000}, std::uint32_t{0xffffffff}})
                {
                    fixture.Reset();
                    fixture.Cell(A::CountRva, static_cast<std::int32_t>(A::ObservationLimit));
                    fixture.Seed(index, loaded, offset, count);
                    value = OutputSentinel();
                    identity = Identity(index);
                    Check(reader.Read(identity, value) == ClockResult::Ok && value.index == index && value.loaded == loaded && value.idOffset == offset &&
                              value.animationCount == count,
                          "first/last supported rows preserve exact native scalar widths; count is not inferred from loaded/group state");
                }
    for (const auto invalid : {A::ObservationLimit, A::ObservationLimit + 1, 0x10000u, 0xffffffffu})
    {
        fixture.Reset();
        identity = Identity(invalid);
        value = OutputSentinel();
        Check(
            reader.Read(identity, value) == ClockResult::InvalidArgument && UntouchedBlock(value) && PoolTestHost::reads == 0 && PoolTestHost::ownerCalls == 0,
            "full index refuses outside conservative observation worklimit before any native observation");
    }
    fixture.Reset();
    identity = Identity();
    identity.name.fill('x');
    value = OutputSentinel();
    Check(reader.Read(identity, value) == ClockResult::InvalidArgument && UntouchedBlock(value) && !PoolTestHost::reads && !PoolTestHost::ownerCalls,
          "unterminated16-byte name refuses before native reads without reading neighboring loaded/ref fields");
    for (const auto count : {std::int32_t{-1}, std::int32_t{181}, (std::numeric_limits<std::int32_t>::max)()})
    {
        fixture.Reset();
        fixture.Cell(A::CountRva, count);
        identity = Identity();
        value = OutputSentinel();
        Check(reader.Read(identity, value) == ClockResult::BindingChanged && UntouchedBlock(value),
              "unqualified larger/negative current bank count refuses; worklimit is not native capacity");
    }
    fixture.Reset();
    fixture.Cell(A::CountRva, std::int32_t{1});
    identity = Identity();
    value = OutputSentinel();
    Check(reader.Read(identity, value) == ClockResult::NotInitialized && UntouchedBlock(value),
          "identity outside currently published count has no available block");
    fixture.Reset();
    identity = Identity();
    identity.name[0] = 'X';
    value = OutputSentinel();
    Check(reader.Read(identity, value) == ClockResult::BindingChanged && UntouchedBlock(value),
          "same ordinal cannot silently adopt a renamed current native block");
    fixture.Reset();
    identity = Identity();
    Check(reader.Read(identity, value) == ClockResult::Ok, "measure actual block reader boundaries");
    const auto readCount = PoolTestHost::reads, ownerCount = PoolTestHost::ownerCalls;
    for (unsigned fail = 1; fail <= readCount; ++fail)
    {
        fixture.Reset();
        PoolTestHost::failAt = fail;
        value = OutputSentinel();
        Check(reader.Read(identity, value) != ClockResult::Ok && UntouchedBlock(value), "every failed actual OS read preserves entire metadata output");
    }
    for (unsigned revoke = 1; revoke <= ownerCount; ++revoke)
    {
        fixture.Reset();
        PoolTestHost::revokeAt = revoke;
        value = OutputSentinel();
        Check(reader.Read(identity, value) == ClockResult::WrongThread && UntouchedBlock(value), "each owner boundary can revoke before metadata publication");
    }
    for (const auto action : {&ChangeBlockName, &ChangeBlockCount, &ChangeBlockMetadata})
    {
        fixture.Reset();
        PoolTestHost::watched = fixture.Row(1);
        PoolTestHost::after = action;
        value = OutputSentinel();
        Check(reader.Read(identity, value) == ClockResult::BindingChanged && UntouchedBlock(value) && !PoolTestHost::after,
              "rename/count/metadata mutation after actual first row copy cannot publish a mixed snapshot");
    }
    for (const auto state : {std::uint32_t{6}, std::uint32_t{7}, std::uint32_t{8}})
    {
        fixture.Reset();
        fixture.Cell(NativeGameStateProfile::stateRva, state);
        value = OutputSentinel();
        Check(reader.Read(identity, value) == ClockResult::NotInitialized && UntouchedBlock(value),
              "native frontend/loading state does not admit block observation");
    }
    fixture.Reset();
    fixture.Cell(NativeBorrowedGcProfile::markerRva, LONG{1});
    value = OutputSentinel();
    Check(reader.Read(identity, value) != ClockResult::Ok && UntouchedBlock(value), "native GC excludes animation block observation");
    fixture.Reset();
    fixture.Cell(NativeStartGameProfile::ThreadInitializedRva, std::uint8_t{0});
    value = OutputSentinel();
    Check(reader.Read(identity, value) == ClockResult::NotInitialized && UntouchedBlock(value), "native uninitialized GameThread is not authority");
    fixture.Reset();
    value = OutputSentinel();
    ClockResult foreign = ClockResult::Ok;
    std::thread other([&] { foreign = reader.Read(identity, value); });
    other.join();
    Check(foreign == ClockResult::WrongThread && UntouchedBlock(value), "permissive callback cannot replace actual native thread affinity");
    fixture.Reset();
    const auto& pin = A::Code[0];
    auto        changed = static_cast<unsigned char>(pin.bytes[0] ^ 1u);
    fixture.bank.transform.image.Patch(pin.rva, &changed, 1, PAGE_EXECUTE_READ);
    value = OutputSentinel();
    Check(reader.Read(identity, value) != ClockResult::Ok && UntouchedBlock(value), "changed named native lookup cannot reuse animation profile");
    fixture.bank.transform.image.Patch(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
    fixture.Reset();
    const auto changedSelector = static_cast<unsigned char>(A::LoadedSelector + 1);
    fixture.bank.transform.image.Patch(A::RemapRva + A::LoadedOpcodeIndex, &changedSelector, 1, PAGE_EXECUTE_READ);
    value = OutputSentinel();
    Check(reader.Read(identity, value) != ClockResult::Ok && UntouchedBlock(value), "changed exact opcode remap refuses metadata");
    const auto selector = static_cast<unsigned char>(A::LoadedSelector);
    fixture.bank.transform.image.Patch(A::RemapRva + A::LoadedOpcodeIndex, &selector, 1, PAGE_EXECUTE_READ);
    fixture.Reset();
    DWORD old = 0, ignored = 0;
    Check(VirtualProtect(reinterpret_cast<void*>(fixture.Row(1)), A::RowBytes, PAGE_READONLY, &old) != FALSE, "restrict owned block row page");
    value = OutputSentinel();
    Check(reader.Read(identity, value) != ClockResult::Ok && UntouchedBlock(value),
          "readonly/executable row cannot substitute for genuine writable native bank data");
    Check(VirtualProtect(reinterpret_cast<void*>(fixture.Row(1)), A::RowBytes, old, &ignored) != FALSE, "restore owned block page");
    fixture.Reset();
    PoolTestHost::image = 0;
    value = OutputSentinel();
    Check(reader.Read(identity, value) != ClockResult::Ok && UntouchedBlock(value), "main image binding loss refuses metadata output");
    std::puts("Production native animation block metadata primitive fixture completed");
    std::printf("Passed %u native animation block metadata checks (64-bit; no native engine code executed)\n", checks);
    // This entry is also compiled under a non-main name by focused fixtures.
    return 0;
}
