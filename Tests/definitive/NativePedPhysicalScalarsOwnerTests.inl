/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Raw native mass intent through the same retained reference owner
 ****************************************************************************/
namespace
{
    struct PedPhysicalScalarsOwnerContext
    {
        NativePedReferenceToken      token;
        NativePedPhysicalScalarField field = NativePedPhysicalScalarField::Mass;
        ClockResult                  result = ClockResult::NotInitialized;
        unsigned                     operation = 0;
        bool                         duckingOutput = true;
        bool                         flagBoolean = true;
        std::uint32_t                flagInput = 0, flagOutput = 0x12345678;
        float                        target = 0, output = 123;
    };
    void PedPhysicalScalarsOwnerHandler(NativeGameTimingAccess& access, void* opaque)
    {
        auto& context = *static_cast<PedPhysicalScalarsOwnerContext*>(opaque);
        switch (context.operation)
        {
            case 0:
                context.result = access.AcquirePedReference(1, context.token);
                break;
            case 1:
                context.result = access.ReadPedPhysicalScalar(context.token, context.field, context.output);
                break;
            case 2:
                context.result = access.SetPedPhysicalScalar(context.token, context.field, context.target);
                break;
            case 3:
                context.result = access.ReadPedCurrentRotation(context.token, context.output);
                break;
            case 4:
                context.result = access.SetPedCurrentRotation(context.token, context.target);
                break;
            case 5:
                context.result = access.ReadPedOxygenLevel(context.token, context.output);
                break;
            case 6:
                context.result = access.SetPedOxygenLevel(context.token, context.target);
                break;
            case 7:
                context.result = access.ReadPedDucking(context.token, context.duckingOutput);
                break;
            case 8:
                context.result = access.ReadPedCantBeKnockedOffBike(context.token, context.flagOutput);
                break;
            case 9:
                context.result = access.SetPedCantBeKnockedOffBike(context.token, context.flagInput);
                break;
            case 10:
                context.result = access.ReadPedCanBeShotInVehicle(context.token, context.duckingOutput);
                break;
            case 11:
                context.result = access.SetPedCanBeShotInVehicle(context.token, context.flagBoolean);
                break;
            default:
                std::abort();
        }
    }
    void NativePedPhysicalScalarsOwnedSdk(ClockImageFixture& fixture)
    {
        namespace S = NativePedPhysicalScalarsProfile;
        namespace R = NativePedReferenceProfile;
        namespace Ped = NativePedPoolProfile;
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
                  "save only the bounded scalar/reference native proof spans in the existing actual callback fixture");
            saved.push_back(std::move(prior));
            fixture.Patch(rva, bytes, size);
            DWORD previous = 0;
            Check(::VirtualProtect(reinterpret_cast<void*>(fixture.image + rva), size, protection, &previous) != FALSE,
                  "mass owner preserves actual executable, constant, relocated-slot and data page policies");
        };
        for (const auto& pin : Ped::Code)
            admit(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : R::Code)
            admit(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : S::Code)
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
        admit(S::InverseMassOneRva, &S::InverseMassOneBits, sizeof(S::InverseMassOneBits), PAGE_READONLY);
        const std::uintptr_t zero = 0;
        const std::uint32_t  state = 9, marker = 0;
        const auto           thread = GetCurrentThreadId();
        const unsigned char  initialized = 1;
        admit(Ped::RootRva, &zero, sizeof(zero), PAGE_READWRITE);
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
        // This exact occupied native ped needs no current-player row or model
        // factory. Mass does not acquire a different callback or lifetime owner.
        ClockTeleportPool pool(fixture, Ped::RootRva, Ped::NativeCapacity, Ped::ObjectStride);
        pool.Publish();
        pool.flags[0] = 1;
        const auto vtable = fixture.image + R::PedVtableRva;
        std::memcpy(pool.objects, &vtable, sizeof(vtable));
        pool.objects[S::TypeOffset] = 3;
        const auto store = [&](std::size_t offset, std::uint32_t value) { std::memcpy(pool.objects + offset, &value, sizeof(value)); };
        const auto raw = [&](std::size_t offset)
        {
            std::uint32_t value = 0;
            std::memcpy(&value, pool.objects + offset, sizeof(value));
            return value;
        };
        const auto floatBits = [](float value)
        {
            std::uint32_t bits = 0;
            std::memcpy(&bits, &value, sizeof(bits));
            return bits;
        };
        const auto fromBits = [](std::uint32_t value)
        {
            float result = 0;
            std::memcpy(&result, &value, sizeof(result));
            return result;
        };
        store(S::MassOffset, 0x3f800001);
        store(S::TurnMassOffset, 0x40000001);
        store(S::TurnMassOffset + 4, 0x42a50000);
        ClockReaderTestHost::image = ClockPedReferenceOwnerTest::image = fixture.image;
        ClockPedReferenceOwnerTest::partialUnlink = false;
        ClockPedReferenceOwnerTest::unregisterResult = ClockResult::Ok;
        ClockPedPhysicalScalarsOwnerTest::stores = 0;
        ClockPedPhysicalScalarsOwnerTest::cell = 0;
        ClockPedPhysicalScalarsOwnerTest::loseToken = false;
        PedPhysicalScalarsOwnerContext context;
        Check(runtime.access.ReadPedPhysicalScalar(context.token, context.field, context.output) == ClockResult::NotInitialized && context.output == 123 &&
                  runtime.access.SetPedPhysicalScalar(context.token, context.field, 7) == ClockResult::NotInitialized &&
                  !ClockPedPhysicalScalarsOwnerTest::stores,
              "unbound native physical scalars preserve caller output and grant no implicit mutation scope");
        const auto retire = [&](NativeClockService& service)
        {
            if (!service.End())
            {
                fixture.invokeTimer();
                Check(service.End(), "the same admitted callback retires its physical-scalar reference cell");
            }
            Check(!service.IsRetained() && !runtime.pedReferences.IsRetained() && !runtime.timingHandler &&
                      !NativeServiceAdmission::IsClaimed(NativeServiceAdmission::Category::Clock),
                  "scalar retirement acknowledges native cell storage, hooks and owner admission together");
        };
        {
            NativeClockService service;
            BeginOwned(service, fixture, false, true, true, &PedPhysicalScalarsOwnerHandler, &context);
            const auto invoke = [&]()
            {
                Check(service.Renew(runtime.generation.load()), "renew the actual callback lease between independent scalar requests");
                fixture.invokeTimer();
            };
            invoke();
            Check(context.result == ClockResult::Ok && context.token.version && runtime.pedReferences.IsRetained(),
                  "native physical scalars borrow a token issued only after verified native reference registration");
            Check(runtime.access.ReadPedPhysicalScalar(context.token, context.field, context.output) != ClockResult::Ok && context.output == 123 &&
                      runtime.access.SetPedPhysicalScalar(context.token, context.field, 7) != ClockResult::Ok && !ClockPedPhysicalScalarsOwnerTest::stores,
                  "a valid token cannot lend native scalar authority to a producer between callbacks");
            context.operation = 1;
            invoke();
            Check(context.result == ClockResult::Ok && floatBits(context.output) == 0x3f800001 && !runtime.playerInfo.IsBound(),
                  "the actual callback reads generic-ped mass without a current-player binding or unit conversion");
            context.operation = 2;
            context.target = fromBits(0x7fc12345);
            invoke();
            Check(context.result == ClockResult::Ok && raw(S::MassOffset) == 0x7fc12345 && raw(S::TurnMassOffset) == 0x40000001 &&
                      raw(S::TurnMassOffset + 4) == 0x42a50000 && ClockPedPhysicalScalarsOwnerTest::stores == 1,
                  "the actual owner commits raw mass NaN payload while leaving adjacent physical scalars untouched");
            const auto valid = context.token;
            ++context.token.version;
            context.output = 123;
            context.operation = 1;
            invoke();
            Check(context.result != ClockResult::Ok && context.output == 123 && ClockPedPhysicalScalarsOwnerTest::stores == 1 && !runtime.stopping.load(),
                  "a stale scalar token refuses output publication without stopping the unrelated valid owner");
            context.token = valid;
            context.operation = 2;
            context.field = NativePedPhysicalScalarField::TurnMass;
            context.target = fromBits(0x80000000);
            invoke();
            Check(context.result == ClockResult::Ok && raw(S::TurnMassOffset) == 0x80000000 && raw(S::MassOffset) == 0x7fc12345 &&
                      ClockPedPhysicalScalarsOwnerTest::stores == 2,
                  "the same native owner preserves signed-zero turn-mass without changing accepted mass");
            context.operation = 1;
            invoke();
            Check(context.result == ClockResult::Ok && floatBits(context.output) == 0x80000000,
                  "token output is committed only after the complete native scalar read and token postcheck");
            retire(service);
            Check(raw(S::MassOffset) == 0x7fc12345 && raw(S::TurnMassOffset) == 0x80000000,
                  "End retires only reference lifetime and preserves accepted native physical scalar intent");
        }
        {
            context.operation = 0;
            NativeClockService service;
            BeginOwned(service, fixture, false, true, true, &PedPhysicalScalarsOwnerHandler, &context);
            fixture.invokeTimer();
            Check(context.result == ClockResult::Ok, "a new generation independently acquires its physical scalar token");
            const auto stores = ClockPedPhysicalScalarsOwnerTest::stores;
            const auto unlinks = ClockPedReferenceOwnerTest::unregisters;
            const auto node = ClockPedReferenceOwnerTest::node;
            ClockPedPhysicalScalarsOwnerTest::cell = ClockPedReferenceOwnerTest::cell;
            ClockPedPhysicalScalarsOwnerTest::loseToken = true;
            context.operation = 2;
            context.field = NativePedPhysicalScalarField::Mass;
            context.target = fromBits(0xffc07654);
            Check(service.Renew(runtime.generation.load()), "renew before one accepted scalar loses token acknowledgement");
            fixture.invokeTimer();
            Check(context.result != ClockResult::Ok && runtime.stopping.load() && ClockPedPhysicalScalarsOwnerTest::stores == stores + 1 &&
                      raw(S::MassOffset) == 0xffc07654 && raw(S::TurnMassOffset) == 0x80000000 && !ClockPedPhysicalScalarsOwnerTest::loseToken,
                  "post-token loss stops after the scalar succeeded and preserves its accepted native mass bits");
            Check(runtime.pedReferences.IsRetained() && service.IsRetained() &&
                      ClockPedReferenceOwnerTest::Get<std::uintptr_t>(node + 8) == ClockPedPhysicalScalarsOwnerTest::cell && !service.End(),
                  "zero reference word cannot free scalar-owner storage while a native node still names its address");
            fixture.invokeTimer();
            Check(ClockPedPhysicalScalarsOwnerTest::stores == stores + 1 && ClockPedReferenceOwnerTest::unregisters == unlinks &&
                      runtime.pedReferences.IsRetained() && !service.End(),
                  "stopping callbacks retain the cell debt without replaying scalar mutation or native unlink");
            // Only the already-proved ResolveReferences address-retirement
            // postcondition can discharge this debt. Accepted mass is never
            // restored while the reference bank is brought to that state.
            ClockPedReferenceOwnerTest::Put(node + 8, std::uintptr_t{0});
            const auto head = ClockPedReferenceOwnerTest::Get<std::uintptr_t>(fixture.image + R::FreeHeadRva);
            ClockPedReferenceOwnerTest::Put(node, head);
            ClockPedReferenceOwnerTest::Put(fixture.image + R::FreeHeadRva, node);
            ClockPedReferenceOwnerTest::Put(reinterpret_cast<std::uintptr_t>(pool.objects) + R::EntityHeadOffset, std::uintptr_t{0});
            fixture.invokeTimer();
            retire(service);
            Check(ClockPedPhysicalScalarsOwnerTest::stores == stores + 1 && ClockPedReferenceOwnerTest::unregisters == unlinks &&
                      raw(S::MassOffset) == 0xffc07654 && raw(S::TurnMassOffset) == 0x80000000 && raw(S::TurnMassOffset + 4) == 0x42a50000,
                  "bank-wide address absence finishes retirement with no scalar rollback or action replay");
        }
#include "NativePedRotationOwnerTests.inl"
#include "NativePedOxygenStateOwnerTests.inl"
#include "NativePedFlagsOwnerTests.inl"
        ClockPedPhysicalScalarsOwnerTest::cell = 0;
        ClockPedPhysicalScalarsOwnerTest::loseToken = false;
        ClockReaderTestHost::image = ClockPedReferenceOwnerTest::image = 0;
        for (auto item = saved.rbegin(); item != saved.rend(); ++item)
            fixture.Patch(item->rva, item->bytes.data(), item->bytes.size());
        std::puts("Production native ped physical scalars callback owner fixture completed");
    }
}
