/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Owned native storage ABI, lifetime debt and no-replay boundaries
 ****************************************************************************/
#pragma push_macro("main")
#undef main
#define main HistoricalCustomSequenceHeapDefinitionsMain
#include "NativeStreamingSpecialModelTests.cpp"
#undef main
#pragma pop_macro("main")
#include "NativeCustomAnimSequenceStorageFixture.h"
#include <memory>
#include <type_traits>
#define GetModuleHandleW                          PoolTestHost::Module
#define ReadProcessMemory                         PoolTestHost::Read
#define MTA_DE_CUSTOM_ANIM_SEQUENCE_MALLOC_INVOKE ::NativeCustomAnimSequenceStorageFixture::Malloc
#define MTA_DE_CUSTOM_ANIM_SEQUENCE_FREE_INVOKE   ::NativeCustomAnimSequenceStorageFixture::Free
#include "../../Client/game_de/NativeCustomAnimSequenceStorage.cpp"
#undef MTA_DE_CUSTOM_ANIM_SEQUENCE_FREE_INVOKE
#undef MTA_DE_CUSTOM_ANIM_SEQUENCE_MALLOC_INVOKE
#undef ReadProcessMemory
#undef GetModuleHandleW
namespace CustomSequenceStorageTests
{
    namespace Fixture = NativeCustomAnimSequenceStorageFixture;
    namespace Calls = NativeCustomAnimSequenceStorageCalls;
    namespace Profile = NativeCustomAnimSequenceStorageProfile;
    using Storage = NativeCustomAnimSequenceStorage;
    constexpr std::uint64_t Generation = 19;
    struct CallbackContext
    {
        SpecialFixture* fixture = nullptr;
        Storage*        nested = nullptr;
        unsigned        scenario = 0;
        ClockResult     nestedResult = ClockResult::NotInitialized;
    };
    void AfterMalloc(Fixture::State& host) noexcept
    {
        auto& context = *static_cast<CallbackContext*>(host.context);
        if (context.scenario == 1)
            PoolTestHost::ownerResult = ClockResult::OwnerConflict;
        if (context.scenario == 2)
            PoolTestHost::failAt = PoolTestHost::reads + 1;
        if (context.scenario == 3)
            context.fixture->Global(NativeBorrowedGcProfile::markerRva, LONG{1});
        if (context.scenario == 4)
            context.fixture->Global(Profile::AuthorityFreeRva, static_cast<unsigned char>(0xcc));
        if (context.scenario == 5)
            context.nestedResult = Calls::Allocate(host.image, *context.nested, Generation, 1, &PoolTestHost::Owner);
    }
    void AfterFree(Fixture::State& host) noexcept
    {
        const auto& context = *static_cast<CallbackContext*>(host.context);
        if (context.scenario == 1)
            PoolTestHost::ownerResult = ClockResult::OwnerConflict;
        if (context.scenario == 2)
            PoolTestHost::failAt = PoolTestHost::reads + 1;
    }
    void RevokeAtCookie()
    {
        PoolTestHost::ownerResult = ClockResult::OwnerConflict;
    }
    template <class T>
    T Load(std::uintptr_t address)
    {
        T value{};
        std::memcpy(&value, reinterpret_cast<void*>(address), sizeof(value));
        return value;
    }
    bool InitializedRows(std::uintptr_t tag, std::size_t capacity)
    {
        for (std::size_t i = 0; i != capacity; ++i)
        {
            const auto row = tag + i * Profile::RowBytes;
            if (Load<std::uint16_t>(row + Profile::BoneOffset) != 0xffff || Load<std::uint16_t>(row + Profile::FlagsOffset) ||
                Load<std::uint16_t>(row + Profile::FrameCountOffset) || Load<std::uintptr_t>(row + Profile::FramePointerOffset))
                return false;
            for (std::size_t at = 0; at != 0x40; ++at)
                if (Load<unsigned char>(row + at) != 0xa5)
                    return false;
            if (Load<std::uint16_t>(row + 0x46) != 0xa5a5)
                return false;
        }
        return true;
    }
}
int main(int argc, char** argv)
{
    using namespace CustomSequenceStorageTests;
    if (argc != 2)
        return 2;
    static_assert(!std::is_copy_constructible_v<Storage> && !std::is_move_constructible_v<Storage>);
    SpecialFixture fixture(argv[1]);
    Fixture::State host;
    Fixture::active = &host;
    const auto      image = fixture.transform.image.image;
    CallbackContext context;
    const auto      reset = [&]
    {
        fixture.Reset();
        PoolTestHost::image = image;
        host.Reset(image, &SpecialFixture::Patch, &fixture);
        PoolTestHost::reads = PoolTestHost::failAt = PoolTestHost::ownerCalls = PoolTestHost::revokeAt = 0;
        PoolTestHost::ownerResult = ClockResult::Ok;
        PoolTestHost::watched = 0;
        PoolTestHost::after = nullptr;
        context = {&fixture, nullptr, 0, ClockResult::NotInitialized};
        host.context = &context;
    };
    Calls::Receipt receipt;
    reset();
    Storage empty;
    PoolTestHost::ownerResult = ClockResult::WrongThread;
    Check(Calls::Allocate(image, empty, 0, 0, nullptr, &receipt) == ClockResult::Ok && receipt.verified && !receipt.entered && !receipt.readyTag &&
              empty.CanDiscard() && !host.mallocCalls && !PoolTestHost::reads,
          "capacity zero is an exact null result before native owner admission or allocation");
    Check(
        Calls::Allocate(image, empty, Generation, (std::numeric_limits<std::size_t>::max)(), &PoolTestHost::Owner, &receipt) == ClockResult::InvalidArgument &&
            !receipt.entered && empty.CanDiscard() && !host.mallocCalls,
        "full size_t multiplication and cookie overflow refuse before entry without a WORD capacity clamp");
    unsigned completeAllocateReads = 0, completeFreeReads = 0, completeAllocateOwners = 0, completeFreeOwners = 0;
    for (const bool useProxy : {false, true})
        for (const std::size_t capacity : {std::size_t{1}, std::size_t{2}, std::size_t{65536}})
        {
            reset();
            fixture.SeedHeap(useProxy);
            Storage    storage;
            const auto result = Calls::Allocate(image, storage, Generation, capacity, &PoolTestHost::Owner, &receipt);
            if (result != ClockResult::Ok)
            {
                std::fprintf(stderr,
                             "storage allocate proxy=%u capacity=%zu result=%u entered=%u returned=%u initialized=%u calls=%u debt=%u reads=%u owners=%u\n",
                             static_cast<unsigned>(useProxy), capacity, static_cast<unsigned>(result), receipt.entered, receipt.returned, receipt.initialized,
                             host.mallocCalls, receipt.debt, PoolTestHost::reads, PoolTestHost::ownerCalls);
                std::fflush(stderr);
            }
            Check(
                result == ClockResult::Ok && receipt.entered && receipt.returned && receipt.allocated && receipt.initialized && receipt.verified &&
                    receipt.readyTag == host.base + 8 && storage.IssuedTag() == receipt.readyTag && storage.Capacity() == capacity &&
                    storage.OwnerGeneration() == Generation && storage.CanFreeStorage() && !storage.CanDiscard() && host.mallocCalls == 1 &&
                    host.bytes == 8 + capacity * 80 && host.alignment == 0 && host.heap == fixture.Address(useProxy ? fixture.proxy : fixture.heap) &&
                    host.mallocFunction == reinterpret_cast<Calls::MallocFunction>(image + (useProxy ? Profile::ProxyMallocRva : Profile::AuthorityMallocRva)),
                "one genuine current direct/proxy malloc ABI privately issues exactly base+8 after complete initialization");
            Check(Load<std::uint64_t>(host.base) == capacity && InitializedRows(storage.IssuedTag(), capacity),
                  "all capacity rows including beyond 65535 preserve name64/padding and initialize only proven native fields");
            if (!useProxy && capacity == 1)
            {
                completeAllocateReads = PoolTestHost::reads;
                completeAllocateOwners = PoolTestHost::ownerCalls;
            }
            // Current proxy identity may change between callbacks, but the native
            // route still must reach the same captured allocation authority.
            fixture.SeedHeap(!useProxy);
            PoolTestHost::reads = PoolTestHost::ownerCalls = 0;
            Check(Calls::FreeStorage(image, storage, Generation, &PoolTestHost::Owner, &receipt) == ClockResult::Ok && receipt.entered && receipt.returned &&
                      receipt.released && receipt.verified && !receipt.debt && storage.CanDiscard() && !storage.CanFreeStorage() && host.freeCalls == 1 &&
                      host.freeFunction == reinterpret_cast<Calls::FreeFunction>(image + (!useProxy ? Profile::ProxyFreeRva : Profile::AuthorityFreeRva)) &&
                      host.heap == fixture.Address(!useProxy ? fixture.proxy : fixture.heap),
                  "storage-only free follows the current validated outer route to the original authority and consumes the original base once");
            if (useProxy && capacity == 1)
            {
                completeFreeReads = PoolTestHost::reads;
                completeFreeOwners = PoolTestHost::ownerCalls;
            }
            Check(Calls::FreeStorage(image, storage, Generation, &PoolTestHost::Owner, &receipt) != ClockResult::Ok && !receipt.entered && host.freeCalls == 1,
                  "a known returned free cannot replay even after later acknowledgement or raw-tag reuse");
        }
    for (unsigned scenario = 0; scenario != 7; ++scenario)
    {
        reset();
        Storage refused;
        if (scenario == 0)
            PoolTestHost::ownerResult = ClockResult::WrongThread;
        if (scenario == 1)
            fixture.Global(NativeGameStateProfile::stateRva, std::uint32_t{8});
        if (scenario == 2)
            fixture.Global(NativeBorrowedGcProfile::markerRva, LONG{1});
        if (scenario == 3)
            fixture.Global(Profile::AuthorityMallocRva, static_cast<unsigned char>(0xcc));
        if (scenario == 4)
            fixture.Global(AP::HeapVtableRva + 0x30, image + Profile::AuthorityMallocRva);
        if (scenario == 5)
            fixture.Global(AP::HeapAuthorityRva, std::uintptr_t{0});
        if (scenario == 6)
            PoolTestHost::failAt = 1;
        Check(Calls::Allocate(image, refused, Generation, 2, &PoolTestHost::Owner, &receipt) != ClockResult::Ok && !receipt.entered && !receipt.readyTag &&
                  refused.CanDiscard() && !host.mallocCalls,
              "owner/state/GC/code/slot/current heap/read failures refuse before native allocation");
    }
    reset();
    Storage retry;
    Check(Calls::Allocate(image, retry, Generation, 1, nullptr, &receipt) == ClockResult::NotInitialized && retry.CanDiscard() && !receipt.entered,
          "pre-entry refusal leaves a private record available for an explicit later attempt");
    Check(Calls::Allocate(image, retry, Generation, 1, &PoolTestHost::Owner, &receipt) == ClockResult::Ok && host.mallocCalls == 1,
          "a recovered pre-entry owner is admitted without duplicating an earlier native effect");
    Check(Calls::FreeStorage(image, retry, Generation, &PoolTestHost::Owner, &receipt) == ClockResult::Ok, "successful retry is retired normally");
    reset();
    Storage oom;
    host.nullMalloc = true;
    Check(Calls::Allocate(image, oom, Generation, 1, &PoolTestHost::Owner, &receipt) == ClockResult::WriteFailure && receipt.entered && receipt.returned &&
              !receipt.allocated && !receipt.debt && oom.CanDiscard() && !host.freeCalls,
          "normal malloc-null has no owned allocation or invented cleanup debt");
    reset();
    Storage unknown;
    host.mallocResult = ClockResult::OriginalException;
    Check(Calls::Allocate(image, unknown, Generation, 2, &PoolTestHost::Owner, &receipt) == ClockResult::OriginalException && receipt.entered &&
              !receipt.returned && receipt.debt && !receipt.readyTag && !unknown.CanDiscard() && !unknown.CanFreeStorage(),
          "entered allocator failure retains unknown debt without trusting an unreturned output argument");
    Check(Calls::Allocate(image, unknown, Generation, 2, &PoolTestHost::Owner, &receipt) != ClockResult::Ok && !receipt.entered && host.mallocCalls == 1 &&
              Calls::FreeStorage(image, unknown, Generation, &PoolTestHost::Owner, &receipt) != ClockResult::Ok && !host.freeCalls,
          "unknown allocation debt cannot be replayed or guessed into a free");
    for (unsigned scenario = 1; scenario <= 5; ++scenario)
    {
        reset();
        Storage storage, nested;
        context.scenario = scenario;
        context.nested = &nested;
        host.afterMalloc = &AfterMalloc;
        const auto result = Calls::Allocate(image, storage, Generation, 2, &PoolTestHost::Owner, &receipt);
        if (scenario == 5)
            Check(result == ClockResult::Ok && context.nestedResult == ClockResult::Busy && nested.CanDiscard() && host.mallocCalls == 1,
                  "existing shared operation gate rejects nested allocation without another native call");
        else
            Check(result != ClockResult::Ok && receipt.entered && receipt.returned && receipt.allocated && receipt.debt && !receipt.readyTag &&
                      storage.CanFreeStorage() && host.mallocCalls == 1,
                  "known returned allocation survives owner/read/GC/code acknowledgement failure without SDK pointer publication");
        PoolTestHost::ownerResult = ClockResult::Ok;
        PoolTestHost::failAt = 0;
        fixture.Global(NativeBorrowedGcProfile::markerRva, LONG{0});
        Fixture::Install(image, &SpecialFixture::Patch, &fixture);
        host.afterMalloc = nullptr;
        Check(Calls::FreeStorage(image, storage, Generation, &PoolTestHost::Owner, &receipt) == ClockResult::Ok && storage.CanDiscard() && host.freeCalls == 1,
              "explicit recovered original owner can drain only known unpublished storage without native rollback");
    }
    for (const unsigned failAt : {1u, completeAllocateReads})
    {
        reset();
        Storage storage;
        PoolTestHost::failAt = failAt;
        const auto result = Calls::Allocate(image, storage, Generation, 1, &PoolTestHost::Owner, &receipt);
        Check(result != ClockResult::Ok && !receipt.readyTag && (failAt == 1 ? !receipt.entered && storage.CanDiscard() : receipt.entered && receipt.debt),
              "first and measured last distinct allocation read faults preserve their pre-entry or retained-debt boundary");
    }
    for (const unsigned revokeAt : {1u, completeAllocateOwners})
    {
        reset();
        Storage storage;
        PoolTestHost::revokeAt = revokeAt;
        const auto result = Calls::Allocate(image, storage, Generation, 1, &PoolTestHost::Owner, &receipt);
        Check(result != ClockResult::Ok && !receipt.readyTag && (revokeAt == 1 ? !receipt.entered && storage.CanDiscard() : receipt.entered && receipt.debt),
              "first and last actual owner callbacks distinguish pre-entry refusal from private allocation acknowledgement loss");
    }
    for (unsigned scenario = 0; scenario != 6; ++scenario)
    {
        reset();
        Storage storage;
        Check(Calls::Allocate(image, storage, Generation, 2, &PoolTestHost::Owner, &receipt) == ClockResult::Ok, "prepare one privately issued storage record");
        if (scenario == 0)
        {
            const std::uint64_t wrong = 9;
            std::memcpy(reinterpret_cast<void*>(host.base), &wrong, sizeof(wrong));
            Check(Calls::FreeStorage(image, storage, Generation, &PoolTestHost::Owner, &receipt) != ClockResult::Ok && !receipt.entered && !host.freeCalls &&
                      storage.CanFreeStorage(),
                  "initialized cookie mismatch refuses before native free without losing the issued record");
        }
        if (scenario == 1)
        {
            Check(Calls::FreeStorage(image, storage, Generation + 1, &PoolTestHost::Owner, &receipt) == ClockResult::OwnerConflict && !receipt.entered &&
                      !host.freeCalls,
                  "record ownership generation is not granted by possession of an arbitrary tag");
        }
        if (scenario == 2)
        {
            Check(Calls::PinPublication(storage, Generation, image + 0x1000) == ClockResult::Ok && storage.PublicationAffinity() == image + 0x1000 &&
                      !storage.CanDiscard() && !storage.CanFreeStorage() &&
                      Calls::FreeStorage(image, storage, Generation, &PoolTestHost::Owner, &receipt) != ClockResult::Ok && !receipt.entered && !host.freeCalls,
                  "conservative publication pins storage until a future real detach and association lifetime contract exists");
        }
        if (scenario >= 3)
        {
            context.scenario = scenario == 3 ? 1 : 2;
            host.afterFree = &AfterFree;
            if (scenario == 5)
            {
                host.afterFree = nullptr;
                host.freeResult = ClockResult::OriginalException;
                host.consumeFailedFree = true;
            }
            const auto result = Calls::FreeStorage(image, storage, Generation, &PoolTestHost::Owner, &receipt);
            Check(result != ClockResult::Ok && receipt.entered && host.freeCalls == 1 &&
                      (scenario == 5 ? !receipt.returned && receipt.debt && !storage.CanDiscard()
                                     : receipt.returned && receipt.released && !receipt.debt && storage.CanDiscard()),
                  "free exception retains unknown debt while returned-free acknowledgement loss records known consumption before postflight");
            PoolTestHost::ownerResult = ClockResult::Ok;
            PoolTestHost::failAt = 0;
            Check(Calls::FreeStorage(image, storage, Generation, &PoolTestHost::Owner, &receipt) != ClockResult::Ok && !receipt.entered && host.freeCalls == 1,
                  "neither unknown entered free nor known returned free can issue a second free after recovery");
        }
    }
    for (const unsigned failAt : {1u, completeFreeReads})
    {
        reset();
        Storage storage;
        Check(Calls::Allocate(image, storage, Generation, 1, &PoolTestHost::Owner, &receipt) == ClockResult::Ok, "prepare free observation fault case");
        PoolTestHost::reads = 0;
        PoolTestHost::failAt = failAt;
        Check(Calls::FreeStorage(image, storage, Generation, &PoolTestHost::Owner, &receipt) != ClockResult::Ok &&
                  (failAt == 1 ? !receipt.entered && !host.freeCalls : receipt.entered && receipt.released && host.freeCalls == 1),
              "first and measured last free observations do not conflate pre-entry refusal with already consumed storage");
    }
    for (const unsigned revokeAt : {1u, completeFreeOwners})
    {
        reset();
        Storage storage;
        Check(Calls::Allocate(image, storage, Generation, 1, &PoolTestHost::Owner, &receipt) == ClockResult::Ok, "prepare free owner bookend case");
        PoolTestHost::ownerCalls = 0;
        PoolTestHost::revokeAt = revokeAt;
        Check(Calls::FreeStorage(image, storage, Generation, &PoolTestHost::Owner, &receipt) != ClockResult::Ok &&
                  (revokeAt == 1 ? !receipt.entered && !host.freeCalls : receipt.entered && receipt.released && host.freeCalls == 1),
              "first and last actual free owner callbacks preserve known consumption and never infer a second free");
    }
    reset();
    Storage cookieFence;
    Check(Calls::Allocate(image, cookieFence, Generation, 1, &PoolTestHost::Owner, &receipt) == ClockResult::Ok,
          "prepare final storage observation owner fence");
    PoolTestHost::watched = host.base;
    PoolTestHost::after = &RevokeAtCookie;
    Check(Calls::FreeStorage(image, cookieFence, Generation, &PoolTestHost::Owner, &receipt) == ClockResult::OwnerConflict && !receipt.entered &&
              !host.freeCalls && cookieFence.CanFreeStorage(),
          "owner revocation at the final cookie observation is caught before FreeEntered and preserves cleanup authority");
    reset();
    auto storage = std::make_unique<Storage>();
    Check(Calls::Allocate(image, *storage, Generation, 1, &PoolTestHost::Owner, &receipt) == ClockResult::Ok, "prepare actual storage-only retirement");
    auto* frame = static_cast<unsigned char*>(VirtualAlloc(nullptr, 0x1000, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    Check(frame != nullptr, "own an independent frame allocation outside the array lifetime");
    frame[0] = 0x7b;
    const auto frameAddress = reinterpret_cast<std::uintptr_t>(frame);
    std::memcpy(reinterpret_cast<void*>(storage->IssuedTag() + Profile::FramePointerOffset), &frameAddress, sizeof(frameAddress));
    Check(Calls::FreeStorage(image, *storage, Generation, &PoolTestHost::Owner, &receipt) == ClockResult::Ok && frame[0] == 0x7b && host.freeCalls == 1,
          "storage-only free never traverses a frame pointer or invokes whole RemoveAnimSequences");
    VirtualFree(frame, 0, MEM_RELEASE);
    storage.reset();
    Check(host.freeCalls == 1, "CPU record destruction never issues hidden native cleanup");
    reset();
    storage = std::make_unique<Storage>();
    Check(Calls::Allocate(image, *storage, Generation, 1, &PoolTestHost::Owner, &receipt) == ClockResult::Ok,
          "prepare deliberately retained unpublished storage for CPU-only record destruction");
    const auto retainedBase = host.base;
    storage.reset();
    Check(host.Contains(retainedBase) && !host.freeCalls,
          "destroying a live record does not guess native cleanup; the actual controller must retain records and module debt");
    Fixture::active = nullptr;
    std::printf("Passed %u native custom sequence storage checks (64-bit; no native engine code executed)\n", checks);
    std::puts("Production custom sequence storage fixture completed");
}
