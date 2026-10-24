/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Current hierarchy scalars and qualified opaque sequence borrows
 ****************************************************************************/
#pragma push_macro("main")
#undef main
#define main HistoricalHierarchyBankReadHostMain
#include "NativeAnimationHierarchyHashTests.cpp"
#undef main
#pragma pop_macro("main")
#include "../../Client/game_de/NativeAnimationHierarchyMemory.h"
#include "../../Client/game_de/NativeAnimationHierarchyReadProfile.h"
namespace HierarchyBankReadTestHost
{
    using Values = mta::game_de::NativeAnimationHierarchyValues;
    std::uintptr_t locator = 0, cookie = 0, childPointer = 0;
    unsigned       bankChecks = 0, scalarReads = 0, failScalar = 0, childReads = 0, failChild = 0, payloadReads = 0;
    void (*afterScalar)() = nullptr;
    void (*afterChild)() = nullptr;
    void Verify(bool value, const char* reason)
    {
        ++bankChecks;
        Check(value, reason);
    }
    bool Same(const Values& a, const Values& b)
    {
        return a.interfaceAddress == b.interfaceAddress && a.hash == b.hash && a.totalTimeBits == b.totalTimeBits && a.sequenceCount == b.sequenceCount &&
               a.runningCompressed == b.runningCompressed && a.blockId == b.blockId;
    }
    Values HierarchyBankSentinel()
    {
        return {1, 2, 3, 4, 5, 6};
    }
    bool Copy(std::uintptr_t, void*, std::size_t) noexcept;
    void ChangeScalar()
    {
        *reinterpret_cast<std::uint32_t*>(locator + 0x18) ^= 1;
    }
    void ChangePointer()
    {
        *reinterpret_cast<std::uintptr_t*>(locator + 8) = 0;
    }
    void ChangeCookie()
    {
        ++*reinterpret_cast<std::uint64_t*>(cookie);
    }
    void LoseOwner()
    {
        PoolTestHost::ownerResult = ClockResult::OwnerConflict;
    }
    struct Children
    {
        void*               base = nullptr;
        const std::uint64_t capacity = 65537;
        Children()
        {
            base = VirtualAlloc(nullptr, 8 + static_cast<std::size_t>(capacity) * 80, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
            Check(base != nullptr, "allocate only an owned opaque native-shaped child span");
            std::memcpy(base, &capacity, sizeof(capacity));
        }
        ~Children()
        {
            if (base)
                VirtualFree(base, 0, MEM_RELEASE);
        }
        std::uintptr_t Tag() const { return reinterpret_cast<std::uintptr_t>(base) + 8; }
    };
}
namespace mta::game_de::NativeImageAccess
{
    bool HierarchyBankFixtureReadImage(std::uintptr_t image, std::uintptr_t address, void* output, std::size_t bytes, bool writable = false,
                                       bool executable = false) noexcept
    {
        using namespace HierarchyBankReadTestHost;
        const bool selected = address == locator && bytes == 28;
        if (selected && ++scalarReads == failScalar)
            return false;
        if (!ReadImage(image, address, output, bytes, writable, executable))
            return false;
        if (selected && afterScalar)
        {
            const auto action = afterScalar;
            afterScalar = nullptr;
            action();
        }
        return true;
    }
}
#define ReadImage                       HierarchyBankFixtureReadImage
#define MTA_DE_ANIMATION_HIERARCHY_COPY HierarchyBankReadTestHost::Copy
#include "../../Client/game_de/NativeAnimationHierarchyMemory.cpp"
#undef MTA_DE_ANIMATION_HIERARCHY_COPY
#undef ReadImage
namespace HierarchyBankReadTestHost
{
    bool Copy(std::uintptr_t address, void* output, std::size_t bytes) noexcept
    {
        if (address >= childPointer && childPointer && address - childPointer < 65537u * 80u)
            ++payloadReads;
        const bool selected = address == cookie && bytes == 8;
        if (selected && ++childReads == failChild)
            return false;
        if (!mta::game_de::AnimationHierarchyReadDetail::Copy(address, output, bytes))
            return false;
        if (selected && afterChild)
        {
            const auto action = afterChild;
            afterChild = nullptr;
            action();
        }
        return true;
    }
}
int main(int argc, char** argv)
{
    using namespace HierarchyBankReadTestHost;
    namespace Host = AnimationBlockFactoryHost;
    using Memory = mta::game_de::NativeAnimationHierarchyMemory;
    Check(argc == 2, "bank hierarchy fixture owns its output directory");
    BlockFixture owned(argv[1]);
    Host::fixture = &owned;
    Children   children;
    const auto reset = [&]
    {
        Host::Reset();
        locator = Host::Row(6);
        cookie = reinterpret_cast<std::uintptr_t>(children.base);
        childPointer = children.Tag();
        std::memcpy(children.base, &children.capacity, sizeof(children.capacity));
        scalarReads = failScalar = childReads = failChild = payloadReads = 0;
        afterScalar = afterChild = nullptr;
        PoolTestHost::failAt = PoolTestHost::revokeAt = 0;
        PoolTestHost::ownerResult = ClockResult::Ok;
        for (const auto& pin : NativeAnimationHierarchyReadProfile::Code)
            Host::Write(&owned, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        const std::uint16_t count = 0;
        const std::uint8_t  compressed = 0x80;
        std::memcpy(reinterpret_cast<void*>(locator + 8), &childPointer, sizeof(childPointer));
        std::memcpy(reinterpret_cast<void*>(locator + 0x10), &count, sizeof(count));
        std::memcpy(reinterpret_cast<void*>(locator + 0x12), &compressed, sizeof(compressed));
    };
    reset();
    Memory memory({PoolTestHost::image}, &PoolTestHost::Owner);
    for (const std::uint32_t bits : {0u, 0x80000000u, 0x7fc01234u, 0x7f800001u, 0xffffffffu})
    {
        reset();
        std::memcpy(reinterpret_cast<void*>(locator + 0x18), &bits, sizeof(bits));
        Values value = HierarchyBankSentinel();
        Verify(memory.Read(locator, value) == ClockResult::Ok && value.interfaceAddress == locator && value.hash == 0x13579bdf && value.totalTimeBits == bits &&
                   !value.sequenceCount && value.runningCompressed == 0x80 && value.blockId == 1,
               "scalar snapshots preserve raw time bits, full compressed byte and independent logical count without touching children");
    }
    reset();
    owned.Seed(1, 0, 5, 3);
    std::memset(reinterpret_cast<void*>(owned.Row(1)), 0x5a, 16);
    Values value = HierarchyBankSentinel();
    Verify(memory.Read(locator, value) == ClockResult::Ok, "unloaded block and nonterminated name impose no unrelated scalar predicate");
    const std::uintptr_t untrusted = 1;
    std::memcpy(reinterpret_cast<void*>(locator + 8), &untrusted, sizeof(untrusted));
    Verify(memory.Read(locator, value) == ClockResult::Ok && !childReads, "scalar observation neither validates nor follows an unrelated dynamic pointer");
    std::uintptr_t output = 0x12345;
    Verify(memory.ReadSequences(locator, output) != ClockResult::Ok && output == 0x12345, "only nested pointer channel requires cookie/range authority");
    reset();
    Verify(memory.ReadSequences(locator, output) == ClockResult::Ok && output == children.Tag() && !payloadReads,
           "sequence array borrow qualifies the genuine current parent and full private cookie/span without reading rows");
    Verify(memory.ReadSequence(locator, 65536, output) == ClockResult::Ok && output == children.Tag() + std::uintptr_t{65536} * 80 && !payloadReads,
           "full DWORD index follows full allocation capacity independently of zero logical WORD count");
    for (const std::uint32_t index : {65537u, 0xffffffffu})
    {
        output = 0x12345;
        Verify(memory.ReadSequence(locator, index, output) != ClockResult::Ok && output == 0x12345,
               "one-past and full DWORD invalid index cannot narrow into the child span");
    }
    const std::uintptr_t zero = 0;
    std::memcpy(reinterpret_cast<void*>(locator + 8), &zero, sizeof(zero));
    Verify(memory.ReadSequences(locator, output) == ClockResult::Ok && output == 0 && memory.ReadSequence(locator, 0, output) == ClockResult::Ok && !output,
           "null sequence pointer preserves original zero-index null policy without inventing a zero-size allocation");
    output = 0x12345;
    Verify(memory.ReadSequence(locator, 1, output) != ClockResult::Ok && output == 0x12345, "nonzero index cannot extend a null child borrow");
    for (const unsigned failed : {1u, 2u})
    {
        reset();
        failScalar = failed;
        value = HierarchyBankSentinel();
        Verify(memory.Read(locator, value) != ClockResult::Ok && Same(value, HierarchyBankSentinel()),
               "first and final scalar payload failures preserve every public output bit");
        reset();
        failChild = failed;
        output = 0x12345;
        Verify(memory.ReadSequences(locator, output) != ClockResult::Ok && output == 0x12345, "cookie observation failures publish no nested pointer");
    }
    for (const auto change : {&ChangeScalar, &LoseOwner})
    {
        reset();
        afterScalar = change;
        value = HierarchyBankSentinel();
        Verify(memory.Read(locator, value) != ClockResult::Ok && Same(value, HierarchyBankSentinel()),
               "field or native owner transition cannot publish a mixed scalar snapshot");
    }
    for (const auto change : {&ChangePointer, &ChangeCookie, &LoseOwner})
    {
        reset();
        afterChild = change;
        output = 0x12345;
        Verify(memory.ReadSequences(locator, output) != ClockResult::Ok && output == 0x12345,
               "parent pointer, full cookie or owner changes reject the coupled child observation");
    }
    reset();
    const std::uint64_t tooLarge = (std::numeric_limits<std::uint64_t>::max)();
    std::memcpy(children.base, &tooLarge, sizeof(tooLarge));
    output = 0x12345;
    Verify(memory.ReadSequences(locator, output) != ClockResult::Ok && output == 0x12345, "full cookie arithmetic overflow refuses without truncation");
    reset();
    DWORD old = 0, ignored = 0;
    Check(VirtualProtect(children.base, 8 + static_cast<std::size_t>(children.capacity) * 80, PAGE_READONLY, &old) != FALSE, "restrict owned child span");
    output = 0x12345;
    Verify(memory.ReadSequences(locator, output) != ClockResult::Ok && output == 0x12345,
           "readable child-shaped storage alone is insufficient for the writable native span contract");
    Check(VirtualProtect(children.base, 8 + static_cast<std::size_t>(children.capacity) * 80, old, &ignored) != FALSE, "restore owned child span");
    std::puts("Production native animation hierarchy bank fixture completed");
    std::printf("Passed %u native animation hierarchy bank checks (64-bit; no native engine code executed)\n", bankChecks);
    return 0;
}
