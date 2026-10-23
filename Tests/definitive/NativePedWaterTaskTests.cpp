/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual primary2/type268 reads on owned ped/intelligence/task banks
 ****************************************************************************/
#pragma push_macro("main")
#undef main
#define main NativeWaterTaskHistoricalPoolMain
#include "NativeTaskPoolTests.cpp"
#undef main
#pragma pop_macro("main")
#include "../../Client/game_de/NativePedWaterTaskReader.h"
#include "../../Client/game_de/NativePedWaterTaskProfile.h"
#include "../../Client/game_de/NativePedMetadataProfile.h"

// Only main-image/read selection is substituted. Actual production owner,
// GT/GC/state, PE/page, pool/generation and immutable-class checks execute;
// historical fixture mains compile but are never invoked.
#define GetModuleHandleW  PoolTestHost::Module
#define ReadProcessMemory PoolTestHost::Read
#include "../../Client/game_de/NativePedIntelligenceReader.cpp"
#include "../../Client/game_de/NativePedMetadataMemory.cpp"
#include "../../Client/game_de/NativePedWaterTaskReader.cpp"
#undef ReadProcessMemory
#undef GetModuleHandleW

namespace
{
    namespace W = NativePedWaterTaskProfile;
    namespace I = NativePedIntelligenceProfile;
    using WaterReader = NativePedWaterTaskReader;
    struct WaterIntelligenceBank
    {
        unsigned char *header = nullptr, *objects = nullptr, *flags = nullptr;
        WaterIntelligenceBank()
        {
            header = static_cast<unsigned char*>(VirtualAlloc(nullptr, 32, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
            objects = static_cast<unsigned char*>(VirtualAlloc(nullptr, I::NativeCapacity * I::ObjectStride, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
            flags = static_cast<unsigned char*>(VirtualAlloc(nullptr, I::NativeCapacity, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
            Check(header && objects && flags, "own the complete separate native intelligence bank and generation allocation");
        }
        template <class V>
        void Header(std::size_t offset, V value)
        {
            std::memcpy(header + offset, &value, sizeof(value));
        }
        template <class V>
        void Store(std::size_t offset, V value)
        {
            std::memcpy(objects + offset, &value, sizeof(value));
        }
        void Reset()
        {
            std::memset(header, 0, 32);
            std::memset(objects, 0, I::NativeCapacity * I::ObjectStride);
            std::memset(flags, I::EmptyBit, I::NativeCapacity);
            Header(0, reinterpret_cast<std::uintptr_t>(objects));
            Header(8, reinterpret_cast<std::uintptr_t>(flags));
            Header(0x10, I::NativeCapacity);
            Header(0x14, std::int32_t{-1});
            Header(0x18, static_cast<unsigned char>(1));
            flags[0] = 1;
        }
        ~WaterIntelligenceBank()
        {
            VirtualFree(flags, 0, MEM_RELEASE);
            VirtualFree(objects, 0, MEM_RELEASE);
            VirtualFree(header, 0, MEM_RELEASE);
        }
    };
    struct WaterTaskFixture
    {
        TaskFixture                  task;
        WaterIntelligenceBank        intelligence;
        static constexpr std::size_t WaterSlot = 23;
        explicit WaterTaskFixture(const std::filesystem::path& directory) : task(directory)
        {
            auto& image = task.transform.image;
            for (const auto& pin : I::Code)
                image.Patch(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
            for (const auto& pin : NativePedMetadataProfile::Code)
                image.Patch(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
            // The response table datum occupies native .text alongside code;
            // a read-only-only page would invalidate the real code on it.
            for (const auto& pin : W::Code)
                image.Patch(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
            const auto eventType = image.image + W::EventTypeRva;
            image.Patch(W::EventVtableRva + W::EventTypeSlot, &eventType, sizeof(eventType), PAGE_READONLY);
            Reset();
        }
        std::uintptr_t PedAddress() const { return reinterpret_cast<std::uintptr_t>(task.transform.pool.objects); }
        std::uintptr_t IntelAddress() const { return reinterpret_cast<std::uintptr_t>(intelligence.objects); }
        std::uintptr_t WaterAddress() const { return task.tasks.Address(WaterSlot); }
        void           Primary(std::size_t index, std::uintptr_t address) { intelligence.Store(I::PrimaryOffset + index * sizeof(address), address); }
        void           Reset()
        {
            task.Reset();
            intelligence.Reset();
            task.transform.Store(0, task.transform.image.image + I::PedVtableRva);
            task.transform.Store(NativePedMetadataProfile::ModelOffset, std::uint16_t{17});
            task.transform.Store(NativePedMetadataProfile::TypeOffset, static_cast<unsigned char>(3));
            task.transform.Store(I::IntelligenceOffset, IntelAddress());
            intelligence.Store(I::OwnerOffset, PedAddress());
            intelligence.Store(I::ManagerOwnerOffset, PedAddress());
            task.Global(I::RootRva, reinterpret_cast<std::uintptr_t>(intelligence.header));
            task.tasks.Store(WaterSlot, 0, task.transform.image.image + T::Slots[2].vtable);
            task.tasks.flags[WaterSlot] = 5;
        }
        WaterReader::Ped Ped()
        {
            NativePedPoolReader pool({task.transform.image.image}, &PoolTestHost::Owner);
            WaterReader::Ped    value;
            Check(pool.Resolve(1, value) == ClockResult::Ok, "query input uses the real fresh occupied generic native ped observation");
            // Setup is not part of any injected fault or measured query.
            PoolTestHost::reads = PoolTestHost::ownerCalls = 0;
            return value;
        }
        std::vector<unsigned char> Snapshot() const
        {
            auto bytes = task.Snapshot();
            auto append = [&bytes](const void* p, std::size_t size)
            {
                const auto begin = static_cast<const unsigned char*>(p);
                bytes.insert(bytes.end(), begin, begin + size);
            };
            append(intelligence.header, 32);
            append(intelligence.objects, I::NativeCapacity * I::ObjectStride);
            append(intelligence.flags, I::NativeCapacity);
            append(task.transform.pool.objects, NativePedPoolProfile::NativeCapacity * NativePedPoolProfile::ObjectStride);
            return bytes;
        }
    };
    WaterTaskFixture* changingWaterTask = nullptr;
    unsigned          waterTaskChange = 0;
    void              ChangeWaterTask()
    {
        auto& f = *changingWaterTask;
        if (waterTaskChange == 0)
            f.Primary(W::PrimaryIndex, f.task.tasks.Address());
        if (waterTaskChange == 1)
            f.intelligence.flags[0] = 2;
        if (waterTaskChange == 2)
            f.task.tasks.flags[WaterTaskFixture::WaterSlot] = 6;
        if (waterTaskChange == 3)
            f.task.tasks.Store(WaterTaskFixture::WaterSlot, 0, f.task.transform.image.image + 0x431c5b0);
        if (waterTaskChange == 4)
            f.task.Global(I::RootRva, std::uintptr_t{0});
        if (waterTaskChange == 5)
            PoolTestHost::ownerResult = ClockResult::Expired;
    }
}
int main(int argc, char** argv)
{
    Check(argc == 2 && std::filesystem::path(argv[1]).is_absolute(), "task-based water evidence remains in explicit workspace output");
    WaterReader::Binding wrong{123};
    Check(!WaterReader::Qualify(wrong) && wrong.image == 123, "shipping qualification rejects the actual non-GTA host without publishing a binding");
    WaterTaskFixture fixture(argv[1]);
    PoolTestHost::image = fixture.task.transform.image.image;
    const WaterReader::Binding binding{PoolTestHost::image};
    WaterReader                reader(binding, &PoolTestHost::Owner), unowned(binding, nullptr);
    Check(WaterReader::Revalidate(binding), "native event66, constructor, constant268 leaf, deleting destructor and primary2 installation all qualify");
    for (const auto selected : {std::uintptr_t{0}, fixture.task.tasks.Address(), fixture.task.tasks.Address(17), fixture.WaterAddress()})
    {
        fixture.Reset();
        fixture.Primary(W::PrimaryIndex, selected);
        const auto ped = fixture.Ped();
        const auto before = fixture.Snapshot();
        for (const bool previous : {false, true})
        {
            bool output = previous;
            Check(reader.Read(ped, output) == ClockResult::Ok && output == (selected == fixture.WaterAddress()),
                  "only exact primary2 class268 is true; qualified null and unchanged203/305 classes are false");
        }
        Check(before == fixture.Snapshot(), "query invokes no task virtual and writes no ped, intelligence, task or generation byte");
    }
    for (const auto otherPrimary : {std::size_t{0}, std::size_t{1}, std::size_t{3}, std::size_t{4}})
    {
        fixture.Reset();
        fixture.Primary(otherPrimary, fixture.WaterAddress());
        fixture.intelligence.Store(I::SecondaryOffset, fixture.WaterAddress());
        fixture.task.transform.Store(0x78, std::uint32_t{0x08000100});
        const auto ped = fixture.Ped();
        bool       output = true;
        Check(reader.Read(ped, output) == ClockResult::Ok && !output,
              "water in other primary/secondary cells and both physical water flags never substitute for the SDK primary2 predicate");
    }
    fixture.Reset();
    fixture.Primary(W::PrimaryIndex, fixture.task.tasks.Address(17));
    fixture.task.tasks.Store(17, 0x10, fixture.WaterAddress());
    auto ped = fixture.Ped();
    bool output = true;
    Check(reader.Read(ped, output) == ClockResult::Ok && !output,
          "a268 child beneath a different qualified complex primary task is not the SDK primary-task type predicate");
    fixture.Reset();
    ped = fixture.Ped();
    fixture.task.Global(T::RootRva, std::uintptr_t{0});
    output = true;
    Check(reader.Read(ped, output) == ClockResult::Ok && !output,
          "qualified null primary2 needs no task-bank dereference and does not fabricate a task service");
    for (unsigned refusal = 0; refusal != 14; ++refusal)
    {
        fixture.Reset();
        fixture.Primary(W::PrimaryIndex, fixture.WaterAddress());
        ped = fixture.Ped();
        if (refusal == 0)
            PoolTestHost::ownerResult = ClockResult::Expired;
        if (refusal == 1)
            fixture.task.Global(NativeStartGameProfile::ThreadIdRva, GetCurrentThreadId() + 1);
        if (refusal == 2)
            fixture.task.Global(NativeBorrowedGcProfile::markerRva, LONG{1});
        if (refusal == 3)
            fixture.task.Global(NativeGameStateProfile::stateRva, std::uint32_t{7});
        if (refusal == 4)
            fixture.task.transform.pool.flags[0] = 2;
        if (refusal == 5)
            fixture.task.transform.Store(I::IntelligenceOffset, std::uintptr_t{0});
        if (refusal == 6)
            fixture.intelligence.Store(I::ManagerOwnerOffset, std::uintptr_t{0});
        if (refusal == 7)
            fixture.intelligence.flags[0] = I::EmptyBit;
        if (refusal == 8)
            fixture.task.tasks.flags[WaterTaskFixture::WaterSlot] |= T::EmptyBit;
        if (refusal == 9)
            fixture.task.tasks.Store(WaterTaskFixture::WaterSlot, 0, binding.image + 0x431c5b0);
        if (refusal == 10)
            fixture.Primary(W::PrimaryIndex, fixture.WaterAddress() + 1);
        if (refusal == 11)
            fixture.task.transform.Store(NativePedMetadataProfile::TypeOffset, static_cast<unsigned char>(2));
        if (refusal == 12)
            fixture.task.tasks.Store(WaterTaskFixture::WaterSlot, 0, binding.image + 0x431f048 + 8);
        if (refusal == 13)
            PoolTestHost::failAt = 1;
        for (const bool previous : {false, true})
        {
            output = previous;
            if (refusal == 13)
                PoolTestHost::reads = 0;
            Check(reader.Read(ped, output) != ClockResult::Ok && output == previous,
                  "owner/GT/GC/state/ped/manager/generation/unknown-class and precise read failures preserve either prior bool");
        }
    }
    fixture.Reset();
    ped = fixture.Ped();
    output = true;
    Check(unowned.Read(ped, output) == ClockResult::NotInitialized && output, "native pool occupancy never replaces an actual callback owner");
    for (unsigned change = 0; change != 6; ++change)
    {
        fixture.Reset();
        fixture.Primary(W::PrimaryIndex, fixture.WaterAddress());
        ped = fixture.Ped();
        changingWaterTask = &fixture;
        waterTaskChange = change;
        PoolTestHost::watched = change == 2 || change == 3 ? fixture.WaterAddress() : fixture.IntelAddress();
        PoolTestHost::after = &ChangeWaterTask;
        output = true;
        Check(reader.Read(ped, output) != ClockResult::Ok && output,
              "selection, pool/root, task-generation, destructor-vtable and callback loss during observation never publish a stale bool");
        Check(PoolTestHost::after == nullptr, "each lifetime perturbation actually reaches its single watched production read");
    }
    fixture.Reset();
    fixture.Primary(W::PrimaryIndex, fixture.WaterAddress());
    ped = fixture.Ped();
    const auto& pin = W::Code.back();
    auto        changed = static_cast<unsigned char>(pin.bytes[0] ^ 1);
    fixture.task.transform.image.Patch(pin.rva, &changed, 1, PAGE_EXECUTE_READ);
    output = false;
    Check(reader.Read(ped, output) == ClockResult::UnsupportedBuild && !output,
          "changing the native event-to-primary2 installation proof refuses instead of preserving an assumed classic slot");
    fixture.task.transform.image.Patch(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
    fixture.Reset();
    fixture.Primary(W::PrimaryIndex, fixture.WaterAddress());
    ped = fixture.Ped();
    const auto replacement = binding.image + T::Slots[0].getType;
    const auto methodAddress = T::Slots[2].vtable + T::TypeSlot;
    fixture.task.transform.image.Patch(methodAddress, &replacement, sizeof(replacement), PAGE_READONLY);
    output = true;
    Check(reader.Read(ped, output) == ClockResult::UnsupportedBuild && output,
          "the new class must retain its exact read-only native268 method, never a caller-supplied virtual result");
    const auto originalMethod = binding.image + T::Slots[2].getType;
    fixture.task.transform.image.Patch(methodAddress, &originalMethod, sizeof(originalMethod), PAGE_READONLY);
    fixture.Reset();
    fixture.Primary(W::PrimaryIndex, fixture.WaterAddress());
    ped = fixture.Ped();
    output = false;
    Check(reader.Read(ped, output) == ClockResult::Ok && output, "read-only refusals do not poison later genuine admitted observations");
    std::puts("Production native primary water task predicate fixture completed");
    std::printf("Passed %u native ped water-task checks (64-bit; no native engine code executed)\n", checks);
    return 0;
}
