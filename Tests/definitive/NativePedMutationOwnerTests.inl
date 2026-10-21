/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Persistent native vector intent through a genuine registered token
 ****************************************************************************/
namespace
{
    struct PedMutationOwnerContext
    {
        NativePedReferenceToken token;
        NativePedVelocityKind   kind = NativePedVelocityKind::Linear;
        CVector                 target;
        ClockResult             result = ClockResult::NotInitialized;
        bool                    mutate = false;
    };
    void PedMutationOwnerHandler(NativeGameTimingAccess& access, void* opaque)
    {
        auto& context = *static_cast<PedMutationOwnerContext*>(opaque);
        context.result =
            context.mutate ? access.SetReferencedPedVelocity(context.token, context.kind, context.target) : access.AcquirePedReference(1, context.token);
    }
    std::uintptr_t mutationReferenceCell = 0;
    void           LoseMutationReferenceWord()
    {
        // A native weak-reference clear is separate from node retirement.
        // The raw vector operation can finish; only its token postcheck sees
        // this loss, and storage must remain until address absence is proved.
        ClockPedReferenceOwnerTest::Put(mutationReferenceCell, std::uintptr_t{0});
    }
    void NativePedMutationOwnedSdk(ClockImageFixture& fixture)
    {
        namespace V = NativePedVelocityProfile;
        namespace T = NativePedTransformProfile;
        namespace R = NativePedReferenceProfile;
        namespace Ped = NativePedPoolProfile;
        using Bits = std::array<std::uint32_t, 3>;
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
                  "save only bounded proof bytes for combined native reference and velocity ownership");
            saved.push_back(std::move(prior));
            fixture.Patch(rva, bytes, size);
            DWORD previous = 0;
            Check(::VirtualProtect(reinterpret_cast<void*>(fixture.image + rva), size, protection, &previous) != FALSE,
                  "referenced mutation keeps actual code, vtable and native data page policies");
        };
        for (const auto& pin : Ped::Code)
            admit(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : T::Code)
            admit(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : V::Code)
            admit(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : R::Code)
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
        std::vector<unsigned char> nodes(R::ReferenceCount * R::ReferenceBytes);
        for (std::size_t i = 0; i + 1 < R::ReferenceCount; ++i)
        {
            const auto next = fixture.image + R::ReferencesRva + (i + 1) * R::ReferenceBytes;
            std::memcpy(nodes.data() + i * R::ReferenceBytes, &next, sizeof(next));
        }
        admit(R::ReferencesRva, nodes.data(), nodes.size(), PAGE_READWRITE);
        const auto first = fixture.image + R::ReferencesRva;
        admit(R::FreeHeadRva, &first, sizeof(first), PAGE_READWRITE);
        // Reuse the existing occupied-ped utility. This raw vector operation
        // needs neither an actor graph nor a synthetic CEntity/CPed wrapper.
        ClockTeleportPool pool(fixture, Ped::RootRva, Ped::NativeCapacity, Ped::ObjectStride);
        pool.Publish();
        pool.flags[0] = 1;
        const auto vtable = fixture.image + R::PedVtableRva;
        std::memcpy(pool.objects, &vtable, sizeof(vtable));
        pool.objects[V::TypeOffset] = 3;
        const CVector linear(1, 2, 3), angular(4, 5, 6);
        std::memcpy(pool.objects + V::LinearOffset, &linear, sizeof(linear));
        std::memcpy(pool.objects + V::AngularOffset, &angular, sizeof(angular));
        const auto vector = [](const Bits& bits)
        {
            CVector value;
            std::memcpy(&value, bits.data(), sizeof(value));
            return value;
        };
        const auto raw = [&](std::size_t offset)
        {
            Bits value;
            std::memcpy(value.data(), pool.objects + offset, sizeof(value));
            return value;
        };
        ClockReaderTestHost::image = ClockPedReferenceOwnerTest::image = fixture.image;
        ClockPedReferenceOwnerTest::partialUnlink = false;
        ClockPedReferenceOwnerTest::unregisterResult = ClockResult::Ok;
        ClockPedVelocityOwnerTest::stores = ClockPedVelocityOwnerTest::failAt = ClockPedVelocityOwnerTest::afterAt = 0;
        ClockPedVelocityOwnerTest::after = nullptr;
        PedMutationOwnerContext context;
        const Bits              accepted{0x3f800001, 0x80000000, 0x7fc01234};
        context.target = vector(accepted);
        Check(runtime.access.SetReferencedPedVelocity(context.token, context.kind, context.target) == ClockResult::NotInitialized &&
                  !ClockPedVelocityOwnerTest::stores,
              "unbound registered-token velocity supplies no raw mutation authority");
        const auto retire = [&](NativeClockService& service)
        {
            if (!service.End())
            {
                fixture.invokeTimer();
                Check(service.End(), "the real callback retires the registered cell after native velocity mutation ends");
            }
            Check(!service.IsRetained() && !runtime.pedReferences.IsRetained() && !runtime.timingHandler &&
                      !NativeServiceAdmission::IsClaimed(NativeServiceAdmission::Category::Clock),
                  "registered-token mutation retires native cell storage and callback admission together");
        };
        {
            NativeClockService service;
            BeginOwned(service, fixture, false, true, true, &PedMutationOwnerHandler, &context);
            const auto invoke = [&]()
            {
                Check(service.Renew(runtime.generation.load()), "renew one actual callback lease between independent referenced mutation requests");
                fixture.invokeTimer();
            };
            invoke();
            Check(context.result == ClockResult::Ok && context.token.version && runtime.pedReferences.IsRetained(),
                  "referenced velocity borrows a token from actual verified native registration");
            Check(runtime.access.SetReferencedPedVelocity(context.token, context.kind, context.target) != ClockResult::Ok && !ClockPedVelocityOwnerTest::stores,
                  "a producer between callbacks cannot turn a valid registered token into write authority");
            context.mutate = true;
            const auto valid = context.token;
            ++context.token.version;
            invoke();
            Check(context.result != ClockResult::Ok && !ClockPedVelocityOwnerTest::stores && !runtime.stopping.load(),
                  "a stale CPU token refuses before entry without stopping an unrelated valid native owner");
            context.token = valid;
            invoke();
            Check(context.result == ClockResult::Ok && ClockPedVelocityOwnerTest::stores == 3 && raw(V::LinearOffset) == accepted && !runtime.stopping.load(),
                  "the real callback resolves the token and commits all three exact native velocity scalar bits");
            retire(service);
            Check(raw(V::LinearOffset) == accepted,
                  "End retires registered lifetime while preserving accepted raw velocity including signed zero and NaN payload");
        }
        {
            context.mutate = false;
            context.kind = NativePedVelocityKind::Angular;
            const Bits acceptedAfterLoss{0x42200001, 0x80000000, 0x7fc07654};
            context.target = vector(acceptedAfterLoss);
            NativeClockService service;
            BeginOwned(service, fixture, false, true, true, &PedMutationOwnerHandler, &context);
            fixture.invokeTimer();
            Check(context.result == ClockResult::Ok, "the later generation independently acquires its own registered mutation token");
            const auto stores = ClockPedVelocityOwnerTest::stores;
            const auto unlinks = ClockPedReferenceOwnerTest::unregisters;
            const auto node = ClockPedReferenceOwnerTest::node;
            mutationReferenceCell = ClockPedReferenceOwnerTest::cell;
            ClockPedVelocityOwnerTest::afterAt = stores + 3;
            ClockPedVelocityOwnerTest::after = &LoseMutationReferenceWord;
            context.mutate = true;
            Check(service.Renew(runtime.generation.load()), "renew before one accepted native vector loses only its reference-token acknowledgement");
            fixture.invokeTimer();
            Check(context.result != ClockResult::Ok && runtime.stopping.load() && ClockPedVelocityOwnerTest::stores == stores + 3 &&
                      raw(V::AngularOffset) == acceptedAfterLoss && !ClockPedVelocityOwnerTest::after,
                  "post-token loss stops the owner after the raw vector succeeded, preserving all accepted scalar effects");
            Check(runtime.pedReferences.IsRetained() && service.IsRetained() &&
                      ClockPedReferenceOwnerTest::Get<std::uintptr_t>(node + 8) == mutationReferenceCell && !service.End(),
                  "zero reference word cannot release native-visible storage while its registered node still names the cell address");
            fixture.invokeTimer();
            Check(ClockPedVelocityOwnerTest::stores == stores + 3 && ClockPedReferenceOwnerTest::unregisters == unlinks && runtime.pedReferences.IsRetained() &&
                      !service.End(),
                  "stopping callbacks retain zero-word debt without replaying accepted mutation or inventing native unlink authority");
            // Model the separately proved ResolveReferences postcondition:
            // retire the exact address before recycling its node. The native
            // vector remains untouched throughout this lifetime acknowledgement.
            ClockPedReferenceOwnerTest::Put(node + 8, std::uintptr_t{0});
            const auto head = ClockPedReferenceOwnerTest::Get<std::uintptr_t>(fixture.image + R::FreeHeadRva);
            ClockPedReferenceOwnerTest::Put(node, head);
            ClockPedReferenceOwnerTest::Put(fixture.image + R::FreeHeadRva, node);
            ClockPedReferenceOwnerTest::Put(reinterpret_cast<std::uintptr_t>(pool.objects) + R::EntityHeadOffset, std::uintptr_t{0});
            fixture.invokeTimer();
            retire(service);
            Check(ClockPedReferenceOwnerTest::unregisters == unlinks && ClockPedVelocityOwnerTest::stores == stores + 3 &&
                      raw(V::AngularOffset) == acceptedAfterLoss && raw(V::LinearOffset) == accepted,
                  "full-bank address absence acknowledges cell retirement without unlink replay or velocity rollback");
        }
        mutationReferenceCell = 0;
        ClockPedVelocityOwnerTest::afterAt = 0;
        ClockPedVelocityOwnerTest::after = nullptr;
        ClockReaderTestHost::image = ClockPedReferenceOwnerTest::image = 0;
        for (auto item = saved.rbegin(); item != saved.rend(); ++item)
            fixture.Patch(item->rva, item->bytes.data(), item->bytes.size());
        std::puts("Production native referenced ped velocity mutation callback owner fixture completed");
    }
}
