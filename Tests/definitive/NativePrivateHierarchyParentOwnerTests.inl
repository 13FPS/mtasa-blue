/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual private parent acknowledgements own array/frame holds
 ****************************************************************************/
namespace mta::game_de::PrivateAnimationHierarchyDetail
{
    ClockResult WriteSequences(std::uintptr_t, std::uintptr_t, std::uint32_t&) noexcept;
}
namespace ClockPrivateHierarchyParentWriteFixture
{
    unsigned                  writes = 0, mode = 0;
    bool                      ownerLost = false;
    std::uintptr_t            image = 0;
    mta::game_de::ClockResult Write(std::uintptr_t address, std::uintptr_t target, std::uint32_t& exception) noexcept
    {
        ++writes;
        const auto result = mta::game_de::PrivateAnimationHierarchyDetail::WriteSequences(address, target, exception);
        if (result != mta::game_de::ClockResult::Ok)
            return result;
        // Only fault timing is substituted. The real guarded pointer writer
        // completes first; no test manufactures a dependency/detach receipt.
        if (mode == 1)
        {
            exception = EXCEPTION_ACCESS_VIOLATION;
            return mta::game_de::ClockResult::OriginalException;
        }
        if (mode == 2)
            ownerLost = true;
        if (mode == 3)
            *reinterpret_cast<LONG*>(image + mta::game_de::NativeBorrowedGcProfile::markerRva) = 1;
        return result;
    }
}
namespace PrivateHierarchyParentOwnerFixture
{
    namespace Host = NativeCustomAnimSequenceStorageFixture;
    namespace Hook = ClockPrivateHierarchyParentWriteFixture;
    namespace Calls = NativePrivateAnimationHierarchyCalls;
    using Parent = NativePrivateAnimationHierarchyController;
    using Children = NativeCustomAnimSequenceController;
    using Token = NativeAnimationHierarchyReferenceToken;
    unsigned    scenario = 0;
    Token*      mutableIntent = nullptr;
    Parent*     reentrantParent = nullptr;
    Token       reentrantToken;
    ClockResult reentrantResult = ClockResult::Ok;
    ClockResult Owner() noexcept
    {
        const auto result = CheckTimingOwner();
        if (result != ClockResult::Ok)
            return result;
        if (mutableIntent)
        {
            *mutableIntent = {};
            mutableIntent = nullptr;
        }
        if (reentrantParent)
        {
            auto* parent = reentrantParent;
            reentrantParent = nullptr;
            reentrantResult = parent->SetSequences(Hook::image, reentrantToken, 0, &Owner);
        }
        return Hook::ownerLost ? ClockResult::OwnerConflict : ClockResult::Ok;
    }
    void LocalAcknowledgement(ClockImageFixture& fixture, std::uint64_t generation)
    {
        Children       children;
        Parent         parent(&children);
        std::uintptr_t array = 0, frames = 0;
        Token          token;
        Check(children.Allocate(fixture.image, generation, 1, &Owner, array) == ClockResult::Ok &&
                  children.AllocateFrames(fixture.image, generation, 10, &Owner, frames) == ClockResult::Ok &&
                  children.PrepareFreshCompressedSequence(fixture.image, {generation, array, 0}, 1, false, frames, &Owner) == ClockResult::Ok &&
                  parent.Create(fixture.image, generation, &Owner, token) == ClockResult::Ok,
              "local coupled controllers own actual prepared rows and their original frame record");
        auto copied = token;
        mutableIntent = &copied;
        Check(parent.SetSequenceCount(fixture.image, copied, 0xffff, &Owner) == ClockResult::Ok && !copied.locator,
              "parent count captures complete token before owner callback mutates caller storage");
        Calls::Receipt receipt;
        const auto     before = Hook::writes;
        reentrantParent = &parent;
        reentrantToken = token;
        Hook::mode = 2;
        Check(parent.SetSequences(fixture.image, token, array, &Owner, &receipt) != ClockResult::Ok && receipt.entered && receipt.accepted &&
                  !receipt.acknowledged && Hook::writes == before + 1 && *reinterpret_cast<std::uintptr_t*>(token.locator + 8) == array,
              "accepted real attach retains its durable command after post-owner acknowledgement loss");
        Check(!reentrantParent && reentrantResult != ClockResult::Ok,
              "reentrant same-parent cancellation refuses under the real sequence owner gate without orphaning the command");
        Hook::mode = 0;
        Hook::ownerLost = false;
        Check(children.FreeStorage(fixture.image, generation, array, &Owner) != ClockResult::Ok &&
                  children.FreeFrames(fixture.image, generation, frames, &Owner) != ClockResult::Ok,
              "an accepted unacknowledged parent still holds the exact array and frame journals");
        Check(parent.SetSequences(fixture.image, token, array, &Owner, &receipt) == ClockResult::Ok && receipt.acknowledged && !receipt.entered &&
                  Hook::writes == before + 1,
              "same accepted attach resumes the real read-only acknowledgement without a second pointer store");
        Hook::mode = 2;
        Check(parent.SetSequences(fixture.image, token, 0, &Owner, &receipt) != ClockResult::Ok && receipt.entered && receipt.accepted &&
                  !receipt.acknowledged && Hook::writes == before + 2 && !*reinterpret_cast<std::uintptr_t*>(token.locator + 8),
              "a completed null store alone cannot release child ownership after lost acknowledgement");
        Hook::mode = 0;
        Hook::ownerLost = false;
        Check(children.FreeFrames(fixture.image, generation, frames, &Owner) != ClockResult::Ok &&
                  parent.SetSequences(fixture.image, token, 0, &Owner, &receipt) == ClockResult::Ok && receipt.acknowledged && !receipt.entered &&
                  Hook::writes == before + 2,
              "actual same-parent null acknowledgement releases holds without replaying detach");
        Check(children.FreeFrames(fixture.image, generation, frames, &Owner) == ClockResult::Ok &&
                  children.FreeStorage(fixture.image, generation, array, &Owner) == ClockResult::Ok &&
                  parent.Destroy(fixture.image, token, &Owner) == ClockResult::Ok && !parent.IsRetained() && !children.IsRetained(),
              "known private recovery retires each actual resource after the dependency release receipt");
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
        Hook::ownerLost = false;
        WorldGroundOwnerFixture::Callback callback;
        NativeClockService                service;
        callback.action = [](NativeGameTimingAccess&) {};
        BeginOwned(service, fixture, false, true, true, &WorldGroundOwnerFixture::Callback::Invoke, &callback);
        const auto generation = runtime.generation.load();
        const auto invoke = [&]
        {
            Check(service.Renew(generation), "renew only the actual parent callback owner");
            fixture.invokeTimer();
        };
        if (!scenario)
        {
            callback.action = [&](NativeGameTimingAccess&) { LocalAcknowledgement(fixture, generation); };
            invoke();
        }
        auto&          memory = static_cast<NativeAnimationSequenceStorageAccess&>(runtime.access);
        std::uintptr_t array = 0, frameA = 0, frameB = 0, unrelated = 0;
        Token          token, other;
        callback.action = [&](NativeGameTimingAccess& access)
        {
            Check(dynamic_cast<NativeAnimationHierarchyAccess*>(&access) == static_cast<NativeAnimationHierarchyAccess*>(&runtime.access),
                  "hierarchy parent operations belong to the same actual natural callback object");
            Check(memory.AllocateAnimationSequences(generation, 3, array) == ClockResult::Ok &&
                      memory.AllocateAnimationKeyFrames(generation, 10, frameA) == ClockResult::Ok &&
                      memory.AllocateAnimationKeyFrames(generation, 16, frameB) == ClockResult::Ok &&
                      memory.AllocateAnimationKeyFrames(generation, 7, unrelated) == ClockResult::Ok &&
                      runtime.access.PrepareFreshCompressedAnimationSequence({generation, array, 0}, 1, false, frameA) == ClockResult::Ok &&
                      runtime.access.PrepareFreshCompressedAnimationSequence({generation, array, 1}, 1, true, frameB) == ClockResult::Ok &&
                      runtime.access.CreatePrivateAnimationHierarchy(generation, token) == ClockResult::Ok,
                  "issue real three-row capacity with two genuine prepared frame records and an unrelated allocation");
        };
        invoke();
        animation_hierarchy_fixture::AnimationHierarchyReceiver sdk(runtime.access, token);
        const auto                                              pointerWrites = Hook::writes;
        std::array<unsigned char, 40>                           expected;
        std::memcpy(expected.data(), reinterpret_cast<void*>(token.locator), expected.size());
        callback.action = [&](NativeGameTimingAccess&)
        {
            sdk.SetNumSequences(0xffff);
            const std::uint16_t count = 0xffff;
            std::memcpy(expected.data() + 0x10, &count, 2);
            Check(sdk.GetNumSequences() == 0xffff && !sdk.GetSequences() &&
                      !std::memcmp(expected.data(), reinterpret_cast<void*>(token.locator), expected.size()),
                  "actual source count-before-attach changes only WORD10 without clamping to capacity or inventing children");
            if (scenario == 2)
                Hook::mode = 1;
            const auto result = runtime.access.SetPrivateAnimationHierarchySequences(token, array);
            Check((scenario == 2 ? result != ClockResult::Ok : result == ClockResult::Ok) && Hook::writes == pointerWrites + 1,
                  "actual parent pointer transaction uses exactly one guarded real pointer writer");
        };
        invoke();
        Hook::mode = 0;
        if (scenario == 2)
        {
            Check(runtime.stopping.load(), "unknown entered pointer result stops normal runtime work");
        }
        else
        {
            callback.action = [&](NativeGameTimingAccess&)
            {
                std::memcpy(expected.data() + 8, &array, sizeof(array));
                Check(reinterpret_cast<std::uintptr_t>(sdk.GetSequences()) == array && sdk.GetNumSequences() == 0xffff &&
                          !std::memcmp(expected.data(), reinterpret_cast<void*>(token.locator), expected.size()),
                      "acknowledged attach preserves all other raw40 fields and the full source logical count");
                sdk.SetNumSequences(0);
                Check(!sdk.GetNumSequences() && reinterpret_cast<std::uintptr_t>(sdk.GetSequence(2)) == array + 160,
                      "borrowed child rows follow full issued capacity independently of logical count zero");
                std::uintptr_t untouched = 0x123;
                Check(runtime.access.ReadAnimationHierarchySequence(token, 0x10001u, untouched) != ClockResult::Ok && untouched == 0x123,
                      "full DWORD child index cannot narrow to WORD and select another row");
                Check(runtime.access.InitializePrivateAnimationHierarchy(token) != ClockResult::Ok &&
                          memory.FreeAnimationSequences(generation, array) != ClockResult::Ok &&
                          memory.FreeAnimationKeyFrames(generation, frameA) != ClockResult::Ok &&
                          memory.FreeAnimationKeyFrames(generation, frameB) != ClockResult::Ok,
                      "linked Initialize and related frees refuse without clearing genuine child holds");
                Check(memory.FreeAnimationKeyFrames(generation, unrelated) == ClockResult::Ok &&
                          runtime.access.CreatePrivateAnimationHierarchy(generation, other) == ClockResult::Ok &&
                          runtime.access.SetPrivateAnimationHierarchySequences(other, 0) == ClockResult::Ok &&
                          memory.FreeAnimationKeyFrames(generation, frameA) != ClockResult::Ok &&
                          runtime.access.DestroyPrivateAnimationHierarchy(other) == ClockResult::Ok,
                      "an unrelated allocation or another parent's null command cannot release this parent's dependency");
            };
            invoke();
        }
        if (scenario >= 3)
        {
            callback.action = [&](NativeGameTimingAccess&)
            {
                if (scenario == 5)
                    Check(runtime.privateHierarchies.PinPublication(token) == ClockResult::Ok, "irreversibly publish only this actual private parent");
                else
                {
                    const auto           offset = scenario == 3 ? std::uintptr_t{8} : std::uintptr_t{0x20};
                    const std::uintptr_t foreign = array + 1;
                    std::memcpy(reinterpret_cast<void*>(token.locator + offset), &foreign, sizeof(foreign));
                    NativeAnimationHierarchyValues untouched;
                    untouched.interfaceAddress = 7;
                    Check(runtime.access.ReadAnimationHierarchy(token, untouched) != ClockResult::Ok && untouched.interfaceAddress == 7,
                          "observing a foreign linked edge/cache refuses without exposing a partial value");
                    const std::uintptr_t restored = scenario == 3 ? array : 0;
                    std::memcpy(reinterpret_cast<void*>(token.locator + offset), &restored, sizeof(restored));
                }
                Check(runtime.access.SetPrivateAnimationHierarchySequences(token, 0) != ClockResult::Ok &&
                          memory.FreeAnimationSequences(generation, array) != ClockResult::Ok &&
                          memory.FreeAnimationKeyFrames(generation, frameA) != ClockResult::Ok,
                      "foreign/publication evidence cannot be cleared into a private detach acknowledgement");
            };
            invoke();
        }
        if (scenario == 1)
        {
            callback.action = [&](NativeGameTimingAccess&)
            {
                Hook::mode = 3;
                Check(runtime.access.SetPrivateAnimationHierarchySequences(token, 0) != ClockResult::Ok,
                      "actual GC transition after completed null store loses its acknowledgement");
            };
            invoke();
            Hook::mode = 0;
            saved.Global(NativeBorrowedGcProfile::markerRva, LONG{0});
            const auto afterStore = Hook::writes;
            Check(runtime.stopping.load() && !*reinterpret_cast<std::uintptr_t*>(token.locator + 8) && host.Contains(frameA) && host.Contains(frameB),
                  "completed-but-unacknowledged detach retains both original native frame journals");
            Check(!service.End(), "producer End waits for the real natural cleanup callback");
            fixture.invokeTimer();
            Check(service.End() && !service.IsRetained() && Hook::writes == afterStore && !host.Contains(frameA) && !host.Contains(frameB),
                  "real End resumes pending null acknowledgement then frees resources without another pointer store");
            std::puts("Production private hierarchy parent lost acknowledgement recovered");
        }
        else if (scenario >= 2)
        {
            const auto beforeFree = host.freeCalls;
            const auto beforeWrite = Hook::writes;
            Check(!service.End(), "unknown/published parent retains its module and cleanup owner");
            fixture.invokeTimer();
            Check(!service.End() && service.IsRetained() && runtime.privateHierarchies.IsRetained() && runtime.customSequences.IsRetained() &&
                      host.Contains(array - 8) && host.Contains(frameA) && host.Contains(frameB) && Hook::writes == beforeWrite,
                  "actual End retains unresolved linked resources without replaying a pointer store or releasing related frames");
            Check(host.freeCalls == beforeFree, "unresolved parent lifetime keeps the existing runtime cleanup gate closed before child retirement");
            std::puts("Production private hierarchy parent unresolved lifetime retained");
        }
        else
        {
            const auto beforeDetach = Hook::writes;
            callback.action = [&](NativeGameTimingAccess&)
            {
                sdk.SetSequences(nullptr);
                Check(!sdk.GetSequences() && Hook::writes == beforeDetach + 1 && memory.FreeAnimationKeyFrames(generation, frameA) == ClockResult::Ok &&
                          memory.FreeAnimationKeyFrames(generation, frameB) == ClockResult::Ok &&
                          memory.FreeAnimationSequences(generation, array) == ClockResult::Ok,
                      "real same-parent null acknowledgement alone permits related frame and array retirement");
                Check(memory.AllocateAnimationSequences(generation, 1, array) == ClockResult::Ok &&
                          memory.AllocateAnimationKeyFrames(generation, 10, frameA) == ClockResult::Ok &&
                          runtime.access.PrepareFreshCompressedAnimationSequence({generation, array, 0}, 1, false, frameA) == ClockResult::Ok,
                      "prepare a second ordinary private dependency for natural End retirement");
                sdk.SetNumSequences(1);
                sdk.SetSequences(reinterpret_cast<CAnimBlendSequenceSAInterface*>(array));
            };
            invoke();
            const auto beforeEnd = Hook::writes;
            Check(!service.End() && host.Contains(frameA), "End keeps private child records until an engine-thread cleanup callback");
            fixture.invokeTimer();
            Check(service.End() && !service.IsRetained() && !runtime.privateHierarchies.IsRetained() && !runtime.customSequences.IsRetained() &&
                      !host.Contains(frameA) && !host.Contains(array - 8) && Hook::writes == beforeEnd + 1,
                  "natural End performs one real private detach acknowledgement before retiring all linked records");
            std::puts("Production native private hierarchy parent owner fixture completed");
            std::puts("Production actual private hierarchy parent SDK owner fixture completed");
        }
        Host::active = nullptr;
        ClockReaderTestHost::image = previousImage;
        saved.Restore();
    }
}
