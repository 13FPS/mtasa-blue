/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Source-qualified without-delete decrement on the actual ref word
 ****************************************************************************/
#pragma push_macro("main")
#undef main
#define main HistoricalAnimationBlockRefHostMain
#include "NativeAnimationBlockRefTests.cpp"
#undef main
#pragma pop_macro("main")

namespace AnimationBlockRemoveRefTests
{
    unsigned removalChecks = 0;
    void     Verify(bool condition, const char* message)
    {
        ++removalChecks;
        Check(condition, message);
    }
}
int main(int argc, char** argv)
{
    using namespace AnimationBlockRefHost;
    using namespace AnimationBlockRemoveRefTests;
    Verify(argc == 2 && std::filesystem::path(argv[1]).is_absolute(), "new decrement evidence stays in its explicit owned output");
    BlockFixture owned(argv[1]);
    fixture = &owned;
    Reset();
    const RefMemory::Binding binding{PoolTestHost::image};
    const RefMemory          memory(binding, &PoolTestHost::Owner);
    RefMemory::Receipt       receipt;
    auto                     identity = Identity();
    for (const std::uint16_t value : {std::uint16_t{0}, std::uint16_t{1}, std::uint16_t{0x8000}, std::uint16_t{0xffff}})
    {
        Reset();
        identity = Identity();
        Ref() = value;
        *reinterpret_cast<unsigned char*>(owned.Row(1) + A::LoadedOffset) = 0;
        std::array<unsigned char, A::RowBytes> expected{};
        std::memcpy(expected.data(), reinterpret_cast<void*>(owned.Row(1)), expected.size());
        const auto target = static_cast<std::uint16_t>(static_cast<std::uint32_t>(value) - 1u);
        std::memcpy(expected.data() + Profile::RefOffset, &target, sizeof(target));
        const auto result = memory.RemoveWithoutDelete(identity, &receipt);
        Verify(result == ClockResult::Ok && receipt.entered && receipt.accepted && receipt.verified && receipt.before == value && receipt.target == target &&
                   calls == 1 && effects == 1 && !std::memcmp(expected.data(), reinterpret_cast<void*>(owned.Row(1)), expected.size()),
               "without-delete is one unconditional raw WORD decrement including zero wrap, preserving every neighbour on an unloaded block");
    }
    Reset();
    identity = Identity();
    identity.name.fill(0x5a);
    std::memcpy(reinterpret_cast<void*>(owned.Row(1)), identity.name.data(), identity.name.size());
    Ref() = 0x8000;
    Verify(memory.RemoveWithoutDelete(identity, &receipt) == ClockResult::Ok && Ref() == 0x7fff && calls == 1,
           "integer-counter affinity preserves all sixteen raw name bytes without imposing a string terminator");
    NativeAnimationBlockIdentity resolved;
    resolved.ownerGeneration = 0x12345678;
    Verify(memory.ReadCounterIdentity(1, resolved) == ClockResult::Ok && resolved.index == 1 && resolved.name == identity.name && !resolved.ownerGeneration,
           "raw integer lookup returns complete sixteen-byte affinity and leaves CPU generation stamping to the runtime owner");
    std::uint16_t rawRefs = 0x9abc;
    Verify(memory.ReadCounter(resolved, rawRefs) == ClockResult::Ok && rawRefs == 0x7fff,
           "raw counter read stabilizes the native WORD without interpreting its name as a string");
    Verify(memory.AddCounter(resolved, &receipt) == ClockResult::Ok && Ref() == 0x8000 && calls == 2,
           "manager raw AddCounter uses the same native WORD with copied nonterminated affinity");
    rawRefs = 0x9abc;
    Verify(memory.Read(resolved, rawRefs) == ClockResult::InvalidArgument && rawRefs == 0x9abc &&
               memory.Add(resolved, &receipt) == ClockResult::InvalidArgument && !receipt.entered && calls == 2,
           "existing named block Read/Add keep their terminator policy while the new counter channel remains independent");
    const auto resolvedBefore = resolved;
    Verify(memory.ReadCounterIdentity(A::ObservationLimit, resolved) != ClockResult::Ok && resolved.index == resolvedBefore.index &&
               resolved.name == resolvedBefore.name && resolved.ownerGeneration == resolvedBefore.ownerGeneration,
           "out-of-domain integer lookup preserves the complete caller output identity");
    Reset();
    identity = Identity();
    identity.name[0] ^= 1;
    Ref() = 5;
    Verify(memory.RemoveWithoutDelete(identity, &receipt) != ClockResult::Ok && !receipt.entered && !calls && Ref() == 5,
           "foreign current native row affinity refuses before decrement");
    Reset();
    identity = Identity();
    PoolTestHost::ownerResult = ClockResult::WrongThread;
    Verify(memory.RemoveWithoutDelete(identity, &receipt) == ClockResult::WrongThread && !receipt.entered && !calls,
           "without-delete cannot replace the actual native callback owner");
    Reset();
    identity = Identity();
    PoolTestHost::failAt = 1;
    Verify(memory.RemoveWithoutDelete(identity, &receipt) != ClockResult::Ok && !receipt.entered && !calls,
           "failed first required observation preserves the native reference count before entry");
    Reset();
    identity = Identity();
    Ref() = 5;
    caller = &identity;
    mode = 10;
    Verify(memory.RemoveWithoutDelete(identity, &receipt) == ClockResult::Ok && Ref() == 4 && calls == 1 && identity.index == 0xffffffffu,
           "primitive copies complete caller identity before callbacks and ignores later caller-object mutation");
    Reset();
    identity = Identity();
    Ref() = 5;
    mode = 1;
    Verify(memory.RemoveWithoutDelete(identity, &receipt) == ClockResult::Busy && receipt.entered && !receipt.accepted && calls == 1 && Ref() == 12,
           "a concurrent native ref change receives one conditional write attempt and no decrement retry");
    for (const unsigned failure : {3u, 5u, 6u, 9u})
    {
        Reset();
        identity = Identity();
        Ref() = 5;
        mode = failure;
        Verify(memory.RemoveWithoutDelete(identity, &receipt) != ClockResult::Ok && receipt.entered && calls == 1 && effects == 1 && Ref() == 4,
               "accepted decrement survives native fault, row loss, owner loss or read loss without rollback or replay");
    }
    Reset();
    identity = Identity();
    Ref() = 5;
    DWORD previous = 0, ignored = 0;
    Verify(VirtualProtect(reinterpret_cast<void*>(owned.Row(1)), A::RowBytes, PAGE_READONLY, &previous) != FALSE, "restrict the actual owned counter row");
    Verify(memory.RemoveWithoutDelete(identity, &receipt) != ClockResult::Ok && !receipt.entered && !calls && Ref() == 5,
           "readonly counter refuses without-delete before atomic entry");
    Verify(VirtualProtect(reinterpret_cast<void*>(owned.Row(1)), A::RowBytes, previous, &ignored) != FALSE, "restore the owned row protection");
    std::puts("Production native animation without-delete reference fixture completed");
    std::printf("Passed %u native animation without-delete reference checks (64-bit; no native engine code executed)\n", removalChecks);
    return 0;
}
