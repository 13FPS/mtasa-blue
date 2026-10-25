/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Real bank-only ordinal/raw factories through their current owner
 ****************************************************************************/
namespace BankAnimationFactoryOwnerFixture
{
    namespace ReadHook = ClockAnimationHierarchyBankOwnerTest;
    // Read hook callbacks only mutate the owned fixture or caller input. They
    // never issue a private lease or invent a native factory acknowledgement.
    std::uint64_t* callerGeneration = nullptr;
    std::uint32_t* callerOrdinal = nullptr;
    void           ChangeCaller() noexcept
    {
        if (callerGeneration)
            *callerGeneration = 0;
        if (callerOrdinal)
            *callerOrdinal = 0xffffffffu;
    }
    void LoseGeneration() noexcept
    {
        ++timingCallbackGeneration;
    }
    void RunPrepared(ClockImageFixture& fixture)
    {
        namespace P = NativeAnimationBlockProfile;
        namespace M = NativeAnimationManagerProfile;
        namespace H = NativeAnimationBlockHierarchyProfile;
        namespace Host = NativeCustomAnimSequenceStorageFixture;
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
        const auto generation = runtime.generation.load();
        saved.Global(M::AnimationCountRva, std::int32_t{12});
        saved.Global(P::BankRva + P::IdOffset, std::int32_t{5});
        saved.Global(P::BankRva + P::AnimationCountOffset, std::uint32_t{3});
        saved.Global(P::BankRva + P::LoadedOffset, static_cast<unsigned char>(1));
        for (const auto index : {5u, 6u, 7u, 8u})
            NativeAnimationBlockFactoryFixture::SeedHierarchy(fixture.image, &WorldGroundOwnerFixture::Writes::Apply, &saved, index, 0);
        const auto locator = fixture.image + H::BankRva + std::uintptr_t{6} * H::RowBytes;
        fixture.Put(H::BankRva + std::uintptr_t{6} * H::RowBytes + 0x18, std::uint32_t{0x7fc04411});
        ReadHook::locator = locator;
        ReadHook::cookie = 0;
        ReadHook::Reset();
        PrivateAnimationFactoryManagerReceiver manager(runtime.access, runtime.access, runtime.access, runtime.access, generation);
        CAnimManager&                          sdkManager = manager;
        auto&                                  access = static_cast<NativeAnimationHierarchyAccess&>(runtime.access);
        const auto                             action = [&](const char* label, const auto& body)
        {
            callback.action = [&](NativeGameTimingAccess&)
            {
                try
                {
                    body();
                }
                catch (const std::exception& error)
                {
                    std::fprintf(stderr, "Bank factory action %s refused: %s\n", label, error.what());
                    Check(false, label);
                }
            };
            Check(service.Renew(generation), "renew the actual bank factory callback for each bounded observation");
            fixture.invokeTimer();
            callback.action = [](NativeGameTimingAccess&) {};
        };
        const auto same = [](const NativeAnimationHierarchyReferenceToken& a, const NativeAnimationHierarchyReferenceToken& b)
        { return a.ownerGeneration == b.ownerGeneration && a.locator == b.locator; };
        const NativeAnimationHierarchyReferenceToken sentinel{0x71, 0x73};
        auto                                         output = sentinel;
        Check(access.ResolveBankAnimationHierarchyAt(generation, 6, output) != ClockResult::Ok && same(output, sentinel) &&
                  !sdkManager.GetAnimBlendHierarchy(nullptr),
              "bank locator cannot lend admission between callbacks, while the source null-pointer factory remains a CPU null result");
        std::unique_ptr<CAnimBlendHierarchy> ordinal, raw, fresh;
        action("current ordinal and raw bank factories",
               [&]
               {
                   Check(access.ResolveBankAnimationHierarchyAt(generation, 6, output) == ClockResult::Ok && output.ownerGeneration == generation &&
                             output.locator == locator,
                         "actual ordinal service stamps current generation only after complete row/block qualification");
                   ordinal = sdkManager.GetAnimation(6);
                   raw = sdkManager.GetAnimBlendHierarchy(reinterpret_cast<CAnimBlendHierarchySAInterface*>(locator));
                   fresh = sdkManager.GetAnimation(6);
                   Check(ordinal && raw && fresh && ordinal.get() != raw.get() && ordinal.get() != fresh.get() &&
                             reinterpret_cast<std::uintptr_t>(ordinal->GetInterface()) == locator && raw->GetInterface() == ordinal->GetInterface(),
                         "real SDK factories produce fresh CPU facades over the same current native bank row");
                   const float   time = fresh->GetTotalTime();
                   std::uint32_t bits = 0;
                   std::memcpy(&bits, &time, sizeof(bits));
                   Check(bits == 0x7fc04411 && fresh->GetNameHashKey() == 0x13579bdf && fresh->GetAnimBlockID() == 0 && !fresh->IsCustom(),
                         "factory product dispatches actual hierarchy readers with raw float/hash identity and native block ownership");
                   Check(!runtime.customSequences.IsRetained() && !runtime.privateHierarchies.IsRetained() && !host.mallocCalls && !host.freeCalls,
                         "bank borrow factories create no private allocation, ownership or native free authority");
               });
        action("negative and physical-overflow ordinals",
               [&]
               {
                   bool negative = false, oversized = false;
                   try
                   {
                       static_cast<void>(sdkManager.GetAnimation(-1));
                   }
                   catch (const std::runtime_error&)
                   {
                       negative = true;
                   }
                   try
                   {
                       static_cast<void>(sdkManager.GetAnimation((std::numeric_limits<int>::max)()));
                   }
                   catch (const std::runtime_error&)
                   {
                       oversized = true;
                   }
                   output = sentinel;
                   const auto status = access.ResolveBankAnimationHierarchyAt(generation, (std::numeric_limits<std::uint32_t>::max)(), output);
                   Check(negative && oversized && status == ClockResult::InvalidArgument && same(output, sentinel),
                         "negative/int-max/UINT32-max requests refuse rather than wrap, narrow or fabricate an invalid-ID null product");
                   std::uintptr_t                 untouched = 0x8183;
                   NativeAnimationHierarchyMemory memory({fixture.image}, &CheckTimingOwner);
                   Check(memory.ReadAt(0xffffffffu, untouched) == ClockResult::InvalidArgument && untouched == 0x8183,
                         "native ordinal arithmetic checks pinned image bounds before publishing an address");
                   NativeAnimationHierarchyMemory overflow({(std::numeric_limits<std::uintptr_t>::max)() - H::BankRva}, &CheckTimingOwner);
                   Check(overflow.ReadAt(6, untouched) == ClockResult::InvalidArgument && untouched == 0x8183,
                         "native binding addition cannot overflow into an apparently readable bank row");
               });
        action("highwater is not row ownership",
               [&]
               {
                   output = sentinel;
                   Check(access.ResolveBankAnimationHierarchyAt(generation, 8, output) == ClockResult::BindingChanged && same(output, sentinel),
                         "a readable row in a highwater hole refuses without its owning block's actual range");
                   Check(access.ResolveBankAnimationHierarchyAt(generation, 12, output) == ClockResult::InvalidArgument && same(output, sentinel),
                         "logical highwater itself is outside the current bank borrow domain");
                   Check(
                       access.ResolveBankAnimationHierarchyReference(generation, locator + 1, output) == ClockResult::InvalidArgument && same(output, sentinel),
                       "misaligned raw locator cannot be laundered into native row identity");
               });
        fixture.Put(H::BankRva + std::uintptr_t{6} * H::RowBytes + H::OwningBlockOffset, static_cast<std::uint32_t>(P::ObservationLimit));
        action("wrong owning block refuses existing facade",
               [&]
               {
                   output = sentinel;
                   Check(access.ResolveBankAnimationHierarchyAt(generation, 6, output) == ClockResult::BindingChanged && same(output, sentinel),
                         "ordinal geometry cannot substitute for a current valid owning block");
                   bool refused = false;
                   try
                   {
                       static_cast<void>(ordinal->GetNameHashKey());
                   }
                   catch (const std::runtime_error&)
                   {
                       refused = true;
                   }
                   Check(refused && !runtime.stopping.load(), "borrowed facade rechecks actual bank origin on every method without poisoning read-only owner");
               });
        fixture.Put(H::BankRva + std::uintptr_t{6} * H::RowBytes + H::OwningBlockOffset, std::uint32_t{0});
        NativeAnimationBlockName rawName;
        rawName.fill(0x5a);
        saved.Global(P::BankRva, rawName);
        saved.Global(P::BankRva + P::LoadedOffset, static_cast<unsigned char>(0));
        action("preserve current unloaded bank policy",
               [&]
               {
                   output = sentinel;
                   Check(access.ResolveBankAnimationHierarchyAt(generation, 6, output) == ClockResult::Ok && output.locator == locator &&
                             raw->GetNameHashKey() == 0x13579bdf,
                         "existing current row/range policy does not invent LoadedOffset or NUL-name requirements for an unloaded persistent row");
               });
        saved.Global(P::BankRva + P::LoadedOffset, static_cast<unsigned char>(1));
        saved.Global(P::BankRva + P::AnimationCountOffset, std::uint32_t{1});
        action("moved range refuses raw row",
               [&]
               {
                   output = sentinel;
                   Check(access.ResolveBankAnimationHierarchyReference(generation, locator, output) == ClockResult::BindingChanged && same(output, sentinel),
                         "same aligned raw row loses bank qualification when its owning range no longer contains it");
               });
        saved.Global(P::BankRva + P::AnimationCountOffset, std::uint32_t{3});
        ReadHook::Reset();
        ReadHook::failScalar = 1;
        action("failed actual row observation preserves factory output",
               [&]
               {
                   output = sentinel;
                   Check(access.ResolveBankAnimationHierarchyAt(generation, 6, output) == ClockResult::ReadFailure && same(output, sentinel),
                         "underlying complete bank reader failure preserves every token field");
               });
        ReadHook::Reset();
        std::uint64_t requestedGeneration = generation;
        std::uint32_t requestedOrdinal = 6;
        callerGeneration = &requestedGeneration;
        callerOrdinal = &requestedOrdinal;
        ReadHook::afterScalar = &ChangeCaller;
        action("copy factory input before callbacks",
               [&]
               {
                   output = sentinel;
                   Check(access.ResolveBankAnimationHierarchyAt(requestedGeneration, requestedOrdinal, output) == ClockResult::Ok && !requestedGeneration &&
                             requestedOrdinal == 0xffffffffu && output.ownerGeneration == generation && output.locator == locator,
                         "caller mutation cannot replace the captured ordinal/generation during real bank bookends");
               });
        callerGeneration = nullptr;
        callerOrdinal = nullptr;
        ReadHook::Reset();
        ReadHook::afterScalar = &LoseGeneration;
        action("post-observation generation loss",
               [&]
               {
                   output = sentinel;
                   Check(access.ResolveBankAnimationHierarchyReference(generation, locator, output) == ClockResult::OwnerConflict && same(output, sentinel),
                         "raw factory output cannot publish after actual callback generation changes");
               });
        ReadHook::Reset();
        std::array<unsigned char, H::RowBytes> before{};
        std::memcpy(before.data(), reinterpret_cast<void*>(locator), before.size());
        action("bank facade has no private mutation/delete rights",
               [&]
               {
                   bool changed = false, deleted = false;
                   try
                   {
                       ordinal->Initialize();
                   }
                   catch (const std::runtime_error&)
                   {
                       changed = true;
                   }
                   try
                   {
                       sdkManager.DeleteCustomAnimHierarchyInterface(reinterpret_cast<CAnimBlendHierarchySAInterface*>(locator));
                   }
                   catch (const std::runtime_error&)
                   {
                       deleted = true;
                   }
                   Check(changed && deleted && std::memcmp(before.data(), reinterpret_cast<void*>(locator), before.size()) == 0 &&
                             !runtime.customSequences.IsRetained() && !runtime.privateHierarchies.IsRetained() && !host.mallocCalls && !host.freeCalls,
                         "bank borrow refuses private operations without writes, adoption or destructor/free side effects");
               });
        std::unique_ptr<CAnimBlendHierarchy> privateHierarchy;
        action("private owner cannot become a bank factory",
               [&]
               {
                   privateHierarchy = sdkManager.GetCustomAnimBlendHierarchy();
                   const auto privateTag = privateHierarchy->GetInterface();
                   output = sentinel;
                   const auto status = access.ResolveBankAnimationHierarchyReference(generation, reinterpret_cast<std::uintptr_t>(privateTag), output);
                   bool       refused = false;
                   try
                   {
                       static_cast<void>(sdkManager.GetAnimBlendHierarchy(privateTag));
                   }
                   catch (const std::runtime_error&)
                   {
                       refused = true;
                   }
                   Check((status == ClockResult::InvalidArgument || status == ClockResult::BindingChanged) && same(output, sentinel) && refused,
                         "actual issued private hierarchy remains outside the explicit native bank resolver domain");
                   sdkManager.DeleteCustomAnimHierarchyInterface(privateTag);
               });
        privateHierarchy.reset();
        raw.reset();
        fresh.reset();
        Check(std::memcmp(before.data(), reinterpret_cast<void*>(locator), before.size()) == 0 && !host.mallocCalls && !host.freeCalls &&
                  !runtime.customSequences.IsRetained() && !runtime.privateHierarchies.IsRetained(),
              "CPU bank facade destruction releases no row/child/cache native ownership");
        ReadHook::Reset();
        WorldGroundOwnerFixture::Retire(service, fixture);
        NativeClockService next;
        bool               staleFactory = false, staleFacade = false;
        callback.action = [&](NativeGameTimingAccess&)
        {
            try
            {
                static_cast<void>(sdkManager.GetAnimation(6));
            }
            catch (const std::runtime_error&)
            {
                staleFactory = true;
            }
            try
            {
                static_cast<void>(ordinal->GetInterface());
            }
            catch (const std::runtime_error&)
            {
                staleFacade = true;
            }
            output = sentinel;
            Check(access.ResolveBankAnimationHierarchyAt(generation, 6, output) == ClockResult::OwnerConflict && same(output, sentinel),
                  "old factory generation cannot stamp a fresh bank token under another owner epoch");
        };
        BeginOwned(next, fixture, false, true, true, &WorldGroundOwnerFixture::Callback::Invoke, &callback);
        fixture.invokeTimer();
        Check(staleFactory && staleFacade && !ReadHook::scalarReads && !runtime.stopping.load(),
              "old manager and facade refuse before native row observation in a new callback generation");
        WorldGroundOwnerFixture::Retire(next, fixture);
        ordinal.reset();
        ReadHook::locator = ReadHook::cookie = 0;
        ReadHook::Reset();
        Host::active = nullptr;
        ClockReaderTestHost::image = previousImage;
        saved.Restore();
        std::puts("Production native bank animation factory owner fixture completed");
        std::puts("Production actual bank animation factory SDK owner fixture completed");
        std::puts("Production bank animation factory generation bookends completed");
    }
}
