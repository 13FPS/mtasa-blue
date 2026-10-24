/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Private fresh preparation and actual existing sequence observations
 ****************************************************************************/
#pragma push_macro("main")
#undef main
#define main HistoricalFreshCompressedSequenceHostMain
#include "NativeStreamingSpecialModelTests.cpp"
#undef main
#pragma pop_macro("main")
#include "NativeCustomAnimSequenceStorageFixture.h"
#include "AnimationSequenceDEFixture.h"
#include "../../Client/game_de/NativeCustomAnimSequenceController.h"
#include "../../Client/game_de/NativeAnimationSequenceFrameBindCalls.h"
namespace FreshSequenceControllerHost
{
    namespace Fixture = NativeCustomAnimSequenceStorageFixture;
    namespace Bind = NativeAnimationSequenceFrameBindCalls;
    using Controller = NativeCustomAnimSequenceController;
    unsigned                               freshChecks = 0, writes = 0, mode = 0;
    NativeAnimationSequenceReferenceToken* callerToken = nullptr;
    std::size_t*                           callerCount = nullptr;
    bool*                                  callerRoot = nullptr;
    std::uintptr_t*                        callerBase = nullptr;
    void                                   Verify(bool value, const char* why)
    {
        ++freshChecks;
        Check(value, why);
    }
    ClockResult Write(std::uintptr_t, const Bind::Value&, std::uint32_t&) noexcept;
    ClockResult Owner() noexcept
    {
        const auto result = PoolTestHost::Owner();
        if (callerToken)
        {
            *callerToken = {};
            callerToken = nullptr;
        }
        if (callerCount)
        {
            *callerCount = 0;
            callerCount = nullptr;
        }
        if (callerRoot)
        {
            *callerRoot = !*callerRoot;
            callerRoot = nullptr;
        }
        if (callerBase)
        {
            ++*callerBase;
            callerBase = nullptr;
        }
        return result;
    }
    class Service final : public NativeAnimationSequenceAccess
    {
    public:
        Service(Controller& value, std::uintptr_t imageValue) noexcept : controller(value), image(imageValue) {}
        Controller::FrameBindReceipt receipt;
        ClockResult                  PrepareFreshCompressedAnimationSequence(const NativeAnimationSequenceReferenceToken& token, std::size_t count, bool root,
                                                                             std::uintptr_t base) noexcept override
        {
            return controller.PrepareFreshCompressedSequence(image, token, count, root, base, &Owner, &receipt);
        }
        ClockResult ReadAnimationSequence(const NativeAnimationSequenceReferenceToken& token, NativeAnimationSequenceValues& output) noexcept override
        {
            return controller.ReadSequence(image, token, &Owner, output);
        }
        ClockResult ReadAnimationSequenceHash(const NativeAnimationSequenceReferenceToken& token, std::uint32_t& output) noexcept override
        {
            return controller.ReadSequenceHash(image, token, &Owner, output);
        }
        ClockResult ReadAnimationSequenceFrame(const NativeAnimationSequenceReferenceToken& token, std::size_t index, std::uint32_t stride,
                                               std::uintptr_t& output) noexcept override
        {
            return controller.ReadSequenceFrame(image, token, index, stride, &Owner, output);
        }

    private:
        Controller&    controller;
        std::uintptr_t image;
    };
}
#define GetModuleHandleW                           PoolTestHost::Module
#define ReadProcessMemory                          PoolTestHost::Read
#define MTA_DE_CUSTOM_ANIM_SEQUENCE_MALLOC_INVOKE  ::NativeCustomAnimSequenceStorageFixture::Malloc
#define MTA_DE_CUSTOM_ANIM_SEQUENCE_FREE_INVOKE    ::NativeCustomAnimSequenceStorageFixture::Free
#define MTA_DE_ANIMATION_SEQUENCE_FRAME_BIND_WRITE ::FreshSequenceControllerHost::Write
#include "../../Client/game_de/NativeCustomAnimSequenceStorage.cpp"
#undef MTA_DE_ANIMATION_SEQUENCE_FRAME_BIND_WRITE
#undef MTA_DE_CUSTOM_ANIM_SEQUENCE_FREE_INVOKE
#undef MTA_DE_CUSTOM_ANIM_SEQUENCE_MALLOC_INVOKE
#undef ReadProcessMemory
#undef GetModuleHandleW
#include "../../Client/game_de/NativeCustomAnimSequenceController.cpp"
namespace FreshSequenceControllerHost
{
    ClockResult Write(std::uintptr_t row, const Bind::Value& target, std::uint32_t& exception) noexcept
    {
        ++writes;
        const auto result = mta::game_de::CustomAnimSequenceStorageDetail::WriteCompressedFrames(row, target, exception);
        if (result != ClockResult::Ok)
            return result;
        if (mode == 1)
        {
            exception = EXCEPTION_ACCESS_VIOLATION;
            return ClockResult::OriginalException;
        }
        if (mode == 2)
            PoolTestHost::ownerResult = ClockResult::OwnerConflict;
        return ClockResult::Ok;
    }
}
int main(int argc, char** argv)
{
    using namespace FreshSequenceControllerHost;
    constexpr std::uint64_t generation = 0xfedcba9876543261ull;
    Check(argc == 2, "fresh private frame fixture owns its output directory");
    SpecialFixture fixture(argv[1]);
    Fixture::State host;
    Fixture::active = &host;
    const auto image = fixture.transform.image.image;
    const auto reset = [&]
    {
        fixture.Reset();
        PoolTestHost::image = image;
        host.Reset(image, &SpecialFixture::Patch, &fixture);
        for (const auto& pin : NativeAnimationSequenceRowProfile::Code)
            SpecialFixture::Patch(&fixture, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : NativeAnimationSequenceNameProfile::Code)
            SpecialFixture::Patch(&fixture, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : NativeAnimationSequenceFrameBindProfile::Code)
            SpecialFixture::Patch(&fixture, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        PoolTestHost::reads = PoolTestHost::failAt = PoolTestHost::ownerCalls = PoolTestHost::revokeAt = 0;
        PoolTestHost::ownerResult = ClockResult::Ok;
        PoolTestHost::watched = 0;
        PoolTestHost::after = nullptr;
        writes = mode = 0;
        callerToken = nullptr;
        callerCount = nullptr;
        callerRoot = nullptr;
        callerBase = nullptr;
    };
    NativeAnimationSequenceAccess missing;
    Verify(missing.PrepareFreshCompressedAnimationSequence({}, (std::numeric_limits<std::size_t>::max)(), true, 0) == ClockResult::NotInitialized,
           "optional private service defaults to refusal while general SetKeyFrames remains pure");
    for (const bool root : {false, true})
    {
        reset();
        Controller          controller;
        std::uintptr_t      tag = 0, frames = 0;
        const std::size_t   count = root ? 3u : 65536u;
        const std::uint32_t stride = root ? 16u : 10u;
        const auto          bytes = static_cast<std::uint32_t>(count * stride);
        Check(controller.Allocate(image, generation, 1, &Owner, tag) == ClockResult::Ok &&
                  controller.AllocateFrames(image, generation, bytes, &Owner, frames) == ClockResult::Ok,
              "prepare actual fresh row and compressed byte record");
        NativeAnimationSequenceReferenceToken token{generation, tag, 0};
        Check(controller.SetSequenceBoneTag(image, token, 9, &Owner) == ClockResult::Ok, "prepare optional physical bone selector");
        NativeAnimationSequenceName name;
        Check(NativeAnimationSequenceNamePolicy::Copy("boundName", name) && controller.SetSequenceName(image, token, name, &Owner) == ClockResult::Ok,
              "prepare defined logical name without disturbing fresh pointer/count");
        Service                                               service(controller, image);
        animation_sequence_fixture::AnimationSequenceReceiver receiver(service, token);
        CAnimBlendSequence&                                   sdk = receiver;
        const auto                                            priorHash = sdk.GetHash();
        const auto                                            priorBone = sdk.GetBoneTag();
        std::array<unsigned char, 80>                         expected{};
        std::memcpy(expected.data(), reinterpret_cast<void*>(tag), expected.size());
        const std::uint16_t flags = static_cast<std::uint16_t>(0x10u | 0x05u | (root ? 2u : 0u));
        const auto          wordCount = static_cast<std::uint16_t>(count);
        std::memcpy(expected.data() + 0x42, &flags, sizeof(flags));
        std::memcpy(expected.data() + 0x44, &wordCount, sizeof(wordCount));
        std::memcpy(expected.data() + 0x48, &frames, sizeof(frames));
        auto changedToken = token;
        auto changedCount = count;
        auto changedRoot = root;
        auto changedBase = frames;
        callerToken = &changedToken;
        callerCount = &changedCount;
        callerRoot = &changedRoot;
        callerBase = &changedBase;
        Verify(service.PrepareFreshCompressedAnimationSequence(changedToken, changedCount, changedRoot, changedBase) == ClockResult::Ok &&
                   service.receipt.accepted && service.receipt.verified && service.receipt.count == count && service.receipt.bytes == bytes && writes == 1 &&
                   !changedToken.ownerGeneration && !changedCount && changedRoot != root && changedBase == frames + 1u,
               "explicit service preserves copied full count/root/base/token before callbacks mutate caller values");
        Verify(!std::memcmp(expected.data(), reinterpret_cast<void*>(tag), expected.size()) && host.mallocCalls == 2 && !host.freeCalls,
               "fresh commit changes only native flags/WORDcount/pointer with no allocation or child cleanup");
        bool payloadUnchanged = true;
        for (std::size_t index = 0; index != bytes; ++index)
            payloadUnchanged = payloadUnchanged && reinterpret_cast<const unsigned char*>(frames)[index] == 0xa5;
        Verify(payloadUnchanged, "preparation leaves all existing animation payload bytes unchanged");
        Verify(reinterpret_cast<std::uintptr_t>(sdk.GetKeyFrames()) == frames && sdk.GetKeyFramesCount() == wordCount && !sdk.IsBigChunkForAllSequences() &&
                   sdk.GetHash() == priorHash && sdk.GetBoneTag() == priorBone && reinterpret_cast<std::uintptr_t>(sdk.GetInterface()) == tag,
               "actual existing facade getters expose prepared pointer/rawWORD count without changing logical name/bone or inferring shared08");
        Verify(reinterpret_cast<std::uintptr_t>(sdk.GetKeyFrame(count - 1u, stride)) == frames + (count - 1u) * stride,
               "full byte-view capacity survives even when full65536 narrows the observed WORD count tozero");
        Verify(service.PrepareFreshCompressedAnimationSequence(token, count, root, frames) != ClockResult::Ok && !service.receipt.entered && writes == 1,
               "already bound row cannot replay a fresh transaction, including nonnull-pointer WORDcountzero");
        Check(controller.FreeFrames(image, generation, frames, &Owner) == ClockResult::Ok &&
                  controller.FreeStorage(image, generation, tag, &Owner) == ClockResult::Ok,
              "retire each original record explicitly after private preparation");
        Verify(host.freeCalls == 2 && !controller.IsRetained() && !controller.RecoveryPending(),
               "preparation neither pins publication nor erases original journals");
    }
    reset();
    {
        Controller     controller;
        std::uintptr_t tag = 0, frames = 0;
        Check(controller.Allocate(image, generation, 1, &Owner, tag) == ClockResult::Ok &&
                  controller.AllocateFrames(image, generation, 30, &Owner, frames) == ClockResult::Ok,
              "prepare fresh refusal records");
        NativeAnimationSequenceReferenceToken token{generation, tag, 0};
        Controller::FrameBindReceipt          receipt;
        for (const auto invalidBase : {std::uintptr_t{0}, frames + 1u, tag})
            Verify(controller.PrepareFreshCompressedSequence(image, token, 3, false, invalidBase, &Owner, &receipt) != ClockResult::Ok && !receipt.entered &&
                       !writes,
                   "only the original currently issued same-owner frame base grants attachment authority");
        Verify(controller.PrepareFreshCompressedSequence(image, token, 0, false, frames, &Owner, &receipt) != ClockResult::Ok && !receipt.entered &&
                   controller.PrepareFreshCompressedSequence(image, token, (std::numeric_limits<std::size_t>::max)(), true, frames, &Owner, &receipt) !=
                       ClockResult::Ok &&
                   !receipt.entered && !writes,
               "zero/full-width overflowing count cannot pass private span admission");
        Verify(controller.PrepareFreshCompressedSequence(image, token, 3, true, frames, &Owner, &receipt) != ClockResult::Ok && !receipt.entered && !writes,
               "root16-byte span cannot reuse a shorter child10-byte allocation");
        Check(controller.FreeFrames(image, generation, frames, &Owner) == ClockResult::Ok, "free a refused current frame record");
        Verify(controller.PrepareFreshCompressedSequence(image, token, 3, false, frames, &Owner, &receipt) != ClockResult::Ok && !receipt.entered && !writes,
               "freed frame origin cannot be recovered through a readable stale pointer");
        Check(controller.FreeStorage(image, generation, tag, &Owner) == ClockResult::Ok, "retire refused array record");
    }
    for (const unsigned failure : {1u, 2u})
    {
        reset();
        Controller     controller;
        std::uintptr_t tag = 0, frames = 0;
        Check(controller.Allocate(image, generation, 1, &Owner, tag) == ClockResult::Ok &&
                  controller.AllocateFrames(image, generation, 30, &Owner, frames) == ClockResult::Ok,
              "prepare entered binding failure records");
        NativeAnimationSequenceReferenceToken token{generation, tag, 0};
        Controller::FrameBindReceipt          receipt;
        mode = failure;
        Verify(controller.PrepareFreshCompressedSequence(image, token, 3, false, frames, &Owner, &receipt) != ClockResult::Ok && receipt.entered &&
                   receipt.accepted == (failure == 2) && writes == 1 && controller.IsRetained() && controller.RecoveryPending() && !host.freeCalls,
               "unknown binding write and accepted-store lost acknowledgement retain both original allocation journals");
        mode = 0;
        PoolTestHost::ownerResult = ClockResult::Ok;
        Verify(controller.PrepareFreshCompressedSequence(image, token, 3, false, frames, &Owner, &receipt) != ClockResult::Ok && writes == 1,
               "entered private binding cannot replay or automatically restore fields");
        Controller::RetireProgress progress;
        Verify(controller.Retire(image, generation, 4, &Owner, progress) == ClockResult::Ok && !progress.retained && host.freeCalls == 2,
               "known unpublished binding debt retires original storage only without interpreting payload or invoking native child cleanup");
    }
    Fixture::active = nullptr;
    std::puts("Production private fresh compressed sequence controller fixture completed");
    std::puts("Actual prepared sequence getter observations completed");
    std::printf("Passed %u fresh compressed sequence controller checks (64-bit; no native engine code executed)\n", freshChecks);
    return 0;
}
