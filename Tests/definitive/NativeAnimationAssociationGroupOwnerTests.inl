/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Real partial group SDK reads over current native cookie-owned rows
 ****************************************************************************/
namespace AnimationAssociationGroupOwnerFixture
{
    namespace Host = NativeCustomAnimSequenceStorageFixture;
    namespace Hook = ClockAnimationManagerOwnerTest;
    std::uintptr_t                                 rows = 0, block = 0;
    NativeAnimationAssociationGroupReferenceToken* callerToken = nullptr;
    bool                                           changed = false;
    unsigned                                       transition = 0;
    void                                           ChangeCaller() noexcept
    {
        callerToken->ownerGeneration = 0;
        callerToken->index = 0xffffffffu;
        callerToken->locator = 1;
    }
    void ChangeAfterPayload() noexcept
    {
        if (Hook::reads < 3)
        {
            Hook::afterRead = &ChangeAfterPayload;
            return;
        }
        if (transition == 0)
        {
            const std::uint32_t id = 4;
            std::memcpy(reinterpret_cast<void*>(rows + 32 + 0x18), &id, 4);
        }
        if (transition == 1)
        {
            const std::uint8_t loaded = 0;
            std::memcpy(reinterpret_cast<void*>(block + NativeAnimationBlockProfile::LoadedOffset), &loaded, 1);
        }
        if (transition == 2)
        {
            const std::uintptr_t null = 0;
            std::memcpy(reinterpret_cast<void*>(rows + 32), &null, 8);
        }
        changed = true;
    }
    void LoseFinalOwner() noexcept
    {
        if (Hook::reads < 8)
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
        namespace B = NativeAnimationBlockProfile;
        for (const auto& pin : P::Code)
            WorldGroundOwnerFixture::Writes::Apply(&saved, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        constexpr std::uint64_t capacity = 4;
        constexpr std::size_t   bytes = 8 + capacity * 32;
        void*                   allocation = VirtualAlloc(nullptr, bytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        Check(allocation != nullptr, "own only task fixture storage representing the actual native group allocation");
        rows = reinterpret_cast<std::uintptr_t>(allocation) + 8;
        block = fixture.image + B::BankRva;
        std::memset(allocation, 0xa5, bytes);
        std::memcpy(allocation, &capacity, 8);
        const std::array<std::uint32_t, 3> nums{0, 1, 0x80000000u};
        const std::array<std::uint32_t, 3> groups{0, 3, 0xffffffffu};
        for (std::size_t i = 0; i != nums.size(); ++i)
        {
            const auto row = rows + i * 32;
            const auto parentBlock = i ? block : std::uintptr_t{0};
            std::memcpy(reinterpret_cast<void*>(row), &parentBlock, 8);
            std::memcpy(reinterpret_cast<void*>(row + 0x10), &nums[i], 4);
            std::memcpy(reinterpret_cast<void*>(row + 0x18), &groups[i], 4);
        }
        std::array<unsigned char, B::RowBytes> nativeBlock{};
        std::memset(nativeBlock.data(), 'A', 16);
        nativeBlock[B::LoadedOffset] = 0x80;
        // A full16 affinity is deliberately not a C string. IsLoaded observes
        // only the typed block's real loaded byte, not a named-factory policy.
        saved.Global(B::BankRva, nativeBlock);
        saved.Global(B::CountRva, std::int32_t{1});
        saved.Global(P::GlobalRva, rows);
        saved.Global(P::CountRva, std::int32_t{3});
        std::array<unsigned char, bytes> expected{};
        std::memcpy(expected.data(), allocation, bytes);
        const auto                                                   generation = runtime.generation.load();
        auto&                                                        access = static_cast<NativeAnimationAssociationGroupAccess&>(runtime.access);
        std::array<NativeAnimationAssociationGroupReferenceToken, 3> tokens{};
        Hook::watched = fixture.image + P::GlobalRva;
        Hook::watchedBytes = 8;
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
                catch (const std::exception& e)
                {
                    std::fprintf(stderr, "Association group action %s refused: %s\n", label, e.what());
                    Check(false, label);
                }
            };
            Check(service.Renew(generation), "renew actual owner for each bounded current group observation");
            fixture.invokeTimer();
            callback.action = [](NativeGameTimingAccess&) {};
        };
        const auto unchanged = [&]
        {
            return !std::memcmp(expected.data(), allocation, bytes) && !std::memcmp(nativeBlock.data(), reinterpret_cast<void*>(block), nativeBlock.size()) &&
                   !host.mallocCalls && !host.freeCalls && !runtime.customSequences.IsRetained() && !runtime.privateHierarchies.IsRetained();
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
        const auto same = [](const auto& a, const auto& b) { return a.ownerGeneration == b.ownerGeneration && a.index == b.index && a.locator == b.locator; };
        const auto unchangedValues = [](const auto& v) { return v.interfaceAddress == 71 && v.numAnimations == 73 && v.groupId == 79; };
        for (std::uint32_t i = 0; i != tokens.size(); ++i)
        {
            action("actual qualified current row token",
                   [&]
                   {
                       Check(access.ResolveAnimationAssociationGroup(generation, i, tokens[i]) == ClockResult::Ok && tokens[i].ownerGeneration == generation &&
                                 tokens[i].index == i && tokens[i].locator == rows + i * 32 && unchanged(),
                             "actual runtime stamps owner only after original-cookie/current-row resolution");
                   });
        }
        AnimationAssociationGroupReceiver empty(access, tokens[0]), created(access, tokens[1]), negative(access, tokens[2]);
        for (std::size_t i = 0; i != tokens.size(); ++i)
        {
            action("actual source created predicate",
                   [&]
                   {
                       AnimationAssociationGroupReceiver receiver(access, tokens[i]);
                       Check(receiver.IsCreated() == (nums[i] != 0) && unchanged(),
                             "real SDK Created uses rawNumDWORD nonzero including negative representation");
                   });
        }
        action("actual signed count and group fields",
               [&]
               {
                   Check(negative.GetNumAnimations() == (std::numeric_limits<int>::min)() && static_cast<int>(created.GetGroupID()) == 3 && unchanged(),
                         "real SDK preserves signed count and exact groupDWORD18 without substituting index or logical extent");
               });
        action("actual opaque interface only",
               [&]
               {
                   Check(reinterpret_cast<std::uintptr_t>(created.GetInterface()) == tokens[1].locator && unchanged(),
                         "actual GetInterface borrows exact current native row without SA layout/ownership claim");
               });
        action("actual typed loaded null and raw byte",
               [&]
               {
                   Check(!empty.IsLoaded() && created.IsLoaded() && unchanged(),
                         "typed IsLoaded returns null-block false and rawBYTE10 nonzero with no NUL-name requirement");
               });
        action("Loaded has no Created prerequisite",
               [&]
               {
                   std::memcpy(reinterpret_cast<void*>(rows), &block, 8);
                   const bool           loaded = empty.IsLoaded();
                   const std::uintptr_t null = 0;
                   std::memcpy(reinterpret_cast<void*>(rows), &null, 8);
                   Check(loaded && unchanged(), "a genuine current block remains observable while rawNumAnimations is zero");
               });
        action("scalar channel does not borrow block",
               [&]
               {
                   const std::uintptr_t foreign = rows;
                   std::memcpy(reinterpret_cast<void*>(rows + 32), &foreign, 8);
                   NativeAnimationAssociationGroupValues values;
                   const auto                            scalar = access.ReadAnimationAssociationGroup(tokens[1], values);
                   bool                                  loaded = true;
                   const auto                            coupled = access.ReadAnimationAssociationGroupLoaded(tokens[1], loaded);
                   std::memcpy(reinterpret_cast<void*>(rows + 32), &block, 8);
                   Check(scalar == ClockResult::Ok && values.numAnimations == 1 && values.groupId == 3 && coupled == ClockResult::BindingChanged && loaded &&
                             unchanged(),
                         "raw scalar source remains independent while nonnull arbitrary block cannot be adopted by loaded channel");
               });
        action("locator generation and full index refusals",
               [&]
               {
                   NativeAnimationAssociationGroupValues values{71, 73, 79};
                   auto                                  wrong = tokens[1];
                   wrong.locator += 32;
                   const auto locator = access.ReadAnimationAssociationGroup(wrong, values);
                   wrong = tokens[1];
                   ++wrong.ownerGeneration;
                   const auto                                    owner = access.ReadAnimationAssociationGroup(wrong, values);
                   NativeAnimationAssociationGroupReferenceToken output{71, 73, 79}, sentinel = output;
                   const auto                                    index = access.ResolveAnimationAssociationGroup(generation, 0xffffffffu, output);
                   Check(locator == ClockResult::BindingChanged && owner == ClockResult::OwnerConflict && index == ClockResult::InvalidArgument &&
                             unchangedValues(values) && same(output, sentinel) && unchanged(),
                         "token row cannot redirect, stale owner cannot publish, full index cannot narrow");
               });
        action("current shutdown null invalidates prior token",
               [&]
               {
                   fixture.Put(P::GlobalRva, std::uintptr_t{0});
                   NativeAnimationAssociationGroupValues values{71, 73, 79};
                   const auto                            result = access.ReadAnimationAssociationGroup(tokens[1], values);
                   fixture.Put(P::GlobalRva, rows);
                   Check(result == ClockResult::NotInitialized && unchangedValues(values) && unchanged(),
                         "old locator alone cannot survive actual current-global retirement");
               });
        action("caller token copied before callbacks",
               [&]
               {
                   auto source = tokens[1];
                   callerToken = &source;
                   Hook::afterRead = &ChangeCaller;
                   NativeAnimationAssociationGroupValues values;
                   const auto                            result = access.ReadAnimationAssociationGroup(source, values);
                   Hook::afterRead = nullptr;
                   callerToken = nullptr;
                   Check(result == ClockResult::Ok && values.interfaceAddress == tokens[1].locator && values.numAnimations == 1 && !source.ownerGeneration &&
                             unchanged(),
                         "real owner captures complete caller token before actual observation callback mutates its source");
               });
        for (transition = 0; transition != 3; ++transition)
        {
            action("coherent scalar/block bookends",
                   [&]
                   {
                       Hook::reads = 0;
                       changed = false;
                       Hook::afterRead = &ChangeAfterPayload;
                       NativeAnimationAssociationGroupValues values{71, 73, 79};
                       bool                                  loaded = true;
                       const auto                            result =
                           transition ? access.ReadAnimationAssociationGroupLoaded(tokens[1], loaded) : access.ReadAnimationAssociationGroup(tokens[1], values);
                       Hook::afterRead = nullptr;
                       std::memcpy(reinterpret_cast<void*>(rows + 32 + 0x18), &groups[1], 4);
                       std::memcpy(reinterpret_cast<void*>(block + B::LoadedOffset), nativeBlock.data() + B::LoadedOffset, 1);
                       std::memcpy(reinterpret_cast<void*>(rows + 32), &block, 8);
                       Check(changed && result == ClockResult::BindingChanged && (transition ? loaded : unchangedValues(values)) && unchanged(),
                             "torn groupID/blockloaded/parentblock observations cannot publish copied values");
                   });
        }
        action("first native global read failure",
               [&]
               {
                   Hook::failNext = true;
                   NativeAnimationAssociationGroupValues values{71, 73, 79};
                   Check(access.ReadAnimationAssociationGroup(tokens[1], values) == ClockResult::ReadFailure && unchangedValues(values) &&
                             !runtime.stopping.load(),
                         "read refusal preserves output and grants no retained operation debt");
               });
        action("final owner bookend",
               [&]
               {
                   const auto previous = timingCallbackGeneration;
                   Hook::reads = 0;
                   changed = false;
                   Hook::afterRead = &LoseFinalOwner;
                   NativeAnimationAssociationGroupValues values{71, 73, 79};
                   const auto                            result = access.ReadAnimationAssociationGroup(tokens[1], values);
                   Hook::afterRead = nullptr;
                   timingCallbackGeneration = previous;
                   Check(changed && result == ClockResult::OwnerConflict && unchangedValues(values) && unchanged(),
                         "final current owner loss cannot publish an old native row value");
               });
        Check(refuses([&] { static_cast<void>(created.GetInterface()); }) && unchanged(), "opaque group interface still requires actual callback admission");
        Hook::afterRead = nullptr;
        Hook::failNext = false;
        Hook::watched = 0;
        Hook::watchedBytes = sizeof(std::int32_t);
        WorldGroundOwnerFixture::Retire(service, fixture);
        Check(unchanged(), "readonly group End never retires/frees engine-owned table or block bank");
        fixture.Put(P::GlobalRva, std::uintptr_t{0});
        fixture.Put(P::CountRva, std::int32_t{0});
        Check(VirtualFree(allocation, 0, MEM_RELEASE) != FALSE, "release only task-owned fixture buffer after the real native observer retires");
        rows = block = 0;
        Host::active = nullptr;
        ClockReaderTestHost::image = previousImage;
        saved.Restore();
        std::puts("Production animation association group owner fixture completed");
        std::puts("Production actual animation association group SDK fixture completed");
    }
}
