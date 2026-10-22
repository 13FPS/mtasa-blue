/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Destructive task intent uses the real retained ped/callback owner
 ****************************************************************************/
namespace
{
    struct TaskSecondaryOwnerContext
    {
        NativePedReferenceToken token;
        int                     index = 0;
        bool                    remove = false;
        ClockResult             result = ClockResult::NotInitialized;
    };
    void TaskSecondaryOwnerHandler(NativeGameTimingAccess& access, void* opaque)
    {
        auto& context = *static_cast<TaskSecondaryOwnerContext*>(opaque);
        context.result = context.remove ? access.RemoveReferencedPedSecondaryTask(context.token, context.index) : access.AcquirePedReference(1, context.token);
    }
    void NativeTaskSecondaryOwnedSdk(ClockImageFixture& fixture)
    {
        namespace R = NativePedReferenceProfile;
        namespace Ped = NativePedPoolProfile;
        namespace I = NativePedIntelligenceProfile;
        namespace T = NativeTaskPoolProfile;
        namespace D = NativeTaskRemovalProfile;
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
                  "save exact task-removal/reference proof spans in the existing owned callback image");
            saved.push_back(std::move(prior));
            fixture.Patch(rva, bytes, size);
            DWORD previous = 0;
            Check(::VirtualProtect(reinterpret_cast<void*>(fixture.image + rva), size, protection, &previous) != FALSE,
                  "task removal keeps native executable, immutable vtable and writable bank page contracts");
        };
        for (const auto& pin : Ped::Code)
            admit(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : R::Code)
            admit(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : I::Code)
            admit(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : T::Code)
            admit(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : D::Code)
            admit(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : NativeGameStateProfile::pins)
            admit(pin.rva, pin.bytes, pin.size, pin.code ? PAGE_EXECUTE_READ : PAGE_READONLY);
        for (const auto& pin : NativeBorrowedGcProfile::pins)
            admit(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : NativeStartGameProfile::Code)
            if (pin.rva == 0xab879c || pin.rva == 0xab92f0 || pin.rva == 0x1af4ef0)
                admit(pin.rva, pin.bytes.data(), pin.size, PAGE_EXECUTE_READ);
        for (const auto& slot : R::Slots)
        {
            const auto target = fixture.image + slot.targetRva;
            admit(slot.rva, &target, sizeof(target), PAGE_READONLY);
        }
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
        for (const auto root : {Ped::RootRva, I::RootRva, T::RootRva})
            admit(root, &zero, sizeof(zero), PAGE_READWRITE);
        admit(NativeGameStateProfile::stateRva, &state, sizeof(state), PAGE_READWRITE);
        admit(NativeStartGameProfile::ThreadInitializedRva, &initialized, 1, PAGE_READWRITE);
        admit(NativeStartGameProfile::ThreadIdRva, &thread, sizeof(thread), PAGE_READWRITE);
        admit(NativeBorrowedGcProfile::markerRva, &marker, sizeof(marker), PAGE_READWRITE);
        std::vector<unsigned char> nodes(R::ReferenceCount * R::ReferenceBytes);
        for (std::size_t i = 0; i + 1 < R::ReferenceCount; ++i)
        {
            const auto next = fixture.image + R::ReferencesRva + (i + 1) * R::ReferenceBytes;
            std::memcpy(nodes.data() + i * R::ReferenceBytes, &next, sizeof(next));
        }
        admit(R::ReferencesRva, nodes.data(), nodes.size(), PAGE_READWRITE);
        const auto first = fixture.image + R::ReferencesRva;
        admit(R::FreeHeadRva, &first, sizeof(first), PAGE_READWRITE);
        ClockTeleportPool peds(fixture, Ped::RootRva, Ped::NativeCapacity, Ped::ObjectStride);
        ClockTeleportPool intelligence(fixture, I::RootRva, I::NativeCapacity, I::ObjectStride);
        ClockTeleportPool tasks(fixture, T::RootRva, T::NativeCapacity, T::ObjectStride);
        peds.Publish();
        intelligence.Publish();
        tasks.Publish();
        peds.flags[0] = 1;
        intelligence.flags[3] = 19;
        const auto ped = reinterpret_cast<std::uintptr_t>(peds.objects);
        const auto nativeIntelligence = reinterpret_cast<std::uintptr_t>(intelligence.objects + 3 * I::ObjectStride);
        const auto manager = nativeIntelligence + I::ManagerOffset;
        const auto vtable = fixture.image + I::PedVtableRva;
        std::memcpy(peds.objects, &vtable, sizeof(vtable));
        peds.objects[I::TypeOffset] = 3;
        std::memcpy(peds.objects + I::IntelligenceOffset, &nativeIntelligence, sizeof(nativeIntelligence));
        std::array<std::uintptr_t, 13> prefix{};
        prefix.front() = prefix.back() = ped;
        std::memcpy(reinterpret_cast<void*>(nativeIntelligence), prefix.data(), sizeof(prefix));
        const auto secondary = [&](int index) { return nativeIntelligence + I::SecondaryOffset + static_cast<std::uintptr_t>(index) * sizeof(std::uintptr_t); };
        const auto slotWord = [&](int index) { return ClockPedReferenceOwnerTest::Get<std::uintptr_t>(secondary(index)); };
        const auto installTask = [&](int index, std::size_t kind, std::size_t row)
        {
            const auto task = reinterpret_cast<std::uintptr_t>(tasks.objects + row * T::ObjectStride);
            const auto table = fixture.image + T::Slots[kind].vtable;
            tasks.flags[row] = static_cast<unsigned char>((tasks.flags[row] + 1) & static_cast<unsigned char>(~T::EmptyBit));
            std::memcpy(reinterpret_cast<void*>(task), &table, sizeof(table));
            ClockPedReferenceOwnerTest::Put(task + D::ComplexChildOffset, zero);
            ClockPedReferenceOwnerTest::Put(secondary(index), task);
            ClockTaskRemovalOwnerTest::flag = reinterpret_cast<std::uintptr_t>(tasks.flags + row);
            return task;
        };
        ClockReaderTestHost::image = ClockPedReferenceOwnerTest::image = ClockTaskRemovalOwnerTest::image = fixture.image;
        ClockPedReferenceOwnerTest::partialUnlink = false;
        ClockPedReferenceOwnerTest::unregisterResult = ClockResult::Ok;
        ClockTaskRemovalOwnerTest::calls = ClockTaskRemovalOwnerTest::deletions = 0;
        ClockTaskRemovalOwnerTest::result = ClockResult::Ok;
        ClockTaskRemovalOwnerTest::clearReference = false;
        TaskSecondaryOwnerContext   context;
        DefaultPedIntelligenceOwner absent;
        Check(absent.RemoveReferencedPedSecondaryTask(context.token, 0) == ClockResult::NotInitialized &&
                  runtime.access.RemoveReferencedPedSecondaryTask(context.token, 0) == ClockResult::NotInitialized && !ClockTaskRemovalOwnerTest::calls,
              "default and unbound task-removal owners explicitly refuse without invoking a native entry");
        const auto retire = [&](NativeClockService& service)
        {
            const auto calls = ClockTaskRemovalOwnerTest::calls;
            if (!service.End())
            {
                fixture.invokeTimer();
                Check(service.End(), "natural callback retires the actual registered ped reference after task intent");
            }
            Check(!service.IsRetained() && !runtime.pedReferences.IsRetained() && !runtime.timingHandler &&
                      !NativeServiceAdmission::IsClaimed(NativeServiceAdmission::Category::Clock) && ClockTaskRemovalOwnerTest::calls == calls,
                  "task intent has no separate Runtime or restoration debt and cleanup never replays deletion");
        };
        {
            NativeClockService service;
            BeginOwned(service, fixture, false, true, true, &TaskSecondaryOwnerHandler, &context);
            const auto invoke = [&]
            {
                Check(service.Renew(runtime.generation.load()), "renew the same callback lease between secondary-task requests");
                fixture.invokeTimer();
            };
            invoke();
            Check(context.result == ClockResult::Ok && context.token.version && runtime.pedReferences.IsRetained(),
                  "task removal borrows genuine native-cell registration from the existing owner");
            Check(runtime.access.RemoveReferencedPedSecondaryTask(context.token, 0) != ClockResult::Ok && !ClockTaskRemovalOwnerTest::calls,
                  "a producer holding a valid CPU token cannot delete a task between admitted callbacks");
            ClockResult foreign = ClockResult::Ok;
            std::thread other([&] { foreign = runtime.access.RemoveReferencedPedSecondaryTask(context.token, 0); });
            other.join();
            Check(foreign != ClockResult::Ok && !ClockTaskRemovalOwnerTest::calls, "foreign threads receive no native task-deletion grant");
            context.remove = true;
            const auto token = context.token;
            ++context.token.version;
            invoke();
            Check(context.result != ClockResult::Ok && !ClockTaskRemovalOwnerTest::calls && !runtime.stopping.load(),
                  "stale token refusal is pre-entry and does not stop an unrelated admitted owner");
            context.token = token;
            for (const auto invalid : {-1, 6, 2147483647})
            {
                context.index = invalid;
                invoke();
                Check(context.result == ClockResult::InvalidArgument && !ClockTaskRemovalOwnerTest::calls && !runtime.stopping.load(),
                      "native unchecked secondary indexes are refused without clamping or entry");
            }
            context.index = 0;
            invoke();
            Check(context.result == ClockResult::Ok && !ClockTaskRemovalOwnerTest::calls && !runtime.stopping.load(),
                  "an empty manager cell acknowledges a no-op without fabricating native deletion entry");
            for (std::size_t kind = 0; kind < T::Slots.size(); ++kind)
            {
                context.index = kind ? 5 : 0;
                const auto task = installTask(context.index, kind, kind + 7);
                const auto calls = ClockTaskRemovalOwnerTest::calls;
                invoke();
                Check(context.result == ClockResult::Ok && ClockTaskRemovalOwnerTest::calls == calls + 1 && !slotWord(context.index) &&
                          (tasks.flags[kind + 7] & T::EmptyBit) && ClockTaskRemovalOwnerTest::entry == D::SecondaryRva &&
                          ClockTaskRemovalOwnerTest::manager == manager && ClockTaskRemovalOwnerTest::index == context.index &&
                          ClockTaskRemovalOwnerTest::sawLiveManagerCellDuringDeletion && !runtime.stopping.load(),
                      "actual callback routes both bounded native task classes through the exact manager/index ABI and acknowledges deletion-before-clear");
                Check(task != 0, "accepted task addresses were real owned pool rows rather than SDK wrappers");
            }
            const auto deletions = ClockTaskRemovalOwnerTest::deletions;
            retire(service);
            Check(ClockTaskRemovalOwnerTest::deletions == deletions && !slotWord(0) && !slotWord(5),
                  "End preserves accepted secondary deletion and never restores task pointers");
        }
        {
            context.remove = false;
            context.index = 2;
            installTask(context.index, 0, 12);
            NativeClockService service;
            BeginOwned(service, fixture, false, true, true, &TaskSecondaryOwnerHandler, &context);
            fixture.invokeTimer();
            Check(context.result == ClockResult::Ok, "new deletion generation acquires its own registered ped token");
            const auto calls = ClockTaskRemovalOwnerTest::calls;
            ClockTaskRemovalOwnerTest::result = ClockResult::OriginalException;
            context.remove = true;
            Check(service.Renew(runtime.generation.load()), "renew before one entered native deletion reports a late failure");
            fixture.invokeTimer();
            Check(context.result == ClockResult::OriginalException && runtime.stopping.load() && ClockTaskRemovalOwnerTest::calls == calls + 1 &&
                      !slotWord(context.index) && (tasks.flags[12] & T::EmptyBit),
                  "entered native failure stops the real owner while preserving its already accepted task retirement and manager clear");
            retire(service);
            Check(ClockTaskRemovalOwnerTest::calls == calls + 1 && !slotWord(context.index),
                  "failure cleanup does not retry deletion or restore its prior pointer snapshot");
            ClockTaskRemovalOwnerTest::result = ClockResult::Ok;
        }
        {
            context.remove = false;
            context.index = 3;
            installTask(context.index, 1, 13);
            NativeClockService service;
            BeginOwned(service, fixture, false, true, true, &TaskSecondaryOwnerHandler, &context);
            fixture.invokeTimer();
            Check(context.result == ClockResult::Ok, "post-token-loss experiment starts with actual independent native registration");
            const auto calls = ClockTaskRemovalOwnerTest::calls;
            const auto unlinks = ClockPedReferenceOwnerTest::unregisters;
            const auto node = ClockPedReferenceOwnerTest::node;
            ClockTaskRemovalOwnerTest::referenceCell = ClockPedReferenceOwnerTest::cell;
            ClockTaskRemovalOwnerTest::clearReference = true;
            context.remove = true;
            Check(service.Renew(runtime.generation.load()), "renew before a completed deletion loses only its final token acknowledgement");
            fixture.invokeTimer();
            Check(context.result != ClockResult::Ok && runtime.stopping.load() && ClockTaskRemovalOwnerTest::calls == calls + 1 && !slotWord(context.index) &&
                      (tasks.flags[13] & T::EmptyBit) && runtime.pedReferences.IsRetained(),
                  "post-call registered-token invalidation stops the owner despite a successful raw deletion and preserves its effects");
            Check(!service.End(), "zero reference word retains native-visible cell storage until separate address retirement is proved");
            fixture.invokeTimer();
            Check(ClockTaskRemovalOwnerTest::calls == calls + 1 && ClockPedReferenceOwnerTest::unregisters == unlinks && runtime.pedReferences.IsRetained(),
                  "stopping callbacks neither replay accepted deletion nor guess unlink authority from an empty weak-reference word");
            // Model only the separately proved native reference-bank retirement
            // postcondition; task state remains deleted throughout this step.
            ClockPedReferenceOwnerTest::Put(node + 8, zero);
            const auto head = ClockPedReferenceOwnerTest::Get<std::uintptr_t>(fixture.image + R::FreeHeadRva);
            ClockPedReferenceOwnerTest::Put(node, head);
            ClockPedReferenceOwnerTest::Put(fixture.image + R::FreeHeadRva, node);
            ClockPedReferenceOwnerTest::Put(ped + R::EntityHeadOffset, zero);
            fixture.invokeTimer();
            retire(service);
            Check(ClockTaskRemovalOwnerTest::calls == calls + 1 && ClockPedReferenceOwnerTest::unregisters == unlinks && !slotWord(context.index),
                  "full-bank address absence releases the existing owner without replay, task restoration or fabricated wrapper lifetime");
        }
        ClockTaskRemovalOwnerTest::flag = ClockTaskRemovalOwnerTest::referenceCell = ClockTaskRemovalOwnerTest::image = 0;
        ClockReaderTestHost::image = ClockPedReferenceOwnerTest::image = 0;
        for (auto item = saved.rbegin(); item != saved.rend(); ++item)
            fixture.Patch(item->rva, item->bytes.data(), item->bytes.size());
        std::puts("Production native secondary-task removal callback owner fixture completed");
    }
}
