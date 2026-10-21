/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Complete CRopes through the actual retained native callback owner
 *****************************************************************************/
namespace
{
    struct RopesOwnerContext
    {
        unsigned       operation = 0, refusal = 0;
        CRopes*        ropes = reinterpret_cast<CRopes*>(std::uintptr_t{1});
        CVector        position{1.25f, -2.5f, 300.0f};
        std::uint32_t  duration = 4000;
        std::int32_t   identity = -9876;
        std::uintptr_t key = 0xdeadbeef;
        ClockResult    result = ClockResult::NotInitialized;
        bool           threw = false;
    };

    void RopesOwnerHandler(NativeGameTimingAccess& access, void* opaque)
    {
        auto&      context = *static_cast<RopesOwnerContext*>(opaque);
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
        context.threw = false;
        try
        {
            switch (context.operation)
            {
                case 0:
                    context.result = access.GetRopes(context.ropes);
                    break;
                case 1:
                    context.identity = context.ropes->CreateRopeForSwatPed(context.position, context.duration);
                    context.result = ClockResult::Ok;
                    break;
                case 2:
                    context.ropes->RemoveEntityRope(reinterpret_cast<CEntitySAInterface*>(context.key));
                    context.result = ClockResult::Ok;
                    break;
                case 3:
                    context.result = access.CreateSwatRope(context.position, context.duration, context.identity);
                    break;
                case 4:
                    context.result = access.RemoveEntityRope(context.key);
                    break;
                case 5:
                    context.identity = context.ropes->CreateRopeForSwatPed(context.position);
                    context.result = ClockResult::Ok;
                    break;
                default:
                    std::abort();
            }
        }
        catch (const std::runtime_error&)
        {
            context.threw = true;
            context.result = ClockResult::WriteFailure;
        }
        runtime.refreshed.store(refreshed);
        if (context.refusal == 3)
            --callbackDepth;
        runtime.admissionGeneration.store(admission);
        timingCallbackGeneration = generation;
    }

    void NativeRopesOwnedSdk(ClockImageFixture& fixture)
    {
        namespace R = NativeRopesProfile;
        namespace S = NativeGameStateProfile;
        static_assert(!std::is_abstract_v<CRopesDE>);
        // Only the final native call boundary is replaced. The actual image,
        // generation, callback, loaded-state and row lifetime checks still run.
        struct SavedPin
        {
            std::uintptr_t             rva;
            std::vector<unsigned char> bytes;
        };
        std::vector<SavedPin> pins;
        const auto            admit = [&](std::uintptr_t rva, const unsigned char* bytes, std::size_t size, bool code)
        {
            SavedPin saved{rva, std::vector<unsigned char>(size)};
            SIZE_T   copied = 0;
            Check(::ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(fixture.image + rva), saved.bytes.data(), size, &copied) && copied == size,
                  "save each bounded native rope qualification span in the owned image");
            pins.push_back(std::move(saved));
            fixture.Patch(rva, bytes, size);
            DWORD prior = 0;
            Check(::VirtualProtect(reinterpret_cast<void*>(fixture.image + rva), size, code ? PAGE_EXECUTE_READ : PAGE_READONLY, &prior) != FALSE,
                  "rope qualification consumes real executable/read-only image pages without executing fixture native bytes");
        };
        for (const auto& pin : R::Code)
            admit(pin.rva, pin.bytes.data(), pin.size, true);
        for (const auto& pin : S::pins)
            admit(pin.rva, pin.bytes, pin.size, pin.code);
        const auto table = fixture.Get<std::array<unsigned char, R::RowCount * R::RowBytes>>(R::TableRva);
        const auto references = std::make_unique<std::array<unsigned char, R::ReferenceCount * R::ReferenceBytes>>(
            fixture.Get<std::array<unsigned char, R::ReferenceCount * R::ReferenceBytes>>(R::ReferencesRva));
        const auto oldCounter = fixture.Get<std::uint32_t>(R::CounterRva);
        const auto oldState = fixture.Get<std::uint32_t>(S::stateRva);
        const auto oldPlayer = fixture.Get<std::uintptr_t>(NativePlayerInfoProfile::BankRva);
        const auto oldDestroy = fixture.Get<std::uintptr_t>(R::HookVtableRva);
        const auto oldRemove = fixture.Get<std::uintptr_t>(R::HookVtableRva + 0x20);
        const auto writable = [&](std::uintptr_t rva, std::size_t bytes)
        {
            DWORD prior = 0;
            Check(::VirtualProtect(reinterpret_cast<void*>(fixture.image + rva), bytes, PAGE_READWRITE, &prior) != FALSE,
                  "only owned image rope data is admitted as writable native storage");
        };
        writable(R::TableRva, table.size());
        writable(R::ReferencesRva, references->size());
        writable(R::CounterRva, 4);
        writable(S::stateRva, 4);
        writable(NativePlayerInfoProfile::BankRva, 8);
        const auto clearRows = [&]
        {
            const std::array<unsigned char, R::RowCount * R::RowBytes> empty{};
            fixture.Put(R::TableRva, empty);
        };
        clearRows();
        std::vector<unsigned char> noReferences(references->size());
        ClockRopesOwnerTest::Write(fixture.image + R::ReferencesRva, noReferences.data(), noReferences.size());
        fixture.Put(R::CounterRva, std::uint32_t{0});
        fixture.Put(S::stateRva, std::uint32_t{9});
        fixture.Put(NativePlayerInfoProfile::BankRva, std::uintptr_t{0});
        const auto destructor = fixture.image + R::HookDestructorRva;
        const auto remove = fixture.image + R::HookRemoveRva;
        fixture.Patch(R::HookVtableRva, &destructor, sizeof(destructor));
        fixture.Patch(R::HookVtableRva + 0x20, &remove, sizeof(remove));
        DWORD prior = 0;
        Check(::VirtualProtect(reinterpret_cast<void*>(fixture.image + R::HookVtableRva), 0x28, PAGE_READONLY, &prior) != FALSE,
              "rope hook class targets are actual relocated image pointers on read-only vtable pages");
        ClockReaderTestHost::image = ClockRopesOwnerTest::image = fixture.image;
        ClockRopesOwnerTest::creates = ClockRopesOwnerTest::removes = 0;
        ClockRopesOwnerTest::result = ClockResult::Ok;
        ClockRopesOwnerTest::nativeResult = 1;
        ClockRopesOwnerTest::effect = true;
        RopesOwnerContext context;
        Check(runtime.access.GetRopes(context.ropes) == ClockResult::NotInitialized && context.ropes == reinterpret_cast<CRopes*>(std::uintptr_t{1}) &&
                  runtime.access.CreateSwatRope(context.position, context.duration, context.identity) == ClockResult::NotInitialized &&
                  context.identity == -9876,
              "unbound ropes cannot publish an SDK object, mutate native state or overwrite output");
        const auto retire = [&](NativeClockService& service)
        {
            if (!service.End())
            {
                fixture.invokeTimer();
                Check(service.End(), "native rope callback retirement drains genuine pending owner work if present");
            }
            Check(!service.IsRetained() && !runtime.ropesMemory.IsBound() && !runtime.ropesGeneration && !runtime.timingHandler && !runtime.timingContext &&
                      !NativeServiceAdmission::IsClaimed(NativeServiceAdmission::Category::Clock),
                  "rope retirement releases its binding only after the same retained engine owner retires");
        };
        {
            NativeClockService service;
            BeginOwned(service, fixture, false, true, true, &RopesOwnerHandler, &context);
            const auto invoke = [&]
            {
                Check(service.Renew(runtime.generation.load()), "renew one genuine native ropes callback lease");
                fixture.invokeTimer();
            };
            invoke();
            Check(context.result == ClockResult::Ok && context.ropes == &runtime.ropesSdk && runtime.ropesMemory.IsBound() &&
                      runtime.ropesGeneration == runtime.generation.load() && !fixture.Get<std::uintptr_t>(NativePlayerInfoProfile::BankRva),
                  "complete CRopes publication requires the loaded native world, not CJ's player-info row");
            auto* const sdk = context.ropes;
            invoke();
            Check(context.result == ClockResult::Ok && context.ropes == sdk, "fresh rope callbacks preserve the actual complete SDK wrapper identity");
            context.operation = 5;
            invoke();
            Check(context.result == ClockResult::Ok && context.identity == 100 && ClockRopesOwnerTest::entry == R::CreateRva &&
                      ClockRopesOwnerTest::identity == 100 && ClockRopesOwnerTest::duration == 4000 && fixture.Get<std::uint32_t>(R::CounterRva) == 1,
                  "actual CRopes default duration reaches the proved native Swat entry and commits its generated ID");
            context.operation = 1;
            context.duration = 0xffffffff;
            fixture.Put(R::CounterRva, std::uint32_t{255});
            invoke();
            Check(context.result == ClockResult::Ok && context.identity == 355 && ClockRopesOwnerTest::duration == 0xffffffff &&
                      fixture.Get<std::uint32_t>(R::CounterRva) == 0,
                  "native DWORD duration and eight-bit ID counter wrap retain the genuine DE branch semantics");
            clearRows();
            for (std::size_t i = 0; i < R::RowCount; ++i)
            {
                fixture.Put(R::TableRva + i * R::RowBytes + R::IdentityOffset, std::uintptr_t{1000} + i);
                fixture.Put(R::TableRva + i * R::RowBytes + R::TypeOffset, static_cast<unsigned char>(8));
            }
            ClockRopesOwnerTest::effect = false;
            ClockRopesOwnerTest::nativeResult = 0;
            invoke();
            Check(context.result == ClockResult::Ok && context.identity == 100 && fixture.Get<std::uint32_t>(R::CounterRva) == 1 &&
                      fixture.Get<std::uintptr_t>(R::TableRva + R::IdentityOffset) == 1000,
                  "DE Register0/full still advances and returns the script ID without inventing a native row");
            ClockRopesOwnerTest::effect = true;
            ClockRopesOwnerTest::nativeResult = 1;
            clearRows();
            fixture.Put(R::TableRva + R::IdentityOffset, context.key);
            fixture.Put(R::TableRva + R::TypeOffset, static_cast<unsigned char>(8));
            context.operation = 2;
            invoke();
            Check(context.result == ClockResult::Ok && ClockRopesOwnerTest::entry == R::RemoveRva && ClockRopesOwnerTest::row == fixture.image + R::TableRva &&
                      !fixture.Get<unsigned char>(R::TableRva + R::TypeOffset),
                  "actual CRopes removal compares an unreadable opaque identity and calls the genuine row receiver without dereferencing the argument");
            context.key = 0xbaadf00d;
            const auto removes = ClockRopesOwnerTest::removes;
            invoke();
            Check(context.result == ClockResult::Ok && ClockRopesOwnerTest::removes == removes,
                  "missing native identity completes the classic search without a fabricated native action");
            context.operation = 0;
            const auto creates = ClockRopesOwnerTest::creates;
            for (unsigned refusal = 1; refusal <= 4; ++refusal)
            {
                context.refusal = refusal;
                context.ropes = reinterpret_cast<CRopes*>(std::uintptr_t{1});
                invoke();
                Check(context.result != ClockResult::Ok && context.ropes == reinterpret_cast<CRopes*>(std::uintptr_t{1}),
                      "stale generation/admission, nested callback and expired scope cannot publish a ropes SDK lease");
                context.operation = 3;
                context.identity = -9876;
                invoke();
                Check(context.result != ClockResult::Ok && context.identity == -9876 && ClockRopesOwnerTest::creates == creates,
                      "the same owner refuses rope entry and preserves output after callback authority expires");
                context.operation = 0;
            }
            context.refusal = 0;
            const auto generation = runtime.ropesGeneration;
            runtime.ropesGeneration = generation + 1;
            invoke();
            Check(context.result == ClockResult::OwnerConflict, "a rope binding from another generation is not reusable authority");
            runtime.ropesGeneration = generation;
            for (const std::uint32_t state : {6u, 7u, 8u})
            {
                fixture.Put(S::stateRva, state);
                context.operation = 3;
                invoke();
                Check(context.result == ClockResult::NotInitialized && ClockRopesOwnerTest::creates == creates,
                      "rope native action cannot enter from a loading or startup transition instead of playing9");
            }
            fixture.Put(S::stateRva, std::uint32_t{9});
            const auto foreign = static_cast<unsigned char>(R::Code[0].bytes[0] ^ 1);
            fixture.Patch(R::Code[0].rva, &foreign, 1);
            invoke();
            Check(context.result == ClockResult::BindingChanged && ClockRopesOwnerTest::creates == creates,
                  "foreign exact-profile code refuses before a native rope call");
            fixture.Patch(R::Code[0].rva, R::Code[0].bytes.data(), 1);
            retire(service);
            Check(fixture.Get<std::uint32_t>(R::CounterRva) == 1 && fixture.Get<std::uintptr_t>(R::TableRva + R::IdentityOffset) == 0xdeadbeef &&
                      !fixture.Get<unsigned char>(R::TableRva + R::TypeOffset),
                  "End preserves accepted rope creation/removal effects instead of replaying an old table or counter");
        }
        // Native entry can take effect before an exception or post-call refusal.
        // These are terminal actions, not a transaction eligible for replay.
        for (unsigned failure = 0; failure < 4; ++failure)
        {
            clearRows();
            fixture.Put(R::CounterRva, std::uint32_t{20});
            fixture.Put(S::stateRva, std::uint32_t{9});
            context.operation = failure == 3 ? 4 : 3;
            context.identity = -9876;
            context.key = 0xdeadbeef;
            if (failure == 3)
            {
                fixture.Put(R::TableRva + R::IdentityOffset, context.key);
                fixture.Put(R::TableRva + R::TypeOffset, static_cast<unsigned char>(8));
            }
            ClockRopesOwnerTest::result = failure == 0 || failure == 3 ? ClockResult::OriginalException : ClockResult::Ok;
            if (failure == 1)
                ClockRopesOwnerTest::after = [] { runtime.admissionGeneration.fetch_add(1); };
            if (failure == 2)
                ClockRopesOwnerTest::after = []
                {
                    const std::uint32_t later = 81;
                    ClockRopesOwnerTest::Write(runtime.image + NativeRopesProfile::CounterRva, &later, sizeof(later));
                };
            NativeClockService service;
            BeginOwned(service, fixture, false, true, true, &RopesOwnerHandler, &context);
            const auto entries = ClockRopesOwnerTest::creates + ClockRopesOwnerTest::removes;
            fixture.invokeTimer();
            const auto expected = failure == 0 || failure == 3 ? ClockResult::OriginalException : ClockResult::OwnerConflict;
            Check(context.result == expected && ClockRopesOwnerTest::creates + ClockRopesOwnerTest::removes == entries + 1 && runtime.stopping.load(),
                  "entered native exception, revoked post-call scope or foreign counter stops the same real owner without replay");
            Check(context.identity == -9876 && runtime.access.CreateSwatRope(context.position, context.duration, context.identity) != ClockResult::Ok &&
                      ClockRopesOwnerTest::creates + ClockRopesOwnerTest::removes == entries + 1,
                  "a stopped lease preserves caller output and cannot retry an already-entered rope action");
            const auto type = fixture.Get<unsigned char>(R::TableRva + R::TypeOffset);
            Check(type == (failure == 3 ? 0 : 8), "observable native row effects survive the later refusal honestly");
            retire(service);
            Check(fixture.Get<unsigned char>(R::TableRva + R::TypeOffset) == type && fixture.Get<std::uint32_t>(R::CounterRva) == (failure == 2 ? 81u : 20u),
                  "cleanup neither rewinds native effects nor overwrites a foreign counter advance");
        }
        context.ropes = reinterpret_cast<CRopes*>(std::uintptr_t{1});
        Check(runtime.access.GetRopes(context.ropes) == ClockResult::NotInitialized && context.ropes == reinterpret_cast<CRopes*>(std::uintptr_t{1}),
              "retired generation cannot resurrect the complete native ropes service");
        ClockRopesOwnerTest::result = ClockResult::Ok;
        ClockRopesOwnerTest::after = nullptr;
        ClockReaderTestHost::image = ClockRopesOwnerTest::image = 0;
        fixture.Put(R::TableRva, table);
        ClockRopesOwnerTest::Write(fixture.image + R::ReferencesRva, references->data(), references->size());
        fixture.Put(R::CounterRva, oldCounter);
        fixture.Put(S::stateRva, oldState);
        fixture.Put(NativePlayerInfoProfile::BankRva, oldPlayer);
        fixture.Patch(R::HookVtableRva, &oldDestroy, sizeof(oldDestroy));
        fixture.Patch(R::HookVtableRva + 0x20, &oldRemove, sizeof(oldRemove));
        for (auto pin = pins.rbegin(); pin != pins.rend(); ++pin)
            fixture.Patch(pin->rva, pin->bytes.data(), pin->bytes.size());
        std::puts("Production complete native ropes SDK owner fixture completed");
    }
}
