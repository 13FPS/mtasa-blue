/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual SDK current GetTime under genuine post-original callback ownership
 ****************************************************************************/
namespace ModelTimeGetterOwnerFixture
{
    namespace T = NativeModelTimeProfile;
    namespace Host = ClockModelTimeGetterOwnerTest;
    namespace Bytes = ClockPrivateAnimationAssociationTest;
    ClockImageFixture* activeFixture = nullptr;
    std::uintptr_t     image = 0, modelPointer = 0, bankSlot = 0, providerPointer = 0;
    unsigned           mutation = 0;
    void               Changed() noexcept
    {
        if (mutation == 1)
            Bytes::Store(modelPointer + T::TimeOffset, static_cast<unsigned char>(Bytes::Load<unsigned char>(modelPointer + T::TimeOffset) ^ 1));
        if (mutation == 2)
            Bytes::Store(modelPointer + T::TimeOffset + 1, static_cast<unsigned char>(Bytes::Load<unsigned char>(modelPointer + T::TimeOffset + 1) ^ 1));
        if (mutation == 3)
            Bytes::Store(modelPointer + T::TimeOffset + 2, static_cast<std::uint16_t>(Bytes::Load<std::uint16_t>(modelPointer + T::TimeOffset + 2) ^ 1));
        if (mutation == 4)
            Bytes::Store(image + T::PoolCountRva, std::int32_t{2});
        if (mutation == 5)
            Bytes::Store(bankSlot, std::uintptr_t{0});
        if (mutation == 6)
            Bytes::Store(image + NativeStreamingRequestProfile::RootRva, std::uintptr_t{0});
        if (mutation == 7)
            Bytes::Store(providerPointer + NativeStreamingRequestProfile::ProviderCountOffset, std::int32_t{1});
        if (mutation == 8)
            Bytes::Store(image + NativeBorrowedGcProfile::markerRva, LONG{1});
        if (mutation == 9)
            Bytes::Store(image + NativeStartGameProfile::ThreadIdRva, DWORD{0});
        // Immutable vtable/code pages require the fixture's protected patch
        // boundary. Its ordinary data helper intentionally cannot write them.
        if (mutation == 10)
        {
            const auto method = image + T::TypeMethodRva;
            activeFixture->Patch(T::VtableRva + T::TimeSlot, &method, sizeof(method));
        }
        const unsigned char bad = 0x90;
        if (mutation == 11)
            activeFixture->Patch(T::TypeMethodRva, &bad, sizeof(bad));
        if (mutation == 12)
            activeFixture->Patch(T::ProducerRva, &bad, sizeof(bad));
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
        const auto modelAddress = fixture.image + T::PoolRva;
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

        const auto                 privateModel = reinterpret_cast<std::uintptr_t>(model.data());
        std::vector<unsigned char> pool(T::PoolCapacity * T::RowBytes, 0x5a);
        for (std::size_t i = 0; i != T::PoolCapacity; ++i)
            store(pool.data(), i * T::RowBytes, fixture.image + T::VtableRva);
        admit(T::PoolRva, pool.data(), pool.size(), PAGE_READWRITE);
        const std::int32_t one = 1;
        admit(T::PoolCountRva, &one, sizeof(one), PAGE_READWRITE);
        std::array<std::uintptr_t, 16> timeTable{};
        for (std::size_t i = 0; i != std::size(T::VtableMethods); ++i)
            timeTable[i] = fixture.image + T::VtableMethods[i];
        timeTable[15] = fixture.image + 0xace850;
        admit(T::VtableRva, timeTable.data(), sizeof(timeTable), PAGE_READONLY);
        for (const auto& pin : T::Pins)
            admit(pin.rva, pin.bytes, pin.size, pin.code ? PAGE_EXECUTE_READ : PAGE_READONLY);
        const std::array<unsigned char, 6> timedType{0xb0, 3, 0xc3, 0xcc, 0xcc, 0xcc};
        admit(T::TypeMethodRva, timedType.data(), timedType.size(), PAGE_EXECUTE_READ);
        const unsigned char padding = 0xcc;
        admit(0xace850, &padding, 1, PAGE_EXECUTE_READ);
        constexpr std::uintptr_t           leafRva = 0x1100030;
        const std::array<unsigned char, 6> nonTime{0xb8, 6, 0, 0, 0, 0xc3};
        admit(leafRva, nonTime.data(), nonTime.size(), PAGE_EXECUTE_READ);
        const auto privateLeaf = fixture.image + leafRva;
        admit(modelTableRva + P::TypeMethodSlot, &privateLeaf, sizeof(privateLeaf), PAGE_READONLY);
        activeFixture = &fixture;
        image = fixture.image;
        modelPointer = modelAddress;
        providerPointer = providerAddress;
        bankSlot = image + NativeModelBankProfile::BankRva + 3 * sizeof(std::uintptr_t);
        Host::active = true;
        Host::model = modelAddress;
        Host::afterTuple = nullptr;
        Host::copyFailure = false;
        Host::tupleReads = 0;
        WorldGroundOwnerFixture::Callback callback;
        callback.action = [](NativeGameTimingAccess&) {};
        NativeClockService service;
        BeginOwned(service, fixture, false, true, true, &WorldGroundOwnerFixture::Callback::Invoke, &callback);
        ModelTimeObserver observer(3, runtime.access);
        CModelInfo&       sdk = observer;
        unsigned          handled = 0;
        const auto        action = [&](const auto& operation)
        {
            const auto before = handled;
            callback.action = [&](NativeGameTimingAccess&)
            {
                ++handled;
                operation();
            };
            Check(service.Renew(runtime.generation.load()), "renew actual retained current time callback");
            fixture.invokeTimer();
            callback.action = [](NativeGameTimingAccess&) {};
            Check(handled == before + 1 && !runtime.stopping.load(), "time read completes once and leaves the actual owner usable");
        };
        const auto restore = [&]
        {
            modelPointer = modelAddress;
            Host::model = modelAddress;
            Bytes::Store(bankSlot, modelAddress);
            Bytes::Store(image + T::PoolCountRva, std::int32_t{1});
            Bytes::Store(image + P::RootRva, root);
            Bytes::Store(providerAddress + P::ProviderCountOffset, std::int32_t{1002});
            Bytes::Store(image + NativeBorrowedGcProfile::markerRva, LONG{0});
            Bytes::Store(image + NativeStartGameProfile::ThreadIdRva, GetCurrentThreadId());
            Bytes::Store(image + NativeGameStateProfile::stateRva, std::uint32_t{9});
            fixture.Patch(T::VtableRva, timeTable.data(), sizeof(timeTable));
            for (const auto& pin : T::Pins)
                fixture.Patch(pin.rva, pin.bytes, pin.size);
            fixture.Patch(T::TypeMethodRva, timedType.data(), timedType.size());
            fixture.Patch(leafRva, nonTime.data(), nonTime.size());
            Bytes::Store(modelAddress, image + T::VtableRva);
            Bytes::Store(privateModel, modelTable);
            Host::copyFailure = false;
            Host::afterTuple = nullptr;
            Host::tupleReads = 0;
        };
        const auto bits = [](char value)
        {
            unsigned char output = 0;
            std::memcpy(&output, &value, 1);
            return output;
        };
        const auto snapshot = [&]
        {
            std::vector<unsigned char> output(T::PoolCapacity * T::RowBytes + NativeModelBankProfile::BankBytes + 4 + L::RowStride);
            std::memcpy(output.data(), reinterpret_cast<void*>(image + T::PoolRva), T::PoolCapacity * T::RowBytes);
            std::memcpy(output.data() + T::PoolCapacity * T::RowBytes, reinterpret_cast<void*>(image + NativeModelBankProfile::BankRva),
                        NativeModelBankProfile::BankBytes);
            std::memcpy(output.data() + T::PoolCapacity * T::RowBytes + NativeModelBankProfile::BankBytes, reinterpret_cast<void*>(image + T::PoolCountRva), 4);
            std::memcpy(output.data() + T::PoolCapacity * T::RowBytes + NativeModelBankProfile::BankBytes + 4,
                        reinterpret_cast<void*>(image + L::ArrayRva + 3 * L::RowStride), L::RowStride);
            return output;
        };
        const auto                             nativeCalls = ClockModelClassificationOwnerTest::calls;
        constexpr std::array<unsigned char, 8> values{0, 1, 6, 20, 24, 0x7f, 0x80, 0xff};
        for (const auto row : std::array<std::size_t, 2>{0, T::PoolCapacity - 1})
        {
            const auto address = image + T::PoolRva + row * T::RowBytes;
            Bytes::Store(bankSlot, address);
            Bytes::Store(image + T::PoolCountRva, static_cast<std::int32_t>(row + 1));
            Host::model = address;
            for (const auto on : values)
                for (const auto off : values)
                {
                    const std::array<unsigned char, 4> tuple{on, off, 0xef, 0xbe};
                    std::memcpy(reinterpret_cast<void*>(address + T::TimeOffset), tuple.data(), tuple.size());
                    fixture.Put(L::ArrayRva + 3 * L::RowStride + L::StateOffset, static_cast<unsigned char>(on & 1));
                    const auto before = snapshot();
                    Host::tupleReads = 0;
                    action(
                        [&]
                        {
                            char a = 37, b = 71;
                            Check(sdk.GetTime(a, b) && bits(a) == on && bits(b) == off, "actual SDK observes raw On/Off from first or last admitted timed row");
                        });
                    Check(snapshot() == before && Host::tupleReads == 2, "getter preserves complete pool/bank/count and pairedWORD without any native effect");
                    action(
                        [&]
                        {
                            char alias = 99;
                            Check(sdk.GetTime(alias, alias) && bits(alias) == off, "actual SDK publishes On then Off for aliased ordinary outputs");
                        });
                }
        }
        restore();
        for (const unsigned char type : values)
        {
            if (type == 3)
                continue;
            const std::array<unsigned char, 6> code{0xb8, type, 0, 0, 0, 0xc3};
            fixture.Patch(leafRva, code.data(), code.size());
            Bytes::Store(bankSlot, privateModel);
            Host::tupleReads = 0;
            action(
                [&]
                {
                    char a = 37, b = 71;
                    Check(!sdk.GetTime(a, b) && a == 37 && b == 71, "admitted nonTIME returns false and preserves both outputs");
                });
            Check(!Host::tupleReads, "nonTIME never touches time metadata");
        }
        restore();
        Bytes::Store(bankSlot, std::uintptr_t{0});
        action(
            [&]
            {
                char a = 37, b = 71;
                Check(!sdk.GetTime(a, b) && a == 37 && b == 71, "current null model returns false without changing either output");
            });
        Check(!Host::tupleReads, "null model never reads type or time metadata");
        restore();
        std::puts("Production model time getter actual SDK owner fixture completed");
        const auto refusal = [&](std::uint32_t key = 3)
        {
            NativeModelTimeObservation output{true, 0x91, 0xe7};
            const auto                 result = runtime.access.ReadModelTime(key, output);
            Check(result != ClockResult::Ok && output.hasTime && output.on == 0x91 && output.off == 0xe7,
                  "time owner refusal preserves the complete typed tuple");
            char a = 37, b = 71;
            bool threw = false;
            try
            {
                observer.GetTime(a, b);
            }
            catch (const std::runtime_error&)
            {
                threw = true;
            }
            // Invalid-index tests use the typed route; the fixed SDK observer
            // still has key3 and may succeed. Other failures must propagate.
            if (key == 3)
                Check(threw && a == 37 && b == 71, "actual SDK refusal preserves both caller outputs");
        };
        for (const auto count : std::array<std::int32_t, 4>{0, -1, 170, 0x7fffffff})
        {
            Bytes::Store(image + T::PoolCountRva, count);
            action([&] { refusal(); });
            restore();
        }
        for (const auto pointer : std::array<std::uintptr_t, 4>{modelAddress + 8, modelAddress + T::PoolCapacity * T::RowBytes, privateModel, modelAddress - 8})
        {
            if (pointer == privateModel)
                Bytes::Store(privateModel, image + T::VtableRva);
            Bytes::Store(bankSlot, pointer);
            action([&] { refusal(); });
            restore();
        }
        // Native AL3 alone never widens a classic time-info field offset.
        const std::array<unsigned char, 6> unknownTime{0xb8, 3, 0, 0, 0, 0xc3};
        fixture.Patch(leafRva, unknownTime.data(), unknownTime.size());
        Bytes::Store(bankSlot, privateModel);
        action([&] { refusal(); });
        restore();
        for (const auto& pin : T::Pins)
        {
            const auto bad = static_cast<unsigned char>(pin.bytes[0] ^ 1);
            fixture.Patch(pin.rva, &bad, 1);
            action([&] { refusal(); });
            restore();
        }
        for (std::size_t slot = 0; slot != std::size(T::VtableMethods); ++slot)
        {
            const auto wrongMethod = image + T::TimeMethodRva + 1;
            fixture.Patch(T::VtableRva + slot * 8, &wrongMethod, sizeof(wrongMethod));
            action([&] { refusal(); });
            restore();
        }
        const auto dos = Bytes::Load<IMAGE_DOS_HEADER>(image);
        const auto nt = Bytes::Load<IMAGE_NT_HEADERS64>(image + static_cast<std::uintptr_t>(dos.e_lfanew));
        const auto sectionTable = static_cast<std::uintptr_t>(dos.e_lfanew) + offsetof(IMAGE_NT_HEADERS64, OptionalHeader) + nt.FileHeader.SizeOfOptionalHeader;
        for (const auto target : std::array<std::uintptr_t, 2>{T::ConstructorRva, T::PoolRva})
            for (std::size_t index = 0; index != nt.FileHeader.NumberOfSections; ++index)
            {
                const auto at = sectionTable + index * sizeof(IMAGE_SECTION_HEADER);
                const auto section = Bytes::Load<IMAGE_SECTION_HEADER>(image + at);
                if (target < section.VirtualAddress || target - section.VirtualAddress >= section.Misc.VirtualSize)
                    continue;
                auto changed = section;
                changed.Characteristics &= ~(target == T::ConstructorRva ? IMAGE_SCN_MEM_EXECUTE : IMAGE_SCN_MEM_WRITE);
                fixture.Patch(at, &changed, sizeof(changed));
                action([&] { refusal(); });
                fixture.Patch(at, &section, sizeof(section));
            }
        for (unsigned changed = 1; changed <= 12; ++changed)
        {
            restore();
            mutation = changed;
            Host::afterTuple = &Changed;
            action(
                [&]
                {
                    NativeModelTimeObservation value{true, 0x91, 0xe7};
                    const auto                 result = runtime.access.ReadModelTime(3, value);
                    restore();
                    Check(result != ClockResult::Ok && value.hasTime && value.on == 0x91 && value.off == 0xe7,
                          "tuple/pair/count/bank/context/GC/thread/table/source mutation before final bookend refuses");
                });
        }
        restore();
        Host::copyFailure = true;
        action([&] { refusal(); });
        restore();
        action(
            [&]
            {
                Bytes::Store(image + NativeGameStateProfile::stateRva, std::uint32_t{8});
                refusal();
                Bytes::Store(image + NativeGameStateProfile::stateRva, std::uint32_t{9});
            });
        restore();
        for (const auto key : std::array<std::uint32_t, 4>{20000, 65535, 65536, 0xffffffffu})
            action(
                [&]
                {
                    NativeModelTimeObservation value{true, 0x91, 0xe7};
                    Check(runtime.access.ReadModelTime(key, value) == ClockResult::InvalidArgument && value.hasTime && value.on == 0x91 && value.off == 0xe7,
                          "fullDWORD invalid key never aliases a pool row");
                });
        NativeModelTimeObservation outside{true, 0x91, 0xe7};
        Check(runtime.access.ReadModelTime(3, outside) != ClockResult::Ok && outside.on == 0x91 && outside.off == 0xe7,
              "outside actual callback grants no tuple authority");
        ClockResult foreign = ClockResult::Ok;
        std::thread other(
            [&]
            {
                NativeModelTimeObservation value{true, 0x91, 0xe7};
                foreign = runtime.access.ReadModelTime(3, value);
                if (value.on != 0x91 || value.off != 0xe7)
                    std::abort();
            });
        other.join();
        Check(foreign != ClockResult::Ok, "foreign thread cannot borrow the retained owner's timed tuple");
        action(
            [&]
            {
                NativeModelTimeObservation value{true, 0x91, 0xe7};
                Check(
                    NativeModelClassificationCalls::ReadTime(image, 3, nullptr, value) == ClockResult::NotInitialized && value.on == 0x91 && value.off == 0xe7,
                    "missing owner refuses without a native call or output effect");
            });
        Check(ClockModelClassificationOwnerTest::calls == nativeCalls, "all timed reads and refusals invoke no original classifier or native getter");
        std::puts("Production model time getter source current owner refusals fixture completed");
        std::puts("Production model time getter raw outputs and complete bytes preserved fixture completed");
        WorldGroundOwnerFixture::Retire(service, fixture);
        NativeModelTimeObservation retired{true, 0x91, 0xe7};
        Check(runtime.access.ReadModelTime(3, retired) != ClockResult::Ok && retired.on == 0x91 && retired.off == 0xe7,
              "retired owner cannot replay the prior tuple");
        Host::active = Host::copyFailure = false;
        Host::model = 0;
        Host::afterTuple = nullptr;
        activeFixture = nullptr;
        image = modelPointer = providerPointer = bankSlot = 0;
        ClockReaderTestHost::image = priorReader;
        for (auto item = saved.rbegin(); item != saved.rend(); ++item)
            fixture.Patch(item->rva, item->bytes.data(), item->bytes.size());
    }
}
