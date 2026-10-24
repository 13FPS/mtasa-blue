/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual produced-row writes, shared metadata and frame view fences
 ****************************************************************************/
#pragma push_macro("main")
#undef main
#define main HistoricalCustomSequenceRowsHostMain
#include "NativeStreamingSpecialModelTests.cpp"
#undef main
#pragma pop_macro("main")
#include "NativeCustomAnimSequenceStorageFixture.h"
#include "../../Client/game_de/NativeAnimationSequenceRowCalls.h"
#include "../../Client/game_de/NativeAnimationSequenceRowProfile.h"
namespace SequenceRowTestHost
{
    namespace Rows = NativeAnimationSequenceRowCalls;
    namespace Fixture = NativeCustomAnimSequenceStorageFixture;
    unsigned    writes = 0, mode = 0, nativeChecks = 0;
    ClockResult Write(std::uintptr_t, Rows::Operation, const Rows::Value&, std::uint32_t&) noexcept;
    void        CheckNative(bool value, const char* why)
    {
        ++nativeChecks;
        Check(value, why);
    }
    ClockResult Owner() noexcept
    {
        return PoolTestHost::Owner();
    }
}
#define GetModuleHandleW                          PoolTestHost::Module
#define ReadProcessMemory                         PoolTestHost::Read
#define MTA_DE_CUSTOM_ANIM_SEQUENCE_MALLOC_INVOKE ::NativeCustomAnimSequenceStorageFixture::Malloc
#define MTA_DE_CUSTOM_ANIM_SEQUENCE_FREE_INVOKE   ::NativeCustomAnimSequenceStorageFixture::Free
#define MTA_DE_ANIMATION_SEQUENCE_ROW_WRITE       ::SequenceRowTestHost::Write
#include "../../Client/game_de/NativeCustomAnimSequenceStorage.cpp"
#undef MTA_DE_ANIMATION_SEQUENCE_ROW_WRITE
#undef MTA_DE_CUSTOM_ANIM_SEQUENCE_FREE_INVOKE
#undef MTA_DE_CUSTOM_ANIM_SEQUENCE_MALLOC_INVOKE
#undef ReadProcessMemory
#undef GetModuleHandleW
namespace SequenceRowTestHost
{
    ClockResult Write(std::uintptr_t row, Rows::Operation operation, const Rows::Value& target, std::uint32_t& exception) noexcept
    {
        ++writes;
        const auto result = mta::game_de::CustomAnimSequenceStorageDetail::WriteRow(row, operation, target, exception);
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
    using namespace SequenceRowTestHost;
    namespace Calls = NativeCustomAnimSequenceStorageCalls;
    using Storage = NativeCustomAnimSequenceStorage;
    constexpr std::uint64_t generation = 47;
    Check(argc == 2, "row fixture owns its explicit output directory");
    SpecialFixture fixture(argv[1]);
    Fixture::State host;
    Fixture::active = &host;
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
        writes = mode = 0;
        for (const auto& pin : NativeAnimationSequenceRowProfile::Code)
            SpecialFixture::Patch(&fixture, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
    };
    reset();
    {
        Storage        array, frames;
        Calls::Receipt allocation;
        Check(Calls::Allocate(image, array, generation, 2, &Owner, &allocation) == ClockResult::Ok, "prepare actual issued array storage");
        const auto tag = allocation.readyTag;
        Check(NativeAnimationKeyFrameStorageCalls::Allocate(image, frames, generation, 41, &Owner, &allocation) == ClockResult::Ok,
              "prepare actual frame record");
        const auto frameBase = allocation.readyTag;
        CheckNative(frames.Capacity() == 0 && frames.ByteSize() == 41, "frame byte extent remains separate from sequence capacity");
        Rows::Value physical;
        CheckNative(Rows::Read(image, array, generation, 0, &Owner, physical) == ClockResult::Ok && physical.row == tag && physical.boneTag == 0xffff &&
                        !physical.flags && !physical.frameCount && !physical.keyFrames,
                    "physical row view comes from the actually constructed eighty-byte allocation");
        for (const std::int32_t value : {-1, 65535, -2, 65536, (std::numeric_limits<std::int32_t>::min)(), (std::numeric_limits<std::int32_t>::max)()})
        {
            const std::uint16_t flags = 0x2208, bone = 0x1234;
            std::memcpy(reinterpret_cast<void*>(tag + 0x42), &flags, sizeof(flags));
            std::memcpy(reinterpret_cast<void*>(tag + 0x40), &bone, sizeof(bone));
            std::array<unsigned char, 80> expected{};
            std::memcpy(expected.data(), reinterpret_cast<void*>(tag), expected.size());
            const auto          target = static_cast<std::uint16_t>(value);
            const std::uint16_t targetFlags = flags | 0x10;
            if (value != -1)
            {
                std::memcpy(expected.data() + 0x40, &target, 2);
                std::memcpy(expected.data() + 0x42, &targetFlags, 2);
            }
            Rows::Receipt receipt;
            const auto    before = writes;
            CheckNative(Rows::SetBoneTag(image, array, generation, 0, value, &Owner, &receipt) == ClockResult::Ok && receipt.accepted && receipt.verified &&
                            writes == before + (value == -1 ? 0u : 1u) && !std::memcmp(expected.data(), reinterpret_cast<void*>(tag), expected.size()),
                        "real guarded OR-bone writer narrows the full intent, preserves other bits and treats only full -1 as a no-write");
        }
        for (const auto pointer : {frameBase, frameBase + 40})
            CheckNative(Rows::CheckFramePointer(image, frames, generation, pointer, &Owner) == ClockResult::Ok,
                        "current issued frame base and interior qualify without reading payload");
        CheckNative(Rows::CheckFramePointer(image, frames, generation, frameBase + 41, &Owner) != ClockResult::Ok &&
                        Rows::CheckFramePointer(image, frames, generation, tag, &Owner) != ClockResult::Ok,
                    "one-past and unrelated array pointers add no frame authority");
        Rows::Receipt initialized;
        std::memcpy(reinterpret_cast<void*>(tag + 0x48), &frameBase, sizeof(frameBase));
        std::array<unsigned char, 80> initExpected{};
        std::memcpy(initExpected.data(), reinterpret_cast<void*>(tag), initExpected.size());
        const std::uint16_t  unknown = 0xffff, zero = 0;
        const std::uintptr_t noFrames = 0;
        std::memcpy(initExpected.data() + 0x40, &unknown, 2);
        std::memcpy(initExpected.data() + 0x42, &zero, 2);
        std::memcpy(initExpected.data() + 0x44, &zero, 2);
        std::memcpy(initExpected.data() + 0x48, &noFrames, sizeof(noFrames));
        CheckNative(Rows::Initialize(image, array, generation, 0, &Owner, &initialized) == ClockResult::Ok && initialized.accepted &&
                        initialized.after.boneTag == 0xffff && !initialized.after.flags && !initialized.after.frameCount && !initialized.after.keyFrames &&
                        host.Contains(frameBase) && !host.freeCalls && !std::memcmp(initExpected.data(), reinterpret_cast<void*>(tag), initExpected.size()),
                    "actual initializer clears its fields but never silently frees a prior frame allocation");
        PoolTestHost::ownerResult = ClockResult::WrongThread;
        const auto before = writes;
        CheckNative(Rows::Initialize(image, array, generation, 0, &Owner, &initialized) != ClockResult::Ok && !initialized.entered && writes == before,
                    "pre-entry owner refusal performs no native row store");
        PoolTestHost::ownerResult = ClockResult::Ok;
        PoolTestHost::reads = 0;
        Check(Rows::Read(image, array, generation, 0, &Owner, physical) == ClockResult::Ok, "measure only the new successful row observation");
        const auto completeReads = PoolTestHost::reads;
        for (const auto failedRead : {1u, completeReads})
        {
            const Rows::Value sentinel{1, 2, 3, 4, 5};
            physical = sentinel;
            PoolTestHost::reads = 0;
            PoolTestHost::failAt = failedRead;
            CheckNative(Rows::Read(image, array, generation, 0, &Owner, physical) != ClockResult::Ok && physical.row == sentinel.row &&
                            physical.keyFrames == sentinel.keyFrames && physical.boneTag == sentinel.boneTag && physical.flags == sentinel.flags &&
                            physical.frameCount == sentinel.frameCount && array.GetState() == Storage::State::LiveUnpublished,
                        "first and final required row read failures preserve every output field without inventing write debt");
        }
        PoolTestHost::failAt = 0;
        CheckNative(Rows::Read(image, array, generation, (std::numeric_limits<std::size_t>::max)(), &Owner, physical) != ClockResult::Ok &&
                        Rows::Read(image, array, generation + 1, 0, &Owner, physical) != ClockResult::Ok &&
                        Rows::Read(image, frames, generation, 0, &Owner, physical) != ClockResult::Ok,
                    "full-width index, foreign generation and wrong record kind cannot adopt a physical row");
        Check(Calls::FreeStorage(image, array, generation, &Owner) == ClockResult::Ok &&
                  NativeAnimationKeyFrameStorageCalls::FreeStorage(image, frames, generation, &Owner) == ClockResult::Ok,
              "retire prepared private records");
    }
    for (const unsigned failure : {1u, 2u})
    {
        reset();
        Storage        array;
        Calls::Receipt allocation;
        Check(Calls::Allocate(image, array, generation, 1, &Owner, &allocation) == ClockResult::Ok, "prepare private row mutation failure");
        mode = failure;
        Rows::Receipt receipt;
        CheckNative(Rows::SetBoneTag(image, array, generation, 0, 12, &Owner, &receipt) != ClockResult::Ok && receipt.entered && writes == 1 &&
                        receipt.accepted == (failure == 2) && !receipt.verified && array.GetState() == Storage::State::FaultDebt && array.CanFreeStorage(),
                    "real writer completion distinguishes unknown failure from accepted stores with failed post-owner acknowledgement");
        const auto    tag = allocation.readyTag;
        std::uint16_t bone = 0;
        std::memcpy(&bone, reinterpret_cast<void*>(tag + 0x40), sizeof(bone));
        CheckNative(bone == 12, "entered failure preserves actual native stores instead of restoring a snapshot");
        PoolTestHost::ownerResult = ClockResult::Ok;
        mode = 0;
        CheckNative(Rows::SetBoneTag(image, array, generation, 0, 12, &Owner, &receipt) != ClockResult::Ok && writes == 1,
                    "faulted row cannot replay an entered mutation");
        CheckNative(Calls::FreeStorage(image, array, generation, &Owner) == ClockResult::Ok && host.freeCalls == 1,
                    "known unpublished faulted storage retires once through ordinary native free without whole hierarchy cleanup");
    }
    Fixture::active = nullptr;
    std::printf("Passed %u native animation sequence row checks (64-bit; no native engine code executed)\n", nativeChecks);
    std::puts("Production native animation sequence rows fixture completed");
    return 0;
}
