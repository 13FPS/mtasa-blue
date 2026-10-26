/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual persistent static WORD setters over qualified borrowed rows
 ****************************************************************************/
namespace ClockAnimationStaticAssociationWriteTest
{
    bool                                           enabled = false;
    unsigned                                       mode = 0, calls = 0;
    NativeAnimationStaticAssociationReferenceToken token{};
    std::uintptr_t                                 parent = 0, replacementChild = 0;
    ClockResult                                    nested = ClockResult::Ok;
    bool                                           lastAccepted = false;
    std::uint16_t                                  lastObserved = 0;
    ClockResult Exchange(std::uintptr_t address, std::uint16_t expected, std::uint16_t target, std::uint16_t& observed, bool& accepted,
                         std::uint32_t& exception) noexcept
    {
        using mta::game_de::AnimationStaticAssociationWriteDetail::ExchangeWord;
        if (!enabled)
            return ExchangeWord(address, expected, target, observed, accepted, exception);
        ++calls;
        if (mode == 1)
        {
            const std::uint16_t competitor = 0x55aau;
            std::memcpy(reinterpret_cast<void*>(address), &competitor, 2);
        }
        if (mode == 4)
            nested = runtime.access.SetAnimationStaticAssociationFlags(token, 7);
        const auto result = ExchangeWord(address, expected, target, observed, accepted, exception);
        lastAccepted = accepted;
        lastObserved = observed;
        if (result != ClockResult::Ok)
            return result;
        if (mode == 2)
            ++timingCallbackGeneration;
        if (mode == 3)
        {
            // Real store returned, but its acknowledgement is unknown to the
            // primitive; the current parent has meanwhile changed origin.
            std::memcpy(reinterpret_cast<void*>(parent + 8), &replacementChild, 8);
            accepted = false;
            exception = 0xc0000005u;
            return ClockResult::OriginalException;
        }
        return ClockResult::Ok;
    }
}
namespace AnimationStaticAssociationWritesOwnerFixture
{
    namespace Host = NativeCustomAnimSequenceStorageFixture;
    namespace ReadHook = ClockAnimationManagerOwnerTest;
    namespace WriteHook = ClockAnimationStaticAssociationWriteTest;
    unsigned                                        scenario = 0;
    std::uint64_t*                                  callerGeneration = nullptr;
    NativeAnimationStaticAssociationReferenceToken* callerToken = nullptr;
    std::int16_t*                                   callerValue = nullptr;
    std::uintptr_t                                  parentBase = 0;
    bool                                            changed = false;
    void                                            ChangeCaller() noexcept
    {
        callerToken->ownerGeneration = 0;
        callerToken->index = (std::numeric_limits<std::size_t>::max)();
        *callerValue = 7;
    }
    void ChangeCookie() noexcept
    {
        if (ReadHook::reads < 3)
        {
            ReadHook::afterRead = &ChangeCookie;
            return;
        }
        const std::uint64_t cap = 1;
        std::memcpy(reinterpret_cast<void*>(parentBase), &cap, 8);
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
        namespace A = NativeAnimationStaticAssociationProfile;
        using Field = NativeAnimationManagerMemory::StaticAssociationScalar;
        using Receipt = NativeAnimationManagerMemory::StaticAssociationWriteReceipt;
        for (const auto& pin : P::Code)
            WorldGroundOwnerFixture::Writes::Apply(&saved, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : A::ReadCode)
            WorldGroundOwnerFixture::Writes::Apply(&saved, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        constexpr std::uint64_t outerCapacity = 2, childCapacity = 3;
        constexpr std::size_t   outerBytes = 8 + outerCapacity * 32, childBytes = 8 + childCapacity * 32, nodeBytes = std::size_t{0x7fff} * 8;
        void*                   outerAllocation = VirtualAlloc(nullptr, outerBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        void*                   childAllocation = VirtualAlloc(nullptr, childBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        void*                   replacementAllocation = VirtualAlloc(nullptr, childBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        void*                   nodeAllocation = VirtualAlloc(nullptr, nodeBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        Check(outerAllocation && childAllocation && replacementAllocation && nodeAllocation,
              "own only task buffers representing existing engine allocations, never issue engine storage");
        parentBase = reinterpret_cast<std::uintptr_t>(outerAllocation);
        const auto rows = parentBase + 8;
        const auto child = reinterpret_cast<std::uintptr_t>(childAllocation) + 8;
        const auto replacement = reinterpret_cast<std::uintptr_t>(replacementAllocation) + 8;
        const auto nodes = reinterpret_cast<std::uintptr_t>(nodeAllocation);
        std::memset(outerAllocation, 0xa5, outerBytes);
        std::memset(childAllocation, 0xa5, childBytes);
        std::memset(replacementAllocation, 0x5c, childBytes);
        std::memset(nodeAllocation, 0x3a, nodeBytes);
        std::memcpy(outerAllocation, &outerCapacity, 8);
        std::memcpy(childAllocation, &childCapacity, 8);
        std::memcpy(replacementAllocation, &childCapacity, 8);
        const std::uint32_t zero = 0;
        std::memcpy(reinterpret_cast<void*>(rows + 8), &child, 8);
        std::memcpy(reinterpret_cast<void*>(rows + 0x10), &zero, 4);
        std::memcpy(reinterpret_cast<void*>(rows + 0x14), &zero, 4);
        const std::array<std::uint16_t, 4> initialWords{4, 2, 3, 0};
        const std::uintptr_t               hierarchy = 0x76543210;
        for (std::size_t i = 0; i != childCapacity; ++i)
        {
            const auto row = child + i * 32;
            std::memcpy(reinterpret_cast<void*>(row + 8), initialWords.data(), 8);
            std::memcpy(reinterpret_cast<void*>(row + 0x10), &nodes, 8);
            std::memcpy(reinterpret_cast<void*>(row + 0x18), &hierarchy, 8);
        }
        saved.Global(P::GlobalRva, rows);
        saved.Global(P::CountRva, std::int32_t{1});
        std::array<unsigned char, outerBytes> outerExpected{};
        std::array<unsigned char, childBytes> childExpected{}, replacementExpected{};
        std::vector<unsigned char>            nodeExpected(nodeBytes);
        std::memcpy(outerExpected.data(), outerAllocation, outerBytes);
        std::memcpy(childExpected.data(), childAllocation, childBytes);
        std::memcpy(replacementExpected.data(), replacementAllocation, childBytes);
        std::memcpy(nodeExpected.data(), nodeAllocation, nodeBytes);
        const auto                                     generation = runtime.generation.load();
        auto&                                          access = static_cast<NativeAnimationStaticAssociationAccess&>(runtime.access);
        NativeAnimationStaticAssociationReferenceToken token;
        const auto                                     action = [&](const char* label, const auto& body)
        {
            callback.action = [&](NativeGameTimingAccess&)
            {
                try
                {
                    body();
                }
                catch (const std::exception& error)
                {
                    std::fprintf(stderr, "Static association write %s refused: %s\n", label, error.what());
                    Check(false, label);
                }
            };
            Check(service.Renew(generation), "renew the actual writer for each bounded persistent setter action");
            fixture.invokeTimer();
            callback.action = [](NativeGameTimingAccess&) {};
        };
        const auto intact = [&]
        {
            return !std::memcmp(outerExpected.data(), outerAllocation, outerBytes) && !std::memcmp(childExpected.data(), childAllocation, childBytes) &&
                   !std::memcmp(replacementExpected.data(), replacementAllocation, childBytes) &&
                   !std::memcmp(nodeExpected.data(), nodeAllocation, nodeBytes) && !host.mallocCalls && !host.freeCalls &&
                   !runtime.customSequences.IsRetained() && !runtime.privateHierarchies.IsRetained();
        };
        action("current physical writer token",
               [&]
               {
                   Check(access.ResolveAnimationStaticAssociation(generation, 0, 1, token) == ClockResult::Ok && token.locator == child + 32,
                         "actual writer resolves current original child allocation even while logical Created count is zero");
               });
        AnimationStaticAssociationReceiver receiver(access, token);
        WriteHook::token = token;
        WriteHook::parent = rows;
        WriteHook::replacementChild = replacement;
        WriteHook::enabled = true;
        WriteHook::mode = 0;
        WriteHook::calls = 0;
        ReadHook::watched = fixture.image + P::GlobalRva;
        ReadHook::watchedBytes = 8;
        ReadHook::reads = 0;
        ReadHook::afterRead = nullptr;
        ReadHook::failNext = false;
        const auto expectedWord = [&](std::size_t offset, std::uint16_t bits) { std::memcpy(childExpected.data() + 8 + 32 + offset, &bits, 2); };
        const auto seedWord = [&](std::size_t offset, std::uint16_t bits)
        {
            std::memcpy(reinterpret_cast<void*>(token.locator + offset), &bits, 2);
            expectedWord(offset, bits);
        };
        const auto seedNodes = [&](std::uintptr_t pointer)
        {
            std::memcpy(reinterpret_cast<void*>(token.locator + 0x10), &pointer, 8);
            std::memcpy(childExpected.data() + 8 + 32 + 0x10, &pointer, 8);
        };
        const auto word = [&](std::size_t offset)
        {
            std::uint16_t bits = 0;
            std::memcpy(&bits, reinterpret_cast<void*>(token.locator + offset), 2);
            return bits;
        };
        if (scenario == 0)
        {
            for (unsigned field = 1; field != 4; ++field)
            {
                for (const auto bits : {std::uint16_t{0}, std::uint16_t{0x7fff}, std::uint16_t{0x8000}, std::uint16_t{0xffff}})
                {
                    action("all signed source WORD bits",
                           [&]
                           {
                               std::int16_t value = 0;
                               std::memcpy(&value, &bits, 2);
                               const auto calls = WriteHook::calls;
                               if (field == 1)
                                   receiver.SetAnimID(value);
                               if (field == 2)
                                   receiver.SetAnimGroup(value);
                               if (field == 3)
                                   receiver.SetFlags(value);
                               expectedWord(8 + field * 2, bits);
                               Check(WriteHook::calls == calls + 1 && WriteHook::lastAccepted && !runtime.stopping.load() && intact(),
                                     "actual SDK setter preserves every signed bit pattern with one real16CAS and no neighbor/pointer/payload changes");
                           });
                }
            }
            action("unsigned count shrinks without node ownership",
                   [&]
                   {
                       const auto calls = WriteHook::calls;
                       receiver.SetNumBlendNodes(3);
                       expectedWord(8, 3);
                       Check(word(8) == 3 && WriteHook::calls == calls + 1 && intact(),
                             "real count setter admits only non-growing fresh count without allocation/free or payload rewrite");
                   });
            action("growth and full16 request refuse before entry",
                   [&]
                   {
                       const auto calls = WriteHook::calls;
                       const auto growth = access.SetAnimationStaticAssociationNumBlendNodes(token, 4);
                       const auto high = access.SetAnimationStaticAssociationNumBlendNodes(token, 0xffffu);
                       Check(growth == ClockResult::InvalidArgument && high == ClockResult::InvalidArgument && WriteHook::calls == calls && word(8) == 3 &&
                                 !runtime.stopping.load() && intact(),
                             "valid virtual pages never masquerade as node allocation capacity; owner refuses without clamp or poisoning");
                   });
            action("signed-positive maximum count",
                   [&]
                   {
                       seedWord(8, 0x7fffu);
                       const auto calls = WriteHook::calls;
                       receiver.SetNumBlendNodes(0x7fffu);
                       expectedWord(8, 0x7fffu);
                       Check(word(8) == 0x7fffu && WriteHook::calls == calls + 1 && intact(),
                             "max signed-positive native count is admitted against the current positive node span");
                       seedWord(8, 4);
                   });
            action("zero count has no node capacity predicate",
                   [&]
                   {
                       seedWord(8, 0);
                       seedNodes(0x1234);
                       const auto calls = WriteHook::calls;
                       receiver.SetNumBlendNodes(0);
                       expectedWord(8, 0);
                       Check(WriteHook::calls == calls + 1 && word(8) == 0 && intact(),
                             "current zero accepts zero without dereferencing arbitrary node pointer or inventing a payload extent");
                       seedNodes(nodes);
                       seedWord(8, 4);
                   });
            action("high-bit current count",
                   [&]
                   {
                       const auto calls = WriteHook::calls;
                       seedWord(8, 0x8000u);
                       const auto result = access.SetAnimationStaticAssociationNumBlendNodes(token, 0);
                       seedWord(8, 4);
                       Check(result == ClockResult::InvalidArgument && WriteHook::calls == calls && !runtime.stopping.load() && intact(),
                             "a high-bit current count cannot be treated as a safe positive node extent");
                   });
            for (const auto pointer : {fixture.image + NativeAnimationBlockProfile::BankRva, child})
            {
                action("invalid current positive node range",
                       [&]
                       {
                           const auto calls = WriteHook::calls;
                           seedNodes(pointer);
                           const auto result = access.SetAnimationStaticAssociationNumBlendNodes(token, 1);
                           seedNodes(nodes);
                           Check(result == ClockResult::BindingChanged && WriteHook::calls == calls && !runtime.stopping.load() && intact(),
                                 "image/child-alias pages cannot replace actual current node storage or become a capacity lease");
                       });
            }
            action("direct real primitive receipt",
                   [&]
                   {
                       Receipt    receipt;
                       const auto before = word(0x0e);
                       const auto calls = WriteHook::calls;
                       const auto result =
                           NativeAnimationManagerMemory({fixture.image}, &CheckTimingOwner).WriteStaticAssociation(token, Field::Flags, 0x8001u, &receipt);
                       expectedWord(0x0e, 0x8001u);
                       Check(result == ClockResult::Ok && receipt.entered && receipt.accepted && receipt.verified && receipt.before == before &&
                                 receipt.target == 0x8001u && receipt.observed == before && receipt.address == token.locator + 0x0e &&
                                 WriteHook::calls == calls + 1 && intact(),
                             "actual primitive exposes exact entered/accepted/verified evidence for the sole persistent WORD exchange");
                   });
            action("copied caller token and value",
                   [&]
                   {
                       auto         source = token;
                       std::int16_t value = (std::numeric_limits<std::int16_t>::min)();
                       callerToken = &source;
                       callerValue = &value;
                       ReadHook::afterRead = &ChangeCaller;
                       const auto result = access.SetAnimationStaticAssociationAnimID(source, value);
                       ReadHook::afterRead = nullptr;
                       callerToken = nullptr;
                       callerValue = nullptr;
                       expectedWord(0x0a, 0x8000u);
                       Check(result == ClockResult::Ok && !source.ownerGeneration && value == 7 && word(0x0a) == 0x8000u && intact(),
                             "real provider copies entire physical intent before source objects change in observation callbacks");
                   });
            action("reentrant setter under real writer guard",
                   [&]
                   {
                       const auto flags = word(0x0e);
                       const auto calls = WriteHook::calls;
                       WriteHook::mode = 4;
                       WriteHook::nested = ClockResult::Ok;
                       receiver.SetAnimID(9);
                       WriteHook::mode = 0;
                       expectedWord(0x0a, 9);
                       Check(WriteHook::nested == ClockResult::Busy && word(0x0e) == flags && WriteHook::calls == calls + 1 && !runtime.stopping.load() &&
                                 intact(),
                             "actual reentrant writer refuses before entry while the original persistent setter completes once");
                   });
            action("actual GT and GC refusal",
                   [&]
                   {
                       const auto  calls = WriteHook::calls;
                       ClockResult foreign = ClockResult::Ok, nativeThread = ClockResult::Ok;
                       std::thread worker(
                           [&]
                           {
                               foreign = access.SetAnimationStaticAssociationFlags(token, 7);
                               nativeThread = NativeStartGameCalls::CheckThread(fixture.image);
                           });
                       worker.join();
                       fixture.Put(NativeBorrowedGcProfile::markerRva, LONG{1});
                       const auto gc = access.SetAnimationStaticAssociationFlags(token, 7);
                       fixture.Put(NativeBorrowedGcProfile::markerRva, LONG{0});
                       // This timing-only owner leaves the legacy ready bit
                       // false. A foreign thread has no timing callback TLS,
                       // so the runtime rejects admission before its GT test.
                       // Check the native GT guard independently as well.
                       if (foreign != ClockResult::NotInitialized || nativeThread != ClockResult::WrongThread || gc != ClockResult::Busy)
                           std::fprintf(stderr, "Static write guards: runtime=%u nativeThread=%u gc=%u ready=%u timing=%u calls=%u/%u\n",
                                        static_cast<unsigned>(foreign), static_cast<unsigned>(nativeThread), static_cast<unsigned>(gc),
                                        static_cast<unsigned>(runtime.ready.load()), static_cast<unsigned>(runtime.timingMode), WriteHook::calls, calls);
                       Check(!runtime.ready.load() && runtime.timingMode && foreign == ClockResult::NotInitialized &&
                                 nativeThread == ClockResult::WrongThread && gc == ClockResult::Busy && WriteHook::calls == calls && !runtime.stopping.load() &&
                                 intact(),
                             "missing callback admission, foreign native GT and active GC all refuse without CAS or neighbor mutation");
                   });
            action("fresh parent-child-cookie qualification",
                   [&]
                   {
                       const auto calls = WriteHook::calls;
                       auto       wrong = token;
                       ++wrong.ownerGeneration;
                       const auto owner = access.SetAnimationStaticAssociationFlags(wrong, 7);
                       wrong = token;
                       wrong.locator += 32;
                       const auto           locator = access.SetAnimationStaticAssociationFlags(wrong, 7);
                       const std::uintptr_t null = 0;
                       std::memcpy(reinterpret_cast<void*>(rows + 8), &null, 8);
                       const auto childMissing = access.SetAnimationStaticAssociationFlags(token, 7);
                       std::memcpy(reinterpret_cast<void*>(rows + 8), &child, 8);
                       const auto corrupt = (std::numeric_limits<std::uint64_t>::max)();
                       std::memcpy(childAllocation, &corrupt, 8);
                       const auto cookie = access.SetAnimationStaticAssociationFlags(token, 7);
                       std::memcpy(childAllocation, &childCapacity, 8);
                       Check(owner == ClockResult::OwnerConflict && locator == ClockResult::BindingChanged && childMissing == ClockResult::NotInitialized &&
                                 cookie == ClockResult::BindingChanged && WriteHook::calls == calls && !runtime.stopping.load() && intact(),
                             "each actual mutation reacquires current origin/cookies before any persistent effect");
                   });
            action("pre-entry cookie transition and read failure",
                   [&]
                   {
                       const auto calls = WriteHook::calls;
                       ReadHook::reads = 0;
                       changed = false;
                       ReadHook::afterRead = &ChangeCookie;
                       const auto transition = access.SetAnimationStaticAssociationFlags(token, 7);
                       ReadHook::afterRead = nullptr;
                       std::memcpy(outerAllocation, &outerCapacity, 8);
                       ReadHook::failNext = true;
                       const auto fault = access.SetAnimationStaticAssociationFlags(token, 7);
                       ReadHook::failNext = false;
                       Check(changed && transition == ClockResult::BindingChanged && fault == ClockResult::ReadFailure && WriteHook::calls == calls &&
                                 !runtime.stopping.load() && intact(),
                             "torn parent extent/read loss refuses before CAS and preserves every native neighbor");
                   });
            action("invalid native field is not a byte-offset capability",
                   [&]
                   {
                       const auto calls = WriteHook::calls;
                       Receipt    receipt;
                       const auto result =
                           NativeAnimationManagerMemory({fixture.image}, &CheckTimingOwner).WriteStaticAssociation(token, static_cast<Field>(255), 7, &receipt);
                       Check(result == ClockResult::InvalidArgument && !receipt.entered && WriteHook::calls == calls && intact(),
                             "private field enum cannot become arbitrary native-memory write authority");
                   });
        }
        else
        {
            Check(scenario <= 3, "only the agreed isolated entered-failure writer scenarios are accepted");
            WriteHook::mode = scenario;
            const auto  oldGeneration = timingCallbackGeneration;
            const auto  calls = WriteHook::calls;
            ClockResult result = ClockResult::Ok;
            action("one actual isolated persistent write failure",
                   [&]
                   {
                       result = access.SetAnimationStaticAssociationAnimID(token, 0x1234);
                       if (scenario == 1)
                           expectedWord(0x0a, 0x55aau);
                       else
                           expectedWord(0x0a, 0x1234u);
                       if (scenario == 3)
                           std::memcpy(outerExpected.data() + 8 + 8, &replacement, 8);
                       Check(WriteHook::calls == calls + 1 && runtime.stopping.load() && intact(),
                             "entered refusal stops the real owner and never retries/restores wrong-origin storage");
                       if (scenario == 1)
                           Check(result == ClockResult::OwnerConflict && !WriteHook::lastAccepted && WriteHook::lastObserved == 0x55aau,
                                 "actual16CAS contention observes competitor WORD without overwriting or retrying it");
                       if (scenario == 2)
                           Check(result == ClockResult::OwnerConflict && WriteHook::lastAccepted,
                                 "accepted store survives lost current-owner acknowledgement with no rollback");
                       if (scenario == 3)
                           Check(result == ClockResult::OriginalException && WriteHook::lastAccepted,
                                 "entered unknown acknowledgement retains actual effect and new-origin parent without unsafe undo");
                       const auto repeated = access.SetAnimationStaticAssociationAnimID(token, 9);
                       Check(repeated != ClockResult::Ok && WriteHook::calls == calls + 1 && intact(),
                             "stopped owner cannot replay an entered persistent scalar setter");
                   });
            timingCallbackGeneration = oldGeneration;
            WriteHook::mode = 0;
        }
        WriteHook::enabled = false;
        ReadHook::afterRead = nullptr;
        ReadHook::failNext = false;
        ReadHook::watched = 0;
        ReadHook::watchedBytes = sizeof(std::int32_t);
        WorldGroundOwnerFixture::Retire(service, fixture);
        Check(intact(), "End restores owner hooks without rolling back persistent WORDs, freeing node payloads, or touching replacement origins");
        fixture.Put(P::GlobalRva, std::uintptr_t{0});
        fixture.Put(P::CountRva, std::int32_t{0});
        Check(VirtualFree(nodeAllocation, 0, MEM_RELEASE) != FALSE && VirtualFree(replacementAllocation, 0, MEM_RELEASE) != FALSE &&
                  VirtualFree(childAllocation, 0, MEM_RELEASE) != FALSE && VirtualFree(outerAllocation, 0, MEM_RELEASE) != FALSE,
              "release only task fixture buffers after actual owner retirement; production never owns or frees these native borrows");
        WriteHook::parent = WriteHook::replacementChild = 0;
        parentBase = 0;
        Host::active = nullptr;
        ClockReaderTestHost::image = previousImage;
        saved.Restore();
        if (scenario == 0)
        {
            std::puts("Production static association scalar writes owner fixture completed");
            std::puts("Production actual static association scalar SDK fixture completed");
        }
        if (scenario == 1)
            std::puts("Production static association scalar conflict stop fixture completed");
        if (scenario == 2)
            std::puts("Production static association scalar lost acknowledgement fixture completed");
        if (scenario == 3)
            std::puts("Production static association scalar unknown write fixture completed");
    }
}
