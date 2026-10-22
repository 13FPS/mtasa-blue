/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Persistent asynchronous request intent belongs to the real owner
 ****************************************************************************/
namespace
{
    struct StreamingRequestOwnerContext
    {
        std::uint32_t index = 3, flags = 6;
        ClockResult   result = ClockResult::NotInitialized;
    };
    void StreamingRequestOwnerHandler(NativeGameTimingAccess& access, void* opaque)
    {
        auto& context = *static_cast<StreamingRequestOwnerContext*>(opaque);
        context.result = access.RequestStreamingModel(context.index, context.flags);
    }
    void NativeStreamingRequestOwnedSdk(ClockImageFixture& fixture)
    {
        namespace P = NativeStreamingRequestProfile;
        namespace L = NativeModelLoadedProfile;
        namespace W = NativeStartupWorldProfile;
        namespace U = NativeStartupProvidersProfile;
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
                  "save exact owned request/model/registry spans before current callback admission");
            saved.push_back(std::move(prior));
            fixture.Patch(rva, bytes, size);
            DWORD previous = 0;
            Check(::VirtualProtect(reinterpret_cast<void*>(fixture.image + rva), size, protection, &previous) != FALSE,
                  "native request qualification retains immutable code and writable image data pages");
        };
        for (const auto& pin : NativePedPoolProfile::Code)
            admit(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : NativeModelBankProfile::Code)
            admit(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : L::Code)
            admit(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : P::Code)
            admit(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : NativeUObjectMembershipProfile::pins)
            admit(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : NativeGameStateProfile::pins)
            admit(pin.rva, pin.bytes, pin.size, pin.code ? PAGE_EXECUTE_READ : PAGE_READONLY);
        for (const auto& pin : NativeBorrowedGcProfile::pins)
            admit(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : NativeStartGameProfile::Code)
            if (pin.rva == 0xab879c || pin.rva == 0xab92f0 || pin.rva == 0x1af4ef0)
                admit(pin.rva, pin.bytes.data(), pin.size, PAGE_EXECUTE_READ);
        alignas(std::uintptr_t) std::array<unsigned char, P::ProviderOffset + sizeof(std::uintptr_t)>    owner{};
        alignas(std::uintptr_t) std::array<unsigned char, P::ProviderCountOffset + sizeof(std::int32_t)> provider{};
        alignas(std::uintptr_t) std::array<unsigned char, P::ModelPrefixBytes>                           model{};
        alignas(std::uintptr_t) std::array<unsigned char, 0x40>                                          meta{};
        std::vector<unsigned char>                                                                       providerRows(4 * P::ProviderRowStride);
        alignas(std::uintptr_t) std::array<unsigned char, 2 * U::objectItemBytes>                        items{};
        std::array<std::uintptr_t, 1>                                                                    chunks{reinterpret_cast<std::uintptr_t>(items.data())};
        const auto                                                                                       root = reinterpret_cast<std::uintptr_t>(owner.data());
        const auto providerAddress = reinterpret_cast<std::uintptr_t>(provider.data());
        const auto modelAddress = reinterpret_cast<std::uintptr_t>(model.data());
        const auto rowsAddress = reinterpret_cast<std::uintptr_t>(providerRows.data());
        const auto store = [](void* object, std::size_t offset, const auto& value)
        { std::memcpy(static_cast<unsigned char*>(object) + offset, &value, sizeof(value)); };
        const auto table = fixture.image + P::RootVtableRva;
        const auto metaAddress = reinterpret_cast<std::uintptr_t>(meta.data());
        for (const auto object : {root, providerAddress})
        {
            store(reinterpret_cast<void*>(object), 0, table);
            store(reinterpret_cast<void*>(object), W::objectClassOffset, metaAddress);
        }
        store(owner.data(), W::objectIndexOffset, std::int32_t{0});
        store(provider.data(), W::objectIndexOffset, std::int32_t{1});
        store(items.data(), 0, root);
        store(items.data(), U::objectItemBytes, providerAddress);
        store(owner.data(), P::ProviderOffset, providerAddress);
        store(provider.data(), P::ProviderRowsOffset, rowsAddress);
        store(provider.data(), P::ProviderCountOffset, std::int32_t{4});
        constexpr std::uintptr_t modelTableRva = 0x431bf00;
        const auto               modelTable = fixture.image + modelTableRva;
        const auto               method = fixture.image + NativeClockProfile::hookEntryRva;
        store(model.data(), 0, modelTable);
        admit(modelTableRva + P::TypeMethodSlot, &method, sizeof(method), PAGE_READONLY);
        admit(modelTableRva + P::AnimationMethodSlot, &method, sizeof(method), PAGE_READONLY);
        std::vector<std::uintptr_t> bank(NativeModelBankProfile::ModelCount);
        bank[3] = modelAddress;
        std::vector<unsigned char> streaming(L::ArrayBytes);
        admit(NativeModelBankProfile::BankRva, bank.data(), NativeModelBankProfile::BankBytes, PAGE_READWRITE);
        admit(L::ArrayRva, streaming.data(), streaming.size(), PAGE_READWRITE);
        const auto chunkPointer = reinterpret_cast<std::uintptr_t>(chunks.data());
        admit(U::objectChunksRva, &chunkPointer, sizeof(chunkPointer), PAGE_READWRITE);
        const std::int32_t objectCount = 2;
        admit(U::objectCountRva, &objectCount, sizeof(objectCount), PAGE_READWRITE);
        admit(P::RootRva, &root, sizeof(root), PAGE_READWRITE);
        const std::uint32_t state = 9, marker = 0;
        const auto          thread = GetCurrentThreadId();
        const unsigned char initialized = 1;
        admit(NativeGameStateProfile::stateRva, &state, sizeof(state), PAGE_READWRITE);
        admit(NativeStartGameProfile::ThreadInitializedRva, &initialized, 1, PAGE_READWRITE);
        admit(NativeStartGameProfile::ThreadIdRva, &thread, sizeof(thread), PAGE_READWRITE);
        admit(NativeBorrowedGcProfile::markerRva, &marker, sizeof(marker), PAGE_READWRITE);
        const auto row = L::ArrayRva + 3 * L::RowStride;
        const auto seedRow = [&](unsigned char flags, unsigned char status)
        {
            fixture.Put(row + L::FlagsOffset, flags);
            fixture.Put(row + L::StateOffset, status);
        };
        ClockReaderTestHost::image = ClockStreamingRequestOwnerTest::image = fixture.image;
        ClockStreamingRequestOwnerTest::root = root;
        ClockStreamingRequestOwnerTest::calls = 0;
        ClockStreamingRequestOwnerTest::result = ClockResult::Ok;
        ClockStreamingRequestOwnerTest::changeRoot = false;
        StreamingRequestOwnerContext context;
        DefaultPedIntelligenceOwner  absent;
        Check(absent.RequestStreamingModel(3, 6) == ClockResult::NotInitialized && runtime.access.RequestStreamingModel(3, 6) == ClockResult::NotInitialized &&
                  !ClockStreamingRequestOwnerTest::calls,
              "default/unbound streaming writers refuse without a fabricated request or native call");
        const auto retire = [&](NativeClockService& service)
        {
            const auto calls = ClockStreamingRequestOwnerTest::calls;
            if (!service.End())
            {
                fixture.invokeTimer();
                Check(service.End(), "natural callback retires the existing request runtime");
            }
            Check(!service.IsRetained() && !runtime.timingHandler && !NativeServiceAdmission::IsClaimed(NativeServiceAdmission::Category::Clock) &&
                      ClockStreamingRequestOwnerTest::calls == calls,
                  "request cleanup releases only the existing owner and never replays accepted loading intent");
        };
        {
            NativeClockService service;
            BeginOwned(service, fixture, false, true, true, &StreamingRequestOwnerHandler, &context);
            Check(runtime.access.RequestStreamingModel(3, 6) != ClockResult::Ok && !ClockStreamingRequestOwnerTest::calls,
                  "producer knowledge of a model key or live native graph grants no request callback authority");
            ClockResult foreign = ClockResult::Ok;
            std::thread other([&] { foreign = runtime.access.RequestStreamingModel(3, 6); });
            other.join();
            Check(foreign != ClockResult::Ok && !ClockStreamingRequestOwnerTest::calls, "foreign threads cannot request models through this owner");
            const auto invoke = [&]
            {
                Check(service.Renew(runtime.generation.load()), "renew one native owner between explicit model requests");
                fixture.invokeTimer();
            };
            // The full SDK request domain now includes resources after the
            // DFF bank; only physical list sentinels and larger keys are invalid.
            for (const auto invalid : {NativeStreamingResourceProfile::ResourceEnd, NativeStreamingResourceProfile::ResourceEnd + 3, std::uint32_t{0xffff},
                                       std::uint32_t{0x10000}, std::uint32_t{0xffffffff}})
            {
                context.index = invalid;
                invoke();
                Check(context.result == ClockResult::InvalidArgument && !ClockStreamingRequestOwnerTest::calls && !runtime.stopping.load(),
                      "invalid full SDK keys refuse before native entry without consuming this owner");
            }
            for (const auto upgrade : {std::uint32_t{1000}, std::uint32_t{1193}})
            {
                context.index = upgrade;
                invoke();
                Check(context.result == ClockResult::UnsupportedBuild && !ClockStreamingRequestOwnerTest::calls && !runtime.stopping.load(),
                      "the exact MTA upgrade dispatch range cannot be replaced by an ordinary native request");
            }
            context.index = 3;
            fixture.Put(NativeStartGameProfile::ThreadIdRva, static_cast<DWORD>(thread + 1));
            invoke();
            Check(context.result != ClockResult::Ok && !ClockStreamingRequestOwnerTest::calls && !runtime.stopping.load(),
                  "real native GameThread identity is required before entry");
            fixture.Put(NativeStartGameProfile::ThreadIdRva, thread);
            fixture.Put(NativeBorrowedGcProfile::markerRva, std::uint32_t{1});
            invoke();
            Check(context.result != ClockResult::Ok && !ClockStreamingRequestOwnerTest::calls && !runtime.stopping.load(),
                  "native GC marker refuses model requests before entry");
            fixture.Put(NativeBorrowedGcProfile::markerRva, marker);
            fixture.Put(NativeGameStateProfile::stateRva, std::uint32_t{7});
            invoke();
            Check(context.result != ClockResult::Ok && !ClockStreamingRequestOwnerTest::calls && !runtime.stopping.load(),
                  "native non-playing state cannot authorize a streaming request");
            fixture.Put(NativeGameStateProfile::stateRva, state);
            store(provider.data(), P::ProviderCountOffset, std::int32_t{3});
            invoke();
            Check(context.result != ClockResult::Ok && !ClockStreamingRequestOwnerTest::calls && !runtime.stopping.load(),
                  "actual provider count, rather than capacity or global model limit, bounds the selected native row");
            store(provider.data(), P::ProviderCountOffset, std::int32_t{4});
            store(items.data(), U::itemFlagsOffset, std::uint32_t{0x30000000});
            invoke();
            Check(context.result != ClockResult::Ok && !ClockStreamingRequestOwnerTest::calls && !runtime.stopping.load(),
                  "native root registry retirement flags refuse before the request entry");
            store(items.data(), U::itemFlagsOffset, std::uint32_t{0});
            seedRow(0, 0xff);
            invoke();
            Check(context.result != ClockResult::Ok && !ClockStreamingRequestOwnerTest::calls && !runtime.stopping.load(),
                  "unknown raw stream states remain observable but cannot authorize the request operation");
            seedRow(0, 0);
            ClockStreamingRequestOwnerTest::reenterScene = +[]() noexcept
            {
                const CVector position(1, 2, 3);
                return runtime.access.LoadStreamingScene(&position);
            };
            const auto sceneCalls = ClockStreamingSceneOwnerTest::calls;
            invoke();
            Check(ClockStreamingRequestOwnerTest::nestedScene == ClockResult::Busy && ClockStreamingSceneOwnerTest::calls == sceneCalls,
                  "same native Request scope refuses recursive adapter scene completion before native entry");
            ClockStreamingRequestOwnerTest::reenterScene = nullptr;
            Check(
                context.result == ClockResult::Ok && ClockStreamingRequestOwnerTest::calls == 1 && ClockStreamingRequestOwnerTest::entry == P::RequestRva &&
                    ClockStreamingRequestOwnerTest::modelIndex == 3 && ClockStreamingRequestOwnerTest::flags == 6 &&
                    fixture.Get<unsigned char>(row + L::FlagsOffset) == 6 && fixture.Get<unsigned char>(row + L::StateOffset) == 2 && !runtime.stopping.load(),
                "actual native callback enters the exact ordinary-model request once and acknowledges asynchronous requested state without inventing loaded1");
            context.flags = 0x16;
            invoke();
            Check(context.result == ClockResult::Ok && ClockStreamingRequestOwnerTest::calls == 2 && ClockStreamingRequestOwnerTest::flags == 0x16 &&
                      fixture.Get<unsigned char>(row + L::StateOffset) == 2 && !runtime.stopping.load(),
                  "a later explicit SDK request is separate accepted intent rather than an automatic cleanup retry");
            retire(service);
            Check(fixture.Get<unsigned char>(row + L::StateOffset) == 2, "normal End preserves native queue/loading effects");
        }
        {
            seedRow(0, 0);
            context.flags = 6;
            NativeClockService service;
            BeginOwned(service, fixture, false, true, true, &StreamingRequestOwnerHandler, &context);
            const auto calls = ClockStreamingRequestOwnerTest::calls;
            ClockStreamingRequestOwnerTest::result = ClockResult::OriginalException;
            fixture.invokeTimer();
            Check(context.result == ClockResult::OriginalException && runtime.stopping.load() && ClockStreamingRequestOwnerTest::calls == calls + 1 &&
                      fixture.Get<unsigned char>(row + L::StateOffset) == 2,
                  "entered native request failure stops the existing owner while preserving already accepted row/queue effects");
            retire(service);
            Check(ClockStreamingRequestOwnerTest::calls == calls + 1 && fixture.Get<unsigned char>(row + L::StateOffset) == 2,
                  "failure cleanup never replays the request or restores its pre-call streaming bytes");
            ClockStreamingRequestOwnerTest::result = ClockResult::Ok;
        }
        {
            seedRow(0, 0);
            NativeClockService service;
            BeginOwned(service, fixture, false, true, true, &StreamingRequestOwnerHandler, &context);
            const auto calls = ClockStreamingRequestOwnerTest::calls;
            fixture.invokeTimer();
            Check(context.result == ClockResult::Ok && ClockStreamingRequestOwnerTest::calls == calls + 1 && !runtime.stopping.load(),
                  "request gate releases after prior entered exception and permits later deliberate native admission");
            retire(service);
        }
        {
            seedRow(0, 0);
            NativeClockService service;
            BeginOwned(service, fixture, false, true, true, &StreamingRequestOwnerHandler, &context);
            const auto calls = ClockStreamingRequestOwnerTest::calls;
            ClockStreamingRequestOwnerTest::replacementRoot = 0;
            ClockStreamingRequestOwnerTest::changeRoot = true;
            fixture.invokeTimer();
            Check(context.result != ClockResult::Ok && runtime.stopping.load() && ClockStreamingRequestOwnerTest::calls == calls + 1 &&
                      fixture.Get<unsigned char>(row + L::StateOffset) == 2 && fixture.Get<std::uintptr_t>(P::RootRva) == 0,
                  "post-entry native root loss prevents final acknowledgement and stops the owner without rolling back accepted asynchronous effects");
            retire(service);
            Check(ClockStreamingRequestOwnerTest::calls == calls + 1 && fixture.Get<unsigned char>(row + L::StateOffset) == 2,
                  "retirement after root loss never retries the request or manufactures restoration authority");
            fixture.Put(P::RootRva, root);
        }
        ClockStreamingRequestOwnerTest::image = ClockStreamingRequestOwnerTest::root = ClockStreamingRequestOwnerTest::replacementRoot = 0;
        ClockStreamingRequestOwnerTest::changeRoot = false;
        ClockReaderTestHost::image = 0;
        for (auto item = saved.rbegin(); item != saved.rend(); ++item)
            fixture.Patch(item->rva, item->bytes.data(), item->bytes.size());
        std::puts("Production native streaming request callback owner fixture completed");
    }
}
