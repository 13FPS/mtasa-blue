/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual static-association getters retain physical native row affinity
 ****************************************************************************/
namespace AnimationStaticAssociationOwnerFixture
{
    namespace Host = NativeCustomAnimSequenceStorageFixture;
    namespace Hook = ClockAnimationManagerOwnerTest;
    std::uintptr_t                                  rows = 0, child = 0;
    NativeAnimationStaticAssociationReferenceToken* callerToken = nullptr;
    bool                                            changed = false;
    unsigned                                        transition = 0;
    void                                            ChangeCaller() noexcept
    {
        callerToken->ownerGeneration = 0;
        callerToken->groupIndex = 0xffffffffu;
        callerToken->index = (std::numeric_limits<std::size_t>::max)();
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
            const std::uint16_t flags = 7;
            std::memcpy(reinterpret_cast<void*>(child + 32 + 0x0e), &flags, 2);
        }
        if (transition == 1)
        {
            const std::uintptr_t hierarchy = 0x9988;
            std::memcpy(reinterpret_cast<void*>(child + 32 + 0x18), &hierarchy, 8);
        }
        if (transition == 2)
        {
            const auto pointer = child + 32;
            std::memcpy(reinterpret_cast<void*>(rows + 8), &pointer, 8);
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
        namespace A = NativeAnimationStaticAssociationProfile;
        for (const auto& pin : P::Code)
            WorldGroundOwnerFixture::Writes::Apply(&saved, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : A::ReadCode)
            WorldGroundOwnerFixture::Writes::Apply(&saved, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        constexpr std::uint64_t outerCapacity = 2, childCapacity = 3;
        constexpr std::size_t   outerBytes = 8 + outerCapacity * 32, childBytes = 8 + childCapacity * 32;
        void*                   outerAllocation = VirtualAlloc(nullptr, outerBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        void*                   childAllocation = VirtualAlloc(nullptr, childBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        Check(outerAllocation && childAllocation, "own only task fixture buffers representing original native parent/child allocations");
        rows = reinterpret_cast<std::uintptr_t>(outerAllocation) + 8;
        child = reinterpret_cast<std::uintptr_t>(childAllocation) + 8;
        std::memset(outerAllocation, 0xa5, outerBytes);
        std::memset(childAllocation, 0xa5, childBytes);
        std::memcpy(outerAllocation, &outerCapacity, 8);
        std::memcpy(childAllocation, &childCapacity, 8);
        const std::uint32_t zero = 0;
        std::memcpy(reinterpret_cast<void*>(rows + 8), &child, 8);
        std::memcpy(reinterpret_cast<void*>(rows + 0x10), &zero, 4);
        std::memcpy(reinterpret_cast<void*>(rows + 0x14), &zero, 4);
        // No Created, node, block, hierarchy target or vtable predicate is
        // introduced: the source getters observe only the actual row fields.
        const std::array<std::uint16_t, 4> words{0xffffu, 0xfffeu, 0x8000u, 0xffffu};
        const std::uintptr_t               nodePointer = 0x12345678, hierarchyPointer = 0x76543210;
        for (std::size_t index = 0; index != childCapacity; ++index)
        {
            const auto row = child + index * 32;
            std::memcpy(reinterpret_cast<void*>(row + 8), words.data(), 8);
            std::memcpy(reinterpret_cast<void*>(row + 0x10), &nodePointer, 8);
            std::memcpy(reinterpret_cast<void*>(row + 0x18), &hierarchyPointer, 8);
        }
        saved.Global(P::GlobalRva, rows);
        saved.Global(P::CountRva, std::int32_t{1});
        std::array<unsigned char, outerBytes> outerExpected{};
        std::array<unsigned char, childBytes> childExpected{};
        std::memcpy(outerExpected.data(), outerAllocation, outerBytes);
        std::memcpy(childExpected.data(), childAllocation, childBytes);
        const auto                                     generation = runtime.generation.load();
        auto&                                          access = static_cast<NativeAnimationStaticAssociationAccess&>(runtime.access);
        NativeAnimationStaticAssociationReferenceToken token;
        Hook::watched = fixture.image + P::GlobalRva;
        Hook::watchedBytes = 8;
        Hook::reads = 0;
        Hook::afterRead = nullptr;
        Hook::failNext = false;
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
                    std::fprintf(stderr, "Static association action %s refused: %s\n", label, error.what());
                    Check(false, label);
                }
            };
            Check(service.Renew(generation), "renew actual owner for each physical static row observation");
            fixture.invokeTimer();
            callback.action = [](NativeGameTimingAccess&) {};
        };
        const auto unchanged = [&]
        {
            return !std::memcmp(outerExpected.data(), outerAllocation, outerBytes) && !std::memcmp(childExpected.data(), childAllocation, childBytes) &&
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
        const auto sameToken = [](const auto& a, const auto& b)
        { return a.ownerGeneration == b.ownerGeneration && a.groupIndex == b.groupIndex && a.index == b.index && a.locator == b.locator; };
        const NativeAnimationStaticAssociationValues sentinel{71, 73, 79, 83, 89, 97};
        const auto                                   sameValues = [&](const auto& value)
        {
            return value.interfaceAddress == sentinel.interfaceAddress && value.numBlendNodes == sentinel.numBlendNodes && value.animId == sentinel.animId &&
                   value.animGroup == sentinel.animGroup && value.flags == sentinel.flags && value.hierarchyAddress == sentinel.hierarchyAddress;
        };
        action("current original child row resolution",
               [&]
               {
                   Check(access.ResolveAnimationStaticAssociation(generation, 0, 1, token) == ClockResult::Ok && token.ownerGeneration == generation &&
                             token.groupIndex == 0 && token.index == 1 && token.locator == child + 32 && unchanged(),
                         "actual runtime resolves physical row by cookie capacity even though NumAnims is zero");
               });
        AnimationStaticAssociationReceiver receiver(access, token);
        action("actual unsigned node count getter",
               [&] { Check(receiver.GetNumBlendNodes() == 0xffffu && unchanged(), "actual SDK NumBlendNodes preserves complete unsignedWORD8"); });
        action("actual signed ID and group getters",
               [&]
               {
                   Check(receiver.GetAnimID() == -2 && receiver.GetAnimGroup() == (std::numeric_limits<std::int16_t>::min)() && unchanged(),
                         "actual SDK ID/group getters preserve raw signedWORD+A/C without source enum fallback");
               });
        action("actual signed flags getter",
               [&] { Check(receiver.GetFlags() == -1 && unchanged(), "actual SDK flags preserve signedWORDe without applying a manager policy"); });
        action("actual opaque pointer getters",
               [&]
               {
                   Check(reinterpret_cast<std::uintptr_t>(receiver.GetAnimHierarchyInterface()) == hierarchyPointer &&
                             reinterpret_cast<std::uintptr_t>(receiver.GetInterface()) == token.locator && hierarchyPointer != nodePointer && unchanged(),
                         "real hierarchy getter observes native+18 instead of allocated node-array+10; raw targets are not adopted/dereferenced");
               });
        action("offset change cannot reselect physical row",
               [&]
               {
                   const std::uint32_t offset = 0xfffffffeu;
                   std::memcpy(reinterpret_cast<void*>(rows + 0x14), &offset, 4);
                   const auto observed = receiver.GetAnimID();
                   std::memcpy(reinterpret_cast<void*>(rows + 0x14), &zero, 4);
                   Check(observed == -2 && unchanged(), "existing token remains the same physical row after mutable animation ID offset changes");
               });
        action("full capacity index and stale generation",
               [&]
               {
                   NativeAnimationStaticAssociationReferenceToken output{71, 73, 79, 83}, before = output;
                   const auto                                     outside = access.ResolveAnimationStaticAssociation(generation, 0, childCapacity, output);
                   const auto huge = access.ResolveAnimationStaticAssociation(generation, 0, (std::numeric_limits<std::size_t>::max)(), output);
                   auto       wrong = token;
                   ++wrong.ownerGeneration;
                   auto       values = sentinel;
                   const auto old = access.ReadAnimationStaticAssociation(wrong, values);
                   Check(outside == ClockResult::InvalidArgument && huge == ClockResult::InvalidArgument && sameToken(output, before) &&
                             old == ClockResult::OwnerConflict && sameValues(values) && unchanged(),
                         "fullsize_t physical index and copied generation refuse without publishing output");
               });
        action("physical locator and child origin",
               [&]
               {
                   auto wrong = token;
                   wrong.locator += 32;
                   auto                 values = sentinel;
                   const auto           locator = access.ReadAnimationStaticAssociation(wrong, values);
                   const std::uintptr_t null = 0;
                   std::memcpy(reinterpret_cast<void*>(rows + 8), &null, 8);
                   const auto missing = access.ReadAnimationStaticAssociation(token, values);
                   std::memcpy(reinterpret_cast<void*>(rows + 8), &child, 8);
                   Check(locator == ClockResult::BindingChanged && missing == ClockResult::NotInitialized && sameValues(values) && unchanged(),
                         "current child pointer and physical locator jointly qualify row identity; old issued pointer is never enough");
               });
        action("malformed original child cookie",
               [&]
               {
                   const auto wrong = (std::numeric_limits<std::uint64_t>::max)();
                   std::memcpy(childAllocation, &wrong, 8);
                   auto       values = sentinel;
                   const auto result = access.ReadAnimationStaticAssociation(token, values);
                   std::memcpy(childAllocation, &childCapacity, 8);
                   Check(result == ClockResult::BindingChanged && sameValues(values) && unchanged(),
                         "corrupt original64-bit cookie cannot invent current child extent");
               });
        action("copied caller token",
               [&]
               {
                   auto source = token;
                   callerToken = &source;
                   Hook::afterRead = &ChangeCaller;
                   NativeAnimationStaticAssociationValues values;
                   const auto                             result = access.ReadAnimationStaticAssociation(source, values);
                   Hook::afterRead = nullptr;
                   callerToken = nullptr;
                   Check(result == ClockResult::Ok && values.interfaceAddress == token.locator && values.animId == -2 && !source.ownerGeneration && unchanged(),
                         "actual runtime snapshots complete caller token before observation callbacks mutate it");
               });
        for (transition = 0; transition != 3; ++transition)
        {
            action("current payload and child bookends",
                   [&]
                   {
                       Hook::reads = 0;
                       changed = false;
                       Hook::afterRead = &ChangeAfterPayload;
                       auto       values = sentinel;
                       const auto result = access.ReadAnimationStaticAssociation(token, values);
                       Hook::afterRead = nullptr;
                       std::memcpy(reinterpret_cast<void*>(child + 32 + 0x0e), &words[3], 2);
                       std::memcpy(reinterpret_cast<void*>(child + 32 + 0x18), &hierarchyPointer, 8);
                       std::memcpy(reinterpret_cast<void*>(rows + 8), &child, 8);
                       Check(changed && result == ClockResult::BindingChanged && sameValues(values) && unchanged(),
                             "flags/hierarchy/currentchild changes after payload refuse a torn copied row");
                   });
        }
        action("first native read failure",
               [&]
               {
                   Hook::failNext = true;
                   auto values = sentinel;
                   Check(access.ReadAnimationStaticAssociation(token, values) == ClockResult::ReadFailure && sameValues(values) && !runtime.stopping.load(),
                         "native read refusal preserves values without native entry/effect debt");
               });
        action("final current owner bookend",
               [&]
               {
                   const auto previous = timingCallbackGeneration;
                   Hook::reads = 0;
                   changed = false;
                   Hook::afterRead = &LoseFinalOwner;
                   auto       values = sentinel;
                   const auto result = access.ReadAnimationStaticAssociation(token, values);
                   Hook::afterRead = nullptr;
                   timingCallbackGeneration = previous;
                   Check(changed && result == ClockResult::OwnerConflict && sameValues(values) && unchanged(),
                         "final generation loss cannot publish current static row data");
               });
        Check(refuses([&] { static_cast<void>(receiver.GetInterface()); }) && unchanged(),
              "raw source interface read still requires actual current callback admission");
        Hook::afterRead = nullptr;
        Hook::failNext = false;
        Hook::watched = 0;
        Hook::watchedBytes = sizeof(std::int32_t);
        WorldGroundOwnerFixture::Retire(service, fixture);
        Check(unchanged(), "readonly static observer End never frees/retains parent/child/node/hierarchy storage");
        fixture.Put(P::GlobalRva, std::uintptr_t{0});
        fixture.Put(P::CountRva, std::int32_t{0});
        Check(VirtualFree(childAllocation, 0, MEM_RELEASE) != FALSE && VirtualFree(outerAllocation, 0, MEM_RELEASE) != FALSE,
              "release only task-owned fixture buffers after actual native observer retirement");
        rows = child = 0;
        Host::active = nullptr;
        ClockReaderTestHost::image = previousImage;
        saved.Restore();
        std::puts("Production animation static association owner fixture completed");
        std::puts("Production actual animation static association SDK fixture completed");
    }
}
