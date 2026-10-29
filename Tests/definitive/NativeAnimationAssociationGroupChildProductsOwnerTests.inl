/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual same-parent block and static-association facade products
 ****************************************************************************/
namespace AnimationAssociationGroupChildProductsOwnerFixture
{
    namespace Host = NativeCustomAnimSequenceStorageFixture;
    namespace Hook = ClockAnimationManagerOwnerTest;
    std::uintptr_t                                 parent = 0, alternateBlock = 0;
    NativeAnimationAssociationGroupReferenceToken* callerToken = nullptr;
    bool                                           changed = false;
    unsigned                                       transition = 0;
    void                                           ChangeCaller() noexcept
    {
        callerToken->ownerGeneration = 0;
        callerToken->index = 0xffffffffu;
        callerToken->locator = 1;
    }
    void ChangeParentAfterFirstObservation() noexcept
    {
        if (Hook::reads < 3)
        {
            Hook::afterRead = &ChangeParentAfterFirstObservation;
            return;
        }
        if (transition == 0)
            std::memcpy(reinterpret_cast<void*>(parent), &alternateBlock, sizeof(alternateBlock));
        else
        {
            const std::uint32_t offset = 43;
            std::memcpy(reinterpret_cast<void*>(parent + 0x14), &offset, sizeof(offset));
        }
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
        blockRow[B::LoadedOffset] = 1;
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
        constexpr std::uint64_t capacity = 2, childCapacity = 3;
        constexpr std::size_t   outerBytes = 8 + capacity * 32, childBytes = 8 + childCapacity * 32;
        void*                   allocation = VirtualAlloc(nullptr, outerBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        void*                   childAllocation = VirtualAlloc(nullptr, childBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        Check(allocation && childAllocation, "own only fixture buffers representing current engine group/static allocations");
        const auto rows = reinterpret_cast<std::uintptr_t>(allocation) + 8;
        const auto child = reinterpret_cast<std::uintptr_t>(childAllocation) + 8;
        parent = rows + 32;
        std::memset(allocation, 0xa5, outerBytes);
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
        saved.Global(P::CountRva, std::int32_t{2});
        std::array<unsigned char, outerBytes> expected{};
        std::array<unsigned char, childBytes> expectedChild{};
        std::memcpy(expected.data(), allocation, outerBytes);
        std::memcpy(expectedChild.data(), childAllocation, childBytes);
        const auto                                    generation = runtime.generation.load();
        auto&                                         access = static_cast<NativeAnimationAssociationGroupAccess&>(runtime.access);
        NativeAnimationAssociationGroupReferenceToken emptyToken, token;
        const auto                                    action = [&](const char* label, const auto& body)
        {
            callback.action = [&](NativeGameTimingAccess&)
            {
                try
                {
                    body();
                }
                catch (const std::exception& error)
                {
                    std::fprintf(stderr, "Group child product %s refused: %s\n", label, error.what());
                    Check(false, label);
                }
            };
            Check(service.Renew(generation), "renew the actual producer before each bounded child-product action");
            StaticAssociationInitializationOwnerFixture::ProducerPulse pulse(service, generation);
            Check(pulse.renewals.load() && !pulse.unexpectedFailures.load(), "independent current producer keeps unchanged 500ms policy");
            fixture.invokeTimer();
            pulse.Stop();
            Check(!pulse.unexpectedFailures.load() && runtime.producerThread == GetCurrentThreadId(), "join producer before normal service duties resume");
            callback.action = [](NativeGameTimingAccess&) {};
        };
        const auto intact = [&]
        {
            return !std::memcmp(expected.data(), allocation, outerBytes) && !std::memcmp(expectedChild.data(), childAllocation, childBytes) &&
                   !std::memcmp(blockRow.data(), reinterpret_cast<void*>(block), blockRow.size()) &&
                   !std::memcmp(otherBlock.data(), reinterpret_cast<void*>(alternateBlock), otherBlock.size()) && !host.mallocCalls && !host.freeCalls &&
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
        const auto sentinel = []
        {
            NativeAnimationAssociationGroupBlock result;
            result.interfaceAddress = 71;
            result.identity.index = 73;
            result.identity.ownerGeneration = 79;
            result.identity.name.fill(0xa7);
            return result;
        };
        const auto unchangedBlock = [](const NativeAnimationAssociationGroupBlock& value)
        {
            NativeAnimationBlockName name{};
            name.fill(0xa7);
            return value.interfaceAddress == 71 && value.identity.index == 73 && value.identity.ownerGeneration == 79 && value.identity.name == name;
        };
        const auto unchangedStatic = [](const NativeAnimationStaticAssociationReferenceToken& value)
        { return value.ownerGeneration == 71 && value.groupIndex == 73 && value.index == 79 && value.locator == 83; };
        action("fresh current parent tokens",
               [&]
               {
                   Check(access.ResolveAnimationAssociationGroup(generation, 0, emptyToken) == ClockResult::Ok &&
                             access.ResolveAnimationAssociationGroup(generation, 1, token) == ClockResult::Ok && token.locator == parent && intact(),
                         "actual owner stamps both current-array parent tokens without child ownership claims");
               });
        AnimationAssociationGroupReceiver empty(access, emptyToken), receiver(access, token);
        Hook::watched = fixture.image + P::GlobalRva;
        Hook::watchedBytes = 8;
        Hook::reads = 0;
        Hook::afterRead = nullptr;
        Hook::failNext = false;
        action("exact source null and nonnull block products",
               [&]
               {
                   auto                     missing = sentinel();
                   auto                     found = sentinel();
                   NativeAnimationBlockName expectedName{};
                   std::memcpy(expectedName.data(), blockRow.data(), expectedName.size());
                   Check(access.ReadAnimationAssociationGroupBlock(emptyToken, missing) == ClockResult::Ok && !missing.interfaceAddress &&
                             !missing.identity.ownerGeneration && missing.identity.name == NativeAnimationBlockName{} && empty.GetAnimBlock() == nullptr &&
                             access.ReadAnimationAssociationGroupBlock(token, found) == ClockResult::Ok && found.interfaceAddress == block &&
                             found.identity.index == 1 && found.identity.ownerGeneration == generation && found.identity.name == expectedName && intact(),
                         "actual coupled runtime preserves source null and exact current block index/name16/generation, including bytes after NUL");
                   auto memory = sentinel();
                   Check(NativeAnimationManagerMemory({fixture.image}, &CheckTimingOwner).ReadAssociationGroupBlock(token, memory) == ClockResult::Ok &&
                             memory.interfaceAddress == block && memory.identity.index == 1 && !memory.identity.ownerGeneration &&
                             memory.identity.name == expectedName && intact(),
                         "native memory proves origin while only runtime may publish CPU owner generation");
                   Check(refuses([&] { static_cast<void>(receiver.GetAnimBlock()); }) && intact(),
                         "nonnull SDK block cannot invent the required explicit borrowed CGame for Request");
               });
        action("unloaded block identity remains a source product",
               [&]
               {
                   const std::uint8_t zero = 0;
                   fixture.Put(B::BankRva + B::RowBytes + B::LoadedOffset, zero);
                   auto       found = sentinel();
                   const auto result = access.ReadAnimationAssociationGroupBlock(token, found);
                   fixture.Put(B::BankRva + B::RowBytes + B::LoadedOffset, blockRow[B::LoadedOffset]);
                   Check(result == ClockResult::Ok && found.interfaceAddress == block && found.identity.ownerGeneration == generation && intact(),
                         "block facade identity does not substitute an IsLoaded gate for source GetAnimBlock");
               });
        CAnimBlendStaticAssociation* first = nullptr;
        action("zero Created does not gate direct group child",
               [&]
               {
                   NativeAnimationStaticAssociationReferenceToken selected;
                   Check(access.ResolveAnimationAssociationGroupStaticById(token, 18, selected) == ClockResult::Ok && selected.ownerGeneration == generation &&
                             selected.groupIndex == 1 && selected.index == 1 && selected.locator == child + 32,
                         "actual direct group selector uses array and modulo32 offset without manager Created or enum gates");
                   first = receiver.GetAnimation(18);
                   Check(first && reinterpret_cast<std::uintptr_t>(first) != child + 32 &&
                             reinterpret_cast<std::uintptr_t>(first->GetInterface()) == child + 32 && first->GetAnimID() == ids[1] &&
                             receiver.GetAnimation(18) == first && intact(),
                         "actual GetAnimation returns one group-owned CPU facade over the current physical native row, not a cast engine pointer");
               });
        action("physical cached child survives offset reselection",
               [&]
               {
                   receiver.SetIDOffset(41);
                   const std::uint32_t nextOffset = 41;
                   std::memcpy(expected.data() + 8 + 32 + 0x14, &nextOffset, 4);
                   auto* same = receiver.GetAnimation(42);
                   auto* other = receiver.GetAnimation(41);
                   Check(same == first && other && other != first && reinterpret_cast<std::uintptr_t>(other->GetInterface()) == child &&
                             other->GetAnimID() == ids[0] && first->GetAnimID() == ids[1] && intact(),
                         "updated ID selection reuses the same physical CPU child while a different physical row gets its own facade");
               });
        action("full uint32 direct group ID is not narrowed",
               [&]
               {
                   receiver.SetIDOffset(-1);
                   const std::uint32_t negativeOffset = 0xffffffffu;
                   std::memcpy(expected.data() + 8 + 32 + 0x14, &negativeOffset, 4);
                   auto* selected = receiver.GetAnimation(0xffffffffu);
                   Check(selected && reinterpret_cast<std::uintptr_t>(selected->GetInterface()) == child && selected->GetAnimID() == ids[0] && intact(),
                         "full uint32 animation IDs preserve the native unsigned offset subtraction rather than an enum or WORD cast");
                   receiver.SetIDOffset(41);
                   const std::uint32_t restored = 41;
                   std::memcpy(expected.data() + 8 + 32 + 0x14, &restored, 4);
               });
        action("current parent and child refusal preserves outputs",
               [&]
               {
                   auto bad = token;
                   ++bad.ownerGeneration;
                   auto       blockValue = sentinel();
                   const auto owner = access.ReadAnimationAssociationGroupBlock(bad, blockValue);
                   bad = token;
                   bad.locator -= 32;
                   const auto           locator = access.ReadAnimationAssociationGroupBlock(bad, blockValue);
                   const std::uintptr_t foreign = parent;
                   std::memcpy(reinterpret_cast<void*>(parent), &foreign, 8);
                   const auto foreignBlock = access.ReadAnimationAssociationGroupBlock(token, blockValue);
                   std::memcpy(reinterpret_cast<void*>(parent), &block, 8);
                   NativeAnimationStaticAssociationReferenceToken staticValue{71, 73, 79, 83};
                   bad = token;
                   bad.locator -= 32;
                   const auto badParent = access.ResolveAnimationAssociationGroupStaticById(bad, 42, staticValue);
                   const auto fullIndex = access.ResolveAnimationAssociationGroupStaticById(token, 0xffffffffu, staticValue);
                   const auto invalid = (std::numeric_limits<std::uint64_t>::max)();
                   std::memcpy(childAllocation, &invalid, 8);
                   const auto cookie = access.ResolveAnimationAssociationGroupStaticById(token, 42, staticValue);
                   std::memcpy(childAllocation, &childCapacity, 8);
                   Check(owner == ClockResult::OwnerConflict && locator == ClockResult::BindingChanged && foreignBlock != ClockResult::Ok &&
                             unchangedBlock(blockValue) && badParent == ClockResult::BindingChanged && fullIndex == ClockResult::InvalidArgument &&
                             cookie == ClockResult::BindingChanged && unchangedStatic(staticValue) &&
                             refuses([&] { static_cast<void>(receiver.GetAnimation(0xffffffffu)); }) && intact(),
                         "same-owner parent origin and current child capacity are required for every publication; failures never become null/default success");
               });
        action("block immutable identity is currently qualified",
               [&]
               {
                   std::array<unsigned char, 16> original{};
                   std::memcpy(original.data(), reinterpret_cast<void*>(block), 16);
                   std::array<unsigned char, 16> unterminated{};
                   unterminated.fill('Q');
                   fixture.Put(B::BankRva + B::RowBytes, unterminated);
                   auto       value = sentinel();
                   const auto result = access.ReadAnimationAssociationGroupBlock(token, value);
                   fixture.Put(B::BankRva + B::RowBytes, original);
                   Check(result == ClockResult::BindingChanged && unchangedBlock(value) && intact(),
                         "nonnull source block publishes no facade identity until its actual complete name16 domain is admissible");
               });
        action("complete caller parent token copied before callback",
               [&]
               {
                   auto source = token;
                   callerToken = &source;
                   Hook::afterRead = &ChangeCaller;
                   auto       value = sentinel();
                   const auto result = access.ReadAnimationAssociationGroupBlock(source, value);
                   Hook::afterRead = nullptr;
                   callerToken = nullptr;
                   Check(result == ClockResult::Ok && value.interfaceAddress == block && value.identity.ownerGeneration == generation &&
                             !source.ownerGeneration && intact(),
                         "actual owner snapshots caller parent intent before current native observation callbacks can redirect it");
               });
        for (transition = 0; transition != 2; ++transition)
        {
            action("coupled parent and child bookends",
                   [&]
                   {
                       Hook::reads = 0;
                       changed = false;
                       Hook::afterRead = &ChangeParentAfterFirstObservation;
                       auto                                           blockValue = sentinel();
                       NativeAnimationStaticAssociationReferenceToken staticValue{71, 73, 79, 83};
                       const auto result = transition ? access.ResolveAnimationAssociationGroupStaticById(token, 42, staticValue)
                                                      : access.ReadAnimationAssociationGroupBlock(token, blockValue);
                       Hook::afterRead = nullptr;
                       std::memcpy(reinterpret_cast<void*>(parent), &block, 8);
                       const std::uint32_t restoredOffset = 41;
                       std::memcpy(reinterpret_cast<void*>(parent + 0x14), &restoredOffset, 4);
                       Check(changed && result == ClockResult::BindingChanged && (transition ? unchangedStatic(staticValue) : unchangedBlock(blockValue)) &&
                                 intact(),
                             "a parent block or ID-offset transition cannot publish a product selected from a different observation");
                   });
        }
        action("read refusal and native group retirement",
               [&]
               {
                   Hook::failNext = true;
                   auto       blockValue = sentinel();
                   const auto fault = access.ReadAnimationAssociationGroupBlock(token, blockValue);
                   Hook::failNext = false;
                   fixture.Put(P::GlobalRva, std::uintptr_t{0});
                   NativeAnimationStaticAssociationReferenceToken staticValue{71, 73, 79, 83};
                   const auto                                     retired = access.ResolveAnimationAssociationGroupStaticById(token, 42, staticValue);
                   fixture.Put(P::GlobalRva, rows);
                   Check(fault == ClockResult::ReadFailure && retired == ClockResult::NotInitialized && unchangedBlock(blockValue) &&
                             unchangedStatic(staticValue) && !runtime.stopping.load() && intact(),
                         "read loss or native table retirement preserves outputs without creating native teardown debt");
               });
        Check(refuses([&] { static_cast<void>(first->GetInterface()); }) && intact(),
              "cached CPU child still needs an admitted current callback for native reads");
        Hook::afterRead = nullptr;
        Hook::failNext = false;
        Hook::watched = 0;
        Hook::watchedBytes = sizeof(std::int32_t);
        WorldGroundOwnerFixture::Retire(service, fixture);
        Check(intact(), "ordinary End retires hooks without freeing engine group, block or static storage from CPU facade caches");
        fixture.Put(P::GlobalRva, std::uintptr_t{0});
        fixture.Put(P::CountRva, std::int32_t{0});
        Check(VirtualFree(childAllocation, 0, MEM_RELEASE) != FALSE && VirtualFree(allocation, 0, MEM_RELEASE) != FALSE,
              "only task-owned native-shaped fixture buffers are released after actual owner retirement");
        parent = alternateBlock = 0;
        Host::active = nullptr;
        ClockReaderTestHost::image = previousImage;
        saved.Restore();
        std::puts("Production animation association group child products owner fixture completed");
        std::puts("Production actual animation association group static child SDK fixture completed");
        std::puts("Production animation association group child products coupled refusal fixture completed");
    }
}
