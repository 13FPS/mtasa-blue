/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual source hash-in-block and name factories share current native bank ownership
 ****************************************************************************/
namespace AnimationBlockHashFactoryOwnerFixture
{
    namespace Host = NativeCustomAnimSequenceStorageFixture;
    namespace Hook = ClockAnimationOwnerTest;
    class BorrowedBlock final : public CAnimBlockDE
    {
    public:
        BorrowedBlock(NativeAnimationBlockAccess& owner, const NativeAnimationBlockIdentity& identity) : CAnimBlockDE(owner, identity) {}
        // Request requires a real game/streaming provider. It is unrelated to
        // this observed immutable block receiver and must never be fabricated.
        void Request(EModelRequestType, bool) override { std::abort(); }
    };
    NativeAnimationBlockIdentity* caller = nullptr;
    std::uint32_t*                callerHash = nullptr;
    std::uintptr_t                changedAddress = 0;
    std::uint32_t                 changedHash = 0;
    bool                          changed = false;
    void                          ChangeCaller() noexcept
    {
        caller->index = 1;
        caller->name[0] ^= 1;
        caller->ownerGeneration = 0;
        *callerHash = 0;
    }
    void ChangeSearchedPrefix() noexcept
    {
        if (Hook::reads < 3)
        {
            Hook::afterRead = &ChangeSearchedPrefix;
            return;
        }
        std::memcpy(reinterpret_cast<void*>(changedAddress), &changedHash, sizeof(changedHash));
        changed = true;
    }
    void LoseFinalOwner() noexcept
    {
        if (Hook::reads < 6)
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
        namespace P = NativeAnimationBlockProfile;
        namespace M = NativeAnimationManagerProfile;
        namespace H = NativeAnimationBlockHierarchyProfile;
        const auto generation = runtime.generation.load();
        saved.Global(P::CountRva, std::int32_t{1});
        saved.Global(M::AnimationCountRva, std::int32_t{12});
        saved.Global(P::BankRva + P::IdOffset, std::int32_t{5});
        saved.Global(P::BankRva + P::AnimationCountOffset, std::uint32_t{4});
        saved.Global(P::BankRva + P::LoadedOffset, std::uint8_t{0});
        NativeAnimationBlockName name{'p', 'e', 'd', 0};
        name[15] = 0xa5;
        fixture.Put(P::BankRva, name);
        const auto nameHash = NativeClassicAnimationNamePolicy::Hash("owner_hash_name");
        for (const auto index : {5u, 6u, 7u, 8u})
            NativeAnimationBlockFactoryFixture::SeedHierarchy(fixture.image, &WorldGroundOwnerFixture::Writes::Apply, &saved, index, 0);
        fixture.Put(H::BankRva + 5 * H::RowBytes, std::uint32_t{0});
        fixture.Put(H::BankRva + 6 * H::RowBytes, nameHash);
        fixture.Put(H::BankRva + 7 * H::RowBytes, nameHash);
        fixture.Put(H::BankRva + 8 * H::RowBytes, std::uint32_t{0xffffffff});
        const auto address = [&](std::uint32_t ordinal) { return fixture.image + H::BankRva + static_cast<std::uintptr_t>(ordinal) * H::RowBytes; };
        Hook::row = fixture.image + P::BankRva;
        Hook::reads = 0;
        Hook::failNext = false;
        Hook::afterRead = nullptr;
        PrivateAnimationFactoryManagerReceiver manager(runtime.access, runtime.access, runtime.access, generation);
        CAnimManager&                          sdk = manager;
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
                    std::fprintf(stderr, "Block hash factory action %s refused: %s\n", label, error.what());
                    Check(false, label);
                }
            };
            Check(service.Renew(generation), "renew a natural callback separately for each bounded hash-factory observation");
            fixture.invokeTimer();
            callback.action = [](NativeGameTimingAccess&) {};
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
        NativeAnimationBlockIdentity identity;
        std::unique_ptr<CAnimBlock>  block;
        action("copy genuine current native block identity",
               [&]
               {
                   Check(runtime.access.ReadAnimationBlockIdentity(generation, 0, identity) == ClockResult::Ok && identity.name == name,
                         "hash factory begins with actual full16 native name/index/current-generation affinity");
                   block = std::make_unique<BorrowedBlock>(runtime.access, identity);
               });
        std::array<unsigned char, P::RowBytes>     expectedBlock{};
        std::array<unsigned char, 4 * H::RowBytes> expectedRows{};
        std::memcpy(expectedBlock.data(), reinterpret_cast<void*>(fixture.image + P::BankRva), expectedBlock.size());
        std::memcpy(expectedRows.data(), reinterpret_cast<void*>(address(5)), expectedRows.size());
        const auto unchanged = [&]
        {
            return !std::memcmp(expectedBlock.data(), reinterpret_cast<void*>(fixture.image + P::BankRva), expectedBlock.size()) &&
                   !std::memcmp(expectedRows.data(), reinterpret_cast<void*>(address(5)), expectedRows.size()) && !host.mallocCalls && !host.freeCalls &&
                   !runtime.customSequences.IsRetained() && !runtime.privateHierarchies.IsRetained();
        };
        std::unique_ptr<CAnimBlendHierarchy> product;
        for (const auto request : {std::pair<std::uint32_t, std::uint32_t>{0, 5}, {nameHash, 6}, {0xffffffffu, 8}})
        {
            action("native first hash match with full uint32 value",
                   [&]
                   {
                       std::uintptr_t output = 0x12345678;
                       Check(
                           runtime.access.FindAnimationBlockHierarchy(identity, request.first, output) == ClockResult::Ok && output == address(request.second),
                           "source hash lookup preserves0/highbits and picks the first matching hierarchy in an unloaded-flag current block");
                   });
            action("actual manager hash factory",
                   [&]
                   {
                       product = sdk.GetAnimation(request.first, block);
                       Check(product != nullptr, "real same-owner hash factory returns a CPU facade after bank qualification");
                   });
            action("actual returned bank facade observation",
                   [&]
                   {
                       Check(reinterpret_cast<std::uintptr_t>(product->GetInterface()) == address(request.second) &&
                                 product->GetNameHashKey() == request.first && unchanged(),
                             "actual hash-factory product reads the exact first matching current native hierarchy without acquiring ownership");
                   });
            product.reset();
        }
        action("qualified hash miss is null",
               [&]
               {
                   std::uintptr_t output = 0x12345678;
                   Check(runtime.access.FindAnimationBlockHierarchy(identity, 0xdeadbeefu, output) == ClockResult::Ok && !output,
                         "a complete qualified no-match scan publishes real null rather than a refusal sentinel");
               });
        action("actual SDK no-match result",
               [&] { Check(!sdk.GetAnimation(0xdeadbeefu, block), "actual unsigned SDK preserves the source complete-miss nullptr result"); });
        action("source name delegates to the same qualified hash lookup",
               [&]
               {
                   product = sdk.GetAnimation("OwNeR_HaSh_NaMe", block);
                   Check(product != nullptr, "actual source ASCII name key selects the native block hash");
               });
        action("name result observes exact same bank owner",
               [&]
               {
                   Check(reinterpret_cast<std::uintptr_t>(product->GetInterface()) == address(6) && unchanged(),
                         "name facade is the same source first-match bank borrow");
               });
        product.reset();
        action("name miss and null input",
               [&]
               {
                   Check(!sdk.GetAnimation("not_present_in_this_block", block) &&
                             refuses([&] { static_cast<void>(sdk.GetAnimation(static_cast<const char*>(nullptr), block)); }),
                         "actual name overload keeps qualified miss distinct from invalid null input");
               });
        action("count0 does not dereference the unused native offset",
               [&]
               {
                   fixture.Put(P::BankRva + P::AnimationCountOffset, std::uint32_t{0});
                   fixture.Put(P::BankRva + P::IdOffset, (std::numeric_limits<std::int32_t>::min)());
                   std::uintptr_t output = 0x12345678;
                   const auto     result = runtime.access.FindAnimationBlockHierarchy(identity, 0xffffffffu, output);
                   fixture.Put(P::BankRva + P::IdOffset, std::int32_t{5});
                   fixture.Put(P::BankRva + P::AnimationCountOffset, std::uint32_t{4});
                   Check(result == ClockResult::Ok && !output && unchanged(),
                         "source zero-count loop acknowledges a current empty block without importing ordinal or loaded guards");
               });
        std::unique_ptr<CAnimBlock> empty;
        NativeAnimationBlockAccess  foreignOwner;
        std::unique_ptr<CAnimBlock> foreign = std::make_unique<BorrowedBlock>(foreignOwner, identity);
        auto                        oldIdentity = identity;
        oldIdentity.ownerGeneration = generation + 1;
        std::unique_ptr<CAnimBlock> stale = std::make_unique<BorrowedBlock>(runtime.access, oldIdentity);
        action("facade provider and generation ownership",
               [&]
               {
                   Check(refuses([&] { static_cast<void>(sdk.GetAnimation(nameHash, empty)); }) &&
                             refuses([&] { static_cast<void>(sdk.GetAnimation(nameHash, foreign)); }) &&
                             refuses([&] { static_cast<void>(sdk.GetAnimation(nameHash, stale)); }) && unchanged(),
                         "null, foreign complete provider and stale immutable block token cannot supply actual manager hash-factory affinity");
                   std::uintptr_t output = 0x12345678;
                   Check(runtime.access.FindAnimationBlockHierarchy(oldIdentity, nameHash, output) == ClockResult::OwnerConflict && output == 0x12345678,
                         "native owner-generation refusal leaves hash output untouched");
               });
        action("positive count must fit exact current native extent",
               [&]
               {
                   fixture.Put(M::AnimationCountRva, std::int32_t{8});
                   std::uintptr_t output = 0x12345678;
                   const auto     result = runtime.access.FindAnimationBlockHierarchy(identity, nameHash, output);
                   fixture.Put(M::AnimationCountRva, std::int32_t{12});
                   Check(result == ClockResult::BindingChanged && output == 0x12345678 && unchanged(),
                         "corrupt current range cannot become a partial match or a complete miss");
               });
        action("each searched hierarchy must belong to this actual block",
               [&]
               {
                   fixture.Put(H::BankRva + 6 * H::RowBytes + H::OwningBlockOffset, std::uint32_t{1});
                   std::uintptr_t output = 0x12345678;
                   const auto     result = runtime.access.FindAnimationBlockHierarchy(identity, nameHash, output);
                   fixture.Put(H::BankRva + 6 * H::RowBytes + H::OwningBlockOffset, std::uint32_t{0});
                   Check(result == ClockResult::BindingChanged && output == 0x12345678 && unchanged(),
                         "wrong owning-block cell cannot be adopted merely because its hash matches");
               });
        action("copied caller intent survives native observation callbacks",
               [&]
               {
                   auto           input = identity;
                   std::uint32_t  hash = 0xffffffffu;
                   std::uintptr_t output = 0x12345678;
                   caller = &input;
                   callerHash = &hash;
                   Hook::afterRead = &ChangeCaller;
                   const auto result = runtime.access.FindAnimationBlockHierarchy(input, hash, output);
                   caller = nullptr;
                   callerHash = nullptr;
                   Check(result == ClockResult::Ok && output == address(8) && input.ownerGeneration == 0 && !hash && unchanged(),
                         "real native owner captures complete identity/hash before callbacks and does not re-read mutated caller arguments");
               });
        for (const auto hash : {nameHash, 0xdeadbeefu})
        {
            action("changed searched prefix is refusal",
                   [&]
                   {
                       Hook::reads = 0;
                       changedAddress = address(5);
                       changedHash = hash;
                       changed = false;
                       Hook::afterRead = &ChangeSearchedPrefix;
                       std::uintptr_t output = 0x12345678;
                       const auto     result = runtime.access.FindAnimationBlockHierarchy(identity, hash, output);
                       Hook::afterRead = nullptr;
                       fixture.Put(H::BankRva + 5 * H::RowBytes, std::uint32_t{0});
                       Check(changed && result == ClockResult::BindingChanged && output == 0x12345678 && unchanged(),
                             "both first-match and complete-miss paths requalify every earlier searched hash before publishing a result");
                   });
        }
        action("failed first native block observation preserves output",
               [&]
               {
                   Hook::failNext = true;
                   std::uintptr_t output = 0x12345678;
                   Check(runtime.access.FindAnimationBlockHierarchy(identity, nameHash, output) == ClockResult::ReadFailure && output == 0x12345678 &&
                             !runtime.stopping.load(),
                         "read failure is not converted into a valid no-match nullptr");
               });
        action("lost final owner cannot publish a hash result",
               [&]
               {
                   const auto previous = timingCallbackGeneration;
                   Hook::reads = 0;
                   changed = false;
                   Hook::afterRead = &LoseFinalOwner;
                   std::uintptr_t output = 0x12345678;
                   const auto     result = runtime.access.FindAnimationBlockHierarchy(identity, nameHash, output);
                   Hook::afterRead = nullptr;
                   timingCallbackGeneration = previous;
                   Check(changed && result == ClockResult::OwnerConflict && output == 0x12345678 && unchanged(),
                         "final actual callback-generation loss preserves output and adds no native mutation or lifetime debt");
               });
        Check(refuses([&] { static_cast<void>(sdk.GetAnimation(nameHash, block)); }) && unchanged(),
              "copied block affinity does not lend native factory authority outside a callback");
        block.reset();
        foreign.reset();
        stale.reset();
        Check(unchanged(), "CPU block/factory facade destruction frees no native hierarchy, sequence, cache or block ownership");
        Hook::afterRead = nullptr;
        Hook::failNext = false;
        Hook::row = 0;
        WorldGroundOwnerFixture::Retire(service, fixture);
        Host::active = nullptr;
        ClockReaderTestHost::image = previousImage;
        saved.Restore();
        std::puts("Production animation block hash factory owner fixture completed");
        std::puts("Production actual block hash factory SDK fixture completed");
    }
}
