/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Existing empty-slot special request through the actual SDK owner
 ****************************************************************************/
{
    namespace X = NativeStreamingSpecialModelProfile;
    namespace A = NativeModelReferenceTransactionProfile;
    for (const auto& pin : X::Code)
        admit(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
    for (const auto& pin : A::Code)
        admit(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
    for (const auto& pin : NativeModelReferenceProfile::Code)
        admit(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
    for (const auto& pin : NativeStreamingRemoveProfile::Code)
        admit(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
    admit(NativeStreamingFileIdsProfile::initializeRva, NativeStreamingFileIdsProfile::initialize.data(), NativeStreamingFileIdsProfile::initialize.size(),
          PAGE_EXECUTE_READ);
    for (const auto& pin : NativeStreamingLoadAllProfile::Code)
        admit(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
    for (const auto& pin : NativeStartupOwnerProfile::pins)
        admit(pin.rva, pin.bytes, pin.size, pin.code ? PAGE_EXECUTE_READ : PAGE_READONLY);
    for (const auto& pin : NativeStartupWorldProfile::pins)
        admit(pin.rva, pin.bytes, pin.size, pin.code ? PAGE_EXECUTE_READ : PAGE_READONLY);
    admit(X::CrcTableRva, X::CrcTable, sizeof(X::CrcTable), PAGE_READONLY);
    admit(A::ClassSizesRva, A::ClassSizes.data(), sizeof(A::ClassSizes), PAGE_READONLY);
    alignas(std::uintptr_t) std::array<unsigned char, A::HeapBytes> specialHeap{};
    const auto                                                      specialHeapAddress = reinterpret_cast<std::uintptr_t>(specialHeap.data());
    store(specialHeap.data(), 0, fixture.image + A::HeapVtableRva);
    admit(A::HeapRva, &specialHeapAddress, sizeof(specialHeapAddress), PAGE_READWRITE);
    admit(A::HeapAuthorityRva, &specialHeapAddress, sizeof(specialHeapAddress), PAGE_READWRITE);
    for (const auto slot : {std::pair<std::uintptr_t, std::uintptr_t>{A::HeapVtableRva + 0x10, X::HeapAllocateRva},
                            {A::HeapVtableRva + 0x20, A::HeapReallocateRva},
                            {A::HeapVtableRva + 0x38, A::HeapQuantizeRva}})
    {
        const auto target = fixture.image + slot.second;
        admit(slot.first, &target, sizeof(target), PAGE_READONLY);
    }
    const unsigned char ret = 0xc3;
    admit(X::HeapAllocateRva, &ret, 1, PAGE_EXECUTE_READ);
    std::array<unsigned char, A::LookupBytes> specialLookup{};
    std::array<std::uint16_t, A::ClassCount>  specialReverse{};
    std::size_t                               category = 0;
    for (std::size_t i = 0; i != specialLookup.size(); ++i)
    {
        while (category < A::ClassSizes.size() && A::ClassSizes[category] < i * 16)
            ++category;
        specialLookup[i] = static_cast<unsigned char>(category);
    }
    for (std::size_t i = 0; i != specialReverse.size(); ++i)
        specialReverse[i] = A::ClassSizes[A::ClassSizes.size() - i - 1];
    admit(A::LookupRva, specialLookup.data(), specialLookup.size(), PAGE_READWRITE);
    admit(A::ReverseSizesRva, specialReverse.data(), sizeof(specialReverse), PAGE_READWRITE);
    // Compose one real raw registry for the existing root/provider and typed
    // world graph. No private object gets a second fixture-specific identity.
    for (const auto rva :
         {NativeStartupOwnerProfile::gameModeClassCacheRva, W::levelClassCacheRva, NativeStreamingLoadAllProfile::RootWorldSlotRva, W::ownerGetWorldSlotRva})
    {
        const auto prior = fixture.Get<std::uintptr_t>(rva);
        admit(rva, &prior, sizeof(prior),
              rva == NativeStreamingLoadAllProfile::RootWorldSlotRva || rva == W::ownerGetWorldSlotRva ? PAGE_READONLY : PAGE_READWRITE);
    }
    const auto graphWrite = +[](void* opaque, std::uintptr_t rva, const void* bytes, std::size_t size, DWORD protection)
    {
        auto& selected = *static_cast<ClockImageFixture*>(opaque);
        selected.Patch(rva, bytes, size);
        DWORD ignored = 0;
        Check(VirtualProtect(reinterpret_cast<void*>(selected.image + rva), size, protection, &ignored) != FALSE, "admit typed world global protection");
    };
    NativeStreamingLoadAllFixture::Graph                                       specialGraph(fixture.image, root, graphWrite, &fixture);
    alignas(std::uintptr_t) std::array<unsigned char, 12 * U::objectItemBytes> specialItems{};
    std::vector<unsigned char>                                                 specialNameBlock(X::NameBlockBytes), specialHashes(0x4000);
    std::vector<unsigned char>                                                 specialNamePool(X::NamePoolBytes);
    store(specialNamePool.data(), 0x10, reinterpret_cast<std::uintptr_t>(specialNameBlock.data()));
    for (std::size_t i = 0; i != X::ShardCount; ++i)
    {
        auto* shard = specialNamePool.data() + X::ShardOffset + i * X::ShardStride;
        store(shard, 0xc, std::uint32_t{255});
        store(shard, 0x10, reinterpret_cast<std::uintptr_t>(specialHashes.data()) + i * 0x400);
        store(shard, 0x18, fixture.image + X::NamePoolRva);
    }
    admit(X::NamePoolRva, specialNamePool.data(), specialNamePool.size(), PAGE_READWRITE);
    const unsigned char initializedNames = 1;
    admit(X::NameInitializedRva, &initializedNames, 1, PAGE_READWRITE);
    std::array<unsigned char, X::CacheHeaderBytes> emptyCache{};
    store(emptyCache.data(), 0x2c, std::int32_t{128});
    store(emptyCache.data(), 0x30, std::int32_t{-1});
    admit(X::CacheRva, emptyCache.data(), emptyCache.size(), PAGE_READWRITE);
    admit(X::ReverseCacheRva, emptyCache.data(), emptyCache.size(), PAGE_READWRITE);
    const auto resetSpecial = [&]
    {
        specialGraph.Reset();
        std::memcpy(specialItems.data(), specialGraph.items.data(), specialGraph.items.size());
        store(provider.data(), W::objectIndexOffset, std::int32_t{11});
        store(provider.data(), W::objectClassOffset, specialGraph.Class(0));
        store(provider.data(), 0x20, root);
        store(specialItems.data(), 11 * U::objectItemBytes, providerAddress);
        store(specialItems.data(), 11 * U::objectItemBytes + 0x10, std::uint32_t{111});
        chunks[0] = reinterpret_cast<std::uintptr_t>(specialItems.data());
        fixture.Put(U::objectChunksRva, reinterpret_cast<std::uintptr_t>(chunks.data()));
        fixture.Put(U::objectCountRva, std::int32_t{12});
        store(model.data(), 0x22, std::int16_t{0});
        std::memset(model.data() + 8, 0, 0x15);
        seedRow(0x40, 0);
        fixture.Put(X::NameInitializedRva, initializedNames);
        ClockSpecialModelOwnerTest::mode = 0;
        ClockSpecialModelOwnerTest::during = nullptr;
    };
    struct SpecialSdk final : CStreamingDE
    {
        explicit SpecialSdk(NativeStreamingModelAccess& access) : CStreamingDE(access) {}
        [[noreturn]] static void Unsupported()
        {
            std::fputs("Unexpected unqualified streaming SDK call\n", stderr);
            std::abort();
        }
        CStreamingInfo* GetStreamingInfo(uint32) override { Unsupported(); }
        void            ReinitStreaming() override { Unsupported(); }
        unsigned char   AddArchive(const wchar_t*) override { Unsupported(); }
        void            RemoveArchive(unsigned char) override { Unsupported(); }
        void            SetStreamingInfo(unsigned int, unsigned char, unsigned int, unsigned short, unsigned int) override { Unsupported(); }
        bool            SetStreamingBufferSize(uint32) override { Unsupported(); }
        uint32          GetStreamingBufferSize() override { Unsupported(); }
        void            MakeSpaceFor(std::uint32_t) override { Unsupported(); }
        std::uint32_t   GetMemoryUsed() const override { Unsupported(); }
        void            RemoveBigBuildings() override { Unsupported(); }
        void            LoadSceneCollision(const CVector*) override { Unsupported(); }
    } specialSdk(runtime.access);
    struct SpecialCallback
    {
        std::function<void(NativeGameTimingAccess&)> action;
        static void Invoke(NativeGameTimingAccess& access, void* opaque) { static_cast<SpecialCallback*>(opaque)->action(access); }
    } specialCall;
    struct DefaultSpecialOwner final : NativeGameTimingAccess
    {
        ClockResult SetTimeScale(float) noexcept override { return ClockResult::NotInitialized; }
        ClockResult SetMinuteDuration(std::uint32_t) noexcept override { return ClockResult::NotInitialized; }
        ClockResult Pause(bool) noexcept override { return ClockResult::NotInitialized; }
    } specialAbsent;
    ClockSpecialModelOwnerTest::image = fixture.image;
    ClockSpecialModelOwnerTest::metadata = modelAddress;
    ClockSpecialModelOwnerTest::calls = 0;
    Check(specialAbsent.RequestStreamingSpecialModel(3, "special", 6) == ClockResult::NotInitialized &&
              runtime.access.RequestStreamingSpecialModel(3, "special", 6) != ClockResult::Ok,
          "older and unbound owners cannot report a special-model no-op as acceptance");
    const auto retireSpecial = [&](NativeClockService& service)
    {
        const auto calls = ClockSpecialModelOwnerTest::calls;
        if (!service.End())
        {
            fixture.invokeTimer();
            Check(service.End(), "natural callback retires special-model owner");
        }
        Check(!service.IsRetained() && !runtime.timingHandler && !NativeServiceAdmission::IsClaimed(NativeServiceAdmission::Category::Clock) &&
                  ClockSpecialModelOwnerTest::calls == calls,
              "retirement never repeats special-model entry or rolls back persistent name/request effects");
    };
    ClockResult specialResult = ClockResult::NotInitialized;
    // Exercise mutable caller intent and an actual SDK string literal from
    // the caller module; neither grants authority over native image data.
    char mixedName[] = "Mixed_2", persistentName[] = "persist";
    resetSpecial();
    {
        NativeClockService service;
        specialCall.action = [&](NativeGameTimingAccess& access) { specialResult = access.RequestStreamingSpecialModel(3, mixedName, 0x80000116u); };
        BeginOwned(service, fixture, false, true, true, &SpecialCallback::Invoke, &specialCall);
        Check(runtime.access.RequestStreamingSpecialModel(3, "special", 6) != ClockResult::Ok && !ClockSpecialModelOwnerTest::calls,
              "retained model context does not grant between-callback special entry");
        std::thread foreign([&] { specialResult = runtime.access.RequestStreamingSpecialModel(3, "special", 6); });
        foreign.join();
        Check(specialResult != ClockResult::Ok && !ClockSpecialModelOwnerTest::calls && !runtime.stopping.load(),
              "foreign special request never enters or Stops another owner");
        const auto invokeSpecial = [&]
        {
            Check(service.Renew(runtime.generation.load()), "renew qualified special-model callback");
            fixture.invokeTimer();
        };
        store(model.data(), 0x22, std::int16_t{1});
        invokeSpecial();
        Check(specialResult == ClockResult::UnsupportedBuild && !ClockSpecialModelOwnerTest::calls && !runtime.stopping.load(),
              "positive native model refs refuse unqualified ped/object retirement before entry");
        store(model.data(), 0x22, std::int16_t{0});
        fixture.Put(X::NameInitializedRva, std::uint8_t{0});
        invokeSpecial();
        Check(specialResult != ClockResult::Ok && !ClockSpecialModelOwnerTest::calls && !runtime.stopping.load(),
              "uninitialized real FName pool is not bypassed by owner wiring");
        fixture.Put(X::NameInitializedRva, initializedNames);
        ClockSpecialModelOwnerTest::during = +[]()
        {
            const CVector position(0, 0, 0);
            char          nestedName[] = "again";
            Check(runtime.access.RequestStreamingSpecialModel(3, nestedName, 6) == ClockResult::Busy &&
                      runtime.access.RequestStreamingModel(3, 6) == ClockResult::Busy && runtime.access.RequestStreamingUpgrade(1000, 6) == ClockResult::Busy &&
                      runtime.access.LoadStreamingModels(false) == ClockResult::Busy && runtime.access.LoadStreamingModels(true) == ClockResult::Busy &&
                      runtime.access.LoadStreamingScene(&position) == ClockResult::Busy,
                  "genuine special entry retains the existing shared loading gate against every adapter loading intent");
        };
        invokeSpecial();
        Check(specialResult == ClockResult::Ok && ClockSpecialModelOwnerTest::calls == 1 &&
                  ClockSpecialModelOwnerTest::entry == reinterpret_cast<NativeStreamingSpecialModelCalls::NativeFunction>(fixture.image + X::EntryRva) &&
                  ClockSpecialModelOwnerTest::model == 3 && ClockSpecialModelOwnerTest::channel == 0x80000116u &&
                  std::strcmp(ClockSpecialModelOwnerTest::copied.data(), "Mixed_2") == 0,
              "actual callback admits one genuine entry with copied original name and every DWORD channel bit");
        resetSpecial();
        specialCall.action = [&](NativeGameTimingAccess&)
        {
            try
            {
                CStreaming& sdk = specialSdk;
                sdk.RequestSpecialModel(3, "eight888", 0xffffffffu);
                specialResult = ClockResult::Ok;
            }
            catch (const std::exception& error)
            {
                Check(false, error.what());
            }
        };
        invokeSpecial();
        Check(specialResult == ClockResult::Ok && ClockSpecialModelOwnerTest::calls == 2 && ClockSpecialModelOwnerTest::channel == 0xffffffffu &&
                  std::strcmp(reinterpret_cast<const char*>(model.data() + 0xc), "eight888") == 0,
              "actual SDK RequestSpecialModel reaches the same retained native owner after the previous gate was released");
        retireSpecial(service);
    }
    for (const unsigned failure : {1u, 2u})
    {
        resetSpecial();
        ClockSpecialModelOwnerTest::mode = failure;
        NativeClockService service;
        specialCall.action = [&](NativeGameTimingAccess& access) { specialResult = access.RequestStreamingSpecialModel(3, persistentName, 6); };
        BeginOwned(service, fixture, false, true, true, &SpecialCallback::Invoke, &specialCall);
        const auto before = ClockSpecialModelOwnerTest::calls;
        fixture.invokeTimer();
        Check(specialResult != ClockResult::Ok && runtime.stopping.load() && ClockSpecialModelOwnerTest::calls == before + 1 &&
                  std::strcmp(reinterpret_cast<const char*>(model.data() + 0xc), "persist") == 0 && fixture.Get<unsigned char>(row + L::StateOffset) == 2,
              "entered exception or post-root loss Stops the admitted owner and preserves native name/request prefix");
        retireSpecial(service);
        Check(ClockSpecialModelOwnerTest::calls == before + 1 && std::strcmp(reinterpret_cast<const char*>(model.data() + 0xc), "persist") == 0,
              "cleanup never repeats native special entry or restores its accepted name");
    }
    ClockSpecialModelOwnerTest::image = ClockSpecialModelOwnerTest::metadata = 0;
    ClockSpecialModelOwnerTest::during = nullptr;
    ClockSpecialModelOwnerTest::mode = 0;
    std::puts("Production native special model callback owner fixture completed");
    std::puts("Production actual special model SDK callback fixture completed");
}
