/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Retain the real callback owner while native pointer cells survive
 ****************************************************************************/
namespace
{
    struct PedReferenceOwnerContext
    {
        NativePedReferenceToken    token;
        NativePedPoolReader::Value value;
        ClockResult                result = ClockResult::NotInitialized;
        bool                       resolve = false;
    };
    void PedReferenceOwnerHandler(NativeGameTimingAccess& access, void* opaque)
    {
        auto& context = *static_cast<PedReferenceOwnerContext*>(opaque);
        context.result = context.resolve ? access.ResolvePedReference(context.token, context.value) : access.AcquirePedReference(1, context.token);
    }
    void NativePedReferenceOwnedSdk(ClockImageFixture& fixture)
    {
        namespace P = NativePedReferenceProfile;
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
        PedReferenceOwnerContext context;
        context.token.version = 99;
        context.token.ownerGeneration = 777;
        context.token.slot = 123;
        context.value.handle = 0xdeadbeef;
        Check(runtime.access.AcquirePedReference(1, context.token) == ClockResult::NotInitialized && context.token.version == 99 &&
                  context.token.ownerGeneration == 777 && !ClockPedReferenceOwnerTest::registers,
              "unbound reference acquire preserves output and grants no native registration authority");
        const auto retained = [&]()
        { return runtime.pedReferences.IsRetained() && runtime.hooked.load() && NativeServiceAdmission::IsClaimed(NativeServiceAdmission::Category::Clock); };
        const auto retired = [&](NativeClockService& service)
        {
            Check(service.End() && !service.IsRetained() && !runtime.pedReferences.IsRetained() && !runtime.referenceGeneration.load() &&
                      !runtime.timingHandler && !runtime.timingContext && !NativeServiceAdmission::IsClaimed(NativeServiceAdmission::Category::Clock),
                  "actual terminal acknowledgement retires controller, hooks, callback context and admission together");
        };
        {
            NativeClockService service;
            BeginOwned(service, fixture, false, true, true, &PedReferenceOwnerHandler, &context);
            Check(runtime.access.AcquirePedReference(1, context.token) != ClockResult::Ok && !ClockPedReferenceOwnerTest::registers,
                  "a producer between callbacks cannot register a native-visible cell");
            fixture.invokeTimer();
            Check(context.result == ClockResult::Ok && context.token.version && context.token.ownerGeneration == runtime.generation.load() &&
                      ClockPedReferenceOwnerTest::registers == 1 && ClockPedReferenceOwnerTest::forced && ClockPedReferenceOwnerTest::entry == P::RegisterRva &&
                      retained(),
                  "the actual retained callback verifies a forced native registration before publishing its CPU token");
            const auto cell = ClockPedReferenceOwnerTest::cell;
            context.resolve = true;
            Check(runtime.access.ResolvePedReference(context.token, context.value) != ClockResult::Ok && context.value.handle == 0xdeadbeef,
                  "a matching token alone supplies no between-callback native pointer borrow");
            Check(service.Renew(runtime.generation.load()), "renew the real reference owner before the next callback borrow");
            fixture.invokeTimer();
            Check(context.result == ClockResult::Ok && context.value.handle == 1 && context.value.nativePed == reinterpret_cast<std::uintptr_t>(pool.objects),
                  "the actual callback resolves its token through the real pool and exact registered cell address");
            Check(!service.End() && service.IsRetained() && retained() && !ClockPedReferenceOwnerTest::unregisters,
                  "producer End keeps the owner and cell alive instead of calling native unlink outside its callback");
            fixture.invokeTimer();
            Check(ClockPedReferenceOwnerTest::unregisters == 1 && ClockPedReferenceOwnerTest::cell == cell &&
                      ClockPedReferenceOwnerTest::entry == P::UnregisterRva,
                  "the same natural cleanup callback unlinks the original stable cell address exactly once");
            retired(service);
        }
        {
            context.resolve = false;
            NativeClockService service;
            BeginOwned(service, fixture, false, true, true, &PedReferenceOwnerHandler, &context);
            fixture.invokeTimer();
            Check(context.result == ClockResult::Ok && retained(), "a later owner generation registers through the same real controller");
            const auto cell = ClockPedReferenceOwnerTest::cell;
            const auto node = ClockPedReferenceOwnerTest::node;
            const auto calls = ClockPedReferenceOwnerTest::unregisters;
            ClockPedReferenceOwnerTest::partialUnlink = true;
            ClockPedReferenceOwnerTest::unregisterResult = ClockResult::OriginalException;
            Check(!service.End() && ClockPedReferenceOwnerTest::unregisters == calls,
                  "faulted-retirement scenario still cannot enter native cleanup on the producer");
            fixture.invokeTimer();
            Check(ClockPedReferenceOwnerTest::unregisters == calls + 1 && retained() && service.IsRetained() &&
                      ClockPedReferenceOwnerTest::Get<std::uintptr_t>(node + 8) == cell,
                  "an entered unlink fault keeps native-visible allocation, module, hooks and owner admission retained");
            ClockPedReferenceOwnerTest::Put(cell, std::uintptr_t{0});
            fixture.invokeTimer();
            Check(ClockPedReferenceOwnerTest::unregisters == calls + 1 && retained() && !service.End(),
                  "a zero pointer word is not cell-address retirement, and uncertain unlink is never replayed");
            // Only the separately proved whole-bank absence postcondition can
            // discharge uncertain storage debt. No old handle is retried.
            ClockPedReferenceOwnerTest::Put(node + 8, std::uintptr_t{0});
            fixture.invokeTimer();
            Check(ClockPedReferenceOwnerTest::unregisters == calls + 1, "bank-wide absence recovery frees storage without a second native unregister entry");
            retired(service);
        }
        ClockPedReferenceOwnerTest::partialUnlink = false;
        ClockPedReferenceOwnerTest::unregisterResult = ClockResult::Ok;
        ClockReaderTestHost::image = ClockPedReferenceOwnerTest::image = 0;
        for (auto item = saved.rbegin(); item != saved.rend(); ++item)
            fixture.Patch(item->rva, item->bytes.data(), item->bytes.size());
        std::puts("Production native ped reference callback owner fixture completed");
    }
}
