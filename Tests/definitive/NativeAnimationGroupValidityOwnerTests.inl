/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual group Created bool observes native cookie-owned array lifetime
 ****************************************************************************/
namespace AnimationGroupValidityOwnerFixture
{
    namespace Host = NativeCustomAnimSequenceStorageFixture;
    namespace Hook = ClockAnimationManagerOwnerTest;
    std::uintptr_t rows = 0, baseAddress = 0;
    std::uint64_t* callerGeneration = nullptr;
    std::uint32_t* callerIndex = nullptr;
    bool           changed = false;
    void           ChangeCaller() noexcept
    {
        *callerGeneration = 0;
        *callerIndex = 0xffffffffu;
    }
    void ChangeValueAfterPayload() noexcept
    {
        if (Hook::reads < 2)
        {
            Hook::afterRead = &ChangeValueAfterPayload;
            return;
        }
        const std::uint32_t value = 1;
        std::memcpy(reinterpret_cast<void*>(rows + 0x10), &value, 4);
        changed = true;
    }
    void ChangeCookieAfterPayload() noexcept
    {
        if (Hook::reads < 2)
        {
            Hook::afterRead = &ChangeCookieAfterPayload;
            return;
        }
        const std::uint64_t capacity = 7;
        std::memcpy(reinterpret_cast<void*>(baseAddress), &capacity, 8);
        changed = true;
    }
    void LoseFinalOwner() noexcept
    {
        if (Hook::reads < 4)
        {
            Hook::afterRead = &LoseFinalOwner;
            return;
        }
        ++timingCallbackGeneration;
        changed = true;
    }
    void RunPrepared(ClockImageFixture& fixture)
    {
        WorldGroundOwnerFixture::Writes saved{fixture, {}};
        const auto                      admit = [&](const auto& pins)
        {
            for (const auto& pin : pins)
                WorldGroundOwnerFixture::Writes::Apply(&saved, pin.rva, pin.bytes, pin.size, pin.code ? PAGE_EXECUTE_READ : PAGE_READONLY);
        };
        for (const auto& pin : NativeStreamingLoadAllProfile::Code)
            WorldGroundOwnerFixture::Writes::Apply(&saved, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        admit(NativeStartupOwnerProfile::pins);
        admit(NativeStartupWorldProfile::pins);
        for (const auto& pin : NativeUObjectMembershipProfile::pins)
            WorldGroundOwnerFixture::Writes::Apply(&saved, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        admit(NativeGameStateProfile::pins);
        for (const auto& pin : NativeBorrowedGcProfile::pins)
            WorldGroundOwnerFixture::Writes::Apply(&saved, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : NativeStartGameProfile::Code)
            if (pin.rva == 0xab879c || pin.rva == 0xab92f0 || pin.rva == 0x1af4ef0)
                WorldGroundOwnerFixture::Writes::Apply(&saved, pin.rva, pin.bytes.data(), pin.size, PAGE_EXECUTE_READ);
        alignas(std::uintptr_t) std::array<unsigned char, 0x180> root{};
        NativeStreamingLoadAllFixture::Graph graph(fixture.image, reinterpret_cast<std::uintptr_t>(root.data()), &WorldGroundOwnerFixture::Writes::Apply,
                                                   &saved);
        NativePedClothesFixture::Install(fixture.image, &WorldGroundOwnerFixture::Writes::Apply, &saved);
        NativePedClothesFixture::NameState names;
        names.Reset(fixture.image, &WorldGroundOwnerFixture::Writes::Apply, &saved);
        Host::State host;
        host.Reset(fixture.image, &WorldGroundOwnerFixture::Writes::Apply, &saved);
        Host::active = &host;
        for (const auto& pin : NativeAnimationSequenceRowProfile::Code)
            WorldGroundOwnerFixture::Writes::Apply(&saved, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        // Cleanup exercises the real fresh frame producers, so their existing
        // instruction profile must be installed before binding either row.
        for (const auto& pin : NativeAnimationSequenceFrameBindProfile::Code)
            WorldGroundOwnerFixture::Writes::Apply(&saved, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : NativeAnimationSequenceNameProfile::Code)
            WorldGroundOwnerFixture::Writes::Apply(&saved, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        NativeAnimationBlockFactoryFixture::Install(fixture.image, &WorldGroundOwnerFixture::Writes::Apply, &saved);
        for (const auto& pin : NativeAnimationHierarchyReadProfile::Code)
            WorldGroundOwnerFixture::Writes::Apply(&saved, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        WorldGroundOwnerFixture::Writes::Apply(&saved, 0xfec67a, private_hierarchy_fixture::CacheProof, sizeof(private_hierarchy_fixture::CacheProof),
                                               PAGE_EXECUTE_READ);
        graph.Reset();
        saved.Global(NativeGameStateProfile::stateRva, std::uint32_t{9});
        saved.Global(NativeStartGameProfile::ThreadInitializedRva, static_cast<unsigned char>(1));
        saved.Global(NativeStartGameProfile::ThreadIdRva, GetCurrentThreadId());
        saved.Global(NativeBorrowedGcProfile::markerRva, LONG{0});
        const auto previousImage = ClockReaderTestHost::image;
        ClockReaderTestHost::image = fixture.image;
        WorldGroundOwnerFixture::Callback callback;
        NativeClockService                service;
        callback.action = [](NativeGameTimingAccess&) {};
        BeginOwned(service, fixture, false, true, true, &WorldGroundOwnerFixture::Callback::Invoke, &callback);
        // BeginOwned replaces admission before shipping's module-pin step.
        // This scope tests that lifetime too, so pin the real callback code.
        HMODULE cleanupModule = nullptr;
        Check(GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN, reinterpret_cast<LPCWSTR>(&HookClockContinuation),
                                 &cleanupModule) &&
                  cleanupModule,
              "cleanup fixture obtains the genuine process-lifetime pin for its compiled callback module");
        runtime.modulePinned.store(true);
        namespace P = NativeAnimationGroupArrayProfile;
        for (const auto& pin : P::Code)
            WorldGroundOwnerFixture::Writes::Apply(&saved, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        constexpr std::uint64_t capacity = 8;
        constexpr std::size_t   bytes = P::CookieBytes + capacity * P::RowBytes;
        void*                   allocation = VirtualAlloc(nullptr, bytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        Check(allocation != nullptr, "own only the synthetic engine array buffer used by the readonly lifecycle fixture");
        baseAddress = reinterpret_cast<std::uintptr_t>(allocation);
        rows = baseAddress + P::CookieBytes;
        std::memset(allocation, 0xa5, bytes);
        std::memcpy(allocation, &capacity, sizeof(capacity));
        for (std::uint32_t index = 0; index != capacity; ++index)
        {
            const auto          row = rows + index * P::RowBytes;
            const std::uint64_t zero = 0;
            const std::uint32_t invalid = 0xffffffffu;
            std::memcpy(reinterpret_cast<void*>(row), &zero, 8);
            std::memcpy(reinterpret_cast<void*>(row + 8), &zero, 8);
            std::memcpy(reinterpret_cast<void*>(row + 0x10), &zero, 8);
            std::memcpy(reinterpret_cast<void*>(row + 0x18), &invalid, 4);
        }
        const std::array<std::uint32_t, 6> values{0, 1, 0xffffffffu, 0x80000000u, 0, 17};
        for (std::uint32_t index = 0; index != values.size(); ++index)
            std::memcpy(reinterpret_cast<void*>(rows + index * P::RowBytes + P::CreatedOffset), &values[index], 4);
        const std::uintptr_t unrelatedChild = 0x1234;
        std::memcpy(reinterpret_cast<void*>(rows + 8), &unrelatedChild, sizeof(unrelatedChild));
        saved.Global(P::GlobalRva, rows);
        saved.Global(P::CountRva, std::int32_t{6});
        std::array<unsigned char, bytes> expected{};
        std::memcpy(expected.data(), allocation, expected.size());
        const auto                             generation = runtime.generation.load();
        PrivateAnimationFactoryManagerReceiver manager(runtime.access, generation);
        CAnimManager&                          sdk = manager;
        Hook::watched = fixture.image + P::GlobalRva;
        Hook::watchedBytes = sizeof(std::uintptr_t);
        Hook::reads = 0;
        Hook::failNext = false;
        Hook::afterRead = nullptr;
        const auto action = [&](const char* label, const auto& body)
        {
            callback.action = [&](NativeGameTimingAccess&)
            {
                try
                {
                    body();
                }
                catch (const std::exception& error)
                {
                    std::fprintf(stderr, "Group validity action %s refused: %s\n", label, error.what());
                    Check(false, label);
                }
            };
            Check(service.Renew(generation), "renew the actual native owner separately for each Created observation");
            fixture.invokeTimer();
            callback.action = [](NativeGameTimingAccess&) {};
        };
        const auto unchanged = [&]
        {
            return !std::memcmp(expected.data(), allocation, expected.size()) && !host.mallocCalls && !host.freeCalls &&
                   !runtime.customSequences.IsRetained() && !runtime.privateHierarchies.IsRetained();
        };
        const auto refuses = [](const auto& call)
        {
            try
            {
                call();
            }
            catch (const std::exception&)
            {
                return true;
            }
            return false;
        };
        for (std::uint32_t index = 0; index != values.size(); ++index)
        {
            action("source Created DWORD predicate",
                   [&]
                   {
                       bool output = values[index] == 0;
                       Check(runtime.access.ReadAnimationGroupCreated(generation, index, output) == ClockResult::Ok && output == (values[index] != 0) &&
                                 sdk.IsValidGroup(static_cast<AssocGroupId>(index)) == (values[index] != 0) && unchanged(),
                             "actual SDK/native Created uses rawDWORD+10 nonzero including negative bits, never child+8 or a positive-only clamp");
                   });
        }
        action("current logical count and original allocation capacity are distinct",
               [&]
               {
                   fixture.Put(P::CountRva, std::int32_t{9});
                   bool       output = false;
                   const auto within = runtime.access.ReadAnimationGroupCreated(generation, 1, output);
                   bool       outside = true;
                   const auto beyond = runtime.access.ReadAnimationGroupCreated(generation, 8, outside);
                   fixture.Put(P::CountRva, std::int32_t{6});
                   Check(within == ClockResult::Ok && output && beyond == ClockResult::InvalidArgument && outside && unchanged(),
                         "a mutable count change cannot expand the actual cookie extent or require false count/capacity equality");
               });
        action("logical range still guards valid physical rows",
               [&]
               {
                   fixture.Put(P::CountRva, std::int32_t{4});
                   bool       output = true;
                   const auto result = runtime.access.ReadAnimationGroupCreated(generation, 5, output);
                   fixture.Put(P::CountRva, std::int32_t{6});
                   Check(result == ClockResult::InvalidArgument && output && unchanged(),
                         "cookie capacity alone cannot admit a row outside current native logical extent");
               });
        action("full index and owner affinity preserve outputs",
               [&]
               {
                   bool output = true;
                   Check(runtime.access.ReadAnimationGroupCreated(generation, 0xffffffffu, output) == ClockResult::InvalidArgument && output &&
                             runtime.access.ReadAnimationGroupCreated(generation + 1, 0, output) == ClockResult::OwnerConflict && output && unchanged(),
                         "full32 group index cannot wrap and foreign generation cannot publish a false Created value");
               });
        action("null current native array refuses",
               [&]
               {
                   fixture.Put(P::GlobalRva, std::uintptr_t{0});
                   bool       output = true;
                   const auto result = runtime.access.ReadAnimationGroupCreated(generation, 0, output);
                   fixture.Put(P::GlobalRva, rows);
                   Check(result == ClockResult::NotInitialized && output && unchanged(),
                         "native shutdown/null state is an unavailable receiver, never an invented Created=false field observation");
               });
        action("nonpositive logical count refuses",
               [&]
               {
                   fixture.Put(P::CountRva, std::int32_t{0});
                   bool       output = true;
                   const auto result = runtime.access.ReadAnimationGroupCreated(generation, 0, output);
                   fixture.Put(P::CountRva, std::int32_t{6});
                   Check(result == ClockResult::NotInitialized && output && unchanged(), "source count0 cannot lend logical row authority from an old cookie");
               });
        action("malformed cookie cannot forge allocation extent",
               [&]
               {
                   const std::uint64_t wrong = (std::numeric_limits<std::uint64_t>::max)();
                   std::memcpy(allocation, &wrong, 8);
                   bool       output = true;
                   const auto result = runtime.access.ReadAnimationGroupCreated(generation, 0, output);
                   std::memcpy(allocation, &capacity, 8);
                   Check(result == ClockResult::BindingChanged && output && unchanged(),
                         "invalid64-bit producer cookie refuses before row multiplication or access");
               });
        for (const auto pointer : {rows + 1, fixture.image + NativeAnimationBlockProfile::BankRva + 8})
        {
            action("actual global needs genuine private aligned allocation",
                   [&]
                   {
                       fixture.Put(P::GlobalRva, pointer);
                       bool       output = true;
                       const auto result = runtime.access.ReadAnimationGroupCreated(generation, 0, output);
                       fixture.Put(P::GlobalRva, rows);
                       Check(result != ClockResult::Ok && output && unchanged(),
                             "misaligned or MEM_IMAGE storage cannot be adopted as the native malloc cookie array");
                   });
        }
        action("captured caller intent is immutable through callbacks",
               [&]
               {
                   auto          sourceGeneration = generation;
                   std::uint32_t sourceIndex = 1;
                   bool          output = false;
                   callerGeneration = &sourceGeneration;
                   callerIndex = &sourceIndex;
                   Hook::afterRead = &ChangeCaller;
                   const auto result = runtime.access.ReadAnimationGroupCreated(sourceGeneration, sourceIndex, output);
                   Hook::afterRead = nullptr;
                   callerGeneration = nullptr;
                   callerIndex = nullptr;
                   Check(result == ClockResult::Ok && output && !sourceGeneration && sourceIndex == 0xffffffffu && unchanged(),
                         "actual receiver snapshots generation/full index before a native-global read callback changes caller objects");
               });
        action("actual NumAnims transition invalidates observation",
               [&]
               {
                   Hook::reads = 0;
                   changed = false;
                   Hook::afterRead = &ChangeValueAfterPayload;
                   bool       output = false;
                   const auto result = runtime.access.ReadAnimationGroupCreated(generation, 0, output);
                   Hook::afterRead = nullptr;
                   std::memcpy(reinterpret_cast<void*>(rows + P::CreatedOffset), &values[0], 4);
                   Check(changed && result == ClockResult::BindingChanged && !output && unchanged(),
                         "Changed current NumAnims cannot become a torn copied Created result");
               });
        action("actual capacity transition invalidates observation",
               [&]
               {
                   Hook::reads = 0;
                   changed = false;
                   Hook::afterRead = &ChangeCookieAfterPayload;
                   bool       output = true;
                   const auto result = runtime.access.ReadAnimationGroupCreated(generation, 0, output);
                   Hook::afterRead = nullptr;
                   std::memcpy(allocation, &capacity, 8);
                   Check(changed && result == ClockResult::BindingChanged && output && unchanged(),
                         "constructor-cookie bookends are required independently from count/global equality");
               });
        action("first native global read fault preserves bool",
               [&]
               {
                   Hook::failNext = true;
                   bool output = true;
                   Check(runtime.access.ReadAnimationGroupCreated(generation, 0, output) == ClockResult::ReadFailure && output && !runtime.stopping.load(),
                         "a lost source global read is refusal, not a synthetic false or a retained native operation");
               });
        action("final current owner check preserves bool",
               [&]
               {
                   const auto previous = timingCallbackGeneration;
                   Hook::reads = 0;
                   changed = false;
                   Hook::afterRead = &LoseFinalOwner;
                   bool       output = true;
                   const auto result = runtime.access.ReadAnimationGroupCreated(generation, 0, output);
                   Hook::afterRead = nullptr;
                   timingCallbackGeneration = previous;
                   Check(changed && result == ClockResult::OwnerConflict && output && unchanged(),
                         "real final generation loss refuses without field mutation/native debt");
               });
        const auto before = Hook::reads;
        Check(!sdk.IsValidGroup(static_cast<AssocGroupId>(0xffffffffu)) && Hook::reads == before,
              "source invalid enum gate remains a CPU false result without borrowing the native array outside a callback");
        Check(refuses([&] { static_cast<void>(sdk.IsValidGroup(static_cast<AssocGroupId>(0))); }) && unchanged(),
              "a valid enum does not lend native owner admission to an off-callback bool query");
        Hook::afterRead = nullptr;
        Hook::failNext = false;
        Hook::watched = 0;
        Hook::watchedBytes = sizeof(std::int32_t);
        WorldGroundOwnerFixture::Retire(service, fixture);
        Check(unchanged(), "ending the readonly observer never frees or retains the engine-owned array represented by this task buffer");
        fixture.Put(P::GlobalRva, std::uintptr_t{0});
        fixture.Put(P::CountRva, std::int32_t{0});
        Check(VirtualFree(allocation, 0, MEM_RELEASE) != FALSE, "release only task-owned fixture storage after the real readonly native owner retires");
        rows = baseAddress = 0;
        Host::active = nullptr;
        ClockReaderTestHost::image = previousImage;
        saved.Restore();
        std::puts("Production animation group validity owner fixture completed");
        std::puts("Production actual animation group validity SDK fixture completed");
    }
}
