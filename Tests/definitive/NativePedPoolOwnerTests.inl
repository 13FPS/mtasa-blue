/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Same natural Runtime ownership for qualified native ped references
 ****************************************************************************/
namespace
{
    struct PedPoolOwnerContext
    {
        std::uint32_t              handle = 1;
        unsigned                   operation = 1, refusal = 0;
        ClockResult                result = ClockResult::NotInitialized;
        NativePedPoolReader::Value value;
    };
    void PedPoolOwnerHandler(NativeGameTimingAccess& access, void* opaque)
    {
        auto&      context = *static_cast<PedPoolOwnerContext*>(opaque);
        const auto generation = timingCallbackGeneration;
        const auto admission = runtime.admissionGeneration.load();
        const auto refreshed = runtime.refreshed.load();
        if (context.refusal == 1)
            timingCallbackGeneration = generation + 1;
        if (context.refusal == 2)
            runtime.admissionGeneration.store(admission + 1);
        if (context.refusal == 3)
            ++callbackDepth;
        if (context.refusal == 4)
            runtime.refreshed.store(GetTickCount64() - ClockServiceLeaseMsV1 - 1);
        context.result = context.operation == 1 ? access.ResolvePedPoolHandle(context.handle, context.value) : access.RecheckPedPoolHandle(context.value);
        runtime.refreshed.store(refreshed);
        if (context.refusal == 3)
            --callbackDepth;
        runtime.admissionGeneration.store(admission);
        timingCallbackGeneration = generation;
    }
    void NativePedPoolOwnedReads(ClockImageFixture& fixture)
    {
        namespace Ped = NativePedPoolProfile;
        // Only previously qualified instruction bytes are populated; none is
        // executed. The existing Clock fixture already supplies genuine image
        // sections and actual post-original timer callback admission.
        for (const auto& pin : Ped::Code)
        {
            fixture.Patch(pin.rva, pin.bytes, pin.size);
            DWORD old = 0;
            Check(::VirtualProtect(reinterpret_cast<void*>(fixture.image + pin.rva), pin.size, PAGE_EXECUTE_READ, &old) != FALSE,
                  "admit only the bounded owned native ped-handle profile code pages");
        }
        const auto vtable = fixture.image + NativeStatsWriteProfile::PlayerVtableRva;
        const auto savedMethod = fixture.Get<std::uintptr_t>(NativeStatsWriteProfile::PlayerVtableRva);
        const auto method = fixture.image + Ped::Code[1].rva;
        fixture.Patch(NativeStatsWriteProfile::PlayerVtableRva, &method, sizeof(method));
        DWORD old = 0, ignored = 0;
        Check(::VirtualProtect(reinterpret_cast<void*>(vtable), 8, PAGE_READONLY, &old) != FALSE,
              "the controlled ped vtable uses a real read-only image page and executable native entry identity");
        auto* header = static_cast<unsigned char*>(::VirtualAlloc(nullptr, 32, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        auto* objects = static_cast<unsigned char*>(::VirtualAlloc(nullptr, Ped::NativeCapacity * Ped::ObjectStride, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        auto* flags = static_cast<unsigned char*>(::VirtualAlloc(nullptr, Ped::NativeCapacity, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        Check(header && objects && flags, "retain native-style separate owned pool/header/generation allocations for the real callback reader");
        const auto put = [=](std::size_t offset, const auto& value) { std::memcpy(header + offset, &value, sizeof(value)); };
        put(0, reinterpret_cast<std::uintptr_t>(objects));
        put(8, reinterpret_cast<std::uintptr_t>(flags));
        put(16, static_cast<std::int32_t>(Ped::NativeCapacity));
        put(20, std::int32_t{-1});
        put(24, static_cast<unsigned char>(1));
        std::memset(flags, 0x80, Ped::NativeCapacity);
        flags[0] = 1;
        std::memcpy(objects, &vtable, sizeof(vtable));
        const auto root = reinterpret_cast<std::uintptr_t>(header);
        const auto savedRoot = fixture.Get<std::uintptr_t>(Ped::RootRva);
        Check(::VirtualProtect(reinterpret_cast<void*>(fixture.image + Ped::RootRva), 8, PAGE_READWRITE, &old) != FALSE,
              "only the owned native pool-global page is admitted writable before this focused cohort");
        fixture.Put(Ped::RootRva, root);
        ClockReaderTestHost::image = fixture.image;
        PedPoolOwnerContext context;
        context.value.handle = 0xdeadbeef;
        context.value.nativePed = 123;
        const auto untouched = [&] { return context.value.handle == 0xdeadbeef && context.value.nativePed == 123; };
        Check(runtime.access.ResolvePedPoolHandle(1, context.value) == ClockResult::NotInitialized && untouched(),
              "unbound callback access cannot turn a real native pool into a fabricated SDK player");
        NativeClockService service;
        BeginOwned(service, fixture, false, true, true, &PedPoolOwnerHandler, &context);
        const auto invoke = [&]
        {
            Check(service.Renew(runtime.generation.load()), "renew the actual native owner between independent ped-handle observations");
            fixture.invokeTimer();
        };
        Check(runtime.access.ResolvePedPoolHandle(1, context.value) != ClockResult::Ok && untouched(),
              "producer adoption and matching thread ID still do not grant a pool observation between callbacks");
        invoke();
        Check(context.result == ClockResult::Ok && context.value.handle == 1 && context.value.slot == 0 && context.value.generation == 1 &&
                  context.value.pool == root && context.value.nativePed == reinterpret_cast<std::uintptr_t>(objects),
              "the real natural callback resolves handle1 to its actual occupied pool slot0 generation1");
        context.operation = 2;
        invoke();
        Check(context.result == ClockResult::Ok, "the same admitted callback can freshly recheck its consumed native identity");
        context.operation = 1;
        for (unsigned refusal = 1; refusal <= 4; ++refusal)
        {
            context.value.handle = 0xdeadbeef;
            context.value.nativePed = 123;
            context.refusal = refusal;
            invoke();
            Check(context.result != ClockResult::Ok && untouched(), "stale/admission/nested/expired real callback scopes preserve prior caller output");
        }
        context.refusal = 0;
        bool        foreignRefused = false;
        std::thread foreign(
            [&]
            {
                NativePedPoolReader::Value value;
                value.handle = 0xdeadbeef;
                value.nativePed = 123;
                foreignRefused = runtime.access.ResolvePedPoolHandle(1, value) != ClockResult::Ok && value.handle == 0xdeadbeef && value.nativePed == 123;
            });
        foreign.join();
        Check(foreignRefused, "a foreign thread cannot borrow the real ped pool's callback admission");
        invoke();
        context.operation = 2;
        flags[0] = 2;
        invoke();
        Check(context.result != ClockResult::Ok, "native generation advancement revokes the previous callback observation");
        context.operation = 1;
        context.handle = 2;
        invoke();
        Check(context.result == ClockResult::Ok && context.value.handle == 2 && context.value.generation == 2,
              "the same native slot is observed only under its current valid handle, never through ambient actor fallback");
        flags[0] = 0x80;
        context.handle = 0x80;
        context.value.handle = 0xdeadbeef;
        context.value.nativePed = 123;
        invoke();
        Check(context.result == ClockResult::NotInitialized && untouched(),
              "a crafted free-slot handle cannot be published even when its low byte compares equal");
        const std::uintptr_t absent = 0;
        fixture.Put(Ped::RootRva, absent);
        context.handle = 1;
        invoke();
        Check(context.result == ClockResult::NotInitialized && untouched(),
              "absent native pool remains explicit in the actual owner rather than meaning local player ready");
        if (!service.End())
        {
            fixture.invokeTimer();
            Check(service.End(), "read-only native ped facts retain no pool mutation journal before the real callback hooks retire");
        }
        Check(!service.IsRetained() && !runtime.timingHandler && !runtime.timingContext && !runtime.hooked.load() && !runtime.timerHooked.load(),
              "pool observation adds no independent callback owner or deferred native pointer lifetime");
        Check(runtime.access.ResolvePedPoolHandle(1, context.value) == ClockResult::NotInitialized && untouched(),
              "retired owner cannot resurrect its prior native pool observation");
        fixture.Put(Ped::RootRva, savedRoot);
        fixture.Patch(NativeStatsWriteProfile::PlayerVtableRva, &savedMethod, sizeof(savedMethod));
        Check(::VirtualFree(flags, 0, MEM_RELEASE) && ::VirtualFree(objects, 0, MEM_RELEASE) && ::VirtualFree(header, 0, MEM_RELEASE),
              "release only test-owned banks after all real callbacks and pointer consumers have retired");
        Check(::VirtualProtect(reinterpret_cast<void*>(vtable), 8, PAGE_READONLY, &ignored) != FALSE, "restore fixture read-only player table page");
        ClockReaderTestHost::image = 0;
        std::puts("Production native ped-pool handle callback owner fixture completed");
    }
}
