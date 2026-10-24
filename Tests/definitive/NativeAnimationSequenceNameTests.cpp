/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Bounded raw names preserve physical neighbours and native journals
 ****************************************************************************/
#pragma push_macro("main")
#undef main
#define main HistoricalSequenceNameHostMain
#include "NativeAnimationSequenceCopyFrameTests.cpp"
#undef main
#pragma pop_macro("main")
#include "../../Client/game_de/NativeAnimationSequenceNameCalls.h"
#include "../../Client/game_de/NativeAnimationSequenceNameProfile.h"
namespace SequenceNameTestHost
{
    namespace Calls = NativeAnimationSequenceNameCalls;
    namespace Heap = NativeCustomAnimSequenceStorageFixture;
    unsigned nameChecks = 0, nameWrites = 0, mode = 0;
    void     Verify(bool value, const char* reason)
    {
        ++nameChecks;
        Check(value, reason);
    }
    ClockResult Owner() noexcept
    {
        return PoolTestHost::Owner();
    }
    ClockResult Write(std::uintptr_t row, const Calls::Name& name, std::size_t terminatedBytes, bool projectBone, std::uint16_t bone,
                      std::uint32_t& exception) noexcept
    {
        ++nameWrites;
        const auto result = mta::game_de::CustomAnimSequenceStorageDetail::WriteName(row, name, terminatedBytes, projectBone, bone, exception);
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
    Calls::Record Snapshot(std::uintptr_t row)
    {
        Calls::Record value;
        std::memcpy(value.bytes.data(), reinterpret_cast<void*>(row), value.bytes.size());
        return value;
    }
}
int main(int argc, char** argv)
{
    using namespace SequenceNameTestHost;
    namespace Arrays = NativeCustomAnimSequenceStorageCalls;
    namespace Frames = NativeAnimationKeyFrameStorageCalls;
    using Storage = NativeCustomAnimSequenceStorage;
    constexpr std::uint64_t generation = 0x100000071ULL;
    Check(argc == 2, "name fixture owns its explicit output directory");
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
        nameWrites = mode = 0;
        for (const auto& pin : NativeAnimationSequenceRowProfile::Code)
            SpecialFixture::Patch(&fixture, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : NativeAnimationSequenceNameProfile::Code)
            SpecialFixture::Patch(&fixture, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
    };
    reset();
    {
        Storage         array, frames;
        Arrays::Receipt allocated;
        Check(Arrays::Allocate(image, array, generation, 2, &Owner, &allocated) == ClockResult::Ok, "issue actual current array record");
        const auto row = allocated.readyTag + 80;
        Check(Frames::Allocate(image, frames, generation, 41, &Owner, &allocated) == ClockResult::Ok, "issue independent native frame record");
        const auto          frameBase = allocated.readyTag;
        const auto          frame = frameBase + 4;
        const std::uint16_t oldBone = 0x7b7b, flags = 0x2208, count = 7;
        std::memcpy(reinterpret_cast<void*>(row + 0x40), &oldBone, 2);
        std::memcpy(reinterpret_cast<void*>(row + 0x42), &flags, 2);
        std::memcpy(reinterpret_cast<void*>(row + 0x44), &count, 2);
        std::memcpy(reinterpret_cast<void*>(row + 0x48), &frame, sizeof(frame));
        Calls::Name name{};
        const char  spelling[] = {'m', 'i', 'X', static_cast<char>(0xe9), 0};
        std::memcpy(name.data(), spelling, sizeof(spelling));
        auto expected = Snapshot(row);
        std::memcpy(expected.bytes.data(), spelling, sizeof(spelling));
        Calls::Receipt receipt;
        Verify(Calls::Set(image, array, generation, 1, name, 0xa1b21234, &Owner, &receipt, &Write) == ClockResult::Ok && receipt.entered && receipt.accepted &&
                   receipt.rawWritten && receipt.verified && receipt.row == row && nameWrites == 1 && Snapshot(row).bytes == expected.bytes,
               "real writer preserves spelling/high bytes and writes only the NUL-terminated prefix, not transport tail or inactive bone");
        const std::uint16_t selectedFlags = flags | 0x10;
        std::memcpy(reinterpret_cast<void*>(row + 0x42), &selectedFlags, 2);
        expected = Snapshot(row);
        const std::uint16_t projected = 0xabcd;
        std::memcpy(expected.bytes.data() + 0x40, &projected, 2);
        Verify(Calls::Set(image, array, generation, 1, name, 0x7654abcd, &Owner, &receipt, &Write) == ClockResult::Ok &&
                   Snapshot(row).bytes == expected.bytes && host.Contains(frameBase) && !host.freeCalls,
               "physical selector10 projects only logical hash low16 while preserving every flag, frame and original allocation record");
        name.fill(static_cast<char>(0xe1));
        name.back() = 0;
        expected = Snapshot(row);
        std::memcpy(expected.bytes.data(), name.data(), name.size());
        const std::uint16_t maximumBone = 0x1234;
        std::memcpy(expected.bytes.data() + 0x40, &maximumBone, 2);
        Verify(Calls::Set(image, array, generation, 1, name, 0x98761234, &Owner, &receipt, &Write) == ClockResult::Ok && Snapshot(row).bytes == expected.bytes,
               "the bounded domain includes exactly sixty-three raw nonzero bytes plus NUL without case folding or truncation");
        name.fill('Q');
        name[0] = 0;
        expected = Snapshot(row);
        expected.bytes[0] = 0;
        const std::uint16_t emptyBone = 0xffff;
        std::memcpy(expected.bytes.data() + 0x40, &emptyBone, 2);
        Verify(Calls::Set(image, array, generation, 1, name, 0xffffffff, &Owner, &receipt, &Write) == ClockResult::Ok && Snapshot(row).bytes == expected.bytes,
               "empty name changes only its first NUL and selected bone, retaining the complete old native name tail");
        name.fill('Z');
        const auto before = nameWrites;
        const auto untouched = Snapshot(row);
        Verify(Calls::Set(image, array, generation, 1, name, 0, &Owner, &receipt, &Write) != ClockResult::Ok && !receipt.entered && nameWrites == before &&
                   Snapshot(row).bytes == untouched.bytes,
               "name64 without NUL refuses before stores rather than inventing a sixty-fourth-byte read or truncation");
        name[0] = 0;
        PoolTestHost::ownerResult = ClockResult::WrongThread;
        Verify(Calls::Set(image, array, generation, 1, name, 0, &Owner, &receipt, &Write) != ClockResult::Ok && !receipt.entered && nameWrites == before,
               "wrong callback owner cannot invoke the CPU writer");
        PoolTestHost::ownerResult = ClockResult::Ok;
        Verify(Calls::Set(image, array, generation, (std::numeric_limits<std::size_t>::max)(), name, 0, &Owner, &receipt, &Write) != ClockResult::Ok &&
                   Calls::Set(image, frames, generation, 0, name, 0, &Owner, &receipt, &Write) != ClockResult::Ok && nameWrites == before,
               "full-width row index and wrong storage kind grant no name authority");
        Check(Arrays::FreeStorage(image, array, generation, &Owner) == ClockResult::Ok &&
                  Frames::FreeStorage(image, frames, generation, &Owner) == ClockResult::Ok,
              "storage cleanup retains its original array/frame owners");
    }
    for (const unsigned failure : {1u, 2u})
    {
        reset();
        Storage         array;
        Arrays::Receipt allocated;
        Check(Arrays::Allocate(image, array, generation, 1, &Owner, &allocated) == ClockResult::Ok, "prepare current name failure target");
        const auto  row = allocated.readyTag;
        Calls::Name name{};
        std::memcpy(name.data(), "Name", 5);
        mode = failure;
        Calls::Receipt receipt;
        Verify(Calls::Set(image, array, generation, 0, name, 0x12345678, &Owner, &receipt, &Write) != ClockResult::Ok && receipt.entered &&
                   receipt.accepted == (failure == 2) && receipt.rawWritten == (failure == 2) && !receipt.verified && nameWrites == 1 &&
                   !std::memcmp(reinterpret_cast<void*>(row), "Name", 5) && array.GetState() == Storage::State::FaultDebt && array.CanFreeStorage(),
               "unknown or accepted write failure preserves actual name stores and precise acknowledgement without rewinding native storage");
        mode = 0;
        PoolTestHost::ownerResult = ClockResult::Ok;
        Verify(Calls::Set(image, array, generation, 0, name, 0x12345678, &Owner, &receipt, &Write) != ClockResult::Ok && nameWrites == 1,
               "entered failure cannot replay a name write");
        Check(Arrays::FreeStorage(image, array, generation, &Owner) == ClockResult::Ok,
              "known private name mutation debt retires by ordinary storage-only free");
    }
    Heap::active = nullptr;
    std::puts("Production native animation sequence name fixture completed");
    std::printf("Passed %u native animation sequence name checks (64-bit; no native engine code executed)\n", nameChecks);
    return 0;
}
