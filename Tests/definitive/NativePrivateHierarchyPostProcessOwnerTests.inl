/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Real saved quaternion/time bodies consume held typed frame authority
 ****************************************************************************/
namespace mta::game_de::PrivateAnimationHierarchyDetail
{
    ClockResult NativePostProcessCall(void (*)(std::uintptr_t), std::uintptr_t, std::uint32_t&) noexcept;
}
namespace ClockPrivateHierarchyPostProcessFixture
{
    unsigned                                             calls = 0, mode = 0;
    std::uintptr_t                                       image = 0;
    bool                                                 reenter = false;
    mta::game_de::NativeAnimationHierarchyReferenceToken parentToken;
    mta::game_de::NativeAnimationSequenceReferenceToken  rowToken;
    mta::game_de::ClockResult                            parentReentry = mta::game_de::ClockResult::Ok, rowReentry = mta::game_de::ClockResult::Ok;
    mta::game_de::ClockResult                            Invoke(void (*function)(std::uintptr_t), std::uintptr_t hierarchy, std::uint32_t& exception) noexcept
    {
        ++calls;
        if (reenter)
        {
            reenter = false;
            mta::game_de::NativeAnimationHierarchyValues h;
            mta::game_de::NativeAnimationSequenceValues  row;
            parentReentry = runtime.privateHierarchies.Read(image, parentToken, &CheckTimingOwner, h);
            rowReentry = runtime.customSequences.ReadSequence(image, rowToken, &CheckTimingOwner, row);
        }
        // Execute the actual whole saved body at its admitted MEM_IMAGE RVA.
        // Only intentional failure cases lose acknowledgement after it returns.
        const auto result = mta::game_de::PrivateAnimationHierarchyDetail::NativePostProcessCall(function, hierarchy, exception);
        if (result != mta::game_de::ClockResult::Ok)
            return result;
        if (mode)
            *reinterpret_cast<LONG*>(image + mta::game_de::NativeBorrowedGcProfile::markerRva) = 1;
        if (mode == 1)
        {
            exception = EXCEPTION_ACCESS_VIOLATION;
            return mta::game_de::ClockResult::OriginalException;
        }
        return result;
    }
}
namespace PrivateHierarchyPostProcessOwnerFixture
{
    namespace Hook = ClockPrivateHierarchyPostProcessFixture;
    namespace Host = NativeCustomAnimSequenceStorageFixture;
    namespace Profile = NativePrivateAnimationHierarchyPostProcessProfile;
    using Token = NativeAnimationHierarchyReferenceToken;
    using Sdk = animation_hierarchy_fixture::AnimationHierarchyReceiver;
    unsigned scenario = 0;
    struct RootFrame
    {
        float q[4];
        float time;
        float position[3];
    };
    struct ChildFrame
    {
        float q[4];
        float time;
    };
    static_assert(sizeof(RootFrame) == 32 && sizeof(ChildFrame) == 20);
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
        for (const auto& pin : Profile::Code)
            WorldGroundOwnerFixture::Writes::Apply(&saved, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        WorldGroundOwnerFixture::Writes::Apply(&saved, Profile::SignMask.rva, Profile::SignMask.bytes, Profile::SignMask.size, PAGE_READONLY);
        graph.Reset();
        saved.Global(NativeGameStateProfile::stateRva, std::uint32_t{9});
        saved.Global(NativeStartGameProfile::ThreadInitializedRva, static_cast<unsigned char>(1));
        saved.Global(NativeStartGameProfile::ThreadIdRva, GetCurrentThreadId());
        saved.Global(NativeBorrowedGcProfile::markerRva, LONG{0});
        const auto previousImage = ClockReaderTestHost::image;
        ClockReaderTestHost::image = fixture.image;
        Hook::image = fixture.image;
        Hook::calls = Hook::mode = 0;
        WorldGroundOwnerFixture::Callback callback;
        NativeClockService                service;
        callback.action = [](NativeGameTimingAccess&) {};
        BeginOwned(service, fixture, false, true, true, &WorldGroundOwnerFixture::Callback::Invoke, &callback);
        const auto generation = runtime.generation.load();
        const auto invoke = [&]
        {
            Check(service.Renew(generation), "renew only the actual hierarchy postprocessor callback owner");
            fixture.invokeTimer();
        };
        auto&          memory = static_cast<NativeAnimationSequenceStorageAccess&>(runtime.access);
        Token          token;
        std::uintptr_t array = 0, rootFrames = 0, childFrames = 0;
        callback.action = [&](NativeGameTimingAccess& access)
        {
            Check(dynamic_cast<NativeAnimationHierarchyAccess*>(&access) == static_cast<NativeAnimationHierarchyAccess*>(&runtime.access) &&
                      runtime.access.CreatePrivateAnimationHierarchy(generation, token) == ClockResult::Ok &&
                      memory.AllocateAnimationSequences(generation, 2, array) == ClockResult::Ok &&
                      memory.AllocateAnimationKeyFrames(generation, 64, rootFrames) == ClockResult::Ok &&
                      memory.AllocateAnimationKeyFrames(generation, 40, childFrames) == ClockResult::Ok,
                  "actual callback co-owns CPU hierarchy, array and original native20/32 frame records");
        };
        invoke();
        Sdk                                                   sdk(runtime.access, token);
        animation_sequence_fixture::AnimationSequenceReceiver row(runtime.access, {generation, array, 0});
        if (!scenario)
        {
            // Separate independent probes with real natural callback leases;
            // expiry must never stand in for a typed descriptor refusal.
            callback.action = [&](NativeGameTimingAccess&)
            {
                // Genuine zero-count bodies need no child array or payload.
                sdk.RemoveQuaternionFlips();
                sdk.CalculateTotalTime();
                Check(!sdk.GetTotalTime() && Hook::calls == 2, "saved empty hierarchy bodies execute their genuine zero-count paths");
            };
            invoke();
            callback.action = [&](NativeGameTimingAccess&)
            {
                Check(runtime.access.PrepareFreshCompressedAnimationSequence({generation, array, 0}, 2, true, rootFrames) == ClockResult::Ok,
                      "prepare an actual Compressed descriptor for typed payload refusal");
                sdk.SetNumSequences(1);
                sdk.SetSequences(reinterpret_cast<CAnimBlendSequenceSAInterface*>(array));
                sdk.SetRunningCompressed(false);
            };
            invoke();
            const auto before = Hook::calls;
            callback.action = [&](NativeGameTimingAccess&)
            {
                Check(runtime.access.RemovePrivateAnimationHierarchyQuaternionFlips(token) == ClockResult::NotInitialized &&
                          runtime.access.CalculatePrivateAnimationHierarchyTotalTime(token) == ClockResult::NotInitialized && Hook::calls == before &&
                          !runtime.stopping.load(),
                      "hierarchy compressedfalse cannot turn a genuine Compressed row descriptor into float payload authority");
            };
            invoke();
            callback.action = [&](NativeGameTimingAccess&)
            {
                sdk.SetSequences(nullptr);
                row.Initialize();
            };
            invoke();
            callback.action = [&](NativeGameTimingAccess&)
            {
                // Raw04clear bytes alone are not an accepted encoding record.
                const std::uint16_t flags = 3, count = 2;
                std::memcpy(reinterpret_cast<void*>(array + 0x42), &flags, 2);
                std::memcpy(reinterpret_cast<void*>(array + 0x44), &count, 2);
                std::memcpy(reinterpret_cast<void*>(array + 0x48), &rootFrames, sizeof(rootFrames));
                sdk.SetSequences(reinterpret_cast<CAnimBlendSequenceSAInterface*>(array));
            };
            invoke();
            callback.action = [&](NativeGameTimingAccess&)
            {
                Check(runtime.access.RemovePrivateAnimationHierarchyQuaternionFlips(token) == ClockResult::NotInitialized &&
                          runtime.access.CalculatePrivateAnimationHierarchyTotalTime(token) == ClockResult::NotInitialized && Hook::calls == before &&
                          !runtime.stopping.load(),
                      "private raw float-shaped metadata with Unknown descriptor refuses both payload operations before entry");
            };
            invoke();
            callback.action = [&](NativeGameTimingAccess&)
            {
                sdk.SetSequences(nullptr);
                row.Initialize();
            };
            invoke();
        }
        RootFrame  roots[2] = {{{0.5f, 0.5f, 0.5f, 0.5f}, 1.5f, {1, 2, 3}}, {{-0.5f, -0.5f, -0.5f, -0.5f}, 3.0f, {4, 5, 6}}};
        ChildFrame children[2] = {{{0.5f, 0.5f, 0.5f, 0.5f}, 1.0f}, {{-0.5f, -0.5f, -0.5f, -0.5f}, 4.0f}};
        std::memcpy(reinterpret_cast<void*>(rootFrames), roots, sizeof(roots));
        std::memcpy(reinterpret_cast<void*>(childFrames), children, sizeof(children));
        callback.action = [&](NativeGameTimingAccess&)
        {
            Check(runtime.access.PrepareFreshUncompressedAnimationSequence({generation, array, 0}, 2, true, rootFrames) == ClockResult::Ok &&
                      runtime.access.PrepareFreshUncompressedAnimationSequence({generation, array, 1}, 2, false, childFrames) == ClockResult::Ok,
                  "bind already-native finite float records through actual accepted Uncompressed descriptors");
            sdk.SetName("Actual-Postprocess");
            sdk.SetNumSequences(2);
            sdk.SetSequences(reinterpret_cast<CAnimBlendSequenceSAInterface*>(array));
        };
        invoke();
        const auto mallocBefore = host.mallocCalls, freeBefore = host.freeCalls;
        const auto callsBefore = Hook::calls;
        if (!scenario)
        {
            callback.action = [&](NativeGameTimingAccess&)
            {
                sdk.SetNumSequences(0x8000);
                Check(runtime.access.RemovePrivateAnimationHierarchyQuaternionFlips(token) != ClockResult::Ok &&
                          runtime.access.CalculatePrivateAnimationHierarchyTotalTime(token) != ClockResult::Ok,
                      "negative signed native hierarchy counts refuse instead of walking a clamped unsigned range");
                sdk.SetNumSequences(3);
                Check(runtime.access.RemovePrivateAnimationHierarchyQuaternionFlips(token) != ClockResult::Ok &&
                          runtime.access.CalculatePrivateAnimationHierarchyTotalTime(token) != ClockResult::Ok && Hook::calls == callsBefore,
                      "positive native logical count cannot exceed the actual held array capacity");
                sdk.SetNumSequences(2);
            };
            invoke();
            std::array<unsigned char, 160> rows;
            std::memcpy(rows.data(), reinterpret_cast<void*>(array), rows.size());
            std::array<unsigned char, 40> hierarchy;
            std::memcpy(hierarchy.data(), reinterpret_cast<void*>(token.locator), hierarchy.size());
            Hook::parentToken = token;
            Hook::rowToken = {generation, array, 0};
            Hook::reenter = true;
            callback.action = [&](NativeGameTimingAccess&)
            {
                sdk.RemoveQuaternionFlips();
                Check(Hook::parentReentry == ClockResult::Busy && Hook::rowReentry == ClockResult::Busy,
                      "real invocation holds both actual owner locks without recursive borrowing or native replay");
                sdk.CalculateTotalTime();
                Check(sdk.GetTotalTime() == 4.0f && Hook::calls == callsBefore + 2,
                      "actual saved time body returns total absolute duration after quaternion execution");
            };
            invoke();
            roots[1].q[0] = roots[1].q[1] = roots[1].q[2] = roots[1].q[3] = 0.5f;
            children[1].q[0] = children[1].q[1] = children[1].q[2] = children[1].q[3] = 0.5f;
            roots[1].time = 1.5f;
            children[1].time = 3.0f;
            const float total = 4.0f;
            std::memcpy(hierarchy.data() + 0x18, &total, 4);
            Check(!std::memcmp(reinterpret_cast<void*>(rootFrames), roots, sizeof(roots)) &&
                      !std::memcmp(reinterpret_cast<void*>(childFrames), children, sizeof(children)) &&
                      !std::memcmp(reinterpret_cast<void*>(array), rows.data(), rows.size()) &&
                      !std::memcmp(reinterpret_cast<void*>(token.locator), hierarchy.data(), hierarchy.size()),
                  "real MEM_IMAGE bodies repair quaternion signs and convert times in place while preserving translation, rows and other hierarchy fields");
            Check(runtime.access.RemovePrivateAnimationHierarchyQuaternionFlips(token) != ClockResult::Ok &&
                      runtime.access.CalculatePrivateAnimationHierarchyTotalTime(token) != ClockResult::Ok && Hook::calls == callsBefore + 2,
                  "retained CPU facades cannot execute either body between callbacks");
        }
        else
        {
            Hook::mode = scenario;
            callback.action = [&](NativeGameTimingAccess&)
            {
                Check(runtime.access.CalculatePrivateAnimationHierarchyTotalTime(token) != ClockResult::Ok,
                      "completed real time execution reports unknown or lost owner acknowledgement");
            };
            invoke();
            Hook::mode = 0;
            Check(runtime.stopping.load() && runtime.privateHierarchies.IsRetained() && runtime.customSequences.IsRetained() &&
                      Hook::calls == callsBefore + 1 && *reinterpret_cast<float*>(token.locator + 0x18) == 4.0f &&
                      *reinterpret_cast<float*>(rootFrames + 32 + 0x10) == 1.5f && *reinterpret_cast<float*>(childFrames + 20 + 0x10) == 3.0f &&
                      host.freeCalls == freeBefore,
                  "entered time effects remain visible and all actual parent/frame journals survive Stop without inverse writes");
            Check(runtime.access.CalculatePrivateAnimationHierarchyTotalTime(token) != ClockResult::Ok && Hook::calls == callsBefore + 1,
                  "lost acknowledgement cannot run the non-idempotent time conversion a second time");
            saved.Global(NativeBorrowedGcProfile::markerRva, LONG{0});
        }
        const auto beforeEnd = Hook::calls;
        Check(host.mallocCalls == mallocBefore && host.freeCalls == freeBefore && !service.End(),
              "neither postprocessor allocates/frees resources and End waits for actual callback retirement");
        fixture.invokeTimer();
        Check(service.End() && !service.IsRetained() && !runtime.privateHierarchies.IsRetained() && !runtime.customSequences.IsRetained() &&
                  Hook::calls == beforeEnd && host.freeCalls == freeBefore + 3,
              "actual private detach/ACK cleanup retires original resources once without replaying payload mutation");
        Host::active = nullptr;
        ClockReaderTestHost::image = previousImage;
        saved.Restore();
        if (!scenario)
        {
            std::puts("Production native private hierarchy postprocess owner fixture completed");
            std::puts("Production actual private hierarchy postprocess SDK owner fixture completed");
        }
        else
            std::puts("Production private hierarchy postprocess entered failure recovered without replay");
    }
}
