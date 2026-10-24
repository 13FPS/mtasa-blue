/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Only the new source-owned animation ref leaf on real OS image pages
 ****************************************************************************/
#pragma push_macro("main")
#undef main
#define main NativeAnimationBlockRefHistoricalMetadataMain
#include "NativeAnimationBlockTests.cpp"
#undef main
#pragma pop_macro("main")
#include "../../Client/game_de/NativeAnimationBlockRefMemory.h"
#include "NativeAnimationBlockRefFixture.h"
#include <intrin.h>
namespace AnimationBlockRefHost
{
    using RefMemory = mta::game_de::NativeAnimationBlockRefMemory;
    namespace Profile = mta::game_de::NativeAnimationBlockRefProfile;
    BlockFixture*                               fixture = nullptr;
    mta::game_de::NativeAnimationBlockIdentity* caller = nullptr;
    unsigned                                    calls = 0, effects = 0, mode = 0, preReads = 0, preOwners = 0;
    void                                        Install(void* host, std::uintptr_t rva, const void* bytes, std::size_t size, DWORD protection)
    {
        static_cast<BlockFixture*>(host)->bank.transform.image.Patch(rva, bytes, size, protection);
    }
    std::uint16_t& Ref(std::uint32_t index = 1)
    {
        return *reinterpret_cast<std::uint16_t*>(fixture->Row(index) + Profile::RefOffset);
    }
    void Reset()
    {
        fixture->Reset();
        PoolTestHost::image = fixture->bank.transform.image.image;
        NativeAnimationBlockRefFixture::Install(PoolTestHost::image, &Install, fixture);
        calls = effects = mode = preReads = preOwners = 0;
        caller = nullptr;
        PoolTestHost::reads = PoolTestHost::ownerCalls = 0;
    }
    void ChangeCaller()
    {
        caller->index = 0xffffffffu;
        caller->name.fill('X');
        caller->ownerGeneration = 0;
    }
    void ChangeRef()
    {
        Ref() = 77;
    }
    ClockResult Exchange(std::uintptr_t address, std::uint16_t expected, std::uint16_t target, std::uint16_t& observed, std::uint32_t& exception) noexcept
    {
        ++calls;
        preReads = PoolTestHost::reads;
        preOwners = PoolTestHost::ownerCalls;
        if (mode == 1)
            *reinterpret_cast<std::uint16_t*>(address) = static_cast<std::uint16_t>(expected + 7u);
        if (mode == 2)
        {
            exception = 0xc0000005u;
            return ClockResult::OriginalException;
        }
        SHORT compare = 0, desired = 0;
        std::memcpy(&compare, &expected, sizeof(compare));
        std::memcpy(&desired, &target, sizeof(desired));
        const auto previous = InterlockedCompareExchange16(reinterpret_cast<volatile SHORT*>(address), desired, compare);
        std::memcpy(&observed, &previous, sizeof(observed));
        if (observed == expected)
            ++effects;
        if (mode == 3)
        {
            exception = 0xc0000005u;
            return ClockResult::OriginalException;
        }
        if (mode == 4)
        {
            Ref() = 0x4567;
            *reinterpret_cast<std::uint8_t*>(fixture->Row(1) + A::LoadedOffset) = 0;
            *reinterpret_cast<std::uint32_t*>(fixture->Row(1) + A::AnimationCountOffset) = 91;
        }
        if (mode == 5)
            *reinterpret_cast<unsigned char*>(fixture->Row(1)) = 'X';
        if (mode == 6)
            PoolTestHost::ownerResult = ClockResult::WrongThread;
        if (mode == 7)
            PoolTestHost::image = 0;
        if (mode == 8)
        {
            const auto& pin = Profile::Code[0];
            const auto  changed = static_cast<unsigned char>(pin.bytes[0] ^ 1u);
            fixture->bank.transform.image.Patch(pin.rva, &changed, 1, PAGE_EXECUTE_READ);
        }
        if (mode == 9)
            PoolTestHost::failAt = PoolTestHost::reads + 1;
        if (mode == 10)
            ChangeCaller();
        return ClockResult::Ok;
    }
}
#define MTA_DE_ANIMATION_BLOCK_REF_EXCHANGE AnimationBlockRefHost::Exchange
#define ReadProcessMemory                   PoolTestHost::Read
#include "../../Client/game_de/NativeAnimationBlockRefMemory.cpp"
#undef ReadProcessMemory
#undef MTA_DE_ANIMATION_BLOCK_REF_EXCHANGE
int main(int argc, char** argv)
{
    using namespace AnimationBlockRefHost;
    Check(argc == 2 && std::filesystem::path(argv[1]).is_absolute(), "ref evidence stays in its workspace output");
    BlockFixture owned(argv[1]);
    fixture = &owned;
    Reset();
    const RefMemory::Binding binding{PoolTestHost::image};
    const RefMemory          memory(binding, &PoolTestHost::Owner), unowned(binding, nullptr);
    auto                     identity = Identity();
    std::uint16_t            output = 0x9abc;
    Check(RefMemory::Revalidate(binding), "saved genuine consumer pins qualify WORD+12 independently from metadata");
    Check(unowned.Read(identity, output) == ClockResult::NotInitialized && output == 0x9abc, "native bank alone grants no ref owner");
    for (const auto index : {0u, 1u, A::ObservationLimit - 1})
        for (const auto loaded : {std::uint8_t{0}, std::uint8_t{1}, std::uint8_t{0xff}})
            for (const auto value : {std::uint16_t{0}, std::uint16_t{1}, std::uint16_t{0xfffe}, std::uint16_t{0xffff}})
            {
                Reset();
                owned.Cell(A::CountRva, static_cast<std::int32_t>(A::ObservationLimit));
                owned.Seed(index, loaded, -17, 0xffffffffu);
                Ref(index) = value;
                *reinterpret_cast<unsigned char*>(owned.Row(index) + 0x11) = 0x7a;
                identity = Identity(index);
                output = 0x9abc;
                Check(memory.Read(identity, output) == ClockResult::Ok && output == value, "GetRefs preserves all ushort bits even before loading");
                std::array<unsigned char, A::RowBytes> before{}, after{};
                std::memcpy(before.data(), reinterpret_cast<void*>(owned.Row(index)), before.size());
                RefMemory::Receipt receipt;
                const auto         result = memory.Add(identity, &receipt);
                const auto         expected = static_cast<std::uint16_t>(static_cast<std::uint32_t>(value) + 1u);
                std::memcpy(after.data(), reinterpret_cast<void*>(owned.Row(index)), after.size());
                std::memcpy(before.data() + Profile::RefOffset, &expected, sizeof(expected));
                const bool correct = result == ClockResult::Ok && receipt.entered && receipt.accepted && receipt.verified && receipt.before == value &&
                                     receipt.target == expected && receipt.observed == value && calls == 1 && effects == 1 && before == after;
                if (!correct)
                {
                    std::fprintf(
                        stderr,
                        "ref index=%u loaded=%u value=%u result=%u entered=%u accepted=%u verified=%u before=%u target=%u observed=%u calls=%u effects=%u\n",
                        index, static_cast<unsigned>(loaded), static_cast<unsigned>(value), static_cast<unsigned>(result),
                        static_cast<unsigned>(receipt.entered), static_cast<unsigned>(receipt.accepted), static_cast<unsigned>(receipt.verified),
                        static_cast<unsigned>(receipt.before), static_cast<unsigned>(receipt.target), static_cast<unsigned>(receipt.observed), calls, effects);
                    std::fflush(stderr);
                }
                Check(correct, "AddRef executes one exact WORD CAS, wraps ffff to zero, and preserves every neighbor");
            }
    for (const auto invalid : {A::ObservationLimit, 0xffffffffu})
    {
        Reset();
        identity = Identity(invalid);
        RefMemory::Receipt receipt;
        Check(memory.Add(identity, &receipt) == ClockResult::InvalidArgument && !receipt.entered && !calls && !PoolTestHost::reads,
              "unqualified full-width indices refuse before observation or mutation");
    }
    Reset();
    identity = Identity();
    identity.name.fill('x');
    RefMemory::Receipt receipt;
    Check(memory.Add(identity, &receipt) == ClockResult::InvalidArgument && !receipt.entered && !calls, "unterminated copied name refuses before entry");
    for (unsigned scenario = 0; scenario != 5; ++scenario)
    {
        Reset();
        identity = Identity();
        if (scenario == 0)
            owned.Cell(A::CountRva, std::int32_t{1});
        if (scenario == 1)
            identity.name[0] = 'X';
        if (scenario == 2)
            owned.Cell(NativeGameStateProfile::stateRva, std::uint32_t{8});
        if (scenario == 3)
            owned.Cell(NativeBorrowedGcProfile::markerRva, LONG{1});
        if (scenario == 4)
            owned.Cell(NativeStartGameProfile::ThreadInitializedRva, std::uint8_t{0});
        Check(memory.Add(identity, &receipt) != ClockResult::Ok && !receipt.entered && !calls && Ref() == 0,
              "current count/name/state/GC/native thread refusals precede the conditional write");
    }
    Reset();
    identity = Identity();
    Ref() = 123;
    Check(memory.Read(identity, output) == ClockResult::Ok, "measure new read boundary once");
    const auto readOnlyReads = PoolTestHost::reads, readOnlyOwners = PoolTestHost::ownerCalls;
    for (const auto failure : {1u, readOnlyReads})
    {
        Reset();
        PoolTestHost::failAt = failure;
        output = 0x9abc;
        Check(memory.Read(identity, output) != ClockResult::Ok && output == 0x9abc && !calls, "first/last distinct read failures preserve GetRefs output");
    }
    for (const auto failure : {1u, readOnlyOwners})
    {
        Reset();
        PoolTestHost::revokeAt = failure;
        output = 0x9abc;
        Check(memory.Read(identity, output) == ClockResult::WrongThread && output == 0x9abc, "first/last owner revocation preserves GetRefs output");
    }
    Reset();
    identity = Identity();
    Ref() = 5;
    Check(memory.Add(identity, &receipt) == ClockResult::Ok, "measure preflight and postflight boundaries once");
    const auto beforeReads = preReads, beforeOwners = preOwners, allReads = PoolTestHost::reads, allOwners = PoolTestHost::ownerCalls;
    Check(beforeReads > 1 && beforeOwners > 1 && allReads > beforeReads && allOwners > beforeOwners, "entry separates distinct observed bookends");
    for (const auto failure : {1u, beforeReads, beforeReads + 1, allReads})
    {
        Reset();
        Ref() = 5;
        PoolTestHost::failAt = failure;
        const auto result = memory.Add(identity, &receipt);
        const bool entered = failure > beforeReads;
        Check(result != ClockResult::Ok && receipt.entered == entered && receipt.accepted == entered && calls == (entered ? 1u : 0u) &&
                  effects == (entered ? 1u : 0u) && Ref() == (entered ? 6 : 5),
              "first/last pre/post read failures never replay or roll back accepted increments");
    }
    for (const auto failure : {1u, beforeOwners, beforeOwners + 1, allOwners})
    {
        Reset();
        Ref() = 5;
        PoolTestHost::revokeAt = failure;
        const auto result = memory.Add(identity, &receipt);
        const bool entered = failure > beforeOwners;
        Check(result == ClockResult::WrongThread && receipt.entered == entered && receipt.accepted == entered && calls == (entered ? 1u : 0u) &&
                  effects == (entered ? 1u : 0u) && Ref() == (entered ? 6 : 5),
              "actual owner revocation before/after CAS retains exactly its accepted native effect");
    }
    for (unsigned scenario = 1; scenario <= 10; ++scenario)
    {
        Reset();
        Ref() = 5;
        identity = Identity();
        caller = &identity;
        mode = scenario;
        const auto result = memory.Add(identity, &receipt);
        if (scenario == 1)
            Check(result == ClockResult::Busy && receipt.entered && !receipt.accepted && !receipt.verified && calls == 1 && !effects && Ref() == 12,
                  "conflicting WORD is preserved and never reacquired/retried");
        else if (scenario == 2 || scenario == 3)
            Check(result == ClockResult::OriginalException && receipt.entered && !receipt.accepted && !receipt.verified && calls == 1 &&
                      effects == (scenario == 3 ? 1u : 0u) && Ref() == (scenario == 3 ? 6 : 5) && receipt.exceptionCode == 0xc0000005u,
                  "unknown write failure retains any accepted effect without replay or guessed rollback");
        else if (scenario == 4 || scenario == 10)
            Check(result == ClockResult::Ok && receipt.entered && receipt.accepted && receipt.verified && calls == 1 && effects == 1 &&
                      Ref() == (scenario == 4 ? 0x4567 : 6),
                  "current valid native progress and later caller mutation preserve the copied original increment intent");
        else
            Check(result != ClockResult::Ok && receipt.entered && receipt.accepted && !receipt.verified && calls == 1 && effects == 1 && Ref() == 6,
                  "post-entry native name/owner/image/code/read loss leaves accepted count intact and never retries");
    }
    Reset();
    identity = Identity();
    caller = &identity;
    Ref() = 42;
    PoolTestHost::watched = owned.Row(1) + Profile::RefOffset;
    PoolTestHost::after = &ChangeCaller;
    output = 0x9abc;
    Check(memory.Read(identity, output) == ClockResult::Ok && output == 42 && identity.index == 0xffffffffu,
          "GetRefs captures complete identity before callbacks and never re-reads later caller storage");
    Reset();
    identity = Identity();
    caller = &identity;
    Ref() = 42;
    PoolTestHost::watched = owned.Row(1) + Profile::RefOffset;
    PoolTestHost::after = &ChangeCaller;
    Check(memory.Add(identity, &receipt) == ClockResult::Ok && receipt.entered && receipt.accepted && receipt.verified && calls == 1 && effects == 1 &&
              receipt.address == owned.Row(1) + Profile::RefOffset && Ref() == 43 && identity.index == 0xffffffffu,
          "AddRef retains original complete identity through pre-entry callbacks without retaining caller storage");
    Reset();
    identity = Identity();
    Ref() = 42;
    PoolTestHost::watched = owned.Row(1) + Profile::RefOffset;
    PoolTestHost::after = &ChangeRef;
    output = 0x9abc;
    Check(memory.Read(identity, output) == ClockResult::OwnerConflict && output == 0x9abc && Ref() == 77,
          "two ref observations reject mixed counts without altering current native progress");
    Reset();
    identity = Identity();
    const auto& pin = Profile::Code[0];
    const auto  changed = static_cast<unsigned char>(pin.bytes[0] ^ 1u);
    owned.bank.transform.image.Patch(pin.rva, &changed, 1, PAGE_EXECUTE_READ);
    Check(memory.Add(identity, &receipt) != ClockResult::Ok && !receipt.entered && !calls, "changed genuine reference consumer refuses before entry");
    Reset();
    DWORD old = 0, ignored = 0;
    Check(VirtualProtect(reinterpret_cast<void*>(owned.Row(1)), A::RowBytes, PAGE_READONLY, &old) != FALSE, "restrict owned bank row protection");
    Check(memory.Add(identity, &receipt) != ClockResult::Ok && !receipt.entered && !calls, "readonly row cannot receive a reference increment");
    Check(VirtualProtect(reinterpret_cast<void*>(owned.Row(1)), A::RowBytes, old, &ignored) != FALSE, "restore owned bank row protection");
    Reset();
    ClockResult foreign = ClockResult::Ok;
    std::thread other([&] { foreign = memory.Add(identity, &receipt); });
    other.join();
    Check(foreign == ClockResult::WrongThread && !receipt.entered && !calls, "callback cannot replace actual native GameThread identity");
    std::puts("Production native animation block refs primitive fixture completed");
    std::printf("Passed %u native animation block refs checks (64-bit; no native engine code executed)\n", checks);
    return 0;
}
