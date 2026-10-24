/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual owner copy transactions and current frame byte views
 ****************************************************************************/
namespace CustomAnimationSequenceCopyFrameOwnerFixture
{
    namespace Host = NativeCustomAnimSequenceStorageFixture;
    using Service = NativeAnimationSequenceAccess;
    void Run(ClockImageFixture& fixture)
    {
        WorldGroundOwnerFixture::Writes writes{fixture, {}};
        const auto                      admit = [&](const auto& pins)
        {
            for (const auto& pin : pins)
                WorldGroundOwnerFixture::Writes::Apply(&writes, pin.rva, pin.bytes, pin.size, pin.code ? PAGE_EXECUTE_READ : PAGE_READONLY);
        };
        for (const auto& pin : NativeStreamingLoadAllProfile::Code)
            WorldGroundOwnerFixture::Writes::Apply(&writes, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        admit(NativeStartupOwnerProfile::pins);
        admit(NativeStartupWorldProfile::pins);
        for (const auto& pin : NativeUObjectMembershipProfile::pins)
            WorldGroundOwnerFixture::Writes::Apply(&writes, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        admit(NativeGameStateProfile::pins);
        for (const auto& pin : NativeBorrowedGcProfile::pins)
            WorldGroundOwnerFixture::Writes::Apply(&writes, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : NativeStartGameProfile::Code)
            if (pin.rva == 0xab879c || pin.rva == 0xab92f0 || pin.rva == 0x1af4ef0)
                WorldGroundOwnerFixture::Writes::Apply(&writes, pin.rva, pin.bytes.data(), pin.size, PAGE_EXECUTE_READ);
        alignas(std::uintptr_t) std::array<unsigned char, 0x180> root{};
        NativeStreamingLoadAllFixture::Graph graph(fixture.image, reinterpret_cast<std::uintptr_t>(root.data()), &WorldGroundOwnerFixture::Writes::Apply,
                                                   &writes);
        NativePedClothesFixture::Install(fixture.image, &WorldGroundOwnerFixture::Writes::Apply, &writes);
        NativePedClothesFixture::NameState names;
        names.Reset(fixture.image, &WorldGroundOwnerFixture::Writes::Apply, &writes);
        Host::State host;
        host.Reset(fixture.image, &WorldGroundOwnerFixture::Writes::Apply, &writes);
        Host::active = &host;
        for (const auto& pin : NativeAnimationSequenceRowProfile::Code)
            WorldGroundOwnerFixture::Writes::Apply(&writes, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        const auto resetContext = [&]
        {
            graph.Reset();
            writes.Global(NativeGameStateProfile::stateRva, std::uint32_t{9});
            writes.Global(NativeStartGameProfile::ThreadInitializedRva, static_cast<unsigned char>(1));
            writes.Global(NativeStartGameProfile::ThreadIdRva, GetCurrentThreadId());
            writes.Global(NativeBorrowedGcProfile::markerRva, LONG{0});
        };
        resetContext();
        const auto previousImage = ClockReaderTestHost::image;
        ClockReaderTestHost::image = fixture.image;
        WorldGroundOwnerFixture::Callback callback;
        NativeClockService                service;
        ClockResult                       result = ClockResult::NotInitialized;
        Service*                          captured = nullptr;
        callback.action = [&](NativeGameTimingAccess& access) { captured = dynamic_cast<Service*>(&access); };
        BeginOwned(service, fixture, false, true, true, &WorldGroundOwnerFixture::Callback::Invoke, &callback);
        fixture.invokeTimer();
        Check(captured == static_cast<Service*>(&runtime.access),
              "actual natural timing callback exposes the same owner's optional storage service by cross-cast");
        const auto generation = runtime.generation.load();
        const auto invoke = [&]
        {
            Check(service.Renew(generation), "renew only the admitted custom sequence storage callback owner");
            fixture.invokeTimer();
        };
        using Sdk = animation_sequence_fixture::AnimationSequenceReceiver;
        auto&                                 memory = static_cast<NativeAnimationSequenceStorageAccess&>(runtime.access);
        NativeAnimationSequenceReferenceToken sourceToken, targetToken;
        std::uintptr_t                        sourceArray = 0, targetArray = 0, frames = 0, oldFrames = 0;
        callback.action = [&](NativeGameTimingAccess&)
        {
            Check(memory.AllocateAnimationSequences(generation, 2, sourceArray) == ClockResult::Ok &&
                      memory.AllocateAnimationSequences(generation, 1, targetArray) == ClockResult::Ok &&
                      memory.AllocateAnimationKeyFrames(generation, 96, frames) == ClockResult::Ok &&
                      memory.AllocateAnimationKeyFrames(generation, 16, oldFrames) == ClockResult::Ok,
                  "actual runtime issues the two arrays and both independently owned frame allocations");
            sourceToken = {generation, sourceArray, 1};
            targetToken = {generation, targetArray, 0};
        };
        invoke();
        Sdk        sourceFacade(*captured, sourceToken), target(*captured, targetToken), sharedTarget(*captured, targetToken);
        const auto sourceRow = sourceArray + 80;
        callback.action = [&](NativeGameTimingAccess&)
        {
            sourceFacade.SetBoneTag(0x1234);
            target.SetBoneTag(0x4321);
        };
        invoke();
        std::array<unsigned char, 80> expected{};
        for (std::size_t i = 0; i < expected.size(); ++i)
            expected[i] = static_cast<unsigned char>(i ^ 0x5a);
        const auto          pointer = frames + 8;
        const std::uint16_t count = 0;
        std::memcpy(expected.data() + 0x44, &count, sizeof(count));
        std::memcpy(expected.data() + 0x48, &pointer, sizeof(pointer));
        std::memcpy(reinterpret_cast<void*>(sourceRow), expected.data(), expected.size());
        std::memcpy(reinterpret_cast<void*>(targetArray + 0x48), &oldFrames, sizeof(oldFrames));
        ClockSequenceCopyFrameOwnerTest::copies = ClockSequenceCopyFrameOwnerTest::mode = 0;
        bool hashRefused = false;
        callback.action = [&](NativeGameTimingAccess&)
        {
            target.CopySequenceProperties(sourceFacade.GetInterface());
            Check(sharedTarget.GetBoneTag() == 0x1234 && !std::memcmp(reinterpret_cast<void*>(targetArray), expected.data(), expected.size()),
                  "actual SDK whole-row copy publishes shared logical low bits separately from all eighty physical bytes");
            try
            {
                (void)sharedTarget.GetHash();
            }
            catch (const std::runtime_error&)
            {
                hashRefused = true;
            }
            const auto before = ClockSequenceCopyFrameOwnerTest::copies;
            target.CopySequenceProperties(target.GetInterface());
            Check(ClockSequenceCopyFrameOwnerTest::copies == before, "actual facade self-copy follows the same current owner without issuing a store");
            Check(reinterpret_cast<std::uintptr_t>(sharedTarget.GetKeyFrame(4, 16)) == pointer + 64 &&
                      reinterpret_cast<std::uintptr_t>(sharedTarget.GetKeyFrame((std::numeric_limits<std::size_t>::max)(), 0)) == pointer,
                  "actual SDK preserves full byte arithmetic and zero stride independent of frameCount zero");
        };
        invoke();
        Check(hashRefused && ClockSequenceCopyFrameOwnerTest::copies == 1 && host.Contains(frames) && host.Contains(oldFrames) && !host.freeCalls &&
                  !runtime.stopping.load(),
              "copy leaves undefined high hash bits honest and preserves overwritten and aliased allocation journals");
        std::uintptr_t output = 0x12345;
        Check(captured->ReadAnimationSequenceFrame(targetToken, 0, 1, output) != ClockResult::Ok && output == 0x12345,
              "new frame view refuses outside the actual callback without partial output");
        callback.action = [&](NativeGameTimingAccess&)
        {
            output = 0x12345;
            result = captured->ReadAnimationSequenceFrame(targetToken, 5, 16, output);
        };
        invoke();
        Check(result != ClockResult::Ok && output == 0x12345 && !runtime.stopping.load(),
              "span overrun is an ordinary read-only refusal, not a write effect or owner stop");
        callback.action = [&](NativeGameTimingAccess&) { result = captured->CopyAnimationSequenceProperties(targetToken, sourceRow + 1); };
        invoke();
        Check(result != ClockResult::Ok && ClockSequenceCopyFrameOwnerTest::copies == 1 && !runtime.stopping.load(),
              "unaligned raw source locator cannot adopt an issued row or stop a read-only admission refusal");
        auto changedToken = targetToken;
        ClockSequenceCopyFrameOwnerTest::caller = &changedToken;
        ClockSequenceCopyFrameOwnerTest::mode = 3;
        callback.action = [&](NativeGameTimingAccess&) { result = captured->CopyAnimationSequenceProperties(changedToken, sourceRow); };
        invoke();
        Check(result == ClockResult::Ok && !changedToken.ownerGeneration && changedToken.firstTag == 1 &&
                  changedToken.index == (std::numeric_limits<std::size_t>::max)(),
              "runtime captures all target intent before copy callbacks mutate the caller object");
        ClockSequenceCopyFrameOwnerTest::caller = nullptr;
        ClockSequenceCopyFrameOwnerTest::mode = 0;
        const auto beforeEnd = host.freeCalls;
        Check(!service.End() && service.IsRetained() && host.freeCalls == beforeEnd,
              "producer End keeps every original resource journal rather than guessing child ownership from copied pointers");
        fixture.invokeTimer();
        Check(host.freeCalls == beforeEnd + 4 && service.End() && !service.IsRetained(),
              "natural recovery frees each original array/frame record exactly once despite row aliases and overwritten frames");
        for (const unsigned failure : {1u, 2u})
        {
            resetContext();
            NativeClockService                    next;
            NativeAnimationSequenceReferenceToken failedSource, failedTarget;
            std::uintptr_t                        source = 0, destination = 0, firstFrames = 0, secondFrames = 0;
            callback.action = [&](NativeGameTimingAccess& access)
            {
                captured = dynamic_cast<Service*>(&access);
                const auto gen = runtime.generation.load();
                Check(memory.AllocateAnimationSequences(gen, 1, source) == ClockResult::Ok &&
                          memory.AllocateAnimationSequences(gen, 1, destination) == ClockResult::Ok &&
                          memory.AllocateAnimationKeyFrames(gen, 16, firstFrames) == ClockResult::Ok &&
                          memory.AllocateAnimationKeyFrames(gen, 16, secondFrames) == ClockResult::Ok,
                      "prepare original resources for entered copy failure");
                failedSource = {gen, source, 0};
                failedTarget = {gen, destination, 0};
                Sdk from(*captured, failedSource), to(*captured, failedTarget);
                from.SetBoneTag(0x1234);
                to.SetBoneTag(0x4321);
                std::memcpy(reinterpret_cast<void*>(source + 0x48), &firstFrames, sizeof(firstFrames));
                std::memcpy(reinterpret_cast<void*>(destination + 0x48), &secondFrames, sizeof(secondFrames));
            };
            BeginOwned(next, fixture, false, true, true, &WorldGroundOwnerFixture::Callback::Invoke, &callback);
            fixture.invokeTimer();
            const auto before = ClockSequenceCopyFrameOwnerTest::copies;
            const auto freed = host.freeCalls;
            ClockSequenceCopyFrameOwnerTest::mode = failure;
            callback.action = [&](NativeGameTimingAccess&) { result = captured->CopyAnimationSequenceProperties(failedTarget, source); };
            Check(next.Renew(failedTarget.ownerGeneration), "renew only the current copy transaction owner");
            fixture.invokeTimer();
            Check(result != ClockResult::Ok && ClockSequenceCopyFrameOwnerTest::copies == before + 1 &&
                      !std::memcmp(reinterpret_cast<void*>(source), reinterpret_cast<void*>(destination), 80) && runtime.stopping.load() &&
                      runtime.customSequences.IsRetained() && host.freeCalls == freed && host.Contains(firstFrames) && host.Contains(secondFrames),
                  "unknown or accepted post-copy failure retains actual bytes, original aliased frames and owner cleanup debt");
            Check(captured->CopyAnimationSequenceProperties(failedTarget, source) != ClockResult::Ok && ClockSequenceCopyFrameOwnerTest::copies == before + 1 &&
                      !next.End(),
                  "stopped runtime cannot replay the copy or remove its retained cleanup authority");
            resetContext();
            ClockSequenceCopyFrameOwnerTest::mode = 0;
            fixture.invokeTimer();
            Check(host.freeCalls == freed + 4 && next.End() && !next.IsRetained(),
                  "the genuine callback retires all known unpublished resources after the one entered copy attempt");
        }
        Host::active = nullptr;
        ClockReaderTestHost::image = previousImage;
        writes.Restore();
        std::puts("Production native custom animation sequence copy/frame owner fixture completed");
        std::puts("Production actual custom animation sequence copy/frame SDK owner fixture completed");
    }
}
