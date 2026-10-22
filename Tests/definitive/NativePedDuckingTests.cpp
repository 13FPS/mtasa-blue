/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Genuine ducking-bit queries over owned occupied generic/player peds
 ****************************************************************************/
#pragma push_macro("main")
#undef main
#define main NativePedDuckingHistoricalTransformMain
#include "NativePedTransformReaderTests.cpp"
#undef main
#pragma pop_macro("main")
#include "../../Client/game_de/NativePedMetadataMemory.h"
#include "../../Client/game_de/NativePedMetadataProfile.h"
#include "../../Client/game_de/NativePedDuckingMemory.h"
#include "../../Client/game_de/NativePedDuckingProfile.h"
#include <thread>
namespace DuckingHost
{
    std::uintptr_t flagsAddress = 0;
    unsigned       flagsReads = 0, failFlagRead = 0;
    BOOL WINAPI    Read(HANDLE process, LPCVOID input, LPVOID output, SIZE_T bytes, SIZE_T* copied)
    {
        if (reinterpret_cast<std::uintptr_t>(input) == flagsAddress && bytes == sizeof(std::uint32_t) && ++flagsReads == failFlagRead)
        {
            if (copied)
                *copied = 0;
            return FALSE;
        }
        return PoolTestHost::Read(process, input, output, bytes, copied);
    }
}
// Existing real image/pool utilities compile once. Every authority/profile/
// field check is production code; only copied-byte faults are scheduled here.
#define GetModuleHandleW  PoolTestHost::Module
#define ReadProcessMemory PoolTestHost::Read
#include "../../Client/game_de/NativeGameStateMemory.cpp"
#include "../../Client/game_de/NativeStartGameCalls.cpp"
#include "../../Client/game_de/NativePedMetadataMemory.cpp"
#undef ReadProcessMemory
#define ReadProcessMemory DuckingHost::Read
#include "../../Client/game_de/NativePedDuckingMemory.cpp"
#undef ReadProcessMemory
#undef GetModuleHandleW
namespace
{
    namespace DP = NativePedDuckingProfile;
    struct DuckingFixture
    {
        TransformFixture transform;
        explicit DuckingFixture(const std::filesystem::path& directory) : transform(directory)
        {
            auto&              image = transform.image;
            IMAGE_NT_HEADERS64 nt{};
            std::memcpy(&nt, reinterpret_cast<void*>(image.image + 0x80), sizeof(nt));
            nt.FileHeader.NumberOfSections = 3;
            std::array<IMAGE_SECTION_HEADER, 3> sections{};
            const std::array<std::uintptr_t, 4> boundaries{0x1000, 0x3f40000, 0x5000000, (NativePedPoolProfile::ImageSize + 0xfff) & ~std::uintptr_t{0xfff}};
            for (std::size_t index = 0; index != sections.size(); ++index)
            {
                auto& section = sections[index];
                std::memcpy(section.Name, index == 0 ? ".text" : index == 1 ? ".rdata" : ".data", index == 1 ? 6 : 5);
                section.VirtualAddress = static_cast<DWORD>(boundaries[index]);
                section.Misc.VirtualSize = static_cast<DWORD>(boundaries[index + 1] - boundaries[index]);
                section.SizeOfRawData = 0x200;
                section.PointerToRawData = static_cast<DWORD>(0x400 + index * 0x200);
                section.Characteristics = IMAGE_SCN_MEM_READ | (index == 0 ? IMAGE_SCN_CNT_CODE | IMAGE_SCN_MEM_EXECUTE
                                                                           : IMAGE_SCN_CNT_UNINITIALIZED_DATA | (index == 2 ? IMAGE_SCN_MEM_WRITE : 0));
            }
            image.Patch(0x80, &nt, sizeof(nt), PAGE_READONLY);
            image.Patch(0x80 + sizeof(nt), sections.data(), sizeof(sections), PAGE_READONLY);
            for (const auto& pin : NativePedMetadataProfile::Code)
                image.Patch(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
            for (const auto& pin : DP::Code)
                image.Patch(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
            for (const auto& pin : NativeGameStateProfile::pins)
                image.Patch(pin.rva, pin.bytes, pin.size, pin.code ? PAGE_EXECUTE_READ : PAGE_READONLY);
            for (const auto& pin : NativeBorrowedGcProfile::pins)
                image.Patch(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
            for (const auto& pin : NativeStartGameProfile::Code)
                if (pin.rva == 0xab879c || pin.rva == 0xab92f0 || pin.rva == 0x1af4ef0)
                    image.Patch(pin.rva, pin.bytes.data(), pin.size, PAGE_EXECUTE_READ);
            Reset();
        }
        template <class T>
        void Global(std::uintptr_t rva, const T& value)
        {
            transform.image.Patch(rva, &value, sizeof(value), PAGE_READWRITE);
        }
        template <class T>
        void Store(std::size_t slot, std::size_t offset, const T& value)
        {
            std::memcpy(transform.pool.objects + slot * NativePedPoolProfile::ObjectStride + offset, &value, sizeof(value));
        }
        std::uint32_t Flags(std::size_t slot = 0) const
        {
            std::uint32_t value = 0;
            std::memcpy(&value, transform.pool.objects + slot * NativePedPoolProfile::ObjectStride + DP::FlagsOffset, sizeof(value));
            return value;
        }
        void Reset()
        {
            transform.Reset(false);
            for (const auto slot : {std::size_t{0}, std::size_t{8}})
            {
                Store(slot, 0, transform.image.image + (slot == 0 ? DP::PedVtableRva : DP::PlayerVtableRva));
                Store(slot, 0x3a, std::uint16_t{0xffff});
                Store(slot, 0x6a, static_cast<unsigned char>(0xfb));
                Store(slot, DP::FlagsOffset - 4, std::uint32_t{0x12345678});
                Store(slot, DP::FlagsOffset, std::uint32_t{0});
                Store(slot, DP::FlagsOffset + 4, std::uint32_t{0x87654321});
            }
            Global(NativeStartGameProfile::ThreadInitializedRva, static_cast<unsigned char>(1));
            Global(NativeStartGameProfile::ThreadIdRva, GetCurrentThreadId());
            Global(NativeBorrowedGcProfile::markerRva, LONG{0});
            Global(NativeGameStateProfile::stateRva, std::uint32_t{9});
            // Actual occupied generic/player slots do not require any current
            // player selector, task, intelligence object or valid model bank.
            Global(0x53eb720, std::uintptr_t{0});
            Global(0x522bcf6, static_cast<unsigned char>(0xff));
            DuckingHost::flagsAddress = reinterpret_cast<std::uintptr_t>(transform.pool.objects) + DP::FlagsOffset;
            DuckingHost::flagsReads = DuckingHost::failFlagRead = 0;
        }
        NativePedPoolReader::Value Ped(std::uint32_t handle = 1)
        {
            NativePedPoolReader::Value value;
            Check(NativePedPoolReader({transform.image.image}, &PoolTestHost::Owner).Resolve(handle, value) == ClockResult::Ok,
                  "ducking query consumes a genuine occupied native handle observation");
            return value;
        }
    };
    DuckingFixture* changingDucking = nullptr;
    unsigned        duckingChange = 0;
    void            ChangeDucking()
    {
        auto& f = *changingDucking;
        switch (duckingChange)
        {
            case 0:
                f.Store(0, DP::FlagsOffset, f.Flags() ^ DP::DuckingMask);
                break;
            case 1:
                f.transform.pool.flags[0] = 2;
                break;
            case 2:
                PoolTestHost::ownerResult = ClockResult::Expired;
                break;
            case 3:
                f.Global(NativeBorrowedGcProfile::markerRva, LONG{1});
                break;
            case 4:
                f.Store(0, DP::FlagsOffset, f.Flags() ^ std::uint32_t{0x80000001});
                break;
        }
    }
}
int main(int argc, char** argv)
{
    using Memory = NativePedDuckingMemory;
    Check(argc == 2 && std::filesystem::path(argv[1]).is_absolute(), "native ducking evidence uses an explicit workspace output directory");
    Memory::Binding refused{123};
    Check(!Memory::Qualify(refused) && refused.image == 123, "real wrong-executable qualification preserves the caller binding");
    DuckingFixture fixture(argv[1]);
    changingDucking = &fixture;
    PoolTestHost::image = fixture.transform.image.image;
    const Memory::Binding binding{fixture.transform.image.image};
    Memory                memory(binding, &PoolTestHost::Owner), unowned(binding, nullptr);
    const auto            reset = [&]
    {
        fixture.Reset();
        return fixture.Ped();
    };
    Check(Memory::Revalidate(binding), "native named ducking and actual common generic/player field producers qualify together");
    struct DefaultAccess final : NativePedDuckingAccess
    {
    } access;
    for (bool prior : {false, true})
    {
        bool output = prior;
        Check(access.ReadPedDucking(NativePedReferenceToken{}, output) == ClockResult::NotInitialized && output == prior,
              "default access refuses without converting absent authority into successful false ducking");
    }
    const std::array<std::uint32_t, 8> seeds{0, DP::DuckingMask, ~DP::DuckingMask, 0xffffffff, 1, 0x80000000, 0xa5a5a5a5, 0x5a5a5a5a};
    for (const auto slot : {std::size_t{0}, std::size_t{8}})
        for (const auto flags : seeds)
        {
            fixture.Reset();
            fixture.Store(slot, DP::FlagsOffset, flags);
            const auto ped = fixture.Ped(slot == 0 ? 1 : (8u << 8) | 127u);
            bool       output = (flags & DP::DuckingMask) == 0;
            Check(memory.ReadDucking(ped, output) == ClockResult::Ok && output == ((flags & DP::DuckingMask) != 0) && fixture.Flags(slot) == flags,
                  "both admitted classes expose exactly named bit26 across unrelated flag patterns without native writes");
        }
    auto ped = reset();
    for (bool prior : {false, true})
    {
        bool output = prior;
        Check(unowned.ReadDucking(ped, output) == ClockResult::NotInitialized && output == prior,
              "an occupied native ped and readable flags do not substitute for actual callback ownership");
    }
    for (unsigned guard = 0; guard != 10; ++guard)
    {
        ped = reset();
        switch (guard)
        {
            case 0:
                PoolTestHost::ownerResult = ClockResult::Expired;
                break;
            case 1:
                fixture.Global(NativeStartGameProfile::ThreadInitializedRva, static_cast<unsigned char>(0));
                break;
            case 2:
                fixture.Global(NativeStartGameProfile::ThreadIdRva, GetCurrentThreadId() + 1);
                break;
            case 3:
                fixture.Global(NativeBorrowedGcProfile::markerRva, LONG{1});
                break;
            case 4:
                fixture.Global(NativeGameStateProfile::stateRva, std::uint32_t{7});
                break;
            case 5:
                fixture.Global(NativePedPoolProfile::RootRva, std::uintptr_t{0});
                break;
            case 6:
                fixture.transform.pool.flags[0] = 0x81;
                break;
            case 7:
                fixture.transform.pool.flags[0] = 2;
                break;
            case 8:
                fixture.Store(0, 0x6a, static_cast<unsigned char>(0xfa));
                break;
            case 9:
                fixture.Store(0, 0, fixture.transform.image.image + std::uintptr_t{0x1300010});
                ped = fixture.Ped();
                break;
        }
        for (bool prior : {false, true})
        {
            bool output = prior;
            Check(memory.ReadDucking(ped, output) != ClockResult::Ok && output == prior,
                  "typed occupied identity and native owner/thread/GC/state failures preserve both possible caller booleans");
        }
    }
    for (duckingChange = 0; duckingChange != 5; ++duckingChange)
    {
        ped = reset();
        fixture.Store(0, DP::FlagsOffset, DP::DuckingMask);
        PoolTestHost::watched = ped.nativePed + DP::FlagsOffset;
        PoolTestHost::after = &ChangeDucking;
        bool       output = false;
        const auto result = memory.ReadDucking(ped, output);
        if (duckingChange == 4)
            Check(result == ClockResult::Ok && output && !PoolTestHost::after && fixture.Flags() == (DP::DuckingMask ^ 0x80000001),
                  "changing unrelated flag bits does not fabricate a ducking transition or overwrite neighboring state");
        else
            Check(result != ClockResult::Ok && !output && !PoolTestHost::after,
                  "ducking transition or lost slot/owner/GC between genuine flag reads refuses without stale output");
    }
    for (unsigned failure = 1; failure <= 2; ++failure)
    {
        ped = reset();
        DuckingHost::failFlagRead = failure;
        bool output = true;
        Check(memory.ReadDucking(ped, output) == ClockResult::ReadFailure && output,
              "either actual flag DWORD copy can fail without a fake successful zero result");
    }
    for (const auto& pin : DP::Code)
    {
        ped = reset();
        const auto bad = static_cast<unsigned char>(pin.bytes[0] ^ 1);
        fixture.transform.image.Patch(pin.rva, &bad, 1, PAGE_EXECUTE_READ);
        bool output = true;
        Check(memory.ReadDucking(ped, output) != ClockResult::Ok && output, "named ducking consumer and actual common initializer pins are mandatory");
        fixture.transform.image.Patch(pin.rva, pin.bytes, 1, PAGE_EXECUTE_READ);
    }
    ped = reset();
    ClockResult foreign = ClockResult::Ok;
    bool        preserved = false;
    std::thread other(
        [&]
        {
            bool output = true;
            foreign = memory.ReadDucking(ped, output);
            preserved = output;
        });
    other.join();
    Check(foreign != ClockResult::Ok && preserved, "foreign thread cannot borrow the current native ped query authority");
    ped = reset();
    PoolTestHost::image = 0;
    bool output = true;
    Check(memory.ReadDucking(ped, output) != ClockResult::Ok && output, "a changed current module revokes prior ped ducking observations");
    std::puts("Production native ped ducking primitive fixture completed");
    std::printf("Passed %u native ped-ducking checks (64-bit; no native engine code executed)\n", checks);
    return 0;
}
