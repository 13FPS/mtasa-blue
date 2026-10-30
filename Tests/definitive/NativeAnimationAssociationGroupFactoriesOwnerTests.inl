/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual manager factories over borrowed current native group rows
 ****************************************************************************/
namespace AnimationAssociationGroupFactoriesOwnerFixture
{
    namespace Host = NativeCustomAnimSequenceStorageFixture;
    namespace Hook = ClockAnimationManagerOwnerTest;
    std::uintptr_t parent = 0, alternateBlock = 0, image = 0, rowsBase = 0, replacementRows = 0;
    void*          cookie = nullptr;
    unsigned       transition = 0;
    bool           changed = false;
    void           ChangeObservedOrigin() noexcept
    {
        changed = true;
        if (transition == 0)
        {
            const std::uint64_t value = 119;
            std::memcpy(cookie, &value, 8);
        }
        if (transition == 1)
        {
            const std::int32_t value = 119;
            std::memcpy(reinterpret_cast<void*>(image + NativeAnimationGroupArrayProfile::CountRva), &value, 4);
        }
        if (transition == 2)
            std::memcpy(reinterpret_cast<void*>(image + NativeAnimationGroupArrayProfile::GlobalRva), &replacementRows, 8);
        if (transition == 3)
            ++timingCallbackGeneration;
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
        namespace B = NativeAnimationBlockProfile;
        for (const auto& pin : P::Code)
            WorldGroundOwnerFixture::Writes::Apply(&saved, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : A::Code)
            WorldGroundOwnerFixture::Writes::Apply(&saved, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : A::ReadCode)
            WorldGroundOwnerFixture::Writes::Apply(&saved, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : B::Code)
            WorldGroundOwnerFixture::Writes::Apply(&saved, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        saved.Global(B::GroupSlotRva, fixture.image + B::GroupRva, PAGE_READONLY);
        saved.Global(B::RemapRva + B::LoadedOpcodeIndex, static_cast<unsigned char>(B::LoadedSelector), PAGE_EXECUTE_READ);
        saved.Global(B::DispatchRva + B::LoadedSelector * 4, static_cast<std::uint32_t>(B::LoadedBranchRva), PAGE_EXECUTE_READ);
        saved.Global(B::CountRva, std::int32_t{2});
        saved.Global(NativeAnimationManagerProfile::AnimationCountRva, std::int32_t{12});
        std::array<unsigned char, B::RowBytes> blockRow{};
        const char                             name[] = "PED";
        std::memcpy(blockRow.data(), name, sizeof(name));
        blockRow[7] = 0x80;
        blockRow[B::LoadedOffset] = 0;
        const std::int32_t  blockOffset = 5;
        const std::uint32_t blockCount = 3;
        std::memcpy(blockRow.data() + B::IdOffset, &blockOffset, 4);
        std::memcpy(blockRow.data() + B::AnimationCountOffset, &blockCount, 4);
        const auto block = fixture.image + B::BankRva + B::RowBytes;
        alternateBlock = fixture.image + B::BankRva;
        saved.Global(B::BankRva + B::RowBytes, blockRow);
        auto otherBlock = blockRow;
        otherBlock[0] = 'X';
        saved.Global(B::BankRva, otherBlock);
        constexpr std::uint64_t capacity = 120, childCapacity = 3;
        constexpr std::size_t   outerBytes = 8 + capacity * 32, childBytes = 8 + childCapacity * 32;
        void*                   allocation = VirtualAlloc(nullptr, outerBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        void*                   childAllocation = VirtualAlloc(nullptr, childBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        Check(allocation && childAllocation, "own only fixture buffers representing current engine group/static allocations");
        const auto rows = reinterpret_cast<std::uintptr_t>(allocation) + 8;
        const auto child = reinterpret_cast<std::uintptr_t>(childAllocation) + 8;
        parent = rows + (capacity - 1) * 32;
        std::memset(allocation, 0, outerBytes);
        std::memset(childAllocation, 0, childBytes);
        std::memcpy(allocation, &capacity, 8);
        std::memcpy(childAllocation, &childCapacity, 8);
        const std::uint32_t  count = 0, offset = 17, groupId = 0x80000003u;
        const std::uintptr_t null = 0;
        std::memcpy(reinterpret_cast<void*>(rows), &null, 8);
        std::memcpy(reinterpret_cast<void*>(parent), &block, 8);
        std::memcpy(reinterpret_cast<void*>(parent + 8), &child, 8);
        std::memcpy(reinterpret_cast<void*>(parent + 0x10), &count, 4);
        std::memcpy(reinterpret_cast<void*>(parent + 0x14), &offset, 4);
        std::memcpy(reinterpret_cast<void*>(parent + 0x18), &groupId, 4);
        const std::array<std::int16_t, 3> ids{23, 29, 31};
        for (std::size_t index = 0; index != ids.size(); ++index)
            std::memcpy(reinterpret_cast<void*>(child + index * 32 + A::IdOffset), &ids[index], 2);
        saved.Global(P::GlobalRva, rows);
        saved.Global(P::CountRva, std::int32_t{120});
        std::array<unsigned char, outerBytes> expected{};
        std::array<unsigned char, childBytes> expectedChild{};
        std::memcpy(expected.data(), allocation, outerBytes);
        std::memcpy(expectedChild.data(), childAllocation, childBytes);
        auto&      access = static_cast<NativeAnimationAssociationGroupAccess&>(runtime.access);
        const auto generation = runtime.generation.load();
        image = fixture.image;
        rowsBase = rows;
        cookie = allocation;
        void* replacement = VirtualAlloc(nullptr, outerBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        Check(replacement != nullptr, "own a second native-shaped array solely for a deterministic current-base transition");
        std::memcpy(replacement, allocation, outerBytes);
        replacementRows = reinterpret_cast<std::uintptr_t>(replacement) + 8;
        Check(rows > static_cast<std::uintptr_t>((std::numeric_limits<std::uint32_t>::max)()),
              "actual group factory fixture covers addresses whose upper32 bits must survive transport");
        ped_rebuild_fixture::Game                       game;
        AnimationAssociationGroupFactoryManagerReceiver manager(runtime.access, runtime.access, game, generation), observations(runtime.access, generation);
        CAnimManager&                                   sdk = manager;
        Check(!observations.GetAnimBlendAssocGroup(nullptr) && !runtime.stopping.load(),
              "actual null raw factory returns before callback admission and absent positive providers");
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
                    std::fprintf(stderr, "Group factory %s: %s\n", label, error.what());
                    Check(false, label);
                }
            };
            Check(service.Renew(generation), "renew actual group factory callback owner");
            StaticAssociationInitializationOwnerFixture::ProducerPulse pulse(service, generation);
            Check(pulse.renewals.load() && !pulse.unexpectedFailures.load(), "factory fixture retains unchanged independent500ms producer policy");
            fixture.invokeTimer();
            pulse.Stop();
            Check(!pulse.unexpectedFailures.load() && runtime.producerThread == GetCurrentThreadId(), "join producer before next group factory action");
            callback.action = [](NativeGameTimingAccess&) {};
        };
        const auto intact = [&]
        {
            return !std::memcmp(expected.data(), allocation, outerBytes) && !std::memcmp(expectedChild.data(), childAllocation, childBytes) &&
                   !std::memcmp(blockRow.data(), reinterpret_cast<void*>(block), blockRow.size()) &&
                   !std::memcmp(otherBlock.data(), reinterpret_cast<void*>(alternateBlock), otherBlock.size()) && !host.mallocCalls && !host.freeCalls &&
                   !runtime.groupCreations.IsRetained() && !runtime.privateAssociations.IsRetained() && !runtime.customSequences.IsRetained() &&
                   !runtime.privateHierarchies.IsRetained();
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
        const auto same = [](const NativeAnimationAssociationGroupReferenceToken& value)
        { return value.ownerGeneration == 71 && value.index == 73 && value.locator == 79; };
        std::unique_ptr<CAnimBlendAssocGroup> first, last, roundtrip;
        action("first last numeric and raw products",
               [&]
               {
                   first = sdk.GetAnimBlendAssoc(0);
                   last = sdk.GetAnimBlendAssoc(119);
                   Check(first && last && !first->IsCreated() && !last->IsCreated() && reinterpret_cast<std::uintptr_t>(first->GetInterface()) == rows &&
                             reinterpret_cast<std::uintptr_t>(last->GetInterface()) == parent && !first->GetAnimBlock() && last->GetAnimBlock() &&
                             !last->IsLoaded(),
                         "actual manager publishes first/last rows without Created,118-prefix,groupID-equality or loaded gates and warms real block cache");
                   roundtrip = sdk.GetAnimBlendAssocGroup(last->GetInterface());
                   Check(
                       roundtrip && roundtrip.get() != last.get() && roundtrip->GetInterface() == last->GetInterface() && roundtrip->GetAnimBlock() && intact(),
                       "raw factory transports exact full-width physical native identity into a separately owned CPU product");
                   NativeAnimationAssociationGroupReferenceToken native;
                   Check(NativeAnimationManagerMemory({fixture.image}, &CheckTimingOwner).ResolveAssociationGroupReference(parent, native) == ClockResult::Ok &&
                             !native.ownerGeneration && native.index == 119 && native.locator == parent,
                         "actual memory proves physical origin while only runtime stamps admitted CPU generation");
                   auto* staticChild = last->GetAnimation(18);
                   Check(staticChild && reinterpret_cast<std::uintptr_t>(staticChild) != child + 32 &&
                             reinterpret_cast<std::uintptr_t>(staticChild->GetInterface()) == child + 32 && staticChild->GetAnimID() == ids[1] &&
                             last->GetAnimation(18) == staticChild && intact(),
                         "factory product supplies actual group-owned static CPU child while current native row remains borrowed");
               });
        auto* unchanged = last.get();
        action("physical row domain refusals",
               [&]
               {
                   for (const auto locator : {rows - 8, rows + 1, rows + 31, rows + capacity * 32, (std::numeric_limits<std::uintptr_t>::max)()})
                   {
                       NativeAnimationAssociationGroupReferenceToken output{71, 73, 79};
                       Check(access.ResolveAnimationAssociationGroupInterface(generation, locator, output) != ClockResult::Ok && same(output),
                             "raw physical resolver refuses cookie/interior/end/outside locators without publishing a partial token");
                       Check(refuses([&] { last = sdk.GetAnimBlendAssocGroup(reinterpret_cast<CAnimBlendAssocGroupSAInterface*>(locator)); }) &&
                                 last.get() == unchanged && intact(),
                             "failed real raw factory preserves existing unique_ptr output and native facts");
                   }
                   Check(refuses([&] { last = sdk.GetAnimBlendAssoc(120); }) && refuses([&] { last = sdk.GetAnimBlendAssoc(0xffffffffu); }) &&
                             last.get() == unchanged && intact(),
                         "numeric factory preserves full unsigned SDK ID instead of clamping or aliasing to an ordinary group");
                   Check(refuses([&] { last = observations.GetAnimBlendAssoc(0); }) && last.get() == unchanged && intact(),
                         "observation-only manager cannot fabricate positive full products without explicit block/game providers");
               });
        Hook::watched = fixture.image + P::GlobalRva;
        Hook::watchedBytes = 8;
        for (transition = 0; transition != 4; ++transition)
        {
            action("current cookie count base and owner bookends",
                   [&]
                   {
                       Hook::reads = 0;
                       changed = false;
                       Hook::afterRead = &ChangeObservedOrigin;
                       NativeAnimationAssociationGroupReferenceToken output{71, 73, 79};
                       const auto                                    result = access.ResolveAnimationAssociationGroupInterface(generation, parent, output);
                       Hook::afterRead = nullptr;
                       timingCallbackGeneration = generation;
                       std::memcpy(allocation, &capacity, 8);
                       fixture.Put(P::CountRva, std::int32_t{120});
                       fixture.Put(P::GlobalRva, rows);
                       Check(changed && result != ClockResult::Ok && same(output) && intact(),
                             "coupled raw resolver bookends refuse changed cookie/count/base or callback owner and preserve output");
                   });
        }
        action("invalid count and generation",
               [&]
               {
                   NativeAnimationAssociationGroupReferenceToken output{71, 73, 79};
                   Check(access.ResolveAnimationAssociationGroupInterface(generation + 1, parent, output) != ClockResult::Ok && same(output),
                         "wrong generation cannot acquire an actual group token from a raw address");
                   fixture.Put(P::CountRva, std::int32_t{-1});
                   const auto negative = access.ResolveAnimationAssociationGroupInterface(generation, parent, output);
                   fixture.Put(P::CountRva, std::int32_t{120});
                   const std::uint64_t bad = (std::numeric_limits<std::uint64_t>::max)();
                   std::memcpy(allocation, &bad, 8);
                   const auto corrupt = access.ResolveAnimationAssociationGroupInterface(generation, parent, output);
                   std::memcpy(allocation, &capacity, 8);
                   Check(negative != ClockResult::Ok && corrupt != ClockResult::Ok && same(output) && intact(),
                         "invalid current signed count or producer-impossible cookie refuses instead of inventing a wrapper");
               });
        action("read loss preserves output",
               [&]
               {
                   Hook::failNext = true;
                   NativeAnimationAssociationGroupReferenceToken output{71, 73, 79};
                   const auto                                    result = access.ResolveAnimationAssociationGroupInterface(generation, parent, output);
                   Hook::failNext = false;
                   Check(result == ClockResult::ReadFailure && same(output) && intact() && !runtime.stopping.load(),
                         "lost current-array observation is a refusal, not a null product, partial token or native lifetime fault");
               });
        NativeAnimationAssociationGroupReferenceToken output{71, 73, 79};
        Check(access.ResolveAnimationAssociationGroupInterface(generation, parent, output) != ClockResult::Ok && same(output) && intact(),
              "same raw locator/generation cannot bypass real callback admission between actions");
        Check(refuses([&] { static_cast<void>(last->GetInterface()); }) && intact(), "returned CPU group re-admits every native operation");
        // Base-typed deletion must release the real CPU caches. It owns no
        // native group, static array, block reference or original destructor.
        first.reset();
        last.reset();
        roundtrip.reset();
        Check(intact(), "actual unique_ptr common-base destruction leaves all native row/child/block/ref bytes and native allocator call counts unchanged");
        Hook::afterRead = nullptr;
        Hook::failNext = false;
        Hook::watched = 0;
        Hook::watchedBytes = 4;
        WorldGroundOwnerFixture::Retire(service, fixture);
        Check(intact(), "ordinary End unloads hooks without private native debt manufactured by borrowed factory products");
        fixture.Put(P::GlobalRva, std::uintptr_t{0});
        fixture.Put(P::CountRva, std::int32_t{0});
        Check(VirtualFree(replacement, 0, MEM_RELEASE) && VirtualFree(childAllocation, 0, MEM_RELEASE) && VirtualFree(allocation, 0, MEM_RELEASE),
              "release only task-owned native-shaped buffers after actual owner and CPU wrappers retire");
        parent = alternateBlock = image = rowsBase = replacementRows = 0;
        cookie = nullptr;
        Host::active = nullptr;
        ClockReaderTestHost::image = previousImage;
        saved.Restore();
        std::puts("Production animation association group factories actual SDK owner fixture completed");
        std::puts("Production animation association group factories physical resolver refusal fixture completed");
        std::puts("Production animation association group factories CPU lifetime native facts fixture completed");
    }
}
