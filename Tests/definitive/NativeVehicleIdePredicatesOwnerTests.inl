/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual seven named policies and four original classifiers on one current owner
 ****************************************************************************/
namespace ClockNamedVehiclePredicateOwnerTest
{
    bool           active = false, copyFailure = false;
    std::uintptr_t model = 0;
    unsigned       fieldReads = 0;
    void (*afterField)() noexcept = nullptr;
    BOOL WINAPI Read(HANDLE process, LPCVOID source, LPVOID output, SIZE_T bytes, SIZE_T* copied) noexcept
    {
        const auto result = ::ReadProcessMemory(process, source, output, bytes, copied);
        if (result && active && reinterpret_cast<std::uintptr_t>(source) == model + 0x84 && bytes == 4)
        {
            ++fieldReads;
            if (copyFailure)
                return FALSE;
            if (const auto after = afterField)
            {
                afterField = nullptr;
                after();
            }
        }
        return result;
    }
}
namespace VehicleIdePredicatesOwnerFixture
{
    namespace Named = NativeModelVehicleNamedProfile;
    namespace Host = ClockNamedVehiclePredicateOwnerTest;
    namespace Bytes = ClockPrivateAnimationAssociationTest;
    std::uintptr_t image = 0, modelPointer = 0, providerPointer = 0, bankSlot = 0, typeLeaf = 0;
    unsigned       mutation = 0;
    void           Changed() noexcept
    {
        if (mutation == 1)
            Bytes::Store(modelPointer + 0x84, Bytes::Load<std::uint32_t>(modelPointer + 0x84) ^ 0x100u);
        if (mutation == 2)
            Bytes::Store(bankSlot, std::uintptr_t{0});
        if (mutation == 3)
            Bytes::Store(image + NativeStreamingRequestProfile::RootRva, std::uintptr_t{0});
        if (mutation == 4)
            Bytes::Store(providerPointer + NativeStreamingRequestProfile::ProviderCountOffset, std::int32_t{1});
        if (mutation == 5)
            Bytes::Store(image + NativeBorrowedGcProfile::markerRva, LONG{1});
        if (mutation == 6)
            Bytes::Store(image + NativeStartGameProfile::ThreadIdRva, DWORD{0});
    }
    bool Query(CModelInfo& model, NativeModelVehiclePredicate predicate)
    {
        switch (predicate)
        {
            case NativeModelVehiclePredicate::Boat:
                return model.IsBoat();
            case NativeModelVehiclePredicate::Car:
                return model.IsCar();
            case NativeModelVehiclePredicate::Plane:
                return model.IsPlane();
            case NativeModelVehiclePredicate::Heli:
                return model.IsHeli();
            case NativeModelVehiclePredicate::Train:
                return model.IsTrain();
            case NativeModelVehiclePredicate::Bike:
                return model.IsBike();
            case NativeModelVehiclePredicate::Bmx:
                return model.IsBmx();
            case NativeModelVehiclePredicate::Trailer:
                return model.IsTrailer();
            case NativeModelVehiclePredicate::MonsterTruck:
                return model.IsMonsterTruck();
            case NativeModelVehiclePredicate::QuadBike:
                return model.IsQuadBike();
            case NativeModelVehiclePredicate::FakePlane:
                return model.IsFakePlane();
        }
        throw std::runtime_error("unknown named vehicle owner fixture predicate");
    }
    void Run(ClockImageFixture& fixture)
    {
        const auto priorReader = ClockReaderTestHost::image;
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
        alignas(std::uintptr_t) std::array<unsigned char, 0x100>                                         model{};
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
        alignas(std::uintptr_t) std::array<unsigned char, 0x100> companionModel{};
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
        ClockReaderTestHost::image = fixture.image;
        for (const auto& pin : NativeModelClassificationProfile::Code)
            admit(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : Named::Pins)
            admit(pin.rva, pin.bytes, pin.size, pin.rva == Named::ProducerRva ? PAGE_EXECUTE_READ : PAGE_READONLY);
        constexpr std::uintptr_t           leafRva = 0x1100030;
        const auto                         leaf = fixture.image + leafRva;
        const std::array<unsigned char, 6> pure{0xb8, 6, 0, 0, 0, 0xc3};
        admit(leafRva, pure.data(), pure.size(), PAGE_EXECUTE_READ);
        admit(modelTableRva + P::TypeMethodSlot, &leaf, sizeof(leaf), PAGE_READONLY);
        image = fixture.image;
        modelPointer = modelAddress;
        providerPointer = providerAddress;
        bankSlot = image + NativeModelBankProfile::BankRva + 3 * sizeof(std::uintptr_t);
        typeLeaf = leaf;
        Host::model = modelAddress;
        Host::active = true;
        Host::wholeOriginal = false;
        ClockModelClassificationOwnerTest::image = image;
        ClockModelClassificationOwnerTest::scenario = 0;
        ClockModelClassificationOwnerTest::calls = 0;
        WorldGroundOwnerFixture::Callback callback;
        callback.action = [](NativeGameTimingAccess&) {};
        NativeClockService service;
        BeginOwned(service, fixture, false, true, true, &WorldGroundOwnerFixture::Callback::Invoke, &callback);
        NamedVehiclePredicateObserver observer(3, runtime.access);
        CModelInfo&                   sdk = observer;
        unsigned                      handled = 0;
        const auto                    action = [&](const auto& operation)
        {
            const auto before = handled;
            callback.action = [&](NativeGameTimingAccess&)
            {
                ++handled;
                operation();
            };
            Check(service.Renew(runtime.generation.load()), "renew exact current named predicate callback");
            fixture.invokeTimer();
            callback.action = [](NativeGameTimingAccess&) {};
            Check(handled == before + 1 && !runtime.stopping.load(), "one actual callback completes and leaves read-only owner usable");
        };
        const auto restore = [&]
        {
            Bytes::Store(bankSlot, modelAddress);
            Bytes::Store(image + P::RootRva, root);
            Bytes::Store(providerAddress + P::ProviderCountOffset, std::int32_t{1002});
            Bytes::Store(image + NativeBorrowedGcProfile::markerRva, LONG{0});
            Bytes::Store(image + NativeStartGameProfile::ThreadIdRva, GetCurrentThreadId());
            Bytes::Store(image + NativeGameStateProfile::stateRva, std::uint32_t{9});
            fixture.Patch(leafRva, pure.data(), pure.size());
            Host::afterField = nullptr;
            Host::copyFailure = false;
        };
        const auto snapshot = [&]
        {
            std::array<unsigned char, 0x100 + 8 + 16> result{};
            std::memcpy(result.data(), model.data(), model.size());
            std::memcpy(result.data() + 0x100, reinterpret_cast<void*>(bankSlot), 8);
            std::memcpy(result.data() + 0x108, reinterpret_cast<void*>(image + L::ArrayRva + 3 * L::RowStride), 16);
            return result;
        };
        const auto priorCalls = ClockModelClassificationOwnerTest::calls;
        for (const auto& entry : Named::Entries)
            for (const unsigned char loaded : {static_cast<unsigned char>(0), static_cast<unsigned char>(1)})
                for (const auto subtype : std::array<std::uint32_t, 14>{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, entry.subtype | 0x100u, 0xffffffffu})
                {
                    store(model.data(), 0x84, subtype);
                    fixture.Put(L::ArrayRva + 3 * L::RowStride + L::StateOffset, loaded);
                    const auto before = snapshot();
                    Host::fieldReads = 0;
                    action([&]
                           { Check(Query(sdk, entry.predicate) == (subtype == entry.subtype), "actual named SDK compares fullDWORD on actual vehicle AL6"); });
                    Check(snapshot() == before && Host::fieldReads == 2 && ClockModelClassificationOwnerTest::calls == priorCalls,
                          "seven named policies preserve native bytes and loaded state without invoking a native classifier or truncating subtype");
                }
        for (const auto& entry : Named::Entries)
        {
            for (const unsigned char type :
                 {static_cast<unsigned char>(0), static_cast<unsigned char>(1), static_cast<unsigned char>(3), static_cast<unsigned char>(5),
                  static_cast<unsigned char>(7), static_cast<unsigned char>(8), static_cast<unsigned char>(255)})
            {
                const std::array<unsigned char, 6> code{0xb8, type, 0, 0, 0, 0xc3};
                fixture.Patch(leafRva, code.data(), code.size());
                Host::fieldReads = 0;
                action([&] { Check(!Query(sdk, entry.predicate), "actual nonvehicle native AL is an admitted false named predicate"); });
                Check(!Host::fieldReads, "nonvehicle policy never reads a vehicle subtype field");
            }
            restore();
            Bytes::Store(bankSlot, std::uintptr_t{0});
            Host::fieldReads = 0;
            action([&] { Check(!Query(sdk, entry.predicate), "a current canonical null bank slot is an admitted false"); });
            Check(!Host::fieldReads, "null policy does not read metadata type or subtype");
            restore();
        }
        action(
            [&]
            {
                NativeModelClassificationCalls::Receipt receipt;
                bool                                    output = true;
                Check(NativeModelClassificationCalls::Read(image, 3, NativeModelVehiclePredicate::Train, &CheckTimingOwner, output, &receipt) ==
                              ClockResult::Ok &&
                          receipt.policyPort && !receipt.entered && !receipt.returned && !receipt.entryRva,
                      "policy receipt explicitly distinguishes copied native facts from an executed DE entry");
            });
        std::puts("Production named vehicle predicates actual SDK owner fixture completed");
        std::puts("Production named vehicle predicates raw DWORD null nonvehicle effects fixture completed");
        // Existing four still execute their complete genuine67-byte bodies.
        Host::wholeOriginal = true;
        for (const auto& entry : NativeModelClassificationProfile::Entries)
        {
            const auto target = entry.predicate == NativeModelVehiclePredicate::Boat    ? 5u
                                : entry.predicate == NativeModelVehiclePredicate::Car   ? 0u
                                : entry.predicate == NativeModelVehiclePredicate::Plane ? 4u
                                                                                        : 3u;
            for (const auto subtype : std::array<std::uint32_t, 3>{target, target | 0x100u, 0xffffffffu})
            {
                store(model.data(), 0x84, subtype);
                const auto before = ClockModelClassificationOwnerTest::calls;
                action(
                    [&]
                    {
                        Check(Query(sdk, entry.predicate) == (subtype == target),
                              "existing four actual SDK queries execute complete original classifier results");
                    });
                Check(ClockModelClassificationOwnerTest::calls == before + 1 && ClockModelClassificationOwnerTest::entry == entry.rva,
                      "existing named original entry remains unchanged and executes once");
            }
        }
        // Pinning the named grammar must not broaden the old four dependency set.
        for (const auto& pin : Named::Pins)
        {
            const auto bad = static_cast<unsigned char>(pin.bytes[0] ^ 1);
            fixture.Patch(pin.rva, &bad, 1);
            bool       preserved = true;
            const auto oldCalls = ClockModelClassificationOwnerTest::calls;
            store(model.data(), 0x84, std::uint32_t{5});
            action(
                [&]
                {
                    Check(runtime.access.ReadModelVehiclePredicate(3, NativeModelVehiclePredicate::Train, preserved) != ClockResult::Ok && preserved,
                          "changed full producer or any earlier type literal refuses a new named result");
                    Check(sdk.IsBoat(), "existing four classifiers do not acquire named parser/literal dependencies");
                });
            Check(ClockModelClassificationOwnerTest::calls == oldCalls + 1, "only the actual old classifier is invoked during named profile refusal");
            fixture.Patch(pin.rva, pin.bytes, 1);
        }
        Host::wholeOriginal = false;
        std::puts("Production existing four native classifier whole originals fixture completed");
        const auto noCalls = ClockModelClassificationOwnerTest::calls;
        for (unsigned changed = 1; changed <= 6; ++changed)
            for (const bool previous : {false, true})
            {
                restore();
                store(model.data(), 0x84, std::uint32_t{6});
                mutation = changed;
                Host::afterField = &Changed;
                action(
                    [&]
                    {
                        bool       value = previous;
                        const auto status = runtime.access.ReadModelVehiclePredicate(3, NativeModelVehiclePredicate::Train, value);
                        restore();
                        Check(status != ClockResult::Ok && value == previous, "lost subtype/bank/provider/root/GC/thread bookend preserves caller result");
                    });
            }
        Host::copyFailure = true;
        action(
            [&]
            {
                bool       preserved = true;
                const auto result = runtime.access.ReadModelVehiclePredicate(3, NativeModelVehiclePredicate::Train, preserved);
                Host::copyFailure = false;
                Check(result == ClockResult::ReadFailure && preserved, "guarded field copy failure remains a refusal");
            });
        const std::array<unsigned char, 6> impure{0x8b, 0x41, 0x20, 0xc3, 0xcc, 0xcc};
        fixture.Patch(leafRva, impure.data(), impure.size());
        action(
            [&]
            {
                bool value = true;
                Check(runtime.access.ReadModelVehiclePredicate(3, NativeModelVehiclePredicate::Bike, value) != ClockResult::Ok && value,
                      "unqualified type getter cannot be invoked or synthesized");
            });
        restore();
        for (const auto index : std::array<std::uint32_t, 4>{20000, 65535, 65536, 0xffffffffu})
            action(
                [&]
                {
                    bool value = true;
                    Check(runtime.access.ReadModelVehiclePredicate(index, NativeModelVehiclePredicate::Train, value) == ClockResult::InvalidArgument && value,
                          "fullDWORD invalid index never aliases the current model row");
                });
        action(
            [&]
            {
                bool value = true;
                Check(
                    runtime.access.ReadModelVehiclePredicate(3, static_cast<NativeModelVehiclePredicate>(255), value) == ClockResult::InvalidArgument && value,
                    "unsupported enum remains default-refusing");
            });
        bool outside = true;
        Check(runtime.access.ReadModelVehiclePredicate(3, NativeModelVehiclePredicate::Train, outside) != ClockResult::Ok && outside,
              "retained runtime outside its current callback lends no classification authority");
        ClockResult foreign = ClockResult::Ok;
        std::thread other(
            [&]
            {
                bool value = true;
                foreign = runtime.access.ReadModelVehiclePredicate(3, NativeModelVehiclePredicate::Train, value);
                if (!value)
                    std::abort();
            });
        other.join();
        Check(foreign != ClockResult::Ok && ClockModelClassificationOwnerTest::calls == noCalls,
              "foreign thread and all named refusals enter no native classifier");
        action(
            [&]
            {
                NativeModelClassificationCalls::Receipt receipt;
                bool                                    value = true;
                Check(NativeModelClassificationCalls::Read(image, 3, NativeModelVehiclePredicate::Bike, nullptr, value, &receipt) ==
                              ClockResult::NotInitialized &&
                          value && !receipt.entered,
                      "missing owner refuses a named policy without an entry");
            });
        std::puts("Production named vehicle predicates source owner bookend refusals fixture completed");
        WorldGroundOwnerFixture::Retire(service, fixture);
        Check(ClockModelClassificationOwnerTest::calls == noCalls, "natural retirement never invokes a named predicate or replays an original");
        Host::active = Host::wholeOriginal = Host::copyFailure = false;
        Host::afterField = nullptr;
        Host::model = 0;
        image = modelPointer = providerPointer = bankSlot = typeLeaf = 0;
        ClockModelClassificationOwnerTest::image = 0;
        ClockReaderTestHost::image = priorReader;
        for (auto item = saved.rbegin(); item != saved.rend(); ++item)
            fixture.Patch(item->rva, item->bytes.data(), item->bytes.size());
    }
}
