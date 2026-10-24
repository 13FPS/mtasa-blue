/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Only actual controller sequence copy and bounded frame-view methods
 ****************************************************************************/
#pragma push_macro("main")
#undef main
#define main HistoricalSequenceCopyFrameControllerHostMain
#include "NativeStreamingSpecialModelTests.cpp"
#undef main
#pragma pop_macro("main")
#include "NativeCustomAnimSequenceStorageFixture.h"
#include "../../Client/game_de/NativeCustomAnimSequenceController.h"
#include "../../Client/game_de/NativeAnimationSequenceCopyCalls.h"
#include "../../Client/game_de/NativeAnimationSequenceFrameViewCalls.h"
namespace SequenceCopyFrameControllerHost
{
    namespace Fixture = NativeCustomAnimSequenceStorageFixture;
    namespace Copy = NativeAnimationSequenceCopyCalls;
    unsigned                               copyFrameChecks = 0, copies = 0, mode = 0;
    NativeAnimationSequenceReferenceToken* callerToken = nullptr;
    std::size_t*                           callerIndex = nullptr;
    std::uint32_t*                         callerStride = nullptr;
    void                                   Verify(bool value, const char* why)
    {
        ++copyFrameChecks;
        Check(value, why);
    }
    ClockResult CopyRow(std::uintptr_t, const Copy::Record&, std::uint32_t&) noexcept;
    ClockResult Owner() noexcept
    {
        const auto result = PoolTestHost::Owner();
        if (callerToken)
        {
            *callerToken = {};
            callerToken = nullptr;
        }
        if (callerIndex)
        {
            *callerIndex = (std::numeric_limits<std::size_t>::max)();
            callerIndex = nullptr;
        }
        if (callerStride)
        {
            *callerStride = 0;
            callerStride = nullptr;
        }
        return result;
    }
}
#define GetModuleHandleW                          PoolTestHost::Module
#define ReadProcessMemory                         PoolTestHost::Read
#define MTA_DE_CUSTOM_ANIM_SEQUENCE_MALLOC_INVOKE ::NativeCustomAnimSequenceStorageFixture::Malloc
#define MTA_DE_CUSTOM_ANIM_SEQUENCE_FREE_INVOKE   ::NativeCustomAnimSequenceStorageFixture::Free
#define MTA_DE_ANIMATION_SEQUENCE_COPY_WRITE      ::SequenceCopyFrameControllerHost::CopyRow
#include "../../Client/game_de/NativeCustomAnimSequenceStorage.cpp"
#undef MTA_DE_ANIMATION_SEQUENCE_COPY_WRITE
#undef MTA_DE_CUSTOM_ANIM_SEQUENCE_FREE_INVOKE
#undef MTA_DE_CUSTOM_ANIM_SEQUENCE_MALLOC_INVOKE
#undef ReadProcessMemory
#undef GetModuleHandleW
#include "../../Client/game_de/NativeCustomAnimSequenceController.cpp"
namespace SequenceCopyFrameControllerHost
{
    ClockResult CopyRow(std::uintptr_t target, const Copy::Record& source, std::uint32_t& exception) noexcept
    {
        ++copies;
        const auto result = mta::game_de::CustomAnimSequenceStorageDetail::CopyRow(target, source, exception);
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
    using namespace SequenceCopyFrameControllerHost;
    using Controller = NativeCustomAnimSequenceController;
    constexpr std::uint64_t  generation = 0xfedcba9876543251ull;
    constexpr std::uintptr_t sentinel = 0x12345678u;
    Check(argc == 2, "copy/frame controller fixture owns its output directory");
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
        PoolTestHost::reads = PoolTestHost::failAt = PoolTestHost::ownerCalls = PoolTestHost::revokeAt = 0;
        PoolTestHost::ownerResult = ClockResult::Ok;
        PoolTestHost::watched = 0;
        PoolTestHost::after = nullptr;
        copies = mode = 0;
        callerToken = nullptr;
        callerIndex = nullptr;
        callerStride = nullptr;
    };
    reset();
    {
        Controller     controller;
        std::uintptr_t destination = 0, source = 0, frames = 0;
        Check(controller.Allocate(image, generation, 2, &Owner, destination) == ClockResult::Ok &&
                  controller.Allocate(image, generation, 2, &Owner, source) == ClockResult::Ok &&
                  controller.AllocateFrames(image, generation, 41, &Owner, frames) == ClockResult::Ok,
              "prepare current copy/frame owner records");
        NativeAnimationSequenceReferenceToken target{generation, destination, 0}, original{generation, source, 1};
        const auto                            sourceRow = source + 80u;
        Check(controller.SetSequenceBoneTag(image, original, 9, &Owner) == ClockResult::Ok &&
                  controller.SetSequenceBoneTag(image, target, 7, &Owner) == ClockResult::Ok,
              "prepare distinct owner-held logical rows");
        std::memset(reinterpret_cast<void*>(sourceRow), 0xc7, 64);
        const std::uint16_t flags = 0x2208, frameCount = 0, padding = 0xbeef;
        std::memcpy(reinterpret_cast<void*>(sourceRow + 0x42), &flags, sizeof(flags));
        std::memcpy(reinterpret_cast<void*>(sourceRow + 0x44), &frameCount, sizeof(frameCount));
        std::memcpy(reinterpret_cast<void*>(sourceRow + 0x46), &padding, sizeof(padding));
        std::memcpy(reinterpret_cast<void*>(sourceRow + 0x48), &frames, sizeof(frames));
        std::array<unsigned char, 80> expected{};
        std::memcpy(expected.data(), reinterpret_cast<void*>(sourceRow), expected.size());
        Controller::CopyReceipt receipt;
        Verify(controller.CopySequenceProperties(image, target, sourceRow, &Owner, &receipt) == ClockResult::Ok && receipt.entered && receipt.rawCopied &&
                   receipt.verified && !receipt.selfCopy && copies == 1 && !std::memcmp(expected.data(), reinterpret_cast<void*>(destination), 80) &&
                   !host.freeCalls && controller.IsRetained(),
               "one controller admits aligned same-generation rows, copies all backend bytes and never frees replaced or aliased children");
        NativeAnimationSequenceValues value;
        Verify(controller.ReadSequence(image, target, &Owner, value) == ClockResult::Ok && value.boneTag == 9 && value.keyFrames == frames && value.bigChunk,
               "actual CopyProperties transfers source logical metadata as well as physical frame aliases");
        Check(controller.SetSequenceBoneTag(image, original, 15, &Owner) == ClockResult::Ok, "change the separate source logical value");
        Verify(controller.ReadSequence(image, target, &Owner, value) == ClockResult::Ok && value.boneTag == 9,
               "copied logical state is a value rather than a shared pointer to the source row");
        const auto beforeSelf = copies;
        Verify(controller.CopySequenceProperties(image, target, destination, &Owner, &receipt) == ClockResult::Ok && receipt.selfCopy && receipt.verified &&
                   !receipt.entered && !receipt.rawCopied && copies == beforeSelf && !host.freeCalls,
               "qualified self-copy remains a no-store without losing owner metadata or journals");
        for (const auto badSource : {std::uintptr_t{0}, sourceRow + 1u, frames})
            Verify(controller.CopySequenceProperties(image, target, badSource, &Owner, &receipt) != ClockResult::Ok && !receipt.entered && copies == beforeSelf,
                   "null, unaligned and non-array source locators do not grant raw-copy authority");
        auto changed = target;
        callerToken = &changed;
        Verify(controller.CopySequenceProperties(image, changed, sourceRow, &Owner, &receipt) == ClockResult::Ok && !changed.ownerGeneration &&
                   receipt.rawCopied && controller.ReadSequence(image, target, &Owner, value) == ClockResult::Ok && value.boneTag == 15,
               "destination intent is copied before owner callbacks while the current source logical snapshot is transferred");
        std::uintptr_t output = sentinel;
        Verify(controller.ReadSequenceFrame(image, target, 0, 1, &Owner, output) == ClockResult::Ok && output == frames &&
                   controller.ReadSequenceFrame(image, target, 40, 1, &Owner, output) == ClockResult::Ok && output == frames + 40u,
               "frame byte views use actual current record extent despite logical/native frameCount zero");
        Verify(controller.ReadSequenceFrame(image, target, (std::numeric_limits<std::size_t>::max)(), 0, &Owner, output) == ClockResult::Ok && output == frames,
               "zero stride preserves the current pointer even for full SIZE_MAX index");
        std::size_t   index = 2;
        std::uint32_t stride = 4;
        changed = target;
        callerToken = &changed;
        callerIndex = &index;
        callerStride = &stride;
        Verify(controller.ReadSequenceFrame(image, changed, index, stride, &Owner, output) == ClockResult::Ok && output == frames + 8u &&
                   !changed.ownerGeneration && index == (std::numeric_limits<std::size_t>::max)() && !stride,
               "controller copies token, full index and DWORD stride before its first owner callback");
        for (const auto bad : {std::pair<std::size_t, std::uint32_t>{41, 1}, {40, 4}, {(std::numeric_limits<std::size_t>::max)(), 0xffffffffu}})
        {
            output = sentinel;
            Verify(controller.ReadSequenceFrame(image, target, bad.first, bad.second, &Owner, output) != ClockResult::Ok && output == sentinel,
                   "one-past, crossing span and overflowing full-width arithmetic preserve output");
        }
        PoolTestHost::ownerResult = ClockResult::WrongThread;
        output = sentinel;
        Verify(controller.ReadSequenceFrame(image, target, 0, 1, &Owner, output) == ClockResult::WrongThread && output == sentinel &&
                   controller.CopySequenceProperties(image, target, sourceRow, &Owner, &receipt) == ClockResult::WrongThread && !receipt.entered,
               "both new methods refuse a foreign callback before publication or mutation");
        PoolTestHost::ownerResult = ClockResult::Ok;
        Controller     foreign;
        std::uintptr_t foreignFrames = 0, foreignArray = 0;
        Check(foreign.AllocateFrames(image, generation, 41, &Owner, foreignFrames) == ClockResult::Ok &&
                  foreign.Allocate(image, generation, 1, &Owner, foreignArray) == ClockResult::Ok,
              "prepare another controller's current records");
        Verify(controller.CopySequenceProperties(image, target, foreignArray, &Owner, &receipt) != ClockResult::Ok && !receipt.entered,
               "same-generation source rows issued by another controller cannot be adopted");
        std::memcpy(reinterpret_cast<void*>(destination + 0x48), &foreignFrames, sizeof(foreignFrames));
        output = sentinel;
        Verify(controller.ReadSequenceFrame(image, target, 0, 1, &Owner, output) != ClockResult::Ok && output == sentinel,
               "frame origin comes from this exact controller journal rather than same-generation foreign heap bytes");
        std::memcpy(reinterpret_cast<void*>(destination + 0x48), &frames, sizeof(frames));
        Check(foreign.FreeFrames(image, generation, foreignFrames, &Owner) == ClockResult::Ok &&
                  foreign.FreeStorage(image, generation, foreignArray, &Owner) == ClockResult::Ok,
              "retire the unrelated owner");
        Check(controller.FreeFrames(image, generation, frames, &Owner) == ClockResult::Ok, "free aliased frames through their one independent journal");
        output = sentinel;
        Verify(controller.ReadSequenceFrame(image, target, 0, 1, &Owner, output) != ClockResult::Ok && output == sentinel,
               "freed frame records cannot publish a dangling view");
        Check(controller.FreeStorage(image, generation, source, &Owner) == ClockResult::Ok, "retire source array storage only");
        Verify(controller.CopySequenceProperties(image, target, sourceRow, &Owner, &receipt) != ClockResult::Ok && !receipt.entered,
               "freed source rows refuse before any same-address reissue without an invented ABA guarantee");
        Check(controller.FreeStorage(image, generation, destination, &Owner) == ClockResult::Ok, "retire destination storage without child cleanup");
        Verify(!controller.IsRetained() && !controller.RecoveryPending(), "all known copy/frame records retire through their explicit storage owner");
    }
    for (const unsigned failure : {1u, 2u})
    {
        reset();
        Controller     controller;
        std::uintptr_t targetTag = 0, sourceTag = 0;
        Check(controller.Allocate(image, generation, 1, &Owner, targetTag) == ClockResult::Ok &&
                  controller.Allocate(image, generation, 1, &Owner, sourceTag) == ClockResult::Ok,
              "prepare entered-copy failure records");
        NativeAnimationSequenceReferenceToken target{generation, targetTag, 0}, source{generation, sourceTag, 0};
        Check(controller.SetSequenceBoneTag(image, source, 12, &Owner) == ClockResult::Ok, "prepare source logical copy state");
        Controller::CopyReceipt receipt;
        mode = failure;
        Verify(controller.CopySequenceProperties(image, target, sourceTag, &Owner, &receipt) != ClockResult::Ok && receipt.entered &&
                   receipt.rawCopied == (failure == 2) && copies == 1 && controller.IsRetained() && controller.RecoveryPending() && !host.freeCalls,
               "completed-copy lost acknowledgement differs from untrusted writer failure and keeps both owner journals");
        PoolTestHost::ownerResult = ClockResult::Ok;
        mode = 0;
        Verify(controller.CopySequenceProperties(image, target, sourceTag, &Owner, &receipt) != ClockResult::Ok && copies == 1,
               "a faulted target cannot replay the entered raw copy");
        Controller::RetireProgress progress;
        Verify(controller.Retire(image, generation, 4, &Owner, progress) == ClockResult::Ok && !progress.retained && host.freeCalls == 2,
               "known unpublished arrays still retire storage only, with no copy replay or native child retirement");
    }
    Fixture::active = nullptr;
    std::puts("Production sequence copy/frame controller fixture completed");
    std::printf("Passed %u sequence copy/frame controller checks (64-bit; no native engine code executed)\n", copyFrameChecks);
    return 0;
}
