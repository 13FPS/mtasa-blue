/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual immediate task-bank reads on owned image and native-sized pages
 ****************************************************************************/
#define main NativeTaskPoolTransformFixtureMain
#include "NativePedTransformReaderTests.cpp"
#undef main
#include "../../Client/game_de/NativeTaskPoolReader.h"
#include "../../Client/game_de/NativeTaskPoolProfile.h"
#include <thread>
#include <tuple>

// Only module/read selection is controlled. No native task virtual function,
// allocator or deletion code is executed by this real checked reader fixture.
#define GetModuleHandleW  PoolTestHost::Module
#define ReadProcessMemory PoolTestHost::Read
#include "../../Client/game_de/NativeGameStateMemory.cpp"
#include "../../Client/game_de/NativeStartGameCalls.cpp"
#include "../../Client/game_de/NativeTaskPoolReader.cpp"
#undef ReadProcessMemory
#undef GetModuleHandleW

namespace
{
    namespace T = NativeTaskPoolProfile;
    using Reader = NativeTaskPoolReader;
    struct TaskPool
    {
        unsigned char *header = nullptr, *objects = nullptr, *flags = nullptr;
        TaskPool()
        {
            header = static_cast<unsigned char*>(VirtualAlloc(nullptr, 32, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
            objects = static_cast<unsigned char*>(VirtualAlloc(nullptr, T::NativeCapacity * T::ObjectStride, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
            flags = static_cast<unsigned char*>(VirtualAlloc(nullptr, T::NativeCapacity, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
            Check(header && objects && flags, "own the complete2500*A8 task bank, native header and all generation flags");
            Reset();
        }
        template <class V>
        void Header(std::size_t offset, const V& value)
        {
            std::memcpy(header + offset, &value, sizeof(value));
        }
        template <class V>
        void Store(std::size_t slot, std::size_t offset, const V& value)
        {
            std::memcpy(objects + slot * T::ObjectStride + offset, &value, sizeof(value));
        }
        std::uintptr_t Address(std::size_t slot = 0) const { return reinterpret_cast<std::uintptr_t>(objects + slot * T::ObjectStride); }
        void           Reset()
        {
            Header(0, reinterpret_cast<std::uintptr_t>(objects));
            Header(8, reinterpret_cast<std::uintptr_t>(flags));
            Header(0x10, static_cast<std::int32_t>(T::NativeCapacity));
            Header(0x14, std::int32_t{-1});
            Header(0x18, static_cast<unsigned char>(1));
            std::memset(objects, 0, T::NativeCapacity * T::ObjectStride);
            std::memset(flags, 0x80, T::NativeCapacity);
        }
        ~TaskPool()
        {
            if (header)
                VirtualFree(header, 0, MEM_RELEASE);
            if (objects)
                VirtualFree(objects, 0, MEM_RELEASE);
            if (flags)
                VirtualFree(flags, 0, MEM_RELEASE);
        }
    };
    struct TaskFixture
    {
        TransformFixture transform;
        TaskPool         tasks;
        explicit TaskFixture(const std::filesystem::path& directory) : transform(directory)
        {
            auto&              image = transform.image;
            IMAGE_NT_HEADERS64 nt{};
            std::memcpy(&nt, reinterpret_cast<void*>(image.image + 0x80), sizeof(nt));
            nt.FileHeader.NumberOfSections = 3;
            std::array<IMAGE_SECTION_HEADER, 3> sections{};
            const std::array<std::uintptr_t, 4> bounds{0x1000, 0x3f40000, 0x5000000, (NativePedPoolProfile::ImageSize + 0xfff) & ~std::uintptr_t{0xfff}};
            for (std::size_t i = 0; i < sections.size(); ++i)
            {
                auto& section = sections[i];
                std::memcpy(section.Name, i == 0 ? ".text" : i == 1 ? ".rdata" : ".data", i == 1 ? 6 : 5);
                section.VirtualAddress = static_cast<DWORD>(bounds[i]);
                section.Misc.VirtualSize = static_cast<DWORD>(bounds[i + 1] - bounds[i]);
                section.SizeOfRawData = 0x200;
                section.PointerToRawData = static_cast<DWORD>(0x400 + i * 0x200);
                section.Characteristics = IMAGE_SCN_MEM_READ | (i == 0 ? IMAGE_SCN_CNT_CODE | IMAGE_SCN_MEM_EXECUTE
                                                                       : IMAGE_SCN_CNT_UNINITIALIZED_DATA | (i == 2 ? IMAGE_SCN_MEM_WRITE : 0));
            }
            image.Patch(0x80, &nt, sizeof(nt), PAGE_READONLY);
            image.Patch(0x80 + sizeof(nt), sections.data(), sizeof(sections), PAGE_READONLY);
            for (const auto& pin : T::Code)
                image.Patch(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
            for (const auto& slot : T::Slots)
            {
                const auto dtor = image.image + slot.deletingDestructor, type = image.image + slot.getType, simple = image.image + slot.isSimple;
                image.Patch(slot.vtable, &dtor, sizeof(dtor), PAGE_READONLY);
                image.Patch(slot.vtable + T::TypeSlot, &type, sizeof(type), PAGE_READONLY);
                image.Patch(slot.vtable + T::SimpleSlot, &simple, sizeof(simple), PAGE_READONLY);
            }
            for (const auto& pin : NativeGameStateProfile::pins)
                image.Patch(pin.rva, pin.bytes, pin.size, pin.code ? PAGE_EXECUTE_READ : PAGE_READONLY);
            for (const auto& pin : NativeBorrowedGcProfile::pins)
                image.Patch(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
            for (const auto& pin : NativeStartGameProfile::Code)
                if (pin.rva == 0xab879c || pin.rva == 0xab92f0 || pin.rva == 0x1af4ef0)
                    image.Patch(pin.rva, pin.bytes.data(), pin.size, PAGE_EXECUTE_READ);
            Reset();
        }
        template <class V>
        void Global(std::uintptr_t rva, const V& value)
        {
            transform.image.Patch(rva, &value, sizeof(value), PAGE_READWRITE);
        }
        void Reset()
        {
            transform.Reset(false);
            tasks.Reset();
            tasks.Store(0, 0, transform.image.image + T::Slots[0].vtable);
            tasks.flags[0] = 1;
            tasks.Store(17, 0, transform.image.image + T::Slots[1].vtable);
            tasks.flags[17] = 127;
            tasks.Store(T::NativeCapacity - 1, 0, transform.image.image + T::Slots[0].vtable);
            tasks.flags[T::NativeCapacity - 1] = 0;
            Global(T::RootRva, reinterpret_cast<std::uintptr_t>(tasks.header));
            Global(NativeStartGameProfile::ThreadInitializedRva, static_cast<unsigned char>(1));
            Global(NativeStartGameProfile::ThreadIdRva, GetCurrentThreadId());
            Global(NativeBorrowedGcProfile::markerRva, LONG{0});
            Global(NativeGameStateProfile::stateRva, std::uint32_t{9});
            // This task read has no dependence on a current-player row.
            Global(0x53eb720, std::uintptr_t{0});
            Global(0x522bcf6, static_cast<unsigned char>(0xff));
        }
        std::vector<unsigned char> Snapshot() const
        {
            std::vector<unsigned char> bytes(32 + T::NativeCapacity * (T::ObjectStride + 1));
            std::memcpy(bytes.data(), tasks.header, 32);
            std::memcpy(bytes.data() + 32, tasks.objects, T::NativeCapacity * T::ObjectStride);
            std::memcpy(bytes.data() + 32 + T::NativeCapacity * T::ObjectStride, tasks.flags, T::NativeCapacity);
            return bytes;
        }
    };
    Reader::Value Sentinel()
    {
        Reader::Value v;
        v.pool = 1;
        v.objects = 2;
        v.flags = 3;
        v.nativeTask = 4;
        v.vtable = 5;
        v.deletingDestructor = 6;
        v.getTypeMethod = 7;
        v.isSimpleMethod = 8;
        v.slot = 99;
        v.nativeType = 88;
        v.capacity = 77;
        v.cursor = -9;
        v.generation = 0x55;
        v.ownsAllocations = 0x44;
        v.simple = true;
        return v;
    }
    bool Untouched(const Reader::Value& a)
    {
        const auto b = Sentinel();
        return std::tie(a.pool, a.objects, a.flags, a.nativeTask, a.vtable, a.deletingDestructor, a.getTypeMethod, a.isSimpleMethod, a.slot, a.nativeType,
                        a.capacity, a.cursor, a.generation, a.ownsAllocations,
                        a.simple) == std::tie(b.pool, b.objects, b.flags, b.nativeTask, b.vtable, b.deletingDestructor, b.getTypeMethod, b.isSimpleMethod,
                                              b.slot, b.nativeType, b.capacity, b.cursor, b.generation, b.ownsAllocations, b.simple);
    }
    TaskFixture* changingTask = nullptr;
    unsigned     taskChange = 0;
    void         ChangeTask()
    {
        auto& fixture = *changingTask;
        if (taskChange == 0)
            fixture.tasks.flags[0] = 2;
        if (taskChange == 1)
            fixture.tasks.Header(0x14, std::int32_t{5});
        if (taskChange == 2)
            fixture.Global(T::RootRva, std::uintptr_t{0});
        if (taskChange == 3)
            fixture.tasks.Store(0, 0, fixture.transform.image.image + T::Slots[1].vtable);
        if (taskChange == 4)
            PoolTestHost::ownerResult = ClockResult::Expired;
    }
}
int main(int argc, char** argv)
{
    Check(argc == 2 && std::filesystem::path(argv[1]).is_absolute(), "task-bank proof uses explicit workspace evidence directory");
    Reader::Binding refused{123};
    Check(!Reader::Qualify(refused) && refused.image == 123, "shipping task qualifier refuses the actual non-GTA fixture executable");
    TaskFixture fixture(argv[1]);
    PoolTestHost::image = fixture.transform.image.image;
    const Reader::Binding binding{fixture.transform.image.image};
    Reader                reader(binding, &PoolTestHost::Owner), unowned(binding, nullptr);
    Check(Reader::Revalidate(binding), "task reader qualifies exact image/header, task initializer/allocator/destructor and both real constant-method vtables");
    for (const auto slot : {std::size_t{0}, std::size_t{17}, T::NativeCapacity - 1})
    {
        fixture.Reset();
        const auto    before = fixture.Snapshot();
        Reader::Value value;
        const auto&   descriptor = T::Slots[slot == 17 ? 1 : 0];
        Check(reader.Read(fixture.tasks.Address(slot), value) == ClockResult::Ok && value.slot == slot && value.nativeTask == fixture.tasks.Address(slot) &&
                  value.capacity == 2500 && value.ownsAllocations == 1 && value.cursor == -1 &&
                  value.objects == reinterpret_cast<std::uintptr_t>(fixture.tasks.objects) &&
                  value.flags == reinterpret_cast<std::uintptr_t>(fixture.tasks.flags) && value.vtable == binding.image + descriptor.vtable &&
                  value.deletingDestructor == binding.image + descriptor.deletingDestructor && value.getTypeMethod == binding.image + descriptor.getType &&
                  value.isSimpleMethod == binding.image + descriptor.isSimple && value.nativeType == descriptor.nativeType &&
                  value.simple == descriptor.simple && value.generation == fixture.tasks.flags[slot] && reader.Recheck(value) == ClockResult::Ok,
              "both exact ctor classes and the final native2500th row expose only qualified opaque identity/native type/simple facts");
        Check(before == fixture.Snapshot(), "successful task observations execute no native task call and change no header, object or flag byte");
    }
    fixture.Reset();
    auto output = Sentinel();
    Check(unowned.Read(fixture.tasks.Address(), output) == ClockResult::NotInitialized && Untouched(output),
          "pool occupancy never replaces mandatory real callback admission");
    for (const auto address : {std::uintptr_t{0}, fixture.tasks.Address() - 1, fixture.tasks.Address() + 1, fixture.tasks.Address(T::NativeCapacity),
                               reinterpret_cast<std::uintptr_t>(fixture.transform.matrix)})
    {
        output = Sentinel();
        Check(reader.Read(address, output) != ClockResult::Ok && Untouched(output),
              "null,interior,out-of-bank and external heap-like addresses cannot become native task rows");
    }
    for (unsigned guard = 0; guard < 14; ++guard)
    {
        fixture.Reset();
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
            fixture.Global(T::RootRva, std::uintptr_t{0});
        if (guard == 6)
            fixture.tasks.Header(0, std::uintptr_t{0});
        if (guard == 7)
            fixture.tasks.Header(8, std::uintptr_t{0});
        if (guard == 8)
            fixture.tasks.Header(0x10, std::int32_t{2499});
        if (guard == 9)
            fixture.tasks.Header(0x14, std::int32_t{2500});
        if (guard == 10)
            fixture.tasks.Header(0x18, static_cast<unsigned char>(0));
        if (guard == 11)
            fixture.tasks.flags[0] = 0x81;
        if (guard == 12)
            fixture.tasks.Store(0, 0, binding.image + std::uintptr_t{0x431c570});
        if (guard == 13)
            fixture.tasks.Store(0, 0, std::uintptr_t{0});
        output = Sentinel();
        Check(reader.Read(fixture.tasks.Address(), output) != ClockResult::Ok && Untouched(output),
              "task callback/nativeGT/GC/state,complete header, occupied flag and exact constructor-vtable all remain mandatory");
    }
    fixture.Reset();
    Reader::Value previous;
    Check(reader.Read(fixture.tasks.Address(), previous) == ClockResult::Ok, "capture the first native task generation for an immediate recheck");
    fixture.tasks.flags[0] = 2;
    Check(reader.Recheck(previous) == ClockResult::BindingChanged, "same-address later native generation never reuses the earlier opaque observation");
    for (taskChange = 0; taskChange < 5; ++taskChange)
    {
        fixture.Reset();
        changingTask = &fixture;
        PoolTestHost::watched = fixture.tasks.Address();
        PoolTestHost::after = &ChangeTask;
        output = Sentinel();
        Check(reader.Read(fixture.tasks.Address(), output) != ClockResult::Ok && Untouched(output),
              "between-copy generation,cursor,root,vtable or owner changes refuse before publishing caller output");
    }
    fixture.Reset();
    ClockResult foreignResult = ClockResult::Ok;
    std::thread foreign(
        [&]()
        {
            auto untouched = Sentinel();
            foreignResult = reader.Read(fixture.tasks.Address(), untouched);
            if (!Untouched(untouched))
                foreignResult = ClockResult::Ok;
        });
    foreign.join();
    Check(foreignResult != ClockResult::Ok, "a real different host thread cannot borrow initialized native GameThread authority");
    fixture.Reset();
    PoolTestHost::failAt = PoolTestHost::reads + 1;
    output = Sentinel();
    Check(reader.Read(fixture.tasks.Address(), output) != ClockResult::Ok && Untouched(output), "actual copied-byte failures preserve every output member");
    fixture.Reset();
    DWORD old = 0, ignored = 0;
    auto* lastPage = fixture.tasks.objects + ((T::NativeCapacity * T::ObjectStride - 1) & ~std::size_t{0xfff});
    Check(VirtualProtect(lastPage, 1, PAGE_NOACCESS, &old) != FALSE, "guard the actual final bank page while querying the first row");
    output = Sentinel();
    const auto truncated = reader.Read(fixture.tasks.Address(), output);
    Check(VirtualProtect(lastPage, 1, old, &ignored) != FALSE, "restore final owned task-bank page after refusal");
    Check(truncated != ClockResult::Ok && Untouched(output), "first-row access still validates the complete2500*A8 native allocation bound");
    fixture.Reset();
    const auto& pin = T::Code[1];
    const auto  changed = static_cast<unsigned char>(pin.bytes[0] ^ 1);
    fixture.transform.image.Patch(pin.rva, &changed, 1, PAGE_EXECUTE_READ);
    output = Sentinel();
    Check(reader.Read(fixture.tasks.Address(), output) != ClockResult::Ok && Untouched(output),
          "foreign allocator code cannot inherit earlier task layout qualification");
    fixture.transform.image.Patch(pin.rva, pin.bytes, 1, PAGE_EXECUTE_READ);
    const auto wrong = binding.image + T::Slots[1].getType;
    fixture.transform.image.Patch(T::Slots[0].vtable + T::TypeSlot, &wrong, sizeof(wrong), PAGE_READONLY);
    output = Sentinel();
    Check(reader.Read(fixture.tasks.Address(), output) != ClockResult::Ok && Untouched(output),
          "a changed native method slot cannot silently change the task's qualified constant type");
    const auto right = binding.image + T::Slots[0].getType;
    fixture.transform.image.Patch(T::Slots[0].vtable + T::TypeSlot, &right, sizeof(right), PAGE_READONLY);
    PoolTestHost::image = 0;
    output = Sentinel();
    Check(reader.Read(fixture.tasks.Address(), output) != ClockResult::Ok && Untouched(output),
          "changed current-image identity refuses without exposing a saved task pointer");
    std::puts("Production native task pool primitive fixture completed");
    std::printf("Passed %u native task-pool checks (64-bit; no native engine code executed)\n", checks);
    return 0;
}
