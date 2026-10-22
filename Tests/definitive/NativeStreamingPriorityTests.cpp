/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Real priority selection and native lifetime boundaries on owned pages
 ****************************************************************************/
#pragma push_macro("main")
#undef main
#define main HistoricalPriorityLoadAllMain
#include "NativeStreamingLoadAllTests.cpp"
#undef main
#pragma pop_macro("main")
#include "../../Client/game_de/NativeStreamingPriorityCalls.h"
#include "../../Client/game_de/NativeStreamingPriorityProfile.h"
#include <utility>
namespace
{
    mta::game_de::ClockResult InvokePriorityWait(mta::game_de::NativeStreamingPriorityCalls::WaitFunction, std::uintptr_t, std::int32_t&,
                                                 std::uint32_t&) noexcept;
    mta::game_de::ClockResult InvokePriorityDrain(mta::game_de::NativeStreamingPriorityCalls::DrainFunction, std::uintptr_t, std::uint32_t&) noexcept;
}
#define GetModuleHandleW                       PoolTestHost::Module
#define ReadProcessMemory                      PoolTestHost::Read
#define MTA_DE_STREAMING_PRIORITY_WAIT_INVOKE  ::InvokePriorityWait
#define MTA_DE_STREAMING_PRIORITY_DRAIN_INVOKE ::InvokePriorityDrain
#include "../../Client/game_de/NativeStreamingPriorityCalls.cpp"
#undef MTA_DE_STREAMING_PRIORITY_DRAIN_INVOKE
#undef MTA_DE_STREAMING_PRIORITY_WAIT_INVOKE
#undef ReadProcessMemory
#undef GetModuleHandleW
namespace
{
    namespace PC = NativeStreamingPriorityCalls;
    namespace PP = NativeStreamingPriorityProfile;
    namespace PD = PC::PriorityDetail;
    struct PriorityFixture : LoadFixture
    {
        unsigned char *engine = nullptr, *asset = nullptr, *assetType = nullptr, *registry = nullptr, *pending = nullptr;
        unsigned char *handles = nullptr, *controls = nullptr, *loader = nullptr;
        explicit PriorityFixture(const std::filesystem::path& directory) : LoadFixture(directory)
        {
            const auto allocate = [](std::size_t bytes)
            { return static_cast<unsigned char*>(VirtualAlloc(nullptr, bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE)); };
            engine = allocate(0x1000);
            asset = allocate(0x1000);
            assetType = allocate(0x1000);
            registry = allocate(0x1000);
            pending = allocate(0x10000);
            handles = allocate(0x21000);
            controls = allocate(0x5000);
            loader = allocate(0x1000);
            Check(engine && asset && assetType && registry && pending && handles && controls && loader,
                  "own current typed manager, genuine native-shaped handle/control and pending arrays");
            for (const auto& pin : PP::Code)
                transform.image.Patch(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
            Reset();
        }
        void SetProtection(void* address, std::size_t bytes, DWORD protection)
        {
            DWORD previous = 0;
            Check(VirtualProtect(address, bytes, protection, &previous) != FALSE, "change protection only on fixture-owned pages");
        }
        void Registry(unsigned char* object, std::int32_t index, std::uintptr_t vtable, std::uintptr_t type)
        {
            Put(object, 0, vtable);
            Put(object, 8, std::uint32_t{0});
            Put(object, 0xc, index);
            Put(object, 0x10, type);
            Put(object, 0x20, std::uintptr_t{0});
            Put(registry, static_cast<std::size_t>(index) * 24, Address(object));
            Put(registry, static_cast<std::size_t>(index) * 24 + 8, std::uint32_t{0});
            Put(registry, static_cast<std::size_t>(index) * 24 + 0x10, static_cast<std::uint32_t>(200 + index));
        }
        void Pointer(std::uintptr_t rva, std::uintptr_t target) { transform.image.Patch(rva, &target, sizeof(target), PAGE_READONLY); }
        void State(std::uint32_t model, unsigned char flags, unsigned char state)
        {
            Global(PP::RowsRva + static_cast<std::uintptr_t>(model) * PP::RowBytes + 12, std::array<unsigned char, 2>{flags, state});
        }
        PD::Pending Record(std::size_t index) const
        {
            PD::Pending result;
            std::memcpy(&result, pending + index * sizeof(result), sizeof(result));
            return result;
        }
        std::uintptr_t Handle(std::size_t index) const { return Address(handles) + index * 0x100; }
        std::uintptr_t Control(std::size_t index) const { return Address(controls) + index * 0x20; }
        void           Add(std::uint32_t model, unsigned char flags = 0x10, unsigned char state = 3, std::uint32_t oldPriority = 50)
        {
            PD::Array array;
            std::memcpy(&array, application + 0x80, sizeof(array));
            const auto index = static_cast<std::size_t>(array.count);
            Check(index <= PP::MaximumCalls, "fixture pending slab capacity covers bounded producer");
            PD::Pending record;
            record.boundObject = Address(application);
            record.boundInterface = Address(application) + PP::CallbackOffset;
            record.handle = {Handle(index), Control(index)};
            record.model = model;
            record.originalPriority = oldPriority;
            record.paths = {record.handle.object + 0xa0, 1, 1};
            std::memcpy(pending + index * sizeof(record), &record, sizeof(record));
            auto* handle = reinterpret_cast<unsigned char*>(record.handle.object);
            auto* control = reinterpret_cast<unsigned char*>(record.handle.control);
            Put(handle, 0, record.handle.object);
            Put(handle, 8, record.handle.control);
            Put(handle, 0x98, Address(asset) + PP::AssetStreamableOffset);
            Put(control, 0, transform.image.image + PP::ControlVtableRva);
            Put(control, 8, std::int32_t{1});
            Put(control, 12, std::int32_t{1});
            Put(control, 16, record.handle.object);
            array.data = Address(pending);
            ++array.count;
            array.capacity = array.count;
            Put(application, 0x80, array);
            State(model, flags, state);
        }
        void Reset()
        {
            for (const auto pair : {std::pair<unsigned char*, std::size_t>{pending, 0x10000}, {handles, 0x21000}, {controls, 0x5000}})
            {
                SetProtection(pair.first, pair.second, PAGE_READWRITE);
                std::memset(pair.first, 0, pair.second);
            }
            LoadFixture::Reset();
            std::memset(engine, 0, 0x1000);
            std::memset(asset, 0, 0x1000);
            std::memset(assetType, 0, 0x1000);
            std::memset(registry, 0, 0x1000);
            std::memset(loader, 0, 0x1000);
            std::array<unsigned char, PP::PhysicalRows * PP::RowBytes> streamRows{};
            transform.image.Patch(PP::RowsRva, streamRows.data(), streamRows.size(), PAGE_READWRITE);
            std::memcpy(registry, graph.items.data(), graph.items.size());
            graph.chunks[0] = Address(registry);
            Global(NativeStartupWorldProfile::objectCountRva, std::int32_t{14});
            Registry(engine, 11, transform.image.image + NativeStartupWorldProfile::ownerVtableRva, graph.Class(0));
            Registry(asset, 12, transform.image.image + PP::AssetVtableRva, Address(assetType));
            Registry(assetType, 13, transform.image.image + NativeStartupWorldProfile::ownerVtableRva, graph.Class(0));
            Put(engine, PP::EngineAssetOffset, Address(asset));
            Global(PP::EngineRva, Address(engine));
            Global(PP::AssetClassRva, Address(assetType));
            Put(application, PP::CallbackOffset, transform.image.image + PP::CallbackVtableRva);
            Put(application, 0x78, static_cast<unsigned char>(0));
            Put(application, 0x80, PD::Array{});
            Pointer(PP::CallbackVtableRva + 0x10, transform.image.image + PP::CallbackRva);
            Pointer(PP::ControlVtableRva, transform.image.image + PP::ControlDestroyRva);
            Pointer(PP::ControlVtableRva + 8, transform.image.image + PP::ControlDeleteRva);
            Put(loader, 0, transform.image.image + PP::ControlVtableRva);
            Pointer(PP::ControlVtableRva + 0x30, transform.image.image + PP::WaitRva);
            Global(PP::LoaderRva, Address(loader));
        }
        ~PriorityFixture()
        {
            for (auto* memory : {engine, asset, assetType, registry, pending, handles, controls, loader})
                VirtualFree(memory, 0, MEM_RELEASE);
        }
    };
    PriorityFixture*                             activePriority = nullptr;
    unsigned                                     priorityScenario = 0, waitCalls = 0, drainCalls = 0;
    std::array<std::uintptr_t, PP::MaximumCalls> waited{};
    PC::WaitFunction                             passedWait = nullptr;
    PC::DrainFunction                            passedDrain = nullptr;
    ClockResult                                  recursivePriority = ClockResult::Ok, recursiveFull = ClockResult::Ok;
    ClockResult                                  InvokeFullReentry(LC::NativeFunction, std::uint32_t&) noexcept
    {
        PC::Receipt nested;
        recursivePriority = PC::Load(activePriority->transform.image.image, &PoolTestHost::Owner, &nested);
        if (nested.entered)
            std::abort();
        return ClockResult::Ok;
    }
    void ChangePriority()
    {
        activePriority->State(17, 0, 3);
    }
    ClockResult InvokePriorityWait(PC::WaitFunction function, std::uintptr_t handle, std::int32_t& result, std::uint32_t& exception) noexcept
    {
        auto& f = *activePriority;
        waited[waitCalls++] = handle;
        passedWait = function;
        result = 2;
        f.application[0x78] = 1;
        if (priorityScenario == 1)
        {
            exception = EXCEPTION_ACCESS_VIOLATION;
            return ClockResult::OriginalException;
        }
        if (priorityScenario == 2)
            f.Global(LPF::RootRva, std::uintptr_t{0});
        if (priorityScenario == 3)
            PoolTestHost::ownerResult = ClockResult::Expired;
        if (priorityScenario == 4)
            f.Global(NativeBorrowedGcProfile::markerRva, LONG{1});
        if (priorityScenario == 5)
            f.Put(f.engine, PP::EngineAssetOffset, std::uintptr_t{0});
        if (priorityScenario == 6)
        {
            f.State(17, 0, 1);
            f.Put(f.application, 0x80, PD::Array{});
            f.SetProtection(f.pending, 0x10000, PAGE_NOACCESS);
            f.SetProtection(f.handles, 0x1000, PAGE_NOACCESS);
            f.SetProtection(f.controls, 0x1000, PAGE_NOACCESS);
        }
        if (priorityScenario == 7)
        {
            PC::Receipt nested;
            LC::Receipt full;
            recursivePriority = PC::Load(f.transform.image.image, &PoolTestHost::Owner, &nested);
            recursiveFull = LC::Load(f.transform.image.image, false, &PoolTestHost::Owner, &full);
            if (nested.entered || full.entered)
                std::abort();
        }
        return ClockResult::Ok;
    }
    ClockResult InvokePriorityDrain(PC::DrainFunction function, std::uintptr_t root, std::uint32_t& exception) noexcept
    {
        auto& f = *activePriority;
        ++drainCalls;
        passedDrain = function;
        if (root != f.Address(f.application))
            std::abort();
        if (priorityScenario == 8)
        {
            exception = EXCEPTION_ACCESS_VIOLATION;
            return ClockResult::OriginalException;
        }
        if (priorityScenario == 9)
            f.Global(LPF::RootRva, std::uintptr_t{0});
        if (priorityScenario == 10)
            f.Put(f.engine, PP::EngineAssetOffset, std::uintptr_t{0});
        if (priorityScenario == 11)
        {
            // A failed native completion can leave the same request pending;
            // the wrapper must not wait it twice or invent loaded state1.
            return ClockResult::Ok;
        }
        if (priorityScenario != 6)
        {
            PD::Array array;
            std::memcpy(&array, f.application + 0x80, sizeof(array));
            for (std::int32_t i = 0; i != array.count; ++i)
            {
                const auto record = f.Record(static_cast<std::size_t>(i));
                bool       completed = false;
                for (unsigned index = 0; index != waitCalls; ++index)
                    if (waited[index] == record.handle.object)
                        completed = true;
                if (completed)
                    f.State(record.model, 0, 1);
            }
        }
        f.application[0x78] = 0;
        if (priorityScenario == 12 && drainCalls == 1)
            f.Add(20000);
        if (priorityScenario == 13)
            f.State(18, 0, 1);
        return ClockResult::Ok;
    }
}
int main(int argc, char** argv)
{
    Check(argc == 2 && std::filesystem::path(argv[1]).is_absolute(), "priority evidence uses explicit workspace-owned directory");
    PriorityFixture fixture(argv[1]);
    activePriority = &fixture;
    const auto image = fixture.transform.image.image;
    PoolTestHost::image = image;
    PC::Receipt receipt;
    const auto  reset = [&]
    {
        fixture.Reset();
        priorityScenario = waitCalls = drainCalls = 0;
        waited.fill(0);
        passedWait = nullptr;
        passedDrain = nullptr;
        recursivePriority = recursiveFull = ClockResult::Ok;
    };
    reset();
    Check(PC::Load(image, nullptr, &receipt) == ClockResult::NotInitialized && !receipt.entered && !waitCalls && !drainCalls,
          "native image does not supply callback authority");
    Check(PC::Load(image, &PoolTestHost::Owner, &receipt) == ClockResult::Ok && !receipt.entered && receipt.returned && !receipt.selectedBefore,
          "empty current priority set is a qualified no-op without loading unrelated work");
    for (const auto state : {static_cast<unsigned char>(2), static_cast<unsigned char>(3)})
    {
        reset();
        fixture.Add(17, 0x10, state, 50);
        fixture.Add(18, 0, 3, 65);
        Check(PC::Load(image, &PoolTestHost::Owner, &receipt) == ClockResult::Ok && receipt.entered && receipt.returned && receipt.waits == 1 &&
                  receipt.drains == 1 && receipt.selectedBefore == 1 && receipt.selectedAfter == 0 && receipt.lastModel == 17 &&
                  waited[0] == fixture.Handle(0) && reinterpret_cast<std::uintptr_t>(passedWait) == image + PP::WaitRva &&
                  reinterpret_cast<std::uintptr_t>(passedDrain) == image + PP::DrainRva,
              "current flags select real handle-backed state2/3, including promotion after oldpriority50; original priority65 cannot force unrelated wait");
        unsigned char stateAfter = 0;
        std::memcpy(&stateAfter, reinterpret_cast<void*>(image + PP::RowsRva + 18 * PP::RowBytes + 13), 1);
        Check(stateAfter == 3, "unselected pending request is not force-completed");
    }
    reset();
    fixture.Add(17);
    fixture.Add(20000);
    fixture.Add(26311);
    Check(PC::Load(image, &PoolTestHost::Owner, &receipt) == ClockResult::Ok && waitCalls == 3 && drainCalls == 3 && receipt.selectedBefore == 3 &&
              receipt.selectedAfter == 0,
          "DFF, first TXD and last genuine SCM requests share the real native completion path");
    reset();
    fixture.Add(17);
    priorityScenario = 12;
    Check(PC::Load(image, &PoolTestHost::Owner, &receipt) == ClockResult::Ok && waitCalls == 2 && drainCalls == 2 && waited[1] == fixture.Handle(1) &&
              receipt.selectedBefore == 1 && receipt.selectedAfter == 0,
          "callback-generated new priority work is selected from fresh rows and new owned queue, not an old list snapshot");
    reset();
    fixture.Add(17);
    fixture.Add(18, 0, 3, 65);
    priorityScenario = 13;
    Check(PC::Load(image, &PoolTestHost::Owner, &receipt) == ClockResult::Ok && waitCalls == 1 && waited[0] == fixture.Handle(0) && drainCalls == 1 &&
              fixture.Stream(18)[1] == 1,
          "genuine drain may retire already-ready nonpriority callbacks without forcing their handles through Wait");
    reset();
    fixture.State(26312, 0x10, 3);
    fixture.State(26315, 0x10, 2);
    Check(PC::Load(image, &PoolTestHost::Owner, &receipt) == ClockResult::Ok && !waitCalls && !drainCalls,
          "initializer list sentinels never become request authority");
    reset();
    fixture.Add(17);
    fixture.State(20000, 0x10, 2);
    Check(PC::Load(image, &PoolTestHost::Owner, &receipt) == ClockResult::UnsupportedBuild && !receipt.entered && !waitCalls && !drainCalls,
          "ALL selected rows preflight before effects; unresolved state2 without native handle never becomes a successful global flush");
    for (unsigned failure = 0; failure != 13; ++failure)
    {
        reset();
        fixture.Add(17);
        switch (failure)
        {
            case 0:
                fixture.Put(fixture.controls, 8, std::int32_t{0});
                break;
            case 1:
                fixture.Put(fixture.controls, 8, (std::numeric_limits<std::int32_t>::max)());
                break;
            case 2:
                fixture.Put(fixture.controls, 16, fixture.Handle(0) + 8);
                break;
            case 3:
                fixture.Put(fixture.handles, 8, fixture.Control(0) + 8);
                break;
            case 4:
                fixture.Put(fixture.handles, 0x98, fixture.Address(fixture.asset));
                break;
            case 5:
                fixture.Put(fixture.pending, 0x18, fixture.Address(fixture.application));
                break;
            case 6:
                fixture.Put(fixture.application, 0x88, std::int32_t{-1});
                break;
            case 7:
                fixture.Global(PP::LoaderRva, std::uintptr_t{0});
                break;
            case 8:
                fixture.Put(fixture.registry, 12 * 24 + NativeStartupProvidersProfile::itemFlagsOffset, NativeStartupProvidersProfile::itemRejectMask);
                break;
            case 9:
                fixture.Global(NativeStartGameProfile::ThreadIdRva, static_cast<DWORD>(GetCurrentThreadId() + 1));
                break;
            case 10:
                fixture.Global(NativeGameStateProfile::stateRva, std::uint32_t{7});
                break;
            case 11:
                fixture.Global(NativeBorrowedGcProfile::markerRva, LONG{1});
                break;
            case 12:
                fixture.Add(17);
                break;
        }
        Check(PC::Load(image, &PoolTestHost::Owner, &receipt) != ClockResult::Ok && !receipt.entered && !waitCalls && !drainCalls,
              "bad strong/control/self/manager/callback/array/loader/member/GT/GC/state or ambiguous pending key refuses before entry");
    }
    for (unsigned scenario = 1; scenario <= 10; ++scenario)
    {
        reset();
        fixture.Add(17);
        priorityScenario = scenario;
        const auto result = PC::Load(image, &PoolTestHost::Owner, &receipt);
        if (scenario == 6)
            Check(result == ClockResult::Ok && waitCalls == 1 && drainCalls == 1 && receipt.returned,
                  "callbacks can free old handle, control and queue pages before Wait returns; postchecks only use fresh root storage");
        else if (scenario == 7)
            Check(result == ClockResult::Ok && recursivePriority == ClockResult::Busy && recursiveFull == ClockResult::Busy && waitCalls == 1,
                  "one genuine completion owner gate rejects recursive selective and full loading without consuming outer ownership");
        else
            Check(result != ClockResult::Ok && receipt.entered && !receipt.returned && waitCalls == 1 && drainCalls <= 1,
                  "entered exception or lost root/owner/GC/manager after native wait/drain preserves effects without replay or fake acknowledgement");
    }
    reset();
    fixture.Add(17);
    priorityScenario = 11;
    Check(PC::Load(image, &PoolTestHost::Owner, &receipt) == ClockResult::Ok && receipt.returned && waitCalls == 1 && drainCalls == 1 &&
              receipt.selectedAfter == 1,
          "native failed/requeued status is preserved without spinning, repeated handle waits or forced loaded-state writes");
    reset();
    for (std::uint32_t model = 0; model <= PP::MaximumCalls; ++model)
        fixture.Add(model);
    Check(PC::Load(image, &PoolTestHost::Owner, &receipt) == ClockResult::UnsupportedBuild && !receipt.entered && !waitCalls,
          "bounded selection capacity refuses before effects rather than allocating indefinitely or silently skipping priority work");
    reset();
    fixture.Add(17);
    LC::Receipt full;
    Check(LC::Load(image, false, &PoolTestHost::Owner, &full, &InvokeFullReentry) == ClockResult::Ok && full.entered && full.returned &&
              recursivePriority == ClockResult::Busy && !waitCalls,
          "actual full-load caller consumes the shared gate and refuses nested selective admission too");
    reset();
    fixture.Add(17);
    PoolTestHost::watched = image + PP::RowsRva;
    PoolTestHost::after = &ChangePriority;
    Check(PC::Load(image, &PoolTestHost::Owner, &receipt) != ClockResult::Ok && !receipt.entered && !waitCalls,
          "row promotion/selection changes between actual chunk copies cannot create a hybrid admission");
    for (const auto& pin : PP::Code)
    {
        reset();
        fixture.Add(17);
        const auto bad = static_cast<unsigned char>(pin.bytes[0] ^ 1);
        fixture.transform.image.Patch(pin.rva, &bad, 1, PAGE_EXECUTE_READ);
        Check(PC::Load(image, &PoolTestHost::Owner, &receipt) != ClockResult::Ok && !receipt.entered && !waitCalls,
              "exact wait/producer/callback/manager/priority qualification pins bind native layout and ABI");
        fixture.transform.image.Patch(pin.rva, pin.bytes, 1, PAGE_EXECUTE_READ);
    }
    reset();
    fixture.Add(17);
    ClockResult foreign = ClockResult::Ok;
    std::thread thread([&] { foreign = PC::Load(image, &PoolTestHost::Owner, &receipt); });
    thread.join();
    Check(foreign != ClockResult::Ok && !receipt.entered && !waitCalls, "foreign thread cannot adopt the real callback's completion authority");
    reset();
    fixture.Add(17);
    PoolTestHost::failAt = 1;
    Check(PC::Load(image, &PoolTestHost::Owner, &receipt) != ClockResult::Ok && !receipt.entered && !waitCalls, "guarded read failure precedes native effects");
    reset();
    fixture.Add(17);
    Check(PC::Load(image, &PoolTestHost::Owner, &receipt) == ClockResult::Ok, "shared owner gate also releases after faults and refusal");
    std::puts("Production native priority-completion primitive fixture completed");
    std::printf("Passed %u native streaming-priority checks (64-bit; no native engine code executed)\n", checks);
    return 0;
}
