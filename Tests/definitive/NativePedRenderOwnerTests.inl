/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual registered-ped opaque render observation and independent profile
 ****************************************************************************/
namespace ClockPedRenderOwnerTest
{
    std::uintptr_t address = 0;
    unsigned       reads = 0, failAt = 0;
    void (*afterRead)() noexcept = nullptr;
    BOOL WINAPI Read(HANDLE process, LPCVOID source, LPVOID destination, SIZE_T bytes, SIZE_T* copied)
    {
        const bool selected = reinterpret_cast<std::uintptr_t>(source) == address && bytes == sizeof(std::uintptr_t);
        if (selected)
        {
            ++reads;
            if (failAt && reads == failAt)
            {
                failAt = 0;
                if (copied)
                    *copied = 0;
                return FALSE;
            }
        }
        const auto result = ::ReadProcessMemory(process, source, destination, bytes, copied);
        if (result && selected && afterRead)
        {
            const auto action = afterRead;
            afterRead = nullptr;
            action();
        }
        return result;
    }
}
namespace PedRenderOwnerFixture
{
    namespace Hook = ClockPedRenderOwnerTest;
    std::uintptr_t           field = 0;
    unsigned                 mode = 0;
    unsigned char*           poolFlag = nullptr;
    NativePedReferenceToken* callerToken = nullptr;
    void                     ChangeAfterCopy() noexcept
    {
        if (mode == 1)
        {
            const std::uintptr_t next = 0x9988;
            std::memcpy(reinterpret_cast<void*>(field), &next, sizeof(next));
        }
        if (mode == 2)
            *poolFlag = 2;
        if (mode == 3)
            ++timingCallbackGeneration;
        if (mode == 4)
        {
            callerToken->ownerGeneration = 0;
            ++callerToken->version;
        }
        if (mode == 5)
        {
            const LONG active = 1;
            std::memcpy(reinterpret_cast<void*>(runtime.image + NativeBorrowedGcProfile::markerRva), &active, sizeof(active));
        }
    }
    struct Context
    {
        std::function<void(NativeGameTimingAccess&)> action;
    };
    void Handler(NativeGameTimingAccess& access, void* opaque)
    {
        static_cast<Context*>(opaque)->action(access);
    }
    void RunPrepared(ClockImageFixture& fixture)
    {
        namespace P = NativePedReferenceProfile;
        namespace Ped = NativePedPoolProfile;
        namespace M = NativePedMetadataProfile;
        namespace R = NativePedRenderProfile;
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
        for (const auto& pin : M::Code)
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
        admit(NativePlayerInfoProfile::BankRva, &zero, sizeof(zero), PAGE_READWRITE);
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
        const auto vtable = fixture.image + P::PedVtableRva;
        std::memcpy(pool.objects, &vtable, sizeof(vtable));
        pool.objects[P::TypeOffset] = 0xab;
        ClockReaderTestHost::image = ClockPedReferenceOwnerTest::image = fixture.image;
        ClockPedReferenceOwnerTest::registers = ClockPedReferenceOwnerTest::unregisters = 0;
        ClockPedReferenceOwnerTest::partialUnlink = false;
        ClockPedReferenceOwnerTest::unregisterResult = ClockResult::Ok;
        pool.flags[1] = 1;
        const auto playerVtable = fixture.image + P::PlayerVtableRva;
        std::memcpy(pool.objects + Ped::ObjectStride, &playerVtable, sizeof(playerVtable));
        pool.objects[Ped::ObjectStride + P::TypeOffset] = 0xab;
        const std::uint16_t model = 0xffff;
        const std::uint64_t left = 0x13579bdf2468ace0ull, right = 0x1020304050607080ull;
        for (std::size_t i = 0; i != 2; ++i)
        {
            auto* object = pool.objects + i * Ped::ObjectStride;
            std::memcpy(object + M::ModelOffset, &model, sizeof(model));
            std::memcpy(object + R::RenderOffset - 8, &left, 8);
            std::memcpy(object + R::RenderOffset + 8, &right, 8);
        }
        Context context;
        context.action = [](NativeGameTimingAccess&) {};
        NativeClockService service;
        BeginOwned(service, fixture, false, true, true, &Handler, &context);
        HMODULE module = nullptr;
        Check(GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN, reinterpret_cast<LPCWSTR>(&HookClockContinuation),
                                 &module) &&
                  module,
              "pin actual compiled callback code before registered native cells are published");
        runtime.modulePinned.store(true);
        const auto generation = runtime.generation.load();
        const auto action = [&](const char* label, const auto& body)
        {
            context.action = [&](NativeGameTimingAccess&)
            {
                try
                {
                    body();
                }
                catch (const std::exception& error)
                {
                    std::fprintf(stderr, "Ped render action %s refused: %s\n", label, error.what());
                    Check(false, label);
                }
            };
            Check(service.Renew(generation), "renew actual registered-ped owner for each bounded render observation");
            fixture.invokeTimer();
            context.action = [](NativeGameTimingAccess&) {};
        };
        std::array<NativePedReferenceToken, 2> tokens{};
        for (std::size_t i = 0; i != tokens.size(); ++i)
        {
            action("actual registered receiver issuance",
                   [&]
                   {
                       Check(runtime.access.AcquirePedReference(static_cast<std::uint32_t>((i << 8) | 1), tokens[i]) == ClockResult::Ok &&
                                 tokens[i].ownerGeneration == generation,
                             "render reader borrows only an issued current generic/player registered token");
                   });
        }
        Check(runtime.pedReferences.IsRetained() && ClockPedReferenceOwnerTest::registers == 2,
              "two real native registration acknowledgements remain lifecycle debt");
        std::array<unsigned char, Ped::ObjectStride * 2> original{};
        std::memcpy(original.data(), pool.objects, original.size());
        const auto seed = [&](std::size_t index, std::uintptr_t value)
        {
            std::memcpy(pool.objects + index * Ped::ObjectStride + R::RenderOffset, &value, sizeof(value));
            std::memcpy(original.data() + index * Ped::ObjectStride + R::RenderOffset, &value, sizeof(value));
        };
        const auto intact = [&]
        {
            return !std::memcmp(original.data(), pool.objects, original.size()) && ClockPedReferenceOwnerTest::registers == 2 &&
                   !ClockPedReferenceOwnerTest::unregisters && runtime.pedReferences.IsRetained() && !runtime.playerInfo.IsBound() &&
                   !fixture.Get<std::uintptr_t>(NativePlayerInfoProfile::BankRva);
        };
        const auto refuses = [](const auto& body)
        {
            try
            {
                body();
            }
            catch (const std::exception&)
            {
                return true;
            }
            return false;
        };
        ped_render_fixture::PedRenderFixture generic(runtime.access, runtime.access, tokens[0]), player(runtime.access, runtime.access, tokens[1]);
        std::array<CPedMetadataDE*, 2>       views{&generic, &player};
        for (std::size_t i = 0; i != tokens.size(); ++i)
        {
            for (const auto raw : {std::uintptr_t{0}, std::uintptr_t{1}, (std::numeric_limits<std::uintptr_t>::max)(), fixture.image})
            {
                seed(i, raw);
                field = reinterpret_cast<std::uintptr_t>(pool.objects + i * Ped::ObjectStride + R::RenderOffset);
                Hook::address = field;
                action("actual raw nullable render getter",
                       [&]
                       {
                           Check(reinterpret_cast<std::uintptr_t>(views[i]->GetRpClump()) == raw && intact(),
                                 "actual generic/player SDK returns exactped+20 including null/unreadable/image/poison tags without pointee/schema/model "
                                 "predicates");
                       });
            }
        }
        field = reinterpret_cast<std::uintptr_t>(pool.objects + R::RenderOffset);
        Hook::address = field;
        seed(0, 0x12345678);
        action("independent old metadata channel",
               [&]
               {
                   const auto& pin = R::Code[4];
                   const auto  changed = static_cast<unsigned char>(pin.bytes[0] ^ 1u);
                   fixture.Patch(pin.rva, &changed, 1);
                   std::uintptr_t    output = 71;
                   const auto        render = runtime.access.ReadPedRender(tokens[0], output);
                   NativePedMetadata metadata;
                   const auto        old = runtime.access.ReadPedMetadata(tokens[0], metadata);
                   fixture.Patch(pin.rva, pin.bytes, 1);
                   Check(render == ClockResult::BindingChanged && output == 71 && old == ClockResult::Ok && metadata.modelIndex == 0xffff &&
                             metadata.entityType == 3 && intact(),
                         "new render producer pins affect only the new channel, never old rawmodel/type observation");
               });
        action("unregistered stale and owner-generation tokens",
               [&]
               {
                   std::uintptr_t          output = 71;
                   NativePedReferenceToken absent;
                   const auto              none = runtime.access.ReadPedRender(absent, output);
                   auto                    stale = tokens[0];
                   ++stale.version;
                   const auto version = runtime.access.ReadPedRender(stale, output);
                   stale = tokens[0];
                   ++stale.ownerGeneration;
                   const auto owner = runtime.access.ReadPedRender(stale, output);
                   Check(none != ClockResult::Ok && version != ClockResult::Ok && owner == ClockResult::OwnerConflict && output == 71 &&
                             !runtime.stopping.load() && intact(),
                         "only actual current registered-cell identity can publish rawrender; refusals preserve caller output");
               });
        for (unsigned failure = 1; failure != 3; ++failure)
        {
            action("first and last distinct private field reads",
                   [&]
                   {
                       Hook::reads = 0;
                       Hook::failAt = failure;
                       std::uintptr_t output = 71;
                       const auto     result = runtime.access.ReadPedRender(tokens[0], output);
                       Hook::failAt = 0;
                       Check(result == ClockResult::ReadFailure && output == 71 && Hook::reads == failure && !runtime.stopping.load() && intact(),
                             "real OS field-copy loss cannot become invented null or a native cleanup obligation");
                   });
        }
        for (mode = 1; mode != 6; ++mode)
        {
            action("rawfield poolgen currentowner and caller bookends",
                   [&]
                   {
                       auto source = tokens[0];
                       callerToken = &source;
                       poolFlag = pool.flags;
                       Hook::reads = 0;
                       Hook::afterRead = &ChangeAfterCopy;
                       const auto     previous = timingCallbackGeneration;
                       std::uintptr_t output = 71;
                       const auto     result = runtime.access.ReadPedRender(source, output);
                       Hook::afterRead = nullptr;
                       callerToken = nullptr;
                       poolFlag = nullptr;
                       timingCallbackGeneration = previous;
                       fixture.Put(NativeBorrowedGcProfile::markerRva, LONG{0});
                       pool.flags[0] = 1;
                       seed(0, 0x12345678);
                       if (mode == 4)
                           Check(result == ClockResult::Ok && output == 0x12345678 && !source.ownerGeneration && intact(),
                                 "runtime snapshots token before field-copy callback mutates caller CPU identity");
                       else
                           Check(result != ClockResult::Ok && output == 71 && !runtime.stopping.load() && intact(),
                                 "fieldflip/currentpoolreuse/generation/GC loss refuses a torn render value without altering ped or unrelated fields");
                   });
        }
        action(
            "foreign native thread has no read admission",
            [&]
            {
                ClockResult    result = ClockResult::Ok, nativeThread = ClockResult::Ok;
                std::uintptr_t output = 71;
                std::thread    worker(
                    [&]
                    {
                        result = runtime.access.ReadPedRender(tokens[0], output);
                        nativeThread = NativeStartGameCalls::CheckThread(fixture.image);
                    });
                worker.join();
                Check(result == ClockResult::NotInitialized && nativeThread == ClockResult::WrongThread && output == 71 && !runtime.stopping.load() && intact(),
                      "missing timing callback and actualforeign GT guard independently refuse without borrowing a render target");
            });
        std::uintptr_t output = 71;
        Check(runtime.access.ReadPedRender(tokens[0], output) != ClockResult::Ok && output == 71 && refuses([&] { static_cast<void>(generic.GetRpClump()); }) &&
                  intact(),
              "valid CPU token alone cannot readrender outside its admitted engine callback");
        Hook::afterRead = nullptr;
        Hook::failAt = 0;
        Hook::address = 0;
        const auto releases = ClockPedReferenceOwnerTest::unregisters;
        Check(!service.End() && runtime.pedReferences.IsRetained() && ClockPedReferenceOwnerTest::unregisters == releases,
              "producer End cannot release registered native cells before their qualified retirement callback");
        fixture.invokeTimer();
        Check(service.End() && !service.IsRetained() && !runtime.pedReferences.IsRetained() && ClockPedReferenceOwnerTest::unregisters == releases + 2 &&
                  !runtime.timingHandler && !NativeServiceAdmission::IsClaimed(NativeServiceAdmission::Category::Clock),
              "actual lifecycle acknowledges both registered-cell releases while acquiring no render target cleanup authority");
        for (std::size_t i = 0; i != 2; ++i)
            Check(!std::memcmp(pool.objects + i * Ped::ObjectStride + R::RenderOffset - 8, original.data() + i * Ped::ObjectStride + R::RenderOffset - 8, 24),
                  "reference cleanup never modifies rawrender or either neighboring field and never dereferences its target");
        ClockReaderTestHost::image = ClockPedReferenceOwnerTest::image = 0;
        field = 0;
        for (auto item = saved.rbegin(); item != saved.rend(); ++item)
            fixture.Patch(item->rva, item->bytes.data(), item->bytes.size());
        std::puts("Production native ped render owner fixture completed");
        std::puts("Production actual ped render SDK fixture completed");
    }
}
