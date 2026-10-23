/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Immediate task identity through the same actual callback owner
 ****************************************************************************/
namespace
{
    struct TaskPoolOwnerContext
    {
        std::uintptr_t              task = 0;
        NativeTaskPoolReader::Value value;
        ClockResult                 result = ClockResult::NotInitialized;
    };
    void TaskPoolOwnerHandler(NativeGameTimingAccess& access, void* opaque)
    {
        auto& context = *static_cast<TaskPoolOwnerContext*>(opaque);
        context.result = access.ReadTaskPoolEntry(context.task, context.value);
    }
    void NativeTaskPoolOwnedSdk(ClockImageFixture& fixture)
    {
        namespace Ped = NativePedPoolProfile;
        namespace T = NativeTaskPoolProfile;
        struct Saved
        {
            std::uintptr_t             rva;
            std::vector<unsigned char> bytes;
        };
        std::vector<Saved> saved;
        const auto         admit = [&](std::uintptr_t rva, const void* bytes, std::size_t size, DWORD protection)
        {
            Saved  prior{rva, std::vector<unsigned char>(size)};
            SIZE_T copied = 0;
            Check(::ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(fixture.image + rva), prior.bytes.data(), size, &copied) && copied == size,
                  "save only exact task/owner proof spans in the existing owned callback image");
            saved.push_back(std::move(prior));
            fixture.Patch(rva, bytes, size);
            DWORD previous = 0;
            Check(::VirtualProtect(reinterpret_cast<void*>(fixture.image + rva), size, protection, &previous) != FALSE,
                  "task identity keeps true image code, immutable vtable and mutable bank page contracts");
        };
        for (const auto& pin : Ped::Code)
            admit(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : T::Code)
            admit(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : NativeGameStateProfile::pins)
            admit(pin.rva, pin.bytes, pin.size, pin.code ? PAGE_EXECUTE_READ : PAGE_READONLY);
        for (const auto& pin : NativeBorrowedGcProfile::pins)
            admit(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : NativeStartGameProfile::Code)
            if (pin.rva == 0xab879c || pin.rva == 0xab92f0 || pin.rva == 0x1af4ef0)
                admit(pin.rva, pin.bytes.data(), pin.size, PAGE_EXECUTE_READ);
        for (const auto& slot : T::Slots)
            for (const auto& method : std::array<std::pair<std::size_t, std::uintptr_t>, 3>{
                     {{0, slot.deletingDestructor}, {T::SimpleSlot, slot.isSimple}, {T::TypeSlot, slot.getType}}})
            {
                const auto target = fixture.image + method.second;
                admit(slot.vtable + method.first, &target, sizeof(target), PAGE_READONLY);
            }
        const std::uintptr_t zero = 0;
        const std::uint32_t  state = 9, marker = 0;
        const auto           thread = GetCurrentThreadId();
        const unsigned char  initialized = 1;
        admit(Ped::RootRva, &zero, sizeof(zero), PAGE_READWRITE);
        admit(T::RootRva, &zero, sizeof(zero), PAGE_READWRITE);
        admit(NativeGameStateProfile::stateRva, &state, sizeof(state), PAGE_READWRITE);
        admit(NativeStartGameProfile::ThreadInitializedRva, &initialized, 1, PAGE_READWRITE);
        admit(NativeStartGameProfile::ThreadIdRva, &thread, sizeof(thread), PAGE_READWRITE);
        admit(NativeBorrowedGcProfile::markerRva, &marker, sizeof(marker), PAGE_READWRITE);
        ClockTeleportPool tasks(fixture, T::RootRva, T::NativeCapacity, T::ObjectStride);
        tasks.Publish();
        const std::array<std::size_t, T::Slots.size()> indexes{0, 7, 13};
        for (std::size_t i = 0; i != T::Slots.size(); ++i)
        {
            tasks.flags[indexes[i]] = static_cast<unsigned char>(3 + i);
            const auto table = fixture.image + T::Slots[i].vtable;
            std::memcpy(tasks.objects + indexes[i] * T::ObjectStride, &table, sizeof(table));
        }
        ClockReaderTestHost::image = fixture.image;
        TaskPoolOwnerContext context;
        context.task = reinterpret_cast<std::uintptr_t>(tasks.objects);
        context.value.nativeTask = 123;
        context.value.nativeType = 456;
        DefaultPedIntelligenceOwner absent;
        Check(absent.ReadTaskPoolEntry(context.task, context.value) == ClockResult::NotInitialized && context.value.nativeTask == 123 &&
                  context.value.nativeType == 456 && runtime.access.ReadTaskPoolEntry(context.task, context.value) == ClockResult::NotInitialized,
              "default/unbound task reads preserve output and create no SDK task or persistent lease");
        NativeClockService service;
        BeginOwned(service, fixture, false, true, true, &TaskPoolOwnerHandler, &context);
        Check(runtime.access.ReadTaskPoolEntry(context.task, context.value) != ClockResult::Ok && context.value.nativeTask == 123,
              "producer-side task address knowledge cannot substitute for the natural callback authority");
        const auto invoke = [&]
        {
            Check(service.Renew(runtime.generation.load()), "renew actual callback ownership between immediate native task reads");
            fixture.invokeTimer();
        };
        for (std::size_t i = 0; i != T::Slots.size(); ++i)
        {
            context.task = reinterpret_cast<std::uintptr_t>(tasks.objects + indexes[i] * T::ObjectStride);
            invoke();
            Check(context.result == ClockResult::Ok && context.value.nativeTask == context.task && context.value.slot == indexes[i] &&
                      context.value.vtable == fixture.image + T::Slots[i].vtable && context.value.nativeType == T::Slots[i].nativeType &&
                      context.value.simple == T::Slots[i].simple && context.value.getTypeMethod == fixture.image + T::Slots[i].getType &&
                      context.value.isSimpleMethod == fixture.image + T::Slots[i].isSimple,
                  "actual admitted callback observes each proved native task type/simple identity without invoking native methods");
        }
        context.value.nativeTask = 123;
        ClockResult                 foreign = ClockResult::Ok;
        NativeTaskPoolReader::Value foreignOutput;
        foreignOutput.nativeTask = 789;
        std::thread other([&] { foreign = runtime.access.ReadTaskPoolEntry(context.task, foreignOutput); });
        other.join();
        Check(foreign != ClockResult::Ok && foreignOutput.nativeTask == 789, "foreign thread cannot borrow a task snapshot from the retained native owner");
        fixture.Put(NativeStartGameProfile::ThreadIdRva, static_cast<DWORD>(thread + 1));
        invoke();
        Check(context.result != ClockResult::Ok && context.value.nativeTask == 123 && !runtime.stopping.load(),
              "real native GameThread mismatch refuses task output without pretending a read was an entered action");
        fixture.Put(NativeStartGameProfile::ThreadIdRva, thread);
        fixture.Put(NativeBorrowedGcProfile::markerRva, std::uint32_t{1});
        invoke();
        Check(context.result != ClockResult::Ok && context.value.nativeTask == 123, "native GC exclusion remains required for task identity");
        fixture.Put(NativeBorrowedGcProfile::markerRva, marker);
        fixture.Put(NativeGameStateProfile::stateRva, std::uint32_t{7});
        invoke();
        Check(context.result != ClockResult::Ok && context.value.nativeTask == 123, "native non-PLAYING state grants no current task observation");
        fixture.Put(NativeGameStateProfile::stateRva, state);
        const auto flag = tasks.flags[indexes[1]];
        tasks.flags[indexes[1]] = static_cast<unsigned char>(flag | T::EmptyBit);
        invoke();
        Check(context.result != ClockResult::Ok && context.value.nativeTask == 123, "a pool-retired task address does not publish a recycled native identity");
        tasks.flags[indexes[1]] = flag;
        ClockTaskPoolOwnerTest::address = context.task;
        ClockTaskPoolOwnerTest::flag = reinterpret_cast<std::uintptr_t>(tasks.flags + indexes[1]);
        ClockTaskPoolOwnerTest::changeOnRead = true;
        const auto changes = ClockTaskPoolOwnerTest::changes;
        invoke();
        Check(context.result != ClockResult::Ok && context.value.nativeTask == 123 && ClockTaskPoolOwnerTest::changes == changes + 1 &&
                  tasks.flags[indexes[1]] != flag && !runtime.stopping.load(),
              "real OS-read acknowledgement cannot publish a task whose occupied generation changed between captures");
        const auto advancedFlag = tasks.flags[indexes[1]];
        // These observations own no native reference cells or writes. Requiring
        // an extra callback would incorrectly invent deferred cleanup work.
        Check(service.End() && !service.IsRetained() && !runtime.timingHandler && !NativeServiceAdmission::IsClaimed(NativeServiceAdmission::Category::Clock),
              "task observations retire the same native module/hooks without deletion hooks or SDK objects");
        Check(tasks.flags[indexes[1]] == advancedFlag, "read-only task retirement preserves native pool advancement instead of rewinding its snapshot");
        Check(runtime.access.ReadTaskPoolEntry(context.task, context.value) != ClockResult::Ok && context.value.nativeTask == 123,
              "a retired owner supplies no later native task lifetime");
        ClockTaskPoolOwnerTest::changeOnRead = false;
        ClockTaskPoolOwnerTest::address = ClockTaskPoolOwnerTest::flag = 0;
        ClockReaderTestHost::image = 0;
        for (auto item = saved.rbegin(); item != saved.rend(); ++item)
            fixture.Patch(item->rva, item->bytes.data(), item->bytes.size());
        std::puts("Production native task pool callback owner fixture completed");
    }
}
