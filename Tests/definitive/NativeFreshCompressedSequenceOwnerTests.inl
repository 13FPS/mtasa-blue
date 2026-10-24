/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Explicit private binding keeps the general SDK setter pure
 ****************************************************************************/
namespace FreshCompressedSequenceOwnerFixture
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
        for (const auto& pin : NativeAnimationSequenceNameProfile::Code)
            WorldGroundOwnerFixture::Writes::Apply(&writes, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : NativeAnimationSequenceFrameBindProfile::Code)
            WorldGroundOwnerFixture::Writes::Apply(&writes, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        using Sdk = animation_sequence_fixture::AnimationSequenceReceiver;
        auto&                                 memory = static_cast<NativeAnimationSequenceStorageAccess&>(runtime.access);
        constexpr std::size_t                 count = 65537;
        constexpr std::uint32_t               bytes = static_cast<std::uint32_t>(count * 16);
        std::uintptr_t                        array = 0, frames = 0, undersizedFrames = 0;
        NativeAnimationSequenceReferenceToken token, freshToken;
        callback.action = [&](NativeGameTimingAccess&)
        {
            Check(memory.AllocateAnimationSequences(generation, 2, array) == ClockResult::Ok &&
                      memory.AllocateAnimationKeyFrames(generation, bytes, frames) == ClockResult::Ok &&
                      memory.AllocateAnimationKeyFrames(generation, 16, undersizedFrames) == ClockResult::Ok,
                  "issue actual original array and frame resources");
            token = {generation, array, 0};
            freshToken = {generation, array, 1};
            Sdk sdk(*captured, token);
            sdk.SetName("Frames");
            sdk.SetBoneTag(7);
        };
        invoke();
        Sdk                           sdk(*captured, token), fresh(*captured, freshToken);
        CKeyGenDE                     keygen;
        const auto                    hash = (keygen.GetKey("FRAMES") & 0xffff0000u) | 7;
        std::array<unsigned char, 80> expected{};
        std::memcpy(expected.data(), reinterpret_cast<void*>(array), expected.size());
        const std::uint16_t flags = 0x17, narrowed = 1;
        std::memcpy(expected.data() + 0x42, &flags, 2);
        std::memcpy(expected.data() + 0x44, &narrowed, 2);
        std::memcpy(expected.data() + 0x48, &frames, sizeof(frames));
        ClockFreshCompressedSequenceOwnerTest::writes = ClockFreshCompressedSequenceOwnerTest::mode = 0;
        callback.action = [&](NativeGameTimingAccess&)
        {
            result = captured->PrepareFreshCompressedAnimationSequence(token, count, true, frames);
            Check(result == ClockResult::Ok && !std::memcmp(reinterpret_cast<void*>(array), expected.data(), expected.size()) && sdk.GetHash() == hash &&
                      sdk.GetBoneTag() == 7 && sdk.GetKeyFramesCount() == 1 && reinterpret_cast<std::uintptr_t>(sdk.GetKeyFrames()) == frames &&
                      !sdk.IsBigChunkForAllSequences() && reinterpret_cast<std::uintptr_t>(sdk.GetKeyFrame(65536, 16)) == frames + std::uintptr_t{65536} * 16,
                  "explicit service checks full span before WORD storage while real getters preserve logical state and source byte views");
        };
        invoke();
        Check(ClockFreshCompressedSequenceOwnerTest::writes == 1 && !host.freeCalls && host.Contains(frames) && host.Contains(undersizedFrames) &&
                  *reinterpret_cast<unsigned char*>(frames) == 0xa5 && *reinterpret_cast<unsigned char*>(frames + bytes - 1) == 0xa5,
              "private binding writes no payload, infers no shared08 ownership and retires no original resource");
        callback.action = [&](NativeGameTimingAccess&) { result = captured->PrepareFreshCompressedAnimationSequence(freshToken, 1, false, frames + 4); };
        invoke();
        Check(result != ClockResult::Ok && ClockFreshCompressedSequenceOwnerTest::writes == 1 && !runtime.stopping.load(),
              "fresh row still requires exact issued frame base");
        callback.action = [&](NativeGameTimingAccess&)
        { result = captured->PrepareFreshCompressedAnimationSequence(freshToken, count, true, undersizedFrames); };
        invoke();
        Check(result != ClockResult::Ok && ClockFreshCompressedSequenceOwnerTest::writes == 1 && !runtime.stopping.load(),
              "full count underallocation refuses before native entry");
        auto changed = freshToken;
        ClockFreshCompressedSequenceOwnerTest::caller = &changed;
        ClockFreshCompressedSequenceOwnerTest::mode = 3;
        callback.action = [&](NativeGameTimingAccess&) { result = captured->PrepareFreshCompressedAnimationSequence(changed, 1, false, undersizedFrames); };
        invoke();
        Check(result == ClockResult::Ok && !changed.ownerGeneration && changed.firstTag == 1 && changed.index == (std::numeric_limits<std::size_t>::max)(),
              "actual service copies token intent before synchronous bind callbacks mutate its caller");
        ClockFreshCompressedSequenceOwnerTest::caller = nullptr;
        ClockFreshCompressedSequenceOwnerTest::mode = 0;
        bool undefinedHash = false;
        callback.action = [&](NativeGameTimingAccess&)
        {
            Check(fresh.GetKeyFramesCount() == 1 && reinterpret_cast<std::uintptr_t>(fresh.GetKeyFrames()) == undersizedFrames &&
                      fresh.GetBoneTag() == 0xffff && !fresh.IsBigChunkForAllSequences(),
                  "private child binding preserves existing logical metadata and raw frame views");
            try
            {
                (void)fresh.GetHash();
            }
            catch (const std::runtime_error&)
            {
                undefinedHash = true;
            }
            result = captured->PrepareFreshCompressedAnimationSequence(token, count, true, frames);
        };
        invoke();
        Check(undefinedHash && result != ClockResult::Ok && ClockFreshCompressedSequenceOwnerTest::writes == 2 && !runtime.stopping.load(),
              "builder preserves unknown hash and cannot replay an already linked row");
        const auto before = ClockFreshCompressedSequenceOwnerTest::writes;
        Check(captured->PrepareFreshCompressedAnimationSequence(token, 1, false, frames) != ClockResult::Ok &&
                  ClockFreshCompressedSequenceOwnerTest::writes == before,
              "retained CPU service provides no frame-link authority between callbacks");
        const auto freed = host.freeCalls;
        Check(!service.End() && service.IsRetained() && host.freeCalls == freed, "producer End retains all original private resources");
        fixture.invokeTimer();
        Check(host.freeCalls == freed + 3 && service.End() && !service.IsRetained(), "natural callback retires each private array/frame record once");
        for (const unsigned failure : {1u, 2u})
        {
            resetContext();
            NativeClockService                    next;
            NativeAnimationSequenceReferenceToken failed;
            std::uintptr_t                        tag = 0, buffer = 0;
            callback.action = [&](NativeGameTimingAccess& access)
            {
                captured = dynamic_cast<Service*>(&access);
                const auto gen = runtime.generation.load();
                Check(
                    memory.AllocateAnimationSequences(gen, 1, tag) == ClockResult::Ok && memory.AllocateAnimationKeyFrames(gen, 10, buffer) == ClockResult::Ok,
                    "prepare original private resources for one entered child bind");
                failed = {gen, tag, 0};
            };
            BeginOwned(next, fixture, false, true, true, &WorldGroundOwnerFixture::Callback::Invoke, &callback);
            fixture.invokeTimer();
            const auto writesBefore = ClockFreshCompressedSequenceOwnerTest::writes;
            const auto freeBefore = host.freeCalls;
            ClockFreshCompressedSequenceOwnerTest::mode = failure;
            callback.action = [&](NativeGameTimingAccess&) { result = captured->PrepareFreshCompressedAnimationSequence(failed, 1, false, buffer); };
            Check(next.Renew(failed.ownerGeneration), "renew only the current private binding owner");
            fixture.invokeTimer();
            Check(result != ClockResult::Ok && ClockFreshCompressedSequenceOwnerTest::writes == writesBefore + 1 &&
                      *reinterpret_cast<std::uintptr_t*>(tag + 0x48) == buffer && *reinterpret_cast<std::uint16_t*>(tag + 0x42) == 5 &&
                      runtime.stopping.load() && runtime.customSequences.IsRetained() && host.Contains(buffer) && host.freeCalls == freeBefore,
                  "entered failure retains actual links and original allocation journals after Stop");
            Check(captured->PrepareFreshCompressedAnimationSequence(failed, 1, false, buffer) != ClockResult::Ok &&
                      ClockFreshCompressedSequenceOwnerTest::writes == writesBefore + 1 && !next.End(),
                  "stopped owner cannot replay or rewind its private bind");
            resetContext();
            ClockFreshCompressedSequenceOwnerTest::mode = 0;
            fixture.invokeTimer();
            Check(host.freeCalls == freeBefore + 2 && next.End() && !next.IsRetained(), "known unpublished bind debt retires once on its genuine callback");
        }
        Host::active = nullptr;
        ClockReaderTestHost::image = previousImage;
        writes.Restore();
        std::puts("Production native fresh compressed sequence owner fixture completed");
        std::puts("Production actual fresh compressed sequence service owner fixture completed");
    }
}
