/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Same retained owner for native vector copies and partial stores
 ****************************************************************************/
namespace
{
    struct PedVelocityOwnerContext
    {
        NativePedVelocityKind kind = NativePedVelocityKind::Linear;
        CVector               target{10, 20, 30}, output{101, 102, 103};
        ClockResult           result = ClockResult::NotInitialized;
        bool                  write = false;
    };
    void PedVelocityOwnerHandler(NativeGameTimingAccess& access, void* opaque)
    {
        auto& context = *static_cast<PedVelocityOwnerContext*>(opaque);
        context.result = context.write ? access.SetPedVelocity(1, context.kind, context.target) : access.ReadPedVelocity(1, context.kind, context.output);
    }
    void NativePedVelocityOwnedSdk(ClockImageFixture& fixture)
    {
        namespace P = NativePedVelocityProfile;
        namespace T = NativePedTransformProfile;
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
                  "save bounded native velocity proof spans in the existing owned callback image");
            saved.push_back(std::move(prior));
            fixture.Patch(rva, bytes, size);
            DWORD previous = 0;
            Check(::VirtualProtect(reinterpret_cast<void*>(fixture.image + rva), size, protection, &previous) != FALSE,
                  "actual velocity owner keeps independent native code and data page policies");
        };
        for (const auto& pin : Ped::Code)
            admit(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : T::Code)
            admit(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : P::Code)
            admit(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : NativeGameStateProfile::pins)
            admit(pin.rva, pin.bytes, pin.size, pin.code ? PAGE_EXECUTE_READ : PAGE_READONLY);
        for (const auto& pin : NativeBorrowedGcProfile::pins)
            admit(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : NativeStartGameProfile::Code)
            if (pin.rva == 0xab879c || pin.rva == 0xab92f0 || pin.rva == 0x1af4ef0)
                admit(pin.rva, pin.bytes.data(), pin.size, PAGE_EXECUTE_READ);
        const auto method = fixture.image + Ped::Code[1].rva;
        const auto teleport = fixture.image + T::NativeTeleportRva;
        for (const auto table : {T::PedVtableRva, T::PlayerVtableRva})
        {
            admit(table, &method, sizeof(method), PAGE_READONLY);
            admit(table + T::TeleportSlotOffset, &teleport, sizeof(teleport), PAGE_READONLY);
        }
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
        // Reuse the previous cohort's generic owned pool helper. Raw physical
        // vectors need no actor graph, matrix allocation or synthetic SDK ped.
        ClockTeleportPool pool(fixture, Ped::RootRva, Ped::NativeCapacity, Ped::ObjectStride);
        pool.Publish();
        pool.flags[0] = 1;
        const auto vtable = fixture.image + T::PlayerVtableRva;
        std::memcpy(pool.objects, &vtable, sizeof(vtable));
        pool.objects[P::TypeOffset] = 3;
        const CVector linear(1, 2, 3), angular(4, 5, 6);
        std::memcpy(pool.objects + P::LinearOffset, &linear, sizeof(linear));
        std::memcpy(pool.objects + P::AngularOffset, &angular, sizeof(angular));
        PedVelocityOwnerContext context;
        ClockReaderTestHost::image = fixture.image;
        ClockPedVelocityOwnerTest::stores = ClockPedVelocityOwnerTest::failAt = 0;
        Check(runtime.access.ReadPedVelocity(1, context.kind, context.output) == ClockResult::NotInitialized && context.output.fX == 101 &&
                  runtime.access.SetPedVelocity(1, context.kind, context.target) == ClockResult::NotInitialized && !ClockPedVelocityOwnerTest::stores,
              "unbound vector access preserves caller output and supplies no native write scope");
        const auto retire = [&](NativeClockService& service)
        {
            if (!service.End())
            {
                fixture.invokeTimer();
                Check(service.End(), "native vector owner drains only genuine pending callback work");
            }
            Check(!service.IsRetained() && !runtime.timingHandler && !runtime.timingContext &&
                      !NativeServiceAdmission::IsClaimed(NativeServiceAdmission::Category::Clock),
                  "persistent scalar effects create no fabricated rollback or deferred repair debt");
        };
        {
            NativeClockService service;
            BeginOwned(service, fixture, false, true, true, &PedVelocityOwnerHandler, &context);
            Check(runtime.access.SetPedVelocity(1, context.kind, context.target) != ClockResult::Ok && !ClockPedVelocityOwnerTest::stores,
                  "a producer between callbacks cannot borrow native vector mutation authority");
            fixture.invokeTimer();
            Check(context.result == ClockResult::Ok && context.output.fX == 1 && context.output.fY == 2 && context.output.fZ == 3,
                  "the actual admitted callback copies raw native linear velocity into caller storage");
            context.kind = NativePedVelocityKind::Angular;
            context.write = true;
            Check(service.Renew(runtime.generation.load()), "renew the real owner before its independent vector intent");
            fixture.invokeTimer();
            Check(context.result == ClockResult::Ok && ClockPedVelocityOwnerTest::stores == 3 && !runtime.stopping.load(),
                  "the actual retained callback commits the native angular vector through three real aligned scalar exchanges");
            retire(service);
            CVector actual;
            std::memcpy(&actual, pool.objects + P::AngularOffset, sizeof(actual));
            Check(actual.fX == 10 && actual.fY == 20 && actual.fZ == 30,
                  "End preserves accepted native velocity instead of replaying an earlier physical snapshot");
        }
        {
            context.kind = NativePedVelocityKind::Linear;
            context.target = CVector(40, 50, 60);
            const auto stores = ClockPedVelocityOwnerTest::stores;
            ClockPedVelocityOwnerTest::failAt = stores + 2;
            NativeClockService service;
            BeginOwned(service, fixture, false, true, true, &PedVelocityOwnerHandler, &context);
            fixture.invokeTimer();
            Check(context.result == ClockResult::WriteFailure && runtime.stopping.load() && ClockPedVelocityOwnerTest::stores == stores + 2,
                  "a second-component exchange fault publishes entered failure and stops the same real native owner");
            Check(runtime.access.SetPedVelocity(1, context.kind, context.target) != ClockResult::Ok && ClockPedVelocityOwnerTest::stores == stores + 2,
                  "a partially applied native vector is never automatically replayed through the failed lease");
            retire(service);
            CVector actual;
            std::memcpy(&actual, pool.objects + P::LinearOffset, sizeof(actual));
            Check(actual.fX == 40 && actual.fY == 2 && actual.fZ == 3,
                  "terminal cleanup preserves the accepted first scalar and untouched later components without acquiring a repair lease");
        }
        ClockPedVelocityOwnerTest::failAt = 0;
        ClockReaderTestHost::image = 0;
        for (auto item = saved.rbegin(); item != saved.rend(); ++item)
            fixture.Patch(item->rva, item->bytes.data(), item->bytes.size());
        std::puts("Production native ped velocity callback owner fixture completed");
    }
}
