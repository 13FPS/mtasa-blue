/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Whole issued-row copy and bounded frame byte views
 ****************************************************************************/
#pragma push_macro("main")
#undef main
#define main HistoricalSequenceRowsCopyFrameHostMain
#include "NativeAnimationSequenceRowsTests.cpp"
#undef main
#pragma pop_macro("main")
#include "../../Client/game_de/NativeAnimationSequenceCopyCalls.h"
#include "../../Client/game_de/NativeAnimationSequenceFrameViewCalls.h"

namespace SequenceCopyFrameTestHost
{
    namespace Copy = NativeAnimationSequenceCopyCalls;
    namespace View = NativeAnimationSequenceFrameViewCalls;
    namespace Heap = NativeCustomAnimSequenceStorageFixture;
    unsigned copyFrameChecks = 0, copies = 0, mode = 0, payloadReads = 0;
    void     CheckNew(bool value, const char* reason)
    {
        ++copyFrameChecks;
        Check(value, reason);
    }
    ClockResult Owner() noexcept
    {
        return PoolTestHost::Owner();
    }
    ClockResult Write(std::uintptr_t target, const Copy::Record& source, std::uint32_t& exception) noexcept
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
    using namespace SequenceCopyFrameTestHost;
    namespace Arrays = NativeCustomAnimSequenceStorageCalls;
    namespace Frames = NativeAnimationKeyFrameStorageCalls;
    using Storage = NativeCustomAnimSequenceStorage;
    constexpr std::uint64_t generation = 0x100000031ULL;
    Check(argc == 2, "copy/frame fixture owns its output directory");
    SpecialFixture fixture(argv[1]);
    Heap::State    host;
    Heap::active = &host;
    const auto image = fixture.transform.image.image;
    const auto reset = [&]
    {
        fixture.Reset();
        PoolTestHost::image = image;
        host.Reset(image, &SpecialFixture::Patch, &fixture);
        PoolTestHost::reads = PoolTestHost::failAt = PoolTestHost::ownerCalls = PoolTestHost::revokeAt = 0;
        PoolTestHost::ownerResult = ClockResult::Ok;
        PoolTestHost::watched = 0;
        PoolTestHost::after = nullptr;
        copies = mode = 0;
        for (const auto& pin : NativeAnimationSequenceRowProfile::Code)
            SpecialFixture::Patch(&fixture, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
    };
    reset();
    {
        Storage         source, target, frames, oldFrames;
        Arrays::Receipt allocation;
        Check(Arrays::Allocate(image, source, generation, 2, &Owner, &allocation) == ClockResult::Ok, "prepare source array");
        const auto sourceTag = allocation.readyTag;
        Check(Arrays::Allocate(image, target, generation, 1, &Owner, &allocation) == ClockResult::Ok, "prepare destination array");
        const auto targetTag = allocation.readyTag;
        Check(Frames::Allocate(image, frames, generation, 96, &Owner, &allocation) == ClockResult::Ok, "prepare source frame bytes");
        const auto frameBase = allocation.readyTag;
        Check(Frames::Allocate(image, oldFrames, generation, 16, &Owner, &allocation) == ClockResult::Ok, "prepare overwritten frame bytes");
        const auto   oldFrameBase = allocation.readyTag;
        Copy::Record expected;
        for (std::size_t i = 0; i < expected.bytes.size(); ++i)
            expected.bytes[i] = static_cast<unsigned char>(i ^ 0x5a);
        const auto          pointer = frameBase + 8;
        const std::uint16_t count = 0;
        std::memcpy(expected.bytes.data() + 0x44, &count, sizeof(count));
        std::memcpy(expected.bytes.data() + 0x48, &pointer, sizeof(pointer));
        std::memcpy(reinterpret_cast<void*>(sourceTag + 80), expected.bytes.data(), expected.bytes.size());
        std::memcpy(reinterpret_cast<void*>(targetTag + 0x48), &oldFrameBase, sizeof(oldFrameBase));
        Copy::Receipt receipt;
        CheckNew(Copy::Copy(image, target, 0, source, 1, generation, &Owner, &receipt, &Write) == ClockResult::Ok && receipt.entered && receipt.rawCopied &&
                     receipt.verified && !receipt.selfCopy && copies == 1 && receipt.sourceRow == sourceTag + 80 && receipt.targetRow == targetTag &&
                     !std::memcmp(reinterpret_cast<void*>(targetTag), expected.bytes.data(), 80),
                 "one actual full eighty-byte copy preserves name, padding, flags and raw frame pointer");
        CheckNew(host.Contains(oldFrameBase) && host.Contains(frameBase) && !host.freeCalls,
                 "overwritten and aliased frame records remain separately owned; copy retires no child");
        CheckNew(Copy::Copy(image, target, 0, target, 0, generation, &Owner, &receipt, &Write) == ClockResult::Ok && receipt.selfCopy && !receipt.entered &&
                     !receipt.rawCopied && receipt.verified && copies == 1,
                 "qualified self-copy is a no-store acknowledgement");
        CheckNew(Copy::Copy(image, source, 0, source, 1, generation, &Owner, &receipt, &Write) == ClockResult::Ok && copies == 2 &&
                     !std::memcmp(reinterpret_cast<void*>(sourceTag), expected.bytes.data(), 80),
                 "distinct rows in one array use the same whole-record policy");
        const auto before = copies;
        CheckNew(Copy::Copy(image, target, 0, source, 2, generation, &Owner, &receipt, &Write) != ClockResult::Ok &&
                     Copy::Copy(image, target, (std::numeric_limits<std::size_t>::max)(), source, 0, generation, &Owner, &receipt, &Write) != ClockResult::Ok &&
                     Copy::Copy(image, target, 0, frames, 0, generation, &Owner, &receipt, &Write) != ClockResult::Ok &&
                     Copy::Copy(image, target, 0, source, 0, generation + 1, &Owner, &receipt, &Write) != ClockResult::Ok && copies == before,
                 "capacity, full-width row index, record kind and generation refuse before copy entry");
        PoolTestHost::ownerResult = ClockResult::WrongThread;
        CheckNew(Copy::Copy(image, target, 0, source, 0, generation, &Owner, &receipt, &Write) != ClockResult::Ok && !receipt.entered && copies == before,
                 "owner refusal cannot issue even a self-contained CPU copy");
        PoolTestHost::ownerResult = ClockResult::Ok;
        std::uintptr_t output = 0;
        CheckNew(View::Read(image, target, generation, 0, frames, 4, 16, &Owner, output) == ClockResult::Ok && output == pointer + 64,
                 "frame view uses actual issued byte extent even when logical frameCount is zero");
        CheckNew(View::Read(image, target, generation, 0, frames, (std::numeric_limits<std::size_t>::max)(), 0, &Owner, output) == ClockResult::Ok &&
                     output == pointer,
                 "zero stride preserves the current interior pointer for the full SIZE_MAX index");
        for (const auto index : {std::size_t{5}, std::size_t{6}, (std::numeric_limits<std::size_t>::max)()})
        {
            output = 0x12345;
            CheckNew(View::Read(image, target, generation, 0, frames, index, 16, &Owner, output) != ClockResult::Ok && output == 0x12345,
                     "span overrun, one-past and multiplication overflow preserve the caller output");
        }
        output = 0x12345;
        CheckNew(View::Read(image, target, generation, 0, oldFrames, 0, 1, &Owner, output) != ClockResult::Ok && output == 0x12345,
                 "a different genuine frame record cannot authorize the selected row pointer");
        // Observe the real ReadProcessMemory seam while keeping the frame
        // pages valid for ordinary owned-allocation range qualification.
        PoolTestHost::watched = pointer;
        PoolTestHost::after = +[] { ++payloadReads; };
        output = 0;
        CheckNew(View::Read(image, target, generation, 0, frames, 0, 1, &Owner, output) == ClockResult::Ok && output == pointer && !payloadReads,
                 "borrowed byte view publishes an address without reading frame payload");
        PoolTestHost::after = nullptr;
        PoolTestHost::watched = 0;
        PoolTestHost::reads = 0;
        Check(View::Read(image, target, generation, 0, frames, 0, 1, &Owner, output) == ClockResult::Ok, "measure new view bookends once");
        const auto fullReads = PoolTestHost::reads;
        for (const auto failed : {1u, fullReads})
        {
            output = 0x12345;
            PoolTestHost::reads = 0;
            PoolTestHost::failAt = failed;
            CheckNew(View::Read(image, target, generation, 0, frames, 0, 1, &Owner, output) != ClockResult::Ok && output == 0x12345 &&
                         target.GetState() == Storage::State::LiveUnpublished,
                     "first and final view read failures publish no pointer and create no write debt");
        }
        PoolTestHost::failAt = 0;
        Check(Arrays::FreeStorage(image, source, generation, &Owner) == ClockResult::Ok &&
                  Arrays::FreeStorage(image, target, generation, &Owner) == ClockResult::Ok &&
                  Frames::FreeStorage(image, frames, generation, &Owner) == ClockResult::Ok &&
                  Frames::FreeStorage(image, oldFrames, generation, &Owner) == ClockResult::Ok,
              "retire each original array/frame allocation once");
    }
    for (const unsigned failure : {1u, 2u})
    {
        reset();
        Storage         source, target;
        Arrays::Receipt allocation;
        Check(Arrays::Allocate(image, source, generation, 1, &Owner, &allocation) == ClockResult::Ok, "prepare copy failure source");
        const auto sourceTag = allocation.readyTag;
        Check(Arrays::Allocate(image, target, generation, 1, &Owner, &allocation) == ClockResult::Ok, "prepare copy failure target");
        const auto targetTag = allocation.readyTag;
        std::memset(reinterpret_cast<void*>(sourceTag), 0x6b, 80);
        mode = failure;
        Copy::Receipt receipt;
        CheckNew(Copy::Copy(image, target, 0, source, 0, generation, &Owner, &receipt, &Write) != ClockResult::Ok && receipt.entered &&
                     receipt.rawCopied == (failure == 2) && !receipt.verified && copies == 1 &&
                     !std::memcmp(reinterpret_cast<void*>(sourceTag), reinterpret_cast<void*>(targetTag), 80) &&
                     target.GetState() == Storage::State::FaultDebt && target.CanFreeStorage(),
                 "unknown or accepted copy failure preserves actual bytes, receipt distinction and known private storage debt");
        PoolTestHost::ownerResult = ClockResult::Ok;
        mode = 0;
        CheckNew(Copy::Copy(image, target, 0, source, 0, generation, &Owner, &receipt, &Write) != ClockResult::Ok && copies == 1,
                 "entered copy failure cannot replay or roll back its write");
        Check(Arrays::FreeStorage(image, target, generation, &Owner) == ClockResult::Ok &&
                  Arrays::FreeStorage(image, source, generation, &Owner) == ClockResult::Ok,
              "ordinary private storage cleanup survives copy debt without child retirement");
    }
    Heap::active = nullptr;
    std::printf("Passed %u native animation sequence copy/frame checks (64-bit; no native engine code executed)\n", copyFrameChecks);
    std::puts("Production native animation sequence copy/frame fixture completed");
    return 0;
}
