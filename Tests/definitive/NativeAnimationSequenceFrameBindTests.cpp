/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Private fresh compressed rows retain original frame allocation owners
 ****************************************************************************/
#pragma push_macro("main")
#undef main
#define main HistoricalFreshCompressedSequenceHostMain
#include "NativeAnimationSequenceNameTests.cpp"
#undef main
#pragma pop_macro("main")
#include "../../Client/game_de/NativeAnimationSequenceFrameBindCalls.h"
#include "../../Client/game_de/NativeAnimationSequenceFrameBindProfile.h"
namespace FreshCompressedFrameTestHost
{
    namespace Bind = NativeAnimationSequenceFrameBindCalls;
    namespace Rows = NativeAnimationSequenceRowCalls;
    namespace Heap = NativeCustomAnimSequenceStorageFixture;
    unsigned bindChecks = 0, bindWrites = 0, mode = 0;
    void     Verify(bool value, const char* reason)
    {
        ++bindChecks;
        Check(value, reason);
    }
    ClockResult Owner() noexcept
    {
        return PoolTestHost::Owner();
    }
    ClockResult Write(std::uintptr_t row, const Rows::Value& target, std::uint32_t& exception) noexcept
    {
        ++bindWrites;
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
    NativeAnimationSequenceCopyCalls::Record Snapshot(std::uintptr_t row)
    {
        NativeAnimationSequenceCopyCalls::Record value;
        std::memcpy(value.bytes.data(), reinterpret_cast<void*>(row), value.bytes.size());
        return value;
    }
}
int main(int argc, char** argv)
{
    using namespace FreshCompressedFrameTestHost;
    namespace Arrays = NativeCustomAnimSequenceStorageCalls;
    namespace Frames = NativeAnimationKeyFrameStorageCalls;
    using Storage = NativeCustomAnimSequenceStorage;
    constexpr std::uint64_t generation = 0x100000091ULL;
    Check(argc == 2, "fresh frame fixture owns its explicit evidence directory");
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
        bindWrites = mode = 0;
        for (const auto& pin : NativeAnimationSequenceRowProfile::Code)
            SpecialFixture::Patch(&fixture, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : NativeAnimationSequenceFrameBindProfile::Code)
            SpecialFixture::Patch(&fixture, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
    };
    reset();
    {
        Storage         array, rootFrames, childFrames, smallFrames;
        Arrays::Receipt allocated;
        Check(Arrays::Allocate(image, array, generation, 3, &Owner, &allocated) == ClockResult::Ok, "issue original array storage");
        const auto              tag = allocated.readyTag;
        constexpr std::size_t   fullCount = 65537;
        constexpr std::uint32_t rootBytes = static_cast<std::uint32_t>(fullCount * 16);
        Check(Frames::Allocate(image, rootFrames, generation, rootBytes, &Owner, &allocated) == ClockResult::Ok, "issue full root compressed frame span");
        const auto rootBase = allocated.readyTag;
        Check(Frames::Allocate(image, childFrames, generation, 30, &Owner, &allocated) == ClockResult::Ok, "issue exact child compressed frame span");
        const auto childBase = allocated.readyTag;
        Check(Frames::Allocate(image, smallFrames, generation, 16, &Owner, &allocated) == ClockResult::Ok, "issue too-small but genuine frame allocation");
        const std::uint16_t selector = 0x10;
        std::memcpy(reinterpret_cast<void*>(tag + 0x42), &selector, 2);
        auto                expected = Snapshot(tag);
        const std::uint16_t rootFlags = 0x17, narrowed = 1;
        std::memcpy(expected.bytes.data() + 0x42, &rootFlags, 2);
        std::memcpy(expected.bytes.data() + 0x44, &narrowed, 2);
        std::memcpy(expected.bytes.data() + 0x48, &rootBase, sizeof(rootBase));
        Bind::Receipt receipt;
        Verify(Bind::AttachCompressed(image, array, generation, 0, rootFrames, fullCount, true, &Owner, &receipt, &Write) == ClockResult::Ok &&
                   receipt.entered && receipt.accepted && receipt.verified && bindWrites == 1 && Snapshot(tag).bytes == expected.bytes,
               "real guarded root binding validates the full count before WORD storage and preserves names, bone, selector and padding");
        Verify(host.Contains(rootBase) && !host.freeCalls && *reinterpret_cast<unsigned char*>(rootBase) == 0xa5 &&
                   *reinterpret_cast<unsigned char*>(rootBase + rootBytes - 1) == 0xa5,
               "binding changes no frame payload or original allocation ownership");
        const auto childRow = tag + 80;
        expected = Snapshot(childRow);
        const std::uint16_t childFlags = 5, childCount = 3;
        std::memcpy(expected.bytes.data() + 0x42, &childFlags, 2);
        std::memcpy(expected.bytes.data() + 0x44, &childCount, 2);
        std::memcpy(expected.bytes.data() + 0x48, &childBase, sizeof(childBase));
        Verify(Bind::AttachCompressed(image, array, generation, 1, childFrames, 3, false, &Owner, &receipt, &Write) == ClockResult::Ok &&
                   Snapshot(childRow).bytes == expected.bytes && bindWrites == 2,
               "child compressed stride10 sets only frames/compression bits and never infers shared-chunk08 from nonnull data");
        const auto before = bindWrites;
        Verify(Bind::AttachCompressed(image, array, generation, 0, rootFrames, fullCount, true, &Owner, &receipt, &Write) != ClockResult::Ok &&
                   !receipt.entered && bindWrites == before,
               "already linked row cannot replay the fresh-only builder");
        for (const auto count : {std::size_t{0}, fullCount, (std::numeric_limits<std::size_t>::max)(), std::size_t{0x10000000}})
            Verify(Bind::AttachCompressed(image, array, generation, 2, smallFrames, count, true, &Owner, &receipt, &Write) != ClockResult::Ok &&
                       !receipt.entered && bindWrites == before,
                   "zero, undersized full count and overflow products refuse before low-WORD narrowing or a row store");
        const auto freshRow = tag + 160;
        for (const std::uint16_t forbidden : {std::uint16_t{1}, std::uint16_t{2}, std::uint16_t{4}, std::uint16_t{8}, std::uint16_t{0x20}})
        {
            std::memcpy(reinterpret_cast<void*>(freshRow + 0x42), &forbidden, 2);
            Verify(Bind::AttachCompressed(image, array, generation, 2, childFrames, 1, false, &Owner, &receipt, &Write) != ClockResult::Ok &&
                       !receipt.entered && bindWrites == before,
                   "fresh admission rejects prior root/compression/shared/unknown flags instead of guessing an overwrite policy");
        }
        const std::uint16_t  zero = 0, one = 1;
        const std::uintptr_t nullFrames = 0;
        std::memcpy(reinterpret_cast<void*>(freshRow + 0x42), &zero, 2);
        std::memcpy(reinterpret_cast<void*>(freshRow + 0x44), &one, 2);
        Verify(Bind::AttachCompressed(image, array, generation, 2, childFrames, 1, false, &Owner, &receipt, &Write) != ClockResult::Ok && !receipt.entered,
               "nonzero existing count is not fresh even with a null pointer");
        std::memcpy(reinterpret_cast<void*>(freshRow + 0x44), &zero, 2);
        std::memcpy(reinterpret_cast<void*>(freshRow + 0x48), &childBase, sizeof(childBase));
        Verify(Bind::AttachCompressed(image, array, generation, 2, childFrames, 1, false, &Owner, &receipt, &Write) != ClockResult::Ok && !receipt.entered,
               "existing pointer is not fresh even with count zero");
        std::memcpy(reinterpret_cast<void*>(freshRow + 0x48), &nullFrames, sizeof(nullFrames));
        PoolTestHost::ownerResult = ClockResult::WrongThread;
        Verify(Bind::AttachCompressed(image, array, generation, 2, childFrames, 1, false, &Owner, &receipt, &Write) != ClockResult::Ok && !receipt.entered &&
                   bindWrites == before,
               "wrong callback owner cannot create a private row link");
        PoolTestHost::ownerResult = ClockResult::Ok;
        Verify(Bind::AttachCompressed(image, array, generation + 1, 2, childFrames, 1, false, &Owner, &receipt, &Write) != ClockResult::Ok &&
                   Bind::AttachCompressed(image, array, generation, 2, array, 1, false, &Owner, &receipt, &Write) != ClockResult::Ok && bindWrites == before,
               "foreign generation and an array-shaped substitute cannot adopt frame allocation authority");
        Check(Arrays::FreeStorage(image, array, generation, &Owner) == ClockResult::Ok &&
                  Frames::FreeStorage(image, rootFrames, generation, &Owner) == ClockResult::Ok &&
                  Frames::FreeStorage(image, childFrames, generation, &Owner) == ClockResult::Ok &&
                  Frames::FreeStorage(image, smallFrames, generation, &Owner) == ClockResult::Ok,
              "retire only each original private array/frame record");
    }
    for (const unsigned failure : {1u, 2u})
    {
        reset();
        Storage         array, frames;
        Arrays::Receipt allocated;
        Check(Arrays::Allocate(image, array, generation, 1, &Owner, &allocated) == ClockResult::Ok, "prepare one fresh failure row");
        const auto row = allocated.readyTag;
        Check(Frames::Allocate(image, frames, generation, 10, &Owner, &allocated) == ClockResult::Ok, "prepare one original child frame");
        const auto base = allocated.readyTag;
        mode = failure;
        Bind::Receipt receipt;
        Verify(Bind::AttachCompressed(image, array, generation, 0, frames, 1, false, &Owner, &receipt, &Write) != ClockResult::Ok && receipt.entered &&
                   receipt.accepted == (failure == 2) && !receipt.verified && bindWrites == 1 && *reinterpret_cast<std::uintptr_t*>(row + 0x48) == base &&
                   *reinterpret_cast<std::uint16_t*>(row + 0x42) == 5 && array.GetState() == Storage::State::FaultDebt && array.CanFreeStorage() &&
                   host.Contains(base) && !host.freeCalls,
               "unknown or accepted guarded-store failure keeps real row links and original frame journals without rewind");
        PoolTestHost::ownerResult = ClockResult::Ok;
        mode = 0;
        Verify(Bind::AttachCompressed(image, array, generation, 0, frames, 1, false, &Owner, &receipt, &Write) != ClockResult::Ok && bindWrites == 1,
               "entered fresh-binding failure cannot replay");
        Check(Arrays::FreeStorage(image, array, generation, &Owner) == ClockResult::Ok &&
                  Frames::FreeStorage(image, frames, generation, &Owner) == ClockResult::Ok,
              "ordinary known private resources remain eligible for storage-only recovery");
    }
    Heap::active = nullptr;
    std::puts("Production native fresh compressed animation frame fixture completed");
    std::printf("Passed %u native fresh compressed animation frame checks (64-bit; no native engine code executed)\n", bindChecks);
    return 0;
}
