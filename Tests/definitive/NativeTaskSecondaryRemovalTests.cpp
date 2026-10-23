/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Real secondary-task mutation guards and actual MTA SDK forwarding
 ****************************************************************************/
#define main NativeTaskRemovalTransformFixtureMain
#include "NativePedTransformReaderTests.cpp"
#undef main
#include "../../Client/game_de/NativePedIntelligenceReader.h"
#include "../../Client/game_de/NativePedIntelligenceProfile.h"
#include "../../Client/game_de/NativeTaskPoolReader.h"
#include "../../Client/game_de/NativeTaskPoolProfile.h"
#include "../../Client/game_de/NativeTaskRemovalCalls.h"
#include "../../Client/game_de/NativeTaskRemovalProfile.h"
#include "../../Client/game_de/CTaskManagerDE.h"
#include <thread>
#include <tuple>
#include <stdexcept>

namespace
{
    mta::game_de::ClockResult InvokeRemoval(mta::game_de::NativeTaskRemovalCalls::NativeFunction, std::uintptr_t, std::int32_t, std::uint32_t&) noexcept;
}
// Substitute only module selection, checked OS-copy faults and the final
// genuine native entry. Production owner, pool, code and ABI checks all run.
#define GetModuleHandleW  PoolTestHost::Module
#define ReadProcessMemory PoolTestHost::Read
#include "../../Client/game_de/NativeGameStateMemory.cpp"
#include "../../Client/game_de/NativeStartGameCalls.cpp"
#include "../../Client/game_de/NativePedIntelligenceReader.cpp"
#include "../../Client/game_de/NativeTaskPoolReader.cpp"
#define MTA_DE_TASK_REMOVAL_NATIVE_INVOKE ::InvokeRemoval
#include "../../Client/game_de/NativeTaskRemovalCalls.cpp"
#undef MTA_DE_TASK_REMOVAL_NATIVE_INVOKE
#undef ReadProcessMemory
#undef GetModuleHandleW
#include "../../Client/game_de/CTaskManagerDE.cpp"

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
    namespace R = NativeTaskRemovalCalls;
    namespace RP = NativeTaskRemovalProfile;
    namespace TP = NativeTaskPoolProfile;
    struct RemovalTaskBank
    {
        unsigned char *header = nullptr, *objects = nullptr, *flags = nullptr;
        RemovalTaskBank()
        {
            header = static_cast<unsigned char*>(VirtualAlloc(nullptr, 32, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
            objects = static_cast<unsigned char*>(VirtualAlloc(nullptr, TP::NativeCapacity * TP::ObjectStride, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
            flags = static_cast<unsigned char*>(VirtualAlloc(nullptr, TP::NativeCapacity, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
            Check(header && objects && flags, "own full native task bank separately from ped and intelligence banks");
            Reset();
        }
        template <class T>
        void Header(std::size_t at, const T& value)
        {
            std::memcpy(header + at, &value, sizeof(value));
        }
        template <class T>
        void Store(std::size_t row, std::size_t at, const T& value)
        {
            std::memcpy(objects + row * TP::ObjectStride + at, &value, sizeof(value));
        }
        std::uintptr_t Address(std::size_t row = 0) const { return reinterpret_cast<std::uintptr_t>(objects + row * TP::ObjectStride); }
        void           Reset()
        {
            Header(0, reinterpret_cast<std::uintptr_t>(objects));
            Header(8, reinterpret_cast<std::uintptr_t>(flags));
            Header(0x10, static_cast<std::int32_t>(TP::NativeCapacity));
            Header(0x14, std::int32_t{-1});
            Header(0x18, static_cast<unsigned char>(1));
            std::memset(objects, 0, TP::NativeCapacity * TP::ObjectStride);
            std::memset(flags, 0x80, TP::NativeCapacity);
        }
        ~RemovalTaskBank()
        {
            if (header)
                VirtualFree(header, 0, MEM_RELEASE);
            if (objects)
                VirtualFree(objects, 0, MEM_RELEASE);
            if (flags)
                VirtualFree(flags, 0, MEM_RELEASE);
        }
    };
    struct RemovalFixture : IntelligenceFixture
    {
        RemovalTaskBank tasks;
        explicit RemovalFixture(const std::filesystem::path& directory) : IntelligenceFixture(directory)
        {
            for (const auto& pin : TP::Code)
                transform.image.Patch(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
            for (const auto& pin : RP::Code)
                transform.image.Patch(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
            for (const auto& slot : TP::Slots)
            {
                const auto dtor = transform.image.image + slot.deletingDestructor;
                const auto type = transform.image.image + slot.getType;
                const auto simple = transform.image.image + slot.isSimple;
                transform.image.Patch(slot.vtable, &dtor, sizeof(dtor), PAGE_READONLY);
                transform.image.Patch(slot.vtable + TP::TypeSlot, &type, sizeof(type), PAGE_READONLY);
                transform.image.Patch(slot.vtable + TP::SimpleSlot, &simple, sizeof(simple), PAGE_READONLY);
            }
            Reset();
        }
        void Reset()
        {
            IntelligenceFixture::Reset();
            tasks.Reset();
            Global(TP::RootRva, reinterpret_cast<std::uintptr_t>(tasks.header));
        }
        void Assign(int index, std::size_t row = 0, std::size_t kind = 0, unsigned char generation = 1)
        {
            tasks.Store(row, 0, transform.image.image + TP::Slots[kind].vtable);
            tasks.flags[row] = generation;
            intelligence.Store(17, I::SecondaryOffset + static_cast<std::size_t>(index) * sizeof(std::uintptr_t), tasks.Address(row));
        }
        std::uintptr_t Cell(int index) const
        {
            std::uintptr_t value = 0;
            std::memcpy(&value, intelligence.objects + 17 * I::ObjectStride + I::SecondaryOffset + static_cast<std::size_t>(index) * sizeof(value),
                        sizeof(value));
            return value;
        }
    };
    RemovalFixture*   activeRemoval = nullptr;
    unsigned          removalCalls = 0, removalMode = 0;
    R::NativeFunction passedEntry = nullptr;
    std::uintptr_t    passedManager = 0;
    std::int32_t      passedIndex = -1;
    void              AddRemovalChild()
    {
        activeRemoval->tasks.Store(0, RP::ComplexChildOffset, activeRemoval->tasks.Address(1));
    }
    ClockResult InvokeRemoval(R::NativeFunction function, std::uintptr_t manager, std::int32_t index, std::uint32_t& exception) noexcept
    {
        ++removalCalls;
        passedEntry = function;
        passedManager = manager;
        passedIndex = index;
        auto& fixture = *activeRemoval;
        if (removalMode == 1)
        {
            exception = 0xe0530001;
            return ClockResult::OriginalException;
        }
        const auto address = fixture.Cell(index);
        const auto row = (address - fixture.tasks.Address()) / TP::ObjectStride;
        // This ordinary-host boundary emulates only the already-qualified
        // deletion effects. All real module/owner/pool/manager guards execute.
        fixture.tasks.Store(row, 0, fixture.transform.image.image + std::uintptr_t{0x431c570});
        fixture.tasks.flags[row] |= TP::EmptyBit;
        if (removalMode != 3)
            fixture.intelligence.Store(17, I::SecondaryOffset + static_cast<std::size_t>(index) * sizeof(std::uintptr_t), std::uintptr_t{0});
        if (removalMode == 2)
        {
            exception = 0xe0530002;
            return ClockResult::OriginalException;
        }
        if (removalMode == 4)
            PoolTestHost::ownerResult = ClockResult::Expired;
        if (removalMode == 5)
            fixture.intelligence.flags[17] = 2;
        if (removalMode == 6)
            fixture.transform.pool.flags[0] = 2;
        if (removalMode == 7)
            fixture.intelligence.Store(17, I::SecondaryOffset + static_cast<std::size_t>((index + 1) % 6) * sizeof(std::uintptr_t), std::uintptr_t{0x7771});
        if (removalMode == 8)
            fixture.PedStore(0, I::IntelligenceOffset, fixture.IntelligenceAddress(18));
        if (removalMode == 9)
        {
            const auto changed = static_cast<unsigned char>(RP::Secondary[0] ^ 1);
            fixture.transform.image.Patch(RP::SecondaryRva, &changed, 1, PAGE_EXECUTE_READ);
        }
        return ClockResult::Ok;
    }
    struct RemovalSdkAccess final : NativeTaskRemovalAccess
    {
        NativePedReferenceToken observed{};
        int                     index = 0;
        unsigned                calls = 0;
        ClockResult             result = ClockResult::Ok;
        ClockResult             RemoveReferencedPedSecondaryTask(const NativePedReferenceToken& token, int priority) noexcept override
        {
            observed = token;
            index = priority;
            ++calls;
            return result;
        }
    };
    class RemovalSdkManager final : public CTaskManagerDE
    {
    public:
        RemovalSdkManager(NativeTaskRemovalAccess& access, const NativePedReferenceToken& token) : CTaskManagerDE(access, token) {}
        using CTaskManagerDE::RemoveTaskSecondary;
        [[noreturn]] static void Unexpected() { throw std::logic_error("Unqualified SDK task operation was called"); }
        void                     RemoveTask(int) override { Unexpected(); }
        CTask*                   GetTask(int) override { Unexpected(); }
        CTask*                   GetActiveTask() override { Unexpected(); }
        CTask*                   GetSimplestActiveTask() override { Unexpected(); }
        CTask*                   GetSimplestTask(int) override { Unexpected(); }
        CTask*                   FindActiveTaskByType(int) override { Unexpected(); }
        CTask*                   FindTaskByType(int, int) override { Unexpected(); }
        bool                     RemoveTaskSecondary(int, int) override { Unexpected(); }
        CTask*                   GetTaskSecondary(int) override { Unexpected(); }
        bool                     HasTaskSecondary(const CTask*) override { Unexpected(); }
        void                     ClearTaskEventResponse() override { Unexpected(); }
        void                     Flush(int) override { Unexpected(); }
    };
}

int main(int argc, char** argv)
{
    Check(argc == 2 && std::filesystem::path(argv[1]).is_absolute(), "task-removal proof uses explicit workspace-owned evidence directory");
    RemovalFixture fixture(argv[1]);
    activeRemoval = &fixture;
    PoolTestHost::image = fixture.transform.image.image;
    const auto reset = [&](int index = 0, std::size_t row = 0, std::size_t kind = 0, unsigned char generation = 1)
    {
        fixture.Reset();
        fixture.Assign(index, row, kind, generation);
        removalCalls = removalMode = 0;
        passedEntry = nullptr;
        passedManager = 0;
        passedIndex = -1;
        return fixture.Ped();
    };
    for (int index = 0; index < 6; ++index)
        for (std::size_t kind = 0; kind < 2; ++kind)
            for (const unsigned char generation : {static_cast<unsigned char>(0), static_cast<unsigned char>(127)})
            {
                const auto row = kind == 0 ? std::size_t{0} : TP::NativeCapacity - 1;
                auto       ped = reset(index, row, kind, generation);
                R::Receipt receipt;
                Check(R::RemoveSecondary(fixture.transform.image.image, ped, index, &PoolTestHost::Owner, &receipt, &InvokeRemoval) == ClockResult::Ok &&
                          removalCalls == 1 && receipt.entered && receipt.returned && !receipt.alreadyEmpty && receipt.oldTask == fixture.tasks.Address(row) &&
                          receipt.oldTaskSlot == row && receipt.oldTaskGeneration == generation && receipt.oldTaskType == TP::Slots[kind].nativeType &&
                          fixture.Cell(index) == 0 && (fixture.tasks.flags[row] & TP::EmptyBit) && passedIndex == index &&
                          passedEntry == reinterpret_cast<R::NativeFunction>(fixture.transform.image.image + RP::SecondaryRva) &&
                          passedManager == fixture.IntelligenceAddress() + I::ManagerOffset,
                      "all six secondary slots remove each exact class via one real-ABI boundary with native generation-zero/final-row admission");
            }
    {
        const auto ped = reset(0, 0, 2);
        const auto oldTask = fixture.Cell(0);
        const auto flag = fixture.tasks.flags[0];
        R::Receipt receipt;
        Check(R::RemoveSecondary(fixture.transform.image.image, ped, 0, &PoolTestHost::Owner, &receipt, &InvokeRemoval) == ClockResult::UnsupportedBuild &&
                  !receipt.entered && !receipt.returned && receipt.oldTaskType == 268 && !removalCalls && fixture.Cell(0) == oldTask &&
                  fixture.tasks.flags[0] == flag,
              "new read-only water268 qualification never broadens the old203/305 native deletion capability");
    }
    auto ped = reset();
    fixture.intelligence.Store(17, I::SecondaryOffset, std::uintptr_t{0});
    fixture.Global(TP::RootRva, std::uintptr_t{0});
    R::Receipt receipt;
    Check(R::RemoveSecondary(fixture.transform.image.image, ped, 0, &PoolTestHost::Owner, &receipt, &InvokeRemoval) == ClockResult::Ok &&
              receipt.alreadyEmpty && !receipt.entered && !receipt.returned && removalCalls == 0,
          "actual native null-equality no-op neither needs a task bank nor fabricates a destructive call");
    for (const int bad : {-1, 6, (std::numeric_limits<int>::min)(), (std::numeric_limits<int>::max)()})
    {
        ped = reset();
        const auto reads = PoolTestHost::reads, owners = PoolTestHost::ownerCalls;
        Check(R::RemoveSecondary(fixture.transform.image.image, ped, bad, &PoolTestHost::Owner, &receipt, &InvokeRemoval) == ClockResult::InvalidArgument &&
                  !receipt.entered && removalCalls == 0 && PoolTestHost::reads == reads && PoolTestHost::ownerCalls == owners &&
                  fixture.Cell(0) == fixture.tasks.Address(),
              "unchecked native signed indices are explicitly refused without clamp, array access or deletion");
    }
    for (unsigned guard = 0; guard < 13; ++guard)
    {
        ped = reset();
        if (guard == 0)
            PoolTestHost::ownerResult = ClockResult::Expired;
        if (guard == 1)
            fixture.Global(NativeStartGameProfile::ThreadIdRva, GetCurrentThreadId() + 1);
        if (guard == 2)
            fixture.Global(NativeBorrowedGcProfile::markerRva, LONG{1});
        if (guard == 3)
            fixture.Global(NativeGameStateProfile::stateRva, std::uint32_t{7});
        if (guard == 4)
            fixture.tasks.flags[0] |= TP::EmptyBit;
        if (guard == 5)
            fixture.tasks.Store(0, 0, fixture.transform.image.image + std::uintptr_t{0x431c570});
        if (guard == 6)
            fixture.intelligence.Store(17, I::PrimaryOffset, fixture.tasks.Address());
        if (guard == 7)
            fixture.intelligence.Store(17, I::SecondaryOffset + sizeof(std::uintptr_t), fixture.tasks.Address());
        if (guard == 8)
        {
            fixture.Assign(0, 0, 1);
            fixture.tasks.Store(0, RP::ComplexChildOffset, fixture.tasks.Address(1));
        }
        if (guard == 9)
            fixture.Global(TP::RootRva, std::uintptr_t{0});
        if (guard == 10)
            fixture.intelligence.flags[17] |= I::EmptyBit;
        if (guard == 11)
            fixture.PedStore(0, I::IntelligenceOffset, fixture.IntelligenceAddress(18));
        if (guard == 12)
            fixture.transform.pool.flags[0] = 2;
        Check(R::RemoveSecondary(fixture.transform.image.image, ped, 0, &PoolTestHost::Owner, &receipt, &InvokeRemoval) != ClockResult::Ok &&
                  !receipt.entered && removalCalls == 0,
              "owner,GT,GC,state,pool,destructor,manager alias,child and retained identity refusal precede every destructive call");
    }
    ped = reset();
    Check(R::RemoveSecondary(fixture.transform.image.image, ped, 0, nullptr, &receipt, &InvokeRemoval) == ClockResult::NotInitialized && !receipt.entered &&
              removalCalls == 0,
          "readable task memory supplies no default callback authority");
    ped = reset(0, 0, 1);
    PoolTestHost::watched = fixture.tasks.Address() + RP::ComplexChildOffset;
    PoolTestHost::after = &AddRemovalChild;
    Check(R::RemoveSecondary(fixture.transform.image.image, ped, 0, &PoolTestHost::Owner, &receipt, &InvokeRemoval) == ClockResult::UnsupportedBuild &&
              !receipt.entered && removalCalls == 0 && fixture.Cell(0) == fixture.tasks.Address() && !(fixture.tasks.flags[0] & TP::EmptyBit),
          "a complex child appearing during preflight is rechecked after all pool/manager bookends and refuses recursive deletion");
    for (unsigned mode = 1; mode <= 9; ++mode)
    {
        ped = reset();
        removalMode = mode;
        Check(R::RemoveSecondary(fixture.transform.image.image, ped, 0, &PoolTestHost::Owner, &receipt, &InvokeRemoval) != ClockResult::Ok && receipt.entered &&
                  receipt.returned == (mode != 1 && mode != 2) && removalCalls == 1,
              "entered exception,missing cell clear,owner or identity loss,foreign effects and code change never replay native deletion");
        if (mode != 1)
            Check((fixture.tasks.flags[0] & TP::EmptyBit) != 0, "uncertain native completion preserves accepted task retirement instead of undoing it");
        if (mode == 1 || mode == 2)
            Check(receipt.exceptionCode == 0xe0530000 + mode, "receipt preserves the exact final-boundary exception identity");
        if (mode == 9)
            fixture.transform.image.Patch(RP::SecondaryRva, RP::Secondary.data(), RP::Secondary.size(), PAGE_EXECUTE_READ);
    }
    ped = reset();
    PoolTestHost::reads = PoolTestHost::ownerCalls = 0;
    Check(R::RemoveSecondary(fixture.transform.image.image, ped, 0, &PoolTestHost::Owner, &receipt, &InvokeRemoval) == ClockResult::Ok,
          "measure actual guarded production read and owner boundaries once");
    const auto reads = PoolTestHost::reads, owners = PoolTestHost::ownerCalls;
    for (unsigned failure = 1; failure <= reads; ++failure)
    {
        ped = reset();
        PoolTestHost::reads = PoolTestHost::ownerCalls = 0;
        PoolTestHost::failAt = failure;
        const auto result = R::RemoveSecondary(fixture.transform.image.image, ped, 0, &PoolTestHost::Owner, &receipt, &InvokeRemoval);
        Check(result != ClockResult::Ok && removalCalls <= 1 && receipt.entered == (removalCalls != 0),
              "each actual OS-read fault refuses or accounts for exactly one entered mutation");
        Check(receipt.entered ? fixture.Cell(0) == 0 && (fixture.tasks.flags[0] & TP::EmptyBit)
                              : fixture.Cell(0) == fixture.tasks.Address() && !(fixture.tasks.flags[0] & TP::EmptyBit),
              "failed guards have no effects and failed postchecks preserve accepted deletion without rollback");
    }
    for (unsigned refusal = 1; refusal <= owners; ++refusal)
    {
        ped = reset();
        PoolTestHost::reads = PoolTestHost::ownerCalls = 0;
        PoolTestHost::revokeAt = refusal;
        Check(R::RemoveSecondary(fixture.transform.image.image, ped, 0, &PoolTestHost::Owner, &receipt, &InvokeRemoval) != ClockResult::Ok &&
                  removalCalls <= 1 && receipt.entered == (removalCalls != 0),
              "every genuine callback-owner boundary may revoke without duplicate deletion");
    }
    ped = reset();
    ClockResult foreign = ClockResult::Ok;
    std::thread worker(
        [&]()
        {
            R::Receipt other;
            foreign = R::RemoveSecondary(fixture.transform.image.image, ped, 0, &PoolTestHost::Owner, &other, &InvokeRemoval);
        });
    worker.join();
    Check(foreign != ClockResult::Ok && removalCalls == 0, "a foreign OS thread cannot borrow native task-deletion authority");
    ped = reset();
    for (const auto& pin : RP::Code)
    {
        const auto changed = static_cast<unsigned char>(pin.bytes[0] ^ 1);
        fixture.transform.image.Patch(pin.rva, &changed, 1, PAGE_EXECUTE_READ);
        Check(R::RemoveSecondary(fixture.transform.image.image, ped, 0, &PoolTestHost::Owner, &receipt, &InvokeRemoval) == ClockResult::UnsupportedBuild &&
                  !receipt.entered && removalCalls == 0,
              "foreign secondary deletion/helper code cannot inherit earlier native mutation qualification");
        fixture.transform.image.Patch(pin.rva, pin.bytes, 1, PAGE_EXECUTE_READ);
    }
    NativePedReferenceToken token;
    token.slot = 17;
    token.version = 91;
    token.ownerGeneration = 8;
    RemovalSdkAccess  access;
    RemovalSdkManager manager(access, token);
    CTaskManager&     sdk = manager;
    for (const int index : {0, 1, 2, 3, 4, 5, -1, 6, (std::numeric_limits<int>::min)(), (std::numeric_limits<int>::max)()})
    {
        const auto calls = access.calls;
        sdk.RemoveTaskSecondary(index);
        Check(access.calls == calls + 1 && access.index == index && access.observed.slot == 17 && access.observed.version == 91 &&
                  access.observed.ownerGeneration == 8,
              "the actual SDK virtual forwards its exact signed index and retained CPU token once, without clamp or guessed native pointers");
    }
    access.result = ClockResult::OriginalException;
    bool       threw = false;
    const auto calls = access.calls;
    try
    {
        sdk.RemoveTaskSecondary(2);
    }
    catch (const std::runtime_error&)
    {
        threw = true;
    }
    Check(threw && access.calls == calls + 1, "the actual void SDK method exposes native refusal and never retries destructive intent");
    NativeTaskRemovalAccess defaultAccess;
    RemovalSdkManager       defaultManager(defaultAccess, token);
    threw = false;
    try
    {
        defaultManager.RemoveTaskSecondary(0);
    }
    catch (const std::runtime_error&)
    {
        threw = true;
    }
    Check(threw, "older read-only owners cannot manufacture a successful SDK secondary removal");
    std::puts("Production native task secondary-removal primitive and actual SDK fixture completed");
    std::printf("Passed %u native task secondary-removal checks (64-bit; no native engine code executed)\n", checks);
    return 0;
}
