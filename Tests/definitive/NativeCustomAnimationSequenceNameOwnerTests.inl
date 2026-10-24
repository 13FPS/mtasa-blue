/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual bounded SetName through one retained native storage owner
 ****************************************************************************/
namespace CustomAnimationSequenceNameOwnerFixture
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
        using Sdk = animation_sequence_fixture::AnimationSequenceReceiver;
        auto&                                 memory = static_cast<NativeAnimationSequenceStorageAccess&>(runtime.access);
        NativeAnimationSequenceReferenceToken token;
        std::uintptr_t                        array = 0, frames = 0;
        callback.action = [&](NativeGameTimingAccess&)
        {
            Check(memory.AllocateAnimationSequences(generation, 2, array) == ClockResult::Ok &&
                      memory.AllocateAnimationKeyFrames(generation, 41, frames) == ClockResult::Ok,
                  "actual callback issues the original array and frame resources for a name transaction");
            token = {generation, array, 1};
        };
        invoke();
        const auto          row = array + 80;
        const auto          pointer = frames + 4;
        const std::uint16_t physicalBone = 0x7b7b, flags = 0x2208, count = 7;
        std::memcpy(reinterpret_cast<void*>(row + 0x40), &physicalBone, 2);
        std::memcpy(reinterpret_cast<void*>(row + 0x42), &flags, 2);
        std::memcpy(reinterpret_cast<void*>(row + 0x44), &count, 2);
        std::memcpy(reinterpret_cast<void*>(row + 0x48), &pointer, sizeof(pointer));
        Sdk        first(*captured, token), second(*captured, token);
        CKeyGenDE  keygen;
        const auto firstHash = keygen.GetKey("MIX\xe9");
        ClockSequenceNameOwnerTest::writes = ClockSequenceNameOwnerTest::mode = 0;
        const char                    spelling[] = {'m', 'i', 'X', static_cast<char>(0xe9), 0};
        std::array<unsigned char, 80> expected{};
        std::memcpy(expected.data(), reinterpret_cast<void*>(row), expected.size());
        std::memcpy(expected.data(), spelling, sizeof(spelling));
        callback.action = [&](NativeGameTimingAccess&)
        {
            first.SetName(spelling);
            Check(second.GetHash() == firstHash && second.GetBoneTag() == static_cast<std::uint16_t>(firstHash) &&
                      !std::memcmp(reinterpret_cast<void*>(row), expected.data(), expected.size()),
                  "actual SetName preserves native spelling/high bytes/tail while publishing shared complete classic ASCII hash independently of inactive "
                  "physical bone");
            Check(reinterpret_cast<std::uintptr_t>(second.GetKeyFrames()) == pointer && second.GetKeyFramesCount() == 7 && second.IsBigChunkForAllSequences(),
                  "name assignment preserves current frame and flag observations");
        };
        invoke();
        Check(ClockSequenceNameOwnerTest::writes == 1 && !host.freeCalls && host.Contains(frames) && !runtime.stopping.load(),
              "one name transaction neither retires frames nor invents engine cache work");
        const auto secondHash = keygen.GetKey("NEXT");
        callback.action = [&](NativeGameTimingAccess&)
        {
            first.SetBoneTag(0x1234);
            Check(second.GetHash() == ((firstHash & 0xffff0000u) | 0x1234), "existing bone setter preserves the complete logical name upper bits");
            first.SetName("Next");
            std::uint16_t bone = 0, physicalFlags = 0;
            std::memcpy(&bone, reinterpret_cast<void*>(row + 0x40), 2);
            std::memcpy(&physicalFlags, reinterpret_cast<void*>(row + 0x42), 2);
            Check(second.GetHash() == secondHash && second.GetBoneTag() == static_cast<std::uint16_t>(secondHash) &&
                      bone == static_cast<std::uint16_t>(secondHash) && physicalFlags == (flags | 0x10),
                  "SetName after bone selection replaces full logical union while preserving selector and projecting only its low WORD to physical bone");
        };
        invoke();
        NativeAnimationSequenceName name{};
        Check(NativeAnimationSequenceNamePolicy::Copy("Capture", name), "prepare exact bounded structured name intent");
        auto mutableToken = token;
        ClockSequenceNameOwnerTest::caller = &mutableToken;
        ClockSequenceNameOwnerTest::callerName = &name;
        ClockSequenceNameOwnerTest::mode = 3;
        callback.action = [&](NativeGameTimingAccess&) { result = captured->SetAnimationSequenceName(mutableToken, name); };
        invoke();
        Check(result == ClockResult::Ok && !mutableToken.ownerGeneration && mutableToken.firstTag == 1 &&
                  mutableToken.index == (std::numeric_limits<std::size_t>::max)() && name[0] == 'X' && !std::memcmp(reinterpret_cast<void*>(row), "Capture", 8),
              "real owner keeps copied token/name/hash intent through synchronous caller mutation");
        ClockSequenceNameOwnerTest::caller = nullptr;
        ClockSequenceNameOwnerTest::callerName = nullptr;
        ClockSequenceNameOwnerTest::mode = 0;
        NativeAnimationSequenceName outside{};
        NativeAnimationSequenceNamePolicy::Copy("Outside", outside);
        const auto before = ClockSequenceNameOwnerTest::writes;
        Check(captured->SetAnimationSequenceName(token, outside) != ClockResult::Ok && ClockSequenceNameOwnerTest::writes == before,
              "native name authority remains callback-bound despite a retained facade and valid raw spelling");
        const auto beforeEnd = host.freeCalls;
        Check(!service.End() && service.IsRetained() && host.freeCalls == beforeEnd, "producer End retains original name/frame resource authority");
        fixture.invokeTimer();
        Check(host.freeCalls == beforeEnd + 2 && service.End() && !service.IsRetained(),
              "natural cleanup retires original records once without name-specific retirement");
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
                    memory.AllocateAnimationSequences(gen, 1, tag) == ClockResult::Ok && memory.AllocateAnimationKeyFrames(gen, 16, buffer) == ClockResult::Ok,
                    "prepare original storage for one entered name failure");
                failed = {gen, tag, 0};
                Sdk sdk(*captured, failed);
                sdk.SetName("Before");
                std::memcpy(reinterpret_cast<void*>(tag + 0x48), &buffer, sizeof(buffer));
            };
            BeginOwned(next, fixture, false, true, true, &WorldGroundOwnerFixture::Callback::Invoke, &callback);
            fixture.invokeTimer();
            NativeAnimationSequenceName nextName{};
            NativeAnimationSequenceNamePolicy::Copy("After", nextName);
            const auto writeBefore = ClockSequenceNameOwnerTest::writes;
            const auto freed = host.freeCalls;
            ClockSequenceNameOwnerTest::mode = failure;
            callback.action = [&](NativeGameTimingAccess&) { result = captured->SetAnimationSequenceName(failed, nextName); };
            Check(next.Renew(failed.ownerGeneration), "renew only the admitted name mutation owner");
            fixture.invokeTimer();
            Check(result != ClockResult::Ok && ClockSequenceNameOwnerTest::writes == writeBefore + 1 &&
                      !std::memcmp(reinterpret_cast<void*>(tag), "After", 6) && runtime.stopping.load() && runtime.customSequences.IsRetained() &&
                      host.freeCalls == freed && host.Contains(buffer),
                  "unknown or accepted post-name failure keeps actual stores and all original allocation journals after owner Stop");
            Check(
                captured->SetAnimationSequenceName(failed, nextName) != ClockResult::Ok && ClockSequenceNameOwnerTest::writes == writeBefore + 1 && !next.End(),
                "stopped owner cannot replay or rewind the name transaction or drop cleanup authority");
            resetContext();
            ClockSequenceNameOwnerTest::mode = 0;
            fixture.invokeTimer();
            Check(host.freeCalls == freed + 2 && next.End() && !next.IsRetained(), "genuine callback retires known unpublished name fault resources once");
        }
        Host::active = nullptr;
        ClockReaderTestHost::image = previousImage;
        writes.Restore();
        std::puts("Production native custom animation sequence name owner fixture completed");
        std::puts("Production actual custom animation sequence name SDK owner fixture completed");
    }
}
