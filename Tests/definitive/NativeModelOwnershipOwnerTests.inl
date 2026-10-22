/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Admit native model ownership effects through the actual callback
 ****************************************************************************/
namespace
{
    enum class ModelOwnershipOperation
    {
        Reference,
        UpgradeRequest,
        UpgradeLoaded,
        LoadAll,
        ResourceRequest,
        Classification,
        ModelType,
        Remove
    };
    struct ModelOwnershipOwnerContext
    {
        ModelOwnershipOperation     operation = ModelOwnershipOperation::Reference;
        std::uint32_t               index = 3, flags = 6;
        int                         delta = 1;
        bool                        loaded = true, onlyPriority = false;
        NativeModelVehiclePredicate predicate = NativeModelVehiclePredicate::Boat;
        NativeModelTypeObservation  typeObservation{true, 77};
        ClockResult                 result = ClockResult::NotInitialized;
    };
    void ModelOwnershipOwnerHandler(NativeGameTimingAccess& access, void* opaque)
    {
        auto& context = *static_cast<ModelOwnershipOwnerContext*>(opaque);
        switch (context.operation)
        {
            case ModelOwnershipOperation::Classification:
                context.result = access.ReadModelVehiclePredicate(context.index, context.predicate, context.loaded);
                break;
            case ModelOwnershipOperation::ModelType:
                context.result = access.ReadModelType(context.index, context.typeObservation);
                break;
            case ModelOwnershipOperation::Remove:
                context.result = access.RemoveStreamingModel(context.index);
                break;
            case ModelOwnershipOperation::Reference:
                context.result = access.AdjustModelReference(context.index, context.delta);
                break;
            case ModelOwnershipOperation::UpgradeRequest:
                context.result = access.RequestStreamingUpgrade(context.index, context.flags);
                break;
            case ModelOwnershipOperation::LoadAll:
                context.result = access.LoadStreamingModels(context.onlyPriority);
                break;
            case ModelOwnershipOperation::ResourceRequest:
                context.result = access.RequestStreamingModel(context.index, context.flags);
                break;
            case ModelOwnershipOperation::UpgradeLoaded:
                context.result = access.ReadStreamingUpgradeLoaded(context.index, context.loaded);
                break;
        }
    }
    void NativeModelOwnershipOwnedSdk(ClockImageFixture& fixture)
    {
        namespace P = NativeStreamingRequestProfile;
        namespace L = NativeModelLoadedProfile;
        namespace W = NativeStartupWorldProfile;
        namespace U = NativeStartupProvidersProfile;
        namespace R = NativeModelReferenceProfile;
        namespace G = NativeStreamingUpgradeProfile;
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
        for (const auto& pin : R::Code)
            admit(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : G::Code)
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
        alignas(std::uintptr_t) std::array<unsigned char, R::MapOffset + R::HeaderBytes>                 owner{};
        alignas(std::uintptr_t) std::array<unsigned char, P::ProviderCountOffset + sizeof(std::int32_t)> provider{};
        alignas(std::uintptr_t) std::array<unsigned char, P::ModelPrefixBytes>                           model{};
        alignas(std::uintptr_t) std::array<unsigned char, 0x40>                                          meta{};
        std::vector<unsigned char>                                                                       providerRows(1002 * P::ProviderRowStride);
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
        store(provider.data(), P::ProviderCountOffset, std::int32_t{1002});
        constexpr std::uintptr_t modelTableRva = 0x431bf00;
        const auto               modelTable = fixture.image + modelTableRva;
        const auto               method = fixture.image + NativeClockProfile::hookEntryRva;
        store(model.data(), 0, modelTable);
        alignas(std::uintptr_t) std::array<unsigned char, P::ModelPrefixBytes> companionModel{};
        store(companionModel.data(), 0, modelTable);
        admit(modelTableRva + P::TypeMethodSlot, &method, sizeof(method), PAGE_READONLY);
        admit(modelTableRva + P::AnimationMethodSlot, &method, sizeof(method), PAGE_READONLY);
        std::vector<std::uintptr_t> bank(NativeModelBankProfile::ModelCount);
        bank[3] = bank[1000] = modelAddress;
        bank[1001] = reinterpret_cast<std::uintptr_t>(companionModel.data());
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
        struct alignas(std::uintptr_t) RefRow
        {
            std::uint32_t key = 3, count = 2;
            std::int32_t  next = -1, bucket = 0;
        };
        std::array<RefRow, 4> referenceRows{};
        auto&                 reference = referenceRows[0];
        auto*                 map = owner.data() + R::MapOffset;
        store(map, 0, reinterpret_cast<std::uintptr_t>(&reference));
        store(map, 8, std::int32_t{1});
        store(map, 0xc, std::int32_t{4});
        store(map, 0x10, std::uint32_t{1});
        store(map, 0x28, std::int32_t{1});
        store(map, 0x2c, std::int32_t{128});
        store(map, 0x30, std::int32_t{-1});
        store(map, 0x34, std::int32_t{0});
        store(map, 0x38, std::int32_t{0});
        store(map, 0x48, std::int32_t{1});
        const auto seed = [&](std::uint32_t key, unsigned char status)
        {
            const auto row = L::ArrayRva + key * L::RowStride;
            fixture.Put(row + L::FlagsOffset, static_cast<unsigned char>(0));
            fixture.Put(row + L::StateOffset, status);
        };
        const auto links = [&](std::int32_t count, std::uint16_t selected = 1000, std::uint16_t companion = 1001)
        {
            fixture.Put(G::FirstLinksRva, selected);
            fixture.Put(G::SecondLinksRva, companion);
            fixture.Put(G::LinkCountRva, count);
        };
        // Save all mutated image links, including the short-circuit query's
        // count, before publishing a genuine paired-upgrade fixture.
        const std::array<std::uint16_t, G::LinkBudget> emptyLinks{};
        admit(G::FirstLinksRva, emptyLinks.data(), sizeof(emptyLinks), PAGE_READWRITE);
        admit(G::SecondLinksRva, emptyLinks.data(), sizeof(emptyLinks), PAGE_READWRITE);
        const std::int32_t zero = 0;
        admit(G::LinkCountRva, &zero, sizeof(zero), PAGE_READWRITE);
        links(1);
        seed(3, 1);
        seed(1000, 1);
        seed(1001, 1);
        ClockReaderTestHost::image = ClockModelReferenceOwnerTest::image = ClockStreamingUpgradeOwnerTest::image = fixture.image;
        ClockModelReferenceOwnerTest::calls = ClockStreamingUpgradeOwnerTest::calls = 0;
        ClockModelReferenceOwnerTest::result = ClockStreamingUpgradeOwnerTest::result = ClockResult::Ok;
        ClockModelReferenceOwnerTest::conflict = ClockModelReferenceOwnerTest::loseRoot = ClockStreamingUpgradeOwnerTest::loseRoot = false;
        ClockStreamingUpgradeOwnerTest::companion = 1001;
        ModelOwnershipOwnerContext  context;
        DefaultPedIntelligenceOwner absent;
        bool                        sentinel = true;
        Check(absent.LoadStreamingModels(false) == ClockResult::NotInitialized,
              "older/default owners refuse blocking-all intent rather than report a completed load");
        Check(absent.AdjustModelReference(3, 1) == ClockResult::NotInitialized && absent.RequestStreamingUpgrade(1000, 6) == ClockResult::NotInitialized &&
                  absent.ReadStreamingUpgradeLoaded(1000, sentinel) == ClockResult::NotInitialized && sentinel,
              "default timing owners refuse extra model operations and preserve query output");
        const auto retire = [&](NativeClockService& service)
        {
            const auto refs = ClockModelReferenceOwnerTest::calls, upgrades = ClockStreamingUpgradeOwnerTest::calls;
            if (!service.End())
            {
                fixture.invokeTimer();
                Check(service.End(), "natural callback finishes current ownership retirement");
            }
            Check(!service.IsRetained() && !runtime.timingHandler && !NativeServiceAdmission::IsClaimed(NativeServiceAdmission::Category::Clock) &&
                      ClockModelReferenceOwnerTest::calls == refs && ClockStreamingUpgradeOwnerTest::calls == upgrades,
                  "owner cleanup neither replays CAS nor resubmits a paired native request");
        };
        {
            NativeClockService service;
            BeginOwned(service, fixture, false, true, true, &ModelOwnershipOwnerHandler, &context);
            sentinel = true;
            Check(runtime.access.AdjustModelReference(3, 1) != ClockResult::Ok && runtime.access.RequestStreamingUpgrade(1000, 6) != ClockResult::Ok &&
                      runtime.access.ReadStreamingUpgradeLoaded(1000, sentinel) != ClockResult::Ok && sentinel && !ClockModelReferenceOwnerTest::calls &&
                      !ClockStreamingUpgradeOwnerTest::calls,
                  "a live retained owner still grants no model mutation or query outside its actual callback");
            ClockResult foreign = ClockResult::Ok, foreignUpgrade = ClockResult::Ok, foreignQuery = ClockResult::Ok;
            bool        foreignOutput = true;
            std::thread other(
                [&]
                {
                    foreign = runtime.access.AdjustModelReference(3, 1);
                    foreignUpgrade = runtime.access.RequestStreamingUpgrade(1000, 6);
                    foreignQuery = runtime.access.ReadStreamingUpgradeLoaded(1000, foreignOutput);
                });
            other.join();
            Check(foreign != ClockResult::Ok && foreignUpgrade != ClockResult::Ok && foreignQuery != ClockResult::Ok && foreignOutput &&
                      !ClockModelReferenceOwnerTest::calls && !ClockStreamingUpgradeOwnerTest::calls,
                  "foreign-thread model keys do not borrow callback mutation or query authority and preserve query output");
            const auto invoke = [&]
            {
                Check(service.Renew(runtime.generation.load()), "renew current native ownership callback");
                fixture.invokeTimer();
            };
            context.operation = ModelOwnershipOperation::Reference;
            for (const auto delta : {0, 2, -2})
            {
                context.delta = delta;
                invoke();
                Check(context.result == ClockResult::InvalidArgument && !ClockModelReferenceOwnerTest::calls && !runtime.stopping.load(),
                      "unsupported counter deltas refuse before native memory without consuming this owner");
            }
            context.delta = 1;
            context.index = 4;
            bank[4] = modelAddress;
            fixture.Put(NativeModelBankProfile::BankRva + 4 * sizeof(std::uintptr_t), modelAddress);
            seed(4, 1);
            invoke();
            Check(context.result != ClockResult::Ok && !ClockModelReferenceOwnerTest::calls && reference.count == 2,
                  "a loaded metadata association cannot insert an absent key without the native allocator prerequisites");
            context.index = 3;
            reference.count = 1;
            context.delta = -1;
            invoke();
            Check(context.result == ClockResult::UnsupportedBuild && reference.count == 1 && !ClockModelReferenceOwnerTest::calls,
                  "the final decrement refuses unqualified native removal continuation");
            reference.count = 0;
            invoke();
            Check(context.result == ClockResult::InvalidArgument && !ClockModelReferenceOwnerTest::calls, "underflow refuses before mutation");
            context.delta = 1;
            reference.count = 0xffffffff;
            invoke();
            Check(context.result == ClockResult::InvalidArgument && !ClockModelReferenceOwnerTest::calls, "unsigned maximum refuses wraparound without a CAS");
            reference.count = 0;
            invoke();
            Check(context.result == ClockResult::Ok && reference.count == 1 && ClockModelReferenceOwnerTest::calls == 1,
                  "zero-to-one increments only an already occupied native row and does not manufacture key insertion");
            reference.count = 2;
            seed(3, 2);
            invoke();
            Check(context.result == ClockResult::NotInitialized && ClockModelReferenceOwnerTest::calls == 1,
                  "requested state2 is not a loaded reference-adjustment prerequisite");
            seed(3, 1);
            invoke();
            Check(context.result == ClockResult::Ok && reference.count == 3 && ClockModelReferenceOwnerTest::calls == 2 &&
                      ClockModelReferenceOwnerTest::address == reinterpret_cast<std::uintptr_t>(&reference.count) && !runtime.stopping.load(),
                  "the actual callback reaches one exact native sparse-row CAS without retaining its address");
            context.delta = -1;
            invoke();
            Check(context.result == ClockResult::Ok && reference.count == 2 && ClockModelReferenceOwnerTest::calls == 3,
                  "a positive interior decrement remains distinct from final model removal");
            context.operation = ModelOwnershipOperation::UpgradeLoaded;
            context.index = 1000;
            context.loaded = false;
            links(1);
            seed(1000, 1);
            seed(1001, 1);
            invoke();
            Check(context.result == ClockResult::Ok && context.loaded && !ClockStreamingUpgradeOwnerTest::calls,
                  "the actual owner publishes the genuine selected-plus-companion state1 predicate without a native call");
            seed(1001, 2);
            invoke();
            Check(context.result == ClockResult::Ok && !context.loaded, "a requested companion makes a loaded selected upgrade false");
            seed(1000, 2);
            links(-1);
            context.loaded = true;
            invoke();
            Check(context.result == ClockResult::Ok && !context.loaded, "native selected-notloaded short circuit avoids an irrelevant companion table");
            seed(1000, 1);
            context.loaded = true;
            invoke();
            Check(context.result != ClockResult::Ok && context.loaded && !runtime.stopping.load(),
                  "failed paired query preserves caller output and does not stop this read-only owner");
            links(1);
            seed(1001, 1);
            ClockModelLoadedOwnerTest::address = fixture.image + L::ArrayRva + 1000 * L::RowStride + L::FlagsOffset;
            ClockModelLoadedOwnerTest::advanced = {0, 2};
            ClockModelLoadedOwnerTest::advanceOnRead = true;
            context.loaded = true;
            invoke();
            Check(context.result != ClockResult::Ok && context.loaded && !runtime.stopping.load(),
                  "native selected-row advancement cannot publish an incoherent paired query");
            ClockModelLoadedOwnerTest::address = 0;
            ClockModelLoadedOwnerTest::advanceOnRead = false;
            context.operation = ModelOwnershipOperation::UpgradeRequest;
            for (const auto invalid : {std::uint32_t{999}, std::uint32_t{1194}, std::uint32_t{0x10000}, std::uint32_t{0xffffffff}})
            {
                context.index = invalid;
                invoke();
                Check(context.result == ClockResult::InvalidArgument && !ClockStreamingUpgradeOwnerTest::calls && !runtime.stopping.load(),
                      "full SDK keys outside the actual inclusive upgrade interval refuse without narrowing");
            }
            context.index = 1000;
            context.flags = 0x16;
            seed(1000, 0);
            seed(1001, 0);
            store(provider.data(), P::ProviderCountOffset, std::int32_t{1001});
            invoke();
            Check(context.result != ClockResult::Ok && !ClockStreamingUpgradeOwnerTest::calls && !runtime.stopping.load(),
                  "missing companion provider row refuses before either native request effect");
            store(provider.data(), P::ProviderCountOffset, std::int32_t{1002});
            fixture.Put(NativeStartGameProfile::ThreadIdRva, static_cast<DWORD>(thread + 1));
            invoke();
            Check(context.result != ClockResult::Ok && !ClockStreamingUpgradeOwnerTest::calls,
                  "actual engine GameThread remains mandatory for paired native effects");
            fixture.Put(NativeStartGameProfile::ThreadIdRva, thread);
            invoke();
            Check(context.result == ClockResult::Ok && ClockStreamingUpgradeOwnerTest::calls == 1 && ClockStreamingUpgradeOwnerTest::entry == G::RequestRva &&
                      ClockStreamingUpgradeOwnerTest::modelIndex == 1000 && ClockStreamingUpgradeOwnerTest::flags == 0x16 &&
                      fixture.Get<unsigned char>(L::ArrayRva + 1000 * L::RowStride + L::StateOffset) == 2 &&
                      fixture.Get<unsigned char>(L::ArrayRva + 1001 * L::RowStride + L::StateOffset) == 2 && !runtime.stopping.load(),
                  "one genuine upgrade native entry owns both requested states without claiming either completed");
            retire(service);
            Check(reference.count == 2, "normal End does not undo accepted native model reference effects");
        }
        for (const auto scenario : {0, 1, 2})
        {
            reference.count = 2;
            seed(3, 1);
            fixture.Put(P::RootRva, root);
            context.operation = ModelOwnershipOperation::Reference;
            context.index = 3;
            context.delta = 1;
            ClockModelReferenceOwnerTest::result = scenario == 0 ? ClockResult::OriginalException : ClockResult::Ok;
            ClockModelReferenceOwnerTest::conflict = scenario == 1;
            ClockModelReferenceOwnerTest::loseRoot = scenario == 2;
            NativeClockService service;
            BeginOwned(service, fixture, false, true, true, &ModelOwnershipOwnerHandler, &context);
            const auto calls = ClockModelReferenceOwnerTest::calls;
            fixture.invokeTimer();
            Check(context.result != ClockResult::Ok && runtime.stopping.load() && ClockModelReferenceOwnerTest::calls == calls + 1 &&
                      reference.count == (scenario == 1 ? 9u : 3u),
                  "entered CAS fault, competing count or lost root stops this owner without rewinding external or accepted effects");
            retire(service);
            Check(ClockModelReferenceOwnerTest::calls == calls + 1 && reference.count == (scenario == 1 ? 9u : 3u),
                  "CAS retirement never replays or rolls back a possibly accepted native adjustment");
            ClockModelReferenceOwnerTest::result = ClockResult::Ok;
            fixture.Put(P::RootRva, root);
        }
        for (const bool loseRoot : {false, true})
        {
            links(1);
            seed(1000, 0);
            seed(1001, 0);
            fixture.Put(P::RootRva, root);
            context.operation = ModelOwnershipOperation::UpgradeRequest;
            context.index = 1000;
            context.flags = 6;
            ClockStreamingUpgradeOwnerTest::result = loseRoot ? ClockResult::Ok : ClockResult::OriginalException;
            ClockStreamingUpgradeOwnerTest::loseRoot = loseRoot;
            NativeClockService service;
            BeginOwned(service, fixture, false, true, true, &ModelOwnershipOwnerHandler, &context);
            const auto calls = ClockStreamingUpgradeOwnerTest::calls;
            fixture.invokeTimer();
            Check(context.result != ClockResult::Ok && runtime.stopping.load() && ClockStreamingUpgradeOwnerTest::calls == calls + 1 &&
                      fixture.Get<unsigned char>(L::ArrayRva + 1000 * L::RowStride + L::StateOffset) == 2 &&
                      fixture.Get<unsigned char>(L::ArrayRva + 1001 * L::RowStride + L::StateOffset) == 2,
                  "entered paired-request failure or lost context stops the same owner and preserves both native effects");
            retire(service);
            Check(ClockStreamingUpgradeOwnerTest::calls == calls + 1, "upgrade cleanup cannot replay the whole operation or its companion request");
            fixture.Put(P::RootRva, root);
            ClockStreamingUpgradeOwnerTest::result = ClockResult::Ok;
        }
        // Execute the larger completion/resource/acquisition batch on this
        // same actual callback owner and initialized model context.
#include "NativeStreamingCompletionOwnerTests.inl"
        ClockReaderTestHost::image = ClockModelReferenceOwnerTest::image = ClockStreamingUpgradeOwnerTest::image = 0;
        ClockModelLoadedOwnerTest::address = 0;
        ClockModelLoadedOwnerTest::advanceOnRead = false;
        for (auto item = saved.rbegin(); item != saved.rend(); ++item)
            fixture.Patch(item->rva, item->bytes.data(), item->bytes.size());
        std::puts("Production native model reference callback owner fixture completed");
        std::puts("Production native streaming upgrade callback owner fixture completed");
    }
}
