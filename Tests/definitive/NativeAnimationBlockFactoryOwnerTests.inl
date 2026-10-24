/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Same-owner CPU block products and freshly qualified opaque borrows
 ****************************************************************************/
namespace AnimationBlockFactoryOwnerFixture
{
    void RunPrepared(ClockImageFixture& fixture)
    {
        namespace P = NativeAnimationBlockProfile;
        namespace M = NativeAnimationManagerProfile;
        namespace H = NativeAnimationBlockHierarchyProfile;
        WorldGroundOwnerFixture::Writes writes{fixture, {}};
        NativeAnimationBlockFactoryFixture::Install(fixture.image, &WorldGroundOwnerFixture::Writes::Apply, &writes);
        writes.Global(M::AnimationCountRva, std::int32_t{6});
        NativeAnimationBlockName name{'p', 'e', 'd', 0};
        name[4] = 0x7a;
        fixture.Put(P::BankRva, name);
        fixture.Put(P::BankRva + P::LoadedOffset, static_cast<unsigned char>(0));
        ClockAnimationManagerOwnerTest::watched = fixture.image + P::CountRva;
        ClockAnimationBlockRefOwnerTest::Reset();
        const auto sameIdentity = [](const NativeAnimationBlockIdentity& left, const NativeAnimationBlockIdentity& right)
        { return left.index == right.index && left.name == right.name && left.ownerGeneration == right.ownerGeneration; };
        NativeAnimationBlockIdentity output;
        output.index = 0xe7e7;
        output.name.fill(0xa5);
        output.ownerGeneration = 0x123456789abcdef0ull;
        const auto     sentinel = output;
        std::uintptr_t hierarchy = 0x12345678;
        struct DefaultAccess final : NativeGameTimingAccess
        {
            ClockResult SetTimeScale(float) noexcept override { return ClockResult::NotInitialized; }
            ClockResult SetMinuteDuration(std::uint32_t) noexcept override { return ClockResult::NotInitialized; }
            ClockResult Pause(bool) noexcept override { return ClockResult::NotInitialized; }
        } absent;
        Check(absent.ReadAnimationBlockIdentity(1, 0, output) == ClockResult::NotInitialized && sameIdentity(output, sentinel) &&
                  absent.ReadAnimationBlockInterfaceIdentity(1, fixture.image + P::BankRva, output) == ClockResult::NotInitialized &&
                  sameIdentity(output, sentinel) && absent.ReadAnimationBlockHierarchy(sentinel, 0, hierarchy) == ClockResult::NotInitialized &&
                  hierarchy == 0x12345678,
              "older owner cannot fabricate a generation-stamped block identity or an opaque hierarchy borrow");
        ped_rebuild_fixture::Game         game;
        WorldGroundOwnerFixture::Callback callback;
        ClockResult                       result = ClockResult::NotInitialized;
        NativeClockService                service;
        callback.action = [](NativeGameTimingAccess&) {};
        BeginOwned(service, fixture, false, true, true, &WorldGroundOwnerFixture::Callback::Invoke, &callback);
        const auto                           generation = runtime.generation.load();
        AnimationBlockFactoryManagerReceiver manager(runtime.access, runtime.access, game, generation);
        AnimationBlockFactoryManagerReceiver observations(runtime.access, generation);
        fixture.invokeTimer();
        const auto invoke = [&]
        {
            Check(service.Renew(runtime.generation.load()), "renew only the admitted animation block product owner");
            fixture.invokeTimer();
        };
        Check(runtime.access.ReadAnimationBlockIdentity(generation, 0, output) != ClockResult::Ok && sameIdentity(output, sentinel),
              "CPU generation and locator do not authorize a factory read between callbacks");
        std::thread foreign([&] { result = runtime.access.ReadAnimationBlockInterfaceIdentity(generation, fixture.image + P::BankRva, output); });
        foreign.join();
        Check(result != ClockResult::Ok && sameIdentity(output, sentinel) && !runtime.stopping.load(),
              "foreign factory read preserves identity output without stopping the current owner");
        callback.action = [&](NativeGameTimingAccess& access) { result = access.ReadAnimationBlockIdentity(generation, 0, output); };
        invoke();
        Check(result == ClockResult::Ok && output.index == 0 && output.name == name && output.ownerGeneration == generation,
              "actual native row identity receives CPU generation only after the current owner bookends and preserves the full raw name tail");
        const auto identity = output;
        for (const auto locator : {fixture.image + P::BankRva + 1, fixture.image + P::BankRva + P::ObservationLimit * P::RowBytes})
        {
            output = sentinel;
            callback.action = [&, locator](NativeGameTimingAccess& access)
            { result = access.ReadAnimationBlockInterfaceIdentity(generation, locator, output); };
            invoke();
            Check(result != ClockResult::Ok && sameIdentity(output, sentinel) && !runtime.stopping.load(),
                  "unaligned or out-of-domain raw tags cannot be adopted as qualified current native blocks");
        }
        output = sentinel;
        ClockAnimationOwnerTest::failNext = true;
        callback.action = [&](NativeGameTimingAccess& access) { result = access.ReadAnimationBlockIdentity(generation, 0, output); };
        invoke();
        Check(result == ClockResult::ReadFailure && sameIdentity(output, sentinel) && !runtime.stopping.load(),
              "lost factory row observation is a refusal rather than a missing block or partially published CPU identity");
        NativeAnimationBlockIdentity caller = identity;
        ClockAnimationOwnerTest::identity = &caller;
        ClockAnimationOwnerTest::afterRead = &ClockAnimationOwnerTest::ChangeIndex;
        hierarchy = 0x12345678;
        callback.action = [&](NativeGameTimingAccess& access)
        { result = access.ReadAnimationBlockHierarchy(caller, (std::numeric_limits<std::size_t>::max)(), hierarchy); };
        invoke();
        Check(result == ClockResult::Ok && hierarchy == 0 && caller.index == 1,
              "unloaded hierarchy preserves null before SIZE_MAX or invalid native ranges and uses copied caller identity throughout callbacks");
        ClockAnimationOwnerTest::identity = nullptr;
        std::unique_ptr<CAnimBlock> block, roundtrip, missing;
        CAnimBlockSAInterface*      tag = nullptr;
        bool                        threw = false;
        NativeAnimationBlockName    query{'P', 'E', 'D', 0};
        ClockAnimationManagerOwnerTest::query = &query;
        ClockAnimationManagerOwnerTest::afterRead = &ClockAnimationManagerOwnerTest::ChangeQuery;
        callback.action = [&](NativeGameTimingAccess&)
        {
            threw = false;
            try
            {
                block = manager.GetAnimationBlock(reinterpret_cast<const char*>(query.data()));
                Check(static_cast<bool>(block), "known current name publishes a CPU block");
                tag = block->GetInterface();
            }
            catch (const std::runtime_error&)
            {
                threw = true;
            }
        };
        invoke();
        Check(!threw && block && reinterpret_cast<std::uintptr_t>(tag) == fixture.image + P::BankRva && query[0] != 'P',
              "actual same-owner name factory copies the query before callbacks and returns a complete CPU block with its exact freshly qualified row tag");
        ClockAnimationManagerOwnerTest::query = nullptr;
        callback.action = [&](NativeGameTimingAccess&)
        {
            threw = false;
            try
            {
                missing = manager.GetAnimationBlock("missing");
                roundtrip = manager.GetAnimBlock(tag);
            }
            catch (const std::runtime_error&)
            {
                threw = true;
            }
        };
        invoke();
        Check(!threw && !missing && roundtrip,
              "qualified unknown name remains null while the opaque tag roundtrip creates a separately owned CPU facade without native adoption");
        callback.action = [&](NativeGameTimingAccess&)
        {
            threw = false;
            try
            {
                Check(!observations.GetAnimBlock(nullptr), "null raw clone path requires no positive factory providers");
                (void)observations.GetAnimationBlock("missing");
            }
            catch (const std::runtime_error&)
            {
                threw = true;
            }
        };
        invoke();
        Check(threw && !runtime.stopping.load(),
              "an observation-only manager cannot turn missing game/block providers into a successful missing-name factory result");
        std::puts("Production opaque animation block roundtrip owner fixture completed");

        std::unique_ptr<CAnimBlock> indexed, lastIndexed, invalid;
        int                         firstIndex = -1, lastIndex = -1;
        callback.action = [&](NativeGameTimingAccess&)
        {
            threw = false;
            try
            {
                indexed = manager.GetAnimationBlock(0);
                lastIndexed = manager.GetAnimationBlock(static_cast<int>(P::ObservationLimit - 1));
                Check(indexed && lastIndexed, "valid first and last current integer rows publish CPU blocks");
                firstIndex = indexed->GetIndex();
                lastIndex = lastIndexed->GetIndex();
            }
            catch (const std::exception&)
            {
                threw = true;
            }
        };
        invoke();
        Check(!threw && indexed && lastIndexed && firstIndex == 0 && lastIndex == static_cast<int>(P::ObservationLimit - 1),
              "actual integer factory qualifies first and last current rows without a name lookup, loaded gate or cached block count");
        for (const int bad : {-1, static_cast<int>(P::ObservationLimit)})
        {
            callback.action = [&, bad](NativeGameTimingAccess&)
            {
                threw = false;
                try
                {
                    invalid = manager.GetAnimationBlock(bad);
                }
                catch (const std::exception&)
                {
                    threw = true;
                }
            };
            invoke();
            Check(threw && !invalid && !runtime.stopping.load(),
                  "negative or out-of-domain integer factory intent refuses without publishing a block or stopping the read owner");
        }
        writes.Global(P::CountRva, std::int32_t{1});
        callback.action = [&](NativeGameTimingAccess&)
        {
            threw = false;
            try
            {
                invalid = manager.GetAnimationBlock(1);
            }
            catch (const std::exception&)
            {
                threw = true;
            }
        };
        invoke();
        Check(threw && !invalid && !runtime.stopping.load(),
              "integer factory requires a currently counted native row even when its ordinal is inside the supported observation domain");
        writes.Global(P::CountRva, static_cast<std::int32_t>(P::ObservationLimit));

        // Loaded range checks use the genuine DE row40 and owner ordinal.
        // This borrows only an opaque pointer; no classic hierarchy fields or
        // dynamic sequence buffers are dereferenced by the SDK or this test.
        fixture.Put(P::BankRva + P::LoadedOffset, static_cast<unsigned char>(1));
        fixture.Put(P::BankRva + P::IdOffset, std::int32_t{2});
        fixture.Put(P::BankRva + P::AnimationCountOffset, std::uint32_t{2});
        NativeAnimationBlockFactoryFixture::SeedHierarchy(fixture.image, &WorldGroundOwnerFixture::Writes::Apply, &writes, 2, 0);
        NativeAnimationBlockFactoryFixture::SeedHierarchy(fixture.image, &WorldGroundOwnerFixture::Writes::Apply, &writes, 3, 0);
        CAnimBlendHierarchySAInterface* first = nullptr;
        CAnimBlendHierarchySAInterface* last = nullptr;
        callback.action = [&](NativeGameTimingAccess&)
        {
            threw = false;
            try
            {
                first = block->GetAnimationHierarchyInterface(0);
                last = roundtrip->GetAnimationHierarchyInterface(1);
                block->Request(BLOCKING, false);
            }
            catch (const std::runtime_error&)
            {
                threw = true;
            }
        };
        invoke();
        Check(!threw && reinterpret_cast<std::uintptr_t>(first) == fixture.image + H::BankRva + 2 * H::RowBytes &&
                  reinterpret_cast<std::uintptr_t>(last) == fixture.image + H::BankRva + 3 * H::RowBytes && !ClockAnimationBlockRefOwnerTest::exchanges,
              "actual complete block yields loaded opaque hierarchy addresses and its loaded Request returns before every strictly unavailable game service");
        const auto readHierarchy = [&](std::size_t index)
        {
            hierarchy = 0x12345678;
            callback.action = [&, index](NativeGameTimingAccess& access) { result = access.ReadAnimationBlockHierarchy(identity, index, hierarchy); };
            invoke();
        };
        readHierarchy((std::numeric_limits<std::size_t>::max)());
        Check(result != ClockResult::Ok && hierarchy == 0x12345678 && !runtime.stopping.load(),
              "loaded SIZE_MAX refusal preserves the output rather than overflowing or inventing a native hierarchy lease");
        fixture.Put(H::BankRva + 3 * H::RowBytes + H::OwningBlockOffset, std::uint32_t{1});
        readHierarchy(1);
        Check(result != ClockResult::Ok && hierarchy == 0x12345678 && !runtime.stopping.load(),
              "current hierarchy row owned by another block remains a read-only refusal");
        fixture.Put(H::BankRva + 3 * H::RowBytes + H::OwningBlockOffset, std::uint32_t{0});
        std::puts("Production actual animation block factory SDK owner fixture completed");
        std::uint16_t refsBefore = 0;
        std::memcpy(&refsBefore, reinterpret_cast<const void*>(fixture.image + P::BankRva + NativeAnimationBlockRefProfile::RefOffset), sizeof(refsBefore));
        unsigned destroyed = 0;
        struct TrackingBlock final : CAnimBlockRequestDE
        {
            TrackingBlock(NativeAnimationBlockAccess& access, const NativeAnimationBlockIdentity& identity, CGame& game, unsigned& destroyed)
                : CAnimBlockRequestDE(access, identity, game), destroyed(destroyed)
            {
            }
            ~TrackingBlock() override { ++destroyed; }
            unsigned& destroyed;
        };
        static_assert(std::has_virtual_destructor_v<CAnimBlock> && !std::is_abstract_v<CAnimBlockRequestDE>);
        std::unique_ptr<CAnimBlock> tracked = std::make_unique<TrackingBlock>(runtime.access, identity, game, destroyed);
        const auto                  readsBefore = ClockAnimationOwnerTest::reads;
        tracked.reset();
        roundtrip.reset();
        indexed.reset();
        lastIndexed.reset();
        std::uint16_t refsAfter = 0;
        std::memcpy(&refsAfter, reinterpret_cast<const void*>(fixture.image + P::BankRva + NativeAnimationBlockRefProfile::RefOffset), sizeof(refsAfter));
        Check(destroyed == 1 && ClockAnimationOwnerTest::reads == readsBefore && refsBefore == refsAfter && !ClockAnimationBlockRefOwnerTest::exchanges,
              "unique_ptr<CAnimBlock> runs the real backend-derived CPU destructor once and deletes actual products without native reads, AddRef or cleanup");
        WorldGroundOwnerFixture::Retire(service, fixture);
        NativeAnimationBlockName replacement{'n', 'e', 'w', 0};
        fixture.Put(P::BankRva, replacement);
        NativeClockService          next;
        std::unique_ptr<CAnimBlock> fresh;
        callback.action = [&](NativeGameTimingAccess&)
        {
            threw = false;
            try
            {
                (void)block->GetInterface();
            }
            catch (const std::runtime_error&)
            {
                threw = true;
            }
        };
        BeginOwned(next, fixture, false, true, true, &WorldGroundOwnerFixture::Callback::Invoke, &callback);
        fixture.invokeTimer();
        Check(threw && !runtime.stopping.load(), "old issued block cannot revive its CPU generation even when its native address is reused");
        AnimationBlockFactoryManagerReceiver freshManager(runtime.access, runtime.access, game, runtime.generation.load());
        callback.action = [&](NativeGameTimingAccess&)
        {
            threw = false;
            try
            {
                fresh = freshManager.GetAnimBlock(tag);
                Check(fresh && fresh->GetInterface() == tag && fresh->GetName()[0] == 'n',
                      "fresh raw lookup stamps a new current identity and reads the current native name");
            }
            catch (const std::runtime_error&)
            {
                threw = true;
            }
        };
        Check(next.Renew(runtime.generation.load()), "renew current generation for fresh lookup of a raw locator");
        fixture.invokeTimer();
        Check(!threw && fresh && !runtime.stopping.load(),
              "raw tag is only a fresh locator, so current lookup can qualify its new row without granting an old wrapper a lease");
        WorldGroundOwnerFixture::Retire(next, fixture);
        block.reset();
        fresh.reset();
        ClockAnimationManagerOwnerTest::watched = 0;
        ClockAnimationManagerOwnerTest::afterRead = nullptr;
        ClockAnimationOwnerTest::identity = nullptr;
        ClockAnimationOwnerTest::afterRead = nullptr;
        writes.Restore();
        std::puts("Production native animation block factory owner fixture completed");
    }
}
