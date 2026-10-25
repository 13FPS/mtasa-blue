/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Qualified cache0 and no-child cleanup preserve actual ownership
 ****************************************************************************/
namespace mta::game_de::PrivateAnimationHierarchyDetail
{
    ClockResult ClearEmptySequences(std::uintptr_t, std::uint32_t&) noexcept;
}
namespace ClockPrivateHierarchyEmptyWriteFixture
{
    unsigned                  writes = 0, mode = 0;
    std::uintptr_t            image = 0;
    mta::game_de::ClockResult Write(std::uintptr_t address, std::uint32_t& exception) noexcept
    {
        ++writes;
        const auto result = mta::game_de::PrivateAnimationHierarchyDetail::ClearEmptySequences(address, exception);
        if (result != mta::game_de::ClockResult::Ok)
            return result;
        // Run the actual three-field CPU stores, then schedule an unknown
        // result or real GC loss. Neither substitutes a cleanup receipt.
        if (mode == 1)
        {
            exception = EXCEPTION_ACCESS_VIOLATION;
            return mta::game_de::ClockResult::OriginalException;
        }
        if (mode == 2)
            *reinterpret_cast<LONG*>(image + mta::game_de::NativeBorrowedGcProfile::markerRva) = 1;
        return result;
    }
}
namespace PrivateHierarchyEmptyOwnerFixture
{
    namespace Hook = ClockPrivateHierarchyEmptyWriteFixture;
    namespace Calls = NativePrivateAnimationHierarchyCalls;
    namespace Host = NativeCustomAnimSequenceStorageFixture;
    using Parent = NativePrivateAnimationHierarchyController;
    using Children = NativeCustomAnimSequenceController;
    using Token = NativeAnimationHierarchyReferenceToken;
    unsigned    scenario = 0;
    bool        refuseOwner = false;
    ClockResult Owner() noexcept
    {
        const auto result = CheckTimingOwner();
        return result == ClockResult::Ok && refuseOwner ? ClockResult::OwnerConflict : result;
    }
    void Pending(ClockImageFixture& fixture, std::uint64_t generation)
    {
        Children       children;
        Parent         parent(&children);
        Token          token;
        std::uintptr_t array = 0;
        Check(children.Allocate(fixture.image, generation, 1, &Owner, array) == ClockResult::Ok &&
                  parent.Create(fixture.image, generation, &Owner, token) == ClockResult::Ok,
              "create actual pending-command controllers without a fabricated private phase");
        refuseOwner = true;
        const auto before = Hook::writes;
        Check(parent.SetSequences(fixture.image, token, array, &Owner) != ClockResult::Ok && !*reinterpret_cast<std::uintptr_t*>(token.locator + 8),
              "failed actual preparation keeps a planned parent command with physical pointer zero");
        refuseOwner = false;
        Check(parent.RemoveEmptySequences(fixture.image, token, &Owner) != ClockResult::Ok && Hook::writes == before,
              "pointer zero cannot erase an active preparation journal through empty cleanup");
        NativeAnimationHierarchyValues unknown;
        Check(parent.SetSequences(fixture.image, token, 0, &Owner) == ClockResult::Ok &&
                  parent.RemoveEmptySequences(fixture.image, token, &Owner) == ClockResult::Ok &&
                  parent.Read(fixture.image, token, &Owner, unknown) == ClockResult::Ok && !unknown.hashDefined &&
                  children.FreeStorage(fixture.image, generation, array, &Owner) == ClockResult::Ok &&
                  parent.Destroy(fixture.image, token, &Owner) == ClockResult::Ok && !children.IsRetained() && !parent.IsRetained(),
              "only actual matching no-entry cancellation permits later explicit empty cleanup and retirement");
    }
    void Run(ClockImageFixture& fixture)
    {
        WorldGroundOwnerFixture::Writes saved{fixture, {}};
        const auto                      admit = [&](const auto& pins)
        {
            for (const auto& pin : pins)
                WorldGroundOwnerFixture::Writes::Apply(&saved, pin.rva, pin.bytes, pin.size, pin.code ? PAGE_EXECUTE_READ : PAGE_READONLY);
        };
        for (const auto& pin : NativeStreamingLoadAllProfile::Code)
            WorldGroundOwnerFixture::Writes::Apply(&saved, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        admit(NativeStartupOwnerProfile::pins);
        admit(NativeStartupWorldProfile::pins);
        for (const auto& pin : NativeUObjectMembershipProfile::pins)
            WorldGroundOwnerFixture::Writes::Apply(&saved, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        admit(NativeGameStateProfile::pins);
        for (const auto& pin : NativeBorrowedGcProfile::pins)
            WorldGroundOwnerFixture::Writes::Apply(&saved, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : NativeStartGameProfile::Code)
            if (pin.rva == 0xab879c || pin.rva == 0xab92f0 || pin.rva == 0x1af4ef0)
                WorldGroundOwnerFixture::Writes::Apply(&saved, pin.rva, pin.bytes.data(), pin.size, PAGE_EXECUTE_READ);
        alignas(std::uintptr_t) std::array<unsigned char, 0x180> root{};
        NativeStreamingLoadAllFixture::Graph graph(fixture.image, reinterpret_cast<std::uintptr_t>(root.data()), &WorldGroundOwnerFixture::Writes::Apply,
                                                   &saved);
        NativePedClothesFixture::Install(fixture.image, &WorldGroundOwnerFixture::Writes::Apply, &saved);
        NativePedClothesFixture::NameState names;
        names.Reset(fixture.image, &WorldGroundOwnerFixture::Writes::Apply, &saved);
        Host::State host;
        host.Reset(fixture.image, &WorldGroundOwnerFixture::Writes::Apply, &saved);
        Host::active = &host;
        for (const auto& pin : NativeAnimationSequenceRowProfile::Code)
            WorldGroundOwnerFixture::Writes::Apply(&saved, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : NativeAnimationSequenceFrameBindProfile::Code)
            WorldGroundOwnerFixture::Writes::Apply(&saved, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        NativeAnimationBlockFactoryFixture::Install(fixture.image, &WorldGroundOwnerFixture::Writes::Apply, &saved);
        for (const auto& pin : NativeAnimationHierarchyReadProfile::Code)
            WorldGroundOwnerFixture::Writes::Apply(&saved, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        WorldGroundOwnerFixture::Writes::Apply(&saved, 0xfec67a, private_hierarchy_fixture::CacheProof, sizeof(private_hierarchy_fixture::CacheProof),
                                               PAGE_EXECUTE_READ);
        graph.Reset();
        saved.Global(NativeGameStateProfile::stateRva, std::uint32_t{9});
        saved.Global(NativeStartGameProfile::ThreadInitializedRva, static_cast<unsigned char>(1));
        saved.Global(NativeStartGameProfile::ThreadIdRva, GetCurrentThreadId());
        saved.Global(NativeBorrowedGcProfile::markerRva, LONG{0});
        const auto previousImage = ClockReaderTestHost::image;
        ClockReaderTestHost::image = fixture.image;
        Hook::image = fixture.image;
        Hook::writes = Hook::mode = 0;
        WorldGroundOwnerFixture::Callback callback;
        NativeClockService                service;
        callback.action = [](NativeGameTimingAccess&) {};
        BeginOwned(service, fixture, false, true, true, &WorldGroundOwnerFixture::Callback::Invoke, &callback);
        const auto generation = runtime.generation.load();
        const auto invoke = [&]
        {
            Check(service.Renew(generation), "renew only the current empty-operations callback owner");
            fixture.invokeTimer();
        };
        if (!scenario)
        {
            callback.action = [&](NativeGameTimingAccess&) { Pending(fixture, generation); };
            invoke();
        }
        Token token;
        callback.action = [&](NativeGameTimingAccess& access)
        {
            Check(dynamic_cast<NativeAnimationHierarchyAccess*>(&access) == static_cast<NativeAnimationHierarchyAccess*>(&runtime.access) &&
                      runtime.access.CreatePrivateAnimationHierarchy(generation, token) == ClockResult::Ok,
                  "new operations dispatch on the existing actual hierarchy callback owner");
        };
        invoke();
        animation_hierarchy_fixture::AnimationHierarchyReceiver sdk(runtime.access, token);
        std::array<unsigned char, 40>                           expected;
        callback.action = [&](NativeGameTimingAccess&)
        {
            sdk.SetNumSequences(0xffff);
            sdk.SetName("Empty-Operations");
            sdk.SetRunningCompressed(true);
            sdk.SetAnimationBlockID((std::numeric_limits<int>::min)());
        };
        invoke();
        const std::uint32_t nan = 0x7fc12345;
        std::memcpy(reinterpret_cast<void*>(token.locator + 0x18), &nan, 4);
        std::memset(reinterpret_cast<void*>(token.locator + 4), 0xa5, 4);
        std::memset(reinterpret_cast<void*>(token.locator + 0x1c), 0x5a, 4);
        std::memcpy(expected.data(), reinterpret_cast<void*>(token.locator), expected.size());
        const auto beforeWrite = Hook::writes;
        const auto beforeMalloc = host.mallocCalls, beforeFree = host.freeCalls;
        callback.action = [&](NativeGameTimingAccess&)
        {
            sdk.RemoveFromUncompressedCache();
            Check(!std::memcmp(expected.data(), reinterpret_cast<void*>(token.locator), expected.size()) && Hook::writes == beforeWrite,
                  "qualified cache0 operation has no stores even with nonzero count and raw NaN time");
        };
        invoke();
        if (scenario == 3)
        {
            const std::uintptr_t foreign = 1;
            std::memcpy(reinterpret_cast<void*>(token.locator + 0x20), &foreign, sizeof(foreign));
            callback.action = [&](NativeGameTimingAccess&)
            {
                Check(runtime.access.RemovePrivateAnimationHierarchyFromUncompressedCache(token) != ClockResult::Ok &&
                          runtime.access.RemoveEmptyPrivateAnimationHierarchySequences(token) != ClockResult::Ok && Hook::writes == beforeWrite,
                      "foreign cache refuses both restricted operations before any cleanup writer entry");
            };
            invoke();
            const std::uintptr_t zero = 0;
            std::memcpy(reinterpret_cast<void*>(token.locator + 0x20), &zero, sizeof(zero));
        }
        else
        {
            callback.action = [&](NativeGameTimingAccess&)
            {
                Hook::mode = scenario == 1 ? 2u : scenario == 2 ? 1u : 0u;
                const auto result = runtime.access.RemoveEmptyPrivateAnimationHierarchySequences(token);
                Check((scenario ? result != ClockResult::Ok : result == ClockResult::Ok) && Hook::writes == beforeWrite + 1,
                      "explicit empty operation delegates exactly one real three-field cleanup writer");
            };
            invoke();
            Hook::mode = 0;
            const std::uintptr_t noPointer = 0;
            const std::uint16_t  noCount = 0;
            const std::uint32_t  noTime = 0;
            std::memcpy(expected.data() + 8, &noPointer, sizeof(noPointer));
            std::memcpy(expected.data() + 0x10, &noCount, 2);
            std::memcpy(expected.data() + 0x18, &noTime, 4);
            Check(!std::memcmp(expected.data(), reinterpret_cast<void*>(token.locator), expected.size()),
                  "actual empty cleanup clears8/10/18 to raw positive zero and preserves every other byte");
        }
        Check(host.mallocCalls == beforeMalloc && host.freeCalls == beforeFree,
              "cache qualification and the real empty-store attempt invoke no native allocator or child free");
        if (!scenario)
        {
            callback.action = [&](NativeGameTimingAccess&)
            {
                const auto hash = sdk.GetNameHashKey();
                sdk.RemoveAnimSequences();
                Check(sdk.GetNameHashKey() == hash && !sdk.GetNumSequences() && !sdk.GetSequences() && sdk.IsRunningCompressed() &&
                          sdk.GetAnimBlockID() == (std::numeric_limits<int>::min)() && Hook::writes == beforeWrite + 2,
                      "repeated explicit cleanup is source-defined and preserves known hash, compressed state and signed block ID");
            };
            invoke();
            auto&          memory = static_cast<NativeAnimationSequenceStorageAccess&>(runtime.access);
            std::uintptr_t array = 0, frames = 0;
            callback.action = [&](NativeGameTimingAccess&)
            {
                Check(memory.AllocateAnimationSequences(generation, 2, array) == ClockResult::Ok &&
                          memory.AllocateAnimationKeyFrames(generation, 10, frames) == ClockResult::Ok &&
                          runtime.access.PrepareFreshCompressedAnimationSequence({generation, array, 0}, 1, false, frames) == ClockResult::Ok,
                      "prepare actual linked parent resources for the no-op cache channel");
                sdk.SetSequences(reinterpret_cast<CAnimBlendSequenceSAInterface*>(array));
                std::array<unsigned char, 40> linked;
                std::memcpy(linked.data(), reinterpret_cast<void*>(token.locator), linked.size());
                const auto calls = Hook::writes;
                sdk.RemoveFromUncompressedCache();
                Check(!std::memcmp(linked.data(), reinterpret_cast<void*>(token.locator), linked.size()) &&
                          runtime.access.RemoveEmptyPrivateAnimationHierarchySequences(token) != ClockResult::Ok && Hook::writes == calls &&
                          memory.FreeAnimationSequences(generation, array) != ClockResult::Ok &&
                          memory.FreeAnimationKeyFrames(generation, frames) != ClockResult::Ok,
                      "linked cache0 qualifies the real epoch without clearing bytes or bypassing actual child holds");
                sdk.SetSequences(nullptr);
                sdk.RemoveAnimSequences();
                Check(memory.FreeAnimationKeyFrames(generation, frames) == ClockResult::Ok &&
                          memory.FreeAnimationSequences(generation, array) == ClockResult::Ok &&
                          runtime.access.DestroyPrivateAnimationHierarchy(token) == ClockResult::Ok,
                      "a separate real detach acknowledgement permits no-child cleanup and ordinary child retirement");
            };
            invoke();
            Check(runtime.access.RemovePrivateAnimationHierarchyFromUncompressedCache(token) != ClockResult::Ok &&
                      runtime.access.RemoveEmptyPrivateAnimationHierarchySequences(token) != ClockResult::Ok,
                  "new operations do not turn an old CPU token into callback or stale-lifetime authority");
            WorldGroundOwnerFixture::Retire(service, fixture);
            std::puts("Production native private hierarchy empty operations owner fixture completed");
            std::puts("Production actual private hierarchy empty operations SDK owner fixture completed");
        }
        else
        {
            const auto after = Hook::writes;
            Check(runtime.access.RemoveEmptyPrivateAnimationHierarchySequences(token) != ClockResult::Ok && Hook::writes == after && !service.End(),
                  "stopped or foreign owner cannot replay cleanup or discard its outstanding record");
            saved.Global(NativeBorrowedGcProfile::markerRva, LONG{0});
            fixture.invokeTimer();
            if (scenario == 1)
            {
                Check(service.End() && !service.IsRetained() && !runtime.privateHierarchies.IsRetained() && Hook::writes == after,
                      "accepted count0 survives lost GC acknowledgement and known no-child retirement never rewrites cleanup");
                std::puts("Production private hierarchy empty accepted cleanup recovered");
            }
            else
            {
                Check(!service.End() && service.IsRetained() && runtime.privateHierarchies.IsRetained() && Hook::writes == after,
                      "unknown count outcome or observed foreign cache retains lifetime even after visible fields are cleared");
                std::puts("Production private hierarchy empty unresolved lifetime retained");
            }
        }
        // The pending-command probe allocates only in the success process;
        // empty/cache operations themselves never invoke allocator/free seams.
        if (scenario)
            Check(host.mallocCalls == beforeMalloc && host.freeCalls == beforeFree,
                  "restricted empty/cache operations perform no engine heap or child retirement calls");
        Host::active = nullptr;
        ClockReaderTestHost::image = previousImage;
        saved.Restore();
    }
}
