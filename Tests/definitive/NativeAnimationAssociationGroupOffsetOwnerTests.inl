/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual group offset writes, physical-row identity and stop semantics
 ****************************************************************************/
namespace ClockAnimationAssociationGroupOffsetTest
{
    bool                                          enabled = false;
    unsigned                                      mode = 0, calls = 0;
    NativeAnimationAssociationGroupReferenceToken token{};
    ClockResult                                   nested = ClockResult::Ok;
    ClockResult Exchange(std::uintptr_t address, std::uint32_t expected, std::uint32_t target, std::uint32_t& observed, bool& accepted,
                         std::uint32_t& exception) noexcept
    {
        using mta::game_de::AnimationAssociationGroupWriteDetail::ExchangeDword;
        if (!enabled)
            return ExchangeDword(address, expected, target, observed, accepted, exception);
        ++calls;
        if (mode == 2)
        {
            const std::uint32_t competitor = 0x55aa1234u;
            std::memcpy(reinterpret_cast<void*>(address), &competitor, sizeof(competitor));
        }
        if (mode == 3)
            nested = runtime.access.SetAnimationAssociationGroupIdOffset(token, 7);
        const auto result = ExchangeDword(address, expected, target, observed, accepted, exception);
        if (result == ClockResult::Ok && mode == 1)
            ++timingCallbackGeneration;
        return result;
    }
}
namespace AnimationAssociationGroupOffsetOwnerFixture
{
    namespace Host = NativeCustomAnimSequenceStorageFixture;
    namespace Hook = ClockAnimationManagerOwnerTest;
    namespace WriteHook = ClockAnimationAssociationGroupOffsetTest;
    unsigned                                       scenario = 0;
    NativeAnimationAssociationGroupReferenceToken* callerToken = nullptr;
    std::int32_t*                                  callerValue = nullptr;
    void                                           ChangeCaller() noexcept
    {
        callerToken->ownerGeneration = 0;
        callerToken->index = 0xffffffffu;
        callerToken->locator = 1;
        *callerValue = 7;
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
        using Receipt = NativeAnimationManagerMemory::AssociationGroupOffsetWriteReceipt;
        for (const auto& pin : P::Code)
            WorldGroundOwnerFixture::Writes::Apply(&saved, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : A::Code)
            WorldGroundOwnerFixture::Writes::Apply(&saved, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : A::ReadCode)
            WorldGroundOwnerFixture::Writes::Apply(&saved, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        constexpr std::uint64_t capacity = 2, childCapacity = 3;
        constexpr std::size_t   outerBytes = 8 + capacity * 32, childBytes = 8 + childCapacity * 32;
        void*                   allocation = VirtualAlloc(nullptr, outerBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        void*                   childAllocation = VirtualAlloc(nullptr, childBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        Check(allocation && childAllocation, "own only fixture buffers representing existing native group and static rows");
        const auto rows = reinterpret_cast<std::uintptr_t>(allocation) + 8;
        const auto child = reinterpret_cast<std::uintptr_t>(childAllocation) + 8;
        const auto row = rows + 32;
        std::memset(allocation, 0xa5, outerBytes);
        std::memset(childAllocation, 0, childBytes);
        std::memcpy(allocation, &capacity, 8);
        std::memcpy(childAllocation, &childCapacity, 8);
        const std::uint32_t count = 1, initialOffset = 0, groupId = 0x80000003u;
        std::memcpy(reinterpret_cast<void*>(row + 8), &child, 8);
        std::memcpy(reinterpret_cast<void*>(row + 0x10), &count, 4);
        std::memcpy(reinterpret_cast<void*>(row + 0x14), &initialOffset, 4);
        std::memcpy(reinterpret_cast<void*>(row + 0x18), &groupId, 4);
        const std::int16_t sourceId = 17;
        std::memcpy(reinterpret_cast<void*>(child + 32 + A::IdOffset), &sourceId, 2);
        saved.Global(P::GlobalRva, rows);
        saved.Global(P::CountRva, std::int32_t{2});
        std::array<unsigned char, outerBytes> expected{};
        std::array<unsigned char, childBytes> expectedChild{};
        std::memcpy(expected.data(), allocation, outerBytes);
        std::memcpy(expectedChild.data(), childAllocation, childBytes);
        const auto                                     generation = runtime.generation.load();
        auto&                                          access = static_cast<NativeAnimationAssociationGroupAccess&>(runtime.access);
        auto&                                          staticAccess = static_cast<NativeAnimationStaticAssociationAccess&>(runtime.access);
        NativeAnimationAssociationGroupReferenceToken  token;
        NativeAnimationStaticAssociationReferenceToken physical;
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
                    std::fprintf(stderr, "Group offset %s refused: %s\n", label, error.what());
                    Check(false, label);
                }
            };
            Check(service.Renew(generation), "renew the actual owner for each bounded offset write");
            fixture.invokeTimer();
            callback.action = [](NativeGameTimingAccess&) {};
        };
        const auto intact = [&]
        {
            return !std::memcmp(expected.data(), allocation, outerBytes) && !std::memcmp(expectedChild.data(), childAllocation, childBytes) &&
                   !host.mallocCalls && !host.freeCalls && !runtime.customSequences.IsRetained() && !runtime.privateHierarchies.IsRetained();
        };
        const auto expectOffset = [&](std::uint32_t value) { std::memcpy(expected.data() + 8 + 32 + 0x14, &value, 4); };
        const auto offset = [&]
        {
            std::uint32_t value = 0;
            std::memcpy(&value, reinterpret_cast<void*>(row + 0x14), 4);
            return value;
        };
        action("current same-owner row",
               [&]
               {
                   Check(access.ResolveAnimationAssociationGroup(generation, 1, token) == ClockResult::Ok && token.locator == row &&
                             staticAccess.ResolveAnimationStaticAssociation(generation, 1, 1, physical) == ClockResult::Ok && physical.locator == child + 32 &&
                             intact(),
                         "actual runtime resolves current array row and independent physical static-row token");
               });
        AnimationAssociationGroupReceiver receiver(access, token);
        WriteHook::enabled = true;
        WriteHook::mode = 0;
        WriteHook::calls = 0;
        WriteHook::token = token;
        Hook::watched = fixture.image + P::GlobalRva;
        Hook::watchedBytes = 8;
        Hook::reads = 0;
        Hook::afterRead = nullptr;
        Hook::failNext = false;
        if (scenario == 0)
        {
            for (const auto bits : {std::uint32_t{0}, std::uint32_t{0x7fffffffu}, std::uint32_t{0x80000000u}, std::uint32_t{0xffffffffu}})
            {
                action("all signed32 source bits",
                       [&]
                       {
                           std::int32_t value = 0;
                           std::memcpy(&value, &bits, 4);
                           const auto calls = WriteHook::calls;
                           receiver.SetIDOffset(value);
                           expectOffset(bits);
                           Check(offset() == bits && WriteHook::calls == calls + 1 && !runtime.stopping.load() && intact(),
                                 "actual SDK setter performs one guarded DWORD store and preserves every neighbor and child byte");
                       });
            }
            action("physical identity versus ID selector",
                   [&]
                   {
                       receiver.SetIDOffset(79);
                       expectOffset(79);
                       NativeAnimationStaticAssociationValues         value;
                       NativeAnimationStaticAssociationReferenceToken selected;
                       Check(staticAccess.ReadAnimationStaticAssociation(physical, value) == ClockResult::Ok && value.interfaceAddress == physical.locator &&
                                 value.animId == sourceId &&
                                 staticAccess.ResolveAnimationStaticAssociationById(generation, 1, 80, selected) == ClockResult::Ok &&
                                 selected.index == physical.index && selected.locator == physical.locator && intact(),
                             "persistent offset changes future modulo32 ID selection while issued physical tokens retain their original row");
                   });
            action("no child or Created admission invented",
                   [&]
                   {
                       const std::uint32_t  zero = 0;
                       const std::uintptr_t arbitrary = 1;
                       std::memcpy(reinterpret_cast<void*>(row + 8), &arbitrary, 8);
                       std::memcpy(reinterpret_cast<void*>(row + 0x10), &zero, 4);
                       const auto result = access.SetAnimationAssociationGroupIdOffset(token, -17);
                       std::memcpy(reinterpret_cast<void*>(row + 8), &child, 8);
                       std::memcpy(reinterpret_cast<void*>(row + 0x10), &count, 4);
                       expectOffset(0xffffffefu);
                       Check(result == ClockResult::Ok && offset() == 0xffffffefu && intact(),
                             "source scalar setter needs only its real parent row, not loaded state or arbitrary child payload dereference");
                   });
            action("entered accepted verified receipt",
                   [&]
                   {
                       Receipt    receipt;
                       const auto before = offset();
                       const auto calls = WriteHook::calls;
                       const auto result = NativeAnimationManagerMemory({fixture.image}, &CheckTimingOwner).WriteAssociationGroupIdOffset(token, 101, &receipt);
                       expectOffset(101);
                       Check(result == ClockResult::Ok && receipt.entered && receipt.accepted && receipt.verified && receipt.address == row + 0x14 &&
                                 receipt.before == before && receipt.target == 101 && receipt.observed == before && WriteHook::calls == calls + 1 && intact(),
                             "actual primitive records exact sole-exchange evidence without owning native allocation lifetime");
                   });
            action("copied complete caller intent",
                   [&]
                   {
                       auto         source = token;
                       std::int32_t value = (std::numeric_limits<std::int32_t>::min)();
                       callerToken = &source;
                       callerValue = &value;
                       Hook::afterRead = &ChangeCaller;
                       const auto result = access.SetAnimationAssociationGroupIdOffset(source, value);
                       Hook::afterRead = nullptr;
                       callerToken = nullptr;
                       callerValue = nullptr;
                       expectOffset(0x80000000u);
                       Check(result == ClockResult::Ok && !source.ownerGeneration && value == 7 && offset() == 0x80000000u && intact(),
                             "actual provider snapshots token and signed value before observation callbacks can alter caller storage");
                   });
            action("writer reentry is Busy",
                   [&]
                   {
                       const auto calls = WriteHook::calls;
                       WriteHook::mode = 3;
                       WriteHook::nested = ClockResult::Ok;
                       receiver.SetIDOffset(31);
                       WriteHook::mode = 0;
                       expectOffset(31);
                       Check(WriteHook::nested == ClockResult::Busy && WriteHook::calls == calls + 1 && !runtime.stopping.load() && intact(),
                             "nested writer cannot enter a second exchange or poison the accepted outer persistent store");
                   });
            action("fresh owner locator and array authority",
                   [&]
                   {
                       const auto calls = WriteHook::calls;
                       auto       wrong = token;
                       ++wrong.ownerGeneration;
                       const auto owner = access.SetAnimationAssociationGroupIdOffset(wrong, 7);
                       wrong = token;
                       wrong.locator -= 32;
                       const auto locator = access.SetAnimationAssociationGroupIdOffset(wrong, 7);
                       fixture.Put(P::GlobalRva, std::uintptr_t{0});
                       const auto retired = access.SetAnimationAssociationGroupIdOffset(token, 7);
                       fixture.Put(P::GlobalRva, rows);
                       const auto invalid = (std::numeric_limits<std::uint64_t>::max)();
                       std::memcpy(allocation, &invalid, 8);
                       const auto cookie = access.SetAnimationAssociationGroupIdOffset(token, 7);
                       std::memcpy(allocation, &capacity, 8);
                       Hook::failNext = true;
                       const auto read = access.SetAnimationAssociationGroupIdOffset(token, 7);
                       Hook::failNext = false;
                       Check(owner == ClockResult::OwnerConflict && locator == ClockResult::BindingChanged && retired == ClockResult::NotInitialized &&
                                 cookie == ClockResult::BindingChanged && read == ClockResult::ReadFailure && WriteHook::calls == calls &&
                                 !runtime.stopping.load() && intact(),
                             "stale generation, redirected row, retired array, corrupt cookie and read refusal all stop before native effect");
                   });
        }
        else
        {
            Check(scenario <= 2, "only isolated lost-ack and conflict scenarios execute");
            const auto previous = timingCallbackGeneration;
            WriteHook::mode = scenario;
            action("entered refusal preserves real store",
                   [&]
                   {
                       const auto calls = WriteHook::calls;
                       const auto result = access.SetAnimationAssociationGroupIdOffset(token, 0x1234);
                       expectOffset(scenario == 1 ? 0x1234u : 0x55aa1234u);
                       Check(result == ClockResult::OwnerConflict && WriteHook::calls == calls + 1 && runtime.stopping.load() && intact(),
                             "entered lost acknowledgement or contention stops actual runtime without retrying or restoring persistent native state");
                       const auto repeated = access.SetAnimationAssociationGroupIdOffset(token, 9);
                       Check(repeated != ClockResult::Ok && WriteHook::calls == calls + 1 && intact(),
                             "stopped owner cannot replay an entered persistent DWORD write");
                   });
            timingCallbackGeneration = previous;
            WriteHook::mode = 0;
        }
        WriteHook::enabled = false;
        Hook::afterRead = nullptr;
        Hook::failNext = false;
        Hook::watched = 0;
        Hook::watchedBytes = sizeof(std::int32_t);
        WorldGroundOwnerFixture::Retire(service, fixture);
        Check(intact(), "ordinary End retires hooks without rolling back source intent or freeing borrowed group/static rows");
        fixture.Put(P::GlobalRva, std::uintptr_t{0});
        fixture.Put(P::CountRva, std::int32_t{0});
        Check(VirtualFree(childAllocation, 0, MEM_RELEASE) != FALSE && VirtualFree(allocation, 0, MEM_RELEASE) != FALSE,
              "release only task fixture allocations after actual owner teardown");
        Host::active = nullptr;
        ClockReaderTestHost::image = previousImage;
        saved.Restore();
        if (scenario == 0)
        {
            std::puts("Production animation association group offset owner fixture completed");
            std::puts("Production actual animation association group offset SDK fixture completed");
            std::puts("Production animation association group offset physical token fixture completed");
        }
        if (scenario == 1)
            std::puts("Production animation association group offset lost acknowledgement stop fixture completed");
        if (scenario == 2)
            std::puts("Production animation association group offset conflict stop fixture completed");
    }
}
