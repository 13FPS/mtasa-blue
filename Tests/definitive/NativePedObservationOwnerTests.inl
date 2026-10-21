/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Immediate physical observations through real registered token life
 ****************************************************************************/
namespace
{
    struct PedObservationOwnerContext
    {
        NativePedReferenceToken         token;
        NativePedTransformReader::Value transform;
        CVector*                        position = reinterpret_cast<CVector*>(std::uintptr_t{1});
        CVector                         velocity{101, 102, 103}, raw;
        NativePedVelocityKind           kind = NativePedVelocityKind::Linear;
        ClockResult                     result = ClockResult::NotInitialized, rawResult = ClockResult::NotInitialized;
        unsigned                        operation = 0;
        bool                            alsoRaw = false;
    };
    void PedObservationOwnerHandler(NativeGameTimingAccess& access, void* opaque)
    {
        auto& context = *static_cast<PedObservationOwnerContext*>(opaque);
        switch (context.operation)
        {
            case 0:
                context.result = access.AcquirePedReference(1, context.token);
                break;
            case 1:
                context.result = access.ReadReferencedPedTransform(context.token, context.transform);
                break;
            case 2:
                context.result = access.BorrowReferencedPedPosition(context.token, context.position);
                break;
            case 3:
                context.result = access.ReadReferencedPedVelocity(context.token, context.kind, context.velocity);
                break;
            default:
                std::abort();
        }
        if (context.alsoRaw)
            context.rawResult = access.ReadPedVelocity(1, context.kind, context.raw);
    }
    struct DefaultPedObservations final : NativePedObservationAccess
    {
    };
    void NativePedObservationOwnedSdk(ClockImageFixture& fixture)
    {
        namespace P = NativePedReferenceProfile;
        namespace Ped = NativePedPoolProfile;
        namespace T = NativePedTransformProfile;
        namespace V = NativePedVelocityProfile;
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
                  "save only bounded pointer-cell proof spans in the existing owned callback image");
            saved.push_back(std::move(prior));
            fixture.Patch(rva, bytes, size);
            DWORD previous = 0;
            Check(::VirtualProtect(reinterpret_cast<void*>(fixture.image + rva), size, protection, &previous) != FALSE,
                  "native reference owner preserves real code, relocated-slot and mutable-bank page policies");
        };
        for (const auto& pin : Ped::Code)
            admit(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : P::Code)
            admit(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : T::Code)
            admit(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : V::Code)
            admit(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : NativeGameStateProfile::pins)
            admit(pin.rva, pin.bytes, pin.size, pin.code ? PAGE_EXECUTE_READ : PAGE_READONLY);
        for (const auto& pin : NativeBorrowedGcProfile::pins)
            admit(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : NativeStartGameProfile::Code)
            if (pin.rva == 0xab879c || pin.rva == 0xab92f0 || pin.rva == 0x1af4ef0)
                admit(pin.rva, pin.bytes.data(), pin.size, PAGE_EXECUTE_READ);
        for (const auto& slot : P::Slots)
        {
            const auto target = fixture.image + slot.targetRva;
            admit(slot.rva, &target, sizeof(target), PAGE_READONLY);
        }
        const auto teleport = fixture.image + T::NativeTeleportRva;
        for (const auto table : {T::PedVtableRva, T::PlayerVtableRva})
            admit(table + T::TeleportSlotOffset, &teleport, sizeof(teleport), PAGE_READONLY);
        admit(T::ScriptVelocityInScaleRva, &T::ScriptVelocityInScaleBits, 4, PAGE_READONLY);
        admit(T::ScriptVelocityOutScaleRva, &T::ScriptVelocityOutScaleBits, 4, PAGE_READONLY);
        const std::uintptr_t zero = 0;
        const std::uint32_t  state = 9, marker = 0;
        const auto           thread = GetCurrentThreadId();
        const unsigned char  initialized = 1;
        admit(Ped::RootRva, &zero, sizeof(zero), PAGE_READWRITE);
        admit(NativeGameStateProfile::stateRva, &state, sizeof(state), PAGE_READWRITE);
        admit(NativeStartGameProfile::ThreadInitializedRva, &initialized, 1, PAGE_READWRITE);
        admit(NativeStartGameProfile::ThreadIdRva, &thread, sizeof(thread), PAGE_READWRITE);
        admit(NativeBorrowedGcProfile::markerRva, &marker, sizeof(marker), PAGE_READWRITE);
        std::vector<unsigned char> nodes(P::ReferenceCount * P::ReferenceBytes);
        for (std::size_t i = 0; i + 1 < P::ReferenceCount; ++i)
        {
            const auto next = fixture.image + P::ReferencesRva + (i + 1) * P::ReferenceBytes;
            std::memcpy(nodes.data() + i * P::ReferenceBytes, &next, sizeof(next));
        }
        admit(P::ReferencesRva, nodes.data(), nodes.size(), PAGE_READWRITE);
        const auto first = fixture.image + P::ReferencesRva;
        admit(P::FreeHeadRva, &first, sizeof(first), PAGE_READWRITE);
        // Reuse the actual handle-bank fixture. This service needs no SDK ped,
        // ambient player, matrix or actor graph to register one qualified row.
        ClockTeleportPool pool(fixture, Ped::RootRva, Ped::NativeCapacity, Ped::ObjectStride);
        pool.Publish();
        pool.flags[0] = 1;
        const auto vtable = fixture.image + P::PlayerVtableRva;
        std::memcpy(pool.objects, &vtable, sizeof(vtable));
        pool.objects[P::TypeOffset] = 3;
        ClockReaderTestHost::image = ClockPedReferenceOwnerTest::image = fixture.image;
        ClockPedReferenceOwnerTest::registers = ClockPedReferenceOwnerTest::unregisters = 0;
        ClockPedReferenceOwnerTest::partialUnlink = false;
        ClockPedReferenceOwnerTest::unregisterResult = ClockResult::Ok;
        PedObservationOwnerContext context;
        context.transform.positionAddress = 123;
        DefaultPedObservations absent;
        Check(absent.ReadReferencedPedTransform(context.token, context.transform) == ClockResult::NotInitialized && context.transform.positionAddress == 123 &&
                  absent.BorrowReferencedPedPosition(context.token, context.position) == ClockResult::NotInitialized &&
                  context.position == reinterpret_cast<CVector*>(std::uintptr_t{1}) &&
                  absent.ReadReferencedPedVelocity(context.token, context.kind, context.velocity) == ClockResult::NotInitialized && context.velocity.fX == 101,
              "default optional observations preserve every caller output and publish no native view");
        Check(runtime.access.ReadReferencedPedTransform(context.token, context.transform) == ClockResult::NotInitialized &&
                  context.transform.positionAddress == 123 &&
                  runtime.access.BorrowReferencedPedPosition(context.token, context.position) == ClockResult::NotInitialized &&
                  context.position == reinterpret_cast<CVector*>(std::uintptr_t{1}) &&
                  runtime.access.ReadReferencedPedVelocity(context.token, context.kind, context.velocity) == ClockResult::NotInitialized &&
                  context.velocity.fX == 101,
              "unbound Runtime observations provide no token, physical snapshot or position borrow");
        const std::array<std::uint32_t, 3> positionBits{0x7fc01234, 0x80000000, 0x3f800001};
        const std::array<std::uint32_t, 3> linearBits{0x7f800000, 0x80000000, 0xffc07654};
        const std::array<std::uint32_t, 3> angularBits{0x3f800001, 0xc0200000, 0x7fc05678};
        std::memcpy(pool.objects + T::InlinePositionOffset, positionBits.data(), sizeof(CVector));
        std::memcpy(pool.objects + V::LinearOffset, linearBits.data(), sizeof(CVector));
        std::memcpy(pool.objects + V::AngularOffset, angularBits.data(), sizeof(CVector));
        const auto rawBits = [](const CVector& value)
        {
            std::array<std::uint32_t, 3> result{};
            std::memcpy(result.data(), &value, sizeof(value));
            return result;
        };
        NativeClockService service;
        BeginOwned(service, fixture, false, true, true, &PedObservationOwnerHandler, &context);
        fixture.invokeTimer();
        Check(context.result == ClockResult::Ok && runtime.pedReferences.IsRetained(),
              "physical observations reuse a token verified by real native reference registration");
        Check(runtime.access.ReadReferencedPedTransform(context.token, context.transform) != ClockResult::Ok && context.transform.positionAddress == 123 &&
                  runtime.access.BorrowReferencedPedPosition(context.token, context.position) != ClockResult::Ok &&
                  context.position == reinterpret_cast<CVector*>(std::uintptr_t{1}) &&
                  runtime.access.ReadReferencedPedVelocity(context.token, context.kind, context.velocity) != ClockResult::Ok && context.velocity.fX == 101,
              "a producer holding the current token still cannot borrow physical state between callbacks");
        const auto invoke = [&]
        {
            Check(service.Renew(runtime.generation.load()), "renew actual callback ownership between independent physical observations");
            fixture.invokeTimer();
        };
        context.operation = 1;
        invoke();
        Check(context.result == ClockResult::Ok && context.transform.ped.handle == 1 &&
                  context.transform.ped.nativePed == reinterpret_cast<std::uintptr_t>(pool.objects) && context.transform.matrix == 0 &&
                  rawBits(context.transform.position) == positionBits && rawBits(context.transform.velocity) == linearBits,
              "actual token-backed transform observation preserves raw position and velocity without numerical repair");
        context.operation = 2;
        invoke();
        Check(context.result == ClockResult::Ok && context.position == reinterpret_cast<CVector*>(pool.objects + T::InlinePositionOffset),
              "the actual inline position pointer is borrowed only after both registered-token bookends");
        context.operation = 3;
        invoke();
        Check(context.result == ClockResult::Ok && rawBits(context.velocity) == linearBits,
              "actual token-backed linear velocity retains exact native float bits");
        context.kind = NativePedVelocityKind::Angular;
        invoke();
        Check(context.result == ClockResult::Ok && rawBits(context.velocity) == angularBits,
              "actual token-backed angular velocity selects the separate raw native vector");
        auto* matrix = static_cast<unsigned char*>(::VirtualAlloc(nullptr, 0x60, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
        Check(matrix != nullptr, "own one native-style matrix for immediate token-backed position identity");
        const auto    ped = reinterpret_cast<std::uintptr_t>(pool.objects), matrixAddress = reinterpret_cast<std::uintptr_t>(matrix);
        const CVector matrixPosition(44, 55, 66);
        std::memcpy(matrix + T::MatrixOwnerOffset, &ped, sizeof(ped));
        std::memcpy(matrix + T::MatrixPositionOffset, &matrixPosition, sizeof(matrixPosition));
        std::memcpy(pool.objects + T::MatrixPointerOffset, &matrixAddress, sizeof(matrixAddress));
        context.operation = 2;
        invoke();
        Check(context.result == ClockResult::Ok && context.position == reinterpret_cast<CVector*>(matrix + T::MatrixPositionOffset),
              "a current token preserves the genuine owned matrix vector address without a cached replacement");
        const auto token = context.token;
        ++context.token.version;
        for (unsigned operation = 1; operation <= 3; ++operation)
        {
            context.operation = operation;
            context.transform.positionAddress = 123;
            context.position = reinterpret_cast<CVector*>(std::uintptr_t{1});
            context.velocity = CVector(101, 102, 103);
            invoke();
            Check(context.result != ClockResult::Ok && context.transform.positionAddress == 123 &&
                      context.position == reinterpret_cast<CVector*>(std::uintptr_t{1}) && context.velocity.fX == 101 && !runtime.stopping.load(),
                  "stale CPU tokens preserve all outputs through each genuine observation owner method");
        }
        context.token = token;
        std::memcpy(pool.objects + T::MatrixPointerOffset, &zero, sizeof(zero));
        Check(::VirtualFree(matrix, 0, MEM_RELEASE) != FALSE, "release only owned matrix storage after clearing the exact selector");
        context.operation = 3;
        context.kind = NativePedVelocityKind::Linear;
        context.alsoRaw = true;
        ClockPedObservationOwnerTest::address = ped + V::LinearOffset;
        ClockPedObservationOwnerTest::cell = ClockPedReferenceOwnerTest::cell;
        ClockPedObservationOwnerTest::clearOnRead = true;
        const auto invalidations = ClockPedObservationOwnerTest::invalidations;
        invoke();
        Check(context.result != ClockResult::Ok && context.velocity.fX == 101 && context.rawResult == ClockResult::Ok && rawBits(context.raw) == linearBits &&
                  ClockPedObservationOwnerTest::invalidations == invalidations + 1 && !runtime.stopping.load(),
              "real raw read remains valid while post-read registered-cell invalidation prevents publishing its unacknowledged output");
        // Only the fixture restores its owned word for native unlink. The lost
        // CPU token is not republished and no raw observation is replayed.
        ClockPedReferenceOwnerTest::Put(ClockPedReferenceOwnerTest::cell, ped);
        const auto unlinks = ClockPedReferenceOwnerTest::unregisters;
        Check(!service.End() && runtime.pedReferences.IsRetained(), "producer End retains observation reference storage until natural callback cleanup");
        fixture.invokeTimer();
        Check(service.End() && !service.IsRetained() && !runtime.pedReferences.IsRetained() && !runtime.timingHandler &&
                  ClockPedReferenceOwnerTest::unregisters == unlinks + 1 && !NativeServiceAdmission::IsClaimed(NativeServiceAdmission::Category::Clock),
              "all observation paths share the real registered-cell and module retirement gate");
        ClockPedObservationOwnerTest::clearOnRead = false;
        ClockPedObservationOwnerTest::address = ClockPedObservationOwnerTest::cell = 0;
        ClockReaderTestHost::image = ClockPedReferenceOwnerTest::image = 0;
        for (auto item = saved.rbegin(); item != saved.rend(); ++item)
            fixture.Patch(item->rva, item->bytes.data(), item->bytes.size());
        std::puts("Production native ped observation callback owner fixture completed");
    }
}
