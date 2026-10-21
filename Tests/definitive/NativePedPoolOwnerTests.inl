/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Same natural Runtime ownership for qualified native ped references
 ****************************************************************************/
namespace
{
    struct PedPoolOwnerContext
    {
        std::uint32_t                   handle = 1;
        unsigned                        operation = 1, refusal = 0;
        ClockResult                     result = ClockResult::NotInitialized;
        NativePedPoolReader::Value      value;
        NativePedTransformReader::Value transform;
        CVector*                        position = nullptr;
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
        switch (context.operation)
        {
            case 1:
                context.result = access.ResolvePedPoolHandle(context.handle, context.value);
                break;
            case 2:
                context.result = access.RecheckPedPoolHandle(context.value);
                break;
            case 3:
                context.result = access.ReadPedTransform(context.handle, context.transform);
                break;
            case 4:
                context.result = access.RecheckPedTransform(context.transform);
                break;
            case 5:
                context.result = access.BorrowPedPosition(context.handle, context.position);
                break;
        }
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
        namespace Transform = NativePedTransformProfile;
        for (const auto& pin : Transform::Code)
        {
            fixture.Patch(pin.rva, pin.bytes, pin.size);
            DWORD old = 0;
            Check(::VirtualProtect(reinterpret_cast<void*>(fixture.image + pin.rva), pin.size, PAGE_EXECUTE_READ, &old) != FALSE,
                  "admit the independent ped transform instruction identities in the owned callback fixture");
        }
        const auto savedPedTeleport = fixture.Get<std::uintptr_t>(Transform::PedVtableRva + Transform::TeleportSlotOffset);
        const auto savedPlayerTeleport = fixture.Get<std::uintptr_t>(Transform::PlayerVtableRva + Transform::TeleportSlotOffset);
        const auto savedVelocityIn = fixture.Get<std::uint32_t>(Transform::ScriptVelocityInScaleRva);
        const auto savedVelocityOut = fixture.Get<std::uint32_t>(Transform::ScriptVelocityOutScaleRva);
        const auto teleport = fixture.image + Transform::NativeTeleportRva;
        fixture.Patch(Transform::PedVtableRva + Transform::TeleportSlotOffset, &teleport, sizeof(teleport));
        fixture.Patch(Transform::PlayerVtableRva + Transform::TeleportSlotOffset, &teleport, sizeof(teleport));
        fixture.Patch(Transform::ScriptVelocityInScaleRva, &Transform::ScriptVelocityInScaleBits, sizeof(std::uint32_t));
        fixture.Patch(Transform::ScriptVelocityOutScaleRva, &Transform::ScriptVelocityOutScaleBits, sizeof(std::uint32_t));
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
        // These reads must use the same admitted callback as handle resolution;
        // a standalone checked reader must not become an unowned second path.
        const CVector inlinePosition{11, 22, 33}, velocity{0.125f, -0.25f, 0.5f};
        std::memcpy(objects + Transform::InlinePositionOffset, &inlinePosition, sizeof(inlinePosition));
        std::memcpy(objects + Transform::VelocityOffset, &velocity, sizeof(velocity));
        context.operation = 3;
        Check(runtime.access.ReadPedTransform(1, context.transform) != ClockResult::Ok,
              "producer access cannot borrow a ped transform outside the real natural callback");
        invoke();
        Check(context.result == ClockResult::Ok && context.transform.matrix == 0 && context.transform.position.fX == 11 &&
                  context.transform.velocity.fY == -0.25f,
              "the actual callback reads inline position and raw native velocity without script scaling");
        context.operation = 5;
        invoke();
        Check(context.result == ClockResult::Ok && context.position == reinterpret_cast<CVector*>(objects + Transform::InlinePositionOffset),
              "the actual callback borrows the genuine inline position address rather than a cached vector");
        auto* matrix = static_cast<unsigned char*>(::VirtualAlloc(nullptr, 0x60, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        Check(matrix != nullptr, "retain a test-owned native-style matrix for the callback borrow alternative");
        const auto    nativePed = reinterpret_cast<std::uintptr_t>(objects);
        const auto    matrixAddress = reinterpret_cast<std::uintptr_t>(matrix);
        const CVector matrixPosition{44, 55, 66};
        std::memcpy(matrix + Transform::MatrixOwnerOffset, &nativePed, sizeof(nativePed));
        std::memcpy(matrix + Transform::MatrixPositionOffset, &matrixPosition, sizeof(matrixPosition));
        std::memcpy(objects + Transform::MatrixPointerOffset, &matrixAddress, sizeof(matrixAddress));
        context.operation = 4;
        invoke();
        Check(context.result == ClockResult::BindingChanged, "a matrix replacement invalidates the previous inline transform observation");
        context.operation = 3;
        invoke();
        Check(context.result == ClockResult::Ok && context.transform.matrix == matrixAddress && context.transform.position.fX == 44,
              "the callback follows the current genuine matrix only after checking its ped owner");
        context.operation = 5;
        invoke();
        Check(context.result == ClockResult::Ok && context.position == reinterpret_cast<CVector*>(matrix + Transform::MatrixPositionOffset),
              "matrix-backed position preserves the genuine native address through the actual owner");
        for (unsigned refusal = 1; refusal <= 4; ++refusal)
        {
            context.refusal = refusal;
            auto* sentinel = context.position;
            invoke();
            Check(context.result != ClockResult::Ok && context.position == sentinel,
                  "stale/nested/expired callback scopes cannot publish or replace a borrowed native position");
        }
        context.refusal = 0;
        const std::uintptr_t absentMatrix = 0;
        std::memcpy(matrix + Transform::MatrixOwnerOffset, &absentMatrix, sizeof(absentMatrix));
        invoke();
        Check(context.result == ClockResult::BindingChanged, "a recycled matrix owner is refused by the actual native callback");
        std::memcpy(objects + Transform::MatrixPointerOffset, &absentMatrix, sizeof(absentMatrix));
        Check(::VirtualFree(matrix, 0, MEM_RELEASE) != FALSE, "release only the test-owned matrix after its native selector is cleared");
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
        fixture.Patch(Transform::PedVtableRva + Transform::TeleportSlotOffset, &savedPedTeleport, sizeof(savedPedTeleport));
        fixture.Patch(Transform::PlayerVtableRva + Transform::TeleportSlotOffset, &savedPlayerTeleport, sizeof(savedPlayerTeleport));
        fixture.Patch(Transform::ScriptVelocityInScaleRva, &savedVelocityIn, sizeof(savedVelocityIn));
        fixture.Patch(Transform::ScriptVelocityOutScaleRva, &savedVelocityOut, sizeof(savedVelocityOut));
        Check(::VirtualFree(flags, 0, MEM_RELEASE) && ::VirtualFree(objects, 0, MEM_RELEASE) && ::VirtualFree(header, 0, MEM_RELEASE),
              "release only test-owned banks after all real callbacks and pointer consumers have retired");
        Check(::VirtualProtect(reinterpret_cast<void*>(vtable), 8, PAGE_READONLY, &ignored) != FALSE, "restore fixture read-only player table page");
        ClockReaderTestHost::image = 0;
        std::puts("Production native ped-pool handle callback owner fixture completed");
        std::puts("Production native ped-transform callback owner fixture completed");
    }
}
