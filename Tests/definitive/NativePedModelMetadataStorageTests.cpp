/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Fixed raw metadata storage over the actual issued heap journal
 ****************************************************************************/
#pragma push_macro("main")
#undef main
#define main HistoricalPedMetadataHeapDefinitionsMain
#include "NativeCustomAnimSequenceStorageTests.cpp"
#undef main
#pragma pop_macro("main")
#include "../../Client/game_de/NativePedModelMetadataStorageCalls.h"

namespace PedMetadataStorageTests
{
    namespace Calls = mta::game_de::NativePedModelMetadataStorageCalls;
    namespace Arrays = mta::game_de::NativeCustomAnimSequenceStorageCalls;
    namespace Frames = mta::game_de::NativeAnimationKeyFrameStorageCalls;
    namespace Dynamic = mta::game_de::NativeAnimationAssociationOuterStorageCalls;
    namespace Fixture = NativeCustomAnimSequenceStorageFixture;
    namespace Profile = mta::game_de::NativeCustomAnimSequenceStorageProfile;
    using Storage = mta::game_de::NativeCustomAnimSequenceStorage;
    constexpr std::uint64_t Generation = 73;
    unsigned                payloadReads = 0;
    void                    ObservePayload()
    {
        ++payloadReads;
    }
    struct Context
    {
        SpecialFixture* fixture = nullptr;
        Storage*        nested = nullptr;
        unsigned        scenario = 0;
        ClockResult     nestedResult = ClockResult::NotInitialized;
    };
    void AfterMalloc(Fixture::State& host) noexcept
    {
        auto& context = *static_cast<Context*>(host.context);
        if (context.scenario == 1)
            PoolTestHost::ownerResult = ClockResult::OwnerConflict;
        if (context.scenario == 2)
            PoolTestHost::failAt = PoolTestHost::reads + 1;
        if (context.scenario == 3)
            context.fixture->Global(NativeModelReferenceTransactionProfile::HeapAuthorityRva, std::uintptr_t{0});
        if (context.scenario == 4)
            context.nestedResult = Calls::Allocate(host.image, *context.nested, Generation, &PoolTestHost::Owner);
        if (context.scenario == 5)
        {
            PoolTestHost::watched = host.base;
            PoolTestHost::after = &ObservePayload;
        }
    }
    void AfterFree(Fixture::State& host) noexcept
    {
        const auto& context = *static_cast<Context*>(host.context);
        if (context.scenario == 1)
            PoolTestHost::ownerResult = ClockResult::OwnerConflict;
        if (context.scenario == 2)
            PoolTestHost::failAt = PoolTestHost::reads + 1;
    }
    bool Pattern(std::uintptr_t address, unsigned char byte)
    {
        for (std::size_t index = 0; index != Calls::AllocationBytes; ++index)
            if (reinterpret_cast<const unsigned char*>(address)[index] != byte)
                return false;
        return true;
    }
}

int main(int argc, char** argv)
{
    using namespace PedMetadataStorageTests;
    if (argc != 2)
        return 2;
    static_assert(Calls::AllocationBytes == 152);
    static_assert(!std::is_copy_constructible_v<Storage> && !std::is_move_constructible_v<Storage>);
    SpecialFixture fixture(argv[1]);
    Fixture::State host;
    Fixture::active = &host;
    const auto image = fixture.transform.image.image;
    Context    context;
    const auto reset = [&]
    {
        // Fixture ownership frees only its synthetic VirtualAlloc buffers.
        // It never stands in for a production native debt acknowledgement.
        fixture.Reset();
        PoolTestHost::image = image;
        host.Reset(image, &SpecialFixture::Patch, &fixture);
        PoolTestHost::reads = PoolTestHost::failAt = PoolTestHost::ownerCalls = PoolTestHost::revokeAt = 0;
        PoolTestHost::ownerResult = ClockResult::Ok;
        PoolTestHost::watched = 0;
        PoolTestHost::after = nullptr;
        payloadReads = 0;
        context = {&fixture, nullptr, 0, ClockResult::NotInitialized};
        host.context = &context;
    };
    Calls::Receipt receipt;
    reset();
    Storage preentry;
    Check(Calls::Allocate(image, preentry, 0, &PoolTestHost::Owner, &receipt) == ClockResult::InvalidArgument && !receipt.entered && preentry.CanDiscard() &&
              !host.mallocCalls,
          "fixed positive storage still requires a generation before allocation");
    Check(Calls::Allocate(image, preentry, Generation, nullptr, &receipt) == ClockResult::NotInitialized && !receipt.entered && preentry.CanDiscard() &&
              !host.mallocCalls,
          "missing owner refuses before native entry without a constructor or storage success");
    Check(Calls::Allocate(image, preentry, Generation, &PoolTestHost::Owner, &receipt) == ClockResult::Ok && host.mallocCalls == 1 &&
              Calls::FreeStorage(image, preentry, Generation, &PoolTestHost::Owner, &receipt) == ClockResult::Ok,
          "a no-effect refusal permits an explicit later allocation and exact same-kind retirement");

    for (const bool proxy : {false, true})
    {
        reset();
        fixture.SeedHeap(proxy);
        Storage storage;
        context.scenario = 5;
        host.afterMalloc = &AfterMalloc;
        Check(Calls::Allocate(image, storage, Generation, &PoolTestHost::Owner, &receipt) == ClockResult::Ok && receipt.entered && receipt.returned &&
                  receipt.allocated && !receipt.initialized && receipt.verified && receipt.debt && receipt.capacity == 0 && receipt.bytes == 152 &&
                  receipt.base == host.base && receipt.readyTag == host.base && storage.IssuedTag() == host.base && storage.ByteSize() == 152 &&
                  storage.Capacity() == 0 && storage.GetKind() == Storage::Kind::PedModelMetadata && storage.OwnerGeneration() == Generation &&
                  storage.CanFreeStorage() && !storage.CanDiscard() && host.bytes == 152 && host.alignment == 0 && host.mallocCalls == 1 &&
                  host.heap == fixture.Address(proxy ? fixture.proxy : fixture.heap) &&
                  host.mallocFunction == reinterpret_cast<Arrays::MallocFunction>(image + (proxy ? Profile::ProxyMallocRva : Profile::AuthorityMallocRva)),
              "actual heap direct/proxy entry issues exactly152 bytes with alignment0, original tag and a distinct metadata-only kind");
        Check(Pattern(host.base, 0xa5) && !payloadReads,
              "all152 allocator-pattern bytes remain unchanged: no cookie, zeroing, vptr, model or native constructor promise");
        Check(Arrays::FreeStorage(image, storage, Generation, &PoolTestHost::Owner, &receipt) == ClockResult::OwnerConflict && !receipt.entered &&
                  Frames::FreeStorage(image, storage, Generation, &PoolTestHost::Owner, &receipt) == ClockResult::OwnerConflict && !receipt.entered &&
                  Dynamic::FreeUnconstructed(image, storage, Generation, &PoolTestHost::Owner, &receipt) == ClockResult::OwnerConflict && !receipt.entered &&
                  !host.freeCalls && storage.CanFreeStorage(),
              "array, frame and dynamic-only frees cannot consume metadata storage");
        Check(Calls::FreeStorage(image, storage, Generation + 1, &PoolTestHost::Owner, &receipt) == ClockResult::OwnerConflict && !receipt.entered &&
                  !host.freeCalls,
              "the issued record retains its original generation through explicit storage retirement");
        std::memset(reinterpret_cast<void*>(host.base), 0x3c, Calls::AllocationBytes);
        fixture.SeedHeap(!proxy);
        Check(Calls::FreeStorage(image, storage, Generation, &PoolTestHost::Owner, &receipt) == ClockResult::Ok && receipt.entered && receipt.returned &&
                  receipt.released && receipt.verified && !receipt.debt && storage.CanDiscard() && host.freeCalls == 1 && !payloadReads &&
                  host.freeFunction == reinterpret_cast<Arrays::FreeFunction>(image + (!proxy ? Profile::ProxyFreeRva : Profile::AuthorityFreeRva)),
              "typed storage free uses the current same-authority route and original base without reading opaque metadata payload");
        Check(Calls::FreeStorage(image, storage, Generation, &PoolTestHost::Owner, &receipt) == ClockResult::OwnerConflict && !receipt.entered &&
                  host.freeCalls == 1,
              "known consumed metadata storage cannot replay its free");
    }
    for (unsigned kind = 0; kind != 3; ++kind)
    {
        reset();
        Storage     storage;
        ClockResult result = ClockResult::NotInitialized;
        if (kind == 0)
            result = Arrays::Allocate(image, storage, Generation, 1, &PoolTestHost::Owner, &receipt);
        if (kind == 1)
            result = Frames::Allocate(image, storage, Generation, 152, &PoolTestHost::Owner, &receipt);
        if (kind == 2)
            result = Dynamic::Allocate(image, storage, Generation, &PoolTestHost::Owner, &receipt);
        Check(result == ClockResult::Ok && Calls::FreeStorage(image, storage, Generation, &PoolTestHost::Owner, &receipt) == ClockResult::OwnerConflict &&
                  !receipt.entered && !host.freeCalls && storage.CanFreeStorage(),
              "new metadata-only free rejects every original issued animation kind, including a same-sized152-byte frame");
        if (kind == 0)
            result = Arrays::FreeStorage(image, storage, Generation, &PoolTestHost::Owner, &receipt);
        if (kind == 1)
            result = Frames::FreeStorage(image, storage, Generation, &PoolTestHost::Owner, &receipt);
        if (kind == 2)
            result = Dynamic::FreeUnconstructed(image, storage, Generation, &PoolTestHost::Owner, &receipt);
        Check(result == ClockResult::Ok && host.freeCalls == 1 && storage.CanDiscard(), "original kind-specific cleanup authority remains intact");
    }

    reset();
    Storage nullStorage;
    host.nullMalloc = true;
    Check(Calls::Allocate(image, nullStorage, Generation, &PoolTestHost::Owner, &receipt) == ClockResult::WriteFailure && receipt.entered && receipt.returned &&
              !receipt.allocated && !receipt.initialized && !receipt.debt && nullStorage.CanDiscard() && !host.freeCalls,
          "a known null allocation owns no bytes and is not a successful or initialized model");
    for (unsigned scenario = 1; scenario <= 3; ++scenario)
    {
        reset();
        Storage storage;
        context.scenario = scenario;
        host.afterMalloc = &AfterMalloc;
        Check(Calls::Allocate(image, storage, Generation, &PoolTestHost::Owner, &receipt) != ClockResult::Ok && receipt.entered && receipt.returned &&
                  receipt.allocated && receipt.base == host.base && !receipt.readyTag && !receipt.initialized && receipt.debt &&
                  storage.GetState() == Storage::State::FaultDebt && storage.GetKind() == Storage::Kind::PedModelMetadata && storage.CanFreeStorage(),
              "returned allocation is durably retained before fallible owner, read or heap postchecks");
        PoolTestHost::ownerResult = ClockResult::Ok;
        PoolTestHost::failAt = 0;
        fixture.SeedHeap();
        host.afterMalloc = nullptr;
        Check(Calls::FreeStorage(image, storage, Generation, &PoolTestHost::Owner, &receipt) == ClockResult::Ok && host.freeCalls == 1,
              "same-owner explicit recovery can free a known returned unpublished storage allocation without init or retry");
    }
    reset();
    Storage unknownMalloc;
    host.mallocResult = ClockResult::OriginalException;
    Check(Calls::Allocate(image, unknownMalloc, Generation, &PoolTestHost::Owner, &receipt) == ClockResult::OriginalException && receipt.entered &&
              !receipt.returned && !receipt.allocated && receipt.debt && unknownMalloc.HasOutstandingResource() && !unknownMalloc.CanFreeStorage(),
          "an entered unknown malloc retains debt instead of adopting a possibly returned raw tag");
    Check(Calls::Allocate(image, unknownMalloc, Generation, &PoolTestHost::Owner, &receipt) == ClockResult::OwnerConflict && !receipt.entered &&
              Calls::FreeStorage(image, unknownMalloc, Generation, &PoolTestHost::Owner, &receipt) == ClockResult::OwnerConflict && !receipt.entered &&
              host.mallocCalls == 1 && !host.freeCalls,
          "unknown allocation cannot replay or guess a free from the fixture's address observation");
    reset();
    Storage outer, nested;
    context.scenario = 4;
    context.nested = &nested;
    host.afterMalloc = &AfterMalloc;
    Check(Calls::Allocate(image, outer, Generation, &PoolTestHost::Owner, &receipt) == ClockResult::Ok && context.nestedResult == ClockResult::Busy &&
              nested.CanDiscard() && host.mallocCalls == 1,
          "the existing shared gate rejects reentrant metadata allocation without another native entry");
    host.afterMalloc = nullptr;
    Check(Calls::FreeStorage(image, outer, Generation, &PoolTestHost::Owner, &receipt) == ClockResult::Ok, "known outer storage retires after nested refusal");

    for (unsigned scenario = 1; scenario <= 2; ++scenario)
    {
        reset();
        Storage storage;
        Check(Calls::Allocate(image, storage, Generation, &PoolTestHost::Owner, &receipt) == ClockResult::Ok, "prepare returned-free postcheck boundary");
        context.scenario = scenario;
        host.afterFree = &AfterFree;
        Check(Calls::FreeStorage(image, storage, Generation, &PoolTestHost::Owner, &receipt) != ClockResult::Ok && receipt.entered && receipt.returned &&
                  receipt.released && !receipt.debt && storage.CanDiscard() && host.freeCalls == 1,
              "free acknowledgement consumes original storage before later owner or read refusal");
        PoolTestHost::ownerResult = ClockResult::Ok;
        PoolTestHost::failAt = 0;
        Check(Calls::FreeStorage(image, storage, Generation, &PoolTestHost::Owner, &receipt) == ClockResult::OwnerConflict && !receipt.entered &&
                  host.freeCalls == 1,
              "postcheck refusal cannot resurrect consumed storage or replay the real free");
    }
    for (const bool consumed : {false, true})
    {
        reset();
        Storage storage;
        Check(Calls::Allocate(image, storage, Generation, &PoolTestHost::Owner, &receipt) == ClockResult::Ok, "prepare unknown free boundary");
        host.freeResult = ClockResult::OriginalException;
        host.consumeFailedFree = consumed;
        Check(Calls::FreeStorage(image, storage, Generation, &PoolTestHost::Owner, &receipt) == ClockResult::OriginalException && receipt.entered &&
                  !receipt.returned && !receipt.released && receipt.debt && storage.HasOutstandingResource() && !storage.CanFreeStorage(),
              "unknown free acknowledgement retains debt whether the test resource was consumed or still exists");
        host.freeResult = ClockResult::Ok;
        Check(Calls::FreeStorage(image, storage, Generation, &PoolTestHost::Owner, &receipt) == ClockResult::OwnerConflict && !receipt.entered &&
                  host.freeCalls == 1,
              "neither raw absence nor presence authorizes replay of an entered unknown free");
    }
    reset();
    auto live = std::make_unique<Storage>();
    Check(Calls::Allocate(image, *live, Generation, &PoolTestHost::Owner, &receipt) == ClockResult::Ok && live->HasOutstandingResource(),
          "a live metadata record reports its resource retention obligation");
    const auto base = host.base;
    live.reset();
    Check(host.Contains(base) && !host.freeCalls, "CPU destruction performs no hidden model destructor, free or bank operation; actual owner must retain debt");
    Fixture::active = nullptr;
    std::puts("Production native ped model metadata storage fixture completed");
    std::printf("Passed %u native ped model metadata storage checks (64-bit; no native engine code executed)\n", checks);
    return 0;
}
