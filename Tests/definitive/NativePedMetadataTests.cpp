/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Qualified raw model/type observations over real native pool pages
 ****************************************************************************/
#define main NativePedMetadataTransformFixtureMain
#include "NativePedTransformReaderTests.cpp"
#undef main
#include "../../Client/game_de/NativePedMetadataMemory.h"
#include "../../Client/game_de/NativePedMetadataProfile.h"
#include <thread>

// The same owned OS image/pool utility used by generic vitals retains all
// section, page, native GT/GC/state and pool checks. No native method or model
// resolver is substituted: this read-only slice never resolves a model bank.
#define GetModuleHandleW  PoolTestHost::Module
#define ReadProcessMemory PoolTestHost::Read
#include "../../Client/game_de/NativeGameStateMemory.cpp"
#include "../../Client/game_de/NativeStartGameCalls.cpp"
#include "../../Client/game_de/NativePedMetadataMemory.cpp"
#undef ReadProcessMemory
#undef GetModuleHandleW

namespace
{
    struct MetadataFixture
    {
        TransformFixture transform;
        explicit MetadataFixture(const std::filesystem::path& directory) : transform(directory)
        {
            auto&              image = transform.image;
            IMAGE_NT_HEADERS64 nt{};
            std::memcpy(&nt, reinterpret_cast<void*>(image.image + 0x80), sizeof(nt));
            nt.FileHeader.NumberOfSections = 3;
            std::array<IMAGE_SECTION_HEADER, 3> sections{};
            const std::array<std::uintptr_t, 4> boundaries{0x1000, 0x3f40000, 0x5000000, (NativePedPoolProfile::ImageSize + 0xfff) & ~std::uintptr_t{0xfff}};
            for (std::size_t i = 0; i < sections.size(); ++i)
            {
                auto& section = sections[i];
                std::memcpy(section.Name, i == 0 ? ".text" : i == 1 ? ".rdata" : ".data", i == 1 ? 6 : 5);
                section.VirtualAddress = static_cast<DWORD>(boundaries[i]);
                section.Misc.VirtualSize = static_cast<DWORD>(boundaries[i + 1] - boundaries[i]);
                section.SizeOfRawData = 0x200;
                section.PointerToRawData = static_cast<DWORD>(0x400 + i * 0x200);
                section.Characteristics = IMAGE_SCN_MEM_READ | (i == 0 ? IMAGE_SCN_CNT_CODE | IMAGE_SCN_MEM_EXECUTE
                                                                       : IMAGE_SCN_CNT_UNINITIALIZED_DATA | (i == 2 ? IMAGE_SCN_MEM_WRITE : 0));
            }
            image.Patch(0x80, &nt, sizeof(nt), PAGE_READONLY);
            image.Patch(0x80 + sizeof(nt), sections.data(), sizeof(sections), PAGE_READONLY);
            for (const auto& pin : NativePedMetadataProfile::Code)
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
        void Reset()
        {
            transform.Reset(false);
            Store(0, 0, transform.image.image + std::uintptr_t{0x4318270});
            Store(8, 0, transform.image.image + std::uintptr_t{0x4319730});
            for (const auto slot : {std::size_t{0}, std::size_t{8}})
            {
                Store(slot, 0x3a, std::uint16_t{0x1234});
                Store(slot, 0x6a, static_cast<unsigned char>(0xfb));
                Store(slot, 0x5f0, std::uintptr_t{0});
                Store(slot, 0x7e0, std::int32_t{-1});
            }
            // These observations come from genuine occupied pool slots. No
            // current-player pointer, selector or model lookup grants access.
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
            Check(reader.Resolve(handle, value) == ClockResult::Ok, "metadata requires a real occupied native slot and issued handle observation");
            return value;
        }
    };
    NativePedMetadata MetadataSentinel()
    {
        NativePedMetadata value;
        value.modelIndex = 0xcafe;
        value.entityType = 0xab;
        return value;
    }
    bool MetadataUntouched(const NativePedMetadata& value)
    {
        return value.modelIndex == 0xcafe && value.entityType == 0xab;
    }
    MetadataFixture* changingMetadata = nullptr;
    unsigned         metadataChange = 0;
    void             ChangeMetadata()
    {
        if (metadataChange == 0)
            changingMetadata->Store(0, 0x3a, std::uint16_t{0x4321});
        if (metadataChange == 1)
            changingMetadata->Store(0, 0x6a, static_cast<unsigned char>(0xfa));
        if (metadataChange == 2)
            changingMetadata->transform.pool.flags[0] = 2;
        if (metadataChange == 3)
            PoolTestHost::ownerResult = ClockResult::Expired;
    }
}

int main(int argc, char** argv)
{
    using Memory = NativePedMetadataMemory;
    Check(argc == 2 && std::filesystem::path(argv[1]).is_absolute(), "native metadata evidence uses an explicit workspace output directory");
    Memory::Binding refused{123};
    Check(!Memory::Qualify(refused) && refused.image == 123, "shipping native metadata qualification refuses this actual non-GTA executable");
    MetadataFixture fixture(argv[1]);
    PoolTestHost::image = fixture.transform.image.image;
    const Memory::Binding binding{fixture.transform.image.image};
    Memory                memory(binding, &PoolTestHost::Owner), unowned(binding, nullptr);
    Check(Memory::Revalidate(binding), "exact native generic/player model and type producers qualify only on real owned executable image pages");
    const auto reset = [&]()
    {
        fixture.Reset();
        return fixture.Ped();
    };
    auto ped = reset();
    struct DefaultAccess final : NativePedMetadataAccess
    {
    } defaultAccess;
    auto output = MetadataSentinel();
    Check(defaultAccess.ReadPedMetadata(NativePedReferenceToken{}, output) == ClockResult::NotInitialized && MetadataUntouched(output),
          "the optional internal metadata family refuses without an actual retained Runtime token owner");
    constexpr std::array<std::uint16_t, 6> models{0, 1, 0x7fff, 0x8000, 0xfffe, 0xffff};
    for (const auto slot : {std::size_t{0}, std::size_t{8}})
        for (const auto model : models)
        {
            fixture.Reset();
            const auto handle = slot == 0 ? std::uint32_t{1} : std::uint32_t{(8u << 8) | 127u};
            fixture.Store(slot, 0x3a, model);
            fixture.Store(slot, 0x6a, static_cast<unsigned char>(slot == 0 ? 0x3b : 0xfb));
            ped = fixture.Ped(handle);
            output = MetadataSentinel();
            Check(memory.Read(ped, output) == ClockResult::Ok && output.modelIndex == model && output.entityType == 3,
                  "both exact known ped classes expose the complete uint16 model value and only the native type byte's lower three bits");
        }
    // FFFF remains a raw model observation. No valid model bank is seeded and
    // no model-info pointer is returned or dereferenced by this prerequisite.
    ped = reset();
    output = MetadataSentinel();
    Check(unowned.Read(ped, output) == ClockResult::NotInitialized && MetadataUntouched(output),
          "an occupied native slot and readable fields cannot replace callback ownership or consume caller output");
    for (unsigned guard = 0; guard < 11; ++guard)
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
            fixture.Global(NativePedPoolProfile::RootRva, std::uintptr_t{0});
        if (guard == 6)
            fixture.transform.pool.flags[0] = 0x81;
        if (guard == 7)
            fixture.transform.pool.flags[0] = 2;
        if (guard == 8)
            fixture.Store(0, 0x6a, static_cast<unsigned char>(0xfa));
        if (guard == 9)
        {
            fixture.Store(0, 0, fixture.transform.image.image + std::uintptr_t{0x1300010});
            ped = fixture.Ped();
        }
        if (guard == 10)
            fixture.transform.pool.Put(0, std::uintptr_t{0});
        output = MetadataSentinel();
        Check(memory.Read(ped, output) != ClockResult::Ok && MetadataUntouched(output),
              "native thread/GC/state, exact occupied slot, known type/class and current callback all guard output publication");
    }
    for (metadataChange = 0; metadataChange < 4; ++metadataChange)
    {
        ped = reset();
        changingMetadata = &fixture;
        PoolTestHost::watched = ped.nativePed + 0x3a;
        PoolTestHost::after = &ChangeMetadata;
        output = MetadataSentinel();
        Check(memory.Read(ped, output) != ClockResult::Ok && MetadataUntouched(output) && !PoolTestHost::after,
              "model/type byte coherence, pool generation and callback authority are bookended before exposing metadata");
    }
    ped = reset();
    PoolTestHost::failAt = PoolTestHost::reads + 1;
    output = MetadataSentinel();
    Check(memory.Read(ped, output) != ClockResult::Ok && MetadataUntouched(output), "a failed actual image read preserves both metadata output fields");
    ped = reset();
    ClockResult foreignThread = ClockResult::Ok;
    bool        foreignOutputPreserved = false;
    std::thread foreign(
        [&]()
        {
            auto value = MetadataSentinel();
            foreignThread = memory.Read(ped, value);
            foreignOutputPreserved = MetadataUntouched(value);
        });
    foreign.join();
    Check(foreignThread != ClockResult::Ok && foreignOutputPreserved, "a genuine foreign host thread cannot reuse the admitted native GameThread observation");
    ped = reset();
    const auto& pin = NativePedMetadataProfile::Code[0];
    const auto  changed = static_cast<unsigned char>(pin.bytes[0] ^ 1);
    fixture.transform.image.Patch(pin.rva, &changed, 1, PAGE_EXECUTE_READ);
    output = MetadataSentinel();
    Check(memory.Read(ped, output) != ClockResult::Ok && MetadataUntouched(output),
          "foreign exact native field-producer bytes revoke the assumed metadata layout without publishing fallback values");
    fixture.transform.image.Patch(pin.rva, pin.bytes, 1, PAGE_EXECUTE_READ);
    PoolTestHost::image = 0;
    output = MetadataSentinel();
    Check(memory.Read(ped, output) != ClockResult::Ok && MetadataUntouched(output), "a changed primary image cannot reuse an earlier native ped observation");
    std::puts("Production native ped metadata primitive fixture completed");
    std::printf("Passed %u native ped-metadata checks (64-bit; no native engine code executed)\n", checks);
    return 0;
}
