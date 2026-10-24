/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual callback service owns storage through natural End recovery
 ****************************************************************************/
namespace CustomAnimationSequenceOwnerFixture
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
        NativeAnimationSequenceReferenceToken token;
        std::uintptr_t                        array = 0, frames = 0;
        callback.action = [&](NativeGameTimingAccess&)
        {
            Check(memory.AllocateAnimationSequences(generation, 2, array) == ClockResult::Ok &&
                      memory.AllocateAnimationKeyFrames(generation, 41, frames) == ClockResult::Ok,
                  "actual owner issues row and frame storage");
            token = {generation, array, 0};
        };
        invoke();
        Sdk                           first(*captured, token), second(*captured, token);
        NativeAnimationSequenceValues values{1, 2, 3, 4, true};
        const auto                    sentinel = values;
        Check(captured->ReadAnimationSequence(token, values) != ClockResult::Ok && values.interfaceAddress == sentinel.interfaceAddress,
              "row service refuses between callbacks");
        ClockSequenceRowOwnerTest::writes = ClockSequenceRowOwnerTest::mode = 0;
        bool hashRefused = false;
        callback.action = [&](NativeGameTimingAccess&)
        {
            first.Initialize();
            first.SetBoneTag(0x1234);
            Check(second.GetBoneTag() == 0x1234 && !second.GetKeyFrames() && !second.GetKeyFramesCount() && !second.IsBigChunkForAllSequences() &&
                      reinterpret_cast<std::uintptr_t>(second.GetInterface()) == array,
                  "two actual SDK facades share owner row state and current native views");
            try
            {
                (void)second.GetHash();
            }
            catch (const std::runtime_error&)
            {
                hashRefused = true;
            }
        };
        invoke();
        Check(hashRefused && ClockSequenceRowOwnerTest::writes == 2 && !runtime.stopping.load(),
              "actual eight-method SDK slice refuses undefined high hash bits without inventing a native name hash");
        const auto          pointer = frames + 4;
        const std::uint16_t flags = 8, count = 0xffff, physicalBone = 0x7777;
        std::memcpy(reinterpret_cast<void*>(array + 0x48), &pointer, sizeof(pointer));
        std::memcpy(reinterpret_cast<void*>(array + 0x42), &flags, 2);
        std::memcpy(reinterpret_cast<void*>(array + 0x44), &count, 2);
        std::memcpy(reinterpret_cast<void*>(array + 0x40), &physicalBone, 2);
        callback.action = [&](NativeGameTimingAccess&)
        {
            Check(second.GetBoneTag() == 0x1234 && reinterpret_cast<std::uintptr_t>(second.GetKeyFrames()) == pointer && second.GetKeyFramesCount() == 0xffff &&
                      second.IsBigChunkForAllSequences(),
                  "logical bone getter stays separate from physical bone while issued interior frame, full count and bit08 remain native observations");
        };
        invoke();
        ClockSequenceRowOwnerTest::row = array;
        ClockSequenceRowOwnerTest::afterFrame = +[]() noexcept
        {
            const std::uint16_t changed = 7;
            std::memcpy(reinterpret_cast<void*>(ClockSequenceRowOwnerTest::row + 0x44), &changed, 2);
        };
        values = sentinel;
        callback.action = [&](NativeGameTimingAccess&) { result = captured->ReadAnimationSequence(token, values); };
        invoke();
        Check(result != ClockResult::Ok && values.interfaceAddress == sentinel.interfaceAddress && values.keyFrames == sentinel.keyFrames &&
                  values.frameCount == sentinel.frameCount && values.boneTag == sentinel.boneTag && values.bigChunk == sentinel.bigChunk &&
                  !runtime.stopping.load(),
              "final row bookends reject mutation during actual frame qualification without partial output");
        std::memcpy(reinterpret_cast<void*>(array + 0x48), &array, sizeof(array));
        values = sentinel;
        invoke();
        Check(result != ClockResult::Ok && values.interfaceAddress == sentinel.interfaceAddress && !runtime.stopping.load(),
              "array pointer is not issued frame authority");
        std::memcpy(reinterpret_cast<void*>(array + 0x48), &pointer, sizeof(pointer));
        auto changedToken = token;
        ClockSequenceRowOwnerTest::caller = &changedToken;
        ClockSequenceRowOwnerTest::mode = 3;
        callback.action = [&](NativeGameTimingAccess&) { result = captured->SetAnimationSequenceBoneTag(changedToken, 9); };
        invoke();
        Check(result == ClockResult::Ok && !changedToken.ownerGeneration && changedToken.firstTag == 1 &&
                  changedToken.index == (std::numeric_limits<std::size_t>::max)(),
              "actual owner copies all token intent before write callbacks");
        ClockSequenceRowOwnerTest::mode = 0;
        ClockSequenceRowOwnerTest::caller = nullptr;
        const auto beforeEnd = host.freeCalls;
        Check(!service.End() && service.IsRetained() && host.freeCalls == beforeEnd, "producer End retains row/frame journal without native free");
        fixture.invokeTimer();
        Check(host.freeCalls == beforeEnd + 2 && service.End() && !service.IsRetained(),
              "natural callback drains only known unpublished array/frame storage for partial SDK rows");
        for (const unsigned failure : {1u, 2u})
        {
            resetContext();
            NativeClockService                    next;
            NativeAnimationSequenceReferenceToken failedToken;
            callback.action = [&](NativeGameTimingAccess& access)
            {
                captured = dynamic_cast<Service*>(&access);
                std::uintptr_t tag = 0;
                Check(memory.AllocateAnimationSequences(runtime.generation.load(), 1, tag) == ClockResult::Ok, "prepare real row for entered failure");
                failedToken = {runtime.generation.load(), tag, 0};
            };
            BeginOwned(next, fixture, false, true, true, &WorldGroundOwnerFixture::Callback::Invoke, &callback);
            fixture.invokeTimer();
            const auto before = ClockSequenceRowOwnerTest::writes;
            ClockSequenceRowOwnerTest::mode = failure;
            callback.action = [&](NativeGameTimingAccess&) { result = captured->SetAnimationSequenceBoneTag(failedToken, 12); };
            Check(next.Renew(failedToken.ownerGeneration), "renew one admitted row writer");
            fixture.invokeTimer();
            std::uint16_t bone = 0;
            std::memcpy(&bone, reinterpret_cast<void*>(failedToken.firstTag + 0x40), 2);
            Check(result != ClockResult::Ok && bone == 12 && ClockSequenceRowOwnerTest::writes == before + 1 && runtime.stopping.load() &&
                      runtime.customSequences.IsRetained(),
                  "accepted or unknown entered write keeps actual stores and ordinary journals after lost scope");
            Check(captured->SetAnimationSequenceBoneTag(failedToken, 12) != ClockResult::Ok && ClockSequenceRowOwnerTest::writes == before + 1 && !next.End(),
                  "stopped owner cannot retry or detach its retained journal");
            resetContext();
            ClockSequenceRowOwnerTest::mode = 0;
            fixture.invokeTimer();
            Check(next.End() && !next.IsRetained(), "original callback retires known unpublished mutation debt without snapshot rollback");
        }
        ClockSequenceRowOwnerTest::afterFrame = nullptr;
        Host::active = nullptr;
        ClockReaderTestHost::image = previousImage;
        writes.Restore();
        std::puts("Production native custom animation sequence owner fixture completed");
        std::puts("Production actual custom animation sequence SDK owner fixture completed");
    }
}
