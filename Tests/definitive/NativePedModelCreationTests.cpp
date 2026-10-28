/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Real manual ped defaults and one-shot bank publication on owned data
 ****************************************************************************/
#pragma push_macro("main")
#undef main
#define main PedCreationHistoricalStorageDefinitionsMain
#include "NativeCustomAnimSequenceStorageTests.cpp"
#undef main
#pragma pop_macro("main")
#include "NativePedModelDefaultCollisionFixture.inl"
#include "../../Client/game_de/NativePedModelCreation.h"
#define GetModuleHandleW  PoolTestHost::Module
#define ReadProcessMemory PoolTestHost::Read
#include "../../Client/game_de/NativePedModelDefaultCollision.cpp"
#include "../../Client/game_de/NativePedModelCreation.cpp"
#undef ReadProcessMemory
#undef GetModuleHandleW

namespace PedCreationTests
{
    namespace Calls = NativePedModelCreation;
    namespace Profile = NativePedModelMetadataProfile;
    namespace HeapHost = NativeCustomAnimSequenceStorageFixture;
    using Fixture = NativePedModelDefaultCollisionFixture::Fixture;
    constexpr std::uint64_t Generation = 127;
    constexpr std::uint32_t Model = 17;
    Fixture*                fixture = nullptr;
    unsigned                writeMode = 0, exchangeMode = 0, writes = 0, exchanges = 0, events = 0;
    std::array<unsigned, 3> trace{};
    bool                    initialPattern = false;
    std::uintptr_t          seenSlot = 0, seenExpected = 0, seenReplacement = 0;
    void                    Allocated(HeapHost::State&) noexcept
    {
        if (events < trace.size())
            trace[events++] = 1;
    }
    void ExpireAllocation(HeapHost::State& host) noexcept
    {
        Allocated(host);
        PoolTestHost::ownerResult = ClockResult::Expired;
    }
    std::array<unsigned char, 152> Expected(std::uintptr_t image, std::uintptr_t canonical)
    {
        std::array<unsigned char, 152> bytes{};
        const auto                     vtable = image + Profile::VtableRva;
        const auto                     none = Profile::AnimNone;
        std::memcpy(bytes.data(), &vtable, sizeof(vtable));
        std::memcpy(bytes.data() + 0x30, &canonical, sizeof(canonical));
        std::memcpy(bytes.data() + 0x68, &none, sizeof(none));
        return bytes;
    }
    ClockResult Write(std::uintptr_t object, std::uintptr_t vtable, std::uintptr_t canonical, std::uint32_t& exception) noexcept
    {
        ++writes;
        if (events < trace.size())
            trace[events++] = 2;
        initialPattern = true;
        for (std::size_t i = 0; i != Profile::Size; ++i)
            initialPattern = initialPattern && reinterpret_cast<const unsigned char*>(object)[i] == 0xa5;
        if (writeMode == 1)
        {
            // Only an owned private CPU prefix is modified before the seam's
            // unknown acknowledgement; no native exception is unwound here.
            std::memset(reinterpret_cast<void*>(object), 0x3c, 32);
            exception = 0xe0570127;
            return ClockResult::OriginalException;
        }
        const auto result = Calls::WriteDefaults(object, vtable, canonical, exception);
        if (writeMode == 2)
            reinterpret_cast<unsigned char*>(object)[0x50] ^= 1;
        if (writeMode == 3)
            PoolTestHost::ownerResult = ClockResult::Expired;
        if (writeMode == 4)
        {
            const unsigned char changed = 0x8a;
            fixture->transform.image.Patch(Profile::DefaultCollisionRva + 0x29, &changed, 1, PAGE_READWRITE);
        }
        return result;
    }
    ClockResult Exchange(std::uintptr_t slot, std::uintptr_t expected, std::uintptr_t replacement, Calls::CompareExchangeReceipt& receipt) noexcept
    {
        ++exchanges;
        if (events < trace.size())
            trace[events++] = 3;
        seenSlot = slot;
        seenExpected = expected;
        seenReplacement = replacement;
        if (exchangeMode == 1 || exchangeMode == 5)
            fixture->Slot(Model, expected ^ std::uintptr_t{8});
        if (exchangeMode == 2)
        {
            receipt.exceptionCode = 0xe0570128;
            return ClockResult::OriginalException;
        }
        const auto result = Calls::CompareExchange(slot, expected, replacement, receipt);
        if (exchangeMode == 3)
        {
            // The real owned-bank CAS happened, but its acknowledgement is
            // unknown to production. Raw presence cannot authorize a retry.
            receipt = {};
            receipt.exceptionCode = 0xe0570129;
            return ClockResult::OriginalException;
        }
        if (exchangeMode == 4 || exchangeMode == 5)
            return ClockResult::ReadFailure;
        if (exchangeMode == 6)
            PoolTestHost::ownerResult = ClockResult::Expired;
        return result;
    }
    std::uintptr_t Slot()
    {
        std::uintptr_t result = 0;
        std::memcpy(&result, reinterpret_cast<const void*>(fixture->Image() + NativeModelBankProfile::BankRva + Model * 8), sizeof(result));
        return result;
    }
    void SeedVtable()
    {
        constexpr std::array<std::uintptr_t, 4> offsets{0, 0x20, 0x40, 0x78};
        constexpr std::array<std::uintptr_t, 4> targets{0x11f5540, 0x11f5530, 0x120c530, 0x11f5520};
        for (std::size_t i = 0; i != offsets.size(); ++i)
        {
            const auto address = fixture->Image() + targets[i];
            fixture->transform.image.Patch(Profile::VtableRva + offsets[i], &address, sizeof(address), PAGE_READONLY);
        }
        const std::array<unsigned char, 3> type{0xb0, 0x07, 0xc3};
        fixture->transform.image.Patch(0x11f5530, type.data(), type.size(), PAGE_EXECUTE_READ);
    }
}
int main(int argc, char** argv)
{
    namespace T = PedCreationTests;
    namespace Calls = T::Calls;
    Check(argc == 2 && std::filesystem::path(argv[1]).is_absolute(), "creator evidence uses an owned workspace directory");
    static_assert(!std::is_copy_constructible_v<Calls::Record> && !std::is_move_constructible_v<Calls::Record>);
    T::Fixture fixture(argv[1]);
    T::fixture = &fixture;
    T::HeapHost::State host;
    T::HeapHost::active = &host;
    const auto     image = fixture.Image();
    Calls::Receipt receipt;
    const auto     reset = [&]
    {
        // Host cleanup discards only synthetic test allocations. It is never
        // a production retirement ACK, bank rollback or unknown-debt recovery.
        fixture.ResetDefaultCollision();
        host.Reset(image, &SpecialFixture::Patch, &fixture);
        T::SeedVtable();
        fixture.Slot(T::Model, fixture.Address(fixture.metadata));
        host.afterMalloc = &T::Allocated;
        T::writeMode = T::exchangeMode = T::writes = T::exchanges = T::events = 0;
        T::trace.fill(0);
        T::initialPattern = false;
        T::seenSlot = T::seenExpected = T::seenReplacement = 0;
    };
    const auto create = [&](Calls::Record& record, std::uint64_t generation = T::Generation, std::uint32_t model = T::Model)
    { return Calls::CreateAndPublish(image, record, generation, model, &PoolTestHost::Owner, &receipt, &T::Write, &T::Exchange, &T::HeapHost::Malloc); };
    const auto retire = [&](Calls::Record& record, std::uint64_t generation = T::Generation)
    { return Calls::RetireUnpublished(image, record, generation, &PoolTestHost::Owner, &receipt, &T::HeapHost::Free); };
    reset();
    Calls::Record preentry;
    Check(create(preentry, 0) == ClockResult::InvalidArgument && preentry.CanDiscard() && !host.mallocCalls,
          "zero generation cannot reserve native metadata storage");
    Check(create(preentry, T::Generation, NativeModelBankProfile::ModelCount) == ClockResult::InvalidArgument && preentry.CanDiscard() && !host.mallocCalls,
          "the full native model-key bound is checked before allocation");
    Check(Calls::CreateAndPublish(image, preentry, T::Generation, T::Model, nullptr, &receipt) == ClockResult::NotInitialized && !host.mallocCalls,
          "a private record cannot fabricate the existing callback owner");
    reset();
    std::array<unsigned char, 152> oldBytes{};
    std::memcpy(oldBytes.data(), fixture.metadata, oldBytes.size());
    auto published = std::make_unique<Calls::Record>();
    Check(create(*published) == ClockResult::Ok && receipt.verified && receipt.initEntered && receipt.initReturned && receipt.initVerified &&
              receipt.publishEntered && receipt.publication.returned && receipt.publication.accepted && receipt.publishVerified && receipt.debt &&
              !receipt.unpublishedProved && published->GetStage() == Calls::Stage::Published && !published->CanDiscard() && !published->CanRetireUnpublished(),
          "actual allocate, complete manual initialization and accepted bank publication are journaled in order");
    const auto native = receipt.native, prior = receipt.previousBorrowedModel;
    const auto expected = T::Expected(image, fixture.Canonical());
    Check(host.bytes == 152 && host.alignment == 0 && receipt.allocation.bytes == 152 && !receipt.allocation.capacity && receipt.allocation.base == native &&
              receipt.allocation.readyTag == native && T::initialPattern &&
              !std::memcmp(reinterpret_cast<const void*>(native), expected.data(), expected.size()) && T::trace == std::array<unsigned, 3>{1, 2, 3},
          "fresh allocator bytes become exactly152 zero-based bytes with only ped vptr, canonical collider and full64-bit none sentinel");
    Check(T::Slot() == native && T::seenSlot == image + NativeModelBankProfile::BankRva + T::Model * 8 && T::seenExpected == prior &&
              prior == fixture.Address(fixture.metadata) && T::seenReplacement == native && !std::memcmp(oldBytes.data(), fixture.metadata, oldBytes.size()) &&
              !host.freeCalls,
          "the actual CAS replaces only the bank word and leaves prior borrowed metadata and its lifetime untouched");
    Check(create(*published) == ClockResult::OwnerConflict && host.mallocCalls == 1 && T::writes == 1 && T::exchanges == 1 && T::Slot() == native &&
              retire(*published) == ClockResult::OwnerConflict && !host.freeCalls,
          "published records neither retry, roll back the bank nor raw-free their exposed allocation");
    published.reset();
    Check(host.Contains(native) && !host.freeCalls, "CPU record destruction performs no hidden native free or bank rollback");

    for (unsigned mode = 1; mode <= 4; ++mode)
    {
        reset();
        Calls::Record record;
        T::writeMode = mode;
        Check(create(record) != ClockResult::Ok && receipt.allocation.allocated && receipt.initEntered && !receipt.publishEntered && !T::exchanges &&
                  record.CanRetireUnpublished() && T::Slot() == fixture.Address(fixture.metadata),
              "private initialization prefix, byte mismatch, owner loss or default bookend loss retains only unpublished original storage");
        Check(create(record) == ClockResult::OwnerConflict && host.mallocCalls == 1 && T::writes == 1,
              "a failed initialization itinerary cannot allocate or initialize again");
        PoolTestHost::ownerResult = ClockResult::Ok;
        fixture.SeedDefaultCollision();
        Check(retire(record, T::Generation + 1) == ClockResult::OwnerConflict && !host.freeCalls,
              "unpublished retirement keeps the issued generation and private typed storage authority");
        Check(retire(record) == ClockResult::Ok && record.CanDiscard() && host.freeCalls == 1,
              "explicit same-owner retirement frees only known original unpublished metadata without invoking a model destructor");
    }
    for (const auto mode : {1u, 5u})
    {
        reset();
        Calls::Record record;
        T::exchangeMode = mode;
        Check(create(record) != ClockResult::Ok && receipt.publishEntered && receipt.publication.returned && !receipt.publication.accepted &&
                  receipt.publication.observed != receipt.native && record.CanRetireUnpublished() && T::exchanges == 1,
              "a real known-rejected CAS remains unpublished even if its hook reports a later failure status");
        Check(create(record) == ClockResult::OwnerConflict && T::exchanges == 1 && host.mallocCalls == 1,
              "known rejection is one-shot and never overwrites the changed borrowed bank value");
        Check(retire(record) == ClockResult::Ok && record.CanDiscard() && host.freeCalls == 1,
              "known rejection permits only explicit original-base unpublished retirement");
    }
    for (const auto mode : {2u, 3u, 4u, 6u})
    {
        reset();
        Calls::Record record;
        T::exchangeMode = mode;
        Check(create(record) != ClockResult::Ok && receipt.publishEntered && record.HasOutstandingResource() && !record.CanRetireUnpublished() &&
                  T::exchanges == 1 && !host.freeCalls,
              "unknown CAS, accepted CAS with failing status and accepted post-owner loss retain exposure debt before postchecks");
        PoolTestHost::ownerResult = ClockResult::Ok;
        Check(create(record) == ClockResult::OwnerConflict && retire(record) == ClockResult::OwnerConflict && T::exchanges == 1 && !host.freeCalls,
              "neither raw bank presence nor absence authorizes unknown publication retry, rollback or free");
    }
    reset();
    Calls::Record nullAllocation;
    host.nullMalloc = true;
    Check(create(nullAllocation) != ClockResult::Ok && nullAllocation.CanDiscard() && !T::writes && !T::exchanges && !host.freeCalls,
          "known null allocation never becomes an initialized or published model");
    reset();
    Calls::Record unknownAllocation;
    host.mallocResult = ClockResult::OriginalException;
    Check(create(unknownAllocation) == ClockResult::OriginalException && unknownAllocation.HasOutstandingResource() &&
              !unknownAllocation.CanRetireUnpublished() && !T::writes && !T::exchanges && !host.freeCalls,
          "unknown allocation acknowledgement cannot adopt the allocator seam's observed raw pointer");
    reset();
    Calls::Record allocationOwnerLoss;
    host.afterMalloc = &T::ExpireAllocation;
    Check(create(allocationOwnerLoss) != ClockResult::Ok && receipt.allocation.allocated && allocationOwnerLoss.CanRetireUnpublished() && !T::writes &&
              !T::exchanges,
          "returned allocation is retained before a later owner refusal");
    PoolTestHost::ownerResult = ClockResult::Ok;
    Check(retire(allocationOwnerLoss) == ClockResult::Ok && allocationOwnerLoss.CanDiscard() && host.freeCalls == 1,
          "the known unpublished allocation can explicitly retire after owner recovery");
    T::HeapHost::active = nullptr;
    std::puts("Production native ped model creation fixture completed");
    std::printf("Passed %u native ped model creation checks (64-bit; no native engine code executed)\n", checks);
    return 0;
}
