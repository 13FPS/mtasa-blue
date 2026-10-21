/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 *
 * PURPOSE: Qualified native dynamics through actual registered-token ownership
 ****************************************************************************/
namespace
{
    struct PedPhysicalDynamicsOwnerContext
    {
        NativePedReferenceToken      token;
        NativePedPhysicalScalarField field = NativePedPhysicalScalarField::Mass;
        ClockResult                  result = ClockResult::NotInitialized;
        unsigned                     operation = 0;
        float                        target = 0, output = 123;
        CVector                      centerTarget, centerOutput{101, 102, 103};
    };
    void PedPhysicalDynamicsOwnerHandler(NativeGameTimingAccess& access, void* opaque)
    {
        auto& context = *static_cast<PedPhysicalDynamicsOwnerContext*>(opaque);
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
                context.result = access.ReadPedCenterOfMass(context.token, context.centerOutput);
                break;
            case 4:
                context.result = access.SetPedCenterOfMass(context.token, context.centerTarget);
                break;
            default:
                std::abort();
        }
    }
    void NativePedPhysicalDynamicsOwnedSdk(ClockImageFixture& fixture)
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
        constexpr std::size_t airOffset = S::AirResistanceOffset, elasticityOffset = S::ElasticityOffset, unqualifiedAdjacentOffset = 0xd8,
                              centerOffset = S::CenterOfMassOffset;
        using Bits = std::array<std::uint32_t, 3>;
        const auto vector = [](const Bits& bits)
        {
            CVector value;
            std::memcpy(&value, bits.data(), sizeof(value));
            return value;
        };
        const auto center = [&]
        {
            Bits bits{};
            std::memcpy(bits.data(), pool.objects + centerOffset, sizeof(CVector));
            return bits;
        };
        const auto centerBits = [](const CVector& value)
        {
            Bits bits{};
            std::memcpy(bits.data(), &value, sizeof(value));
            return bits;
        };
        const Bits initialCenter{0x3f800000, 0x40000000, 0x40400000};
        const Bits acceptedCenter{0x7fc01234, 0x80000000, 0x3f800001};
        std::memcpy(pool.objects + centerOffset, initialCenter.data(), sizeof(CVector));
        store(S::MassOffset, 0x3f800001);
        store(S::TurnMassOffset, 0x40000001);
        store(airOffset, 0x3dcccccd);
        store(elasticityOffset, 0x3e800000);
        store(unqualifiedAdjacentOffset, 0x42a50000);
        ClockReaderTestHost::image = ClockPedReferenceOwnerTest::image = fixture.image;
        ClockPedReferenceOwnerTest::partialUnlink = false;
        ClockPedReferenceOwnerTest::unregisterResult = ClockResult::Ok;
        ClockPedPhysicalScalarsOwnerTest::stores = ClockPedPhysicalScalarsOwnerTest::failAt = ClockPedPhysicalScalarsOwnerTest::loseAt = 0;
        ClockPedPhysicalScalarsOwnerTest::cell = 0;
        ClockPedPhysicalScalarsOwnerTest::loseToken = false;
        PedPhysicalDynamicsOwnerContext context;
        context.field = NativePedPhysicalScalarField::AirResistance;
        context.centerTarget = vector(acceptedCenter);
        const auto sentinel = centerBits(context.centerOutput);
        Check(runtime.access.ReadPedPhysicalScalar(context.token, context.field, context.output) == ClockResult::NotInitialized && context.output == 123 &&
                  runtime.access.ReadPedCenterOfMass(context.token, context.centerOutput) == ClockResult::NotInitialized &&
                  centerBits(context.centerOutput) == sentinel &&
                  runtime.access.SetPedCenterOfMass(context.token, context.centerTarget) == ClockResult::NotInitialized &&
                  !ClockPedPhysicalScalarsOwnerTest::stores,
              "unbound dynamic properties preserve outputs and cannot manufacture scalar or local-space vector mutation authority");
        const auto retire = [&](NativeClockService& service)
        {
            if (!service.End())
            {
                fixture.invokeTimer();
                Check(service.End(), "same admitted callback retires the dynamic-property registered cell");
            }
            Check(!service.IsRetained() && !runtime.pedReferences.IsRetained() && !runtime.timingHandler &&
                      !NativeServiceAdmission::IsClaimed(NativeServiceAdmission::Category::Clock),
                  "dynamic property End acknowledges native cell storage, hooks and callback admission together");
        };
        {
            NativeClockService service;
            BeginOwned(service, fixture, false, true, true, &PedPhysicalDynamicsOwnerHandler, &context);
            const auto invoke = [&]
            {
                Check(service.Renew(runtime.generation.load()), "renew actual callback ownership between independent dynamics requests");
                fixture.invokeTimer();
            };
            invoke();
            Check(context.result == ClockResult::Ok && context.token.version && runtime.pedReferences.IsRetained(),
                  "dynamic properties use an actual native registration from this same owner generation");
            Check(runtime.access.ReadPedCenterOfMass(context.token, context.centerOutput) != ClockResult::Ok && centerBits(context.centerOutput) == sentinel &&
                      runtime.access.SetPedCenterOfMass(context.token, context.centerTarget) != ClockResult::Ok && !ClockPedPhysicalScalarsOwnerTest::stores,
                  "registered lifetime metadata does not lend COM authority to a producer between callbacks");
            for (const auto field : {NativePedPhysicalScalarField::AirResistance, NativePedPhysicalScalarField::Elasticity})
            {
                context.field = field;
                context.operation = 1;
                invoke();
                const auto offset = field == NativePedPhysicalScalarField::AirResistance ? airOffset : elasticityOffset;
                Check(context.result == ClockResult::Ok && floatBits(context.output) == raw(offset) && !runtime.playerInfo.IsBound(),
                      "actual callback reads each qualified generic-ped dynamic scalar without a local-player substitution");
                context.target = fromBits(field == NativePedPhysicalScalarField::AirResistance ? 0x7fc12345 : 0x80000000);
                context.operation = 2;
                invoke();
                Check(context.result == ClockResult::Ok && raw(offset) == floatBits(context.target),
                      "same scalar owner forwards each newly qualified field and preserves accepted raw representation");
            }
            context.operation = 3;
            invoke();
            Check(context.result == ClockResult::Ok && centerBits(context.centerOutput) == initialCenter,
                  "COM owner returns raw native local-space coordinates without a matrix transform or borrowed pointer");
            context.operation = 4;
            invoke();
            Check(context.result == ClockResult::Ok && center() == acceptedCenter && ClockPedPhysicalScalarsOwnerTest::stores == 5,
                  "actual owner accepts all three changed COM components through the existing typed scalar-CAS seam");
            context.operation = 3;
            invoke();
            Check(context.result == ClockResult::Ok && centerBits(context.centerOutput) == acceptedCenter,
                  "COM output is committed after primitive and registered-token observation bookends");
            const auto token = context.token;
            ++context.token.version;
            context.output = 123;
            context.centerOutput = CVector(101, 102, 103);
            for (unsigned operation : {1u, 2u, 3u, 4u})
            {
                context.operation = operation;
                invoke();
                Check(context.result != ClockResult::Ok && context.output == 123 && centerBits(context.centerOutput) == sentinel &&
                          ClockPedPhysicalScalarsOwnerTest::stores == 5 && !runtime.stopping.load(),
                      "stale dynamics token refuses every route before effect or output publication without stopping the valid owner");
            }
            context.token = token;
            retire(service);
            Check(center() == acceptedCenter && raw(airOffset) == 0x7fc12345 && raw(elasticityOffset) == 0x80000000 &&
                      raw(unqualifiedAdjacentOffset) == 0x42a50000 && raw(S::MassOffset) == 0x3f800001 && raw(S::TurnMassOffset) == 0x40000001,
                  "End preserves accepted dynamics while leaving mass, turn mass and the unqualified adjacent field untouched");
        }
        {
            context.operation = 0;
            context.centerOutput = CVector(101, 102, 103);
            NativeClockService service;
            BeginOwned(service, fixture, false, true, true, &PedPhysicalDynamicsOwnerHandler, &context);
            fixture.invokeTimer();
            Check(context.result == ClockResult::Ok, "COM read postcondition uses a fresh verified token in a later generation");
            context.operation = 3;
            ClockPedObservationOwnerTest::address = reinterpret_cast<std::uintptr_t>(pool.objects) + centerOffset;
            ClockPedObservationOwnerTest::cell = ClockPedReferenceOwnerTest::cell;
            ClockPedObservationOwnerTest::clearOnRead = true;
            const auto invalidations = ClockPedObservationOwnerTest::invalidations;
            Check(service.Renew(runtime.generation.load()), "renew before exact post-read COM lifetime invalidation");
            fixture.invokeTimer();
            Check(context.result != ClockResult::Ok && centerBits(context.centerOutput) == sentinel &&
                      ClockPedObservationOwnerTest::invalidations == invalidations + 1 && !runtime.stopping.load(),
                  "native COM read cannot publish its local vector after registered-cell loss and does not pretend a failed read was an entered mutation");
            // Only this owned host fixture restores its word for legitimate
            // native unlink; the revoked caller token is not republished.
            ClockPedReferenceOwnerTest::Put(ClockPedReferenceOwnerTest::cell, reinterpret_cast<std::uintptr_t>(pool.objects));
            retire(service);
            Check(center() == acceptedCenter, "read-side token retirement never writes or repairs the COM vector");
            ClockPedObservationOwnerTest::clearOnRead = false;
            ClockPedObservationOwnerTest::address = ClockPedObservationOwnerTest::cell = 0;
        }
        const Bits partialTarget{0x41200001, 0x41a00001, 0x41f00001};
        const Bits afterPartial{partialTarget[0], acceptedCenter[1], acceptedCenter[2]};
        {
            context.operation = 0;
            context.centerTarget = vector(partialTarget);
            NativeClockService service;
            BeginOwned(service, fixture, false, true, true, &PedPhysicalDynamicsOwnerHandler, &context);
            fixture.invokeTimer();
            Check(context.result == ClockResult::Ok, "partial COM scenario owns a distinct verified reference generation");
            const auto stores = ClockPedPhysicalScalarsOwnerTest::stores;
            ClockPedPhysicalScalarsOwnerTest::failAt = stores + 2;
            context.operation = 4;
            Check(service.Renew(runtime.generation.load()), "renew before a scheduled second-component COM fault");
            fixture.invokeTimer();
            Check(context.result != ClockResult::Ok && runtime.stopping.load() && ClockPedPhysicalScalarsOwnerTest::stores == stores + 2 &&
                      center() == afterPartial,
                  "real owner stops on entered partial COM failure while preserving the accepted first component and untouched remaining components");
            fixture.invokeTimer();
            Check(ClockPedPhysicalScalarsOwnerTest::stores == stores + 2 && center() == afterPartial,
                  "stopping callback never retries, completes or rolls back a partially accepted physical vector");
            ClockPedPhysicalScalarsOwnerTest::failAt = 0;
            retire(service);
            Check(center() == afterPartial, "normal reference retirement preserves partial persistent COM intent");
        }
        {
            const Bits finalTarget{0x42200001, 0xffc07654, 0x80000001};
            context.operation = 0;
            context.centerTarget = vector(finalTarget);
            NativeClockService service;
            BeginOwned(service, fixture, false, true, true, &PedPhysicalDynamicsOwnerHandler, &context);
            fixture.invokeTimer();
            Check(context.result == ClockResult::Ok, "final COM mutation owns a fresh native reference before entry");
            const auto stores = ClockPedPhysicalScalarsOwnerTest::stores;
            const auto unlinks = ClockPedReferenceOwnerTest::unregisters;
            const auto node = ClockPedReferenceOwnerTest::node;
            ClockPedPhysicalScalarsOwnerTest::cell = ClockPedReferenceOwnerTest::cell;
            ClockPedPhysicalScalarsOwnerTest::loseAt = stores + 3;
            ClockPedPhysicalScalarsOwnerTest::loseToken = true;
            context.operation = 4;
            Check(service.Renew(runtime.generation.load()), "renew before accepted COM loses only final token acknowledgement");
            fixture.invokeTimer();
            Check(context.result != ClockResult::Ok && runtime.stopping.load() && ClockPedPhysicalScalarsOwnerTest::stores == stores + 3 &&
                      center() == finalTarget && !ClockPedPhysicalScalarsOwnerTest::loseToken,
                  "all accepted COM components persist when the exact post-token check refuses acknowledgement");
            Check(runtime.pedReferences.IsRetained() && service.IsRetained() &&
                      ClockPedReferenceOwnerTest::Get<std::uintptr_t>(node + 8) == ClockPedPhysicalScalarsOwnerTest::cell && !service.End(),
                  "zero native reference word cannot discharge retained COM owner storage while its node still names the cell address");
            fixture.invokeTimer();
            Check(ClockPedPhysicalScalarsOwnerTest::stores == stores + 3 && ClockPedReferenceOwnerTest::unregisters == unlinks && !service.End(),
                  "retained lifetime debt blocks completion without replaying vector intent or native unlink");
            ClockPedReferenceOwnerTest::Put(node + 8, std::uintptr_t{0});
            const auto head = ClockPedReferenceOwnerTest::Get<std::uintptr_t>(fixture.image + R::FreeHeadRva);
            ClockPedReferenceOwnerTest::Put(node, head);
            ClockPedReferenceOwnerTest::Put(fixture.image + R::FreeHeadRva, node);
            ClockPedReferenceOwnerTest::Put(reinterpret_cast<std::uintptr_t>(pool.objects) + R::EntityHeadOffset, std::uintptr_t{0});
            fixture.invokeTimer();
            retire(service);
            Check(center() == finalTarget && ClockPedPhysicalScalarsOwnerTest::stores == stores + 3 && ClockPedReferenceOwnerTest::unregisters == unlinks,
                  "whole-bank cell-address retirement finishes End without vector rollback or action replay");
        }
        ClockPedPhysicalScalarsOwnerTest::failAt = ClockPedPhysicalScalarsOwnerTest::loseAt = 0;
        ClockPedObservationOwnerTest::clearOnRead = false;
        ClockPedObservationOwnerTest::address = ClockPedObservationOwnerTest::cell = 0;
        ClockPedPhysicalScalarsOwnerTest::cell = 0;
        ClockPedPhysicalScalarsOwnerTest::loseToken = false;
        ClockReaderTestHost::image = ClockPedReferenceOwnerTest::image = 0;
        for (auto item = saved.rbegin(); item != saved.rend(); ++item)
            fixture.Patch(item->rva, item->bytes.data(), item->bytes.size());
        std::puts("Production native ped physical dynamics callback owner fixture completed");
    }
}
