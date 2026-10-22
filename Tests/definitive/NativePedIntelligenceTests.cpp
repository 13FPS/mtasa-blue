/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual checked intelligence/embedded-manager observations on owned pages
 ****************************************************************************/
#define main NativePedIntelligenceTransformFixtureMain
#include "NativePedTransformReaderTests.cpp"
#undef main
#include "../../Client/game_de/NativePedIntelligenceReader.h"
#include "../../Client/game_de/NativePedIntelligenceProfile.h"
#include <thread>
#include <tuple>

// Reuse the real OS image/pool fixture. Only module selection and precise read
// faults are substituted; the production reader still qualifies all native
// owner/GT/GC/state/profile/occupied-pool facts before publishing a snapshot.
#define GetModuleHandleW  PoolTestHost::Module
#define ReadProcessMemory PoolTestHost::Read
#include "../../Client/game_de/NativeGameStateMemory.cpp"
#include "../../Client/game_de/NativeStartGameCalls.cpp"
#include "../../Client/game_de/NativePedIntelligenceReader.cpp"
#undef ReadProcessMemory
#undef GetModuleHandleW

namespace
{
    namespace I = NativePedIntelligenceProfile;
    using IntelligenceReader = NativePedIntelligenceReader;
    struct IntelligencePool
    {
        unsigned char *header = nullptr, *objects = nullptr, *flags = nullptr;
        IntelligencePool()
        {
            header = static_cast<unsigned char*>(VirtualAlloc(nullptr, 32, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
            objects = static_cast<unsigned char*>(VirtualAlloc(nullptr, I::NativeCapacity * I::ObjectStride, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
            flags = static_cast<unsigned char*>(VirtualAlloc(nullptr, I::NativeCapacity, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
            Check(header && objects && flags, "own separate native-sized intelligence pool header/object/flag allocations");
            Reset();
        }
        template <class T>
        void Header(std::size_t offset, const T& value)
        {
            std::memcpy(header + offset, &value, sizeof(value));
        }
        template <class T>
        void Store(std::size_t slot, std::size_t offset, const T& value)
        {
            std::memcpy(objects + slot * I::ObjectStride + offset, &value, sizeof(value));
        }
        void Reset()
        {
            Header(0, reinterpret_cast<std::uintptr_t>(objects));
            Header(8, reinterpret_cast<std::uintptr_t>(flags));
            Header(0x10, static_cast<std::int32_t>(I::NativeCapacity));
            Header(0x14, std::int32_t{-1});
            Header(0x18, static_cast<unsigned char>(1));
            std::memset(flags, 0x80, I::NativeCapacity);
            std::memset(objects, 0, I::NativeCapacity * I::ObjectStride);
        }
        ~IntelligencePool()
        {
            if (header)
                VirtualFree(header, 0, MEM_RELEASE);
            if (objects)
                VirtualFree(objects, 0, MEM_RELEASE);
            if (flags)
                VirtualFree(flags, 0, MEM_RELEASE);
        }
    };
    struct IntelligenceFixture
    {
        TransformFixture transform;
        IntelligencePool intelligence;
        explicit IntelligenceFixture(const std::filesystem::path& directory) : transform(directory)
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
            for (const auto& pin : I::Code)
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
        void PedStore(std::size_t slot, std::size_t offset, const T& value)
        {
            std::memcpy(transform.pool.objects + slot * NativePedPoolProfile::ObjectStride + offset, &value, sizeof(value));
        }
        std::uintptr_t PedAddress(std::size_t slot = 0) const
        {
            return reinterpret_cast<std::uintptr_t>(transform.pool.objects + slot * NativePedPoolProfile::ObjectStride);
        }
        std::uintptr_t IntelligenceAddress(std::size_t slot = 17) const
        {
            return reinterpret_cast<std::uintptr_t>(intelligence.objects + slot * I::ObjectStride);
        }
        void Associate(std::size_t pedSlot, std::size_t intelligenceSlot, unsigned char generation)
        {
            PedStore(pedSlot, I::IntelligenceOffset, IntelligenceAddress(intelligenceSlot));
            intelligence.flags[intelligenceSlot] = generation;
            intelligence.Store(intelligenceSlot, I::OwnerOffset, PedAddress(pedSlot));
            intelligence.Store(intelligenceSlot, I::ManagerOwnerOffset, PedAddress(pedSlot));
        }
        void Reset()
        {
            transform.Reset(false);
            intelligence.Reset();
            for (const auto pedSlot : {std::size_t{0}, std::size_t{8}})
            {
                PedStore(pedSlot, 0, transform.image.image + (pedSlot == 0 ? I::PedVtableRva : I::PlayerVtableRva));
                PedStore(pedSlot, I::TypeOffset, static_cast<unsigned char>(0xfb));
                PedStore(pedSlot, 0x5f0, std::uintptr_t{0});
                PedStore(pedSlot, 0x7e0, std::int32_t{-1});
            }
            Associate(0, 17, 1);
            Associate(8, I::NativeCapacity - 1, 127);
            Global(I::RootRva, reinterpret_cast<std::uintptr_t>(intelligence.header));
            Global(0x53eb720, std::uintptr_t{0});
            Global(0x522bcf6, static_cast<unsigned char>(0xff));
            Global(NativeStartGameProfile::ThreadInitializedRva, static_cast<unsigned char>(1));
            Global(NativeStartGameProfile::ThreadIdRva, GetCurrentThreadId());
            Global(NativeBorrowedGcProfile::markerRva, LONG{0});
            Global(NativeGameStateProfile::stateRva, std::uint32_t{9});
        }
        NativePedPoolReader::Value Ped(std::uint32_t handle = 1)
        {
            NativePedPoolReader        reader({transform.image.image}, &PoolTestHost::Owner);
            NativePedPoolReader::Value value;
            Check(reader.Resolve(handle, value) == ClockResult::Ok, "intelligence reader input is a real fresh occupied generic/player pool observation");
            return value;
        }
    };
    IntelligenceReader::Value IntelligenceSentinel()
    {
        IntelligenceReader::Value value;
        value.ped.handle = 0xdeadbeef;
        value.ped.nativePed = 0xfeed;
        value.pool = 1;
        value.objects = 2;
        value.flags = 3;
        value.nativeIntelligence = 4;
        value.taskManager = 5;
        value.slot = 777;
        value.capacity = 888;
        value.cursor = -9;
        value.generation = 0x55;
        value.ownsAllocations = 0x44;
        value.rawType = 0xab;
        value.primaryTasks = {11, 12, 13, 14, 15};
        value.secondaryTasks = {21, 22, 23, 24, 25, 26};
        return value;
    }
    bool IntelligenceUntouched(const IntelligenceReader::Value& value)
    {
        const auto expected = IntelligenceSentinel();
        return value.ped.handle == expected.ped.handle && value.ped.nativePed == expected.ped.nativePed &&
               std::tie(value.pool, value.objects, value.flags, value.nativeIntelligence, value.taskManager, value.slot, value.capacity, value.cursor,
                        value.generation, value.ownsAllocations, value.rawType, value.primaryTasks,
                        value.secondaryTasks) == std::tie(expected.pool, expected.objects, expected.flags, expected.nativeIntelligence, expected.taskManager,
                                                          expected.slot, expected.capacity, expected.cursor, expected.generation, expected.ownsAllocations,
                                                          expected.rawType, expected.primaryTasks, expected.secondaryTasks);
    }
    IntelligenceFixture* changingIntelligence = nullptr;
    unsigned             intelligenceChange = 0;
    void                 ChangeIntelligence()
    {
        auto& fixture = *changingIntelligence;
        if (intelligenceChange == 0)
            fixture.PedStore(0, I::IntelligenceOffset, fixture.IntelligenceAddress(18));
        if (intelligenceChange == 1)
            fixture.intelligence.flags[17] = 2;
        if (intelligenceChange == 2)
            fixture.intelligence.Store(17, I::OwnerOffset, fixture.PedAddress(8));
        if (intelligenceChange == 3)
            fixture.intelligence.Store(17, I::ManagerOwnerOffset, fixture.PedAddress(8));
        if (intelligenceChange == 4)
            fixture.intelligence.Store(17, I::PrimaryOffset, std::uintptr_t{0x4321});
        if (intelligenceChange == 5)
            fixture.intelligence.Store(17, I::SecondaryOffset, std::uintptr_t{0x9871});
        if (intelligenceChange == 6)
            fixture.intelligence.Header(0x14, std::int32_t{42});
        if (intelligenceChange == 7)
            fixture.transform.pool.flags[0] = 2;
        if (intelligenceChange == 8)
            PoolTestHost::ownerResult = ClockResult::Expired;
    }
}

int main(int argc, char** argv)
{
    Check(argc == 2 && std::filesystem::path(argv[1]).is_absolute(), "intelligence evidence stays in an explicit workspace-owned directory");
    IntelligenceReader::Binding refused{123};
    Check(!IntelligenceReader::Qualify(refused) && refused.image == 123,
          "shipping intelligence admission refuses the real non-GTA host without changing output");
    IntelligenceFixture fixture(argv[1]);
    PoolTestHost::image = fixture.transform.image.image;
    const IntelligenceReader::Binding binding{fixture.transform.image.image};
    IntelligenceReader                reader(binding, &PoolTestHost::Owner), unowned(binding, nullptr);
    Check(IntelligenceReader::Revalidate(binding),
          "complete independent intelligence pool/constructor/manager-array native code pins qualify on real owned image pages");
    const auto reset = [&]
    {
        fixture.Reset();
        return fixture.Ped();
    };
    auto ped = reset();
    auto value = IntelligenceSentinel();
    Check(unowned.Read(ped, value) == ClockResult::NotInitialized && IntelligenceUntouched(value),
          "native pool identity does not supply a callback owner or consume output");

    for (const auto pedSlot : {std::size_t{0}, std::size_t{8}})
        for (unsigned char generation : {static_cast<unsigned char>(0), static_cast<unsigned char>(1), static_cast<unsigned char>(127)})
        {
            fixture.Reset();
            const auto intSlot = pedSlot == 0 ? std::size_t{17} : I::NativeCapacity - 1;
            fixture.intelligence.flags[intSlot] = generation;
            const auto handle = pedSlot == 0 ? std::uint32_t{1} : std::uint32_t{(8u << 8) | 127u};
            ped = fixture.Ped(handle);
            for (std::size_t index = 0; index != 5; ++index)
                fixture.intelligence.Store(intSlot, I::PrimaryOffset + index * sizeof(std::uintptr_t), std::uintptr_t{0x1001} + index * 2);
            for (std::size_t index = 0; index != 6; ++index)
                fixture.intelligence.Store(intSlot, I::SecondaryOffset + index * sizeof(std::uintptr_t), std::uintptr_t{0xffff000000000001ull} + index * 2);
            value = IntelligenceSentinel();
            Check(reader.Read(ped, value) == ClockResult::Ok && value.ped.nativePed == fixture.PedAddress(pedSlot) && value.slot == intSlot &&
                      value.nativeIntelligence == fixture.IntelligenceAddress(intSlot) && value.taskManager == value.nativeIntelligence + I::ManagerOffset &&
                      value.generation == generation && value.rawType == 0xfb && value.capacity == static_cast<std::int32_t>(I::NativeCapacity),
                  "both genuine ped classes expose their independently occupied intelligence row and embedded manager including generation zero");
            for (std::size_t index = 0; index != 5; ++index)
                Check(value.primaryTasks[index] == std::uintptr_t{0x1001} + index * 2,
                      "all five primary task cells remain opaque, including unaligned unmapped addresses");
            for (std::size_t index = 0; index != 6; ++index)
                Check(value.secondaryTasks[index] == std::uintptr_t{0xffff000000000001ull} + index * 2,
                      "all six secondary task cells are copied without treating an unreadable pointer as a native task lifetime");
            Check(reader.Recheck(value) == ClockResult::Ok, "immediate coherent identity/task-array snapshot can be rechecked without retaining task leases");
        }
    for (unsigned guard = 0; guard != 18; ++guard)
    {
        ped = reset();
        if (guard == 0)
            PoolTestHost::ownerResult = ClockResult::Expired;
        if (guard == 1)
            fixture.Global(NativeStartGameProfile::ThreadInitializedRva, static_cast<unsigned char>(0));
        if (guard == 2)
            fixture.Global(NativeStartGameProfile::ThreadIdRva, GetCurrentThreadId() + 1);
        if (guard == 3)
            fixture.Global(NativeBorrowedGcProfile::markerRva, LONG{1});
        if (guard == 4)
            fixture.Global(NativeGameStateProfile::stateRva, std::uint32_t{7});
        if (guard == 5)
            fixture.Global(I::RootRva, std::uintptr_t{0});
        if (guard == 6)
            fixture.intelligence.Header(0x10, std::int32_t{999});
        if (guard == 7)
            fixture.intelligence.Header(0x14, std::int32_t{1000});
        if (guard == 8)
            fixture.intelligence.Header(0x18, static_cast<unsigned char>(0));
        if (guard == 9)
            fixture.intelligence.flags[17] = 0x81;
        if (guard == 10)
            fixture.PedStore(0, I::IntelligenceOffset, fixture.IntelligenceAddress(17) + 8);
        if (guard == 11)
            fixture.PedStore(0, I::IntelligenceOffset, fixture.IntelligenceAddress(I::NativeCapacity));
        if (guard == 12)
            fixture.intelligence.Store(17, I::OwnerOffset, fixture.PedAddress(8));
        if (guard == 13)
            fixture.intelligence.Store(17, I::ManagerOwnerOffset, fixture.PedAddress(8));
        if (guard == 14)
            fixture.PedStore(0, I::TypeOffset, static_cast<unsigned char>(0xfa));
        if (guard == 15)
            fixture.transform.pool.flags[0] = 2;
        if (guard == 16)
            fixture.PedStore(0, 0, binding.image + std::uintptr_t{0x1300010});
        if (guard == 17)
            fixture.PedStore(0, I::IntelligenceOffset, std::uintptr_t{0});
        value = IntelligenceSentinel();
        Check(reader.Read(ped, value) != ClockResult::Ok && IntelligenceUntouched(value),
              "each actual callback/thread/GC/state/pool/header/row/owner/type guard preserves output");
        PoolTestHost::ownerResult = ClockResult::Ok;
    }
    ped = reset();
    const std::uintptr_t foreignVtableRva = 0x4319000;
    const auto           firstMethod = binding.image + NativePedPoolProfile::Code[1].rva;
    fixture.transform.image.Patch(foreignVtableRva, &firstMethod, sizeof(firstMethod), PAGE_READONLY);
    fixture.PedStore(0, 0, binding.image + foreignVtableRva);
    auto foreignPed = fixture.Ped();
    value = IntelligenceSentinel();
    Check(reader.Read(foreignPed, value) == ClockResult::UnsupportedBuild && IntelligenceUntouched(value),
          "a different otherwise coherent occupied native class cannot impersonate either qualified ped implementation");
    ped = reset();
    Check(reader.Read(ped, value) == ClockResult::Ok, "seed a complete real observed tuple before stale input checks");
    fixture.intelligence.flags[17] = 2;
    Check(reader.Recheck(value) != ClockResult::Ok, "same intelligence address with a new occupied generation cannot reuse an old snapshot");
    ped = reset();
    Check(reader.Read(ped, value) == ClockResult::Ok, "seed real opaque task cells before native task replacement");
    fixture.intelligence.Store(17, I::SecondaryOffset + 5 * sizeof(std::uintptr_t), std::uintptr_t{123});
    Check(reader.Recheck(value) != ClockResult::Ok, "replacement in the last actual secondary cell invalidates the full immediate tuple");

    ped = reset();
    PoolTestHost::reads = PoolTestHost::ownerCalls = 0;
    Check(reader.Read(ped, value) == ClockResult::Ok, "measure actual guarded OS-read/owner boundaries of the production reader");
    const auto reads = PoolTestHost::reads, owners = PoolTestHost::ownerCalls;
    for (unsigned failure = 1; failure <= reads; ++failure)
    {
        PoolTestHost::reads = PoolTestHost::ownerCalls = 0;
        PoolTestHost::failAt = failure;
        value = IntelligenceSentinel();
        Check(reader.Read(ped, value) != ClockResult::Ok && IntelligenceUntouched(value),
              "every actual bounded image/pool/owner/array read failure preserves caller output");
    }
    PoolTestHost::failAt = 0;
    for (unsigned refusal = 1; refusal <= owners; ++refusal)
    {
        PoolTestHost::reads = PoolTestHost::ownerCalls = 0;
        PoolTestHost::revokeAt = refusal;
        value = IntelligenceSentinel();
        Check(reader.Read(ped, value) == ClockResult::WrongThread && IntelligenceUntouched(value),
              "each real owner bookend can revoke without publishing an intelligence graph");
    }
    PoolTestHost::revokeAt = 0;
    changingIntelligence = &fixture;
    for (intelligenceChange = 0; intelligenceChange != 9; ++intelligenceChange)
    {
        ped = reset();
        // The production reader copies the complete owner+manager prefix in
        // one real OS read. Mutate immediately after that actual copy so the
        // later identity/header checks or second snapshot must notice it.
        PoolTestHost::watched = fixture.IntelligenceAddress(17);
        PoolTestHost::after = &ChangeIntelligence;
        value = IntelligenceSentinel();
        Check(reader.Read(ped, value) != ClockResult::Ok && IntelligenceUntouched(value),
              "changing selectors, row generations, owner pairs, tasks, pool cursor or callback scope cannot publish a hybrid tuple");
        Check(PoolTestHost::after == nullptr, "each coherence scenario reaches the actual bounded native prefix-read boundary");
        PoolTestHost::after = nullptr;
        PoolTestHost::ownerResult = ClockResult::Ok;
    }
    PoolTestHost::watched = 0;
    ped = reset();
    DWORD previous = 0, ignored = 0;
    Check(VirtualProtect(fixture.intelligence.objects, I::NativeCapacity * I::ObjectStride, PAGE_NOACCESS, &previous) != FALSE,
          "revoke only owned intelligence object pages");
    value = IntelligenceSentinel();
    Check(reader.Read(ped, value) != ClockResult::Ok && IntelligenceUntouched(value),
          "unreadable intelligence allocation cannot fall back to ambient player data");
    Check(VirtualProtect(fixture.intelligence.objects, I::NativeCapacity * I::ObjectStride, previous, &ignored) != FALSE,
          "restore only owned intelligence object pages");
    const auto&   pin = I::Code.back();
    unsigned char saved = 0;
    std::memcpy(&saved, reinterpret_cast<void*>(binding.image + pin.rva), 1);
    const auto changed = static_cast<unsigned char>(saved ^ 1);
    fixture.transform.image.Patch(pin.rva, &changed, 1, PAGE_EXECUTE_READ);
    value = IntelligenceSentinel();
    Check(reader.Read(ped, value) != ClockResult::Ok && IntelligenceUntouched(value), "changed qualified constructor/task consumer code refuses output");
    fixture.transform.image.Patch(pin.rva, &saved, 1, PAGE_EXECUTE_READ);
    ClockResult foreignResult = ClockResult::Ok;
    auto        foreignOutput = IntelligenceSentinel();
    std::thread foreign([&] { foreignResult = reader.Read(ped, foreignOutput); });
    foreign.join();
    Check(foreignResult != ClockResult::Ok && IntelligenceUntouched(foreignOutput),
          "native GameThread identity remains mandatory even when the supplied host owner callback itself allows a foreign thread");
    PoolTestHost::image = 0;
    value = IntelligenceSentinel();
    Check(reader.Read(ped, value) != ClockResult::Ok && IntelligenceUntouched(value),
          "changed actual main-image identity cannot reuse an old intelligence binding");
    fixture.Global(I::RootRva, std::uintptr_t{0});
    std::puts("Production native ped intelligence primitive fixture completed");
    std::printf("Passed %u native ped-intelligence checks (64-bit; no native engine code executed)\n", checks);
    return 0;
}
