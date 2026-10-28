/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual one-shot borrowed collision copies on owned model/world pages
 ****************************************************************************/
// Only the owned current-world/bank scaffold is reused. No historical main,
// allocator, native model function or geometry consumer executes here.
#pragma push_macro("main")
#undef main
#define main StartupCollisionHistoricalSpecialDefinitionsMain
#include "NativeStreamingSpecialModelTests.cpp"
#undef main
#pragma pop_macro("main")
#include "../../Client/game_de/NativeStartupModelCollision.h"
#include "../../Client/game_de/NativeModelObjectProfile.h"
#include <type_traits>
#define GetModuleHandleW  PoolTestHost::Module
#define ReadProcessMemory PoolTestHost::Read
#include "../../Client/game_de/NativeStartupModelCollision.cpp"
#undef ReadProcessMemory
#undef GetModuleHandleW

namespace StartupCollisionTests
{
    namespace Calls = mta::game_de::NativeStartupModelCollision;
    namespace P = mta::game_de::NativeModelObjectProfile;
    constexpr std::uint64_t  Generation = 131;
    constexpr std::uint32_t  Target = 17, Source = 18;
    constexpr std::uintptr_t Poison = 0xfedcba9876543211;
    SpecialFixture*          fixture = nullptr;
    unsigned char*           source = nullptr;
    unsigned                 mode = 0, exchanges = 0, mutation = 0;
    std::uintptr_t           seenField = 0, seenExpected = 0, seenReplacement = 0;
    std::uintptr_t           Field(const unsigned char* receiver)
    {
        std::uintptr_t value = 0;
        std::memcpy(&value, receiver + P::CollisionOffset, sizeof(value));
        return value;
    }
    void Store(unsigned char* receiver, std::uintptr_t value)
    {
        std::memcpy(receiver + P::CollisionOffset, &value, sizeof(value));
    }
    void Mutate()
    {
        switch (mutation)
        {
            case 0:
                Store(source, Field(source) ^ (std::uintptr_t{1} << 40));
                break;
            case 1:
                fixture->Slot(Target, 0);
                break;
            case 2:
                fixture->Slot(Source, fixture->Address(fixture->metadata));
                break;
            case 3:
                PoolTestHost::ownerResult = ClockResult::Expired;
                break;
            case 4:
                fixture->Global(NativeStreamingLoadAllProfile::RootRva, std::uintptr_t{0});
                break;
            case 5:
                PoolTestHost::failAt = PoolTestHost::reads + 1;
                break;
        }
    }
    ClockResult Exchange(std::uintptr_t field, std::uintptr_t expected, std::uintptr_t replacement, Calls::CompareExchangeReceipt& receipt) noexcept
    {
        ++exchanges;
        seenField = field;
        seenExpected = expected;
        seenReplacement = replacement;
        if (mode == 1)
            Store(fixture->metadata, 8);
        if (mode == 2)
        {
            receipt.exceptionCode = 0xe0570131;
            return ClockResult::OriginalException;
        }
        if (mode == 4)
        {
            receipt.returned = true;
            receipt.accepted = false;
            receipt.observed = 0;
            return ClockResult::Ok;
        }
        const auto result = Calls::CompareExchange(field, expected, replacement, receipt);
        if (mode == 3)
        {
            // The real owned-page CAS occurred, but its acknowledgement is
            // unavailable. Raw pointer presence must not permit adoption.
            receipt = {};
            receipt.exceptionCode = 0xe0570132;
            return ClockResult::OriginalException;
        }
        if (mode == 5)
            return ClockResult::ReadFailure;
        if (mode == 6)
            PoolTestHost::ownerResult = ClockResult::Expired;
        if (mode == 7)
            fixture->Global(NativeStreamingLoadAllProfile::RootRva, std::uintptr_t{0});
        if (mode == 8)
            fixture->Slot(Source, 0);
        if (mode == 9)
            Store(source, replacement ^ (std::uintptr_t{1} << 40));
        return result;
    }
}

int main(int argc, char** argv)
{
    namespace T = StartupCollisionTests;
    namespace Calls = T::Calls;
    Check(argc == 2 && std::filesystem::path(argv[1]).is_absolute(), "collision-copy evidence uses an explicit owned workspace directory");
    static_assert(!std::is_copy_constructible_v<Calls::Record> && !std::is_move_constructible_v<Calls::Record>);
    SpecialFixture fixture(argv[1]);
    T::fixture = &fixture;
    T::source = static_cast<unsigned char*>(VirtualAlloc(nullptr, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    Check(T::source != nullptr, "own a separate source model prefix without any collision pointee");
    const auto     image = fixture.transform.image.image;
    Calls::Receipt receipt;
    const auto     reset = [&]
    {
        fixture.Reset();
        PoolTestHost::image = image;
        fixture.transform.image.Patch(T::P::CollisionConsumerRva, T::P::CollisionConsumer.data(), T::P::CollisionConsumer.size(), PAGE_EXECUTE_READ);
        const auto badIdentity = static_cast<unsigned char>(T::P::CanonicalPedAssignment[0] ^ 1);
        fixture.transform.image.Patch(0x1134ebb, &badIdentity, 1, PAGE_EXECUTE_READ);
        std::memset(T::source, 0xff, T::P::CollisionPrefixBytes);
        T::Store(fixture.metadata, 0);
        T::Store(T::source, T::Poison);
        fixture.Slot(T::Target, fixture.Address(fixture.metadata));
        fixture.Slot(T::Source, fixture.Address(T::source));
        PoolTestHost::ownerResult = ClockResult::Ok;
        PoolTestHost::reads = PoolTestHost::failAt = PoolTestHost::ownerCalls = PoolTestHost::revokeAt = 0;
        PoolTestHost::watched = 0;
        PoolTestHost::after = nullptr;
        T::mode = T::exchanges = 0;
        T::seenField = T::seenExpected = T::seenReplacement = 0;
    };
    const auto copy = [&](Calls::Record& record, std::uint32_t target = T::Target, std::uint32_t source = T::Source, std::uint64_t generation = T::Generation)
    { return Calls::Copy(image, record, generation, target, source, &PoolTestHost::Owner, &receipt, &T::Exchange); };
    reset();
    Calls::Record invalid;
    Check(copy(invalid, T::Target, T::Source, 0) == ClockResult::InvalidArgument && !receipt.entered && invalid.CanDiscard() && !T::exchanges,
          "zero generation refuses without a native effect");
    Check(copy(invalid) == ClockResult::OwnerConflict && !T::exchanges, "even a no-effect claimed attempt cannot silently replay");
    reset();
    Calls::Record unowned;
    Check(Calls::Copy(image, unowned, T::Generation, T::Target, T::Source, nullptr, &receipt, &T::Exchange) == ClockResult::NotInitialized &&
              !receipt.entered && !T::exchanges,
          "a CPU copy record cannot manufacture callback ownership");
    reset();
    Calls::Record targetMissing;
    fixture.Slot(T::Target, 0);
    fixture.Global(NativeStreamingLoadAllProfile::RootRva, std::uintptr_t{0});
    const auto badConsumer = static_cast<unsigned char>(T::P::CollisionConsumer[0] ^ 1);
    fixture.transform.image.Patch(T::P::CollisionConsumerRva, &badConsumer, 1, PAGE_EXECUTE_READ);
    Check(copy(targetMissing, T::Target, 0xffffffff) == ClockResult::Ok && receipt.skipped && receipt.verified && !receipt.entered && !T::exchanges,
          "null target skips source-key validation, field proof and world lookup in the source order");
    reset();
    Calls::Record targetPresent;
    T::Store(fixture.metadata, 1);
    fixture.Global(NativeStreamingLoadAllProfile::RootRva, std::uintptr_t{0});
    Check(copy(targetPresent, T::Target, 0xffffffff) == ClockResult::Ok && receipt.skipped && !receipt.entered && !T::exchanges,
          "nonnull target collision skips invalid source and unavailable world without querying the collision pointee");
    reset();
    Calls::Record sourceMissing;
    fixture.Slot(T::Source, 0);
    fixture.Global(NativeStreamingLoadAllProfile::RootRva, std::uintptr_t{0});
    Check(copy(sourceMissing) == ClockResult::Ok && receipt.skipped && !receipt.entered && !T::exchanges && T::Field(fixture.metadata) == 0,
          "null source model skips the write and world gate while retaining the target's null collision");
    reset();
    Calls::Record sourceInvalid;
    Check(copy(sourceInvalid, T::Target, 0xffffffff) == ClockResult::InvalidArgument && !receipt.entered && !T::exchanges,
          "a genuinely needed source lookup validates its full unsigned key");
    for (const auto raw : {std::uintptr_t{0}, std::uintptr_t{1}, T::Poison})
    {
        reset();
        T::Store(T::source, raw);
        {
            Calls::Record record;
            Check(copy(record) == ClockResult::Ok && receipt.entered && receipt.returned && receipt.accepted && receipt.verified && !receipt.skipped &&
                      !receipt.debt && record.GetStage() == Calls::Stage::Complete && record.CanDiscard() && T::exchanges == 1,
                  "actual owned-field CAS journals a verified copy, including the real zero-to-zero null collision write");
            Check(T::Field(fixture.metadata) == raw && T::Field(T::source) == raw && T::seenField == fixture.Address(fixture.metadata) + 0x30 &&
                      T::seenExpected == 0 && T::seenReplacement == raw && receipt.sourceCollision == raw,
                  "only the target raw pointer field changes; poison/null source tags are neither dereferenced nor narrowed");
            Check(copy(record) == ClockResult::OwnerConflict && T::exchanges == 1, "completed or skipped records cannot replay their CAS");
        }
        Check(T::Field(fixture.metadata) == raw, "record destruction neither rolls back the field nor frees either borrowed collider");
    }
    for (T::mutation = 0; T::mutation != 6; ++T::mutation)
    {
        reset();
        Calls::Record record;
        PoolTestHost::watched = fixture.Address(T::source) + 0x30;
        PoolTestHost::after = &T::Mutate;
        Check(copy(record) != ClockResult::Ok && !receipt.entered && !T::exchanges && !record.HasDebt() && T::Field(fixture.metadata) == 0,
              "source full-pointer/current-bank, callback, world or read drift refuses before entering any CAS");
    }
    reset();
    Calls::Record rejected;
    T::mode = 1;
    Check(copy(rejected) == ClockResult::Busy && receipt.entered && receipt.returned && !receipt.accepted && !receipt.debt && rejected.CanDiscard() &&
              T::Field(fixture.metadata) == 8 && T::exchanges == 1,
          "real rejected CAS preserves the competing target pointer without debt or overwrite");
    Check(copy(rejected) == ClockResult::OwnerConflict && T::exchanges == 1 && T::Field(fixture.metadata) == 8,
          "a known rejected attempt has no retry or rollback authority");
    for (T::mode = 2; T::mode != 10; ++T::mode)
    {
        const auto selected = T::mode;
        reset();
        T::mode = selected;
        Calls::Record record;
        Check(copy(record) != ClockResult::Ok && receipt.entered && receipt.debt && record.HasDebt() && !record.CanDiscard() && T::exchanges == 1,
              "unknown/contradictory CAS or accepted effect with later bank/world/owner/source loss retains entered debt");
        const auto observed = T::Field(fixture.metadata);
        PoolTestHost::ownerResult = ClockResult::Ok;
        Check(copy(record) == ClockResult::OwnerConflict && T::exchanges == 1 && T::Field(fixture.metadata) == observed,
              "raw field presence or absence cannot adopt an uncertain effect, retry the CAS or roll it back");
    }
    Check(VirtualFree(T::source, 0, MEM_RELEASE) != FALSE, "retire only the fixture's source metadata page, never a borrowed collider");
    std::puts("Production native startup model collision fixture completed");
    std::printf("Passed %u native startup model collision checks (64-bit; no native engine code executed)\n", checks);
    return 0;
}
