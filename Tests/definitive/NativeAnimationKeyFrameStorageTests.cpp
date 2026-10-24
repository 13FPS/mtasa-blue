/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Raw frame buffers over the same issued native heap transaction
 ****************************************************************************/
#pragma push_macro("main")
#undef main
#define main HistoricalArrayStorageDefinitionsMain
#include "NativeCustomAnimSequenceStorageTests.cpp"
#undef main
#pragma pop_macro("main")
namespace KeyFrameStorageTests
{
    namespace Fixture = NativeCustomAnimSequenceStorageFixture;
    namespace Frames = NativeAnimationKeyFrameStorageCalls;
    namespace Arrays = NativeCustomAnimSequenceStorageCalls;
    using Storage = NativeCustomAnimSequenceStorage;
    constexpr std::uint64_t Generation = 31;
    struct Context
    {
        SpecialFixture* fixture = nullptr;
        Storage*        nested = nullptr;
        unsigned        scenario = 0;
        ClockResult     nestedResult = ClockResult::NotInitialized;
    };
    unsigned payloadReads = 0;
    void     PayloadObserved()
    {
        ++payloadReads;
    }
    void AfterMalloc(Fixture::State& state) noexcept
    {
        auto& context = *static_cast<Context*>(state.context);
        if (context.scenario == 1)
            PoolTestHost::ownerResult = ClockResult::OwnerConflict;
        if (context.scenario == 2)
            context.fixture->Global(NativeModelReferenceTransactionProfile::HeapAuthorityRva, std::uintptr_t{0});
        if (context.scenario == 3)
            PoolTestHost::failAt = PoolTestHost::reads + 1;
        if (context.scenario == 4)
            context.nestedResult = Arrays::Allocate(state.image, *context.nested, Generation, 1, &PoolTestHost::Owner);
        if (context.scenario == 5)
        {
            PoolTestHost::watched = state.base;
            PoolTestHost::after = &PayloadObserved;
        }
    }
    void AfterFree(Fixture::State& state) noexcept
    {
        const auto& context = *static_cast<Context*>(state.context);
        if (context.scenario == 1)
            PoolTestHost::ownerResult = ClockResult::OwnerConflict;
        if (context.scenario == 2)
            PoolTestHost::failAt = PoolTestHost::reads + 1;
    }
    bool Pattern(std::uintptr_t address, std::uint32_t bytes, unsigned char pattern)
    {
        for (std::uint32_t i = 0; i != bytes; ++i)
            if (reinterpret_cast<const unsigned char*>(address)[i] != pattern)
                return false;
        return true;
    }
}
int main(int argc, char** argv)
{
    using namespace KeyFrameStorageTests;
    if (argc != 2)
        return 2;
    SpecialFixture fixture(argv[1]);
    Fixture::State host;
    Fixture::active = &host;
    const auto image = fixture.transform.image.image;
    Context    context;
    const auto reset = [&]
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
        payloadReads = 0;
    };
    Frames::Receipt receipt;
    reset();
    Storage zero;
    Check(Frames::Allocate(image, zero, 0, 0, nullptr, &receipt) == ClockResult::Ok && receipt.verified && !receipt.entered && !receipt.readyTag &&
              !receipt.initialized && zero.CanDiscard() && !host.mallocCalls && !PoolTestHost::reads,
          "zero frame bytes is a null no-entry lowlevel result; public owner generation admission remains separate");
    reset();
    Storage maximum;
    host.nullMalloc = true;
    Check(Frames::Allocate(image, maximum, Generation, (std::numeric_limits<std::uint32_t>::max)(), &PoolTestHost::Owner, &receipt) == ClockResult::Ok &&
              receipt.entered && receipt.returned && receipt.verified && !receipt.allocated && !receipt.initialized && !receipt.readyTag && !receipt.debt &&
              maximum.GetKind() == Storage::Kind::KeyFrames && maximum.CanDiscard() && host.bytes == (std::numeric_limits<std::uint32_t>::max)() &&
              host.alignment == 0 && host.mallocCalls == 1,
          "full uint32 request reaches the real native size argument without cookie arithmetic; admitted nullable OOM owns no resource");
    unsigned allocateReads = 0, freeReads = 0;
    for (const bool proxy : {false, true})
        for (const std::uint32_t bytes : {std::uint32_t{1}, std::uint32_t{7}, std::uint32_t{80}, std::uint32_t{65536}})
        {
            reset();
            fixture.SeedHeap(proxy);
            Storage    frames;
            const auto result = Frames::Allocate(image, frames, Generation, bytes, &PoolTestHost::Owner, &receipt);
            if (result != ClockResult::Ok)
            {
                std::fprintf(stderr, "frames result=%u proxy=%u bytes=%u entered=%u returned=%u ready=%llx calls=%u debt=%u\n", static_cast<unsigned>(result),
                             static_cast<unsigned>(proxy), bytes, receipt.entered, receipt.returned, static_cast<unsigned long long>(receipt.readyTag),
                             host.mallocCalls, receipt.debt);
                std::fflush(stderr);
            }
            Check(result == ClockResult::Ok && receipt.entered && receipt.returned && receipt.allocated && receipt.verified && !receipt.initialized &&
                      !receipt.capacity && receipt.bytes == bytes && receipt.readyTag == host.base && frames.IssuedTag() == host.base && !frames.Capacity() &&
                      frames.GetKind() == Storage::Kind::KeyFrames && frames.GetState() == Storage::State::LiveUnpublished && frames.CanFreeStorage() &&
                      !frames.CanDiscard() && host.bytes == bytes && host.alignment == 0 && host.mallocCalls == 1 &&
                      host.mallocFunction ==
                          reinterpret_cast<Arrays::MallocFunction>(image + (proxy ? Fixture::Profile::ProxyMallocRva : Fixture::Profile::AuthorityMallocRva)),
                  "frames use the same one native direct/proxy malloc but issue original base and structural live readiness without payload initialization");
            Check(Pattern(frames.IssuedTag(), bytes, 0xa5), "frame payload is never replaced by an array cookie, row defaults or zero initialization");
            if (!proxy && bytes == 1)
                allocateReads = PoolTestHost::reads;
            Check(Arrays::FreeStorage(image, frames, Generation, &PoolTestHost::Owner, &receipt) == ClockResult::OwnerConflict && !receipt.entered &&
                      !host.freeCalls && frames.CanFreeStorage(),
                  "array free refuses a live frame record before native entry without consuming its generic cleanup authority");
            // Caller data is opaque to storage retirement, including values that
            // could look like a forged cookie or child pointer in another type.
            std::memset(reinterpret_cast<void*>(frames.IssuedTag()), 0x3c, bytes);
            fixture.SeedHeap(!proxy);
            PoolTestHost::reads = 0;
            Check(Frames::FreeStorage(image, frames, Generation, &PoolTestHost::Owner, &receipt) == ClockResult::Ok && receipt.entered && receipt.returned &&
                      receipt.released && receipt.verified && !receipt.debt && frames.CanDiscard() && host.freeCalls == 1 && host.base == frames.IssuedTag() &&
                      host.freeFunction ==
                          reinterpret_cast<Arrays::FreeFunction>(image + (!proxy ? Fixture::Profile::ProxyFreeRva : Fixture::Profile::AuthorityFreeRva)),
                  "frame free consumes only original base through current same-authority route, with no payload interpretation or second free");
            if (proxy && bytes == 1)
                freeReads = PoolTestHost::reads;
        }
    reset();
    Storage array, frames;
    Check(Arrays::Allocate(image, array, Generation, 2, &PoolTestHost::Owner, &receipt) == ClockResult::Ok && array.GetKind() == Storage::Kind::SequenceArray &&
              array.Capacity() == 2 && receipt.initialized && receipt.readyTag == host.base + 8 &&
              CustomSequenceStorageTests::Load<std::uint64_t>(host.base) == 2 && CustomSequenceStorageTests::InitializedRows(array.IssuedTag(), 2),
          "selected array regression retains exact eighty-byte rows, full cookie, initialized capacity and interior first tag");
    Check(Frames::Allocate(image, frames, Generation, 80, &PoolTestHost::Owner, &receipt) == ClockResult::Ok && host.mallocCalls == 2,
          "array and frame records coexist under the same current native heap without a second allocator model");
    Check(Frames::FreeStorage(image, array, Generation, &PoolTestHost::Owner, &receipt) == ClockResult::OwnerConflict && !receipt.entered && !host.freeCalls &&
              array.CanFreeStorage(),
          "frame free refuses a live sequence array record before native entry");
    Check(Arrays::FreeStorage(image, array, Generation, &PoolTestHost::Owner, &receipt) == ClockResult::Ok &&
              Frames::FreeStorage(image, frames, Generation, &PoolTestHost::Owner, &receipt) == ClockResult::Ok && host.freeCalls == 2,
          "each mixed record retires exactly once using only its matching operation");
    reset();
    Storage arrayOom;
    host.nullMalloc = true;
    Check(Arrays::Allocate(image, arrayOom, Generation, 1, &PoolTestHost::Owner, &receipt) == ClockResult::WriteFailure && arrayOom.CanDiscard() &&
              receipt.returned && !receipt.allocated && !receipt.verified,
          "selected array positive-null failure remains distinct from nullable frame OOM");
    for (unsigned scenario = 1; scenario <= 3; ++scenario)
    {
        reset();
        Storage oom;
        host.nullMalloc = true;
        context.scenario = scenario;
        host.afterMalloc = &AfterMalloc;
        Check(Frames::Allocate(image, oom, Generation, 80, &PoolTestHost::Owner, &receipt) != ClockResult::Ok && receipt.entered && receipt.returned &&
                  !receipt.verified && !receipt.allocated && !receipt.readyTag && !receipt.debt && oom.CanDiscard() && host.mallocCalls == 1 && !host.freeCalls,
              "nullable native return never launders lost owner, changed heap or failed post-return observation into successful null");
    }
    reset();
    Storage ownerless;
    Check(Frames::Allocate(image, ownerless, Generation, 80, nullptr, &receipt) == ClockResult::NotInitialized && !receipt.entered && ownerless.CanDiscard(),
          "a positive frame request still requires the real native owner before entry");
    reset();
    Storage unknown;
    host.mallocResult = ClockResult::OriginalException;
    Check(Frames::Allocate(image, unknown, Generation, 80, &PoolTestHost::Owner, &receipt) == ClockResult::OriginalException && receipt.entered &&
              !receipt.returned && receipt.debt && unknown.GetKind() == Storage::Kind::KeyFrames && !unknown.CanFreeStorage(),
          "entered frame allocation failure retains the same unknown-return debt without trusting an output or inventing a release base");
    Check(Frames::Allocate(image, unknown, Generation, 80, &PoolTestHost::Owner, &receipt) != ClockResult::Ok && !receipt.entered && host.mallocCalls == 1 &&
              Frames::FreeStorage(image, unknown, Generation, &PoolTestHost::Owner, &receipt) != ClockResult::Ok && !host.freeCalls,
          "unknown frame allocation debt is neither replayed nor guessed into a cleanup call");
    for (unsigned scenario = 1; scenario <= 5; ++scenario)
    {
        reset();
        Storage storage, nested;
        context.scenario = scenario;
        context.nested = &nested;
        host.afterMalloc = &AfterMalloc;
        const auto result = Frames::Allocate(image, storage, Generation, 80, &PoolTestHost::Owner, &receipt);
        if (scenario <= 3)
            Check(result != ClockResult::Ok && receipt.entered && receipt.returned && receipt.allocated && !receipt.initialized && !receipt.readyTag &&
                      storage.GetKind() == Storage::Kind::KeyFrames && storage.CanFreeStorage(),
                  "known frame allocation remains private recoverable debt after owner, heap or read acknowledgement loss");
        if (scenario == 4)
            Check(result == ClockResult::Ok && context.nestedResult == ClockResult::Busy && nested.CanDiscard() && host.mallocCalls == 1,
                  "the existing one shared gate rejects nested array allocation during a frame transaction");
        if (scenario == 5)
            Check(result == ClockResult::Ok && !payloadReads && PoolTestHost::after != nullptr,
                  "frame allocation never observes payload bytes even as a purported capacity cookie");
        PoolTestHost::ownerResult = ClockResult::Ok;
        PoolTestHost::failAt = 0;
        fixture.SeedHeap();
        host.afterMalloc = nullptr;
        Check(Frames::FreeStorage(image, storage, Generation, &PoolTestHost::Owner, &receipt) == ClockResult::Ok && host.freeCalls == 1 &&
                  (scenario != 5 || (!payloadReads && PoolTestHost::after != nullptr)),
              "explicit same-owner unpublished frame cleanup uses original base and never reads payload or rolls back native effects");
    }
    for (const unsigned failAt : {1u, allocateReads})
    {
        reset();
        Storage storage;
        PoolTestHost::failAt = failAt;
        Check(Frames::Allocate(image, storage, Generation, 1, &PoolTestHost::Owner, &receipt) != ClockResult::Ok && !receipt.readyTag &&
                  (failAt == 1 ? !receipt.entered && storage.CanDiscard() : receipt.entered && receipt.debt && storage.CanFreeStorage()),
              "first and last actual frame allocation observations distinguish pre-entry refusal from retained known allocation debt");
    }
    for (unsigned scenario = 0; scenario != 4; ++scenario)
    {
        reset();
        Storage storage;
        Check(Frames::Allocate(image, storage, Generation, 80, &PoolTestHost::Owner, &receipt) == ClockResult::Ok, "prepare one issued live frame record");
        if (scenario == 0)
        {
            Check(Arrays::PinPublication(storage, Generation, image + 0x1000) == ClockResult::Ok && !storage.CanFreeStorage() && !storage.CanDiscard() &&
                      Frames::FreeStorage(image, storage, Generation, &PoolTestHost::Owner, &receipt) != ClockResult::Ok && !receipt.entered && !host.freeCalls,
                  "published frame buffers retain the same conservative debt until real owner detachment is qualified");
            continue;
        }
        context.scenario = scenario;
        host.afterFree = &AfterFree;
        if (scenario == 3)
        {
            host.afterFree = nullptr;
            host.freeResult = ClockResult::OriginalException;
            host.consumeFailedFree = true;
        }
        Check(Frames::FreeStorage(image, storage, Generation, &PoolTestHost::Owner, &receipt) != ClockResult::Ok && receipt.entered && host.freeCalls == 1 &&
                  (scenario == 3 ? !receipt.returned && receipt.debt && !storage.CanDiscard()
                                 : receipt.returned && receipt.released && !receipt.debt && storage.CanDiscard()),
              "frame free shares unknown entered debt and known returned consumption before a lost post acknowledgement");
        PoolTestHost::ownerResult = ClockResult::Ok;
        PoolTestHost::failAt = 0;
        Check(Frames::FreeStorage(image, storage, Generation, &PoolTestHost::Owner, &receipt) != ClockResult::Ok && !receipt.entered && host.freeCalls == 1,
              "neither known consumed nor unknown entered frame free can replay after owner recovery");
    }
    for (const unsigned failAt : {1u, freeReads})
    {
        reset();
        Storage storage;
        Check(Frames::Allocate(image, storage, Generation, 1, &PoolTestHost::Owner, &receipt) == ClockResult::Ok,
              "prepare raw frame free observation boundary");
        PoolTestHost::reads = 0;
        PoolTestHost::failAt = failAt;
        Check(Frames::FreeStorage(image, storage, Generation, &PoolTestHost::Owner, &receipt) != ClockResult::Ok &&
                  (failAt == 1 ? !receipt.entered && !host.freeCalls : receipt.entered && receipt.released && host.freeCalls == 1),
              "first and last actual frame free observations preserve pre-entry refusal versus already consumed original base");
    }
    reset();
    auto live = std::make_unique<Storage>();
    Check(Frames::Allocate(image, *live, Generation, 80, &PoolTestHost::Owner, &receipt) == ClockResult::Ok, "prepare live frame record CPU destruction");
    const auto base = host.base;
    live.reset();
    Check(host.Contains(base) && !host.freeCalls, "frame record CPU destructor performs no guessed native cleanup; runtime must retain outstanding records");
    Fixture::active = nullptr;
    std::printf("Passed %u native keyframe storage checks (64-bit; no native engine code executed)\n", checks);
    std::puts("Production shared native keyframe storage fixture completed");
}
