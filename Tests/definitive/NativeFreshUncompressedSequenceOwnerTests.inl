/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Explicit native20/32 construction stays on the real runtime owner
 ****************************************************************************/
namespace FreshUncompressedSequenceOwnerFixture
{
    namespace Host = NativeCustomAnimSequenceStorageFixture;
    namespace Hook = ClockFreshCompressedSequenceOwnerTest;
    using Service = NativeAnimationSequenceAccess;
    using Token = NativeAnimationSequenceReferenceToken;
    using Sdk = animation_sequence_fixture::AnimationSequenceReceiver;
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
        const auto resetContext = [&]
        {
            graph.Reset();
            saved.Global(NativeGameStateProfile::stateRva, std::uint32_t{9});
            saved.Global(NativeStartGameProfile::ThreadInitializedRva, static_cast<unsigned char>(1));
            saved.Global(NativeStartGameProfile::ThreadIdRva, GetCurrentThreadId());
            saved.Global(NativeBorrowedGcProfile::markerRva, LONG{0});
        };
        resetContext();
        const auto previousImage = ClockReaderTestHost::image;
        ClockReaderTestHost::image = fixture.image;
        Hook::writes = Hook::mode = 0;
        Hook::caller = nullptr;
        WorldGroundOwnerFixture::Callback callback;
        NativeClockService                service;
        Service*                          access = nullptr;
        callback.action = [&](NativeGameTimingAccess& owner) { access = dynamic_cast<Service*>(&owner); };
        BeginOwned(service, fixture, false, true, true, &WorldGroundOwnerFixture::Callback::Invoke, &callback);
        fixture.invokeTimer();
        const auto generation = runtime.generation.load();
        Check(access == static_cast<Service*>(&runtime.access), "explicit uncompressed service belongs to the actual natural callback object");
        const auto invoke = [&]
        {
            Check(service.Renew(generation), "renew only the current native uncompressed owner");
            fixture.invokeTimer();
        };
        auto&                   memory = static_cast<NativeAnimationSequenceStorageAccess&>(runtime.access);
        constexpr std::size_t   count = 65537;
        constexpr std::uint32_t bytes = static_cast<std::uint32_t>(count * 20);
        std::uintptr_t          array = 0, childFrames = 0, rootFrames = 0, undersizedFrames = 0;
        callback.action = [&](NativeGameTimingAccess&)
        {
            Check(memory.AllocateAnimationSequences(generation, 4, array) == ClockResult::Ok &&
                      memory.AllocateAnimationKeyFrames(generation, bytes, childFrames) == ClockResult::Ok &&
                      memory.AllocateAnimationKeyFrames(generation, 64, rootFrames) == ClockResult::Ok &&
                      memory.AllocateAnimationKeyFrames(generation, 19, undersizedFrames) == ClockResult::Ok,
                  "issue actual array plus native20/32 extents on the existing journal");
        };
        invoke();
        const Token childToken{generation, array, 0}, rootToken{generation, array, 1}, copyToken{generation, array, 2}, freshToken{generation, array, 3};
        Sdk         child(*access, childToken), rootSdk(*access, rootToken), copied(*access, copyToken);
        callback.action = [&](NativeGameTimingAccess&) { child.SetBoneTag(7); };
        invoke();
        std::array<unsigned char, 80> expected;
        std::memcpy(expected.data(), reinterpret_cast<void*>(array), expected.size());
        const std::uint16_t flags = 0x11, narrowed = 1;
        std::memcpy(expected.data() + 0x42, &flags, 2);
        std::memcpy(expected.data() + 0x44, &narrowed, 2);
        std::memcpy(expected.data() + 0x48, &childFrames, sizeof(childFrames));
        std::vector<unsigned char> payload(bytes, static_cast<unsigned char>(0x3d));
        std::memcpy(reinterpret_cast<void*>(childFrames), payload.data(), payload.size());
        const auto before = Hook::writes;
        callback.action = [&](NativeGameTimingAccess&)
        {
            Check(access->PrepareFreshUncompressedAnimationSequence(freshToken, 1, false, childFrames + 1) != ClockResult::Ok &&
                      access->PrepareFreshUncompressedAnimationSequence(freshToken, 1, false, undersizedFrames) != ClockResult::Ok &&
                      access->PrepareFreshUncompressedAnimationSequence(freshToken, 0, false, childFrames) != ClockResult::Ok &&
                      access->PrepareFreshUncompressedAnimationSequence(freshToken, (std::numeric_limits<std::size_t>::max)(), true, childFrames) !=
                          ClockResult::Ok &&
                      Hook::writes == before && !runtime.stopping.load(),
                  "runtime requires original base and full20/32 extent before any WORD narrowing or writer entry");
            Check(access->PrepareFreshUncompressedAnimationSequence(childToken, count, false, childFrames) == ClockResult::Ok &&
                      !std::memcmp(expected.data(), reinterpret_cast<void*>(array), expected.size()) && child.GetKeyFramesCount() == 1 &&
                      child.GetBoneTag() == 7 && !child.IsBigChunkForAllSequences() &&
                      reinterpret_cast<std::uintptr_t>(child.GetKeyFrame(65536, 20)) == childFrames + std::uintptr_t{65536} * 20,
                  "explicit child construction preserves source01/10 without04/08, full span and real MTA byte views");
            Check(access->PrepareFreshUncompressedAnimationSequence(rootToken, 2, true, rootFrames) == ClockResult::Ok &&
                      *reinterpret_cast<std::uint16_t*>(array + 80 + 0x42) == 3 && rootSdk.GetKeyFramesCount() == 2 &&
                      reinterpret_cast<std::uintptr_t>(rootSdk.GetKeyFrame(1, 32)) == rootFrames + 32,
                  "explicit root construction admits genuine32-byte records with only source01/02 flags");
        };
        invoke();
        Check(Hook::writes == before + 2 && !std::memcmp(reinterpret_cast<void*>(childFrames), payload.data(), payload.size()) && !host.freeCalls,
              "metadata binding neither reads/reformats payload nor retires original array/frame journals");
        callback.action = [&](NativeGameTimingAccess&)
        {
            copied.CopySequenceProperties(child.GetInterface());
            Check(copied.GetBoneTag() == 7 && copied.GetKeyFramesCount() == 1 &&
                      reinterpret_cast<std::uintptr_t>(copied.GetKeyFrame(65536, 20)) == childFrames + std::uintptr_t{65536} * 20,
                  "actual accepted Copy preserves source bytes, logical state and observable current frame origin");
            copied.Initialize();
            Check(!copied.GetKeyFrames() && !copied.GetKeyFramesCount() && child.GetKeyFramesCount() == 1 && host.Contains(childFrames),
                  "Initialize clears only the copied row; aliased frame allocation authority remains journaled");
        };
        invoke();
        auto mutableToken = copyToken;
        Hook::caller = &mutableToken;
        Hook::mode = 3;
        callback.action = [&](NativeGameTimingAccess&)
        {
            Check(access->PrepareFreshUncompressedAnimationSequence(mutableToken, 2, true, rootFrames) == ClockResult::Ok && !mutableToken.ownerGeneration &&
                      mutableToken.firstTag == 1 && mutableToken.index == (std::numeric_limits<std::size_t>::max)(),
                  "actual service copies token intent before synchronous native writer callbacks mutate caller storage");
        };
        invoke();
        Hook::mode = 0;
        Hook::caller = nullptr;
        callback.action = [&](NativeGameTimingAccess&)
        {
            Check(access->PrepareFreshUncompressedAnimationSequence(rootToken, 2, true, rootFrames) != ClockResult::Ok &&
                      access->PrepareFreshCompressedAnimationSequence(rootToken, 2, true, rootFrames) != ClockResult::Ok,
                  "neither explicit builder overwrites an already-bound row");
            Check(memory.FreeAnimationKeyFrames(generation, rootFrames) == ClockResult::Ok,
                  "unpublished frame retirement remains on its actual original record");
            NativeAnimationSequenceValues untouched;
            untouched.interfaceAddress = 0x123;
            Check(access->ReadAnimationSequence(rootToken, untouched) != ClockResult::Ok && untouched.interfaceAddress == 0x123,
                  "erased frame authority cannot be recovered from a copied physical pointer or raw uncompressed bit");
            rootSdk.Initialize();
            copied.Initialize();
        };
        invoke();
        Check(access->PrepareFreshUncompressedAnimationSequence(freshToken, 1, false, childFrames) != ClockResult::Ok,
              "CPU retention between callbacks supplies no explicit construction permission");
        Check(!service.End(), "producer End retains outstanding actual allocation records until natural callback cleanup");
        fixture.invokeTimer();
        Check(service.End() && !service.IsRetained() && !runtime.customSequences.IsRetained() && host.freeCalls == 4,
              "actual End frees each remaining original resource once despite Copy/Initialize aliases");
        // Public getters establish the Copy/Initialize effects above. Private
        // encoding descriptors are exercised by their genuine future consumer,
        // never inferred here from the physical04 bit alone.
        for (const unsigned failure : {1u, 2u})
        {
            resetContext();
            NativeClockService next;
            std::uintptr_t     tag = 0, buffer = 0;
            Token              failed;
            callback.action = [&](NativeGameTimingAccess&)
            {
                const auto gen = runtime.generation.load();
                Check(
                    memory.AllocateAnimationSequences(gen, 1, tag) == ClockResult::Ok && memory.AllocateAnimationKeyFrames(gen, 64, buffer) == ClockResult::Ok,
                    "prepare actual original records for one uncompressed entered effect");
                failed = {gen, tag, 0};
            };
            BeginOwned(next, fixture, false, true, true, &WorldGroundOwnerFixture::Callback::Invoke, &callback);
            fixture.invokeTimer();
            const auto freeBefore = host.freeCalls, writesBefore = Hook::writes;
            Hook::mode = failure;
            callback.action = [&](NativeGameTimingAccess&)
            {
                Check(access->PrepareFreshUncompressedAnimationSequence(failed, 2, true, buffer) != ClockResult::Ok,
                      "completed writer with unknown result or lost GC acknowledgement reports failure");
            };
            Check(next.Renew(failed.ownerGeneration), "renew only current uncompressed failure owner");
            fixture.invokeTimer();
            Check(runtime.stopping.load() && runtime.customSequences.IsRetained() && Hook::writes == writesBefore + 1 &&
                      *reinterpret_cast<std::uint16_t*>(tag + 0x42) == 3 && *reinterpret_cast<std::uint16_t*>(tag + 0x44) == 2 &&
                      *reinterpret_cast<std::uintptr_t*>(tag + 0x48) == buffer && host.Contains(buffer) && host.freeCalls == freeBefore,
                  "Stop retains actual uncompressed metadata effects and original frame authority after entry");
            Check(access->PrepareFreshUncompressedAnimationSequence(failed, 2, true, buffer) != ClockResult::Ok && Hook::writes == writesBefore + 1 &&
                      !next.End(),
                  "entered uncompressed operation cannot replay or rewind accepted physical fields");
            resetContext();
            Hook::mode = 0;
            fixture.invokeTimer();
            Check(next.End() && !next.IsRetained() && host.freeCalls == freeBefore + 2 && Hook::writes == writesBefore + 1,
                  "known unpublished debt retires both original records without retrying frame binding");
        }
        Host::active = nullptr;
        ClockReaderTestHost::image = previousImage;
        saved.Restore();
        std::puts("Production native fresh uncompressed sequence owner fixture completed");
        std::puts("Production actual fresh uncompressed sequence service owner fixture completed");
    }
}
